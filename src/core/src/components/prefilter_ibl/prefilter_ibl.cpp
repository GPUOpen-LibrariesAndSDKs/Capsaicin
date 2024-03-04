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

#include "prefilter_ibl.h"

#include "capsaicin_internal.h"

#define _USE_MATH_DEFINES
#include <algorithm>
#include <math.h>

namespace Capsaicin
{
PrefilterIBL::PrefilterIBL() noexcept
    : Component(Name)
{}

PrefilterIBL::~PrefilterIBL() noexcept
{
    terminate();
}

bool PrefilterIBL::init(CapsaicinInternal const &capsaicin) noexcept
{
    prefilter_ibl_buffer_ = gfxCreateTextureCube(
        gfx_, prefilter_ibl_buffer_size_, DXGI_FORMAT_R16G16B16A16_FLOAT, prefilter_ibl_buffer_mips_);
    prefilter_ibl_buffer_.setName("Capsaicin_PrefilterIBL_PrefilterIBLBuffer");

    prefilter_ibl_program_ =
        gfxCreateProgram(gfx_, "components/prefilter_ibl/prefilter_ibl", capsaicin.getShaderPath());

    // init prefiltered IBL
    prefilterIBL(capsaicin);

    return true;
}

void PrefilterIBL::run(CapsaicinInternal &capsaicin) noexcept
{
    // update prefilted IBL
    if (capsaicin.getEnvironmentMapUpdated())
    {
        prefilterIBL(capsaicin);
    }
}

void PrefilterIBL::terminate() noexcept
{
    gfxDestroyProgram(gfx_, prefilter_ibl_program_);
    gfxDestroyTexture(gfx_, prefilter_ibl_buffer_);
}

void PrefilterIBL::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_PrefilteredEnvironmentBuffer", prefilter_ibl_buffer_);
}

void PrefilterIBL::prefilterIBL(CapsaicinInternal const &capsaicin) noexcept
{
    glm::dvec3 const forward_vectors[] = {glm::dvec3(-1.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0), glm::dvec3(0.0, 0.0, -1.0),
        glm::dvec3(0.0, 0.0, 1.0)};

    glm::dvec3 const up_vectors[] = {glm::dvec3(0.0, -1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0),
        glm::dvec3(0.0, 0.0, -1.0), glm::dvec3(0.0, 0.0, 1.0), glm::dvec3(0.0, -1.0, 0.0),
        glm::dvec3(0.0, -1.0, 0.0)};

    for (uint32_t mip_level = 0; mip_level < prefilter_ibl_buffer_mips_; ++mip_level)
    {
        for (uint32_t cubemap_face = 0; cubemap_face < 6; ++cubemap_face)
        {
            GfxDrawState draw_sky_state = {};
            gfxDrawStateSetColorTarget(draw_sky_state, 0, prefilter_ibl_buffer_, mip_level, cubemap_face);

            GfxKernel prefilter_ibl_kernel =
                gfxCreateGraphicsKernel(gfx_, prefilter_ibl_program_, draw_sky_state, "PrefilterIBL");

            uint32_t const buffer_dimensions[] = {std::max(prefilter_ibl_buffer_size_ >> mip_level, 1u),
                std::max(prefilter_ibl_buffer_size_ >> mip_level, 1u)};

            glm::dmat4 const view =
                glm::lookAt(glm::dvec3(0.0), forward_vectors[cubemap_face], up_vectors[cubemap_face]);
            glm::dmat4 const proj          = glm::perspective(M_PI / 2.0, 1.0, 0.1, 1e4);
            glm::mat4 const  view_proj_inv = glm::mat4(glm::inverse(proj * view));

            float const roughness = mip_level / float(prefilter_ibl_buffer_mips_ - 1);

            gfxProgramSetParameter(gfx_, prefilter_ibl_program_, "g_BufferDimensions", buffer_dimensions);
            gfxProgramSetParameter(gfx_, prefilter_ibl_program_, "g_ViewProjectionInverse", view_proj_inv);
            gfxProgramSetParameter(
                gfx_, prefilter_ibl_program_, "g_EnvironmentBuffer", capsaicin.getEnvironmentBuffer());
            gfxProgramSetParameter(
                gfx_, prefilter_ibl_program_, "g_LinearSampler", capsaicin.getLinearSampler());
            gfxProgramSetParameter(gfx_, prefilter_ibl_program_, "g_Roughness", roughness);
            gfxProgramSetParameter(gfx_, prefilter_ibl_program_, "g_SampleSize", prefilter_ibl_sample_size_);

            gfxCommandBindKernel(gfx_, prefilter_ibl_kernel);
            gfxCommandDraw(gfx_, 3);

            gfxDestroyKernel(gfx_, prefilter_ibl_kernel);
        }
    }
}

} // namespace Capsaicin
