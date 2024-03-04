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

#ifndef HASH_GRID_CACHE_HLSL
#define HASH_GRID_CACHE_HLSL

// The number of frames before an unused tile is evicted and returned to the pool:
#define kHashGridCache_TileDecay     50

// The amount of float quantization for atomic updates of the hash cells as integer:
#define kHashGridCache_FloatQuantize 1e3f

//!
//! Hash-grid radiance cache structures.
//!

// The structure for encapsulating the visiblity information within the radiance cache.
struct HashGridCache_Visibility
{
    bool   is_front_face;
    uint   instance_index;
    uint   geometry_index;
    uint   primitive_index;
    float2 barycentrics;
};

//!
//! Hash-grid radiance caching shader bindings.
//!

RWStructuredBuffer<float>  g_HashGridCache_BuffersFloat[]  : register(space93);
RWStructuredBuffer<uint>   g_HashGridCache_BuffersUint[]   : register(space96);
RWStructuredBuffer<uint2>  g_HashGridCache_BuffersUint2[]  : register(space97);
RWStructuredBuffer<float4> g_HashGridCache_BuffersFloat4[] : register(space98);

#define                    g_HashGridCache_HashBuffer                    g_HashGridCache_BuffersUint  [HASHGRIDCACHE_HASHBUFFER]
#define                    g_HashGridCache_DecayCellBuffer               g_HashGridCache_BuffersUint  [HASHGRIDCACHE_DECAYCELLBUFFER]
#define                    g_HashGridCache_DecayTileBuffer               g_HashGridCache_BuffersUint  [HASHGRIDCACHE_DECAYTILEBUFFER]
#define                    g_HashGridCache_ValueBuffer                   g_HashGridCache_BuffersUint2 [HASHGRIDCACHE_VALUEBUFFER]
#define                    g_HashGridCache_UpdateTileBuffer              g_HashGridCache_BuffersUint  [HASHGRIDCACHE_UPDATETILEBUFFER]
#define                    g_HashGridCache_UpdateTileCountBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_UPDATETILECOUNTBUFFER]
#define                    g_HashGridCache_UpdateCellValueBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_UPDATECELLVALUEBUFFER]
#define                    g_HashGridCache_VisibilityBuffer              g_HashGridCache_BuffersFloat4[HASHGRIDCACHE_VISIBILITYBUFFER]
#define                    g_HashGridCache_VisibilityCountBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_VISIBILITYCOUNTBUFFER]
#define                    g_HashGridCache_VisibilityCellBuffer          g_HashGridCache_BuffersUint  [HASHGRIDCACHE_VISIBILITYCELLBUFFER]
#define                    g_HashGridCache_VisibilityQueryBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_VISIBILITYQUERYBUFFER]
#define                    g_HashGridCache_VisibilityRayBuffer           g_HashGridCache_BuffersUint  [HASHGRIDCACHE_VISIBILITYRAYBUFFER]
#define                    g_HashGridCache_VisibilityRayCountBuffer      g_HashGridCache_BuffersUint  [HASHGRIDCACHE_VISIBILITYRAYCOUNTBUFFER]
#define                    g_HashGridCache_PackedTileCountBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_PACKEDTILECOUNTBUFFER0 + g_HashGridCacheConstants.buffer_ping_pong]
#define                    g_HashGridCache_PreviousPackedTileCountBuffer g_HashGridCache_BuffersUint  [HASHGRIDCACHE_PACKEDTILECOUNTBUFFER1 - g_HashGridCacheConstants.buffer_ping_pong]
#define                    g_HashGridCache_PackedTileIndexBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_PACKEDTILEINDEXBUFFER0 + g_HashGridCacheConstants.buffer_ping_pong]
#define                    g_HashGridCache_PreviousPackedTileIndexBuffer g_HashGridCache_BuffersUint  [HASHGRIDCACHE_PACKEDTILEINDEXBUFFER1 - g_HashGridCacheConstants.buffer_ping_pong]
#define                    g_HashGridCache_DebugCellBuffer               g_HashGridCache_BuffersFloat4[HASHGRIDCACHE_DEBUGCELLBUFFER]
#define                    g_HashGridCache_BucketOccupancyBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_BUCKETOCCUPANCYBUFFER]
#define                    g_HashGridCache_BucketOverflowCountBuffer     g_HashGridCache_BuffersUint  [HASHGRIDCACHE_BUCKETOVERFLOWCOUNTBUFFER]
#define                    g_HashGridCache_BucketOverflowBuffer          g_HashGridCache_BuffersUint  [HASHGRIDCACHE_BUCKETOVERFLOWBUFFER]
#define                    g_HashGridCache_FreeBucketCountBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_FREEBUCKETBUFFER]
#define                    g_HashGridCache_UsedBucketCountBuffer         g_HashGridCache_BuffersUint  [HASHGRIDCACHE_USEDBUCKETBUFFER]
#define                    g_HashGridCache_StatsBuffer                   g_HashGridCache_BuffersFloat [HASHGRIDCACHE_STATSBUFFER]

