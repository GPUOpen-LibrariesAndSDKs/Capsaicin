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
class Lens final : public RenderTechnique
{
public:
    Lens();
    ~Lens() override;

    Lens(Lens const &other)                = delete;
    Lens(Lens &&other) noexcept            = delete;
    Lens &operator=(Lens const &other)     = delete;
    Lens &operator=(Lens &&other) noexcept = delete;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    [[nodiscard]] RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        bool  lens_chromatic_enable    = false; /**< Enable chromatic aberration */
        bool  lens_vignette_enable     = false; /**< Enable vignette */
        bool  lens_film_grain_enable   = false; /**< Enable film grain */
        float lens_chromatic_intensity = 0.4F;  /**< Chromatic Aberration intensity */
        float lens_vignette_intensity  = 0.3F;  /**< Vignette intensity */
        float lens_filmgrain_scale     = 0.01F; /**< Film grain grain size relative to screen */
        float lens_filmgrain_amount    = 0.25F; /**< Film grain amount */
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @return The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

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
    [[nodiscard]] bool initLens(CapsaicinInternal const &capsaicin) noexcept;

    RenderOptions options;

    uint32_t grainSeed = 0;
    double   grainTime = 0.0;

    GfxProgram lensProgram;
    GfxKernel  lensMapKernel;
    GfxTexture chromaticAberrationTexture; /**< Temp output texture required when using CA */
};
} // namespace Capsaicin
