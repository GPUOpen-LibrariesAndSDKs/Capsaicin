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

#include "auto_exposure.h"

#include "../../components/blue_noise_sampler/blue_noise_sampler.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
AutoExposure::AutoExposure()
    : RenderTechnique("Auto Exposure")
{}

AutoExposure::~AutoExposure()
{
    AutoExposure::terminate();
}

RenderOptionList AutoExposure::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(auto_exposure_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(auto_exposure_value, options));
    newOptions.emplace(RENDER_OPTION_MAKE(auto_exposure_bias, options));
    return newOptions;
}

AutoExposure::RenderOptions AutoExposure::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(auto_exposure_enable, newOptions, options)
    RENDER_OPTION_GET(auto_exposure_value, newOptions, options)
    RENDER_OPTION_GET(auto_exposure_bias, newOptions, options)
    return newOptions;
}

SharedBufferList AutoExposure::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    buffers.push_back(
        {"Exposure", SharedBuffer::Access::Write, SharedBuffer::Flags::None, sizeof(float), sizeof(float)});
    return buffers;
}

SharedTextureList AutoExposure::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::Read});
    textures.push_back({"Debug", SharedTexture::Access::Read});
    return textures;
}

bool AutoExposure::init(CapsaicinInternal const &capsaicin) noexcept
{
    // Update exposure buffer with initial exposure value
    auto const     &exposureBuffer   = capsaicin.getSharedBuffer("Exposure");
    float const     combinedExposure = options.auto_exposure_value * options.auto_exposure_bias;
    GfxBuffer const uploadBuffer = gfxCreateBuffer<float>(gfx_, 1, &combinedExposure, kGfxCpuAccess_Write);
    gfxCommandCopyBuffer(gfx_, exposureBuffer, uploadBuffer);
    gfxDestroyBuffer(gfx_, uploadBuffer);

    if (options.auto_exposure_enable)
    {
        return initAutoExposure(capsaicin);
    }

    return true;
}

void AutoExposure::render(CapsaicinInternal &capsaicin) noexcept
{
    auto newOptions = convertOptions(capsaicin.getOptions());

    // Ensure exposure and bias are always valid
    newOptions.auto_exposure_value = glm::max(newOptions.auto_exposure_value, 0.0001F);
    newOptions.auto_exposure_bias  = glm::max(newOptions.auto_exposure_bias, 0.0001F);
    bool const exposureChange      = options.auto_exposure_value != newOptions.auto_exposure_value
                             || options.auto_exposure_bias != newOptions.auto_exposure_bias;
    bool const reUpExposure = ((!options.auto_exposure_enable && exposureChange)
                               || (options.auto_exposure_enable
                                   && (newOptions.auto_exposure_enable != options.auto_exposure_enable)));

    if (!options.auto_exposure_enable && newOptions.auto_exposure_enable)
    {
        // Only load resources when auto exposure is being used
        if (!initAutoExposure(capsaicin))
        {
            terminate();
            return;
        }
    }
    else if (options.auto_exposure_enable && !newOptions.auto_exposure_enable)
    {
        // Free resources when auto exposure is disabled
        terminate();
    }
    options = newOptions;

    if (options.auto_exposure_enable)
    {
        GfxTexture input = capsaicin.getSharedTexture("Color");
        if (auto const debugView = capsaicin.getCurrentDebugView(); !debugView.empty() && debugView != "None")
        {
            // Operate on the debug buffer if we are using a debug view
            if (capsaicin.checkDebugViewSharedTexture(debugView))
            {
                // If the debug view is actually an AOV then only use if it's a floating point format
                auto const &debugAOV = capsaicin.getSharedTexture(debugView);
                if (auto const format = debugAOV.getFormat();
                    format == DXGI_FORMAT_R32G32B32A32_FLOAT || format == DXGI_FORMAT_R32G32B32_FLOAT
                    || format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_R11G11B10_FLOAT)
                {
                    input = debugAOV;
                }
            }
            else
            {
                input = capsaicin.getSharedTexture("Debug");
            }
        }

        auto const  bufferDimensions = capsaicin.getRenderDimensions();
        auto const &exposureBuffer   = capsaicin.getSharedBuffer("Exposure");
        gfxProgramSetParameter(gfx_, exposureProgram, "g_BufferDimensions", bufferDimensions);
        gfxProgramSetParameter(gfx_, exposureProgram, "g_InputBuffer", input);
        gfxProgramSetParameter(gfx_, exposureProgram, "g_Exposure", exposureBuffer);

        // Calculate scene key luminance
        gfxProgramSetParameter(gfx_, exposureProgram, "g_Histogram", histogramBuffer);
        {
            TimedSection const timed_section(*this, "BuildHistogram");
            uint32_t const    *numThreads = gfxKernelGetNumThreads(gfx_, histogramKernel);
            uint32_t const     numGroupsX = (bufferDimensions.x + numThreads[0] - 1) / numThreads[0];
            uint32_t const     numGroupsY = (bufferDimensions.y + numThreads[1] - 1) / numThreads[1];
            gfxCommandBindKernel(gfx_, histogramKernel);
            gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
        }

        // Set frame time to zero to reset adaptation on change
        bool const noAdaptation = capsaicin.getCameraChanged() || capsaicin.getSceneUpdated()
                               || capsaicin.getEnvironmentMapUpdated();
        float const frameTime = !noAdaptation ? static_cast<float>(capsaicin.getFrameTime()) : 0.0F;

        // Calculate auto exposure
        gfxProgramSetParameter(gfx_, exposureProgram, "g_KeySceneLuminance", keySceneLuminanceBuffer);
        gfxProgramSetParameter(gfx_, exposureProgram, "g_FrameTime", frameTime);
        gfxProgramSetParameter(gfx_, exposureProgram, "g_ExposureBias", options.auto_exposure_bias);
        {
            TimedSection const timed_section(*this, "AutoExposure");
            gfxCommandBindKernel(gfx_, exposureKernel);
            gfxCommandDispatch(gfx_, 1, 1, 1);
        }

        // Copy back calculated exposure to CPU
        {
            TimedSection const timed_section(*this, "AutoExposureCopyBack");
            uint32_t const     bufferIndex   = gfxGetBackBufferIndex(gfx_);
            float              exposureValue = options.auto_exposure_value;
            if (exposureBufferTemp[bufferIndex].first != 0.0F)
            {
                exposureValue = *gfxBufferGetData<float>(gfx_, exposureBufferTemp[bufferIndex].second.first);
                // Remove the exposure bias that was in use when the exposure was calculated
                exposureValue /= exposureBufferTemp[bufferIndex].second.second;
            }

            // Begin copy of new value (will take 'bufferIndex' number of frames to become valid)
            gfxCommandCopyBuffer(gfx_, exposureBufferTemp[bufferIndex].second.first, exposureBuffer);
            exposureBufferTemp[bufferIndex].second.second = options.auto_exposure_bias;
            exposureBufferTemp[bufferIndex].first         = exposureValue;
        }
    }
    else if (reUpExposure)
    {
        TimedSection const timed_section(*this, "ExposureUpload");
        float              combinedExposure = options.auto_exposure_value;
        combinedExposure                    = glm::max(combinedExposure, 0.0001F);
        GfxBuffer const uploadBuffer =
            gfxCreateBuffer<float>(gfx_, 1, &combinedExposure, kGfxCpuAccess_Write);
        gfxCommandCopyBuffer(gfx_, capsaicin.getSharedBuffer("Exposure"), uploadBuffer);
        gfxDestroyBuffer(gfx_, uploadBuffer);
    }
}