//!
//! Hash-grid radiance caching common functions.
//!

#define HASHGRIDCACHE_STEP_FACTOR 1e3f
#define HASHGRIDCACHE_SIZE_FACTOR 1e-3f

// Gets the size of the hash cell for the given world-space position.
float HashGridCache_GetCellSize(in float3 position)
{
    float cell_size_step = max(distance(g_Eye, position) * g_HashGridCacheConstants.cell_size, g_HashGridCacheConstants.min_cell_size);
    uint log_step_multiplier = uint(log2(HASHGRIDCACHE_STEP_FACTOR * cell_size_step));
    float hit_cell_size = HASHGRIDCACHE_SIZE_FACTOR * exp2(log_step_multiplier);
    return hit_cell_size;
}

float HashGridCache_GetTileSize(in float3 position)
{
    float hit_cell_size = HashGridCache_GetCellSize(position);
    float hit_tile_size = hit_cell_size * float(g_HashGridCacheConstants.tile_cell_ratio);
    return hit_tile_size;
}

struct HashGridCache_Data
{
    float3 eye_position;
    float3 hit_position;
    float3 direction;
    float hit_distance;
};

struct HashGridCache_Desc
{
    uint bucket_index;   // bucket index
    uint tile_hash;      // tile hash
    uint2 cell_offset;   // cell offset within the tile
#ifdef DEBUG_HASH_CELLS
    float hit_tile_size;
    float hit_cell_size;
    float3 c;
    float3 d;
#endif // DEBUG_HASH_CELLS
};

