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
#include "gi10.h"

#include "capsaicin_internal.h"
#include "components/blue_noise_sampler/blue_noise_sampler.h"
#include "components/light_sampler_bounds/light_sampler_bounds.h"
#include "components/stratified_sampler/stratified_sampler.h"

namespace Capsaicin
{
GI10::Base::Base(GI10 &gi10)
    : gfx_(gi10.gfx_)
    , self(gi10)
{}

GI10::ScreenProbes::ScreenProbes(GI10 &gi10)
    : Base(gi10)
    , probe_spawn_tile_size_(sampling_mode_ == kSamplingMode_QuarterSpp     ? (probe_size_ << 1)
                             : sampling_mode_ == kSamplingMode_SixteenthSpp ? (probe_size_ << 2)
                                                                            : probe_size_)
    , probe_buffer_index_(0)
{
}

GI10::ScreenProbes::~ScreenProbes()
{
    for (GfxTexture probe_buffer : probe_buffers_)
        gfxDestroyTexture(gfx_, probe_buffer);
    for (GfxTexture probe_mask_buffer : probe_mask_buffers_)
        gfxDestroyTexture(gfx_, probe_mask_buffer);

    for (GfxBuffer probe_sh_buffer : probe_sh_buffers_)
        gfxDestroyBuffer(gfx_, probe_sh_buffer);
    for (GfxBuffer probe_spawn_buffer : probe_spawn_buffers_)
        gfxDestroyBuffer(gfx_, probe_spawn_buffer);
    gfxDestroyBuffer(gfx_, probe_spawn_scan_buffer_);
    gfxDestroyBuffer(gfx_, probe_spawn_index_buffer_);
    gfxDestroyBuffer(gfx_, probe_spawn_probe_buffer_);
    gfxDestroyBuffer(gfx_, probe_spawn_probe_buffer_);
    gfxDestroyBuffer(gfx_, probe_spawn_tile_count_buffer_);
    gfxDestroyBuffer(gfx_, probe_spawn_radiance_buffer_);
    gfxDestroyBuffer(gfx_, probe_empty_tile_buffer_);
    gfxDestroyBuffer(gfx_, probe_empty_tile_count_buffer_);
    gfxDestroyBuffer(gfx_, probe_override_tile_buffer_);
    gfxDestroyBuffer(gfx_, probe_override_tile_count_buffer_);
    gfxDestroyTexture(gfx_, probe_cached_tile_buffer_);
    gfxDestroyTexture(gfx_, probe_cached_tile_index_buffer_);
    for (GfxBuffer probe_cached_tile_lru_buffer : probe_cached_tile_lru_buffers_)
        gfxDestroyBuffer(gfx_, probe_cached_tile_lru_buffer);
    gfxDestroyBuffer(gfx_, probe_cached_tile_lru_flag_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_lru_count_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_lru_index_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_mru_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_mru_count_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_list_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_list_count_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_list_index_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_list_element_buffer_);
    gfxDestroyBuffer(gfx_, probe_cached_tile_list_element_count_buffer_);
}

void GI10::ScreenProbes::ensureMemoryIsAllocated(CapsaicinInternal const &capsaicin)
{
    uint32_t const buffer_width  = capsaicin.getWidth();
    uint32_t const buffer_height = capsaicin.getHeight();

    uint2 const probe_count {
        (buffer_width + probe_size_ - 1) / probe_size_, (buffer_height + probe_size_ - 1) / probe_size_};

    uint32_t const probe_buffer_width  = probe_count[0] * probe_size_;
    uint32_t const probe_buffer_height = probe_count[1] * probe_size_;

    uint32_t const probe_mask_mip_count = gfxCalculateMipCount(probe_count[0], probe_count[1]);

    uint32_t const max_probe_count = probe_count[0] * probe_count[1];
    max_probe_spawn_count          = (buffer_width + probe_spawn_tile_size_ - 1) / probe_spawn_tile_size_
                          * (buffer_height + probe_spawn_tile_size_ - 1) / probe_spawn_tile_size_;

    max_ray_count = max_probe_spawn_count * probe_size_ * probe_size_;

    if (probe_buffers_->getWidth() != probe_buffer_width
        || probe_buffers_->getHeight() != probe_buffer_height)
    {
        for (GfxTexture probe_buffer : probe_buffers_)
            gfxDestroyTexture(gfx_, probe_buffer);

        for (uint32_t i = 0; i < ARRAYSIZE(probe_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ProbeBuffer%u", i);

            probe_buffers_[i] = gfxCreateTexture2D(
                gfx_, probe_buffer_width, probe_buffer_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
            probe_buffers_[i].setName(buffer);
        }
    }

    if (probe_mask_buffers_->getWidth() != probe_count[0]
        || probe_mask_buffers_->getHeight() != probe_count[1])
    {
        for (GfxTexture probe_mask_buffer : probe_mask_buffers_)
            gfxDestroyTexture(gfx_, probe_mask_buffer);

        gfxCommandBindKernel(gfx_, self.clear_probe_mask_kernel_);

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, self.clear_probe_mask_kernel_);

        for (uint32_t i = 0; i < ARRAYSIZE(probe_mask_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ProbeMaskBuffer%u", i);

            probe_mask_buffers_[i] = gfxCreateTexture2D(
                gfx_, probe_count[0], probe_count[1], DXGI_FORMAT_R32_UINT, probe_mask_mip_count);
            probe_mask_buffers_[i].setName(buffer);

            for (uint32_t j = 0; j < probe_mask_mip_count; ++j)
            {
                gfxProgramSetParameter(
                    gfx_, self.gi10_program_, "g_ScreenProbes_ProbeMaskBuffer", probe_mask_buffers_[i], j);

                uint32_t const num_groups_x =
                    (GFX_MAX(probe_count[0] >> j, 1u) + num_threads[0] - 1) / num_threads[0];
                uint32_t const num_groups_y =
                    (GFX_MAX(probe_count[1] >> j, 1u) + num_threads[1] - 1) / num_threads[1];

                gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
            }
        }
    }

    if (probe_sh_buffers_->getCount() != 9 * max_probe_count)
    {
        for (GfxBuffer probe_sh_buffer : probe_sh_buffers_)
            gfxDestroyBuffer(gfx_, probe_sh_buffer);

        for (uint32_t i = 0; i < ARRAYSIZE(probe_sh_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ProbeSHBuffer%u", i);

            probe_sh_buffers_[i] = gfxCreateBuffer<uint2>(gfx_, 9 * max_probe_count);
            probe_sh_buffers_[i].setName(buffer);
        }
    }

    if (probe_spawn_buffers_->getCount() != max_probe_spawn_count)
    {
        for (GfxBuffer probe_spawn_buffer : probe_spawn_buffers_)
            gfxDestroyBuffer(gfx_, probe_spawn_buffer);
        gfxDestroyBuffer(gfx_, probe_spawn_scan_buffer_);
        gfxDestroyBuffer(gfx_, probe_spawn_index_buffer_);
        gfxDestroyBuffer(gfx_, probe_spawn_probe_buffer_);
        gfxDestroyBuffer(gfx_, probe_spawn_sample_buffer_);
        gfxDestroyBuffer(gfx_, probe_spawn_radiance_buffer_);
        gfxDestroyBuffer(gfx_, probe_override_tile_buffer_);

        for (uint32_t i = 0; i < ARRAYSIZE(probe_spawn_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ProbeSpawnBuffer%u", i);

            probe_spawn_buffers_[i] = gfxCreateBuffer<uint32_t>(gfx_, max_probe_spawn_count);
            probe_spawn_buffers_[i].setName(buffer);
        }

        probe_spawn_scan_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_spawn_count);
        probe_spawn_scan_buffer_.setName("Capsaicin_ProbeSpawnScanBuffer");

        probe_spawn_index_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_spawn_count);
        probe_spawn_index_buffer_.setName("Capsaicin_ProbeSpawnIndexBuffer");

        probe_spawn_probe_buffer_ = gfxCreateBuffer<uint2>(gfx_, max_probe_spawn_count);
        probe_spawn_probe_buffer_.setName("Capsaicin_ProbeSpawnProbeBuffer");

        probe_spawn_sample_buffer_ = gfxCreateBuffer<uint2>(gfx_, max_ray_count);
        probe_spawn_sample_buffer_.setName("Capsaicin_ProbeSpawnSampleBuffer");

        probe_spawn_radiance_buffer_ = gfxCreateBuffer<uint2>(gfx_, max_ray_count);
        probe_spawn_radiance_buffer_.setName("Capsaicin_ProbeSpawnRadianceBuffer");

        probe_override_tile_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_spawn_count);
        probe_override_tile_buffer_.setName("Capsaicin_ProbeOverrideTileBuffer");
    }

    if (probe_empty_tile_buffer_.getCount() != max_probe_count)
    {
        gfxDestroyBuffer(gfx_, probe_empty_tile_buffer_);

        probe_empty_tile_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_count);
        probe_empty_tile_buffer_.setName("Capsaicin_ProbeEmptyTileBuffer");
    }

    if (!probe_empty_tile_count_buffer_.getCount())
    {
        gfxDestroyBuffer(gfx_, probe_empty_tile_count_buffer_);

        probe_empty_tile_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        probe_empty_tile_count_buffer_.setName("Capsaicin_ProbeEmptyTileCountBuffer");
    }

    if (!probe_override_tile_count_buffer_.getCount())
    {
        gfxDestroyBuffer(gfx_, probe_override_tile_count_buffer_);

        probe_override_tile_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        probe_override_tile_count_buffer_.setName("Capsaicin_ProbeOverrideTileCountBuffer");
    }

    if (!probe_spawn_tile_count_buffer_.getCount())
    {
        gfxDestroyBuffer(gfx_, probe_spawn_tile_count_buffer_);

        probe_spawn_tile_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        probe_spawn_tile_count_buffer_.setName("Capsaicin_ProbeSpawnTileCountBuffer");
    }

    if (probe_cached_tile_buffer_.getWidth() != probe_buffer_width
        || probe_cached_tile_buffer_.getHeight() != probe_buffer_height)
    {
        gfxDestroyTexture(gfx_, probe_cached_tile_buffer_);
        gfxDestroyTexture(gfx_, probe_cached_tile_index_buffer_);
        for (GfxBuffer probe_cached_tile_lru_buffer : probe_cached_tile_lru_buffers_)
            gfxDestroyBuffer(gfx_, probe_cached_tile_lru_buffer);
        gfxDestroyBuffer(gfx_, probe_cached_tile_lru_flag_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_lru_count_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_lru_index_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_mru_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_mru_count_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_list_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_list_count_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_list_index_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_list_element_buffer_);
        gfxDestroyBuffer(gfx_, probe_cached_tile_list_element_count_buffer_);

        probe_cached_tile_buffer_ =
            gfxCreateTexture2D(gfx_, probe_buffer_width, probe_buffer_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        probe_cached_tile_buffer_.setName("Capsaicin_ProbeCachedTileBuffer");

        probe_cached_tile_index_buffer_ =
            gfxCreateTexture2D(gfx_, probe_count[0], probe_count[1], DXGI_FORMAT_R32G32B32A32_FLOAT);
        probe_cached_tile_index_buffer_.setName("Capsaicin_ProbeCachedTileIndexBuffer");

        for (uint32_t i = 0; i < ARRAYSIZE(probe_cached_tile_lru_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ProbeCachedTileLRUBuffer%u", i);

            probe_cached_tile_lru_buffers_[i] = gfxCreateBuffer<uint32_t>(gfx_, max_probe_count);
            probe_cached_tile_lru_buffers_[i].setName(buffer);
        }

        probe_cached_tile_lru_flag_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_count + 1);
        probe_cached_tile_lru_flag_buffer_.setName("Capsaicin_ProbeCachedTileLRUFlagBuffer");

        probe_cached_tile_lru_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        probe_cached_tile_lru_count_buffer_.setName("Capsaicin_ProbeCachedTileLRUCountBuffer");

        probe_cached_tile_lru_index_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_count + 1);
        probe_cached_tile_lru_index_buffer_.setName("Capsaicin_ProbeCachedTileLRUIndexBuffer");

        probe_cached_tile_mru_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_count);
        probe_cached_tile_mru_buffer_.setName("Capsaicin_ProbeCachedTileMRUBuffer");

        probe_cached_tile_mru_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        probe_cached_tile_mru_count_buffer_.setName("Capsaicin_ProbeCachedTileMRUCountBuffer");

        probe_cached_tile_list_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_count);
        probe_cached_tile_list_buffer_.setName("Capsaicin_ProbeCachedTileListBuffer");

