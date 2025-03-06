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

#include "render_technique.h"

namespace Capsaicin
{
class SSGI final : public RenderTechnique
{
public:
    SSGI();
    ~SSGI() override;

    SSGI(const SSGI &other)                = delete;
    SSGI(SSGI &&other) noexcept            = delete;
    SSGI &operator=(const SSGI &other)     = delete;
    SSGI &operator=(SSGI &&other) noexcept = delete;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        uint32_t ssgi_slice_count_   = 1;
        uint32_t ssgi_step_count_    = 2;
        float    ssgi_view_radius_   = 0.2F;
        float    ssgi_falloff_range_ = 0.05F;
        bool     ssgi_unroll_kernel_ = false; // BE CAREFUL: if true, check shader for slice and step counts
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @return The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    [[nodiscard]] ComponentList getComponents() const noexcept override;

    /**
     * Gets the required list of shared textures needed for the current render technique.
     * @return A list of all required shared textures.
     */
    [[nodiscard]] SharedTextureList getSharedTextures() const noexcept override;

    /**
     * Gets a list of any debug views provided by the current render technique.
     * @return A list of all supported debug views.
     */
    [[nodiscard]] DebugViewList getDebugViews() const noexcept override;

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
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

protected:
    void initializeStaticResources(CapsaicinInternal const &capsaicin);
    void initializeKernels(CapsaicinInternal const &capsaicin);
    void destroyStaticResources() const;
    void destroyKernels() const;

    RenderOptions options_;

    // Buffers

    // Samplers
    GfxSamplerState point_sampler_;

    // Kernels
    GfxProgram ssgi_program_;
    GfxKernel  main_kernel_;
    GfxKernel  main_unrolled_kernel_;

    // Debug kernels
    GfxProgram debug_occlusion_program_;
    GfxKernel  debug_occlusion_kernel_;
    GfxProgram debug_bent_normal_program_;
    GfxKernel  debug_bent_normal_kernel_;
};
} // namespace Capsaicin
