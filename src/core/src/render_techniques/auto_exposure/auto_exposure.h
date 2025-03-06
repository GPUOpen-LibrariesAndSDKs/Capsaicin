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
class AutoExposure final : public RenderTechnique
{
public:
    AutoExposure();
    ~AutoExposure() override;

    AutoExposure(AutoExposure const &other)                = delete;
    AutoExposure(AutoExposure &&other) noexcept            = delete;
    AutoExposure &operator=(AutoExposure const &other)     = delete;
    AutoExposure &operator=(AutoExposure &&other) noexcept = delete;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    [[nodiscard]] RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        bool  auto_exposure_enable = true;
        float auto_exposure_value  = 0.0F;
        float auto_exposure_bias   = 1.0F;
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @return The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Gets a list of any shared buffers used by the current render technique.
     * @return A list of all supported buffers.
     */
    [[nodiscard]] SharedBufferList getSharedBuffers() const noexcept override;

    /**
     * Gets the required list of shared textures needed for the current render technique.
     * @return A list of all required shared textures.
     */
    [[nodiscard]] SharedTextureList getSharedTextures() const noexcept override;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    [[nodiscard]] bool init(CapsaicinInternal const &capsaicin) noexcept override;

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

private:
    /**
     * Initialise buffers and kernels required when automatically detecting exposure.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    [[nodiscard]] bool initAutoExposure(CapsaicinInternal const &capsaicin) noexcept;

    RenderOptions options;

    GfxBuffer histogramBuffer;
    GfxBuffer keySceneLuminanceBuffer;
    std::vector<std::pair<float, std::pair<GfxBuffer, float>>>
        exposureBufferTemp; /**< Buffer used to copy back calculated exposure into CPU memory */

    GfxProgram exposureProgram;
    GfxKernel  histogramKernel;
    GfxKernel  exposureKernel;
};
} // namespace Capsaicin
