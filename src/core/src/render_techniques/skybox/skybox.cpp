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

RenderOptionList Skybox::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(skybox_use_jittering, options_));
    return newOptions;
}

Skybox::RenderOptions Skybox::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(skybox_use_jittering, newOptions, options)
    return newOptions;
}

SharedTextureList Skybox::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"DirectLighting", SharedTexture::Access::Write, SharedTexture::Flags::Clear,
        DXGI_FORMAT_R16G16B16A16_FLOAT});
    textures.push_back({"Velocity", SharedTexture::Access::Write, SharedTexture::Flags::None,
        DXGI_FORMAT_R16G16_FLOAT});
    textures.push_back({"Depth", SharedTexture::Access::ReadWrite});
    return textures;
}

bool Skybox::init(CapsaicinInternal const &capsaicin) noexcept
{
    GfxDrawState const skybox_draw_state;
    gfxDrawStateSetColorTarget(
        skybox_draw_state, 0, capsaicin.getSharedTexture("DirectLighting").getFormat());
    gfxDrawStateSetColorTarget(
        skybox_draw_state, 1, capsaicin.getSharedTexture("Velocity").getFormat());
    gfxDrawStateSetDepthStencilTarget(skybox_draw_state, capsaicin.getSharedTexture("Depth").getFormat());
    gfxDrawStateSetDepthWriteMask(skybox_draw_state, D3D12_DEPTH_WRITE_MASK_ZERO);
    gfxDrawStateSetDepthFunction(skybox_draw_state, D3D12_COMPARISON_FUNC_GREATER);

    skybox_program_ = capsaicin.createProgram("render_techniques/skybox/skybox");
    skybox_kernel_  = gfxCreateGraphicsKernel(gfx_, skybox_program_, skybox_draw_state);
    return !!skybox_program_;
}

void Skybox::render(CapsaicinInternal &capsaicin) noexcept
{
    options_ = convertOptions(capsaicin.getOptions());

    if (!capsaicin.getEnvironmentBuffer())
    {
        return;
    }

    TimedSection const timed_section(*this, "DrawSkybox");

    gfxProgramSetParameter(gfx_, skybox_program_, "g_Eye", capsaicin.getCamera().eye);
    gfxProgramSetParameter(gfx_, skybox_program_, "g_BufferDimensions", capsaicin.getRenderDimensions());
    gfxProgramSetParameter(gfx_, skybox_program_, "g_ReprojectionMatrix",
        capsaicin.getCameraMatrices(options_.skybox_use_jittering).reprojection);
    gfxProgramSetParameter(gfx_, skybox_program_, "g_ViewProjectionInverse",
        capsaicin.getCameraMatrices(options_.skybox_use_jittering).inv_view_projection);

    gfxProgramSetParameter(gfx_, skybox_program_, "g_EnvironmentBuffer", capsaicin.getEnvironmentBuffer());
    gfxProgramSetParameter(gfx_, skybox_program_, "g_LinearSampler", capsaicin.getLinearSampler());

    gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("DirectLighting"));
    gfxCommandBindColorTarget(gfx_, 1, capsaicin.getSharedTexture("Velocity"));
    gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));

    gfxCommandBindKernel(gfx_, skybox_kernel_);
    gfxCommandDraw(gfx_, 3);
}

void Skybox::terminate() noexcept
{
    gfxDestroyProgram(gfx_, skybox_program_);
    gfxDestroyKernel(gfx_, skybox_kernel_);
}
} // namespace Capsaicin
