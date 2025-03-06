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

#include "lens.h"

#include "../../components/blue_noise_sampler/blue_noise_sampler.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
Lens::Lens()
    : RenderTechnique("Lens")
{}

Lens::~Lens()
{
    Lens::terminate();
}

RenderOptionList Lens::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(lens_chromatic_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(lens_vignette_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(lens_film_grain_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(lens_chromatic_intensity, options));
    newOptions.emplace(RENDER_OPTION_MAKE(lens_vignette_intensity, options));
    newOptions.emplace(RENDER_OPTION_MAKE(lens_filmgrain_scale, options));
    newOptions.emplace(RENDER_OPTION_MAKE(lens_filmgrain_amount, options));
    return newOptions;
}

Lens::RenderOptions Lens::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(lens_chromatic_enable, newOptions, options)
    RENDER_OPTION_GET(lens_vignette_enable, newOptions, options)
    RENDER_OPTION_GET(lens_film_grain_enable, newOptions, options)
    RENDER_OPTION_GET(lens_chromatic_intensity, newOptions, options)
    RENDER_OPTION_GET(lens_vignette_intensity, newOptions, options)
    RENDER_OPTION_GET(lens_filmgrain_scale, newOptions, options)
    RENDER_OPTION_GET(lens_filmgrain_amount, newOptions, options)
    return newOptions;
}

SharedTextureList Lens::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::ReadWrite});
    textures.push_back({"ColorScaled", SharedTexture::Access::ReadWrite, SharedTexture::Flags::Optional});
    return textures;
}

bool Lens::init(CapsaicinInternal const &capsaicin) noexcept
{
    // Reset internal grain values
    grainSeed = 0;
    grainTime = 0.0;

    if (options.lens_chromatic_enable || options.lens_vignette_enable || options.lens_film_grain_enable)
    {
        // Create kernels
        lensProgram = capsaicin.createProgram("render_techniques/lens/lens");

        return initLens(capsaicin);
    }
    return true;
}

void Lens::render(CapsaicinInternal &capsaicin) noexcept
{
    auto const newOptions = convertOptions(capsaicin.getOptions());

    if (!newOptions.lens_chromatic_enable && !newOptions.lens_vignette_enable
        && !newOptions.lens_film_grain_enable)
    {
        if (options.lens_chromatic_enable || options.lens_vignette_enable || options.lens_film_grain_enable)
        {
            // Destroy resources when not being used
            terminate();
            options.lens_chromatic_enable  = false;
            options.lens_vignette_enable   = false;
            options.lens_film_grain_enable = false;
        }
        return;
    }

    bool const recompile = options.lens_chromatic_enable != newOptions.lens_chromatic_enable
                        || options.lens_vignette_enable != newOptions.lens_vignette_enable
                        || options.lens_film_grain_enable != newOptions.lens_film_grain_enable;
    bool const reInit =
        !(options.lens_chromatic_enable || options.lens_vignette_enable || options.lens_film_grain_enable)
        && (newOptions.lens_chromatic_enable || newOptions.lens_vignette_enable
            || newOptions.lens_film_grain_enable);
    options = newOptions;

    if (reInit)
    {
        if (!init(capsaicin))
        {
            return;
        }
    }
    else if (recompile)
    {
        if (!initLens(capsaicin))
        {
            return;
        }
    }

    bool const usesScaling = capsaicin.hasSharedTexture("ColorScaled")
                          && capsaicin.hasOption<bool>("taa_enable")
                          && capsaicin.getOption<bool>("taa_enable");
    auto const &input =
        !usesScaling ? capsaicin.getSharedTexture("Color") : capsaicin.getSharedTexture("ColorScaled");
    auto const &output = options.lens_chromatic_enable ? chromaticAberrationTexture : input;

    auto const bufferDimensions =
        !usesScaling ? capsaicin.getRenderDimensions() : capsaicin.getWindowDimensions();
    gfxProgramSetParameter(gfx_, lensProgram, "g_BufferDimensions", bufferDimensions);
    gfxProgramSetParameter(gfx_, lensProgram, "g_InputBuffer", input);
    gfxProgramSetParameter(gfx_, lensProgram, "g_OutputBuffer", output);
    if (options.lens_chromatic_enable)
    {
        gfxProgramSetParameter(gfx_, lensProgram, "g_LinearClampSampler", capsaicin.getLinearSampler());
        gfxProgramSetParameter(gfx_, lensProgram, "g_ChromAb", options.lens_chromatic_intensity);
    }
    if (options.lens_vignette_enable)
    {
        gfxProgramSetParameter(gfx_, lensProgram, "g_Vignette", options.lens_vignette_intensity);
    }
    if (options.lens_film_grain_enable)
    {
        gfxProgramSetParameter(gfx_, lensProgram, "g_GrainScale", options.lens_filmgrain_scale);
        gfxProgramSetParameter(gfx_, lensProgram, "g_GrainAmount", options.lens_filmgrain_amount);
        grainTime += capsaicin.getFrameTime();
        if (grainTime >= 0.02)
        {
            ++grainSeed;
            grainTime = 0.0;
        }
        gfxProgramSetParameter(gfx_, lensProgram, "g_GrainSeed", grainSeed);
    }
    {
        TimedSection const timed_section(*this, "Lens");
        uint32_t const     numGroupsX = (bufferDimensions.x + 8 - 1) / 8;
        uint32_t const     numGroupsY = (bufferDimensions.y + 8 - 1) / 8;
        gfxCommandBindKernel(gfx_, lensMapKernel);
        gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
    }
    if (options.lens_chromatic_enable)
    {
        // When using chromatic aberration we need to render to a separate output texture to prevent
        // corruption.
        gfxCommandCopyTexture(gfx_, input, chromaticAberrationTexture);
    }
}

