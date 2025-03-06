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
class Bloom final : public RenderTechnique
{
public:
    Bloom();
    ~Bloom() override;

    Bloom(Bloom const &other)                = delete;
    Bloom(Bloom &&other) noexcept            = delete;
    Bloom &operator=(Bloom const &other)     = delete;
    Bloom &operator=(Bloom &&other) noexcept = delete;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    [[nodiscard]] RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        bool     bloom_enable    = false;
        uint32_t bloom_radius    = 1;    /**< Radius of the bloom blur as a screen percentage */
        float    bloom_clip_bias = 1.0F; /**< Bias applied to clipping range */
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
     * Calculate the ideal blur radius and passes to get the desired coverage.
     * @param screenDimensions Current rendering resolution.
     * @return True if radius changed and kernels need recompiling.
     */
    bool calculateBlurParameters(uint2 const &screenDimensions) noexcept;

    /**
     * Initialise the blur kernels.
     * @return True if initialisation succeeded, False otherwise.
     */
    [[nodiscard]] bool initBlurKernel() noexcept;

    RenderOptions options;
    uint32_t      blurPasses = 2;
    uint32_t      blurRadius = 4;

    GfxTexture bloomTexture;

    GfxProgram blurProgram;
    GfxKernel  blurKernel;
    GfxKernel  blur2Kernel;
    GfxProgram combineProgram;
    GfxKernel  combineKernel;
};
} // namespace Capsaicin
