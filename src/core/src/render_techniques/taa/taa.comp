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
#include "../../gpu_shared.h"

#include "../../math/color.hlsl"

#define RADIUS      1

#define GROUP_SIZE  8

#define TILE_DIM    (2 * RADIUS + GROUP_SIZE)

bool g_HaveHistory;
uint2 g_BufferDimensions;

Texture2D g_DepthBuffer;
Texture2D g_HistoryBuffer;
Texture2D g_VelocityBuffer;

RWTexture2D<float4> g_ColorBuffer;
RWTexture2D<float4> g_OutputBuffer;
#ifdef HAS_DIRECT_LIGHTING_BUFFER
Texture2D           g_DirectLightingBuffer;
#endif
#ifdef HAS_GLOBAL_ILLUMINATION_BUFFER
Texture2D           g_GlobalIlluminationBuffer;
#endif

SamplerState g_LinearSampler;
SamplerState g_NearestSampler;

groupshared float3 Tile[TILE_DIM * TILE_DIM];

// https://www.gdcvault.com/play/1022970/Temporal-Reprojection-Anti-Aliasing-in
float2 GetClosestVelocity(in float2 uv, in float2 texel_size, out bool is_sky_pixel)
{
    float2 velocity;
    float  closest_depth = 9.9f;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float2 st    = uv + float2(x, y) * texel_size;
            float  depth = g_DepthBuffer.SampleLevel(g_NearestSampler, st, 0.0f).x;

            if (depth < closest_depth)
            {
                velocity      = g_VelocityBuffer.SampleLevel(g_NearestSampler, st, 0.0f).xy;
                closest_depth = depth;
            }
        }
    }

    is_sky_pixel = (closest_depth >= 1.0f);

    return velocity;
}

float3 ApplySharpening(in float3 center, in float3 top, in float3 left, in float3 right, in float3 bottom)
{
    float sharpen_amount = 0.25f;

    float accum  = 0.0f;
    float weight = 0.0f;
    float result = sqrt(luminance(center));

    {
        float n0 = sqrt(luminance(top));
        float n1 = sqrt(luminance(bottom));
        float n2 = sqrt(luminance(left));
        float n3 = sqrt(luminance(right));

        float w0 = max(1.0f - 6.0f * (abs(result - n0) + abs(result - n1)), 0.0f);
        float w1 = max(1.0f - 6.0f * (abs(result - n2) + abs(result - n3)), 0.0f);

        w0 = min(1.25f * sharpen_amount * w0, w0);
        w1 = min(1.25f * sharpen_amount * w1, w1);

        accum += n0 * w0;
        accum += n1 * w0;
        accum += n2 * w1;
        accum += n3 * w1;

        weight += 2.0f * w0;
        weight += 2.0f * w1;
    }

    result = max(result * (weight + 1.0f) - accum, 0.0f);
    result = squared(result);

    return min(center * result / max(luminance(center), 1e-5f), 1.0f);
}

// Source: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
// License: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae
float3 SampleHistoryCatmullRom(in float2 uv, in float2 texelSize)
{
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv / texelSize;
    float2 texPos1   = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12      = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0  = texPos1 - 1.0f;
    float2 texPos3  = texPos1 + 2.0f;
    float2 texPos12 = texPos1 + offset12;

    texPos0  *= texelSize;
    texPos3  *= texelSize;
    texPos12 *= texelSize;

    float3 result = float3(0.0f, 0.0f, 0.0f);

    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos0.x,  texPos0.y),  0.0f).xyz * w0.x  * w0.y;
    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos12.x, texPos0.y),  0.0f).xyz * w12.x * w0.y;
    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos3.x,  texPos0.y),  0.0f).xyz * w3.x  * w0.y;

    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos0.x,  texPos12.y), 0.0f).xyz * w0.x  * w12.y;
    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos12.x, texPos12.y), 0.0f).xyz * w12.x * w12.y;
    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos3.x,  texPos12.y), 0.0f).xyz * w3.x  * w12.y;

    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos0.x,  texPos3.y),  0.0f).xyz * w0.x  * w3.y;
    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos12.x, texPos3.y),  0.0f).xyz * w12.x * w3.y;
    result += g_HistoryBuffer.SampleLevel(g_LinearSampler, float2(texPos3.x,  texPos3.y),  0.0f).xyz * w3.x  * w3.y;

    return max(result, 0.0f);
}

float3 Tap(in float2 pos)
{
    return Tile[int(pos.x) + TILE_DIM * int(pos.y)];
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ResolveTemporal(in uint2 globalID : SV_DispatchThreadID, in uint2 localID : SV_GroupThreadID, in uint localIndex : SV_GroupIndex, in uint2 groupID : SV_GroupID)
{
    bool is_sky_pixel;

    float2 texel_size = 1.0f / g_BufferDimensions;
    float2 uv         = (globalID + 0.5f) * texel_size;
    float2 tile_pos   = localID + RADIUS + 0.5f;

    // Populate LDS tile
    if (localIndex < TILE_DIM * TILE_DIM / 4)
    {
        int2 anchor = groupID.xy * GROUP_SIZE - RADIUS;

        int2 coord1 = anchor + int2(localIndex % TILE_DIM, localIndex / TILE_DIM);
        int2 coord2 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 4) / TILE_DIM);
        int2 coord3 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 2) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 2) / TILE_DIM);
        int2 coord4 = anchor + int2((localIndex + TILE_DIM * TILE_DIM * 3 / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM * 3 / 4) / TILE_DIM);

        float2 uv1 = (coord1 + 0.5f) * texel_size;
        float2 uv2 = (coord2 + 0.5f) * texel_size;
        float2 uv3 = (coord3 + 0.5f) * texel_size;
        float2 uv4 = (coord4 + 0.5f) * texel_size;

        float3 color0 = 0.0f.xxx;
        float3 color1 = 0.0f.xxx;
        float3 color2 = 0.0f.xxx;
        float3 color3 = 0.0f.xxx;

