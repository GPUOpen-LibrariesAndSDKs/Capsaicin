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

AOVList TAA::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"Color", AOV::Write});

    aovs.push_back({"VisibilityDepth"});
    aovs.push_back({"Velocity"});
    aovs.push_back({.name = "DirectLighting", .flags = AOV::Optional});
    aovs.push_back({.name = "GlobalIllumination", .flags = AOV::Optional});
    return aovs;
}

bool TAA::init(CapsaicinInternal const &capsaicin) noexcept
{
    std::vector<char const *> defines;
    if (capsaicin.hasAOVBuffer("DirectLighting"))
    {
        defines.push_back("HAS_DIRECT_LIGHTING_BUFFER");
    }
    if (capsaicin.hasAOVBuffer("GlobalIllumination"))
    {
        defines.push_back("HAS_GLOBAL_ILLUMINATION_BUFFER");
    }
    taa_program_             = gfxCreateProgram(gfx_, "render_techniques/taa/taa", capsaicin.getShaderPath());
    resolve_temporal_kernel_ = gfxCreateComputeKernel(
        gfx_, taa_program_, "ResolveTemporal", defines.data(), (uint32_t)defines.size());
    resolve_passthru_kernel_ = gfxCreateComputeKernel(
        gfx_, taa_program_, "ResolvePassthru", defines.data(), (uint32_t)defines.size());
    update_history_kernel_ = gfxCreateComputeKernel(gfx_, taa_program_, "UpdateHistory");
    return !!taa_program_;
}

void TAA::render(CapsaicinInternal &capsaicin) noexcept
{
    // Make sure our color buffers are properly created
    uint32_t const buffer_width  = capsaicin.getWidth();
    uint32_t const buffer_height = capsaicin.getHeight();

    bool not_cleared_history = true;
    if (buffer_width != color_buffers_->getWidth() || buffer_height != color_buffers_->getHeight())
    {
        for (GfxTexture &color_buffer : color_buffers_)
            gfxDestroyTexture(gfx_, color_buffer);

        for (uint32_t i = 0; i < ARRAYSIZE(color_buffers_); ++i)
        {
            char buffer[64];
            GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ColorBuffer%u", i);

            color_buffers_[i] =
                gfxCreateTexture2D(gfx_, buffer_width, buffer_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
            color_buffers_[i].setName(buffer);

            gfxCommandClearTexture(gfx_, color_buffers_[i]);
        }

        not_cleared_history = false;
    }

    // Bind the shader parameters
    uint32_t const buffer_dimensions[] = {buffer_width, buffer_height};

    options = convertOptions(capsaicin.getOptions());

    gfxProgramSetParameter(
        gfx_, taa_program_, "g_HaveHistory", not_cleared_history && capsaicin.getFrameIndex() > 0);
    gfxProgramSetParameter(gfx_, taa_program_, "g_BufferDimensions", buffer_dimensions);

    gfxProgramSetParameter(gfx_, taa_program_, "g_DepthBuffer", capsaicin.getAOVBuffer("VisibilityDepth"));
    gfxProgramSetParameter(gfx_, taa_program_, "g_VelocityBuffer", capsaicin.getAOVBuffer("Velocity"));

    gfxProgramSetParameter(gfx_, taa_program_, "g_ColorBuffer", capsaicin.getAOVBuffer("Color"));
    if (capsaicin.hasAOVBuffer("DirectLighting"))
    {
        gfxProgramSetParameter(
            gfx_, taa_program_, "g_DirectLightingBuffer", capsaicin.getAOVBuffer("DirectLighting"));
    }
    if (capsaicin.hasAOVBuffer("GlobalIllumination"))
    {
        gfxProgramSetParameter(
            gfx_, taa_program_, "g_GlobalIlluminationBuffer", capsaicin.getAOVBuffer("GlobalIllumination"));
    }

    gfxProgramSetParameter(gfx_, taa_program_, "g_LinearSampler", capsaicin.getLinearSampler());
    gfxProgramSetParameter(gfx_, taa_program_, "g_NearestSampler", capsaicin.getNearestSampler());

    // If TAA is not enabled, simply pass through
    if (!options.taa_enable)
    {
        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, resolve_passthru_kernel_);
        uint32_t const  num_groups_x = (buffer_width + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_height + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, resolve_passthru_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }
    else
    {
        // Perform the temporal resolve
        {
            TimedSection const timed_section(*this, "ResolveTemporal");

            uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, resolve_temporal_kernel_);
            uint32_t const  num_groups_x = (buffer_width + num_threads[0] - 1) / num_threads[0];
            uint32_t const  num_groups_y = (buffer_height + num_threads[1] - 1) / num_threads[1];

            gfxProgramSetParameter(gfx_, taa_program_, "g_OutputBuffer", color_buffers_[0]);
            gfxProgramSetParameter(gfx_, taa_program_, "g_HistoryBuffer", color_buffers_[1]);

            gfxCommandBindKernel(gfx_, resolve_temporal_kernel_);
            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        }

        // And update the history information
        {
            TimedSection const timed_section(*this, "UpdateHistory");

            uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, update_history_kernel_);
            uint32_t const  num_groups_x = (buffer_width + num_threads[0] - 1) / num_threads[0];
            uint32_t const  num_groups_y = (buffer_height + num_threads[1] - 1) / num_threads[1];

            gfxProgramSetParameter(gfx_, taa_program_, "g_OutputBuffer", color_buffers_[1]);
            gfxProgramSetParameter(gfx_, taa_program_, "g_HistoryBuffer", color_buffers_[0]);

            gfxCommandBindKernel(gfx_, update_history_kernel_);
            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        }
    }
}

void TAA::terminate() noexcept
{
    for (GfxTexture &color_buffer : color_buffers_)
        gfxDestroyTexture(gfx_, color_buffer);

    gfxDestroyProgram(gfx_, taa_program_);
    gfxDestroyKernel(gfx_, resolve_temporal_kernel_);
    gfxDestroyKernel(gfx_, resolve_passthru_kernel_);
    gfxDestroyKernel(gfx_, update_history_kernel_);

    memset(color_buffers_, 0, sizeof(color_buffers_));
}

void TAA::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    ImGui::Checkbox("Use TAA", &capsaicin.getOption<bool>("taa_enable"));
}
} // namespace Capsaicin
