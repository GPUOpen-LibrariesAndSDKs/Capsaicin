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
#include "taa.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
TAA::TAA()
    : RenderTechnique("TAA")
{}

TAA::~TAA()
{
    terminate();
}

RenderOptionList TAA::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(taa_enable, options));
    return newOptions;
}

TAA::RenderOptions TAA::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(taa_enable, newOptions, options)
    return newOptions;
}

SharedTextureList TAA::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::ReadWrite});

    textures.push_back({"VisibilityDepth"});
    textures.push_back({"Velocity"});
    return textures;
}

bool TAA::init(CapsaicinInternal const &capsaicin) noexcept
{
    std::vector<char const *> defines;
    if (capsaicin.hasSharedTexture("DirectLighting"))
    {
        defines.push_back("HAS_DIRECT_LIGHTING_BUFFER");
    }
    if (capsaicin.hasSharedTexture("GlobalIllumination"))
    {
        defines.push_back("HAS_GLOBAL_ILLUMINATION_BUFFER");
    }
    taa_program_             = capsaicin.createProgram("render_techniques/taa/taa");
    resolve_temporal_kernel_ = gfxCreateComputeKernel(
        gfx_, taa_program_, "ResolveTemporal", defines.data(), static_cast<uint32_t>(defines.size()));
    update_history_kernel_ = gfxCreateComputeKernel(gfx_, taa_program_, "UpdateHistory");

    uint32_t index = 0;
    for (GfxTexture &color_buffer : color_buffers_)
    {
        char buffer[64];
        GFX_SNPRINTF(buffer, sizeof(buffer), "ColorBuffer%u", index);
        color_buffer = capsaicin.createRenderTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, buffer);
        ++index;
    }
    return !!taa_program_;
}

void TAA::render(CapsaicinInternal &capsaicin) noexcept
{
    auto const newOptions   = convertOptions(capsaicin.getOptions());
    bool const optionChange = newOptions.taa_enable != options.taa_enable;
    options                 = newOptions;

    if (options.taa_enable)
    {
        // Make sure our color buffers are properly created (a resize may have occured while TAA was disabled
        // so we need to check the buffer resolution directly)
        bool       not_cleared_history = !optionChange;
        auto const dimensions          = capsaicin.getRenderDimensions();
        if (uint2(color_buffers_[0].getWidth(), color_buffers_[0].getHeight()) != dimensions)
        {
            for (GfxTexture &color_buffer : color_buffers_)
            {
                color_buffer = capsaicin.resizeRenderTexture(color_buffer);
            }

            not_cleared_history = false;
        }

        // Bind the shader parameters
        gfxProgramSetParameter(
            gfx_, taa_program_, "g_HaveHistory", not_cleared_history && capsaicin.getFrameIndex() > 0);
        gfxProgramSetParameter(gfx_, taa_program_, "g_BufferDimensions", dimensions);

        gfxProgramSetParameter(
            gfx_, taa_program_, "g_DepthBuffer", capsaicin.getSharedTexture("VisibilityDepth"));
        gfxProgramSetParameter(
            gfx_, taa_program_, "g_VelocityBuffer", capsaicin.getSharedTexture("Velocity"));
        auto const &colorTexture = capsaicin.getSharedTexture("Color");
        gfxProgramSetParameter(gfx_, taa_program_, "g_ColorInBuffer", colorTexture);

        gfxProgramSetParameter(gfx_, taa_program_, "g_ColorBuffer", colorTexture);

        gfxProgramSetParameter(gfx_, taa_program_, "g_LinearSampler", capsaicin.getLinearSampler());
        gfxProgramSetParameter(gfx_, taa_program_, "g_NearestSampler", capsaicin.getNearestSampler());

        // Perform the temporal resolve
        {
            TimedSection const timed_section(*this, "ResolveTemporal");

            uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, resolve_temporal_kernel_);
            uint32_t const  num_groups_x = (dimensions.x + num_threads[0] - 1) / num_threads[0];
            uint32_t const  num_groups_y = (dimensions.y + num_threads[1] - 1) / num_threads[1];

            gfxProgramSetParameter(gfx_, taa_program_, "g_OutputBuffer", color_buffers_[0]);
            gfxProgramSetParameter(gfx_, taa_program_, "g_HistoryBuffer", color_buffers_[1]);

            gfxCommandBindKernel(gfx_, resolve_temporal_kernel_);
            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        }

        // And update the history information
        {
            TimedSection const timed_section(*this, "UpdateHistory");

            uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, update_history_kernel_);
            uint32_t const  num_groups_x = (dimensions.x + num_threads[0] - 1) / num_threads[0];
            uint32_t const  num_groups_y = (dimensions.y + num_threads[1] - 1) / num_threads[1];

            gfxProgramSetParameter(gfx_, taa_program_, "g_OutputBuffer", color_buffers_[1]);
            gfxProgramSetParameter(gfx_, taa_program_, "g_HistoryBuffer", color_buffers_[0]);

            gfxCommandBindKernel(gfx_, update_history_kernel_);
            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        }
    }
}

void TAA::terminate() noexcept
{
    for (GfxTexture const &color_buffer : color_buffers_)
    {
        gfxDestroyTexture(gfx_, color_buffer);
    }

    gfxDestroyProgram(gfx_, taa_program_);
    gfxDestroyKernel(gfx_, resolve_temporal_kernel_);
    gfxDestroyKernel(gfx_, update_history_kernel_);

    memset(color_buffers_, 0, sizeof(color_buffers_));
}

void TAA::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    ImGui::Checkbox("Use TAA", &capsaicin.getOption<bool>("taa_enable"));
}
} // namespace Capsaicin
