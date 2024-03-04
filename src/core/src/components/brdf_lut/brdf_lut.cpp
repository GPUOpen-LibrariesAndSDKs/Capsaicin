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

#include "brdf_lut.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
inline BrdfLut::BrdfLut() noexcept
    : Component(Name)
{}

BrdfLut::~BrdfLut() noexcept
{
    terminate();
}

bool BrdfLut::init(CapsaicinInternal const &capsaicin) noexcept
{
    brdf_lut_buffer_ = gfxCreateTexture2D(gfx_, brdf_lut_size_, brdf_lut_size_, DXGI_FORMAT_R16G16_FLOAT);
    brdf_lut_buffer_.setName("Capsaicin_BrdfLut_LutBuffer");

    GfxProgram const brdf_lut_program =
        gfxCreateProgram(gfx_, "components/brdf_lut/brdf_lut", capsaicin.getShaderPath());
    GfxKernel const brdf_lut_kernel = gfxCreateComputeKernel(gfx_, brdf_lut_program, "ComputeBrdfLut");

    gfxProgramSetParameter(gfx_, brdf_lut_program, "g_LutBuffer", brdf_lut_buffer_);
    gfxProgramSetParameter(gfx_, brdf_lut_program, "g_LutSize", brdf_lut_size_);
    gfxProgramSetParameter(gfx_, brdf_lut_program, "g_SampleSize", brdf_lut_sample_size_);

    // Compute BRDF LUT once in initialization
    uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, brdf_lut_kernel);
    uint32_t const  num_groups_x = (brdf_lut_size_ + num_threads[0] - 1) / num_threads[0];
    uint32_t const  num_groups_y = (brdf_lut_size_ + num_threads[1] - 1) / num_threads[1];
    gfxCommandBindKernel(gfx_, brdf_lut_kernel);
    gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);

    gfxDestroyKernel(gfx_, brdf_lut_kernel);
    gfxDestroyProgram(gfx_, brdf_lut_program);

    return true;
}

void BrdfLut::run([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    // Nothing to do
}

void BrdfLut::terminate() noexcept
{
    gfxDestroyTexture(gfx_, brdf_lut_buffer_);
}

void BrdfLut::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_LutBuffer", brdf_lut_buffer_);
    gfxProgramSetParameter(gfx_, program, "g_LutSize", brdf_lut_size_);
}

} // namespace Capsaicin