        probe_cached_tile_list_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_count);
        probe_cached_tile_list_count_buffer_.setName("Capsaicin_ProbeCachedTileListCountBuffer");

        probe_cached_tile_list_index_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_probe_count);
        probe_cached_tile_list_index_buffer_.setName("Capsaicin_ProbeCachedTileListIndexBuffer");

        probe_cached_tile_list_element_buffer_ = gfxCreateBuffer<uint4>(gfx_, max_probe_count);
        probe_cached_tile_list_element_buffer_.setName("Capsaicin_ProbeCachedTileListElementBuffer");

        probe_cached_tile_list_element_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        probe_cached_tile_list_element_count_buffer_.setName(
            "Capsaicin_ProbeCachedTileListElementCountBuffer");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, self.init_cached_tile_lru_kernel_);
        uint32_t const  num_groups_x = (max_probe_count + num_threads[0] - 1) / num_threads[0];

        gfxProgramSetParameter(gfx_, self.gi10_program_, "g_ScreenProbes_ProbeCachedTileLRUBuffer",
            probe_cached_tile_lru_buffers_[probe_buffer_index_]);

        gfxCommandBindKernel(gfx_, self.init_cached_tile_lru_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);

        // It isn't needed to clear this target technically,
        // but it ensures it has initialized content after
        // resizing, which makes the debug view cleaner :)
        gfxCommandClearTexture(gfx_, probe_cached_tile_buffer_);
    }

    probe_count_ = probe_count;
}

GI10::HashGridCache::HashGridCache(GI10 &gi10)
    : Base(gi10)
    , max_ray_count_(0)
    , num_cells_(0)
    , num_tiles_(0)
    , num_tiles_per_bucket_(0)
    , size_tile_mip0_(0)
    , size_tile_mip1_(0)
    , size_tile_mip2_(0)
    , size_tile_mip3_(0)
    , num_cells_per_tile_mip0_(0)
    , num_cells_per_tile_mip1_(0)
    , num_cells_per_tile_mip2_(0)
    , num_cells_per_tile_mip3_(0)
    , num_cells_per_tile_(0)
    , first_cell_offset_tile_mip0_(0)
    , first_cell_offset_tile_mip1_(0)
    , first_cell_offset_tile_mip2_(0)
    , first_cell_offset_tile_mip3_(0)
    , radiance_cache_hash_buffer_ping_pong_(0)
    , radiance_cache_hash_buffer_(radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_HASHBUFFER])
    , radiance_cache_decay_cell_buffer_(radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_DECAYCELLBUFFER])
    , radiance_cache_decay_tile_buffer_(radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_DECAYTILEBUFFER])
    , radiance_cache_value_buffer_(radiance_cache_hash_buffer_uint2_[HASHGRIDCACHE_VALUEBUFFER])
    , radiance_cache_update_tile_buffer_(radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_UPDATETILEBUFFER])
    , radiance_cache_update_tile_count_buffer_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_UPDATETILECOUNTBUFFER])
    , radiance_cache_update_cell_value_buffer_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_UPDATECELLVALUEBUFFER])
    , radiance_cache_visibility_buffer_(radiance_cache_hash_buffer_float4_[HASHGRIDCACHE_VISIBILITYBUFFER])
    , radiance_cache_visibility_count_buffer_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_VISIBILITYCOUNTBUFFER])
    , radiance_cache_visibility_cell_buffer_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_VISIBILITYCELLBUFFER])
    , radiance_cache_visibility_query_buffer_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_VISIBILITYQUERYBUFFER])
    , radiance_cache_visibility_ray_buffer_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_VISIBILITYRAYBUFFER])
    , radiance_cache_visibility_ray_count_buffer_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_VISIBILITYRAYCOUNTBUFFER])
    , radiance_cache_packed_tile_count_buffer0_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_PACKEDTILECOUNTBUFFER0])
    , radiance_cache_packed_tile_count_buffer1_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_PACKEDTILECOUNTBUFFER1])
    , radiance_cache_packed_tile_index_buffer0_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_PACKEDTILEINDEXBUFFER0])
    , radiance_cache_packed_tile_index_buffer1_(
          radiance_cache_hash_buffer_uint_[HASHGRIDCACHE_PACKEDTILEINDEXBUFFER1])
    , radiance_cache_debug_cell_buffer_(radiance_cache_hash_buffer_float4_[HASHGRIDCACHE_DEBUGCELLBUFFER])
{}

GI10::HashGridCache::~HashGridCache()
{
    for (GfxBuffer buffer : radiance_cache_hash_buffer_uint_)
    {
        gfxDestroyBuffer(gfx_, buffer);
    }

    for (GfxBuffer buffer : radiance_cache_hash_buffer_uint2_)
    {
        gfxDestroyBuffer(gfx_, buffer);
    }

    for (GfxBuffer buffer : radiance_cache_hash_buffer_float4_)
    {
        gfxDestroyBuffer(gfx_, buffer);
    }
}

void GI10::HashGridCache::ensureMemoryIsAllocated(
    CapsaicinInternal const &capsaicin, RenderOptions const &options, std::string_view const &debug_view)
{
    uint32_t const buffer_width  = capsaicin.getWidth();
    uint32_t const buffer_height = capsaicin.getHeight();

    uint32_t const max_ray_count        = self.screen_probes_.max_ray_count;
    uint32_t const num_buckets          = 1u << options.gi10_hash_grid_cache_num_buckets;
    uint32_t const num_tiles_per_bucket = 1u << options.gi10_hash_grid_cache_num_tiles_per_bucket;
    uint32_t const size_tile_mip0       = options.gi10_hash_grid_cache_tile_cell_ratio;
    uint32_t const size_tile_mip1       = size_tile_mip0 >> 1;
    uint32_t const size_tile_mip2       = size_tile_mip1 >> 1;
    uint32_t const size_tile_mip3       = size_tile_mip2 >> 1;
    uint32_t const size_tile_mip4       = size_tile_mip3 >> 1;
    GFX_ASSERT(size_tile_mip4 == 0);
    uint32_t const num_cells_per_tile_mip0 = size_tile_mip0 * size_tile_mip0;
    uint32_t const num_cells_per_tile_mip1 = size_tile_mip1 * size_tile_mip1;
    uint32_t const num_cells_per_tile_mip2 = size_tile_mip2 * size_tile_mip2;
    uint32_t const num_cells_per_tile_mip3 = size_tile_mip3 * size_tile_mip3;
    uint32_t const num_cells_per_tile =
        num_cells_per_tile_mip0 + num_cells_per_tile_mip1 + num_cells_per_tile_mip2 + num_cells_per_tile_mip3;
    uint32_t const first_cell_offset_tile_mip0 = 0;
    uint32_t const first_cell_offset_tile_mip1 = first_cell_offset_tile_mip0 + num_cells_per_tile_mip0;
    uint32_t const first_cell_offset_tile_mip2 = first_cell_offset_tile_mip1 + num_cells_per_tile_mip1;
    uint32_t const first_cell_offset_tile_mip3 = first_cell_offset_tile_mip2 + num_cells_per_tile_mip2;
    GFX_ASSERT(first_cell_offset_tile_mip3 + num_cells_per_tile_mip3 == num_cells_per_tile);
    uint32_t const num_tiles = num_tiles_per_bucket * num_buckets;
    uint32_t const num_cells = num_cells_per_tile * num_tiles;

    if (!radiance_cache_hash_buffer_ || num_tiles != num_tiles_)
    {
        gfxDestroyBuffer(gfx_, radiance_cache_hash_buffer_);
        gfxDestroyBuffer(gfx_, radiance_cache_decay_tile_buffer_);

        radiance_cache_hash_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, num_tiles);
        radiance_cache_hash_buffer_.setName("Capsaicin_RadianceCache_HashBuffer");

        radiance_cache_decay_tile_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, num_tiles);
        radiance_cache_decay_tile_buffer_.setName("Capsaicin_RadianceCache_DecayTileBuffer");

        gfxCommandClearBuffer(gfx_, radiance_cache_hash_buffer_); // clear the radiance cache
    }

    if (!radiance_cache_value_buffer_ || num_cells != num_cells_)
    {
        gfxDestroyBuffer(gfx_, radiance_cache_value_buffer_);

        radiance_cache_value_buffer_ = gfxCreateBuffer<uint2>(gfx_, num_cells);
        radiance_cache_value_buffer_.setName("Capsaicin_RadianceCache_ValueBuffer");
    }

    if (!radiance_cache_update_tile_count_buffer_)
    {
        radiance_cache_update_tile_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        radiance_cache_update_tile_count_buffer_.setName("Capsaicin_RadianceCache_UpdateTileCountBuffer");
    }

    if (!radiance_cache_update_cell_value_buffer_ || num_cells != num_cells_)
    {
        gfxDestroyBuffer(gfx_, radiance_cache_update_cell_value_buffer_);

        radiance_cache_update_cell_value_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, num_cells << 2);
        radiance_cache_update_cell_value_buffer_.setName("Capsaicin_RadianceCache_UpdateCellValueBuffer");

        gfxCommandClearBuffer(gfx_, radiance_cache_update_cell_value_buffer_);
    }

    if (!radiance_cache_visibility_count_buffer_)
    {
        radiance_cache_visibility_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        radiance_cache_visibility_count_buffer_.setName("Capsaicin_RadianceCache_VisibilityCountBuffer");

        radiance_cache_visibility_ray_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        radiance_cache_visibility_ray_count_buffer_.setName(
            "Capsaicin_RadianceCache_VisibilityRayCountBuffer");

        radiance_cache_packed_tile_count_buffer0_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        radiance_cache_packed_tile_count_buffer0_.setName("Capsaicin_RadianceCache_PackedTileCountBuffer0");

        radiance_cache_packed_tile_count_buffer1_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        radiance_cache_packed_tile_count_buffer1_.setName("Capsaicin_RadianceCache_PackedTileCountBuffer1");
    }

    if (!radiance_cache_packed_tile_index_buffer0_ || num_tiles != num_tiles_)
    {
        gfxDestroyBuffer(gfx_, radiance_cache_packed_tile_index_buffer0_);
        gfxDestroyBuffer(gfx_, radiance_cache_packed_tile_index_buffer1_);

        radiance_cache_packed_tile_index_buffer0_ = gfxCreateBuffer<uint32_t>(gfx_, num_tiles);
        radiance_cache_packed_tile_index_buffer0_.setName("Capsaicin_RadianceCache_PackedTileIndexBuffer0");

        radiance_cache_packed_tile_index_buffer1_ = gfxCreateBuffer<uint32_t>(gfx_, num_tiles);
        radiance_cache_packed_tile_index_buffer1_.setName("Capsaicin_RadianceCache_PackedTileIndexBuffer1");

        gfxCommandClearBuffer(gfx_, radiance_cache_packed_tile_index_buffer0_);
        gfxCommandClearBuffer(gfx_, radiance_cache_packed_tile_index_buffer1_);
    }

    // The `packedCell' buffer is not necessary for drawing, but rather used
    // when debugging our hash cells.
    // So, we only allocate the memory when debugging the hash grid radiance
    // cache, and release it when not.
    if (debug_view.starts_with("HashGridCache_"))
    {
        if (!radiance_cache_debug_cell_buffer_ || num_cells != num_cells_)
        {
            gfxDestroyBuffer(gfx_, radiance_cache_debug_cell_buffer_);
            gfxDestroyBuffer(gfx_, radiance_cache_decay_cell_buffer_);

            radiance_cache_debug_cell_buffer_ = gfxCreateBuffer<float4>(gfx_, num_cells);
            radiance_cache_debug_cell_buffer_.setName("Capsaicin_RadianceCache_DebugCellBuffer");

            radiance_cache_decay_cell_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, num_cells);
            radiance_cache_decay_cell_buffer_.setName("Capsaicin_RadianceCache_DecayCellBuffer");

            gfxCommandClearBuffer(gfx_, radiance_cache_decay_cell_buffer_, 0xFFFFFFFFu);
        }
    }
    else
    {
        gfxDestroyBuffer(gfx_, radiance_cache_debug_cell_buffer_);
        gfxDestroyBuffer(gfx_, radiance_cache_decay_cell_buffer_);

        radiance_cache_debug_cell_buffer_ = {};
        radiance_cache_decay_cell_buffer_ = {};
    }

    if (!radiance_cache_update_tile_buffer_ || max_ray_count != max_ray_count_ || num_cells != num_cells_)
    {
        gfxDestroyBuffer(gfx_, radiance_cache_update_tile_buffer_);
        gfxDestroyBuffer(gfx_, radiance_cache_visibility_buffer_);
        gfxDestroyBuffer(gfx_, radiance_cache_visibility_cell_buffer_);
        gfxDestroyBuffer(gfx_, radiance_cache_visibility_query_buffer_);
        gfxDestroyBuffer(gfx_, radiance_cache_visibility_ray_buffer_);

        radiance_cache_update_tile_buffer_ =
            gfxCreateBuffer<uint32_t>(gfx_, GFX_MIN(max_ray_count, num_cells));
        radiance_cache_update_tile_buffer_.setName("Capsaicin_RadianceCache_UpdateTileBuffer");

        radiance_cache_visibility_buffer_ = gfxCreateBuffer<float4>(gfx_, max_ray_count);
        radiance_cache_visibility_buffer_.setName("Capsaicin_RadianceCache_VisibilityBuffer");

        radiance_cache_visibility_cell_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_ray_count);
        radiance_cache_visibility_cell_buffer_.setName("Capsaicin_RadianceCache_VisibilityCellBuffer");

        radiance_cache_visibility_query_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_ray_count);
        radiance_cache_visibility_query_buffer_.setName("Capsaicin_RadianceCache_VisibilityQueryBuffer");

        radiance_cache_visibility_ray_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_ray_count);
        radiance_cache_visibility_ray_buffer_.setName("Capsaicin_RadianceCache_VisibilityRayBuffer");
    }

    max_ray_count_               = max_ray_count;
    num_buckets_                 = num_buckets;
    num_tiles_                   = num_tiles;
    num_cells_                   = num_cells;
    num_tiles_per_bucket_        = num_tiles_per_bucket;
    size_tile_mip0_              = size_tile_mip0;
    size_tile_mip1_              = size_tile_mip1;
    size_tile_mip2_              = size_tile_mip2;
    size_tile_mip3_              = size_tile_mip3;
    num_cells_per_tile_mip0_     = num_cells_per_tile_mip0;
    num_cells_per_tile_mip1_     = num_cells_per_tile_mip1;
    num_cells_per_tile_mip2_     = num_cells_per_tile_mip2;
    num_cells_per_tile_mip3_     = num_cells_per_tile_mip3;
    num_cells_per_tile_          = num_cells_per_tile; // all mips
    first_cell_offset_tile_mip0_ = first_cell_offset_tile_mip0;
    first_cell_offset_tile_mip1_ = first_cell_offset_tile_mip1;
    first_cell_offset_tile_mip2_ = first_cell_offset_tile_mip2;
    first_cell_offset_tile_mip3_ = first_cell_offset_tile_mip3;
}

