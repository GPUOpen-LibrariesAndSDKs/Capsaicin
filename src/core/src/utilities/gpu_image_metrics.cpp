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

#include "gpu_image_metrics.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
GPUImageMetrics::~GPUImageMetrics() noexcept
{
    terminate();
}

bool GPUImageMetrics::initialise(
    GfxContext gfxIn, std::string_view const &shaderPath, Type type, Operation operation) noexcept
{
    gfx = gfxIn;

    if (type != currentType || operation != currentOperation)
    {
        // If configuration has changed then need to recompile kernels
        gfxDestroyProgram(gfx, metricsProgram);
        metricsProgram = {};
        gfxDestroyKernel(gfx, metricsKernel);
        metricsKernel = {};

        if (!metricBufferTemp.empty())
        {
            for (auto &i : metricBufferTemp)
            {
                i.first = 0.0f;
            }
        }
    }
    currentType      = type;
    currentOperation = operation;

    static const std::array<std::string_view, 6> typeName = {"MSE", "RMSE", "PSNR", "RMAE", "SMAPE", "SSIM"};

    if (metricBufferTemp.empty())
    {
        const uint32_t backBufferCount = getAsyncDelay();
        metricBufferTemp.reserve(backBufferCount);
        for (uint32_t i = 0; i < backBufferCount; ++i)
        {
            GfxBuffer   buffer = gfxCreateBuffer<float>(gfx, 1, nullptr, kGfxCpuAccess_Read);
            std::string name   = "GPUImageMetrics_";
            name += typeName[static_cast<uint32_t>(operation)].data();
            name += "Buffer";
            name += std::to_string(i);
            buffer.setName(name.c_str());
            metricBufferTemp.emplace_back(0.0f, buffer);
        }
    }
    else
    {
        // Invalidate current values
        for (auto &i : metricBufferTemp)
        {
            i.first = 0.0f;
        }
    }

    if (!metricBuffer)
    {
        metricBuffer     = gfxCreateBuffer<float>(gfx, 8160 /*required @ 1080p*/);
        std::string name = "GPUImageMetrics_Metrics";
        name += typeName[static_cast<uint32_t>(operation)].data();
        name += "Buffer";
        metricBuffer.setName(name.c_str());
    }

    if (!metricsProgram)
    {
        gfxDestroyProgram(gfx, metricsProgram);
        gfxDestroyKernel(gfx, metricsKernel);
        metricsProgram = gfxCreateProgram(gfx, "utilities/gpu_image_metrics", shaderPath.data());
        std::vector<char const *> baseDefines;
        if (currentType == Type::HDR_RGB || currentType == Type::SDR_RGB || currentType == Type::SDR_SRGB)
        {
            baseDefines.push_back("INPUT_MULTICHANNEL");
        }
        if (currentType == Type::HDR_RGB || currentType == Type::HDR)
        {
            baseDefines.push_back("INPUT_HDR");
        }
        if (currentType != Type::SDR_NONLINEAR && currentType != Type::SDR_SRGB)
        {
            baseDefines.push_back("INPUT_LINEAR");
        }
        if (currentOperation == Operation::PSNR)
        {
            baseDefines.push_back("CALCULATE_PSNR");
        }
        else if (currentOperation == Operation::MSE)
        {
            baseDefines.push_back("CALCULATE_MSE");
        }
        else if (currentOperation == Operation::RMSE)
        {
            baseDefines.push_back("CALCULATE_RMSE");
        }
        else if (currentOperation == Operation::RMAE)
        {
            baseDefines.push_back("CALCULATE_RMAE");
        }
        else if (currentOperation == Operation::SMAPE)
        {
            baseDefines.push_back("CALCULATE_SMAPE");
        }
        else if (currentOperation == Operation::SSIM)
        {
            baseDefines.push_back("CALCULATE_SSIM");
        }
        metricsKernel = gfxCreateComputeKernel(gfx, metricsProgram, "ComputeMetric", baseDefines.data(),
            static_cast<uint32_t>(baseDefines.size()));
    }
    if (!reducer.initialise(gfx, shaderPath, GPUReduce::Type::Float, GPUReduce::Operation::Sum))
    {
        return false;
    }
    return !!metricsKernel;
}

bool GPUImageMetrics::initialise(CapsaicinInternal const &capsaicin, Type type, Operation operation) noexcept
{
    return initialise(capsaicin.getGfx(), capsaicin.getShaderPath(), type, operation);
}

bool GPUImageMetrics::compare(GfxTexture const &sourceImage, GfxTexture const &referenceImage) noexcept
{
    if (!compareInternal(sourceImage, referenceImage))
    {
        return false;
    }

    gfxCommandCopyBuffer(gfx, metricBufferTemp[0].second, 0, metricBuffer, 0, sizeof(float));
    // Force the operation to complete and then read back to CPU
    gfxFinish(gfx);
    auto const newValue = *gfxBufferGetData<float>(gfx, metricBufferTemp[0].second);
    currentValue        = convertMetric(newValue, referenceImage.getWidth() * referenceImage.getHeight());
    return true;
}

