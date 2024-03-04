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
#include "atmosphere.h"

#include "capsaicin_internal.h"

#define _USE_MATH_DEFINES
#include "math.h"

namespace
{
glm::vec3 const forward_vectors[] = {glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
    glm::vec3(0.0f, 0.0f, 1.0f)};

glm::vec3 const up_vectors[] = {glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(0.0f, -1.0f, 0.0f)};
} // namespace

namespace Capsaicin
{
Atmosphere::Atmosphere()
    : RenderTechnique("Atmosphere")
{}

Atmosphere::~Atmosphere()
{
    terminate();
}

RenderOptionList Atmosphere::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(atmosphere_enable, options));
    return newOptions;
}

Atmosphere::RenderOptions Atmosphere::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(atmosphere_enable, newOptions, options)
    return newOptions;
}

bool Atmosphere::init(CapsaicinInternal const &capsaicin) noexcept
{
    atmosphere_program_ =
        gfxCreateProgram(gfx_, "render_techniques/atmosphere/atmosphere", capsaicin.getShaderPath());
    draw_atmosphere_kernel_   = gfxCreateComputeKernel(gfx_, atmosphere_program_, "DrawAtmosphere");
    filter_atmosphere_kernel_ = gfxCreateComputeKernel(gfx_, atmosphere_program_, "FilterAtmosphere");
    return !!atmosphere_program_;
}

void Atmosphere::render(CapsaicinInternal &capsaicin) noexcept
{
    options = convertOptions(capsaicin.getOptions());
    if (!options.atmosphere_enable) return;

    GfxTexture environment_buffer = capsaicin.getEnvironmentBuffer();
    if (!environment_buffer)
    {
        return; // no environment buffer was created
    }

    uint32_t const buffer_dimensions[] = {environment_buffer.getWidth(), environment_buffer.getHeight()};

    gfxProgramSetParameter(gfx_, atmosphere_program_, "g_Eye", capsaicin.getCamera().eye);
    gfxProgramSetParameter(gfx_, atmosphere_program_, "g_FrameIndex", capsaicin.getFrameIndex());
    gfxProgramSetParameter(gfx_, atmosphere_program_, "g_BufferDimensions", buffer_dimensions);

    gfxProgramSetParameter(gfx_, atmosphere_program_, "g_OutEnvironmentBuffer", environment_buffer);

    // Draw the atmosphere
    {
        TimedSection const timed_section(*this, "DrawAtmosphere");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, draw_atmosphere_kernel_);
        uint32_t const  num_groups_x = (buffer_dimensions[0] + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_dimensions[1] + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, draw_atmosphere_kernel_);

        for (uint32_t face_index = 0; face_index < environment_buffer.getDepth(); ++face_index)
        {
            glm::mat4 const view =
                glm::lookAt(glm::vec3(0.0f), forward_vectors[face_index], up_vectors[face_index]);
            glm::mat4 const proj          = glm::perspective((float)M_PI / 2.0f, 1.0f, 0.1f, 1e4f);
            glm::mat4 const view_proj_inv = glm::inverse(proj * view);

            gfxProgramSetParameter(gfx_, atmosphere_program_, "g_FaceIndex", face_index);
            gfxProgramSetParameter(gfx_, atmosphere_program_, "g_ViewProjectionInverse", view_proj_inv);

            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        }
    }

    // Filter the atmosphere
    {
        TimedSection const timed_section(*this, "FilterAtmosphere");

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, filter_atmosphere_kernel_);

        gfxCommandBindKernel(gfx_, filter_atmosphere_kernel_);

        uint32_t buffer_width  = buffer_dimensions[0];
        uint32_t buffer_height = buffer_dimensions[1];

        for (uint32_t mip_level = 1; mip_level < environment_buffer.getMipLevels(); ++mip_level)
        {
            gfxProgramSetParameter(
                gfx_, atmosphere_program_, "g_BufferDimensions", glm::uvec2(buffer_width, buffer_height));

            gfxProgramSetParameter(
                gfx_, atmosphere_program_, "g_InEnvironmentBuffer", environment_buffer, mip_level - 1);
            gfxProgramSetParameter(
                gfx_, atmosphere_program_, "g_OutEnvironmentBuffer", environment_buffer, mip_level);

            buffer_width  = GFX_MAX(buffer_width >> 1, 1u);
            buffer_height = GFX_MAX(buffer_height >> 1, 1u);

            uint32_t const num_groups_x = (buffer_width + num_threads[0] - 1) / num_threads[0];
            uint32_t const num_groups_y = (buffer_height + num_threads[1] - 1) / num_threads[1];
            uint32_t const num_groups_z = environment_buffer.getDepth();

            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, num_groups_z);
        }
    }
}

void Atmosphere::terminate() noexcept
{
    gfxDestroyProgram(gfx_, atmosphere_program_);
    gfxDestroyKernel(gfx_, draw_atmosphere_kernel_);
    gfxDestroyKernel(gfx_, filter_atmosphere_kernel_);
}
} // namespace Capsaicin
