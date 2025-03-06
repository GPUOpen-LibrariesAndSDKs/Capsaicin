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

#include "bloom.h"

#include "capsaicin_internal.h"
#include "components/blue_noise_sampler/blue_noise_sampler.h"
#define FFX_CPU
#include <FidelityFX/gpu/blur/ffx_blur.h>

namespace Capsaicin
{
Bloom::Bloom()
    : RenderTechnique("Bloom")
{}

Bloom::~Bloom()
{
    Bloom::terminate();
}

RenderOptionList Bloom::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(bloom_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(bloom_radius, options));
    newOptions.emplace(RENDER_OPTION_MAKE(bloom_clip_bias, options));
    return newOptions;
}

Bloom::RenderOptions Bloom::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(bloom_enable, newOptions, options)
    RENDER_OPTION_GET(bloom_radius, newOptions, options)
    RENDER_OPTION_GET(bloom_clip_bias, newOptions, options)
    return newOptions;
}

SharedBufferList Bloom::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    buffers.push_back({"Exposure", SharedBuffer::Access::Read});
    return buffers;
}

SharedTextureList Bloom::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::ReadWrite});
    textures.push_back({"Debug", SharedTexture::Access::Write});
    textures.push_back({"ColorScaled", SharedTexture::Access::ReadWrite, SharedTexture::Flags::Optional});
    return textures;
}

bool Bloom::init(CapsaicinInternal const &capsaicin) noexcept
{
    if (options.bloom_enable)
    {
        // Create scratch texture use for bloom output
        bool const usesScaling = capsaicin.hasSharedTexture("ColorScaled")
                              && capsaicin.hasOption<bool>("taa_enable")
                              && capsaicin.getOption<bool>("taa_enable");

        // Create blur texture, starting at half resolution with mip levels for each pass
        bloomTexture =
            usesScaling
                ? capsaicin.createWindowTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, "Bloom_Blur", UINT_MAX, 0.5F)
                : capsaicin.createRenderTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, "Bloom_Blur", UINT_MAX, 0.5F);

        // Initialise desired blur settings
        auto const bufferDimensions =
            !usesScaling ? capsaicin.getRenderDimensions() : capsaicin.getWindowDimensions();
        calculateBlurParameters(bufferDimensions);

        // Create kernels
        blurProgram    = capsaicin.createProgram("render_techniques/bloom/blur");
        combineProgram = capsaicin.createProgram("render_techniques/bloom/combine");
        combineKernel  = gfxCreateComputeKernel(gfx_, combineProgram, "main");

        return initBlurKernel() && !!combineKernel && !!bloomTexture;
    }
    return true;
}

