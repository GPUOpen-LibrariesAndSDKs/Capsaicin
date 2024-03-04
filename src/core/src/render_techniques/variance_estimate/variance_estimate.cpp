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
#include "variance_estimate.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
VarianceEstimate::VarianceEstimate()
    : RenderTechnique("Variance Estimate")
    , cv_(0.0f)
    , readback_buffer_index_(0)
{}

VarianceEstimate::~VarianceEstimate()
{
    terminate();
}

AOVList VarianceEstimate::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"Color", AOV::Read});
    return aovs;
}

bool VarianceEstimate::init(CapsaicinInternal const &capsaicin) noexcept
{
    result_buffer_ = gfxCreateBuffer<float>(gfx_, 1);
    result_buffer_.setName("Capsaicin_ResultBuffer");

    for (uint32_t i = 0; i < ARRAYSIZE(readback_buffers_); ++i)
    {
        char buffer[64];
        GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ReadbackBuffer%u", i);

        readback_buffers_[i] = gfxCreateBuffer<float>(gfx_, 1, nullptr, kGfxCpuAccess_Read);
        readback_buffers_[i].setName(buffer);
    }

    variance_estimate_program_ = gfxCreateProgram(
        gfx_, "render_techniques/variance_estimate/variance_estimate", capsaicin.getShaderPath());
    compute_mean_kernel_      = gfxCreateComputeKernel(gfx_, variance_estimate_program_, "ComputeMean");
    compute_distance_kernel_  = gfxCreateComputeKernel(gfx_, variance_estimate_program_, "ComputeDistance");
    compute_deviation_kernel_ = gfxCreateComputeKernel(gfx_, variance_estimate_program_, "ComputeDeviation");

    return !!variance_estimate_program_;
}

void VarianceEstimate::render(CapsaicinInternal &capsaicin) noexcept
{
    uint32_t const buffer_dimensions[] = {capsaicin.getWidth(), capsaicin.getHeight()};

    uint32_t const  pixel_count  = buffer_dimensions[0] * buffer_dimensions[1];
    uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, compute_mean_kernel_);
    uint32_t const  num_groups_x = (buffer_dimensions[0] + num_threads[0] - 1) / num_threads[0];
    uint32_t const  num_groups_y = (buffer_dimensions[1] + num_threads[1] - 1) / num_threads[1];
    uint32_t const  elem_count =
        (pixel_count + num_threads[0] * num_threads[1] - 1) / (num_threads[0] * num_threads[1]);

    GFX_ASSERT(num_threads[0] == gfxKernelGetNumThreads(gfx_, compute_distance_kernel_)[0]
               && num_threads[1] == gfxKernelGetNumThreads(gfx_, compute_distance_kernel_)[1]
               && num_threads[2] == gfxKernelGetNumThreads(gfx_, compute_distance_kernel_)[2]);

    if (mean_buffer_.getCount() != elem_count)
    {
        gfxDestroyBuffer(gfx_, mean_buffer_);

        mean_buffer_ = gfxCreateBuffer<float>(gfx_, elem_count);
        mean_buffer_.setName("Capsaicin_MeanBuffer");
    }

    if (square_buffer_.getCount() != elem_count)
    {
        gfxDestroyBuffer(gfx_, square_buffer_);

        square_buffer_ = gfxCreateBuffer<float>(gfx_, elem_count);
        square_buffer_.setName("Capsaicin_SquareBuffer");
    }

    gfxProgramSetParameter(gfx_, variance_estimate_program_, "g_BufferDimensions", buffer_dimensions);

    gfxProgramSetParameter(
        gfx_, variance_estimate_program_, "g_ColorBuffer", capsaicin.getAOVBuffer("Color"));

    gfxProgramSetParameter(gfx_, variance_estimate_program_, "g_MeanBuffer", mean_buffer_);
    gfxProgramSetParameter(gfx_, variance_estimate_program_, "g_SquareBuffer", square_buffer_);
    gfxProgramSetParameter(gfx_, variance_estimate_program_, "g_ResultBuffer", result_buffer_);

    // Compute the mean
    {
        TimedSection const timed_section(*this, "ComputeMean");

        gfxCommandBindKernel(gfx_, compute_mean_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        gfxCommandReduceSum(gfx_, kGfxDataType_Float, mean_buffer_, mean_buffer_);
    }

    // Compute the squared distance to the mean
    {
        TimedSection const timed_section(*this, "ComputeDistance");

        gfxCommandBindKernel(gfx_, compute_distance_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        gfxCommandReduceSum(gfx_, kGfxDataType_Float, square_buffer_, square_buffer_);
    }

    // Compute the standard deviation
    {
        TimedSection const timed_section(*this, "ComputeDeviation");

        gfxCommandBindKernel(gfx_, compute_deviation_kernel_);
        gfxCommandDispatch(gfx_, 1, 1, 1);
    }

    // Stream the result back to the CPU
    {
        GfxBuffer const &readback_buffer =
            readback_buffers_[readback_buffer_index_ % ARRAYSIZE(readback_buffers_)];

        if (readback_buffer_index_ >= ARRAYSIZE(readback_buffers_))
        {
            cv_ = *gfxBufferGetData<float>(gfx_, readback_buffer);
        }

        gfxCommandCopyBuffer(gfx_, readback_buffer, result_buffer_);

        if (++readback_buffer_index_ >= 2 * ARRAYSIZE(readback_buffers_))
        {
            readback_buffer_index_ -= ARRAYSIZE(readback_buffers_);
        }
    }
}

void VarianceEstimate::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, mean_buffer_);
    mean_buffer_ = {};
    gfxDestroyBuffer(gfx_, square_buffer_);
    square_buffer_ = {};
    gfxDestroyBuffer(gfx_, result_buffer_);
    result_buffer_ = {};

    for (GfxBuffer &readback_buffer : readback_buffers_)
    {
        gfxDestroyBuffer(gfx_, readback_buffer);
        readback_buffer = {};
    }

    gfxDestroyProgram(gfx_, variance_estimate_program_);
    variance_estimate_program_ = {};
    gfxDestroyKernel(gfx_, compute_mean_kernel_);
    compute_mean_kernel_ = {};
    gfxDestroyKernel(gfx_, compute_distance_kernel_);
    compute_distance_kernel_ = {};
    gfxDestroyKernel(gfx_, compute_deviation_kernel_);
    compute_deviation_kernel_ = {};
}

void VarianceEstimate::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept
{
    ImGui::Text("Noise amount :  %f", cv_);
}
} // namespace Capsaicin