#ifdef HAS_DIRECT_LIGHTING_BUFFER
        color0 += g_DirectLightingBuffer.SampleLevel(g_NearestSampler, uv1, 0.0f).xyz;
        color1 += g_DirectLightingBuffer.SampleLevel(g_NearestSampler, uv2, 0.0f).xyz;
        color2 += g_DirectLightingBuffer.SampleLevel(g_NearestSampler, uv3, 0.0f).xyz;
        color3 += g_DirectLightingBuffer.SampleLevel(g_NearestSampler, uv4, 0.0f).xyz;
#endif

#ifdef HAS_GLOBAL_ILLUMINATION_BUFFER
        color0 += g_GlobalIlluminationBuffer.SampleLevel(g_NearestSampler, uv1, 0.0f).xyz;
        color1 += g_GlobalIlluminationBuffer.SampleLevel(g_NearestSampler, uv2, 0.0f).xyz;
        color2 += g_GlobalIlluminationBuffer.SampleLevel(g_NearestSampler, uv3, 0.0f).xyz;
        color3 += g_GlobalIlluminationBuffer.SampleLevel(g_NearestSampler, uv4, 0.0f).xyz;
#endif

        Tile[localIndex]                               = saturate(color0);
        Tile[localIndex + TILE_DIM * TILE_DIM / 4]     = saturate(color1);
        Tile[localIndex + TILE_DIM * TILE_DIM / 2]     = saturate(color2);
        Tile[localIndex + TILE_DIM * TILE_DIM * 3 / 4] = saturate(color3);
    }
    GroupMemoryBarrierWithGroupSync();

    // Iterate the neighboring samples
    if (any(globalID >= g_BufferDimensions))
    {
        return; // out of bounds
    }

    float  wsum  = 0.0f;
    float3 vsum  = float3(0.0f, 0.0f, 0.0f);
    float3 vsum2 = float3(0.0f, 0.0f, 0.0f);

    for (float y = -RADIUS; y <= RADIUS; ++y)
    {
        for (float x = -RADIUS; x <= RADIUS; ++x)
        {
            float3 neigh = Tap(tile_pos + float2(x, y));
            float  w     = exp(-3.0f * (x * x + y * y) / ((RADIUS + 1.0f) * (RADIUS + 1.0f)));

            vsum2 += neigh * neigh * w;
            vsum  += neigh * w;
            wsum  += w;
        }
    }

    // Calculate mean and standard deviation
    float3 ex  = vsum / wsum;
    float3 ex2 = vsum2 / wsum;
    float3 dev = sqrt(max(ex2 - ex * ex, 0.0f));

    float2 velocity = GetClosestVelocity(uv, texel_size, is_sky_pixel);
    float  box_size = lerp(0.5f, 2.5f, is_sky_pixel ? 0.0f : smoothstep(0.02f, 0.0f, length(velocity)));

    // Reproject and clamp to bounding box
    float3 nmin = ex - dev * box_size;
    float3 nmax = ex + dev * box_size;

    float3 center          = Tap(tile_pos); // retrieve center value
    float3 history         = g_HaveHistory ? SampleHistoryCatmullRom(uv - velocity, texel_size) : center;
    float3 clamped_history = clamp(history, nmin, nmax);
    float3 result          = lerp(clamped_history, center, 1.0f / 16.0f);

    // Write antialised sample to memory
    g_OutputBuffer[globalID] = float4(result, 1.0f);
}

[numthreads(8, 8, 1)]
void ResolvePassthru(in uint2 did : SV_DispatchThreadID)
{
    float3 colour = 0.0f.xxx;
#ifdef HAS_DIRECT_LIGHTING_BUFFER
    colour += g_DirectLightingBuffer.Load(int3(did, 0)).xyz;
#endif
#ifdef HAS_GLOBAL_ILLUMINATION_BUFFER
    colour += g_GlobalIlluminationBuffer.Load(int3(did, 0)).xyz;
#endif

    g_ColorBuffer[did] = float4(colour, 1.0f);
}

[numthreads(8, 8, 1)]
void UpdateHistory(in uint2 did : SV_DispatchThreadID)
{
    if (any(did >= g_BufferDimensions))
    {
        return; // out of bounds
    }

    float2 texel_size = 1.0f / g_BufferDimensions;
    float2 uv         = (did + 0.5f) * texel_size;

    float3 top        = g_HistoryBuffer.SampleLevel(g_NearestSampler, uv + float2( 0.0f,          texel_size.y), 0.0f).xyz;
    float3 left       = g_HistoryBuffer.SampleLevel(g_NearestSampler, uv + float2(-texel_size.x,  0.0f        ), 0.0f).xyz;
    float3 right      = g_HistoryBuffer.SampleLevel(g_NearestSampler, uv + float2( texel_size.x,  0.0f        ), 0.0f).xyz;
    float3 bottom     = g_HistoryBuffer.SampleLevel(g_NearestSampler, uv + float2( 0.0f,         -texel_size.y), 0.0f).xyz;

    float3 center = g_HistoryBuffer.Load(int3(did, 0)).xyz;
    float3 color  = ApplySharpening(center, top, left, right, bottom);

    g_ColorBuffer[did] = float4(color, 1.0f);
    g_OutputBuffer[did] = float4(center, 1.0f);
}
