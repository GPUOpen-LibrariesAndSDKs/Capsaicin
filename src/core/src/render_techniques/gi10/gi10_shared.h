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
#ifndef GI10_SHARED_H
#define GI10_SHARED_H

#include "../../gpu_shared.h"

#ifdef __cplusplus
using namespace Capsaicin;
#endif

enum ScreenProbesDebugModes
{
    SCREENPROBES_DEBUG_RADIANCE = 0,
    SCREENPROBES_DEBUG_RADIANCE_PER_DIRECTION
};

struct ScreenProbesConstants
{
    float                  cell_size;
    uint                   probe_size;
    uint2                  probe_count;
    uint                   probe_mask_mip_count;
    uint                   probe_spawn_tile_size;
    ScreenProbesDebugModes debug_mode;
};

enum HashGridCacheDebugMode
{
    HASHGRIDCACHE_DEBUG_RADIANCE,
    HASHGRIDCACHE_DEBUG_RADIANCE_SAMPLE_COUNT,
    HASHGRIDCACHE_DEBUG_FILTERED_RADIANCE,
    HASHGRIDCACHE_DEBUG_FILTERING_GAIN,
    HASHGRIDCACHE_DEBUG_FILTERED_SAMPLE_COUNT,
    HASHGRIDCACHE_DEBUG_FILTERED_MIP_LEVEL,
    HASHGRIDCACHE_DEBUG_TILE_OCCUPANCY
};

struct HashGridCacheConstants
{
    float                  cell_size;
    float                  min_cell_size;
    float                  tile_size;
    float                  tile_cell_ratio; // tile_size / cell_size
    uint                   num_buckets;
    uint                   num_tiles;
    uint                   num_cells;
    uint                   num_tiles_per_bucket;
    uint                   size_tile_mip0;
    uint                   size_tile_mip1;
    uint                   size_tile_mip2;
    uint                   size_tile_mip3;
    uint                   num_cells_per_tile_mip0;
    uint                   num_cells_per_tile_mip1;
    uint                   num_cells_per_tile_mip2;
    uint                   num_cells_per_tile_mip3;
    uint                   num_cells_per_tile; // sum all available mips
    uint                   first_cell_offset_tile_mip0;
    uint                   first_cell_offset_tile_mip1;
    uint                   first_cell_offset_tile_mip2;
    uint                   first_cell_offset_tile_mip3;
    uint                   buffer_ping_pong;
    float                  max_sample_count;
    uint                   debug_mip_level;
    uint                   debug_propagate;
    uint                   debug_max_cell_decay;
    uint                   debug_bucket_occupancy_histogram_size;
    uint                   debug_bucket_overflow_histogram_size;
    HashGridCacheDebugMode debug_mode;
};

enum HashGridBufferNamesFloat
{
    HASHGRIDCACHE_STATSBUFFER,
    HASHGRID_FLOAT_BUFFER_COUNT
};

enum HashGridBufferNamesUint
{
    HASHGRIDCACHE_HASHBUFFER = 0,
    HASHGRIDCACHE_DECAYCELLBUFFER,
    HASHGRIDCACHE_DECAYTILEBUFFER,
    HASHGRIDCACHE_UPDATETILEBUFFER,
    HASHGRIDCACHE_UPDATETILECOUNTBUFFER,
    HASHGRIDCACHE_UPDATECELLVALUEBUFFER,
    HASHGRIDCACHE_VISIBILITYCOUNTBUFFER,
    HASHGRIDCACHE_VISIBILITYCELLBUFFER,
    HASHGRIDCACHE_VISIBILITYQUERYBUFFER,
    HASHGRIDCACHE_VISIBILITYRAYBUFFER,
    HASHGRIDCACHE_VISIBILITYRAYCOUNTBUFFER,
    HASHGRIDCACHE_PACKEDTILECOUNTBUFFER0,
    HASHGRIDCACHE_PACKEDTILECOUNTBUFFER1,
    HASHGRIDCACHE_PACKEDTILEINDEXBUFFER0,
    HASHGRIDCACHE_PACKEDTILEINDEXBUFFER1,
    HASHGRIDCACHE_BUCKETOCCUPANCYBUFFER,
    HASHGRIDCACHE_BUCKETOVERFLOWCOUNTBUFFER,
    HASHGRIDCACHE_BUCKETOVERFLOWBUFFER,
    HASHGRIDCACHE_FREEBUCKETBUFFER,
    HASHGRIDCACHE_USEDBUCKETBUFFER,
    HASHGRID_UINT_BUFFER_COUNT
};

