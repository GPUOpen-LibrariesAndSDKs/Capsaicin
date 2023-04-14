/**********************************************************************
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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
#include "update_history.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
UpdateHistory::UpdateHistory()
    : RenderTechnique("Update history")
{}

UpdateHistory::~UpdateHistory()
{
    terminate();
}

AOVList UpdateHistory::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"PrevCombinedIllumination", AOV::Write, AOV::Accumulate, DXGI_FORMAT_R16G16B16A16_FLOAT});
    aovs.push_back({"DirectLighting"});
    aovs.push_back({"GlobalIllumination"});
    return aovs;
}

bool UpdateHistory::init(CapsaicinInternal const &capsaicin) noexcept
{
    update_history_program_ =
        gfxCreateProgram(gfx_, "render_techniques/taa/update_history", capsaicin.getShaderPath());
    update_history_kernel_ = gfxCreateComputeKernel(gfx_, update_history_program_, "UpdateHistory");
    return !!update_history_program_;
}

void UpdateHistory::render(CapsaicinInternal &capsaicin) noexcept
{
    gfxProgramSetParameter(
        gfx_, update_history_program_, "g_DirectLightingBuffer", capsaicin.getAOVBuffer("DirectLighting"));
    gfxProgramSetParameter(gfx_, update_history_program_, "g_GlobalIlluminationBuffer",
        capsaicin.getAOVBuffer("GlobalIllumination"));

    gfxProgramSetParameter(gfx_, update_history_program_, "g_PrevCombinedIlluminationBuffer",
        capsaicin.getAOVBuffer("PrevCombinedIllumination"));

    uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, update_history_kernel_);
    uint32_t const  num_groups_x = (capsaicin.getWidth() + num_threads[0] - 1) / num_threads[0];
    uint32_t const  num_groups_y = (capsaicin.getHeight() + num_threads[1] - 1) / num_threads[1];

    gfxCommandBindKernel(gfx_, update_history_kernel_);
    gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
}

void UpdateHistory::terminate()
{
    gfxDestroyProgram(gfx_, update_history_program_);
    gfxDestroyKernel(gfx_, update_history_kernel_);
}
} // namespace Capsaicin