void Lens::terminate() noexcept
{
    gfxDestroyKernel(gfx_, lensMapKernel);
    lensMapKernel = {};
    gfxDestroyProgram(gfx_, lensProgram);
    lensProgram = {};
    gfxDestroyTexture(gfx_, chromaticAberrationTexture);
    chromaticAberrationTexture = {};
}

void Lens::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    bool chromaticEnabled = capsaicin.getOption<bool>("lens_chromatic_enable");
    if (ImGui::Checkbox("Enable Chromatic Aberration", &chromaticEnabled))
    {
        capsaicin.setOption("lens_chromatic_enable", chromaticEnabled);
    }
    if (chromaticEnabled)
    {
        float chromAB = capsaicin.getOption<float>("lens_chromatic_intensity");
        if (ImGui::DragFloat("Chromatic Aberration Intensity", &chromAB, 5e-3F, 0.0F, 20.0F))
        {
            capsaicin.setOption("lens_chromatic_intensity", chromAB);
        }
    }
    bool vignetteEnabled = capsaicin.getOption<bool>("lens_vignette_enable");
    if (ImGui::Checkbox("Enable Vignette", &vignetteEnabled))
    {
        capsaicin.setOption("lens_vignette_enable", vignetteEnabled);
    }
    if (vignetteEnabled)
    {
        float vignette = capsaicin.getOption<float>("lens_vignette_intensity");
        if (ImGui::DragFloat("Vignette Intensity", &vignette, 5e-3F, 0.0F, 2.0F))
        {
            capsaicin.setOption("lens_vignette_intensity", vignette);
        }
    }
    bool filmGrainEnabled = capsaicin.getOption<bool>("lens_film_grain_enable");
    if (ImGui::Checkbox("Enable Film Grain", &filmGrainEnabled))
    {
        capsaicin.setOption("lens_film_grain_enable", filmGrainEnabled);
    }
    if (filmGrainEnabled)
    {
        float grainScale = capsaicin.getOption<float>("lens_filmgrain_scale");
        if (ImGui::DragFloat("Film Grain Scale", &grainScale, 5e-3F, 0.01F, 20.0F))
        {
            capsaicin.setOption("lens_filmgrain_scale", grainScale);
        }
        float grainAmount = capsaicin.getOption<float>("lens_filmgrain_amount");
        if (ImGui::DragFloat("Film Grain Amount", &grainAmount, 5e-3F, 0.0F, 20.0F))
        {
            capsaicin.setOption("lens_filmgrain_amount", grainAmount);
        }
    }
}

bool Lens::initLens(CapsaicinInternal const &capsaicin) noexcept
{
    gfxDestroyKernel(gfx_, lensMapKernel);

    std::vector<char const *> defines;
    if (options.lens_chromatic_enable)
    {
        defines.push_back("ENABLE_CHROMATIC");
        if (!chromaticAberrationTexture)
        {
            bool const usesScaling = capsaicin.hasSharedTexture("ColorScaled")
                                  && capsaicin.hasOption<bool>("taa_enable")
                                  && capsaicin.getOption<bool>("taa_enable");
            chromaticAberrationTexture = usesScaling
                                           ? capsaicin.createWindowTexture(
                                               DXGI_FORMAT_R16G16B16A16_FLOAT, "Lens_ChromaticAberration")
                                           : capsaicin.createRenderTexture(
                                               DXGI_FORMAT_R16G16B16A16_FLOAT, "Lens_ChromaticAberration");
        }
    }
    else
    {
        gfxDestroyTexture(gfx_, chromaticAberrationTexture);
        chromaticAberrationTexture = {};
    }
    if (options.lens_vignette_enable)
    {
        defines.push_back("ENABLE_VIGNETTE");
    }
    if (options.lens_film_grain_enable)
    {
        defines.push_back("ENABLE_FILMGRAIN");
    }

    lensMapKernel = gfxCreateComputeKernel(
        gfx_, lensProgram, "main", defines.data(), static_cast<uint32_t>(defines.size()));

    return !!lensMapKernel;
}
} // namespace Capsaicin
