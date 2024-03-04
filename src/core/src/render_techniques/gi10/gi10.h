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
#pragma once

#include "gi10_shared.h"
#include "render_technique.h"

#include <gfx_scene.h>

namespace Capsaicin
{
class GI10 : public RenderTechnique
{
    GFX_NON_COPYABLE(GI10);

public:
    GI10();
    ~GI10();

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        bool  gi10_use_dxr10                            = false;
        bool  gi10_use_resampling                       = false;
        bool  gi10_use_alpha_testing                    = true;
        bool  gi10_use_direct_lighting                  = true;
        bool  gi10_disable_albedo_textures              = false;
        bool  gi10_disable_specular_materials           = false;
        float gi10_hash_grid_cache_cell_size            = 32.0f;
        float gi10_hash_grid_cache_min_cell_size        = 1e-1f;
        int   gi10_hash_grid_cache_tile_cell_ratio      = 8;    // 8x8               = 64
        int   gi10_hash_grid_cache_num_buckets          = 12;   // 1 << 12           = 4096
        int   gi10_hash_grid_cache_num_tiles_per_bucket = 4;    // 1 <<  4           = 16     total : 4194304
        float gi10_hash_grid_cache_max_sample_count     = 16.f; //
        int   gi10_hash_grid_cache_debug_mip_level      = 0;
        bool  gi10_hash_grid_cache_debug_propagate      = false;
        int   gi10_hash_grid_cache_debug_max_cell_decay = 0; // Debug cells touched this frame
        bool  gi10_hash_grid_cache_debug_stats          = false;
        int   gi10_hash_grid_cache_debug_max_bucket_overflow = 64;
        float gi10_reservoir_cache_cell_size                 = 16.0f;

        bool  gi10_glossy_reflections_halfres                            = true;
        int   gi10_glossy_reflections_denoiser_mode                      = 1; // Atrous Ratio Estimator
        bool  gi10_glossy_reflections_cleanup_fireflies                  = true;
        float gi10_glossy_reflections_low_roughness_threshold            = 0.2f;
        float gi10_glossy_reflections_high_roughness_threshold           = 0.6f;
        int   gi10_glossy_reflections_atrous_pass_count                  = 4;
        int   gi10_glossy_reflections_full_radius                        = 11;
        int   gi10_glossy_reflections_half_radius                        = 11;
        int   gi10_glossy_reflections_mark_fireflies_half_radius         = 3;
        int   gi10_glossy_reflections_mark_fireflies_full_radius         = 2;
        float gi10_glossy_reflections_mark_fireflies_half_low_threshold  = 0.0f;
        float gi10_glossy_reflections_mark_fireflies_full_low_threshold  = 0.0f;
        float gi10_glossy_reflections_mark_fireflies_half_high_threshold = 1.0f;
        float gi10_glossy_reflections_mark_fireflies_full_high_threshold = 1.0f;
        int   gi10_glossy_reflections_cleanup_fireflies_half_radius      = 2;
        int   gi10_glossy_reflections_cleanup_fireflies_full_radius      = 1;
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @returns The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    ComponentList getComponents() const noexcept override;

    /**
     * Gets the required list of AOVs needed for the current render technique.
     * @return A list of all required AOV buffers.
     */
    AOVList getAOVs() const noexcept override;

    /**
     * Gets a list of any debug views provided by the current render technique.
     * @return A list of all supported debug views.
     */
    DebugViewList getDebugViews() const noexcept override;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(CapsaicinInternal const &capsaicin) noexcept override;

