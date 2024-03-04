/**********************************************************************
Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#ifndef SCREEN_PROBES_HLSL
#define SCREEN_PROBES_HLSL

#include "../../math/spherical_harmonics.hlsl"

// The angular threshold for allowing probe-based radiance reuse:
#define kGI10_AngleThreshold cos(2e-2f * PI)

// The amount of float quantization for atomic updates of the probe cells as integer:
#define kGI10_FloatQuantize  1e4f

//!
//! Screen-space radiance caching shader bindings.
//!

Texture2D<uint> g_ScreenProbes_ProbeMask;

RWTexture2D<float4> g_ScreenProbes_ProbeBuffer;
RWTexture2D<uint>   g_ScreenProbes_ProbeMaskBuffer;
RWTexture2D<uint>   g_ScreenProbes_InProbeMaskBuffer;
RWTexture2D<uint>   g_ScreenProbes_OutProbeMaskBuffer;
RWTexture2D<float4> g_ScreenProbes_PreviousProbeBuffer;
RWTexture2D<uint>   g_ScreenProbes_PreviousProbeMaskBuffer;

RWStructuredBuffer<uint2> g_ScreenProbes_ProbeSHBuffer;
RWStructuredBuffer<uint2> g_ScreenProbes_PreviousProbeSHBuffer;

RWStructuredBuffer<uint>  g_ScreenProbes_ProbeSpawnBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeSpawnScanBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeSpawnIndexBuffer;
RWStructuredBuffer<uint2> g_ScreenProbes_ProbeSpawnProbeBuffer;
RWStructuredBuffer<uint2> g_ScreenProbes_ProbeSpawnSampleBuffer;
RWStructuredBuffer<uint2> g_ScreenProbes_ProbeSpawnRadianceBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_PreviousProbeSpawnBuffer;

RWStructuredBuffer<uint> g_ScreenProbes_EmptyTileBuffer;
RWStructuredBuffer<uint> g_ScreenProbes_EmptyTileCountBuffer;
RWStructuredBuffer<uint> g_ScreenProbes_OverrideTileBuffer;
RWStructuredBuffer<uint> g_ScreenProbes_OverrideTileCountBuffer;

RWTexture2D<float4>       g_ScreenProbes_ProbeCachedTileBuffer;
RWTexture2D<float4>       g_ScreenProbes_ProbeCachedTileIndexBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileLRUBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileLRUFlagBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileLRUCountBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileLRUIndexBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileMRUBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileMRUCountBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileListBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileListCountBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileListIndexBuffer;
RWStructuredBuffer<uint4> g_ScreenProbes_ProbeCachedTileListElementBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_ProbeCachedTileListElementCountBuffer;
RWStructuredBuffer<uint>  g_ScreenProbes_PreviousProbeCachedTileLRUBuffer;

groupshared float  lds_ScreenProbes_Radiance[64];
groupshared uint   lds_ScreenProbes_Reprojection[4];
groupshared float4 lds_ScreenProbes_ProbeSHBuffer[9 * 64];
groupshared uint   lds_ScreenProbes_RadianceValues[4 * 64];
groupshared uint   lds_ScreenProbes_RadianceSampleCounts[64];
groupshared uint   lds_ScreenProbes_RadianceReuseSampleCount;
groupshared uint   lds_ScreenProbes_ProbeCachedTileIndex;
groupshared float4 lds_ScreenProbes_RadianceBackup[64];

//!
//! Screen-space radiance caching common functions.
//!

// Packs the pixel seed within the probe tile.
uint ScreenProbes_PackSeed(in uint2 seed)
{
    return (seed.x << 16) | seed.y;
}

// Unpacks the pixel seed from within the probe tile.
uint2 ScreenProbes_UnpackSeed(in uint packed_seed)
{
    return uint2(packed_seed >> 16, packed_seed & 0xFFFFu);
}

// Finds the closest probe to the specified location on the probe grid.
// Here, we start at the highest mip level in the probe mask and fall back
// to lower mips if failing to find a valid probe seed.
// This allows for very large probe search (up to the entire screen) very
// efficiently and is particularly useful to find the neighbor probes in
// disoccluded regions during the final radiance interpolation.
uint ScreenProbes_FindClosestProbe(in uint2 pos)
{
    uint2 dims = uint2((g_BufferDimensions.x + g_ScreenProbesConstants.probe_size - 1) / g_ScreenProbesConstants.probe_size,
                       (g_BufferDimensions.y + g_ScreenProbesConstants.probe_size - 1) / g_ScreenProbesConstants.probe_size);

    pos = min(pos / g_ScreenProbesConstants.probe_size, dims - 1);

    for (uint i = 0; i < g_ScreenProbesConstants.probe_mask_mip_count; ++i)
    {
        uint probe = g_ScreenProbes_ProbeMask.Load(int3(pos, i)).x;

        if (probe != kGI10_InvalidId)
        {
            return probe;   // found a close-by probe :)
        }

        dims = max(dims >> 1, 1);

        pos = min(pos >> 1, dims - 1);
    }

    return kGI10_InvalidId;
}

// This is a variation of the previous function but with an offset that
// is applied iteratively to all visited mip levels of the probe mask.
// Allows to find neighbor probes in specific directions in image space.
uint ScreenProbes_FindClosestProbe(in uint2 pos, in int2 offset)
{
    uint2 dims = uint2((g_BufferDimensions.x + g_ScreenProbesConstants.probe_size - 1) / g_ScreenProbesConstants.probe_size,
                       (g_BufferDimensions.y + g_ScreenProbesConstants.probe_size - 1) / g_ScreenProbesConstants.probe_size);

    pos = min(pos / g_ScreenProbesConstants.probe_size, dims - 1);

    for (uint i = 0; i < g_ScreenProbesConstants.probe_mask_mip_count; ++i)
    {
        int2 loc = int2(pos) + offset;

        if (any(loc < 0) || any(loc >= dims))
        {
            break;  // out of bounds
        }

        uint probe = g_ScreenProbes_ProbeMask.Load(int3(loc, i)).x;

        if (probe != kGI10_InvalidId)
        {
            return probe;   // found a close-by probe :)
        }

        dims = max(dims >> 1, 1);

        pos = min(pos >> 1, dims - 1);
    }

    return kGI10_InvalidId;
}

// Packs the coefficients of the SH color.
uint2 ScreenProbes_PackSHColor(in float4 sh_color)
{
    return (f32tof16(sh_color.xy) << 16) | f32tof16(float2(sh_color.zw));
}

// Unpacks the SH color from its packed format inside the probes.
float4 ScreenProbes_UnpackSHColor(in uint2 packed_sh_color)
{
    return float4(f16tof32(packed_sh_color >> 16), f16tof32(packed_sh_color & 0xFFFFu));
}

// Evaluates the irradiance from the probe's SH representation.
float3 ScreenProbes_CalculateSHIrradiance(in float3 normal, in uint2 probe)
{
    float clamped_cosine_sh[9];
    SH_GetCoefficients_ClampedCosine(normal, clamped_cosine_sh);

    uint probe_count = (g_BufferDimensions.x + g_ScreenProbesConstants.probe_size - 1) / g_ScreenProbesConstants.probe_size;
    uint probe_index = probe.x + probe.y * probe_count;

    float3 irradiance = float3(0.0f, 0.0f, 0.0f);

    for (uint i = 0; i < 9; ++i)
    {
        irradiance += clamped_cosine_sh[i] * ScreenProbes_UnpackSHColor(g_ScreenProbes_ProbeSHBuffer[9 * probe_index + i]).xyz;
    }

    return max(irradiance, 0.0f);
}

// Evaluates the irradiance from the probe's SH representation using a bent cone.
float3 ScreenProbes_CalculateSHIrradiance_BentCone(in float3 normal, in float ao, in uint2 probe)
{
    float clamped_cosine_sh[9];
    SH_GetCoefficients_ClampedCosine_Cone(normal, acos(sqrt(saturate(1.0f - ao))), clamped_cosine_sh);

    uint probe_count = (g_BufferDimensions.x + g_ScreenProbesConstants.probe_size - 1) / g_ScreenProbesConstants.probe_size;
    uint probe_index = probe.x + probe.y * probe_count;

    float3 irradiance = float3(0.0f, 0.0f, 0.0f);

    for (uint i = 0; i < 9; ++i)
    {
        irradiance += clamped_cosine_sh[i] * ScreenProbes_UnpackSHColor(g_ScreenProbes_ProbeSHBuffer[9 * probe_index + i]).xyz;
    }

    return max(irradiance, 0.0f);
}

// Packs the radiance for storing inside the probe cell.
uint2 ScreenProbes_PackRadiance(in float4 radiance)
{
    return (f32tof16(radiance.xy) << 16) | f32tof16(float2(radiance.zw));
}

// Unpacks the radiance from its packed format inside the probe cell.
float4 ScreenProbes_UnpackRadiance(in uint2 packed_radiance)
{
    return float4(f16tof32(packed_radiance >> 16), f16tof32(packed_radiance & 0xFFFFu));
}

// Quantizes the radiance value so it can be blended atomically.
uint4 ScreenProbes_QuantizeRadiance(in float4 radiance)
{
    return uint4(round(kGI10_FloatQuantize * radiance));
}

// Recovers the previously quantized radiance value.
float3 ScreenProbes_RecoverRadiance(in uint3 quantized_radiance)
{
    return quantized_radiance / kGI10_FloatQuantize;
}

// Recovers the previously quantized radiance value.
float4 ScreenProbes_RecoverRadiance(in uint4 quantized_radiance)
{
    return quantized_radiance / kGI10_FloatQuantize;
}

// Accumulates the radiance contribution for a given query.
void ScreenProbes_AccumulateRadiance(in uint query_index, in float3 radiance)
{
    if (dot(radiance, radiance) > 0.0f) // avoid accumulating null contribution(s)
    {
        float4 radiance_and_ray_distance = ScreenProbes_UnpackRadiance(g_ScreenProbes_ProbeSpawnRadianceBuffer[query_index]);

        radiance_and_ray_distance.xyz += radiance;  // accumulate contribution

        g_ScreenProbes_ProbeSpawnRadianceBuffer[query_index] = ScreenProbes_PackRadiance(radiance_and_ray_distance);
    }
}

// Gets the cell and probe index for a given query.
uint2 ScreenProbes_GetCellAndProbeIndex(in uint query_index)
{
    return uint2(query_index % (g_ScreenProbesConstants.probe_size * g_ScreenProbesConstants.probe_size),
                 query_index / (g_ScreenProbesConstants.probe_size * g_ScreenProbesConstants.probe_size));
}

// Packs the screen probe sample.
uint2 ScreenProbes_PackSample(in float3 direction)
{
    uint3 packed_sample = f32tof16(direction);

    return uint2((packed_sample.x << 16) | packed_sample.y, packed_sample.z << 16);
}

// Unpacks the screen probe sample.
float3 ScreenProbes_UnpackSample(in uint2 packed_sample)
{
    uint3 unpacked_sample = uint3(packed_sample.x >> 16, packed_sample.x & 0xFFFFu, packed_sample.y >> 16);

    return f16tof32(unpacked_sample);
}

// Finds the index corresponding to the sampled cell.
uint ScreenProbes_FindCellIndex(in uint local_id, in float s)
{
    uint index = 0;
    uint count = (g_ScreenProbesConstants.probe_size * g_ScreenProbesConstants.probe_size);
    uint start = (local_id / count) * count;

    while (0 < count)
    {
        uint count2 = (count >> 1);
        uint mid    = (index + count2);

        if (lds_ScreenProbes_Radiance[mid + start] > s)
            count = count2;
        else
        {
            index  = (mid + 1);
            count -= (count2 + 1);
        }
    }

    return max(index, 1) - 1;
}

// Scans the radiance for the local invocation.
void ScreenProbes_ScanRadiance(in uint local_id, in float radiance)
{
    uint stride;

    lds_ScreenProbes_Radiance[local_id] = radiance;
    GroupMemoryBarrierWithGroupSync();

    uint block_size = (g_ScreenProbesConstants.probe_size * g_ScreenProbesConstants.probe_size);
    uint first_lane = (local_id / block_size) * block_size;
    uint local_lane = (local_id - first_lane);

    radiance = lds_ScreenProbes_Radiance[first_lane + block_size - 1];
    GroupMemoryBarrierWithGroupSync();

    for (stride = 1; stride <= (block_size >> 1); stride <<= 1)
    {
        if (local_lane < block_size / (2 * stride))
        {
            lds_ScreenProbes_Radiance[first_lane + 2 * (local_lane + 1) * stride - 1] +=
                lds_ScreenProbes_Radiance[first_lane + (2 * local_lane + 1) * stride - 1];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (local_lane == 0)
    {
        lds_ScreenProbes_Radiance[first_lane + block_size - 1] = 0.0f;
    }
    GroupMemoryBarrierWithGroupSync();

    for (stride = (block_size >> 1); stride > 0; stride >>= 1)
    {
        if (local_lane < block_size / (2 * stride))
        {
            float tmp = lds_ScreenProbes_Radiance[first_lane + (2 * local_lane + 1) * stride - 1];
            lds_ScreenProbes_Radiance[first_lane + (2 * local_lane + 1) * stride - 1] = lds_ScreenProbes_Radiance[first_lane + 2 * (local_lane + 1) * stride - 1];
            lds_ScreenProbes_Radiance[first_lane + 2 * (local_lane + 1) * stride - 1] += tmp;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    radiance += lds_ScreenProbes_Radiance[first_lane + block_size - 1];
    GroupMemoryBarrierWithGroupSync();

    lds_ScreenProbes_Radiance[local_id] /= max(radiance, 1e-5f);
    GroupMemoryBarrierWithGroupSync();
}

#endif // SCREEN_PROBES_HLSL