// Gets the description for the radiance cell in the current frame structure.
HashGridCache_Desc HashGridCache_GetDesc(in HashGridCache_Data data)
{
    float3 eye_position = data.eye_position;
    float3 hit_position = data.hit_position;
    float3 direction = data.direction;
    float hit_distance = data.hit_distance;

    float cell_size_step = max(distance(eye_position, hit_position) * g_HashGridCacheConstants.cell_size, g_HashGridCacheConstants.min_cell_size);
    float log_step_multiplier = floor(log2(HASHGRIDCACHE_STEP_FACTOR * cell_size_step));
    float hit_cell_size = HASHGRIDCACHE_SIZE_FACTOR * exp2(log_step_multiplier);
    float hit_tile_size = hit_cell_size * float(g_HashGridCacheConstants.tile_cell_ratio);

    float3 signed_c = floor(hit_position / hit_tile_size);
    float3 signed_d = floor(0.5f + (0.5f * direction + 0.5f) * 4.0f);

    uint1 l = uint(log_step_multiplier);
    uint3 c = asuint(int3(signed_c));             // asuint because pcg and xxhash32 use uint and it matters for negative values
    uint3 d = asuint(int3(signed_d));             //
    uint1 t = uint(hit_distance < hit_tile_size);

    uint bucket_index = pcgHash(l +
                        pcgHash(c.x + pcgHash(c.y + pcgHash(c.z +
                        pcgHash(d.x + pcgHash(d.y + pcgHash(d.z +
                        pcgHash(t)))))))) % g_HashGridCacheConstants.num_buckets;

    uint tile_hash  = max(1,
                      xxHash(l +
                      xxHash(c.x + xxHash(c.y + xxHash(c.z +
                      xxHash(d.x + xxHash(d.y + xxHash(d.z +
                      xxHash(t)))))))));

    float3 e = floor(hit_position / hit_cell_size) - floor(hit_position / hit_tile_size) * g_HashGridCacheConstants.tile_cell_ratio;
    uint2 cell_offset;
    float3 abs_direction = abs(direction);
    float max_direction = max(max(abs_direction.x, abs_direction.y), abs_direction.z);
    if (abs_direction.x == max_direction)
    {
        cell_offset = uint2(e.y, e.z);
    }
    else if (abs_direction.y == max_direction)
    {
        cell_offset = uint2(e.x, e.z);
    }
    else
    {
        cell_offset = uint2(e.x, e.y);
    }

    HashGridCache_Desc desc;
    desc.bucket_index  = bucket_index;
    desc.tile_hash     = tile_hash;
    desc.cell_offset   = cell_offset;
#ifdef DEBUG_HASH_CELLS
    desc.hit_tile_size = hit_tile_size;
    desc.hit_cell_size = hit_cell_size;
    desc.c             = signed_c;
    desc.d             = signed_d;
#endif // DEBUG_HASH_CELLS
    return desc;
}

// Used with HashGridCache_CellIndex for climbing mip levels
uint HashGridCache_CellOffsetMip0(uint cell_index_mip0, out uint2 cell_offset_mip0)
{
    uint tile_index         = cell_index_mip0 / g_HashGridCacheConstants.num_cells_per_tile;    // cell_index_mip0 considers if tiles use mipmaps
    uint cell_linear_offset = cell_index_mip0 % g_HashGridCacheConstants.num_cells_per_tile;
    uint mip_size = g_HashGridCacheConstants.size_tile_mip0;
    uint y_step = mip_size;
    cell_offset_mip0.y = cell_linear_offset / y_step;
    cell_linear_offset -= cell_offset_mip0.y * y_step;
    cell_offset_mip0.x = cell_linear_offset;
    return tile_index;
}

uint HashGridCache_CellIndex(uint2 cell_offset_mip0, uint tile_index, uint mip_level)
{
    uint first_cell_offset_tile_mip_level[4] = {
        g_HashGridCacheConstants.first_cell_offset_tile_mip0,
        g_HashGridCacheConstants.first_cell_offset_tile_mip1,
        g_HashGridCacheConstants.first_cell_offset_tile_mip2,
        g_HashGridCacheConstants.first_cell_offset_tile_mip3
    };

    uint mip_size = g_HashGridCacheConstants.size_tile_mip0 >> mip_level;
    uint2 cell_offset = cell_offset_mip0 >> mip_level;
    return tile_index * g_HashGridCacheConstants.num_cells_per_tile +
           first_cell_offset_tile_mip_level[mip_level] +
           cell_offset.x + cell_offset.y * mip_size;
}

uint HashGridCache_CellIndex(uint2 entry_offset_mip0, uint cell_index)
{
    return HashGridCache_CellIndex(entry_offset_mip0, cell_index, 0);
}

