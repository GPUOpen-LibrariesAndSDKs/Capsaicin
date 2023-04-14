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
#include "reference_pt.h"

#include "capsaicin_internal.h"
#include "components/light_sampler_bounds/light_sampler_bounds.h"
#include "components/stratified_sampler/stratified_sampler.h"

namespace Capsaicin
{
RayCamera caclulateRayCamera(CapsaicinInternal const &capsaicin)
{
    float3 origin = capsaicin.getCamera().eye;
    float2 range  = float2(capsaicin.getCamera().nearZ, capsaicin.getCamera().farZ);

    // Get the size of the screen in the X and Y screen direction
    float size = tan(capsaicin.getCamera().fovY / 2.0f);
    size *= range.x;
    float sizeHalfX = size * capsaicin.getCamera().aspect;
    float sizeHalfY = size;

    // Generate view direction
    float3 forward(capsaicin.getCamera().center - origin);
    forward = normalize(forward);
    // Generate proper horizontal direction
    float3 right(cross(forward, capsaicin.getCamera().up));
    right = normalize(right);
    // Generate proper up direction
    float3 down(cross(forward, right));
    // Normalize vectors
    down = normalize(down);

    // Set each of the camera vectors to an orthonormal basis
    float3 directionX = right;
    float3 directionY = down;
    float3 directionZ = forward;

    // Get weighted distance vector
    directionZ = directionZ * range.x;

    // Get the Scaled Horizontal and up vectors
    directionX *= sizeHalfX;
    directionY *= sizeHalfY;

    // Offset the direction vector
    float3 directionTL = directionZ - directionX - directionY;

    // Scale the direction X and Y vectors from half size
    directionX += directionX;
    directionY += directionY;

    // Scale the X and Y vectors to be pixel length
    directionX /= (float)capsaicin.getWidth();
    directionY /= (float)capsaicin.getHeight();

    return {origin, directionTL, directionX, directionY, range};
}

ReferencePT::ReferencePT()
    : RenderTechnique("Reference PT")
{}

ReferencePT::~ReferencePT()
{
    terminate();
}

RenderOptionList ReferencePT::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_bounce_count, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_min_rr_bounces, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_sample_count, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_disable_albedo_materials, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_disable_direct_lighting, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_disable_specular_lighting, options));
    return newOptions;
}

ReferencePT::RenderOptions ReferencePT::convertOptions(RenderSettings const &settings) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(reference_pt_bounce_count, newOptions, settings.options_)
    RENDER_OPTION_GET(reference_pt_min_rr_bounces, newOptions, settings.options_)
    RENDER_OPTION_GET(reference_pt_sample_count, newOptions, settings.options_)
    RENDER_OPTION_GET(reference_pt_disable_albedo_materials, newOptions, settings.options_)
    RENDER_OPTION_GET(reference_pt_disable_direct_lighting, newOptions, settings.options_)
    RENDER_OPTION_GET(reference_pt_disable_specular_lighting, newOptions, settings.options_)
    return newOptions;
}

ComponentList ReferencePT::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(LightSamplerBounds));
    components.emplace_back(COMPONENT_MAKE(StratifiedSampler));
    return components;
}

AOVList ReferencePT::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"Color", AOV::Write});
    return aovs;
}