GI10::WorldSpaceReSTIR::WorldSpaceReSTIR(GI10 &gi10)
    : Base(gi10)
    , reservoir_indirect_sample_buffer_index_(0)
{}

GI10::WorldSpaceReSTIR::~WorldSpaceReSTIR()
{
    for (GfxBuffer reservoir_hash_buffer : reservoir_hash_buffers_)
        gfxDestroyBuffer(gfx_, reservoir_hash_buffer);
    for (GfxBuffer reservoir_hash_count_buffer : reservoir_hash_count_buffers_)
        gfxDestroyBuffer(gfx_, reservoir_hash_count_buffer);
    for (GfxBuffer reservoir_hash_index_buffer : reservoir_hash_index_buffers_)
        gfxDestroyBuffer(gfx_, reservoir_hash_index_buffer);
    for (GfxBuffer reservoir_hash_value_buffer : reservoir_hash_value_buffers_)
        gfxDestroyBuffer(gfx_, reservoir_hash_value_buffer);
    gfxDestroyBuffer(gfx_, reservoir_hash_list_buffer_);
    gfxDestroyBuffer(gfx_, reservoir_hash_list_count_buffer_);

    gfxDestroyBuffer(gfx_, reservoir_indirect_sample_buffer_);
    for (GfxBuffer reservoir_indirect_sample_normal_buffer : reservoir_indirect_sample_normal_buffers_)
        gfxDestroyBuffer(gfx_, reservoir_indirect_sample_normal_buffer);
    gfxDestroyBuffer(gfx_, reservoir_indirect_sample_material_buffer_);
    for (GfxBuffer reservoir_indirect_sample_reservoir_buffer : reservoir_indirect_sample_reservoir_buffers_)
        gfxDestroyBuffer(gfx_, reservoir_indirect_sample_reservoir_buffer);
}

void GI10::WorldSpaceReSTIR::ensureMemoryIsAllocated(CapsaicinInternal const &capsaicin)
{
    uint32_t const buffer_width  = capsaicin.getWidth();
    uint32_t const buffer_height = capsaicin.getHeight();

    uint32_t const max_ray_count = self.screen_probes_.max_ray_count;

    if (reservoir_hash_buffers_->getCount() != kConstant_NumEntries)
    {
        for (GfxBuffer reservoir_hash_buffer : reservoir_hash_buffers_)
            gfxDestroyBuffer(gfx_, reservoir_hash_buffer);
        for (GfxBuffer reservoir_hash_count_buffer : reservoir_hash_count_buffers_)
            gfxDestroyBuffer(gfx_, reservoir_hash_count_buffer);
        for (GfxBuffer reservoir_hash_index_buffer : reservoir_hash_index_buffers_)
            gfxDestroyBuffer(gfx_, reservoir_hash_index_buffer);
        for (GfxBuffer reservoir_hash_value_buffer : reservoir_hash_value_buffers_)
            gfxDestroyBuffer(gfx_, reservoir_hash_value_buffer);

        for (uint32_t i = 0; i < ARRAYSIZE(reservoir_hash_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_Reservoir_HashBuffer%u", i);

            reservoir_hash_buffers_[i] =
                gfxCreateBuffer<uint32_t>(gfx_, WorldSpaceReSTIR::kConstant_NumEntries);
            reservoir_hash_buffers_[i].setName(buffer);
        }

        for (uint32_t i = 0; i < ARRAYSIZE(reservoir_hash_count_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_Reservoir_HashCountBuffer%u", i);

            reservoir_hash_count_buffers_[i] =
                gfxCreateBuffer<uint32_t>(gfx_, WorldSpaceReSTIR::kConstant_NumEntries);
            reservoir_hash_count_buffers_[i].setName(buffer);
        }

        for (uint32_t i = 0; i < ARRAYSIZE(reservoir_hash_index_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_Reservoir_HashIndexBuffer%u", i);

            reservoir_hash_index_buffers_[i] =
                gfxCreateBuffer<uint32_t>(gfx_, WorldSpaceReSTIR::kConstant_NumEntries);
            reservoir_hash_index_buffers_[i].setName(buffer);
        }

        for (uint32_t i = 0; i < ARRAYSIZE(reservoir_hash_value_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_Reservoir_HashValueBuffer%u", i);

            reservoir_hash_value_buffers_[i] =
                gfxCreateBuffer<uint32_t>(gfx_, WorldSpaceReSTIR::kConstant_NumEntries);
            reservoir_hash_value_buffers_[i].setName(buffer);
        }
    }

    if (reservoir_hash_list_buffer_.getCount() < max_ray_count)
    {
        gfxDestroyBuffer(gfx_, reservoir_hash_list_buffer_);
        gfxDestroyBuffer(gfx_, reservoir_hash_list_count_buffer_);

        reservoir_hash_list_buffer_ = gfxCreateBuffer<uint4>(gfx_, max_ray_count);
        reservoir_hash_list_buffer_.setName("Capsaicin_Reservoir_HashListBuffer");

        reservoir_hash_list_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        reservoir_hash_list_count_buffer_.setName("Capsaicin_Reservoir_HashListCountBuffer");
    }

    if (reservoir_indirect_sample_buffer_.getCount() < max_ray_count)
    {
        gfxDestroyBuffer(gfx_, reservoir_indirect_sample_buffer_);
        for (GfxBuffer reservoir_indirect_sample_normal_buffer : reservoir_indirect_sample_normal_buffers_)
            gfxDestroyBuffer(gfx_, reservoir_indirect_sample_normal_buffer);
        gfxDestroyBuffer(gfx_, reservoir_indirect_sample_material_buffer_);
        for (GfxBuffer reservoir_indirect_sample_reservoir_buffer :
            reservoir_indirect_sample_reservoir_buffers_)
            gfxDestroyBuffer(gfx_, reservoir_indirect_sample_reservoir_buffer);

        reservoir_indirect_sample_buffer_ = gfxCreateBuffer<float4>(gfx_, max_ray_count);
        reservoir_indirect_sample_buffer_.setName("Capsaicin_Reservoir_IndirectSampleBuffer");

        for (uint32_t i = 0; i < ARRAYSIZE(reservoir_indirect_sample_normal_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_Reservoir_IndirectSampleNormalBuffer%u", i);

            reservoir_indirect_sample_normal_buffers_[i] = gfxCreateBuffer<uint32_t>(gfx_, max_ray_count);
            reservoir_indirect_sample_normal_buffers_[i].setName(buffer);
        }

        reservoir_indirect_sample_material_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, max_ray_count);
        reservoir_indirect_sample_material_buffer_.setName(
            "Capsaicin_Reservoir_IndirectSamplerMaterialBuffer");

        for (uint32_t i = 0; i < ARRAYSIZE(reservoir_indirect_sample_reservoir_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_Reservoir_IndirectSampleReservoirBuffer%u", i);

            reservoir_indirect_sample_reservoir_buffers_[i] = gfxCreateBuffer<uint4>(gfx_, max_ray_count);
            reservoir_indirect_sample_reservoir_buffers_[i].setName(buffer);
        }
    }
}

GI10::GIDenoiser::GIDenoiser(GI10 &gi10)
    : Base(gi10)
    , color_buffer_index_(0)
{}

GI10::GIDenoiser::~GIDenoiser()
{
    for (GfxTexture blur_mask : blur_masks_)
        gfxDestroyTexture(gfx_, blur_mask);
    for (GfxTexture color_buffer : color_buffers_)
        gfxDestroyTexture(gfx_, color_buffer);
    for (GfxTexture color_delta_buffer : color_delta_buffers_)
        gfxDestroyTexture(gfx_, color_delta_buffer);
    gfxDestroyBuffer(gfx_, blur_sample_count_buffer_);
}

void GI10::GIDenoiser::ensureMemoryIsAllocated(CapsaicinInternal const &capsaicin)
{
    uint32_t const buffer_width  = capsaicin.getWidth();
    uint32_t const buffer_height = capsaicin.getHeight();

    if (!*blur_masks_ || blur_masks_->getWidth() != buffer_width || blur_masks_->getHeight() != buffer_height)
    {
        for (uint32_t i = 0; i < ARRAYSIZE(blur_masks_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_GIDenoiser_BlurMask%u", i);

            gfxDestroyTexture(gfx_, blur_masks_[i]);

            blur_masks_[i] = gfxCreateTexture2D(gfx_, buffer_width, buffer_height, DXGI_FORMAT_R8_SNORM);
            blur_masks_[i].setName(buffer);
        }

        for (uint32_t i = 0; i < ARRAYSIZE(color_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_GIDenoiser_ColorBuffer%u", i);

            gfxDestroyTexture(gfx_, color_buffers_[i]);

            color_buffers_[i] =
                gfxCreateTexture2D(gfx_, buffer_width, buffer_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
            color_buffers_[i].setName(buffer);
        }

        for (uint32_t i = 0; i < ARRAYSIZE(color_delta_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_GIDenoiser_ColorDeltaBuffer%u", i);

            gfxDestroyTexture(gfx_, color_delta_buffers_[i]);

            color_delta_buffers_[i] =
                gfxCreateTexture2D(gfx_, buffer_width, buffer_height, DXGI_FORMAT_R16_FLOAT);
            color_delta_buffers_[i].setName(buffer);
        }
    }

    if (!blur_sample_count_buffer_)
    {
        blur_sample_count_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, 1);
        blur_sample_count_buffer_.setName("Capsaicin_GIDenoiser_BlurSampleCountBuffer");
    }
}

GI10::GI10()
    : RenderTechnique("GI-1.0")
    , has_delta_lights_(false)
    , screen_probes_(*this)
    , hash_grid_cache_(*this)
    , world_space_restir_(*this)
    , gi_denoiser_(*this)
{}

GI10::~GI10()
{
    terminate();
}

RenderOptionList GI10::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_use_resampling, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_use_alpha_testing, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_use_direct_lighting, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_disable_albedo_textures, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_hash_grid_cache_cell_size, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_hash_grid_cache_tile_cell_ratio, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_hash_grid_cache_num_buckets, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_hash_grid_cache_num_tiles_per_bucket, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_hash_grid_cache_max_sample_count, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_hash_grid_cache_debug_mip_level, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_hash_grid_cache_debug_propagate, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_hash_grid_cache_debug_max_cell_decay, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(gi10_reservoir_cache_cell_size, options_));
    return newOptions;
}

GI10::RenderOptions GI10::convertOptions(RenderSettings const &settings) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(gi10_use_resampling, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_use_alpha_testing, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_use_direct_lighting, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_disable_albedo_textures, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_hash_grid_cache_cell_size, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_hash_grid_cache_tile_cell_ratio, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_hash_grid_cache_num_buckets, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_hash_grid_cache_num_tiles_per_bucket, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_hash_grid_cache_max_sample_count, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_hash_grid_cache_debug_mip_level, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_hash_grid_cache_debug_propagate, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_hash_grid_cache_debug_max_cell_decay, newOptions, settings.options_)
    RENDER_OPTION_GET(gi10_reservoir_cache_cell_size, newOptions, settings.options_)
    return newOptions;
}

ComponentList GI10::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(LightSamplerBounds));
    components.emplace_back(COMPONENT_MAKE(BlueNoiseSampler));
    components.emplace_back(COMPONENT_MAKE(StratifiedSampler));
    return components;
}

AOVList GI10::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"Debug", AOV::Write});
    aovs.push_back({"GlobalIllumination", AOV::Write, AOV::None, DXGI_FORMAT_R16G16B16A16_FLOAT});
    aovs.push_back({.name = "VisibilityDepth", .backup_name = "PrevVisibilityDepth"});
    aovs.push_back({.name = "Normal", .backup_name = "PrevNormal"});
    aovs.push_back({.name = "Details", .backup_name = "PrevDetails"});
    aovs.push_back({"Velocity"});
    aovs.push_back({"OcclusionAndBentNormal"});
    aovs.push_back({"NearFieldGlobalIllumination"});
    aovs.push_back({"Visibility"});
    aovs.push_back({"PrevCombinedIllumination"});
    aovs.push_back({"DisocclusionMask"});
    return aovs;
}