void Bloom::render(CapsaicinInternal &capsaicin) noexcept
{
    auto newOptions = convertOptions(capsaicin.getOptions());

    if (!newOptions.bloom_enable)
    {
        if (options.bloom_enable)
        {
            // Destroy resources when not being used
            terminate();
            options.bloom_enable = false;
        }
        return;
    }

    // Ensure bloom radius is in valid range
    newOptions.bloom_radius    = glm::clamp(newOptions.bloom_radius, 1U, 20U);
    newOptions.bloom_clip_bias = glm::max(newOptions.bloom_clip_bias, 0.001F);

    bool       reCalculate = options.bloom_radius != newOptions.bloom_radius;
    bool const reInit      = !options.bloom_enable && newOptions.bloom_enable;

    options = newOptions;

    bool const usesScaling = capsaicin.hasSharedTexture("ColorScaled")
                          && capsaicin.hasOption<bool>("taa_enable")
                          && capsaicin.getOption<bool>("taa_enable");
    auto const bufferDimensions =
        !usesScaling ? capsaicin.getRenderDimensions() : capsaicin.getWindowDimensions();

    if (reInit)
    {
        // Only initialise data if actually being used
        if (!init(capsaicin))
        {
            return;
        }
    }
    else
    {
        if (usesScaling && capsaicin.getWindowDimensionsUpdated())
        {
            bloomTexture = capsaicin.resizeWindowTexture(bloomTexture, false, UINT_MAX, 0.5F);
            reCalculate  = true;
        }
        else if (!usesScaling && capsaicin.getRenderDimensionsUpdated())
        {
            bloomTexture = capsaicin.resizeRenderTexture(bloomTexture, false, UINT_MAX, 0.5F);
            reCalculate  = true;
        }
        if (reCalculate && calculateBlurParameters(bufferDimensions))
        {
            if (!initBlurKernel())
            {
                return;
            }
        }
    }

    auto const &input =
        !usesScaling ? capsaicin.getSharedTexture("Color") : capsaicin.getSharedTexture("ColorScaled");

    // Generate Bloom texture
    {
        TimedSection const timed_section(*this, "Bloom Blur");
        auto               blurDimensions = bufferDimensions / uint2(2);
        gfxProgramSetParameter(gfx_, blurProgram, "g_BufferDimensions", blurDimensions);
        gfxProgramSetParameter(gfx_, blurProgram, "g_InvBufferDimensions",
            float2(1.0F, 1.0F) / static_cast<float2>(blurDimensions));
        gfxProgramSetTexture(gfx_, blurProgram, "g_InputBuffer", input);
        gfxProgramSetTexture(gfx_, blurProgram, "g_OutputBuffer", bloomTexture, 0);
        gfxProgramSetParameter(gfx_, blurProgram, "g_LinearClampSampler", capsaicin.getLinearSampler());
        gfxProgramSetParameter(gfx_, blurProgram, "g_Exposure", capsaicin.getSharedBuffer("Exposure"));
        float const bloomClip = 1.0F * options.bloom_clip_bias;
        gfxProgramSetParameter(gfx_, blurProgram, "g_BloomClip", bloomClip);
        // Run first pass
        uint32_t           numGroupsX = (blurDimensions.x + FFX_BLUR_TILE_SIZE_X - 1) / FFX_BLUR_TILE_SIZE_X;
        constexpr uint32_t numGroupsY = FFX_BLUR_DISPATCH_Y;
        gfxCommandBindKernel(gfx_, blurKernel);
        gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
        // Run any additional passes to downscale lower mips
        gfxCommandBindKernel(gfx_, blur2Kernel);
        for (uint32_t pass = 1; pass < blurPasses; ++pass)
        {
            blurDimensions /= uint2(2);
            numGroupsX = (blurDimensions.x + FFX_BLUR_TILE_SIZE_X - 1) / FFX_BLUR_TILE_SIZE_X;
            gfxProgramSetParameter(gfx_, blurProgram, "g_BufferDimensions", blurDimensions);
            gfxProgramSetParameter(gfx_, blurProgram, "g_InvBufferDimensions",
                float2(1.0F, 1.0F) / static_cast<float2>(blurDimensions));
            gfxProgramSetTexture(gfx_, blurProgram, "g_InputBuffer", bloomTexture, pass - 1);
            gfxProgramSetTexture(gfx_, blurProgram, "g_OutputBuffer", bloomTexture, pass);
            gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
        }
        // Now run upscale pass to cleanup image
        for (uint32_t pass = blurPasses - 1; pass >= 1; --pass)
        {
            blurDimensions = bufferDimensions / uint2(2);
            for (uint32_t pass2 = 1; pass2 < pass; ++pass2)
            {
                blurDimensions /= uint2(2);
            }
            numGroupsX = (blurDimensions.x + FFX_BLUR_TILE_SIZE_X - 1) / FFX_BLUR_TILE_SIZE_X;
            gfxProgramSetParameter(gfx_, blurProgram, "g_BufferDimensions", blurDimensions);
            gfxProgramSetParameter(gfx_, blurProgram, "g_InvBufferDimensions",
                float2(1.0F, 1.0F) / static_cast<float2>(blurDimensions));
            gfxProgramSetTexture(gfx_, blurProgram, "g_InputBuffer", bloomTexture, pass - 1);
            gfxProgramSetTexture(gfx_, blurProgram, "g_InputBuffer", bloomTexture, pass);
            gfxProgramSetTexture(gfx_, blurProgram, "g_OutputBuffer", bloomTexture, pass - 1);
            gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
        }
    }

    // Combine bloom with input texture
    {
        TimedSection const timed_section(*this, "Bloom Combine");
        gfxProgramSetParameter(gfx_, combineProgram, "g_BufferDimensions", bufferDimensions);
        gfxProgramSetParameter(gfx_, combineProgram, "g_InvBufferDimensions",
            float2(1.0F, 1.0F) / static_cast<float2>(bufferDimensions));
        gfxProgramSetTexture(gfx_, combineProgram, "g_InputBuffer", input);
        gfxProgramSetTexture(gfx_, combineProgram, "g_InputBloomBuffer", bloomTexture);
        gfxProgramSetParameter(gfx_, combineProgram, "g_LinearClampSampler", capsaicin.getLinearSampler());
        gfxProgramSetParameter(gfx_, combineProgram, "g_OutputBuffer", input);
        uint32_t const *numThreads = gfxKernelGetNumThreads(gfx_, combineKernel);
        uint32_t const  numGroupsX = (bufferDimensions.x + numThreads[0] - 1) / numThreads[0];
        uint32_t const  numGroupsY = (bufferDimensions.y + numThreads[1] - 1) / numThreads[1];
        gfxCommandBindKernel(gfx_, combineKernel);
        gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
    }
}