bool ReferencePT::init(CapsaicinInternal const &capsaicin) noexcept
{
    rayCameraData = gfxCreateBuffer<RayCamera>(gfx_, 1, nullptr, kGfxCpuAccess_Write);
    rayCameraData.setName("Capsaicin_PT_RayCamera");
    accumulationBuffer = gfxCreateTexture2D(gfx_, DXGI_FORMAT_R32G32B32A32_FLOAT);
    accumulationBuffer.setName("Capsaicin_PT_AccumulationBuffer");

    textureSampler = gfxCreateSamplerState(gfx_, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    reference_pt_program_ =
        gfxCreateProgram(gfx_, "render_techniques/path_tracer/reference_pt", capsaicin.getShaderPath());
    return initKernels(capsaicin);
}

void ReferencePT::render(CapsaicinInternal &capsaicin) noexcept
{
    RenderOptions         newOptions         = convertOptions(capsaicin.getRenderSettings());
    RenderSettings const &renderSettings     = capsaicin.getRenderSettings();
    auto                  lightSampler       = capsaicin.getComponent<LightSamplerBounds>();
    auto                  stratified_sampler = capsaicin.getComponent<StratifiedSampler>();

    // Check if options change requires kernel recompile
    bool recompile =
        lightSampler->needsRecompile(capsaicin)
        || options.reference_pt_disable_albedo_materials != newOptions.reference_pt_disable_albedo_materials
        || options.reference_pt_disable_direct_lighting != newOptions.reference_pt_disable_direct_lighting
        || options.reference_pt_disable_specular_lighting
               != newOptions.reference_pt_disable_specular_lighting;

    // Check if we can continue to accumulate samples
    bool const accumulate = !recompile && bufferDimensions.x == capsaicin.getWidth()
                         && bufferDimensions.y == capsaicin.getHeight()
                         && checkCameraUpdated(capsaicin.getCamera())
                         && options.reference_pt_bounce_count == newOptions.reference_pt_bounce_count
                         && options.reference_pt_min_rr_bounces == newOptions.reference_pt_min_rr_bounces
                         && !capsaicin.getMeshesUpdated() && !capsaicin.getTransformsUpdated()
                         && !lightSampler->getLightsUpdated();

    // Update light sampling data structure
    if (capsaicin.getMeshesUpdated() || capsaicin.getTransformsUpdated()
        || bufferDimensions == uint2(0) /*i.e. un-initialised*/)
    {
        // Update the light sampler using scene bounds
        auto sceneBounds = capsaicin.getSceneBounds();
        lightSampler->setBounds(sceneBounds, this);
    }

    lightSampler->update(capsaicin, *this);

    // Update the history
    bufferDimensions = uint2(capsaicin.getWidth(), capsaicin.getHeight());
    camera           = capsaicin.getCamera();
    options          = newOptions;

    if (!accumulate)
    {
        cameraData = caclulateRayCamera(capsaicin);
    }

    if (recompile)
    {
        gfxDestroyKernel(gfx_, reference_pt_kernel_);
        initKernels(capsaicin);
    }

    // Bind the shader parameters
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_BufferDimensions", bufferDimensions);
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_FrameIndex", capsaicin.getFrameIndex());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_RayCamera", cameraData);
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_BounceCount", options.reference_pt_bounce_count);
    gfxProgramSetParameter(
        gfx_, reference_pt_program_, "g_BounceRRCount", options.reference_pt_min_rr_bounces);
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_SampleCount", options.reference_pt_sample_count);
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_Accumulate", accumulate ? 1 : 0);

    stratified_sampler->addProgramParameters(capsaicin, reference_pt_program_);

    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_MeshBuffer", capsaicin.getMeshBuffer());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_TransformBuffer", capsaicin.getTransformBuffer());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_IndexBuffer", capsaicin.getIndexBuffer());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_VertexBuffer", capsaicin.getVertexBuffer());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_MaterialBuffer", capsaicin.getMaterialBuffer());

    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_AccumulationBuffer", accumulationBuffer);
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_OutputBuffer", capsaicin.getAOVBuffer("Color"));

    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_Scene", capsaicin.getAccelerationStructure());

    gfxProgramSetParameter(
        gfx_, reference_pt_program_, "g_EnvironmentBuffer", capsaicin.getEnvironmentBuffer());
    gfxProgramSetParameter(
        gfx_, reference_pt_program_, "g_TextureMaps", capsaicin.getTextures(), capsaicin.getTextureCount());

    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_TextureSampler", textureSampler);

    lightSampler->addProgramParameters(capsaicin, reference_pt_program_);

    // Render a reference for the current scene
    {
        TimedSection const timed_section(*this, "ReferencePT");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, reference_pt_kernel_);
        uint32_t const  num_groups_x = (bufferDimensions.x + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (bufferDimensions.y + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, reference_pt_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }
}

bool ReferencePT::initKernels(CapsaicinInternal const &capsaicin) noexcept
{
    // Set up the base defines based on available features
    auto                      lightSampler = capsaicin.getComponent<LightSamplerBounds>();
    std::vector<std::string>  baseDefines(std::move(lightSampler->getShaderDefines(capsaicin)));
    std::vector<char const *> defines;
    for (auto &i : baseDefines)
    {
        defines.push_back(i.c_str());
    }
    if (options.reference_pt_disable_albedo_materials)
    {
        defines.push_back("DISABLE_ALBEDO_MATERIAL");
    }
    if (options.reference_pt_disable_direct_lighting)
    {
        defines.push_back("DISABLE_DIRECT_LIGHTING");
    }
    if (options.reference_pt_disable_specular_lighting)
    {
        defines.push_back("DISABLE_SPECULAR_LIGHTING");
    }
    reference_pt_kernel_ = gfxCreateComputeKernel(
        gfx_, reference_pt_program_, "ReferencePT", defines.data(), (uint32_t)defines.size());
    return !!reference_pt_program_;
}

void ReferencePT::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, rayCameraData);
    gfxDestroyTexture(gfx_, accumulationBuffer);

    gfxDestroySamplerState(gfx_, textureSampler);
    gfxDestroyProgram(gfx_, reference_pt_program_);
    gfxDestroyKernel(gfx_, reference_pt_kernel_);
}

bool ReferencePT::checkCameraUpdated(GfxCamera const &currentCamera) noexcept
{
    return camera.aspect == currentCamera.aspect && camera.center == currentCamera.center
        && camera.eye == currentCamera.eye && camera.farZ == currentCamera.farZ
        && camera.fovY == currentCamera.fovY && camera.nearZ == currentCamera.nearZ
        && camera.type == currentCamera.type && camera.up == currentCamera.up;
}
} // namespace Capsaicin