// Used for loops where the tile grid doesn't matter
uint HashGridCache_CellIndex(uint1 cell_offset_mip_level, uint tile_index, uint mip_level)
{
    uint first_cell_offset_tile_mip_level[4] = {
        g_HashGridCacheConstants.first_cell_offset_tile_mip0,
        g_HashGridCacheConstants.first_cell_offset_tile_mip1,
        g_HashGridCacheConstants.first_cell_offset_tile_mip2,
        g_HashGridCacheConstants.first_cell_offset_tile_mip3
    };

    return tile_index * g_HashGridCacheConstants.num_cells_per_tile +
           first_cell_offset_tile_mip_level[mip_level] +
           cell_offset_mip_level;
}

uint HashGridCache_CellIndex(uint1 cell_offset_mip0, uint tile_index)
{
    return HashGridCache_CellIndex(cell_offset_mip0, tile_index, 0);
}

// Inserts a new radiance cell inside the current frame hash-grid cache.
uint HashGridCache_InsertCell(in HashGridCache_Data data, out uint tile_index, out bool is_new_tile)
{
    uint bucket_offset;
    is_new_tile = false;
    tile_index = kGI10_InvalidId;
    HashGridCache_Desc desc = HashGridCache_GetDesc(data);
    for (bucket_offset = 0; bucket_offset < g_HashGridCacheConstants.num_tiles_per_bucket; ++bucket_offset)
    {
        uint previous_hash;
        tile_index = bucket_offset + desc.bucket_index * g_HashGridCacheConstants.num_tiles_per_bucket;
        InterlockedCompareExchange(g_HashGridCache_HashBuffer[tile_index], 0, desc.tile_hash, previous_hash);
        if (previous_hash == 0)
        {
            is_new_tile = true;
            break;  // inserted new tile and cell
        }
        if (previous_hash == desc.tile_hash)
            break;  // found existing tile and cell
    }
    if (bucket_offset >= g_HashGridCacheConstants.num_tiles_per_bucket)
    {
    #ifdef DEBUG_HASH_STATS
        uint previous_value;
        InterlockedAdd(g_HashGridCache_BucketOverflowCountBuffer[desc.bucket_index], 1, previous_value);
    #endif        
        return kGI10_InvalidId; // too much collisions, out of tiles :(
    }

    return HashGridCache_CellIndex(desc.cell_offset, tile_index);
}

// Finds a radiance cell entry inside the current frame hash-grid cache.
uint HashGridCache_FindCell(in HashGridCache_Data data, out uint tile_index)
{
    uint bucket_offset;
    tile_index = kGI10_InvalidId;
    HashGridCache_Desc desc = HashGridCache_GetDesc(data);
    for (bucket_offset = 0; bucket_offset < g_HashGridCacheConstants.num_tiles_per_bucket; ++bucket_offset)
    {
        tile_index = bucket_offset + desc.bucket_index * g_HashGridCacheConstants.num_tiles_per_bucket;
        uint previous_hash = g_HashGridCache_HashBuffer[tile_index];
        if (previous_hash == 0)
            return kGI10_InvalidId; // no other tile in bucket
        if (previous_hash == desc.tile_hash)
            break;  // found tile and cell
    }
    if (bucket_offset >= g_HashGridCacheConstants.num_tiles_per_bucket)
        return kGI10_InvalidId; // not found in bucket
    return HashGridCache_CellIndex(desc.cell_offset, tile_index);  // found tile and cell
}

// Quantizes the radiance value so it can be blended atomically.
uint4 HashGridCache_QuantizeRadiance(in float3 radiance)
{
    return uint4(uint(round(kHashGridCache_FloatQuantize * radiance.x)),
                 uint(round(kHashGridCache_FloatQuantize * radiance.y)),
                 uint(round(kHashGridCache_FloatQuantize * radiance.z)), 1);
}

// Recovers the previously quantized radiance value.
float4 HashGridCache_RecoverRadiance(in uint4 quantized_radiance)
{
    return float4(quantized_radiance.x / kHashGridCache_FloatQuantize,
                  quantized_radiance.y / kHashGridCache_FloatQuantize,
                  quantized_radiance.z / kHashGridCache_FloatQuantize,
                  quantized_radiance.w);
}

