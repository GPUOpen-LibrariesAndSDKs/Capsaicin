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

#include "../../geometry/path_tracing_shared.h"
#include "render_technique.h"

#include <gfx_scene.h>

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
        uint32_t reference_pt_bounce_count = 30; /**< Maximum number of bounces each path can take */
        uint32_t reference_pt_min_rr_bounces =
            2; /**< Number of bounces a path takes before Russian roulette can be used */
        uint32_t reference_pt_sample_count = 1; /**< Number of paths to trace per pixel per frame */
        bool     reference_pt_disable_albedo_materials =
            false; /**< Sets material to fixed diffuse gray on first intersected surface */
        bool reference_pt_disable_direct_lighting =
            false; /**< Disable sampling direct lighting on first intersection */
        bool reference_pt_disable_specular_materials =
            false; /**< Disable specular sampling/evaluation essentially setting materials to diffuse only */
        bool reference_pt_nee_only    = false; /**< Disable light contributions from source other than NEE */
        bool reference_pt_disable_nee = false; /**< Disable light contributions from Next Event Estimation */
        bool reference_pt_nee_reservoir_resampling =
            false;                           /**< Use reservoir resampling for selecting NEE light samples */
        bool reference_pt_use_dxr10 = false; /**< Use dxr 1.0 ray-tracing pipelines instead of inline rt */
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

    /**
     * Render GUI options.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void renderGUI(CapsaicinInternal &capsaicin) const noexcept override;

protected:
    /**
     * Initialise internal computer kernels.
     * @param capsaicin The current capsaicin context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool initKernels(CapsaicinInternal const &capsaicin) noexcept;

    /**
     * Check if camera has changed.
     * @param currentCamera The current camera.
     * @return True if camera has changed, False otherwise.
     */
    bool checkCameraUpdated(GfxCamera const &currentCamera) noexcept;

    /**
     * Check if kernels needs to be recompiled.
     * @param capsaicin The current capsaicin context.
     * @param newOptions New render options.
     * @return True if kernels needs to be recompiled, False otherwise.
     */
    virtual bool needsRecompile(CapsaicinInternal &capsaicin, RenderOptions const &newOptions) noexcept;

    virtual void setupSbt(CapsaicinInternal &capsaicin) noexcept;

    virtual void setupPTKernel(CapsaicinInternal const &capsaicin,
        std::vector<GfxLocalRootSignatureAssociation>  &local_root_signature_associations,
        std::vector<std::string> &defines, std::vector<std::string> &exports,
        std::vector<std::string> &subobjects) noexcept;

    virtual char const *getProgramName() noexcept;

    GfxBuffer  rayCameraData;
    GfxTexture accumulationBuffer; /**< Buffer used to store pixel running average, .w= number of samples */
    RayCamera  cameraData;
    uint2      bufferDimensions = uint2(0);
    GfxCamera  camera           = {};
    RenderOptions options;

    GfxProgram reference_pt_program_;
    GfxKernel  reference_pt_kernel_;
    GfxSbt     reference_pt_sbt_;
};
} // namespace Capsaicin