void Bloom::terminate() noexcept
{
    gfxDestroyKernel(gfx_, blurKernel);
    blurKernel = {};
    gfxDestroyKernel(gfx_, blur2Kernel);
    blur2Kernel = {};
    gfxDestroyProgram(gfx_, blurProgram);
    blurProgram = {};
    gfxDestroyKernel(gfx_, combineKernel);
    combineKernel = {};
    gfxDestroyProgram(gfx_, combineProgram);
    combineProgram = {};
    gfxDestroyTexture(gfx_, bloomTexture);
    bloomTexture = {};
}

void Bloom::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    bool enabled = capsaicin.getOption<bool>("bloom_enable");
    if (ImGui::Checkbox("Enable Bloom", &enabled))
    {
        capsaicin.setOption("bloom_enable", enabled);
    }
    if (enabled)
    {
        int32_t bloomRadius = static_cast<int32_t>(capsaicin.getOption<uint32_t>("bloom_radius"));
        if (ImGui::DragInt("Bloom Radius", &bloomRadius, 1, 1, 10))
        {
            capsaicin.setOption("bloom_radius", static_cast<uint32_t>(bloomRadius));
        }
        /*float bloomClip = capsaicin.getOption<float>("bloom_clip_bias");
        if (ImGui::DragFloat("Bloom Clip Bias", &bloomClip, 5e-3F, 0.001F, 10.0F))
        {
            capsaicin.setOption("bloom_clip_bias", bloomClip);
        }*/
    }
}

bool Bloom::calculateBlurParameters(uint2 const &screenDimensions) noexcept
{
    // Base blur on vertical percentage
    auto const blurPixels = static_cast<int32_t>(
        (static_cast<float>(screenDimensions.y) * static_cast<float>(options.bloom_radius)) / 100.0F);
    // Find nearest ratio of blur radius and mip-map levels. We always start at 1 mip-level and then search
    // for best combination
    uint32_t   bestBlur  = 3;
    uint32_t   bestMip   = 1;
    int32_t    bestRatio = glm::abs(blurPixels - static_cast<int32_t>(bestBlur * bestBlur));
    uint32_t   count     = 3;
    uint const maxMips   = glm::min(5U, gfxCalculateMipCount(screenDimensions.x, screenDimensions.y)) - 1;
    for (uint32_t mip = 1; mip <= maxMips; ++mip)
    {
        for (uint32_t blur = 3; blur <= 10; ++blur)
        {
            float const   radius = (static_cast<float>(count) / 0.95F);
            int32_t const closeness =
                glm::abs(static_cast<int32_t>(blurPixels) - static_cast<int32_t>(radius));
            if (closeness < bestRatio)
            {
                bestRatio = closeness;
                bestBlur  = blur;
                bestMip   = mip;
            }
            count += mip;
        }
    }

    // Return whether a kernel compile is needed due to change in blur radius
    bool const ret = blurRadius != bestBlur;
    // Update internal values
    blurRadius = bestBlur;
    blurPasses = bestMip;
    return ret;
}

bool Bloom::initBlurKernel() noexcept
{
    gfxDestroyKernel(gfx_, blurKernel);
    std::string const         blurRadiusString = "BLUR_RADIUS=" + std::to_string(blurRadius);
    std::vector<char const *> defines;
    defines.push_back(blurRadiusString.c_str());
    blurKernel = gfxCreateComputeKernel(
        gfx_, blurProgram, "main", defines.data(), static_cast<uint32_t>(defines.size()));

    gfxDestroyKernel(gfx_, blur2Kernel);
    defines.push_back("PASSTHROUGH");
    blur2Kernel = gfxCreateComputeKernel(
        gfx_, blurProgram, "main", defines.data(), static_cast<uint32_t>(defines.size()));

    return !!blurKernel;
}
} // namespace Capsaicin