DebugViewList GI10::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("RadianceCache");
    views.emplace_back("RadianceCachePerDirection");
    views.emplace_back("HashGridCache_Radiance");
    views.emplace_back("HashGridCache_RadianceSampleCount");
    views.emplace_back("HashGridCache_FilteredRadiance");
    views.emplace_back("HashGridCache_FilteredGain");
    views.emplace_back("HashGridCache_FilteredSampleCount");
    views.emplace_back("HashGridCache_FilteredMipLevel");
    views.emplace_back("HashGridCache_Occupancy");
    return views;
}

bool GI10::init(CapsaicinInternal const &capsaicin) noexcept
{
    depth_buffer_ = gfxCreateTexture2D(gfx_, DXGI_FORMAT_D32_FLOAT);
    depth_buffer_.setName("Capsaicin_GIDepthBuffer");

    draw_command_buffer_ = gfxCreateBuffer<uint4>(gfx_, 1);
    draw_command_buffer_.setName("Capsaicin_DrawCommandBuffer");

    dispatch_command_buffer_ = gfxCreateBuffer<DispatchCommand>(gfx_, 1);
    dispatch_command_buffer_.setName("Capsaicin_DispatchCommandBuffer");

    // Set up the base defines based on available features
    auto                      light_sampler = capsaicin.getComponent<LightSamplerBounds>();
    std::vector<std::string>  defines(std::move(light_sampler->getShaderDefines(capsaicin)));
    std::vector<char const *> base_defines;
    for (auto &i : defines)
    {
        base_defines.push_back(i.c_str());
    }
    base_defines.push_back("DISABLE_SPECULAR_LIGHTING");    // TODO: glossy reflections aren't supported yet
    if (!has_delta_lights_) base_defines.push_back("HAS_FEEDBACK");
    if (options_.gi10_use_alpha_testing) base_defines.push_back("USE_ALPHA_TESTING");
    if (capsaicin.hasAOVBuffer("OcclusionAndBentNormal")) base_defines.push_back("HAS_OCCLUSION");
    uint32_t const base_define_count = (uint32_t)base_defines.size();

    std::vector<char const *> resampling_defines = base_defines;
    if (options_.gi10_use_resampling) resampling_defines.push_back("USE_RESAMPLING");
    uint32_t const resampling_define_count = (uint32_t)resampling_defines.size();

    std::vector<char const *> debug_hash_cells_defines = base_defines;
    if (debug_view_.starts_with("HashGridCache_"))
    {
        debug_hash_cells_defines.push_back("DEBUG_HASH_CELLS");
    }
    uint32_t const debug_hash_cells_define_count = (uint32_t)debug_hash_cells_defines.size();

    // We need to clear the radiance cache when toggling the hash cells debug mode;
    // that is because we only populate the `packedCell' buffer when visualizing the
    // cells as this is required to recover the position & orientation of a given cell.
    // However, we skip this completely during normal rendering as it introduces some
    // unnecessary overhead.
    // We therefore need to clear the radiance cache each time we toggle the debug views
    // to make sure all spawned cells have their packed descriptors available.
    if (debug_hash_cells_define_count > 0)
    {
        clearHashGridCache(); // clear the radiance cache
    }

    GfxDrawState resolve_lighting_draw_state;
    gfxDrawStateSetColorTarget(resolve_lighting_draw_state, 0, capsaicin.getAOVBuffer("GlobalIllumination"));

    GfxDrawState debug_screen_probes_draw_state;
    gfxDrawStateSetColorTarget(debug_screen_probes_draw_state, 0, capsaicin.getAOVBuffer("Debug"));

    GfxDrawState debug_hash_grid_cells_draw_state;
    gfxDrawStateSetColorTarget(debug_hash_grid_cells_draw_state, 0, capsaicin.getAOVBuffer("Debug"));
    gfxDrawStateSetDepthStencilTarget(debug_hash_grid_cells_draw_state, depth_buffer_);
    gfxDrawStateSetCullMode(debug_hash_grid_cells_draw_state, D3D12_CULL_MODE_NONE);

    GfxDrawState debug_material_draw_state;
    gfxDrawStateSetColorTarget(debug_material_draw_state, 0, capsaicin.getAOVBuffer("Debug"));

    gi10_program_          = gfxCreateProgram(gfx_, "render_techniques/gi10/gi10", capsaicin.getShaderPath());
    resolve_gi10_kernel_   = gfxCreateGraphicsKernel(gfx_, gi10_program_, resolve_lighting_draw_state,
          "ResolveGI10", base_defines.data(), base_define_count);
    clear_counters_kernel_ = gfxCreateComputeKernel(gfx_, gi10_program_, "ClearCounters");
    generate_draw_kernel_  = gfxCreateComputeKernel(gfx_, gi10_program_, "GenerateDraw");
    generate_dispatch_kernel_ = gfxCreateComputeKernel(gfx_, gi10_program_, "GenerateDispatch");
    generate_update_tiles_dispatch_kernel_ =
        gfxCreateComputeKernel(gfx_, gi10_program_, "GenerateUpdateTilesDispatch");
    debug_screen_probes_kernel_ =
        gfxCreateGraphicsKernel(gfx_, gi10_program_, debug_screen_probes_draw_state, "DebugScreenProbes");
    debug_hash_grid_cells_kernel_ =
        gfxCreateGraphicsKernel(gfx_, gi10_program_, debug_hash_grid_cells_draw_state, "DebugHashGridCells");

    clear_probe_mask_kernel_          = gfxCreateComputeKernel(gfx_, gi10_program_, "ClearProbeMask");
    filter_probe_mask_kernel_         = gfxCreateComputeKernel(gfx_, gi10_program_, "FilterProbeMask");
    init_cached_tile_lru_kernel_      = gfxCreateComputeKernel(gfx_, gi10_program_, "InitCachedTileLRU");
    reproject_screen_probes_kernel_   = gfxCreateComputeKernel(gfx_, gi10_program_, "ReprojectScreenProbes");
    count_screen_probes_kernel_       = gfxCreateComputeKernel(gfx_, gi10_program_, "CountScreenProbes");
    scatter_screen_probes_kernel_     = gfxCreateComputeKernel(gfx_, gi10_program_, "ScatterScreenProbes");
    spawn_screen_probes_kernel_       = gfxCreateComputeKernel(gfx_, gi10_program_, "SpawnScreenProbes");
    compact_screen_probes_kernel_     = gfxCreateComputeKernel(gfx_, gi10_program_, "CompactScreenProbes");
    patch_screen_probes_kernel_       = gfxCreateComputeKernel(gfx_, gi10_program_, "PatchScreenProbes");
    sample_screen_probes_kernel_      = gfxCreateComputeKernel(gfx_, gi10_program_, "SampleScreenProbes");
    populate_screen_probes_kernel_    = gfxCreateComputeKernel(gfx_, gi10_program_, "PopulateScreenProbes",
           debug_hash_cells_defines.data(), debug_hash_cells_define_count);
    blend_screen_probes_kernel_       = gfxCreateComputeKernel(gfx_, gi10_program_, "BlendScreenProbes");
    reorder_screen_probes_kernel_     = gfxCreateComputeKernel(gfx_, gi10_program_, "ReorderScreenProbes");
    filter_screen_probes_kernel_      = gfxCreateComputeKernel(gfx_, gi10_program_, "FilterScreenProbes");
    project_screen_probes_kernel_     = gfxCreateComputeKernel(gfx_, gi10_program_, "ProjectScreenProbes");
    interpolate_screen_probes_kernel_ = gfxCreateComputeKernel(
        gfx_, gi10_program_, "InterpolateScreenProbes", base_defines.data(), base_define_count);

    purge_tiles_kernel_ = gfxCreateComputeKernel(
        gfx_, gi10_program_, "PurgeTiles", debug_hash_cells_defines.data(), debug_hash_cells_define_count);
    populate_cells_kernel_ = gfxCreateComputeKernel(
        gfx_, gi10_program_, "PopulateCells", resampling_defines.data(), resampling_define_count);
    update_tiles_kernel_  = gfxCreateComputeKernel(gfx_, gi10_program_, "UpdateTiles");
    resolve_cells_kernel_ = gfxCreateComputeKernel(gfx_, gi10_program_, "ResolveCells");

    clear_reservoirs_kernel_    = gfxCreateComputeKernel(gfx_, gi10_program_, "ClearReservoirs");
    generate_reservoirs_kernel_ = gfxCreateComputeKernel(
        gfx_, gi10_program_, "GenerateReservoirs", resampling_defines.data(), resampling_define_count);
    compact_reservoirs_kernel_  = gfxCreateComputeKernel(gfx_, gi10_program_, "CompactReservoirs");
    resample_reservoirs_kernel_ = gfxCreateComputeKernel(
        gfx_, gi10_program_, "ResampleReservoirs", base_defines.data(), base_define_count);

    reproject_gi_kernel_     = gfxCreateComputeKernel(gfx_, gi10_program_, "ReprojectGI");
    filter_blur_mask_kernel_ = gfxCreateComputeKernel(gfx_, gi10_program_, "FilterBlurMask");
    filter_gi_kernel_        = gfxCreateComputeKernel(gfx_, gi10_program_, "FilterGI");

    // Ensure our scratch memory is allocated
    screen_probes_.ensureMemoryIsAllocated(capsaicin);
    hash_grid_cache_.ensureMemoryIsAllocated(capsaicin, options_, debug_view_);
    world_space_restir_.ensureMemoryIsAllocated(capsaicin);
    gi_denoiser_.ensureMemoryIsAllocated(capsaicin);

    // Ensure our fullscreen render target is allocated
    irradiance_buffer_ =
        gfxCreateTexture2D(gfx_, capsaicin.getWidth(), capsaicin.getHeight(), DXGI_FORMAT_R16G16B16A16_FLOAT);
    irradiance_buffer_.setName("Capsaicin_IrradianceBuffer");

    return !!filter_gi_kernel_;
}

