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
#include "ssgi.h"

#include "capsaicin_internal.h"
#include "components/blue_noise_sampler/blue_noise_sampler.h"
#include "components/stratified_sampler/stratified_sampler.h"
#include "ssgi_shared.h"

namespace Capsaicin
{
SSGI::SSGI()
    : RenderTechnique("SSGI")
{}

SSGI::~SSGI()
{
    terminate();
}

RenderOptionList SSGI::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(ssgi_slice_count_, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(ssgi_step_count_, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(ssgi_view_radius_, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(ssgi_falloff_range_, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(ssgi_unroll_kernel_, options_));
    return newOptions;
}

SSGI::RenderOptions SSGI::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(ssgi_slice_count_, newOptions, options)
    RENDER_OPTION_GET(ssgi_step_count_, newOptions, options)
    RENDER_OPTION_GET(ssgi_view_radius_, newOptions, options)
    RENDER_OPTION_GET(ssgi_falloff_range_, newOptions, options)
    RENDER_OPTION_GET(ssgi_unroll_kernel_, newOptions, options)
    return newOptions;
}

ComponentList SSGI::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(BlueNoiseSampler));
    components.emplace_back(COMPONENT_MAKE(StratifiedSampler));
    return components;
}

AOVList SSGI::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"Debug", AOV::Write});
    aovs.push_back({"OcclusionAndBentNormal", AOV::Write, AOV::None, DXGI_FORMAT_R16G16B16A16_FLOAT});
    aovs.push_back({"NearFieldGlobalIllumination", AOV::Write, AOV::None, DXGI_FORMAT_R16G16B16A16_FLOAT});

    aovs.push_back({"VisibilityDepth"});
    aovs.push_back({"ShadingNormal"});
    aovs.push_back({"PrevCombinedIllumination"});
    return aovs;
}

DebugViewList SSGI::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("Occlusion");
    views.emplace_back("BentNormal");
    return views;
}

bool SSGI::init(CapsaicinInternal const &capsaicin) noexcept
{
    initializeStaticResources(capsaicin);
    initializeBuffers(capsaicin);
    initializeKernels(capsaicin);
    return !!ssgi_program_;
}

