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

#include "tone_mapping.h"

#include "../../components/blue_noise_sampler/blue_noise_sampler.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
ToneMapping::ToneMapping()
    : RenderTechnique("Tone mapping")
{}

ToneMapping::~ToneMapping()
{
    ToneMapping::terminate();
}

RenderOptionList ToneMapping::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(tonemap_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(tonemap_operator, options));
    return newOptions;
}

ToneMapping::RenderOptions ToneMapping::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(tonemap_enable, newOptions, options)
    RENDER_OPTION_GET(tonemap_operator, newOptions, options)
    return newOptions;
}

ComponentList ToneMapping::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(BlueNoiseSampler));
    return components;
}

SharedBufferList ToneMapping::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    buffers.push_back({"Exposure", SharedBuffer::Access::Read});
    return buffers;
}

SharedTextureList ToneMapping::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::ReadWrite});
    textures.push_back({"Debug", SharedTexture::Access::ReadWrite});
    textures.push_back({"ColorScaled", SharedTexture::Access::ReadWrite, SharedTexture::Flags::Optional});
    return textures;
}

DebugViewList ToneMapping::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("ToneMappedOutput"); // Allow viewing output without overwriting input
    return views;
}

bool ToneMapping::init(CapsaicinInternal const &capsaicin) noexcept
{
    if (options.tonemap_enable)
    {
        // Create kernels
        toneMappingProgram = capsaicin.createProgram("render_techniques/tone_mapping/tone_mapping");

        return initToneMapKernel();
    }
    return true;
}

void ToneMapping::render(CapsaicinInternal &capsaicin) noexcept
{
    auto const newOptions = convertOptions(capsaicin.getOptions());

    if (!newOptions.tonemap_enable)
    {
        if (options.tonemap_enable)
        {
            // Destroy resources when not being used
            terminate();
            options.tonemap_enable = false;
        }
        return;
    }

    bool const recompile = options.tonemap_operator != newOptions.tonemap_operator;
    bool const reInit    = !options.tonemap_enable && newOptions.tonemap_enable;
    options              = newOptions;

    if (reInit)
    {
        if (!init(capsaicin))
        {
            return;
        }
    }
    else if (auto const newColourSpace = gfxGetBackBufferColorSpace(gfx_);
             recompile || newColourSpace != colourSpace)
    {
        if (!initToneMapKernel())
        {
            return;
        }
    }

    bool const usesScaling = capsaicin.hasSharedTexture("ColorScaled")
                          && capsaicin.hasOption<bool>("taa_enable")
                          && capsaicin.getOption<bool>("taa_enable");
    GfxTexture input =
        !usesScaling ? capsaicin.getSharedTexture("Color") : capsaicin.getSharedTexture("ColorScaled");
    GfxTexture output = input;

    if (auto const debugView = capsaicin.getCurrentDebugView(); !debugView.empty() && debugView != "None")
    {
        if (debugView == "ToneMappedOutput")
        {
            // Output tone-mapping to debug view instead of output. This is only possible when the input
            // buffer has the same dimensions as the "Debug" AOV
            if (!usesScaling)
            {
                output = capsaicin.getSharedTexture("Debug");
            }
            else
            {
                capsaicin.setDebugView("None");
            }
        }
        else
        {
            // Tone map the debug buffer if we are using a debug view
            if (capsaicin.checkDebugViewSharedTexture(debugView))
            {
                // If the debug view is actually an AOV then only tonemap if it's a floating point format
                auto const debugAOV = capsaicin.getSharedTexture(debugView);
                if (auto const format = debugAOV.getFormat();
                    format == DXGI_FORMAT_R32G32B32A32_FLOAT || format == DXGI_FORMAT_R32G32B32_FLOAT
                    || format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_R11G11B10_FLOAT)
                {
                    input  = debugAOV;
                    output = capsaicin.getSharedTexture("Debug");
                }
            }
            else
            {
                input  = capsaicin.getSharedTexture("Debug");
                output = input;
            }
        }
    }

    // Call the tone mapping kernel on each pixel of colour buffer
    if (usingDither)
    {
        auto const blue_noise_sampler = capsaicin.getComponent<BlueNoiseSampler>();
        blue_noise_sampler->addProgramParameters(capsaicin, toneMappingProgram);
    }
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_FrameIndex", capsaicin.getFrameIndex());
    auto const bufferDimensions =
        !usesScaling ? capsaicin.getRenderDimensions() : capsaicin.getWindowDimensions();
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_BufferDimensions", bufferDimensions);
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_InputBuffer", input);
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_OutputBuffer", output);
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_Exposure", capsaicin.getSharedBuffer("Exposure"));
    {
        TimedSection const timed_section(*this, "ToneMap");
        uint32_t const    *numThreads = gfxKernelGetNumThreads(gfx_, toneMapKernel);
        uint32_t const     numGroupsX = (bufferDimensions.x + numThreads[0] - 1) / numThreads[0];
        uint32_t const     numGroupsY = (bufferDimensions.y + numThreads[1] - 1) / numThreads[1];
        gfxCommandBindKernel(gfx_, toneMapKernel);
        gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
    }
}

