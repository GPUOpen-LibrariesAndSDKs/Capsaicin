/**********************************************************************
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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
    HashGridCacheDebugMode debug_mode;
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

struct GI10Constants
{
    float4x4 view_proj;
    float4x4 view_proj_prev;
    float4x4 view_proj_inv;
    float4x4 view_proj_inv_prev;
    float4x4 reprojection;
    float4x4 view_inv;
};

struct WorldSpaceReSTIRConstants
{
    float cell_size;
    uint  num_cells;
    uint  num_entries_per_cell;
    uint  unused_padding;
};

#endif
