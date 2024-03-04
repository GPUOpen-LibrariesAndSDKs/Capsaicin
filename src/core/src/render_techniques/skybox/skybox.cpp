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
#include "skybox.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
Skybox::Skybox()
    : RenderTechnique("Skybox")
{}

Skybox::~Skybox()
{
    terminate();
}

AOVList Skybox::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"DirectLighting", AOV::Write, AOV::Clear, DXGI_FORMAT_R16G16B16A16_FLOAT});
    aovs.push_back({"Depth", AOV::ReadWrite});
    return aovs;
}

bool Skybox::init(CapsaicinInternal const &capsaicin) noexcept
{
    GfxDrawState skybox_draw_state;
    gfxDrawStateSetColorTarget(skybox_draw_state, 0, capsaicin.getAOVBuffer("DirectLighting"));
    gfxDrawStateSetDepthStencilTarget(skybox_draw_state, capsaicin.getAOVBuffer("Depth"));

    skybox_program_ = gfxCreateProgram(gfx_, "render_techniques/skybox/skybox", capsaicin.getShaderPath());
    skybox_kernel_  = gfxCreateGraphicsKernel(gfx_, skybox_program_, skybox_draw_state);
    return !!skybox_program_;
}

void Skybox::render(CapsaicinInternal &capsaicin) noexcept
{
    TimedSection const timed_section(*this, "DrawSkybox");
    uint32_t const     buffer_dimensions[] = {capsaicin.getWidth(), capsaicin.getHeight()};

    gfxProgramSetParameter(gfx_, skybox_program_, "g_Eye", capsaicin.getCamera().eye);
    gfxProgramSetParameter(gfx_, skybox_program_, "g_BufferDimensions", buffer_dimensions);
    gfxProgramSetParameter(
        gfx_, skybox_program_, "g_ViewProjectionInverse", capsaicin.getCameraMatrices().inv_view_projection);

    gfxProgramSetParameter(gfx_, skybox_program_, "g_EnvironmentBuffer", capsaicin.getEnvironmentBuffer());

    gfxProgramSetParameter(gfx_, skybox_program_, "g_LinearSampler", capsaicin.getLinearSampler());

    gfxCommandBindKernel(gfx_, skybox_kernel_);
    gfxCommandDraw(gfx_, 3);
}

void Skybox::terminate() noexcept
{
    gfxDestroyProgram(gfx_, skybox_program_);
    gfxDestroyKernel(gfx_, skybox_kernel_);
}
} // namespace Capsaicin
