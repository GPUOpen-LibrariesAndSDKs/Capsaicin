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
#pragma once

#include "reference_pt_shared.h"
#include "render_technique.h"

namespace Capsaicin
{
class ReferencePT : public RenderTechnique
{
public:
    ReferencePT();
    ~ReferencePT();

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        uint32_t reference_pt_bounce_count = 3; /**< Maximum number of bounces each path can take */
        uint32_t reference_pt_min_rr_bounces =
            2; /**< Number of bounces a path takes before Russian roulette can be used */
        uint32_t reference_pt_sample_count = 1; /**< Number of paths to trace per pixel per frame */
        bool     reference_pt_disable_albedo_materials =
            false; /**< Sets material to fixed diffuse gray on first intersected surface */
        bool reference_pt_disable_direct_lighting =
            false; /**< Disable sampling direct lighting on first intersection */
        bool reference_pt_disable_specular_lighting =
            true; /**< Disable specular sampling/evaluation and therefore setting materials to diffuse only
                   */
    };

    /**
     * Convert render settings to internal options format.
     * @param settings Current render settings.
     * @returns The options converted.
     */
    static RenderOptions convertOptions(RenderSettings const &settings) noexcept;

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

protected:
    /**
     * Initialise internal computer kernels.
     * @param capsaicin The current capsaicin context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool initKernels(CapsaicinInternal const &capsaicin) noexcept;

    void terminate() noexcept;
    /**
     * Check if camera has changed.
     * @param currentCamera The current camera.
     * @return True if camera has changed, False otherwise.
     */
    bool checkCameraUpdated(GfxCamera const &currentCamera) noexcept;

    GfxBuffer     rayCameraData;
    GfxTexture    accumulationBuffer; /**< Buffer used to store pixel running average, .w= number of samples */
    RayCamera     cameraData;
    uint2         bufferDimensions = uint2(0);
    GfxCamera     camera           = {};
    RenderOptions options;

    GfxSamplerState textureSampler;
    GfxProgram      reference_pt_program_;
    GfxKernel       reference_pt_kernel_;
};
} // namespace Capsaicin