// Packs the hash grid cell descriptor
#ifdef DEBUG_HASH_CELLS
float4 HashGridCache_ClearDebugCell()
{
    return asfloat(uint4((f32tof16(float4(0.f, 0.f, 0.f, 0.0f)) << 16) |
                         (f32tof16(float4(0.f, 0.f, 0.f, 0.0f)))));
}

uint HashGridCache_PackDebugCell(in HashGridCache_Data data, in uint tile_index, out float4 packed_debug_cell)
{
    // If propagate is true, we display smaller cells with bigger cells values
    uint debug_mip_level = g_HashGridCacheConstants.debug_propagate != 0 ? 0 : g_HashGridCacheConstants.debug_mip_level;

    // We redo HashGridCache_GetDesc for keeping the function signatures "simpler"
    HashGridCache_Desc desc = HashGridCache_GetDesc(data);

    float mip_ratio = float(1u << debug_mip_level);
    float debug_tile_cell_ratio = g_HashGridCacheConstants.tile_cell_ratio / mip_ratio;
    float debug_cell_size = desc.hit_cell_size * mip_ratio;
    float3 debug_e = floor(data.hit_position / debug_cell_size) - floor(data.hit_position / desc.hit_tile_size) * debug_tile_cell_ratio;

    packed_debug_cell = asfloat(uint4((f32tof16(float4(desc.c * desc.hit_tile_size + debug_e * debug_cell_size, desc.hit_cell_size)) << 16) |
                                      (f32tof16(float4(normalize(2.0f * saturate(desc.d / 4.0f) - 1.0f), 0.0f)))));

    uint debug_cell_index = HashGridCache_CellIndex(desc.cell_offset, tile_index, debug_mip_level);
    return debug_cell_index;
}
#endif // DEBUG_HASH_CELLS

// Unpacks the hash grid cell descriptor.
void HashGridCache_UnpackCell(in float4 packed_entry, out float3 position, out float3 direction, out float cell_size)
{
    float4 center_and_size = f16tof32(asuint(packed_entry) >> 16);
    direction = normalize(f16tof32(asuint(packed_entry) & 0xFFFFu).xyz);
    position = center_and_size.xyz;
    cell_size = center_and_size.w;
}

// Packs the radiance for storing inside the hash grid.
uint2 HashGridCache_PackRadiance(in float4 radiance)
{
    return (f32tof16(radiance.xy) << 16) | f32tof16(float2(radiance.zw));
}

// Unpacks the radiance from its packed format inside the hash grid.
float4 HashGridCache_UnpackRadiance(in uint2 packed_radiance)
{
    return float4(f16tof32(packed_radiance >> 16), f16tof32(packed_radiance & 0xFFFFu));
}

// Packs the direction used for looking up the hash grid.
uint HashGridCache_PackDirection(in float3 direction)
{
    uint3 packed_direction = uint3(round(255.0f * (0.5f * direction + 0.5f)));
    return (packed_direction.x << 16) | (packed_direction.y << 8) | packed_direction.z;
}

// Unpacks the direction used for looking up the hash grid.
float3 HashGridCache_UnpackDirection(in uint packed_direction)
{
    uint3 direction = uint3(packed_direction >> 16, packed_direction >> 8, packed_direction) & 0xFFu;
    return normalize(2.0f * direction / 255.0f - 1.0f);
}

float4 HashGridCache_PackVisibility(HashGridCache_Visibility visibility)
{
    return float4(asfloat(visibility.instance_index | (visibility.is_front_face ? 0 : 0x80000000u)),
        asfloat(visibility.geometry_index), asfloat(visibility.primitive_index),
        asfloat((f32tof16(visibility.barycentrics.x) << 16) | (f32tof16(visibility.barycentrics.y) << 0)));
}