bool GPUImageMetrics::compareAsync(GfxTexture const &sourceImage, GfxTexture const &referenceImage) noexcept
{
    if (!compareInternal(sourceImage, referenceImage))
    {
        return false;
    }

    // Stream the result back to the CPU
    const uint32_t bufferIndex = gfxGetBackBufferIndex(gfx);
    if (metricBufferTemp[bufferIndex].first != 0.0f)
    {
        auto const newValue = *gfxBufferGetData<float>(gfx, metricBufferTemp[bufferIndex].second);
        currentValue        = convertMetric(newValue, referenceImage.getWidth() * referenceImage.getHeight());
    }

    // Begin copy of new value (will take 'bufferIndex' number of frames to become valid)
    gfxCommandCopyBuffer(gfx, metricBufferTemp[bufferIndex].second, 0, metricBuffer, 0, sizeof(float));
    metricBufferTemp[bufferIndex].first = currentValue;
    return true;
}

float GPUImageMetrics::getMetricValue() const noexcept
{
    return currentValue;
}

uint32_t GPUImageMetrics::getAsyncDelay() const noexcept
{
    return gfxGetBackBufferCount(gfx);
}

void GPUImageMetrics::terminate() noexcept
{
    gfxDestroyBuffer(gfx, metricBuffer);
    metricBuffer = {};
    for (auto &i : metricBufferTemp)
    {
        gfxDestroyBuffer(gfx, i.second);
        i.second = {};
    }
    metricBufferTemp.clear();

    gfxDestroyProgram(gfx, metricsProgram);
    metricsProgram = {};
    gfxDestroyKernel(gfx, metricsKernel);
    metricsKernel = {};
}

bool GPUImageMetrics::compareInternal(
    GfxTexture const &sourceImage, GfxTexture const &referenceImage) noexcept
{
    if ((sourceImage.getWidth() != referenceImage.getWidth() && sourceImage.getWidth() != 0)
        || (sourceImage.getHeight() != referenceImage.getHeight() && sourceImage.getHeight() != 0))
    {
        return false;
    }

    uint32_t const  dimensions[]    = {referenceImage.getWidth(), referenceImage.getHeight()};
    uint32_t const *numThreads      = gfxKernelGetNumThreads(gfx, metricsKernel);
    uint32_t const  numGroupsX      = (dimensions[0] + numThreads[0] - 1) / numThreads[0];
    uint32_t const  numGroupsY      = (dimensions[1] + numThreads[1] - 1) / numThreads[1];
    uint32_t const  numOutputValues = numGroupsX * numGroupsY;

    if (metricBuffer.getCount() < numOutputValues)
    {
        std::string bufferName = metricBuffer.getName();
        gfxDestroyBuffer(gfx, metricBuffer);
        metricBuffer = gfxCreateBuffer<float>(gfx, numOutputValues);
        metricBuffer.setName(bufferName.c_str());
    }

    gfxProgramSetParameter(gfx, metricsProgram, "g_ImageDimensions", dimensions);

    gfxProgramSetParameter(gfx, metricsProgram, "g_SourceImage", sourceImage);
    gfxProgramSetParameter(gfx, metricsProgram, "g_ReferenceImage", referenceImage);

    gfxProgramSetParameter(gfx, metricsProgram, "g_MetricBuffer", metricBuffer);

    // Compute the metric
    {
        gfxCommandBindKernel(gfx, metricsKernel);
        gfxCommandDispatch(gfx, numGroupsX, numGroupsY, 1);
    }

    // Reduce to single value
    if (numOutputValues > 1)
    {
        if (!reducer.reduce(metricBuffer, numOutputValues))
        {
            return false;
        }
    }

    return true;
}

float GPUImageMetrics::convertMetric(float value, uint32_t totalSamples) const noexcept
{
    double const totalPixels = static_cast<double>(totalSamples);
    double       ret         = (double)value / totalPixels;
    switch (currentOperation)
    {
    case Operation::MSE:
        // MSE = [1/(width*height)]Sum([Ref.x.y - Src.x.y]^2)
        break;
    case Operation::RMSE:
        // RMSE = sqrt(MSE)
        ret = sqrt(ret);
        break;
    case Operation::PSNR:
        // PSNR = 20log10(MaxValue) - 10log10(MSE)
        if (currentType == Type::HDR || currentType == Type::HDR_RGB)
        {
            // MaxValue is set as 1.0f as we assume always using normalised float values
            ret = -10.0 * log10(ret);
        }
        else
        {
            // MaxValue is set as 255 for 8bit values
            ret = 48.13080361 - 10.0 * log10(ret);
        }
        break;
    case Operation::RMAE:
        // RMAE = [1/(width*height)]Sum(Abs(Src.x.y - Ref.x.y)/Ref.x.y)
        break;
    case Operation::SMAPE:
        // SMAPE = [100/(width*height)]Sum(Abs(Ref.x.y - Src.x.y)/([abs(Ref.x.y)+Abs(Src.x.y)]/2)
        ret *= 100.0;
        break;
    case Operation::SSIM:
        // SSIM =[1/(width*height)]Sum(SSIM(x,y))
        break;
    default: break;
    }
    return static_cast<float>(ret);
}

} // namespace Capsaicin