void AutoExposure::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, histogramBuffer);
    histogramBuffer = {};
    gfxDestroyBuffer(gfx_, keySceneLuminanceBuffer);
    keySceneLuminanceBuffer = {};
    for (auto const &i : exposureBufferTemp)
    {
        gfxDestroyBuffer(gfx_, i.second.first);
    }
    exposureBufferTemp.clear();

    gfxDestroyKernel(gfx_, histogramKernel);
    histogramKernel = {};
    gfxDestroyKernel(gfx_, exposureKernel);
    exposureKernel = {};
    gfxDestroyProgram(gfx_, exposureProgram);
    exposureProgram = {};
}

void AutoExposure::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    auto const enabled = capsaicin.getOption<bool>("auto_exposure_enable");
    if (enabled)
    {
        ImGui::BeginDisabled();
        uint32_t const bufferIndex   = gfxGetBackBufferIndex(gfx_);
        float          exposureValue = exposureBufferTemp[bufferIndex].first;
        ImGui::DragFloat("Exposure", &exposureValue, 5e-3F);
        ImGui::EndDisabled();
    }
    else
    {
        ImGui::DragFloat("Exposure", &capsaicin.getOption<float>("auto_exposure_value"), 5e-3F);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto", &capsaicin.getOption<bool>("auto_exposure_enable")))
    {
        if (!capsaicin.getOption<bool>("auto_exposure_enable")
            && capsaicin.getOption<float>("auto_exposure_value") == 0.0F)
        {
            // If no manually set exposure has been applied then default to the last auto exposure value
            uint32_t const bufferIndex = gfxGetBackBufferIndex(gfx_);
            capsaicin.setOption<float>("auto_exposure_value", exposureBufferTemp[bufferIndex].first);
        }
    }
    if (enabled)
    {
        ImGui::DragFloat(
            "Exposure Bias", &capsaicin.getOption<float>("auto_exposure_bias"), 5e-3F, 0.001F, 100.0F);
    }
}

bool AutoExposure::initAutoExposure(CapsaicinInternal const &capsaicin) noexcept
{
    // Create buffer used to store scene luminance histogram
    histogramBuffer = gfxCreateBuffer<float>(gfx_, 128 /*required @ 1080p*/);
    histogramBuffer.setName("AutoExposure_Histogram");
    gfxCommandClearBuffer(gfx_, histogramBuffer, glm::floatBitsToUint(0.0F));

    // Create buffer used to hold key scene luminance
    constexpr float clearValue = 0.0F;
    keySceneLuminanceBuffer    = gfxCreateBuffer<float>(gfx_, 1, &clearValue);
    keySceneLuminanceBuffer.setName("AutoExposure_KeySceneLuminance");

    // Create buffer list used to read-back calculated exposure values
    uint32_t const backBufferCount = gfxGetBackBufferCount(gfx_);
    exposureBufferTemp.reserve(backBufferCount);
    for (uint32_t i = 0; i < backBufferCount; ++i)
    {
        GfxBuffer   buffer = gfxCreateBuffer<float>(gfx_, 1, nullptr, kGfxCpuAccess_Read);
        std::string name   = "AutoExposure_ExposureReadBack";
        name += std::to_string(i);
        buffer.setName(name.c_str());
        exposureBufferTemp.emplace_back(0.0F, std::make_pair(buffer, options.auto_exposure_bias));
    }

    // Create kernels
    exposureProgram = capsaicin.createProgram("render_techniques/auto_exposure/auto_exposure");
    histogramKernel = gfxCreateComputeKernel(gfx_, exposureProgram, "CalculateHistogram");
    exposureKernel  = gfxCreateComputeKernel(gfx_, exposureProgram, "CalculateExposure");

    return !!exposureKernel && !!keySceneLuminanceBuffer;
}

} // namespace Capsaicin
