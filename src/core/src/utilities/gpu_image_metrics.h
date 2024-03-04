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

#include "gpu_reduce.h"
#include "gpu_shared.h"

namespace Capsaicin
{
class CapsaicinInternal;

class GPUImageMetrics
{
public:
    GPUImageMetrics() noexcept = default;

    ~GPUImageMetrics() noexcept;

    /** Type of image values. */
    enum class Type : uint32_t
    {
        HDR = 0,       /**< HDR linear float grayscale/luminance values */
        HDR_RGB,       /**< HDR linear float RGB values */
        SDR,           /**< SDR linear float values */
        SDR_RGB,       /**< SDR linear float RGB values */
        SDR_NONLINEAR, /**< SDR gamma corrected float values */
        SDR_SRGB,      /**< SDR gamma corrected sRGB float values */
    };

    /** Type of comparison operation to perform. */
    enum class Operation
    {
        MSE,   /**< Mean Squared Error */
        RMSE,  /**< Root Mean Squared Error */
        PSNR,  /**< Peak Signal to noise ratio */
        RMAE,  /**< Relative Mean Absolute Error */
        SMAPE, /**< Symmetric Mean Absolute Percentage Error */
        SSIM, /**< Structural Similarity */
    };

    /**
     * Initialise the internal data based on current configuration.
     * @param gfx        Active gfx context.
     * @param shaderPath Path to shader files based on current working directory.
     * @param type       The object type to reduce.
     * @param operation  The type of operation to perform.
     * @return True, if any initialisation/changes succeeded.
     */
    bool initialise(
        GfxContext gfx, std::string_view const &shaderPath, Type type, Operation operation) noexcept;

    /**
     * Initialise the internal data based on current configuration.
     * @param capsaicin Current framework context.
     * @param type      The type of data in the images.
     * @param operation The type of operation to perform.
     * @return True, if any initialisation/changes succeeded.
     */
    bool initialise(CapsaicinInternal const &capsaicin, Type type, Operation operation) noexcept;

    /**
     * Generate comparison metrics for 2 different images.
     * @note This will flush the current GPU pipeline so only call this function is no other work is to be
     * performed. Otherwise use 'compareAsync'.
     * @param sourceImage    The input image to compare.
     * @param referenceImage The reference image to compare to.
     * @returns True, if operation succeeded.
     */
    bool compare(GfxTexture const &sourceImage, GfxTexture const &referenceImage) noexcept;

    /**
     * Asynchronously generate comparison metrics for 2 different images.
     * Used if calculating metrics for multiple frames as it allows many frames in flight at a time.
     * @param sourceImage    The input image to compare.
     * @param referenceImage The reference image to compare to.
     * @returns True, if operation succeeded.
     */
    bool compareAsync(GfxTexture const &sourceImage, GfxTexture const &referenceImage) noexcept;

    /**
     * Read back the value of the most recent calculated metric.
     * @note When using 'compareAsync' there will be a delay before the final value is available. This delay
     * can be retrieved using 'getAsyncDelay'.
     * @returns The calculate metric value, Zero if no value is available.
     */
    float getMetricValue() const noexcept;

    /**
     * Get the number of frames of delay there is when using 'compareAsync'.
     * @returns The number of frames worth of delay.
     */
    uint32_t getAsyncDelay() const noexcept;

private:
    /** Terminates and cleans up this object. */
    void terminate() noexcept;

    bool compareInternal(GfxTexture const &sourceImage, GfxTexture const &referenceImage) noexcept;

    float convertMetric(float value, uint32_t totalSamples) const noexcept;

    GfxContext gfx;

    Type      currentType      = Type::HDR_RGB;
    Operation currentOperation = Operation::RMSE;

    GfxBuffer metricBuffer; /**< Buffer used to hold calculated metric */
    std::vector<std::pair<float, GfxBuffer>>
          metricBufferTemp;    /**< Buffer used to copy back calculated metric into CPU memory */
    float currentValue = 1.0f; /**< Most recent calculated metric value */

    GfxProgram metricsProgram;
    GfxKernel  metricsKernel;

    GPUReduce reducer;
};
} // namespace Capsaicin