void SSGI::render(CapsaicinInternal &capsaicin) noexcept
{
    // BE CAREFUL: Used for rendering current frame and initializing next frame
    auto const options            = convertOptions(capsaicin.getOptions());
    auto       blue_noise_sampler = capsaicin.getComponent<BlueNoiseSampler>();
    auto       stratified_sampler = capsaicin.getComponent<StratifiedSampler>();

    options_ = options;

    uint32_t   buffer_width      = capsaicin.getWidth();
    uint32_t   buffer_height     = capsaicin.getHeight();
    bool const need_reallocation = (buffer_width != buffer_width_ || buffer_height != buffer_height_);
    if (need_reallocation)
    {
        destroyBuffers();
        initializeBuffers(capsaicin);
    }

    // Constants
    GfxBuffer     ssgi_constant_buffer = capsaicin.allocateConstantBuffer<SSGIConstants>(1);
    SSGIConstants ssgi_constants;
    {
        auto const &camera_matrices      = capsaicin.getCameraMatrices();
        auto const &camera               = capsaicin.getCamera();
        ssgi_constants.view              = camera_matrices.view;
        ssgi_constants.proj              = camera_matrices.projection;
        ssgi_constants.inv_view          = camera_matrices.inv_view;
        ssgi_constants.inv_proj          = camera_matrices.inv_projection;
        ssgi_constants.inv_view_proj     = camera_matrices.inv_view_projection;
        ssgi_constants.eye               = glm::float4(camera.eye, 1.f);
        ssgi_constants.forward           = glm::float4(glm::normalize(camera.center - camera.eye), 0.f);
        ssgi_constants.buffer_dimensions = glm::int2(buffer_width_, buffer_height_);
        ssgi_constants.frame_index       = capsaicin.getFrameIndex();
        ssgi_constants.slice_count       = options_.ssgi_slice_count_;
        ssgi_constants.step_count        = options_.ssgi_step_count_;
        ssgi_constants.view_radius       = options_.ssgi_view_radius_;
        ssgi_constants.uv_radius =
            options_.ssgi_view_radius_ * 0.5f
            * glm::max(camera_matrices.projection[0][0], camera_matrices.projection[1][1]);
        float falloff_range        = options_.ssgi_view_radius_ * options_.ssgi_falloff_range_;
        float falloff_from         = options_.ssgi_view_radius_ * (1.f - options_.ssgi_falloff_range_);
        ssgi_constants.falloff_mul = -1.f / falloff_range;
        ssgi_constants.falloff_add = falloff_from / falloff_range + 1.f;
    }
    gfxBufferGetData<SSGIConstants>(gfx_, ssgi_constant_buffer)[0] = ssgi_constants;

    gfxProgramSetParameter(gfx_, ssgi_program_, "g_NearFar",
        glm::float2(capsaicin.getCamera().nearZ, capsaicin.getCamera().farZ));              //
    gfxProgramSetParameter(gfx_, ssgi_program_, "g_InvDeviceZ", capsaicin.getInvDeviceZ()); //

    blue_noise_sampler->addProgramParameters(capsaicin, ssgi_program_);

    stratified_sampler->addProgramParameters(capsaicin, ssgi_program_);

    gfxProgramSetParameter(gfx_, ssgi_program_, "g_SSGIConstants", ssgi_constant_buffer);
    gfxProgramSetParameter(gfx_, ssgi_program_, "g_DepthBuffer", capsaicin.getAOVBuffer("VisibilityDepth"));
    gfxProgramSetParameter(
        gfx_, ssgi_program_, "g_ShadingNormalBuffer", capsaicin.getAOVBuffer("ShadingNormal"));
    gfxProgramSetParameter(
        gfx_, ssgi_program_, "g_LightingBuffer", capsaicin.getAOVBuffer("PrevCombinedIllumination"));
    gfxProgramSetParameter(gfx_, ssgi_program_, "g_OcclusionAndBentNormalBuffer",
        capsaicin.getAOVBuffer("OcclusionAndBentNormal"));
    gfxProgramSetParameter(gfx_, ssgi_program_, "g_NearFieldGlobalIlluminationBuffer",
        capsaicin.getAOVBuffer("NearFieldGlobalIllumination"));
    gfxProgramSetSamplerState(gfx_, ssgi_program_, "g_PointSampler", point_sampler_);

    {
        TimedSection const timed_section(*this, "Main");

        GfxKernel main_kernel = options_.ssgi_unroll_kernel_ ? main_unrolled_kernel_ : main_kernel_;

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, main_kernel);
        uint32_t const  num_groups_x = (buffer_width_ + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_height_ + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, main_kernel);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }

    // Debug modes
    if (capsaicin.getCurrentDebugView() == "Occlusion")
    {
        GfxCommandEvent const command_event(gfx_, "Debug Occlusion");

        gfxProgramSetParameter(
            gfx_, debug_occlusion_program_, "g_BufferDimensions", glm::int2(buffer_width_, buffer_height_));
        gfxProgramSetParameter(gfx_, debug_occlusion_program_, "g_OcclusionAndBentNormalBuffer",
            capsaicin.getAOVBuffer("OcclusionAndBentNormal"));
        gfxProgramSetParameter(
            gfx_, debug_occlusion_program_, "g_DebugBuffer", capsaicin.getAOVBuffer("Debug"));

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, debug_occlusion_kernel_);
        uint32_t const  num_groups_x = (buffer_width_ + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_height_ + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, debug_occlusion_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }
    else if (capsaicin.getCurrentDebugView() == "BentNormal")
    {
        GfxCommandEvent const command_event(gfx_, "Debug Bent Normal");

        gfxProgramSetParameter(
            gfx_, debug_bent_normal_program_, "g_BufferDimensions", glm::int2(buffer_width_, buffer_height_));
        gfxProgramSetParameter(gfx_, debug_bent_normal_program_, "g_OcclusionAndBentNormalBuffer",
            capsaicin.getAOVBuffer("OcclusionAndBentNormal"));
        gfxProgramSetParameter(
            gfx_, debug_bent_normal_program_, "g_DebugBuffer", capsaicin.getAOVBuffer("Debug"));

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, debug_bent_normal_kernel_);
        uint32_t const  num_groups_x = (buffer_width_ + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (buffer_height_ + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, debug_bent_normal_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }

    // Release our constant buffer
    gfxDestroyBuffer(gfx_, ssgi_constant_buffer);
}