void GI10::render(CapsaicinInternal &capsaicin) noexcept
{
    auto const   &render_settings    = capsaicin.getRenderSettings();
    RenderOptions options            = convertOptions(render_settings);
    auto          light_sampler      = capsaicin.getComponent<LightSamplerBounds>();
    auto          blue_noise_sampler = capsaicin.getComponent<BlueNoiseSampler>();
    auto          stratified_sampler = capsaicin.getComponent<StratifiedSampler>();

    bool const needs_debug_view = render_settings.debug_view_ != debug_view_
                               && ((debug_view_.starts_with("HashGridCache_")
                                       && !render_settings.debug_view_.starts_with("HashGridCache_"))
                                   || (!debug_view_.starts_with("HashGridCache_")
                                       && render_settings.debug_view_.starts_with("HashGridCache_")));

    bool const has_delta_lights = (GetDeltaLightCount() != 0);

    bool const needs_recompile =
        (options.gi10_use_resampling != options_.gi10_use_resampling
            || options.gi10_use_alpha_testing != options_.gi10_use_alpha_testing
            || light_sampler->needsRecompile(capsaicin) || needs_debug_view
            || has_delta_lights != has_delta_lights_);

    bool const needs_hash_grid_clear =
        options_.gi10_hash_grid_cache_cell_size != options.gi10_hash_grid_cache_cell_size
        || options_.gi10_hash_grid_cache_debug_mip_level != options.gi10_hash_grid_cache_debug_mip_level
        || options_.gi10_hash_grid_cache_debug_propagate != options.gi10_hash_grid_cache_debug_propagate;

    options_          = options;
    has_delta_lights_ = has_delta_lights;
    debug_view_       = render_settings.debug_view_;

    if (needs_recompile)
    {
        terminate();
        init(capsaicin);
    }

    if (needs_debug_view)
    {
        gfxDestroyKernel(gfx_, debug_screen_probes_kernel_);
        gfxDestroyKernel(gfx_, debug_hash_grid_cells_kernel_);

        GfxDrawState debug_screen_probes_draw_state;
        gfxDrawStateSetColorTarget(debug_screen_probes_draw_state, 0, capsaicin.getAOVBuffer("Debug"));

        GfxDrawState debug_hash_grid_cells_draw_state;
        gfxDrawStateSetColorTarget(debug_hash_grid_cells_draw_state, 0, capsaicin.getAOVBuffer("Debug"));
        gfxDrawStateSetDepthStencilTarget(debug_hash_grid_cells_draw_state, depth_buffer_);
        gfxDrawStateSetCullMode(debug_hash_grid_cells_draw_state, D3D12_CULL_MODE_NONE);

        GfxDrawState debug_material_draw_state;
        gfxDrawStateSetColorTarget(debug_material_draw_state, 0, capsaicin.getAOVBuffer("Debug"));

        debug_screen_probes_kernel_ =
            gfxCreateGraphicsKernel(gfx_, gi10_program_, debug_screen_probes_draw_state, "DebugScreenProbes");
        debug_hash_grid_cells_kernel_ = gfxCreateGraphicsKernel(
            gfx_, gi10_program_, debug_hash_grid_cells_draw_state, "DebugHashGridCells");
    }

    // Clear the hash-grid cache if user's changed the cell size
    if (needs_hash_grid_clear)
    {
        clearHashGridCache(); // clear the radiance cache
    }

    // Ensure our scratch memory is allocated
    screen_probes_.ensureMemoryIsAllocated(capsaicin);
    hash_grid_cache_.ensureMemoryIsAllocated(capsaicin, options_, debug_view_);
    world_space_restir_.ensureMemoryIsAllocated(capsaicin);
    gi_denoiser_.ensureMemoryIsAllocated(capsaicin);

    // Reallocate fullscreen render target if required
    if (irradiance_buffer_.getWidth() != capsaicin.getWidth()
        || irradiance_buffer_.getHeight() != capsaicin.getHeight())
    {
        gfxDestroyTexture(gfx_, irradiance_buffer_);

        irradiance_buffer_ = gfxCreateTexture2D(
            gfx_, capsaicin.getWidth(), capsaicin.getHeight(), DXGI_FORMAT_R16G16B16A16_FLOAT);
        irradiance_buffer_.setName("Capsaicin_IrradianceBuffer");
    }

    // Reserve position values with light bounds sampler
    light_sampler->reserveBoundsValues(screen_probes_.max_ray_count, this);

    // Allocate and populate our constant data
    GfxBuffer gi10_constants               = capsaicin.allocateConstantBuffer<GI10Constants>(1);
    GfxBuffer screen_probes_constants      = capsaicin.allocateConstantBuffer<ScreenProbesConstants>(1);
    GfxBuffer hash_grid_cache_constants    = capsaicin.allocateConstantBuffer<HashGridCacheConstants>(1);
    GfxBuffer world_space_restir_constants = capsaicin.allocateConstantBuffer<WorldSpaceReSTIRConstants>(1);

    GI10Constants gi_constant_data = {};
    auto const   &camera       = capsaicin.getCameraMatrices(render_settings.getOption<bool>("taa_enable"));
    gi_constant_data.view_proj = camera.view_projection;
    gi_constant_data.view_proj_prev     = camera.view_projection_prev;
    gi_constant_data.view_proj_inv      = glm::inverse(glm::dmat4(camera.view_projection));
    gi_constant_data.view_proj_inv_prev = glm::inverse(glm::dmat4(camera.view_projection_prev));
    gi_constant_data.reprojection =
        glm::dmat4(camera.view_projection_prev) * glm::inverse(glm::dmat4(camera.view_projection));
    gi_constant_data.view_inv = camera.inv_view;
    gfxBufferGetData<GI10Constants>(gfx_, gi10_constants)[0] = gi_constant_data;

    ScreenProbesConstants screen_probes_constant_data = {};
    screen_probes_constant_data.cell_size =
        tanf(capsaicin.getCamera().fovY * screen_probes_.probe_size_
             * GFX_MAX(1.0f / capsaicin.getHeight(),
                 (float)capsaicin.getHeight() / (capsaicin.getWidth() * capsaicin.getWidth())));
    screen_probes_constant_data.probe_size            = screen_probes_.probe_size_;
    screen_probes_constant_data.probe_count           = screen_probes_.probe_count_;
    screen_probes_constant_data.probe_mask_mip_count  = screen_probes_.probe_mask_buffers_->getMipLevels();
    screen_probes_constant_data.probe_spawn_tile_size = screen_probes_.probe_spawn_tile_size_;
    screen_probes_constant_data.debug_mode            = SCREENPROBES_DEBUG_RADIANCE;
    if (debug_view_ == "RadianceCachePerDirection")
    {
        screen_probes_constant_data.debug_mode = SCREENPROBES_DEBUG_RADIANCE_PER_DIRECTION;
    }
    gfxBufferGetData<ScreenProbesConstants>(gfx_, screen_probes_constants)[0] = screen_probes_constant_data;

    float cell_size = tanf(capsaicin.getCamera().fovY * options_.gi10_hash_grid_cache_cell_size
                           * GFX_MAX(1.0f / capsaicin.getHeight(),
                               (float)capsaicin.getHeight() / (capsaicin.getWidth() * capsaicin.getWidth())));
    HashGridCacheConstants hash_grid_cache_constant_data = {};
    hash_grid_cache_constant_data.cell_size              = cell_size;
    hash_grid_cache_constant_data.tile_size       = cell_size * options_.gi10_hash_grid_cache_tile_cell_ratio;
    hash_grid_cache_constant_data.tile_cell_ratio = (float)options_.gi10_hash_grid_cache_tile_cell_ratio;
    hash_grid_cache_constant_data.num_buckets     = hash_grid_cache_.num_buckets_;
    hash_grid_cache_constant_data.num_cells       = hash_grid_cache_.num_cells_;
    hash_grid_cache_constant_data.num_tiles       = hash_grid_cache_.num_tiles_;
    hash_grid_cache_constant_data.num_tiles_per_bucket        = hash_grid_cache_.num_tiles_per_bucket_;
    hash_grid_cache_constant_data.size_tile_mip0              = hash_grid_cache_.size_tile_mip0_;
    hash_grid_cache_constant_data.size_tile_mip1              = hash_grid_cache_.size_tile_mip1_;
    hash_grid_cache_constant_data.size_tile_mip2              = hash_grid_cache_.size_tile_mip2_;
    hash_grid_cache_constant_data.size_tile_mip3              = hash_grid_cache_.size_tile_mip3_;
    hash_grid_cache_constant_data.num_cells_per_tile_mip0     = hash_grid_cache_.num_cells_per_tile_mip0_;
    hash_grid_cache_constant_data.num_cells_per_tile_mip1     = hash_grid_cache_.num_cells_per_tile_mip1_;
    hash_grid_cache_constant_data.num_cells_per_tile_mip2     = hash_grid_cache_.num_cells_per_tile_mip2_;
    hash_grid_cache_constant_data.num_cells_per_tile_mip3     = hash_grid_cache_.num_cells_per_tile_mip3_;
    hash_grid_cache_constant_data.num_cells_per_tile          = hash_grid_cache_.num_cells_per_tile_;
    hash_grid_cache_constant_data.first_cell_offset_tile_mip0 = hash_grid_cache_.first_cell_offset_tile_mip0_;
    hash_grid_cache_constant_data.first_cell_offset_tile_mip1 = hash_grid_cache_.first_cell_offset_tile_mip1_;
    hash_grid_cache_constant_data.first_cell_offset_tile_mip2 = hash_grid_cache_.first_cell_offset_tile_mip2_;
    hash_grid_cache_constant_data.first_cell_offset_tile_mip3 = hash_grid_cache_.first_cell_offset_tile_mip3_;
    hash_grid_cache_constant_data.buffer_ping_pong = hash_grid_cache_.radiance_cache_hash_buffer_ping_pong_;
    hash_grid_cache_constant_data.max_sample_count = options_.gi10_hash_grid_cache_max_sample_count;
    hash_grid_cache_constant_data.debug_mip_level  = options_.gi10_hash_grid_cache_debug_mip_level;
    hash_grid_cache_constant_data.debug_propagate  = (uint)options_.gi10_hash_grid_cache_debug_propagate;
    hash_grid_cache_constant_data.debug_max_cell_decay = options_.gi10_hash_grid_cache_debug_max_cell_decay;
    hash_grid_cache_constant_data.debug_mode           = HASHGRIDCACHE_DEBUG_RADIANCE;
    if (debug_view_ == "HashGridCache_RadianceSampleCount")
    {
        hash_grid_cache_constant_data.debug_mode = HASHGRIDCACHE_DEBUG_RADIANCE_SAMPLE_COUNT;
    }
    else if (debug_view_ == "HashGridCache_FilteredRadiance")
    {
        hash_grid_cache_constant_data.debug_mode = HASHGRIDCACHE_DEBUG_FILTERED_RADIANCE;
    }
    else if (debug_view_ == "HashGridCache_FilteredGain")
    {
        hash_grid_cache_constant_data.debug_mode = HASHGRIDCACHE_DEBUG_FILTERING_GAIN;
    }
    else if (debug_view_ == "HashGridCache_FilteredSampleCount")
    {
        hash_grid_cache_constant_data.debug_mode = HASHGRIDCACHE_DEBUG_FILTERED_SAMPLE_COUNT;
    }
    else if (debug_view_ == "HashGridCache_FilteredMipLevel")
    {
        hash_grid_cache_constant_data.debug_mode = HASHGRIDCACHE_DEBUG_FILTERED_MIP_LEVEL;
    }
    else if (debug_view_ == "HashGridCache_Occupancy")
    {
        hash_grid_cache_constant_data.debug_mode = HASHGRIDCACHE_DEBUG_TILE_OCCUPANCY;
    }
    if (debug_view_.starts_with("HashGridCache_Filtered"))
    {
        options_.gi10_hash_grid_cache_debug_mip_level = 0; // Filtering is adaptive...
    }
    if (debug_view_.starts_with("HashGridCache_Filtered")
        || render_settings.debug_view_ == "HashGridCache_Occupancy")
    {
        options_.gi10_hash_grid_cache_debug_propagate = false;
    }
    else if (render_settings.getOption<int>("gi10_hash_grid_cache_debug_mip_level") > 0)
    {
        options_.gi10_hash_grid_cache_debug_propagate = true;
    }

    gfxBufferGetData<HashGridCacheConstants>(gfx_, hash_grid_cache_constants)[0] =
        hash_grid_cache_constant_data;

    WorldSpaceReSTIRConstants world_space_restir_constant_data = {};
    world_space_restir_constant_data.cell_size =
        tanf(capsaicin.getCamera().fovY * options_.gi10_reservoir_cache_cell_size
             * GFX_MAX(1.0f / capsaicin.getHeight(),
                 (float)capsaicin.getHeight() / (capsaicin.getWidth() * capsaicin.getWidth())));
    world_space_restir_constant_data.num_cells            = WorldSpaceReSTIR::kConstant_NumCells;
    world_space_restir_constant_data.num_entries_per_cell = WorldSpaceReSTIR::kConstant_NumEntriesPerCell;
    gfxBufferGetData<WorldSpaceReSTIRConstants>(gfx_, world_space_restir_constants)[0] =
        world_space_restir_constant_data;

    // Bind the shader parameters
    uint32_t const buffer_dimensions[] = {capsaicin.getWidth(), capsaicin.getHeight()};

    float const near_far[] = {capsaicin.getCamera().nearZ, capsaicin.getCamera().farZ};

    GfxBuffer transform_buffer = capsaicin.getTransformBuffer();

    transform_buffer.setStride(sizeof(float4)); // this is to align with UE5 where transforms are stored as
                                                // 4x3 matrices and fetched using 3x float4 reads

    // Some NVIDIA-specific fix
    vertex_buffers_.resize(capsaicin.getVertexBufferCount());

    for (uint32_t i = 0; i < capsaicin.getVertexBufferCount(); ++i)
    {
        vertex_buffers_[i] = capsaicin.getVertexBuffers()[i];

        if (gfx_.getVendorId() == 0x10DEu) // NVIDIA
        {
            vertex_buffers_[i].setStride(4);
        }
    }

    gfxProgramSetParameter(gfx_, gi10_program_, "g_Eye", capsaicin.getCamera().eye);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_NearFar", near_far);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_FrameIndex", capsaicin.getFrameIndex());
    gfxProgramSetParameter(gfx_, gi10_program_, "g_InvDeviceZ", capsaicin.getInvDeviceZ());
    gfxProgramSetParameter(gfx_, gi10_program_, "g_PreviousEye", previous_camera_.eye);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_BufferDimensions", buffer_dimensions);
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_UseDirectLighting", options_.gi10_use_direct_lighting ? 1 : 0);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_PreViewTranslation", capsaicin.getPreViewTranslation());
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_DisableAlbedoTextures", options_.gi10_disable_albedo_textures ? 1 : 0);

    gfxProgramSetParameter(gfx_, gi10_program_, "g_DepthBuffer", capsaicin.getAOVBuffer("VisibilityDepth"));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_NormalBuffer", capsaicin.getAOVBuffer("Normal"));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_DetailsBuffer", capsaicin.getAOVBuffer("Details"));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_VelocityBuffer", capsaicin.getAOVBuffer("Velocity"));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_OcclusionAndBentNormalBuffer",
        capsaicin.getAOVBuffer("OcclusionAndBentNormal"));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_NearFieldGlobalIlluminationBuffer",
        capsaicin.getAOVBuffer("NearFieldGlobalIllumination"));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_VisibilityBuffer", capsaicin.getAOVBuffer("Visibility"));
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_PreviousDepthBuffer", capsaicin.getAOVBuffer("PrevVisibilityDepth"));
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_PreviousNormalBuffer", capsaicin.getAOVBuffer("PrevNormal"));
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_PreviousDetailsBuffer", capsaicin.getAOVBuffer("PrevDetails"));

    stratified_sampler->addProgramParameters(capsaicin, gi10_program_);

    blue_noise_sampler->addProgramParameters(capsaicin, gi10_program_);

    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_IndexBuffers", capsaicin.getIndexBuffers(), capsaicin.getIndexBufferCount());
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_VertexBuffers", vertex_buffers_.data(), capsaicin.getVertexBufferCount());

    gfxProgramSetParameter(gfx_, gi10_program_, "g_MeshBuffer", capsaicin.getMeshBuffer());
    gfxProgramSetParameter(gfx_, gi10_program_, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
    gfxProgramSetParameter(gfx_, gi10_program_, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
    gfxProgramSetParameter(gfx_, gi10_program_, "g_TransformBuffer", transform_buffer);

    gfxProgramSetParameter(gfx_, gi10_program_, "g_IrradianceBuffer", irradiance_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_DrawCommandBuffer", draw_command_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_DispatchCommandBuffer", dispatch_command_buffer_);
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_GlobalIlluminationBuffer", capsaicin.getAOVBuffer("GlobalIllumination"));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_PrevCombinedIlluminationBuffer",
        capsaicin.getAOVBuffer("PrevCombinedIllumination"));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_OcclusionAndBentNormalBuffer",
        capsaicin.getAOVBuffer("OcclusionAndBentNormal"));

    gfxProgramSetParameter(gfx_, gi10_program_, "g_Scene", capsaicin.getAccelerationStructure());

    light_sampler->addProgramParameters(capsaicin, gi10_program_);

    gfxProgramSetParameter(gfx_, gi10_program_, "g_EnvironmentBuffer", capsaicin.getEnvironmentBuffer());

    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_TextureMaps", capsaicin.getTextures(), capsaicin.getTextureCount());

    gfxProgramSetParameter(gfx_, gi10_program_, "g_NearestSampler", capsaicin.getNearestSampler());
    gfxProgramSetParameter(gfx_, gi10_program_, "g_LinearSampler", capsaicin.getLinearSampler());
    gfxProgramSetParameter(gfx_, gi10_program_, "g_TextureSampler", capsaicin.getLinearSampler());

    gfxProgramSetParameter(gfx_, gi10_program_, "g_GI10Constants", gi10_constants);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbesConstants", screen_probes_constants);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_HashGridCacheConstants", hash_grid_cache_constants);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_WorldSpaceReSTIRConstants", world_space_restir_constants);

    // Bind the screen probes shader parameters
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeMask",
        screen_probes_.probe_mask_buffers_[screen_probes_.probe_buffer_index_]);

    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeBuffer",
        screen_probes_.probe_buffers_[screen_probes_.probe_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeMaskBuffer",
        screen_probes_.probe_mask_buffers_[screen_probes_.probe_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_PreviousProbeBuffer",
        screen_probes_.probe_buffers_[1 - screen_probes_.probe_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_PreviousProbeMaskBuffer",
        screen_probes_.probe_mask_buffers_[1 - screen_probes_.probe_buffer_index_]);

    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeSHBuffer",
        screen_probes_.probe_sh_buffers_[screen_probes_.probe_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_PreviousProbeSHBuffer",
        screen_probes_.probe_sh_buffers_[1 - screen_probes_.probe_buffer_index_]);

    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_ScreenProbes_ProbeSpawnBuffer", screen_probes_.probe_spawn_buffers_[0]);
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_ScreenProbes_ProbeSpawnTileCountBuffer", screen_probes_.probe_spawn_tile_count_buffer_);
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_ScreenProbes_ProbeSpawnScanBuffer", screen_probes_.probe_spawn_scan_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeSpawnIndexBuffer",
        screen_probes_.probe_spawn_index_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeSpawnProbeBuffer",
        screen_probes_.probe_spawn_probe_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeSpawnSampleBuffer",
        screen_probes_.probe_spawn_sample_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeSpawnRadianceBuffer",
        screen_probes_.probe_spawn_radiance_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_PreviousProbeSpawnBuffer",
        screen_probes_.probe_spawn_buffers_[1]);

    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_ScreenProbes_EmptyTileBuffer", screen_probes_.probe_empty_tile_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_EmptyTileCountBuffer",
        screen_probes_.probe_empty_tile_count_buffer_);
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_ScreenProbes_OverrideTileBuffer", screen_probes_.probe_override_tile_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_OverrideTileCountBuffer",
        screen_probes_.probe_override_tile_count_buffer_);

    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileBuffer",
        screen_probes_.probe_cached_tile_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileIndexBuffer",
        screen_probes_.probe_cached_tile_index_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileLRUBuffer",
        screen_probes_.probe_cached_tile_lru_buffers_[screen_probes_.probe_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileLRUFlagBuffer",
        screen_probes_.probe_cached_tile_lru_flag_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileLRUCountBuffer",
        screen_probes_.probe_cached_tile_lru_count_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileLRUIndexBuffer",
        screen_probes_.probe_cached_tile_lru_index_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileMRUBuffer",
        screen_probes_.probe_cached_tile_mru_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileMRUCountBuffer",
        screen_probes_.probe_cached_tile_mru_count_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileListBuffer",
        screen_probes_.probe_cached_tile_list_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileListCountBuffer",
        screen_probes_.probe_cached_tile_list_count_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileListIndexBuffer",
        screen_probes_.probe_cached_tile_list_index_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileListElementBuffer",
        screen_probes_.probe_cached_tile_list_element_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeCachedTileListElementCountBuffer",
        screen_probes_.probe_cached_tile_list_element_count_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_PreviousProbeCachedTileLRUBuffer",
        screen_probes_.probe_cached_tile_lru_buffers_[1 - screen_probes_.probe_buffer_index_]);

    // Bind the hash-grid cache shader parameters
    gfxProgramSetParameter(gfx_, gi10_program_, "g_HashGridCache_BuffersUint",
        hash_grid_cache_.radiance_cache_hash_buffer_uint_,
        ARRAYSIZE(hash_grid_cache_.radiance_cache_hash_buffer_uint_));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_HashGridCache_BuffersUint2",
        hash_grid_cache_.radiance_cache_hash_buffer_uint2_,
        ARRAYSIZE(hash_grid_cache_.radiance_cache_hash_buffer_uint2_));
    gfxProgramSetParameter(gfx_, gi10_program_, "g_HashGridCache_BuffersFloat4",
        hash_grid_cache_.radiance_cache_hash_buffer_float4_,
        ARRAYSIZE(hash_grid_cache_.radiance_cache_hash_buffer_float4_));

    // Bind the world-space ReSTIR shader parameters
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_HashBuffer",
        world_space_restir_
            .reservoir_hash_buffers_[world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_HashCountBuffer",
        world_space_restir_
            .reservoir_hash_count_buffers_[world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_HashIndexBuffer",
        world_space_restir_
            .reservoir_hash_index_buffers_[world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_HashValueBuffer",
        world_space_restir_
            .reservoir_hash_value_buffers_[world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_Reservoir_HashListBuffer", world_space_restir_.reservoir_hash_list_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_HashListCountBuffer",
        world_space_restir_.reservoir_hash_list_count_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_PreviousHashBuffer",
        world_space_restir_
            .reservoir_hash_buffers_[1 - world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_PreviousHashCountBuffer",
        world_space_restir_
            .reservoir_hash_count_buffers_[1 - world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_PreviousHashIndexBuffer",
        world_space_restir_
            .reservoir_hash_index_buffers_[1 - world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_PreviousHashValueBuffer",
        world_space_restir_
            .reservoir_hash_value_buffers_[1 - world_space_restir_.reservoir_indirect_sample_buffer_index_]);

    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_IndirectSampleBuffer",
        world_space_restir_.reservoir_indirect_sample_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_IndirectSampleNormalBuffer",
        world_space_restir_.reservoir_indirect_sample_normal_buffers_
            [world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_IndirectSampleMaterialBuffer",
        world_space_restir_.reservoir_indirect_sample_material_buffer_);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_IndirectSampleReservoirBuffer",
        world_space_restir_.reservoir_indirect_sample_reservoir_buffers_
            [world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_PreviousIndirectSampleNormalBuffer",
        world_space_restir_.reservoir_indirect_sample_normal_buffers_
            [1 - world_space_restir_.reservoir_indirect_sample_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_Reservoir_PreviousIndirectSampleReservoirBuffer",
        world_space_restir_.reservoir_indirect_sample_reservoir_buffers_
            [1 - world_space_restir_.reservoir_indirect_sample_buffer_index_]);

    // Bind the GI denoiser shader parameters
    gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_BlurMask", gi_denoiser_.blur_masks_[0]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_BlurredBlurMask", gi_denoiser_.blur_masks_[1]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_ColorBuffer",
        gi_denoiser_.color_buffers_[gi_denoiser_.color_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_ColorDeltaBuffer",
        gi_denoiser_.color_delta_buffers_[gi_denoiser_.color_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_PreviousColorBuffer",
        gi_denoiser_.color_buffers_[1 - gi_denoiser_.color_buffer_index_]);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_PreviousColorDeltaBuffer",
        gi_denoiser_.color_delta_buffers_[1 - gi_denoiser_.color_buffer_index_]);

    gfxProgramSetParameter(
        gfx_, gi10_program_, "g_GIDenoiser_BlurSampleCountBuffer", gi_denoiser_.blur_sample_count_buffer_);

    // Purge the unused tiles (square or cube of cells) within our hash-grid cache
    {
        TimedSection const timed_section(*this, "PurgeRadianceCache");

        GfxBuffer radiance_cache_packed_tile_count_buffer =
            (hash_grid_cache_.radiance_cache_hash_buffer_ping_pong_
                    ? hash_grid_cache_.radiance_cache_packed_tile_count_buffer0_
                    : hash_grid_cache_.radiance_cache_packed_tile_count_buffer1_);

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, purge_tiles_kernel_);
        generateDispatch(radiance_cache_packed_tile_count_buffer, num_threads[0]);

        gfxCommandBindKernel(gfx_, clear_counters_kernel_);
        gfxCommandDispatch(gfx_, 1, 1, 1);
        gfxCommandBindKernel(gfx_, purge_tiles_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Reproject the previous screen probes into the current frame
    {
        TimedSection const timed_section(*this, "ReprojectScreenProbes");

        uint32_t const probe_count[] = {
            (buffer_dimensions[0] + screen_probes_.probe_size_ - 1) / screen_probes_.probe_size_,
            (buffer_dimensions[1] + screen_probes_.probe_size_ - 1) / screen_probes_.probe_size_};

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, reproject_screen_probes_kernel_);
        uint32_t const  num_groups_x =
            (probe_count[0] * screen_probes_.probe_size_ + num_threads[0] - 1) / num_threads[0];
        uint32_t const num_groups_y =
            (probe_count[1] * screen_probes_.probe_size_ + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, reproject_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }

    // Look up our cached probes into the current viewspace
    {
        TimedSection const timed_section(*this, "LookupScreenProbes");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, count_screen_probes_kernel_);
        uint32_t const  num_groups_x =
            (screen_probes_.probe_count_[0] * screen_probes_.probe_count_[1] + num_threads[0] - 1)
            / num_threads[0];

        gfxCommandClearBuffer(gfx_, screen_probes_.probe_cached_tile_list_count_buffer_);
        gfxCommandBindKernel(gfx_, count_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
        gfxCommandScanSum(gfx_, kGfxDataType_Uint, screen_probes_.probe_cached_tile_list_index_buffer_,
            screen_probes_.probe_cached_tile_list_count_buffer_);

        generateDispatch(screen_probes_.probe_cached_tile_list_element_count_buffer_,
            *gfxKernelGetNumThreads(gfx_, scatter_screen_probes_kernel_));

        gfxCommandBindKernel(gfx_, scatter_screen_probes_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Spawn our new screen probes
    {
        TimedSection const timed_section(*this, "SpawnScreenProbes");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, spawn_screen_probes_kernel_);
        uint32_t const  num_groups_x =
            (screen_probes_.max_probe_spawn_count + num_threads[0] - 1) / num_threads[0];
        GFX_ASSERT(*num_threads == *gfxKernelGetNumThreads(gfx_, compact_screen_probes_kernel_));

        gfxCommandBindKernel(gfx_, spawn_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
        /*gfxCommandScanSum(gfx_, kGfxDataType_Uint, screen_probes_.probe_spawn_index_buffer_,
            screen_probes_.probe_spawn_scan_buffer_);
        gfxCommandBindKernel(gfx_, compact_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);*/
    }

    // Stochastically patch the overridable tiles using empty ones (a.k.a., adaptive sampling)
    {
        TimedSection const timed_section(*this, "PatchScreenProbes");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, patch_screen_probes_kernel_);
        generateDispatch(screen_probes_.probe_empty_tile_count_buffer_, num_threads[0]);

        gfxCommandBindKernel(gfx_, patch_screen_probes_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Importance sample the spawned probes
    {
        TimedSection const timed_section(*this, "SampleScreenProbes");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, sample_screen_probes_kernel_);
        uint32_t const  num_groups_x = (screen_probes_.max_ray_count + num_threads[0] - 1) / num_threads[0];

        gfxCommandBindKernel(gfx_, sample_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
    }

    // Now we go and populate the newly spawned probes
    {
        TimedSection const timed_section(*this, "PopulateScreenProbes");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, populate_screen_probes_kernel_);
        uint32_t const  num_groups_x = (screen_probes_.max_ray_count + num_threads[0] - 1) / num_threads[0];

        gfxCommandBindKernel(gfx_, populate_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
    }

    // Update light sampling data structure
    {
        light_sampler->update(capsaicin, *this);
    }

    // Clear our cache prior to generating new reservoirs
    if (options_.gi10_use_resampling)
    {
        TimedSection const timed_section(*this, "ClearReservoirs");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, clear_reservoirs_kernel_);
        uint32_t const  num_groups_x =
            (WorldSpaceReSTIR::kConstant_NumEntries + num_threads[0] - 1) / num_threads[0];

        gfxCommandBindKernel(gfx_, clear_reservoirs_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
    }

    // Generate reservoirs for our secondary hit points
    {
        TimedSection const timed_section(*this, "GenerateReservoirs");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, generate_reservoirs_kernel_);
        generateDispatch(hash_grid_cache_.radiance_cache_visibility_count_buffer_, num_threads[0]);

        gfxCommandBindKernel(gfx_, generate_reservoirs_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Compact the reservoir caching structure
    if (options_.gi10_use_resampling)
    {
        TimedSection const timed_section(*this, "CompactReservoirs");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, compact_reservoirs_kernel_);
        generateDispatch(world_space_restir_.reservoir_hash_list_count_buffer_, num_threads[0]);

        gfxCommandScanSum(gfx_, kGfxDataType_Uint,
            world_space_restir_
                .reservoir_hash_index_buffers_[world_space_restir_.reservoir_indirect_sample_buffer_index_],
            world_space_restir_
                .reservoir_hash_count_buffers_[world_space_restir_.reservoir_indirect_sample_buffer_index_]);
        gfxCommandBindKernel(gfx_, compact_reservoirs_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Perform world-space reservoir reuse if enabled
    if (options_.gi10_use_resampling)
    {
        TimedSection const timed_section(*this, "ResampleReservoirs");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, resample_reservoirs_kernel_);
        generateDispatch(hash_grid_cache_.radiance_cache_visibility_ray_count_buffer_, num_threads[0]);

        gfxCommandBindKernel(gfx_, resample_reservoirs_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Populate the cells of our world-space hash-grid radiance cache
    {
        TimedSection const timed_section(*this, "PopulateRadianceCache");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, populate_cells_kernel_);
        generateDispatch(hash_grid_cache_.radiance_cache_visibility_ray_count_buffer_, num_threads[0]);

        gfxCommandBindKernel(gfx_, populate_cells_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Update our tiles using the result of the raytracing
    {
        TimedSection const timed_section(*this, "UpdateRadianceCache");

        gfxCommandBindKernel(gfx_, generate_update_tiles_dispatch_kernel_);
        gfxCommandDispatch(gfx_, 1, 1, 1);

        gfxCommandBindKernel(gfx_, update_tiles_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Resolve our cells into the per-query storage
    {
        TimedSection const timed_section(*this, "ResolveRadianceCache");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, resolve_cells_kernel_);
        generateDispatch(hash_grid_cache_.radiance_cache_visibility_ray_count_buffer_, num_threads[0]);

        gfxCommandBindKernel(gfx_, resolve_cells_kernel_);
        gfxCommandDispatchIndirect(gfx_, dispatch_command_buffer_);
    }

    // Blend the new results into the probe grid
    {
        TimedSection const timed_section(*this, "BlendScreenProbes");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, blend_screen_probes_kernel_);
        uint32_t const  num_groups_x = (screen_probes_.max_ray_count + num_threads[0] - 1) / num_threads[0];

        gfxCommandBindKernel(gfx_, blend_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
        gfxCommandCopyTexture(gfx_, screen_probes_.probe_buffers_[1 - screen_probes_.probe_buffer_index_],
            screen_probes_.probe_buffers_[screen_probes_.probe_buffer_index_]);
    }

    // Re-order the cached probes so the least-recently used entries are evicted first
    {
        TimedSection const timed_section(*this, "ReorderScreenProbes");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, reorder_screen_probes_kernel_);
        uint32_t const  num_groups_x =
            (screen_probes_.probe_count_[0] * screen_probes_.probe_count_[1] + num_threads[0] - 1)
            / num_threads[0];

        gfxCommandScanSum(gfx_, kGfxDataType_Uint, screen_probes_.probe_cached_tile_lru_index_buffer_,
            screen_probes_.probe_cached_tile_lru_flag_buffer_);
        gfxCommandBindKernel(gfx_, reorder_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
    }

    // Filter the probe mask
    {
        TimedSection const timed_section(*this, "FilterProbeMask");

        uint32_t const probe_count[] = {
            (buffer_dimensions[0] + screen_probes_.probe_size_ - 1) / screen_probes_.probe_size_,
            (buffer_dimensions[1] + screen_probes_.probe_size_ - 1) / screen_probes_.probe_size_};

        uint32_t const *num_threads          = gfxKernelGetNumThreads(gfx_, filter_probe_mask_kernel_);
        uint32_t const  probe_mask_mip_count = gfxCalculateMipCount(probe_count[0], probe_count[1]);

        gfxCommandBindKernel(gfx_, filter_probe_mask_kernel_);

        for (uint32_t i = 1; i < probe_mask_mip_count; ++i)
        {
            gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_InProbeMaskBuffer",
                screen_probes_.probe_mask_buffers_[screen_probes_.probe_buffer_index_], i - 1);
            gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_OutProbeMaskBuffer",
                screen_probes_.probe_mask_buffers_[screen_probes_.probe_buffer_index_], i);

            uint32_t const num_groups_x =
                (GFX_MAX(probe_count[0] >> i, 1u) + num_threads[0] - 1) / num_threads[0];
            uint32_t const num_groups_y =
                (GFX_MAX(probe_count[1] >> i, 1u) + num_threads[1] - 1) / num_threads[1];

            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        }
    }

    // Filter the screen probes
    {
        TimedSection const timed_section(*this, "FilterScreenProbes");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, filter_screen_probes_kernel_);
        uint32_t const  num_groups_x = (screen_probes_.max_ray_count + num_threads[0] - 1) / num_threads[0];

        gfxCommandBindKernel(gfx_, filter_screen_probes_kernel_);

        for (uint32_t i = 0; i < 2; ++i)
        {
            bool const is_first_pass = (i == 0);

            int32_t const blur_direction[] = {is_first_pass ? 1 : 0, is_first_pass ? 0 : 1};

            gfxProgramSetParameter(gfx_, gi10_program_, "g_BlurDirection", blur_direction);

            gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_ProbeBuffer",
                screen_probes_.probe_buffers_[is_first_pass ? 1 - screen_probes_.probe_buffer_index_
                                                            : screen_probes_.probe_buffer_index_]);
            gfxProgramSetParameter(gfx_, gi10_program_, "g_ScreenProbes_PreviousProbeBuffer",
                screen_probes_.probe_buffers_[is_first_pass ? screen_probes_.probe_buffer_index_
                                                            : 1 - screen_probes_.probe_buffer_index_]);

            gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
        }
    }

    // Project the screen probes into SH basis
    {
        TimedSection const timed_section(*this, "ProjectScreenProbes");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, project_screen_probes_kernel_);
        uint32_t const  num_groups_x = (screen_probes_.max_ray_count + num_threads[0] - 1) / num_threads[0];

        gfxCommandBindKernel(gfx_, project_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
    }

    // Interpolate the screen probes to target resolution
    {
        TimedSection const timed_section(*this, "InterpolateScreenProbes");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, interpolate_screen_probes_kernel_);
        uint32_t const  num_groups_x = (buffer_dimensions[0] + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_dimensions[1] + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, interpolate_screen_probes_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }

    // Reproject the previous frame's global illumination
    {
        TimedSection const timed_section(*this, "ReprojectGI");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, reproject_gi_kernel_);
        uint32_t const  num_groups_x = (buffer_dimensions[0] + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_dimensions[1] + num_threads[1] - 1) / num_threads[1];

        gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_ColorBuffer",
            gi_denoiser_.color_buffers_[gi_denoiser_.color_buffer_index_]);
        gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_PreviousColorBuffer",
            gi_denoiser_.color_buffers_[1 - gi_denoiser_.color_buffer_index_]);

        gfxCommandBindKernel(gfx_, reproject_gi_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }

    // Filter the blur mask prior to spatial filtering
    {
        TimedSection const timed_section(*this, "FilterBlurMask");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, filter_blur_mask_kernel_);
        uint32_t const  num_groups_x = (buffer_dimensions[0] + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_dimensions[1] + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, filter_blur_mask_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }

    // And blur our disocclusions
    {
        TimedSection const timed_section(*this, "FilterGI");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, filter_gi_kernel_);
        uint32_t const  num_groups_x = (buffer_dimensions[0] + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_dimensions[1] + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, filter_gi_kernel_);

        for (uint32_t i = 0; i < 2; ++i)
        {
            bool const is_first_pass = (i == 0);

            int32_t const blur_direction[] = {is_first_pass ? 1 : 0, is_first_pass ? 0 : 1};

            gfxProgramSetParameter(gfx_, gi10_program_, "g_BlurDirection", blur_direction);

            gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_ColorBuffer",
                gi_denoiser_.color_buffers_[is_first_pass ? 1 - gi_denoiser_.color_buffer_index_
                                                          : gi_denoiser_.color_buffer_index_]);
            gfxProgramSetParameter(gfx_, gi10_program_, "g_GIDenoiser_PreviousColorBuffer",
                gi_denoiser_.color_buffers_[is_first_pass ? gi_denoiser_.color_buffer_index_
                                                          : 1 - gi_denoiser_.color_buffer_index_]);
            gfxProgramSetParameter(gfx_, gi10_program_, "g_IrradianceBuffer", irradiance_buffer_);

            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        }
    }

    gfxProgramSetParameter(gfx_, gi10_program_, "g_IrradianceBuffer", irradiance_buffer_);

    // Finally, we can resolve the filtered lighting with the per-pixel material details
    {
        gfxProgramSetParameter(gfx_, gi10_program_, "g_TextureSampler", capsaicin.getAnisotropicSampler());

        TimedSection const timed_section(*this, "ResolveGI10");

        gfxCommandBindKernel(gfx_, resolve_gi10_kernel_);
        gfxCommandDraw(gfx_, 3);
    }

    // Debug the screen-space radiance cache if asked to do so
    if (debug_view_.starts_with("RadianceCache"))
    {
        TimedSection const timed_section(*this, "DebugScreenProbes");

        gfxCommandBindKernel(gfx_, debug_screen_probes_kernel_);
        gfxCommandDraw(gfx_, 3);
    }

    // Debug the hash-grid cells if asked to do so
    if (debug_view_.starts_with("HashGridCache_"))
    {
        TimedSection const timed_section(*this, "DebugRadianceCache");

        gfxCommandBindKernel(gfx_, generate_draw_kernel_);
        gfxCommandDispatch(gfx_, 1, 1, 1);
        gfxCommandClearTexture(gfx_, depth_buffer_);
        gfxCommandBindKernel(gfx_, debug_hash_grid_cells_kernel_);
        gfxCommandMultiDrawIndirect(gfx_, draw_command_buffer_, 1);
    }

    // Release our constant buffers
    gfxDestroyBuffer(gfx_, gi10_constants);
    gfxDestroyBuffer(gfx_, screen_probes_constants);
    gfxDestroyBuffer(gfx_, hash_grid_cache_constants);
    gfxDestroyBuffer(gfx_, world_space_restir_constants);

    // Flip the buffers
    screen_probes_.probe_buffer_index_ = (1 - screen_probes_.probe_buffer_index_);
    hash_grid_cache_.radiance_cache_hash_buffer_ping_pong_ =
        (1 - hash_grid_cache_.radiance_cache_hash_buffer_ping_pong_);
    world_space_restir_.reservoir_indirect_sample_buffer_index_ =
        (1 - world_space_restir_.reservoir_indirect_sample_buffer_index_);
    gi_denoiser_.color_buffer_index_ = (1 - gi_denoiser_.color_buffer_index_);

    // And update our history
    previous_camera_ = capsaicin.getCamera();
}

void GI10::terminate()
{
    gfxDestroyTexture(gfx_, depth_buffer_);
    gfxDestroyTexture(gfx_, irradiance_buffer_);
    gfxDestroyBuffer(gfx_, draw_command_buffer_);
    gfxDestroyBuffer(gfx_, dispatch_command_buffer_);

    gfxDestroyProgram(gfx_, gi10_program_);
    gfxDestroyKernel(gfx_, resolve_gi10_kernel_);
    gfxDestroyKernel(gfx_, clear_counters_kernel_);
    gfxDestroyKernel(gfx_, generate_draw_kernel_);
    gfxDestroyKernel(gfx_, generate_dispatch_kernel_);
    gfxDestroyKernel(gfx_, generate_update_tiles_dispatch_kernel_);
    gfxDestroyKernel(gfx_, debug_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, debug_hash_grid_cells_kernel_);

    gfxDestroyKernel(gfx_, clear_probe_mask_kernel_);
    gfxDestroyKernel(gfx_, filter_probe_mask_kernel_);
    gfxDestroyKernel(gfx_, init_cached_tile_lru_kernel_);
    gfxDestroyKernel(gfx_, reproject_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, count_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, scatter_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, spawn_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, compact_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, patch_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, sample_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, populate_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, blend_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, reorder_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, filter_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, project_screen_probes_kernel_);
    gfxDestroyKernel(gfx_, interpolate_screen_probes_kernel_);

    gfxDestroyKernel(gfx_, purge_tiles_kernel_);
    gfxDestroyKernel(gfx_, populate_cells_kernel_);
    gfxDestroyKernel(gfx_, update_tiles_kernel_);
    gfxDestroyKernel(gfx_, resolve_cells_kernel_);

    gfxDestroyKernel(gfx_, clear_reservoirs_kernel_);
    gfxDestroyKernel(gfx_, generate_reservoirs_kernel_);
    gfxDestroyKernel(gfx_, compact_reservoirs_kernel_);
    gfxDestroyKernel(gfx_, resample_reservoirs_kernel_);

    gfxDestroyKernel(gfx_, reproject_gi_kernel_);
    gfxDestroyKernel(gfx_, filter_blur_mask_kernel_);
    gfxDestroyKernel(gfx_, filter_gi_kernel_);

    irradiance_buffer_ = {};
}

void GI10::generateDispatch(GfxBuffer count_buffer, uint32_t group_size)
{
    gfxProgramSetParameter(gfx_, gi10_program_, "g_GroupSize", group_size);
    gfxProgramSetParameter(gfx_, gi10_program_, "g_CountBuffer", count_buffer);

    gfxCommandBindKernel(gfx_, generate_dispatch_kernel_);
    gfxCommandDispatch(gfx_, 1, 1, 1);
}

void GI10::clearHashGridCache()
{
    if (hash_grid_cache_.radiance_cache_hash_buffer_)
    {
        gfxCommandClearBuffer(gfx_, hash_grid_cache_.radiance_cache_hash_buffer_); // clear the radiance cache
        gfxCommandClearBuffer(gfx_, hash_grid_cache_.radiance_cache_packed_tile_count_buffer0_);
        gfxCommandClearBuffer(gfx_, hash_grid_cache_.radiance_cache_packed_tile_count_buffer1_);
    }
}
} // namespace Capsaicin
