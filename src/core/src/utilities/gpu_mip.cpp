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

#include "gpu_mip.h"

#include "capsaicin_internal.h"

#define FFX_CPU
#include <FidelityFX/gpu/ffx_core.h>
#include <bit>
// Order of includes is important
#include <FidelityFX/gpu/spd/ffx_spd.h>

namespace Capsaicin
{
GPUMip::~GPUMip() noexcept
{
    terminate();
}

bool GPUMip::initialise(
    GfxContext const &gfxIn, std::vector<std::string> const &shaderPaths, Type const type) noexcept
{
    gfx = gfxIn;

    if (type != currentType)
    {
        // If configuration has changed then need to recompile kernels
        gfxDestroyProgram(gfx, mipProgramPower2);
        mipProgramPower2 = {};
        gfxDestroyKernel(gfx, mipKernelPower2);
        mipKernelPower2 = {};
        gfxDestroyProgram(gfx, mipProgramNonPower2);
        mipProgramNonPower2 = {};
        gfxDestroyKernel(gfx, mipKernelNonPower2);
        mipKernelNonPower2 = {};
    }
    currentType = type;
    if (!mipProgramPower2)
    {
        gfxDestroyProgram(gfx, mipProgramPower2);
        gfxDestroyKernel(gfx, mipKernelPower2);
        gfxDestroyProgram(gfx, mipProgramNonPower2);
        gfxDestroyKernel(gfx, mipKernelNonPower2);

        std::vector<char const *> includePaths;
        includePaths.reserve(shaderPaths.size());
        for (auto const &path : shaderPaths)
        {
            includePaths.push_back(path.c_str());
        }
        mipProgramPower2    = gfxCreateProgram(gfx, "utilities/gpu_mip2", includePaths[0], nullptr,
               includePaths.data(), static_cast<uint32_t>(includePaths.size()));
        mipProgramNonPower2 = gfxCreateProgram(gfx, "utilities/gpu_mip", includePaths[0], nullptr,
            includePaths.data(), static_cast<uint32_t>(includePaths.size()));
        std::vector<char const *> baseDefines;
        switch (currentType)
        {
        case Type::R:
            baseDefines.push_back("INPUT_LINEAR");
            baseDefines.push_back("TYPE_VEC1");
            break;
        case Type::RG:
            baseDefines.push_back("INPUT_LINEAR");
            baseDefines.push_back("TYPE_VEC2");
            break;
        case Type::RGB: baseDefines.push_back("INPUT_LINEAR"); [[fallthrough]];
        case Type::sRGB: baseDefines.push_back("TYPE_VEC3"); break;
        case Type::RGBA: baseDefines.push_back("INPUT_LINEAR"); [[fallthrough]];
        case Type::sRGBA: baseDefines.push_back("TYPE_VEC4"); break;
        case Type::DepthMin: baseDefines.push_back("DEPTH_MIN"); [[fallthrough]];
        case Type::DepthMax:
            baseDefines.push_back("INPUT_LINEAR");
            baseDefines.push_back("TYPE_DEPTH");
            break;
        default: break;
        }
        mipKernelPower2    = gfxCreateComputeKernel(gfx, mipProgramPower2, "GenerateMips", baseDefines.data(),
               static_cast<uint32_t>(baseDefines.size()));
        mipKernelNonPower2 = gfxCreateComputeKernel(gfx, mipProgramNonPower2, "GenerateMip",
            baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
    }

    return !!mipKernelNonPower2 && !!mipKernelPower2;
}

bool GPUMip::initialise(CapsaicinInternal const &capsaicin, Type const type) noexcept
{
    return initialise(capsaicin.getGfx(), capsaicin.getShaderPaths(), type);
}

bool GPUMip::mip(GfxTexture const &inputTexture) noexcept
{
    return mipInternal(inputTexture, inputTexture);
}

bool GPUMip::mip(GfxTexture const &inputTexture, GfxTexture const &outputTexture) noexcept
{
    return mipInternal(inputTexture, outputTexture);
}

void GPUMip::terminate() noexcept
{
    gfxDestroyProgram(gfx, mipProgramPower2);
    mipProgramPower2 = {};
    gfxDestroyKernel(gfx, mipKernelPower2);
    mipKernelPower2 = {};
    gfxDestroyProgram(gfx, mipProgramNonPower2);
    mipProgramNonPower2 = {};
    gfxDestroyKernel(gfx, mipKernelNonPower2);
    mipKernelNonPower2 = {};
    gfxDestroyBuffer(gfx, mipTempBuffer);
    mipTempBuffer = {};
    gfxDestroyBuffer(gfx, mipSPDCountBuffer);
    mipSPDCountBuffer = {};
}

bool GPUMip::mipInternal(GfxTexture const &inputTexture, GfxTexture const &outputTexture) noexcept
{
    // Get number of mips to create
    auto     dimensions = uint2(inputTexture.getWidth(), inputTexture.getHeight());
    uint32_t mipLevels  = 32U
                       - static_cast<uint32_t>(std::countl_zero(
                           glm::max(dimensions.x, dimensions.y))); // Mips = floor(log2(size)) + 1

    // Clamp mip levels to those supported by texture
    bool const differentInOut = inputTexture != outputTexture;
    mipLevels                 = glm::min(mipLevels, outputTexture.getMipLevels() + (differentInOut ? 1 : 0));
    if (differentInOut)
    {
        if (outputTexture.getWidth() != (inputTexture.getWidth() >> 1)
            && outputTexture.getHeight() != (inputTexture.getHeight() >> 1))
        {
            GFX_PRINT_ERROR(kGfxResult_InvalidOperation,
                "Output texture doesn't have required resolution to match input texture");
        }
        return false;
    }

    // Calculate dispatch size
    uint32_t const *numThreads = gfxKernelGetNumThreads(gfx, mipKernelNonPower2);
    for (uint32_t i = 1; i < mipLevels; ++i)
    {
        auto const mip = i - (differentInOut ? 1 : 0);
        // Check if power of 2
        bool const power2 = ((dimensions.x & (dimensions.x - 1)) | (dimensions.y & (dimensions.y - 1))) == 0;
        if (power2 && all(greaterThan(dimensions, uint2(1))))
        {
            uint32_t workGroupOffset[2]            = {};
            uint32_t numWorkGroupsAndMips[2]       = {};
            uint32_t dispatchThreadGroupCountXY[2] = {};
            uint32_t rectInfo[]                    = {0, 0, dimensions.x, dimensions.y};
            ffxSpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo,
                static_cast<int32_t>(mipLevels - i));

            GfxTexture outputArray[12];
            uint32_t   outputMipArray[12] = {};

            for (uint32_t j = 0; j < numWorkGroupsAndMips[1]; ++j)
            {
                outputArray[j]    = outputTexture;
                outputMipArray[j] = i + j - (differentInOut ? 1 : 0);
            }

            struct SPDConstants
            {
                uint32_t mips;
                uint32_t numWorkGroups;
                uint2    workGroupOffset;
            };

            SPDConstants constantsSPD      = {};
            constantsSPD.mips              = numWorkGroupsAndMips[1];
            constantsSPD.numWorkGroups     = numWorkGroupsAndMips[0];
            constantsSPD.workGroupOffset.x = workGroupOffset[0];
            constantsSPD.workGroupOffset.y = workGroupOffset[1];

            if (!mipTempBuffer)
            {
                mipTempBuffer = gfxCreateBuffer<SPDConstants>(gfx, 1, nullptr, kGfxCpuAccess_Write);
                mipTempBuffer.setName("GPUMip_SPDConstants");
                constexpr uint32_t clearValue = 0;
                mipSPDCountBuffer             = gfxCreateBuffer<uint32_t>(gfx, 1, &clearValue);
                mipSPDCountBuffer.setName("Capsaicin_SPDCounterBuffer");
            }
            gfxBufferGetData<SPDConstants>(gfx, mipTempBuffer)[0] = constantsSPD;

            gfxProgramSetParameter(gfx, mipProgramPower2, "SPDConstants", mipTempBuffer);

            gfxProgramSetParameter(gfx, mipProgramPower2, "g_InputBuffer", inputTexture);
            gfxProgramSetParameter(gfx, mipProgramPower2, "g_OutputBuffers", outputArray, outputMipArray,
                numWorkGroupsAndMips[1]);

            gfxProgramSetParameter(gfx, mipProgramPower2, "g_SPDCounterBuffer", mipSPDCountBuffer);

            // Perform the single pass down-sampling into the hierarchical depth buffer
            gfxCommandBindKernel(gfx, mipKernelPower2);
            gfxCommandDispatch(gfx, dispatchThreadGroupCountXY[0], dispatchThreadGroupCountXY[1], 1);
            break;
        }
        else
        {
            gfxProgramSetParameter(gfx, mipProgramNonPower2, "g_InputDimensions", dimensions);
            auto outputDimensions = max(dimensions / uint2(2), uint2(1));
            gfxProgramSetParameter(gfx, mipProgramNonPower2, "g_OutputDimensions", outputDimensions);

            if (i == 1)
            {
                gfxProgramSetParameter(gfx, mipProgramNonPower2, "g_SourceImage", inputTexture);
            }
            else
            {
                gfxProgramSetParameter(gfx, mipProgramNonPower2, "g_SourceImage", outputTexture, mip - 1);
            }
            gfxProgramSetParameter(gfx, mipProgramNonPower2, "g_OutputImage", outputTexture, mip);

            uint32_t const numGroupsX = (outputDimensions.x + numThreads[0] - 1) / numThreads[0];
            uint32_t const numGroupsY = (outputDimensions.y + numThreads[1] - 1) / numThreads[1];

            gfxCommandBindKernel(gfx, mipKernelNonPower2);
            gfxCommandDispatch(gfx, numGroupsX, numGroupsY, 1);
            dimensions = outputDimensions;
        }
    }
    return true;
}
} // namespace Capsaicin