void SSGI::terminate() noexcept
{
    destroyStaticResources();
    destroyBuffers();
    destroyKernels();
}

void SSGI::initializeStaticResources([[maybe_unused]] CapsaicinInternal const &capsaicin)
{
    point_sampler_ = gfxCreateSamplerState(gfx_, D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
}

void SSGI::initializeBuffers(CapsaicinInternal const &capsaicin)
{
    uint32_t buffer_width  = capsaicin.getWidth();
    uint32_t buffer_height = capsaicin.getHeight();

    buffer_width_  = buffer_width;
    buffer_height_ = buffer_height;
}

void SSGI::initializeKernels(CapsaicinInternal const &capsaicin)
{
    std::string shader_path = capsaicin.getShaderPath();
    shader_path += "render_techniques/ssgi";

    // Defines
    std::vector<char const *> global_defines {};
    std::vector<char const *> unroll_defines {"UNROLL_SLICE_LOOP", "UNROLL_STEP_LOOP"};

    // Kernels
    ssgi_program_ = gfxCreateProgram(gfx_, "ssgi", shader_path.c_str());
    {
        std::vector<char const *> defines;
        defines.insert(defines.cend(), global_defines.cbegin(), global_defines.cend());
        main_kernel_ =
            gfxCreateComputeKernel(gfx_, ssgi_program_, "Main", defines.data(), (uint32_t)defines.size());
    }
    {
        std::vector<char const *> defines;
        defines.insert(defines.cend(), global_defines.cbegin(), global_defines.cend());
        defines.insert(defines.cend(), unroll_defines.cbegin(), unroll_defines.cend());
        main_unrolled_kernel_ =
            gfxCreateComputeKernel(gfx_, ssgi_program_, "Main", defines.data(), (uint32_t)defines.size());
    }

    // Debug kernels
    debug_occlusion_program_   = gfxCreateProgram(gfx_, "ssgi_debug", shader_path.c_str());
    debug_occlusion_kernel_    = gfxCreateComputeKernel(gfx_, debug_occlusion_program_, "DebugOcclusion");
    debug_bent_normal_program_ = gfxCreateProgram(gfx_, "ssgi_debug", shader_path.c_str());
    debug_bent_normal_kernel_  = gfxCreateComputeKernel(gfx_, debug_bent_normal_program_, "DebugBentNormal");
}

void SSGI::destroyStaticResources()
{
    gfxDestroySamplerState(gfx_, point_sampler_);
}

void SSGI::destroyBuffers() {}

void SSGI::destroyKernels()
{
    // Kernels
    gfxDestroyProgram(gfx_, ssgi_program_);
    gfxDestroyKernel(gfx_, main_kernel_);
    gfxDestroyKernel(gfx_, main_unrolled_kernel_);

    // Debug kernels
    gfxDestroyProgram(gfx_, debug_occlusion_program_);
    gfxDestroyKernel(gfx_, debug_occlusion_kernel_);
    gfxDestroyProgram(gfx_, debug_bent_normal_program_);
    gfxDestroyKernel(gfx_, debug_bent_normal_kernel_);
}
} // namespace Capsaicin