    /**
     * Perform render operations.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void render(CapsaicinInternal &capsaicin) noexcept override;

    /**
     * Render GUI options.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void renderGUI(CapsaicinInternal &capsaicin) const noexcept override;

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

protected:
    void generateDispatch(GfxBuffer count_buffer, uint32_t group_size);
    void generateDispatchRays(GfxBuffer count_buffer);
    void clearHashGridCache();

    class Base
    {
        GFX_NON_COPYABLE(Base);

    public:
        Base(GI10 &gi10);

        GfxContext const &gfx_; //
        GI10             &self; //
    };

    // Used for spawning rays from the gbuffers at 1/4 res. by default and interpolating the indirect lighting
    // at primary path vertices.
    struct ScreenProbes : public Base
    {
        enum SamplingMode
        {
            kSamplingMode_OneSpp = 0,
            kSamplingMode_QuarterSpp,
            kSamplingMode_SixteenthSpp,

            kSamplingMode_Count
        };

        ScreenProbes(GI10 &gi10);
        ~ScreenProbes();

        void ensureMemoryIsAllocated(CapsaicinInternal const &capsaicin);

        static constexpr uint32_t     probe_size_    = 8;
        static constexpr SamplingMode sampling_mode_ = kSamplingMode_QuarterSpp;
        uint2                         probe_count_;

        static constexpr uint32_t probe_spawn_tile_size_ =
            (sampling_mode_ == kSamplingMode_QuarterSpp        ? (probe_size_ << 1)
                : sampling_mode_ == kSamplingMode_SixteenthSpp ? (probe_size_ << 2)
                                                               : probe_size_);
        uint32_t   probe_buffer_index_;
        uint32_t   max_probe_spawn_count;
        uint32_t   max_ray_count;
        GfxTexture probe_buffers_[2];
        GfxTexture probe_mask_buffers_[2];
        GfxBuffer  probe_sh_buffers_[2];
        GfxBuffer  probe_spawn_buffers_[2];
        GfxBuffer  probe_spawn_scan_buffer_;
        GfxBuffer  probe_spawn_index_buffer_;
        GfxBuffer  probe_spawn_probe_buffer_;
        GfxBuffer  probe_spawn_sample_buffer_;
        GfxBuffer  probe_spawn_radiance_buffer_;
        GfxBuffer  probe_empty_tile_buffer_;
        GfxBuffer  probe_empty_tile_count_buffer_;
        GfxBuffer  probe_override_tile_buffer_;
        GfxBuffer  probe_override_tile_count_buffer_;
        GfxTexture probe_cached_tile_buffer_;
        GfxTexture probe_cached_tile_index_buffer_;
        GfxBuffer  probe_cached_tile_lru_buffers_[2];
        GfxBuffer  probe_cached_tile_lru_flag_buffer_;
        GfxBuffer  probe_cached_tile_lru_count_buffer_;
        GfxBuffer  probe_cached_tile_lru_index_buffer_;
        GfxBuffer  probe_cached_tile_mru_buffer_;
        GfxBuffer  probe_cached_tile_mru_count_buffer_;
        GfxBuffer  probe_cached_tile_list_buffer_;
        GfxBuffer  probe_cached_tile_list_count_buffer_;
        GfxBuffer  probe_cached_tile_list_index_buffer_;
        GfxBuffer  probe_cached_tile_list_element_buffer_;
        GfxBuffer  probe_cached_tile_list_element_count_buffer_;
    };

    // Used for caching in world space the lighting calculated at primary (same as screen probes) and
    // secondary path vertices.
    struct HashGridCache : public Base
    {
        HashGridCache(GI10 &gi10);
        ~HashGridCache();

        void ensureMemoryIsAllocated(CapsaicinInternal const &capsaicin, RenderOptions const &options,
            std::string_view const &debug_view);

        uint32_t max_ray_count_;
        uint32_t num_buckets_;
        uint32_t num_tiles_;
        uint32_t num_cells_;
        uint32_t num_tiles_per_bucket_;
        uint32_t size_tile_mip0_;
        uint32_t size_tile_mip1_;
        uint32_t size_tile_mip2_;
        uint32_t size_tile_mip3_;
        uint32_t num_cells_per_tile_mip0_;
        uint32_t num_cells_per_tile_mip1_;
        uint32_t num_cells_per_tile_mip2_;
        uint32_t num_cells_per_tile_mip3_;
        uint32_t num_cells_per_tile_; // all mips
        uint32_t first_cell_offset_tile_mip0_;
        uint32_t first_cell_offset_tile_mip1_;
        uint32_t first_cell_offset_tile_mip2_;
        uint32_t first_cell_offset_tile_mip3_;
        uint32_t debug_bucket_occupancy_histogram_size_;
        uint32_t debug_bucket_overflow_histogram_size_;
        uint32_t debug_stats_size_;
        uint64_t debug_total_memory_size_in_bytes_;

        GfxBuffer  radiance_cache_hash_buffer_float_[HASHGRID_FLOAT_BUFFER_COUNT];
        GfxBuffer  radiance_cache_hash_buffer_uint_[HASHGRID_UINT_BUFFER_COUNT];
        GfxBuffer  radiance_cache_hash_buffer_uint2_[HASHGRID_UINT2_BUFFER_COUNT];
        GfxBuffer  radiance_cache_hash_buffer_float4_[HASHGRID_FLOAT4_BUFFER_COUNT];
        uint32_t   radiance_cache_hash_buffer_ping_pong_;
        GfxBuffer &radiance_cache_hash_buffer_;
        GfxBuffer &radiance_cache_decay_cell_buffer_;
        GfxBuffer &radiance_cache_decay_tile_buffer_;
        GfxBuffer &radiance_cache_value_buffer_;
        GfxBuffer &radiance_cache_update_tile_buffer_;
        GfxBuffer &radiance_cache_update_tile_count_buffer_;
        GfxBuffer &radiance_cache_update_cell_value_buffer_;
        GfxBuffer &radiance_cache_visibility_buffer_;
        GfxBuffer &radiance_cache_visibility_count_buffer_;
        GfxBuffer &radiance_cache_visibility_cell_buffer_;
        GfxBuffer &radiance_cache_visibility_query_buffer_;
        GfxBuffer &radiance_cache_visibility_ray_buffer_;
        GfxBuffer &radiance_cache_visibility_ray_count_buffer_;
        GfxBuffer &radiance_cache_packed_tile_count_buffer0_;
        GfxBuffer &radiance_cache_packed_tile_count_buffer1_;
        GfxBuffer &radiance_cache_packed_tile_index_buffer0_;
        GfxBuffer &radiance_cache_packed_tile_index_buffer1_;
        GfxBuffer &radiance_cache_debug_cell_buffer_;
        GfxBuffer &radiance_cache_debug_bucket_occupancy_buffer_;
        GfxBuffer &radiance_cache_debug_bucket_overflow_count_buffer_;
        GfxBuffer &radiance_cache_debug_bucket_overflow_buffer_;
        GfxBuffer &radiance_cache_debug_free_bucket_buffer_;
        GfxBuffer &radiance_cache_debug_used_bucket_buffer_;
        GfxBuffer &radiance_cache_debug_stats_buffer_;
        GfxBuffer  radiance_cache_debug_stats_readback_buffers_[kGfxConstant_BackBufferCount];
        bool       radiance_cache_debug_stats_readback_is_pending_[kGfxConstant_BackBufferCount];

        std::vector<float> debug_bucket_occupancy_histogram_;
        std::vector<float> debug_bucket_overflow_histogram_;
        float              debug_free_bucket_count_;
        float              debug_used_bucket_count_;
    };

    // Used for sampling the direct lighting at primary (i.e., direct lighting; disabled by default) and
    // secondary path vertices.
    struct WorldSpaceReSTIR : public Base
    {
        enum Constants
        {
            kConstant_NumCells          = 0x40000u,
            kConstant_NumEntriesPerCell = 0x10u,
            kConstant_NumEntries        = kConstant_NumCells * kConstant_NumEntriesPerCell
        };

        WorldSpaceReSTIR(GI10 &gi10);
        ~WorldSpaceReSTIR();

        void ensureMemoryIsAllocated(CapsaicinInternal const &capsaicin);

        GfxBuffer reservoir_hash_buffers_[2];
        GfxBuffer reservoir_hash_count_buffers_[2];
        GfxBuffer reservoir_hash_index_buffers_[2];
        GfxBuffer reservoir_hash_value_buffers_[2];
        GfxBuffer reservoir_hash_list_buffer_;
        GfxBuffer reservoir_hash_list_count_buffer_;
        GfxBuffer reservoir_indirect_sample_buffer_;
        GfxBuffer reservoir_indirect_sample_normal_buffers_[2];
        GfxBuffer reservoir_indirect_sample_material_buffer_;
        GfxBuffer reservoir_indirect_sample_reservoir_buffers_[2];
        uint32_t  reservoir_indirect_sample_buffer_index_;
    };

    // Used for tracing and denoising glossy reflections.
    struct GlossyReflections : public Base
    {
        GlossyReflections(GI10 &gi10);
        ~GlossyReflections();

        void ensureMemoryIsAllocated(CapsaicinInternal const &capsaicin);

        GfxTexture  texture_float_[GLOSSY_REFLECTION_TEXTURE_FLOAT_COUNT];
        GfxTexture  texture_float4_[GLOSSY_REFLECTION_TEXTURE_FLOAT4_COUNT];
        GfxTexture &fireflies_buffer_;
        GfxTexture &specular_buffer_;
        GfxTexture &direction_buffer_;
        GfxTexture &reflections_buffer_;
        GfxTexture &standard_dev_buffer_;
        GfxTexture &reflections_buffer0_;
        GfxTexture &average_squared_buffer0_;
        GfxTexture &reflections_buffer1_;
        GfxTexture &average_squared_buffer1_;

        GfxBuffer rt_sample_buffer_;
        GfxBuffer rt_sample_count_buffer_;
    };

    // Used for image-space spatiotemporal denoising of the probes' interpolation results.
    struct GIDenoiser : public Base
    {
        GIDenoiser(GI10 &gi10);
        ~GIDenoiser();

        void ensureMemoryIsAllocated(CapsaicinInternal const &capsaicin);

        GfxTexture blur_masks_[2];
        GfxTexture color_buffers_[2];
        GfxTexture color_delta_buffers_[2];
        uint32_t   color_buffer_index_;
        GfxBuffer  blur_sample_count_buffer_;
    };

    GfxCamera        previous_camera_;
    RenderOptions    options_;
    std::string_view debug_view_;
    GfxTexture       depth_buffer_;
    GfxTexture       irradiance_buffer_;
    GfxBuffer        draw_command_buffer_;
    GfxBuffer        dispatch_command_buffer_;

    // GI-1.0 building blocks:
    ScreenProbes      screen_probes_;
    HashGridCache     hash_grid_cache_;
    WorldSpaceReSTIR  world_space_restir_;
    GlossyReflections glossy_reflections_;
    GIDenoiser        gi_denoiser_;

    // GI-1.0 kernels:
    GfxProgram gi10_program_;
    GfxKernel  resolve_gi10_kernel_;
    GfxKernel  clear_counters_kernel_;
    GfxKernel  generate_draw_kernel_;
    GfxKernel  generate_dispatch_kernel_;
    GfxKernel  generate_dispatch_rays_kernel_;
    GfxKernel  generate_update_tiles_dispatch_kernel_;
    GfxKernel  debug_screen_probes_kernel_;
    GfxKernel  debug_hash_grid_cells_kernel_;
    GfxKernel  debug_reflection_kernel_;

    // Screen probes kernels:
    GfxKernel clear_probe_mask_kernel_;
    GfxKernel filter_probe_mask_kernel_;
    GfxKernel init_cached_tile_lru_kernel_;
    GfxKernel reproject_screen_probes_kernel_;
    GfxKernel count_screen_probes_kernel_;
    GfxKernel scatter_screen_probes_kernel_;
    GfxKernel spawn_screen_probes_kernel_;
    GfxKernel compact_screen_probes_kernel_;
    GfxKernel patch_screen_probes_kernel_;
    GfxKernel sample_screen_probes_kernel_;
    GfxKernel populate_screen_probes_kernel_;
    GfxKernel blend_screen_probes_kernel_;
    GfxKernel reorder_screen_probes_kernel_;
    GfxKernel filter_screen_probes_kernel_;
    GfxKernel project_screen_probes_kernel_;
    GfxKernel interpolate_screen_probes_kernel_;
    GfxSbt    sbt_;

    // Hash grid cache kernels:
    GfxKernel purge_tiles_kernel_;
    GfxKernel populate_cells_kernel_;
    GfxKernel update_tiles_kernel_;
    GfxKernel resolve_cells_kernel_;
    GfxKernel clear_bucket_overflow_count_kernel_;
    GfxKernel clear_bucket_occupancy_kernel_;
    GfxKernel clear_bucket_overflow_kernel_;
    GfxKernel build_bucket_stats_kernel_;
    GfxKernel format_bucket_occupancy_kernel_;
    GfxKernel format_bucket_overflow_kernel_;

    // World-space ReSTIR kernels:
    GfxKernel clear_reservoirs_kernel_;
    GfxKernel generate_reservoirs_kernel_;
    GfxKernel compact_reservoirs_kernel_;
    GfxKernel resample_reservoirs_kernel_;

    // Reflection kernels:
    GfxKernel trace_reflections_kernel_;
    GfxKernel resolve_reflections_kernels_[5];
    GfxKernel reproject_reflections_kernel_;
    GfxKernel mark_fireflies_kernel_;
    GfxKernel cleanup_fireflies_kernel_;
    GfxKernel no_denoiser_reflections_kernel_;

    // GI denoiser kernels:
    GfxKernel reproject_gi_kernel_;
    GfxKernel filter_blur_mask_kernel_;
    GfxKernel filter_gi_kernel_;
};
} // namespace Capsaicin