enum HashGridBufferNamesUint2
{
    HASHGRIDCACHE_VALUEBUFFER = 0,
    HASHGRID_UINT2_BUFFER_COUNT
};

enum HashGridBufferNamesFloat4
{
    HASHGRIDCACHE_VISIBILITYBUFFER = 0,
    HASHGRIDCACHE_DEBUGCELLBUFFER,
    HASHGRID_FLOAT4_BUFFER_COUNT
};

enum GI10DebugMode
{
    GI10_DEBUG_MATERIAL_ALBEDO = 0,
    GI10_DEBUG_MATERIAL_METALLICITY,
    GI10_DEBUG_MATERIAL_ROUGHNESS
};

struct GI10Constants
{
    float4x4                        view_proj;
    float4x4                        view_proj_prev;
    float4x4                        view_proj_inv;
    float4x4                        view_proj_inv_prev;
    float4x4                        reprojection;
    GpuVirtualAddressRange          ray_generation_shader_record;
    GpuVirtualAddressRangeAndStride miss_shader_table;
    uint2                           padding0;
    GpuVirtualAddressRangeAndStride hit_group_table;
    uint2                           padding1;
    GpuVirtualAddressRangeAndStride callable_shader_table;
    uint                            padding2;
};

struct WorldSpaceReSTIRConstants
{
    float cell_size;
    uint  num_cells;
    uint  num_entries_per_cell;
    uint  unused_padding;
};

struct GlossyReflectionsConstants
{
    int2  full_res;
    int   full_radius;
    int   half_radius;
    int   mark_fireflies_half_radius;
    int   mark_fireflies_full_radius;
    float mark_fireflies_half_low_threshold;
    float mark_fireflies_full_low_threshold;
    float mark_fireflies_half_high_threshold;
    float mark_fireflies_full_high_threshold;
    int   cleanup_fireflies_half_radius;
    int   cleanup_fireflies_full_radius;
    float low_roughness_threshold;
    float high_roughness_threshold;
    uint  half_res;
    uint  padding;
};

struct GlossyReflectionsAtrousConstants
{
    int ping_pong;
    int full_step;
    int pass_index;
};

enum GlossyReflectionsNamesFloat
{
    GLOSSY_REFLECTION_FIREFLIES_BUFFER = 0,
    GLOSSY_REFLECTION_TEXTURE_FLOAT_COUNT
};

enum GlossyReflectionsNamesFloat4
{
    GLOSSY_REFLECTION_SPECULAR_BUFFER = 0,
    GLOSSY_REFLECTION_DIRECTION_BUFFER,
    GLOSSY_REFLECTION_REFLECTION_BUFFER,
    GLOSSY_REFLECTION_STANDARD_DEV_BUFFER,
    GLOSSY_REFLECTION_REFLECTIONS_BUFFER_0,     //
    GLOSSY_REFLECTION_REFLECTIONS_BUFFER_1,     // BE CAREFUL: keep 0 and 1 contiguous in memory for ping pong
    GLOSSY_REFLECTION_AVERAGE_SQUARED_BUFFER_0, //
    GLOSSY_REFLECTION_AVERAGE_SQUARED_BUFFER_1, //
    GLOSSY_REFLECTION_TEXTURE_FLOAT4_COUNT
};

#endif
