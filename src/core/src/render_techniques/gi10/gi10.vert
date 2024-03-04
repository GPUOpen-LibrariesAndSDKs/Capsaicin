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

#include "gi10_shared.h"

float3 g_Eye;
float2 g_NearFar;
uint   g_FrameIndex;
float4 g_InvDeviceZ;
float3 g_PreviousEye;

ConstantBuffer<GI10Constants>          g_GI10Constants;
ConstantBuffer<HashGridCacheConstants> g_HashGridCacheConstants;

#include "../../math/hash.hlsl"
#include "../../math/sampling.hlsl"

#include "gi10.hlsl"
#include "hash_grid_cache.hlsl"

float4 ResolveGI10(in uint idx : SV_VertexID) : SV_POSITION
{
    return 1.0f - float4(4.0f * (idx & 1), 4.0f * (idx >> 1), 1.0f, 0.0f);
}

float4 DebugReflection(in uint idx : SV_VertexID) : SV_POSITION
{
    return 1.0f - float4(4.0f * (idx & 1), 4.0f * (idx >> 1), 1.0f, 0.0f);
}

float4 DebugScreenProbes(in uint idx : SV_VertexID) : SV_POSITION
{
    return 1.0f - float4(4.0f * (idx & 1), 4.0f * (idx >> 1), 1.0f, 0.0f);
}

struct DebugHashGridCells_Params
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

uint WrapDecay(uint decay)
{
    if (g_FrameIndex < decay)   // account for integer wraparound case
    {
        decay = ((0xFFFFFFFFu - decay) + g_FrameIndex + 1);
    }
    else
    {
        decay = (g_FrameIndex - decay);
    }

    return decay;
}

float3 NormalizeRadiance(float4 radiance)
{
    return radiance.xyz / max(radiance.w, 1.0f);
}

float4 CellRadiance(uint cell_index)
{
    float4 radiance = HashGridCache_UnpackRadiance(g_HashGridCache_ValueBuffer[cell_index]);
    return float4(NormalizeRadiance(radiance), float(radiance.w > 0.f));
}

float4 CellFilteredRadiance(uint entry_cell_mip0)
{
    float4 radiance = HashGridCache_FilteredRadiance(entry_cell_mip0, false);
    return float4(NormalizeRadiance(radiance), float(radiance.w > 0.f));
}

float4 CellFilteringGain(uint entry_cell_mip0)
{
    float4 filtered_radiance = HashGridCache_FilteredRadiance(entry_cell_mip0, false);
    float4 base_radiance = HashGridCache_UnpackRadiance(g_HashGridCache_ValueBuffer[entry_cell_mip0]);
    float3 diff_radiance = max(0.f, NormalizeRadiance(filtered_radiance) - NormalizeRadiance(base_radiance));
    return float4(diff_radiance, float(filtered_radiance.w > 0.f));
}

float4 HeatSampleCount(float4 radiance)
{
    if (radiance.w < 1.f)
        return float4(0.f, 0.f, 0.f, 0.f);

    return float4(HashGridCache_HeatColor(radiance.w / g_HashGridCacheConstants.max_sample_count), 1.0f);
}

float4 CellRadianceSampleCount(uint cell_index)
{
    uint2 packed_radiance = g_HashGridCache_ValueBuffer[cell_index];
    if (all(packed_radiance == uint2(0, 0)))
        return float4(0.f, 0.f, 0.f, 0.f);

    float4 radiance = HashGridCache_UnpackRadiance(packed_radiance);
    return HeatSampleCount(radiance);
}

float4 CellFilteredSampleCount(uint entry_cell_mip0)
{
    float4 radiance = HashGridCache_FilteredRadiance(entry_cell_mip0, true);
    return HeatSampleCount(radiance);
}

float4 CellFilteredMipLevel(uint entry_cell_mip0)
{
    float4 radiance = HashGridCache_FilteredRadiance(entry_cell_mip0, true);
    return float4(radiance.xyz, 1.f);
}

float4 TileOccupancyColor(uint tile_index, uint num_cells_per_tile_mip_debug)
{
    uint num_used_cells = 0;
    for (uint cell_offset = 0; cell_offset < num_cells_per_tile_mip_debug; ++cell_offset)
    {
        uint cell_index = HashGridCache_CellIndex(cell_offset, tile_index, g_HashGridCacheConstants.debug_mip_level);
        float4 radiance = HashGridCache_UnpackRadiance(g_HashGridCache_ValueBuffer[cell_index]);
        num_used_cells += uint(radiance.w > 0);
    }

    // 0 -> Red, 0.5 -> Blue, 1.0 -> Green
    float heatValue = num_used_cells / float(num_cells_per_tile_mip_debug);
    float3 heatColor;
    heatColor.g = smoothstep(0.5f, 0.8f, heatValue);
    if (heatValue > 0.5f)
        heatColor.b = smoothstep(1.0f, 0.5f, heatValue);
    else
        heatColor.b = smoothstep(0.0f, 0.5f, heatValue);
    heatColor.r = smoothstep(1.0f, 0.0f, heatValue);
    return float4(heatColor, 1.0f);
}