void ToneMapping::terminate() noexcept
{
    gfxDestroyKernel(gfx_, toneMapKernel);
    gfxDestroyProgram(gfx_, toneMappingProgram);

    toneMapKernel      = {};
    toneMappingProgram = {};
}

void ToneMapping::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    bool &enabled = capsaicin.getOption<bool>("tonemap_enable");
    ImGui::Checkbox("Enable Tone Mapping", &enabled);
    if (enabled)
    {
        constexpr auto operatorList =
            "None\0Reinhard Simple\0Reinhard Luminance\0ACES Approximate\0ACES Fitted\0ACES\0PBR Neutral\0Uncharted 2\0Agx Fitted\0Agx\0";
        auto const currentOperator  = capsaicin.getOption<uint32_t>("tonemap_operator");
        auto       selectedOperator = static_cast<int32_t>(currentOperator);
        if (ImGui::Combo("Tone Mapper", &selectedOperator, operatorList, 5))
        {
            if (currentOperator != static_cast<uint32_t>(selectedOperator))
            {
                capsaicin.setOption("tonemap_operator", static_cast<uint32_t>(selectedOperator));
            }
        }
    }
}

bool ToneMapping::initToneMapKernel() noexcept
{
    gfxDestroyKernel(gfx_, toneMapKernel);

    // Get current display color space and depth
    colourSpace = gfxGetBackBufferColorSpace(gfx_);

    std::vector<char const *> defines;
    usingDither              = false;
    auto const displayFormat = gfxGetBackBufferFormat(gfx_);
    if (displayFormat == DXGI_FORMAT_R10G10B10A2_UNORM)
    {
        defines.push_back("DITHER_10");
        usingDither = true;
    }
    else
    {
        // 8bit SDR format
        defines.push_back("DITHER_8");
        usingDither = true;
    }

    if (options.tonemap_operator == 1)
    {
        defines.push_back("TONEMAP_REINHARD");
    }
    else if (options.tonemap_operator == 2)
    {
        defines.push_back("TONEMAP_REINHARDL");
    }
    else if (options.tonemap_operator == 3)
    {
        defines.push_back("TONEMAP_ACESFAST");
    }
    else if (options.tonemap_operator == 4)
    {
        defines.push_back("TONEMAP_ACESFITTED");
    }
    else if (options.tonemap_operator == 5)
    {
        defines.push_back("TONEMAP_ACES");
    }
    else if (options.tonemap_operator == 6)
    {
        defines.push_back("TONEMAP_PBRNEUTRAL");
    }
    else if (options.tonemap_operator == 7)
    {
        defines.push_back("TONEMAP_UNCHARTED2");
    }
    else if (options.tonemap_operator == 8)
    {
        defines.push_back("TONEMAP_AGXFITTED");
    }
    else if (options.tonemap_operator == 9)
    {
        defines.push_back("TONEMAP_AGX");
    }
    toneMapKernel = gfxCreateComputeKernel(
        gfx_, toneMappingProgram, "Tonemap", defines.data(), static_cast<uint32_t>(defines.size()));

    return !!toneMapKernel;
}
} // namespace Capsaicin