// Unpacks the visibility information.
HashGridCache_Visibility HashGridCache_UnpackVisibility(in float4 packed_visibility)
{
    HashGridCache_Visibility visibility;
    visibility.is_front_face   = !((asuint(packed_visibility.x) & 0x80000000u) != 0);
    visibility.instance_index  = (asuint(packed_visibility.x) & 0x7FFFFFFFu);
    visibility.geometry_index  = asuint(packed_visibility.y);
    visibility.primitive_index = asuint(packed_visibility.z);
    visibility.barycentrics.x  = f16tof32(asuint(packed_visibility.w) >> 16);
    visibility.barycentrics.y  = f16tof32(asuint(packed_visibility.w) & 0xFFFFu);
    return visibility;
}

float3 HashGridCache_HeatColor(float heatValue)
{
    // 0 -> Red, 0.5 -> Blue, 1.0 -> Green
    float3 heatColor;
    heatColor.g = smoothstep(0.5f, 0.8f, heatValue);
    if (heatValue > 0.5f)
        heatColor.b = smoothstep(1.0f, 0.5f, heatValue);
    else
        heatColor.b = smoothstep(0.0f, 0.5f, heatValue);
    heatColor.r = smoothstep(1.0f, 0.0f, heatValue);
    return heatColor;
}

float4 HashGridCache_FilteredRadiance(uint cell_index_mip0, bool debug_mip_level)
{
    uint2 cell_offset_mip0;
    uint  tile_index      = HashGridCache_CellOffsetMip0(cell_index_mip0, cell_offset_mip0);
    uint  cell_index_mip1 = g_HashGridCacheConstants.size_tile_mip1 > 0 ? HashGridCache_CellIndex(cell_offset_mip0, tile_index, 1) : kGI10_InvalidId;
    uint  cell_index_mip2 = g_HashGridCacheConstants.size_tile_mip2 > 0 ? HashGridCache_CellIndex(cell_offset_mip0, tile_index, 2) : kGI10_InvalidId;
    uint  cell_index_mip3 = g_HashGridCacheConstants.size_tile_mip3 > 0 ? HashGridCache_CellIndex(cell_offset_mip0, tile_index, 3) : kGI10_InvalidId;

    // Select best mip
    float4 radiance;
    bool   use_mip;

    // Mip 0
    radiance = HashGridCache_UnpackRadiance(g_HashGridCache_ValueBuffer[cell_index_mip0]);
    radiance = debug_mip_level            ? float4(HashGridCache_HeatColor(1.000f), radiance.w) : radiance;

    // Mip 1
    use_mip  = radiance.w < g_HashGridCacheConstants.max_sample_count && cell_index_mip1 != kGI10_InvalidId;
    radiance = use_mip ? HashGridCache_UnpackRadiance(g_HashGridCache_ValueBuffer[cell_index_mip1]) : radiance;
    radiance = debug_mip_level && use_mip ? float4(HashGridCache_HeatColor(0.666f), radiance.w) : radiance;

    // Mip 2
    use_mip  = radiance.w < g_HashGridCacheConstants.max_sample_count && cell_index_mip2 != kGI10_InvalidId;
    radiance = use_mip ? HashGridCache_UnpackRadiance(g_HashGridCache_ValueBuffer[cell_index_mip2]) : radiance;
    radiance = debug_mip_level && use_mip ? float4(HashGridCache_HeatColor(0.333f), radiance.w) : radiance;

    // Mip 3
    use_mip  = radiance.w < g_HashGridCacheConstants.max_sample_count && cell_index_mip3 != kGI10_InvalidId;
    radiance = use_mip ? HashGridCache_UnpackRadiance(g_HashGridCache_ValueBuffer[cell_index_mip3]) : radiance;
    radiance = debug_mip_level && use_mip ? float4(HashGridCache_HeatColor(0.000f), radiance.w) : radiance;

    // Done
    return radiance;
}

#endif // HASH_GRID_CACHE_HLSL