DebugHashGridCells_Params DebugHashGridCells(in uint idx : SV_VertexID)
{
    uint num_cells_per_tile_mipX[4] =
    {
        g_HashGridCacheConstants.num_cells_per_tile_mip0,
        g_HashGridCacheConstants.num_cells_per_tile_mip1,
        g_HashGridCacheConstants.num_cells_per_tile_mip2,
        g_HashGridCacheConstants.num_cells_per_tile_mip3
    };

    // If propagate is true, we display smaller cells with bigger cells values
    uint num_cells_per_tile_mip_debug = num_cells_per_tile_mipX[g_HashGridCacheConstants.debug_propagate != 0 ? 0 : g_HashGridCacheConstants.debug_mip_level];
    uint packed_tile_index            = idx / (6 * num_cells_per_tile_mip_debug);
    uint cell_offset                  = idx / 6 - packed_tile_index * num_cells_per_tile_mip_debug;
    uint tile_index                   = g_HashGridCache_PackedTileIndexBuffer[packed_tile_index];
    uint debug_cell_index;
    uint value_cell_index;

    if (g_HashGridCacheConstants.debug_propagate != 0)
    {
        debug_cell_index = HashGridCache_CellIndex(cell_offset, tile_index, 0);
        uint2 cell_offset_mip0;
        HashGridCache_CellOffsetMip0(debug_cell_index, cell_offset_mip0);
        value_cell_index = HashGridCache_CellIndex(cell_offset_mip0, tile_index, g_HashGridCacheConstants.debug_mip_level);
    }
    else
    {
        debug_cell_index = HashGridCache_CellIndex(cell_offset, tile_index, g_HashGridCacheConstants.debug_mip_level);
        value_cell_index = debug_cell_index;
    }

    float4 packed_cell = g_HashGridCache_DebugCellBuffer[debug_cell_index];
    uint   decay_tile  = WrapDecay(g_HashGridCache_DecayTileBuffer[tile_index]);
    uint   decay_cell  = WrapDecay(g_HashGridCache_DecayCellBuffer[debug_cell_index]);

    float cell_size;
    float3 cell_center, direction;
    HashGridCache_UnpackCell(packed_cell, cell_center, direction, cell_size);

    float3 b1, b2;
    GetOrthoVectors(direction, b1, b2);
    uint vertexID = ((idx % 6) > 2 ? (idx % 6) - 2 : idx % 3);
    float3 position = (vertexID & 1) * b1 + (vertexID >> 1) * b2;
    position = cell_center + 0.5f * cell_size * (position + direction);

    DebugHashGridCells_Params params;
    params.position = mul(g_GI10Constants.view_proj, float4(position, 1.0f));

    switch (g_HashGridCacheConstants.debug_mode)
    {
        case HASHGRIDCACHE_DEBUG_RADIANCE:
            params.color = CellRadiance(value_cell_index);
            break;
        case HASHGRIDCACHE_DEBUG_RADIANCE_SAMPLE_COUNT:
            params.color = CellRadianceSampleCount(value_cell_index);
            break;
        case HASHGRIDCACHE_DEBUG_FILTERED_RADIANCE:
            params.color = CellFilteredRadiance(value_cell_index);
            break;
        case HASHGRIDCACHE_DEBUG_FILTERING_GAIN:
            params.color = CellFilteringGain(value_cell_index);
            break;
        case HASHGRIDCACHE_DEBUG_FILTERED_SAMPLE_COUNT:
            params.color = CellFilteredSampleCount(value_cell_index);
            break;
        case HASHGRIDCACHE_DEBUG_FILTERED_MIP_LEVEL:
            params.color = CellFilteredMipLevel(value_cell_index);
            break;
        case HASHGRIDCACHE_DEBUG_TILE_OCCUPANCY:
            params.color = TileOccupancyColor(tile_index, num_cells_per_tile_mip_debug);
            break;
        default:
            params.color = float4(1.f, 1.f, 1.f, 1.f);
            break;
    }

    // debug_max_cell_decay == 0 -> keep cells contributing to screen probes for the current frame
    if (decay_cell > g_HashGridCacheConstants.debug_max_cell_decay)
    {
        params.color.w = 0;
    }

    return params;
}
