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
class VisibilityBuffer : public RenderTechnique
{
public:
    VisibilityBuffer();
    ~VisibilityBuffer();

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        bool visibility_buffer_use_rt = false; /**< Use Ray Tracing instead of rasterisation */
        bool visibility_buffer_use_rt_dxr10 =
            false; /**< Use dxr 1.0 ray-tracing pipelines instead of inline rt
                   (only effects if visibility_buffer_use_rt is enabled) */
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
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

private:
    /**
     * Initialise internal visibility buffer kernel.
     * @param capsaicin The current capsaicin context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool initKernel(CapsaicinInternal const &capsaicin) noexcept;

    RenderOptions options;
    GfxKernel     disocclusion_mask_kernel_;
    GfxProgram    disocclusion_mask_program_;
    GfxKernel     visibility_buffer_kernel_;
    GfxProgram    visibility_buffer_program_;
    GfxSbt        visibility_buffer_sbt_;
    GfxProgram    debug_velocities_program_;
    GfxKernel     debug_velocities_kernel_;
    GfxProgram    debug_material_program_;
    GfxKernel     debug_material_kernel_;
    GfxProgram    debug_dxr10_program_;
    GfxKernel     debug_dxr10_kernel_;
    GfxSbt        debug_dxr10_sbt_;
};
} // namespace Capsaicin
