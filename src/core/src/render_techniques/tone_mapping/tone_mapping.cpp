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
#include "tone_mapping.h"

#include "capsaicin_internal.h"
#include "../../components/blue_noise_sampler/blue_noise_sampler.h"

namespace Capsaicin
{
ToneMapping::ToneMapping()
    : RenderTechnique("Tone mapping")
{}

ToneMapping::~ToneMapping()
{
    terminate();
}

RenderOptionList ToneMapping::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(tonemap_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(tonemap_exposure, options));
    return newOptions;
}

ToneMapping::RenderOptions ToneMapping::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(tonemap_enable, newOptions, options)
    RENDER_OPTION_GET(tonemap_exposure, newOptions, options)
    return newOptions;
}

ComponentList ToneMapping::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(BlueNoiseSampler));
    return components;
}

AOVList ToneMapping::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"Color", AOV::ReadWrite});
    aovs.push_back({"Debug", AOV::ReadWrite});
    return aovs;
}

bool ToneMapping::init(CapsaicinInternal const &capsaicin) noexcept
{
    tone_mapping_program_ =
        gfxCreateProgram(gfx_, "render_techniques/tone_mapping/tone_mapping", capsaicin.getShaderPath());
    tone_mapping_kernel_ = gfxCreateComputeKernel(gfx_, tone_mapping_program_, "Tonemap");
    return !!tone_mapping_program_;
}

void ToneMapping::render(CapsaicinInternal &capsaicin) noexcept
{
    options = convertOptions(capsaicin.getOptions());

    if (!options.tonemap_enable) return;

    uint32_t const buffer_dimensions[] =
    {
        capsaicin.getWidth(),
        capsaicin.getHeight()
    };

    gfxProgramSetParameter(gfx_, tone_mapping_program_, "g_BufferDimensions", buffer_dimensions);
    gfxProgramSetParameter(gfx_, tone_mapping_program_, "g_FrameIndex", capsaicin.getFrameIndex());
    gfxProgramSetParameter(gfx_, tone_mapping_program_, "g_Exposure", options.tonemap_exposure);

    GfxTexture input      = capsaicin.getAOVBuffer("Color");
    GfxTexture output     = input;
    auto       debug_view = capsaicin.getCurrentDebugView();

    if (!debug_view.empty() && debug_view != "None")
    {
        // Tone map the debug buffer if we are using a debug view
        if (capsaicin.checkDebugViewAOV(debug_view))
        {
            // If the debug view is actually an AOV then only tonemap if its a floating point format
            auto const debugAOV = capsaicin.getAOVBuffer(debug_view);
            auto const format   = debugAOV.getFormat();
            if (format == DXGI_FORMAT_R32G32B32A32_FLOAT || format == DXGI_FORMAT_R32G32B32_FLOAT
                || format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_R11G11B10_FLOAT)
            {
                input  = debugAOV;
                output = capsaicin.getAOVBuffer("Debug");
            }
        }
        else
        {
            input  = capsaicin.getAOVBuffer("Debug");
            output = input;
        }
    }

    auto blue_noise_sampler = capsaicin.getComponent<BlueNoiseSampler>();
    blue_noise_sampler->addProgramParameters(capsaicin, tone_mapping_program_);

    gfxProgramSetParameter(gfx_, tone_mapping_program_, "g_InputBuffer", input);
    gfxProgramSetParameter(gfx_, tone_mapping_program_, "g_OutputBuffer", output);

    uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, tone_mapping_kernel_);
    uint32_t const  num_groups_x = (buffer_dimensions[0] + num_threads[0] - 1) / num_threads[0];
    uint32_t const  num_groups_y = (buffer_dimensions[1] + num_threads[1] - 1) / num_threads[1];

    gfxCommandBindKernel(gfx_, tone_mapping_kernel_);
    gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
}

void ToneMapping::terminate() noexcept
{
    gfxDestroyKernel(gfx_, tone_mapping_kernel_);
    gfxDestroyProgram(gfx_, tone_mapping_program_);
}

void ToneMapping::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    bool &enabled = capsaicin.getOption<bool>("tonemap_enable");
    if (!enabled) ImGui::BeginDisabled(true);
    ImGui::DragFloat("Exposure", &capsaicin.getOption<float>("tonemap_exposure"), 5e-3f);
    if (!enabled) ImGui::EndDisabled();
    ImGui::Checkbox("Enable Tone Mapping", &enabled);
}
} // namespace Capsaicin
