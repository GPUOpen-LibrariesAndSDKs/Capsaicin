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

#include "reference_path_tracer.h"

#include "capsaicin_internal.h"
#include "components/light_builder/light_builder.h"
#include "components/light_sampler/light_sampler_switcher.h"
#include "components/stratified_sampler/stratified_sampler.h"

auto const *kReferencePTRaygenShaderName       = "ReferencePTRaygen";
auto const *kReferencePTMissShaderName         = "ReferencePTMiss";
auto const *kReferencePTShadowMissShaderName   = "ReferencePTShadowMiss";
auto const *kReferencePTAnyHitShaderName       = "ReferencePTAnyHit";
auto const *kReferencePTShadowAnyHitShaderName = "ReferencePTShadowAnyHit";
auto const *kReferencePTClosestHitShaderName   = "ReferencePTClosestHit";
auto const *kReferencePTHitGroupName           = "ReferencePTHitGroup";
auto const *kReferencePTShadowHitGroupName     = "ReferencePTShadowHitGroup";

namespace Capsaicin
{
ReferencePT::ReferencePT()
    : RenderTechnique("Reference Path Tracer")
{}

ReferencePT::~ReferencePT()
{
    ReferencePT::terminate();
}

RenderOptionList ReferencePT::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_bounce_count, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_min_rr_bounces, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_sample_count, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_disable_albedo_materials, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_disable_direct_lighting, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_disable_specular_materials, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_disable_alpha_testing, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_nee_only, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_disable_nee, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_use_dxr10, options));
    newOptions.emplace(RENDER_OPTION_MAKE(reference_pt_accumulate, options));
    return newOptions;
}

ReferencePT::RenderOptions ReferencePT::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(reference_pt_bounce_count, newOptions, options)
    RENDER_OPTION_GET(reference_pt_min_rr_bounces, newOptions, options)
    RENDER_OPTION_GET(reference_pt_sample_count, newOptions, options)
    RENDER_OPTION_GET(reference_pt_disable_albedo_materials, newOptions, options)
    RENDER_OPTION_GET(reference_pt_disable_direct_lighting, newOptions, options)
    RENDER_OPTION_GET(reference_pt_disable_specular_materials, newOptions, options)
    RENDER_OPTION_GET(reference_pt_disable_alpha_testing, newOptions, options)
    RENDER_OPTION_GET(reference_pt_nee_only, newOptions, options)
    RENDER_OPTION_GET(reference_pt_disable_nee, newOptions, options)
    RENDER_OPTION_GET(reference_pt_use_dxr10, newOptions, options)
    RENDER_OPTION_GET(reference_pt_accumulate, newOptions, options)
    return newOptions;
}

ComponentList ReferencePT::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(LightSamplerSwitcher));
    components.emplace_back(COMPONENT_MAKE(StratifiedSampler));
    return components;
}

SharedTextureList ReferencePT::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::Write});
    return textures;
}

bool ReferencePT::init(CapsaicinInternal const &capsaicin) noexcept
{
    rayCameraData = gfxCreateBuffer<RayCamera>(gfx_, 1, nullptr, kGfxCpuAccess_Write);
    rayCameraData.setName("Capsaicin_PT_RayCamera");
    accumulationBuffer =
        capsaicin.createRenderTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, "PT_AccumulationBuffer");

    reference_pt_program_ = capsaicin.createProgram(getProgramName());
    return initKernels(capsaicin);
}

void ReferencePT::render(CapsaicinInternal &capsaicin) noexcept
{
    RenderOptions const newOptions         = convertOptions(capsaicin.getOptions());
    auto const          lightSampler       = capsaicin.getComponent<LightSamplerSwitcher>();
    auto const          lightBuilder       = capsaicin.getComponent<LightBuilder>();
    auto const          stratified_sampler = capsaicin.getComponent<StratifiedSampler>();

    // Check if options change requires kernel recompile
    bool const recompile = needsRecompile(capsaicin, newOptions);

    // Check if we can continue to accumulate samples
    auto const renderDimensions = capsaicin.getRenderDimensions();
    bool const accumulate = options.reference_pt_accumulate && !recompile && !capsaicin.getAnimationUpdated()
                         && !capsaicin.getRenderDimensionsUpdated() && !capsaicin.getCameraUpdated()
                         && options.reference_pt_bounce_count == newOptions.reference_pt_bounce_count
                         && options.reference_pt_min_rr_bounces == newOptions.reference_pt_min_rr_bounces
                         && !capsaicin.getMeshesUpdated() && !capsaicin.getTransformsUpdated()
                         && !lightBuilder->getLightsUpdated()
                         && !lightSampler->getLightSettingsUpdated(capsaicin)
                         && capsaicin.getFrameIndex() > 0;

    // Update the history
    options = newOptions;

    if (!accumulate)
    {
        auto const camera = capsaicin.getCamera();
        cameraData        = caclulateRayCamera(
            {camera.eye, camera.center, camera.up, camera.aspect, camera.fovY, camera.nearZ, camera.farZ},
            renderDimensions);
    }

    if (recompile)
    {
        gfxDestroyKernel(gfx_, reference_pt_kernel_);
        gfxDestroySbt(gfx_, reference_pt_sbt_);
        initKernels(capsaicin);
    }

    if (capsaicin.getRenderDimensionsUpdated())
    {
        accumulationBuffer = capsaicin.resizeRenderTexture(accumulationBuffer, false);
    }

    // Bind the shader parameters
    uint2 const bufferDimensions = capsaicin.getRenderDimensions();
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
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_TransformBuffer", capsaicin.getTransformBuffer());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_IndexBuffer", capsaicin.getIndexBuffer());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_VertexBuffer", capsaicin.getVertexBuffer());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_VertexDataIndex", capsaicin.getVertexDataIndex());
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_MaterialBuffer", capsaicin.getMaterialBuffer());

    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_AccumulationBuffer", accumulationBuffer);
    gfxProgramSetParameter(
        gfx_, reference_pt_program_, "g_OutputBuffer", capsaicin.getSharedTexture("Color"));

    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_Scene", capsaicin.getAccelerationStructure());

    gfxProgramSetParameter(
        gfx_, reference_pt_program_, "g_EnvironmentBuffer", capsaicin.getEnvironmentBuffer());
    auto const &textures = capsaicin.getTextures();
    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_TextureMaps", textures.data(),
        static_cast<uint32_t>(textures.size()));

    gfxProgramSetParameter(gfx_, reference_pt_program_, "g_TextureSampler", capsaicin.getLinearWrapSampler());

    lightSampler->addProgramParameters(capsaicin, reference_pt_program_);

    // Render a reference for the current scene
    if (options.reference_pt_use_dxr10)
    {
        setupSbt(capsaicin);
        gfxCommandBindKernel(gfx_, reference_pt_kernel_);
        gfxCommandDispatchRays(gfx_, reference_pt_sbt_, bufferDimensions.x, bufferDimensions.y, 1);
    }
    else
    {
        TimedSection const timed_section(*this, "ReferencePT");

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, reference_pt_kernel_);
        uint32_t const  num_groups_x = (bufferDimensions.x + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (bufferDimensions.y + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, reference_pt_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }
}

void ReferencePT::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, rayCameraData);
    rayCameraData = {};
    gfxDestroyTexture(gfx_, accumulationBuffer);
    accumulationBuffer = {};

    gfxDestroyProgram(gfx_, reference_pt_program_);
    reference_pt_program_ = {};
    gfxDestroyKernel(gfx_, reference_pt_kernel_);
    reference_pt_kernel_ = {};
    gfxDestroySbt(gfx_, reference_pt_sbt_);
    reference_pt_sbt_ = {};
}

void ReferencePT::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    ImGui::DragInt("Samples Per Pixel",
        reinterpret_cast<int32_t *>(&capsaicin.getOption<uint32_t>("reference_pt_sample_count")), 1, 1, 30);
    auto &bounces = capsaicin.getOption<uint32_t>("reference_pt_bounce_count");
    ImGui::DragInt("Bounces", reinterpret_cast<int32_t *>(&bounces), 1, 0, 30);
    auto &minBounces = capsaicin.getOption<uint32_t>("reference_pt_min_rr_bounces");
    ImGui::DragInt(
        "Min Bounces", reinterpret_cast<int32_t *>(&minBounces), 1, 0, static_cast<int32_t>(bounces));
    minBounces = glm::min(minBounces, bounces);
    ImGui::Checkbox(
        "Disable Albedo Textures", &capsaicin.getOption<bool>("reference_pt_disable_albedo_materials"));
    ImGui::Checkbox(
        "Disable Direct Lighting", &capsaicin.getOption<bool>("reference_pt_disable_direct_lighting"));
    ImGui::Checkbox("NEE Only", &capsaicin.getOption<bool>("reference_pt_nee_only"));
    ImGui::Checkbox("Disable NEE", &capsaicin.getOption<bool>("reference_pt_disable_nee"));
    ImGui::Checkbox(
        "Disable Specular Materials", &capsaicin.getOption<bool>("reference_pt_disable_specular_materials"));
    ImGui::Checkbox(
        "Disable Alpha Testing", &capsaicin.getOption<bool>("reference_pt_disable_alpha_testing"));
    ImGui::Checkbox("Enable Accumulation", &capsaicin.getOption<bool>("reference_pt_accumulate"));
}

bool ReferencePT::initKernels(CapsaicinInternal const &capsaicin) noexcept
{
    // Set up the base defines based on available features
    auto const                lightSampler = capsaicin.getComponent<LightSamplerSwitcher>();
    std::vector const         baseDefines(lightSampler->getShaderDefines(capsaicin));
    std::vector<char const *> defines;
    defines.reserve(baseDefines.size());
    for (auto const &i : baseDefines)
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
    if (options.reference_pt_disable_specular_materials)
    {
        defines.push_back("DISABLE_SPECULAR_MATERIALS");
    }
    if (options.reference_pt_disable_alpha_testing)
    {
        defines.push_back("DISABLE_ALPHA_TESTING");
    }
    if (options.reference_pt_nee_only)
    {
        defines.push_back("DISABLE_NON_NEE");
    }
    if (options.reference_pt_disable_nee)
    {
        defines.push_back("DISABLE_NEE");
    }
    if (options.reference_pt_use_dxr10)
    {
        std::vector<char const *>                     exports;
        std::vector<char const *>                     subobjects;
        std::vector<std::string>                      defines_str;
        std::vector<std::string>                      exports_str;
        std::vector<std::string>                      subobjects_str;
        std::vector<GfxLocalRootSignatureAssociation> local_root_signature_associations;
        setupPTKernel(capsaicin, local_root_signature_associations, defines_str, exports_str, subobjects_str);
        for (auto &i : defines_str)
        {
            defines.push_back(i.c_str());
        }
        exports.reserve(exports_str.size());
        for (auto &i : exports_str)
        {
            exports.push_back(i.c_str());
        }
        subobjects.reserve(subobjects_str.size());
        for (auto &i : subobjects_str)
        {
            subobjects.push_back(i.c_str());
        }

        reference_pt_kernel_ = gfxCreateRaytracingKernel(gfx_, reference_pt_program_,
            local_root_signature_associations.data(),
            static_cast<uint32_t>(local_root_signature_associations.size()), exports.data(),
            static_cast<uint32_t>(exports.size()), subobjects.data(),
            static_cast<uint32_t>(subobjects.size()), defines.data(), static_cast<uint32_t>(defines.size()));

        uint32_t entry_count[kGfxShaderGroupType_Count] {
            capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Raygen),
            capsaicin.getSbtStrideInEntries(
                kGfxShaderGroupType_Miss), // two miss shaders for scattered and shadow ray
            gfxSceneGetInstanceCount(capsaicin.getScene())
                * capsaicin.getSbtStrideInEntries(
                    kGfxShaderGroupType_Hit), // two sets of hit groups for scattered and shadow ray
            capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Callable)};
        GfxKernel sbt_kernels[] {reference_pt_kernel_};
        reference_pt_sbt_ = gfxCreateSbt(gfx_, sbt_kernels, ARRAYSIZE(sbt_kernels), entry_count);
    }
    else
    {
        reference_pt_kernel_ = gfxCreateComputeKernel(gfx_, reference_pt_program_, "ReferencePT",
            defines.data(), static_cast<uint32_t>(defines.size()));
        reference_pt_sbt_    = {};
    }
    return !!reference_pt_program_;
}

bool ReferencePT::needsRecompile(
    CapsaicinInternal const &capsaicin, RenderOptions const &newOptions) const noexcept
{
    auto const lightSampler = capsaicin.getComponent<LightSamplerSwitcher>();

    // Check if options change requires kernel recompile
    bool const recompile =
        lightSampler->needsRecompile(capsaicin)
        || options.reference_pt_disable_albedo_materials != newOptions.reference_pt_disable_albedo_materials
        || options.reference_pt_disable_direct_lighting != newOptions.reference_pt_disable_direct_lighting
        || options.reference_pt_disable_specular_materials
               != newOptions.reference_pt_disable_specular_materials
        || options.reference_pt_disable_alpha_testing != newOptions.reference_pt_disable_alpha_testing
        || options.reference_pt_nee_only != newOptions.reference_pt_nee_only
        || options.reference_pt_disable_nee != newOptions.reference_pt_disable_nee
        || options.reference_pt_use_dxr10 != newOptions.reference_pt_use_dxr10;
    return recompile;
}

void ReferencePT::setupSbt(CapsaicinInternal const &capsaicin) const noexcept
{
    // Populate shader binding table
    gfxSbtSetShaderGroup(
        gfx_, reference_pt_sbt_, kGfxShaderGroupType_Raygen, 0, kReferencePTRaygenShaderName);
    gfxSbtSetShaderGroup(gfx_, reference_pt_sbt_, kGfxShaderGroupType_Miss, 0, kReferencePTMissShaderName);
    gfxSbtSetShaderGroup(
        gfx_, reference_pt_sbt_, kGfxShaderGroupType_Miss, 1, kReferencePTShadowMissShaderName);

    for (uint32_t i = 0; i < capsaicin.getRaytracingPrimitiveCount(); i++)
    {
        gfxSbtSetShaderGroup(gfx_, reference_pt_sbt_, kGfxShaderGroupType_Hit,
            i * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit) + 0, kReferencePTHitGroupName);
        gfxSbtSetShaderGroup(gfx_, reference_pt_sbt_, kGfxShaderGroupType_Hit,
            i * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit) + 1, kReferencePTShadowHitGroupName);
    }
}

void ReferencePT::setupPTKernel([[maybe_unused]] CapsaicinInternal const &capsaicin,
    [[maybe_unused]] std::vector<GfxLocalRootSignatureAssociation>       &local_root_signature_associations,
    [[maybe_unused]] std::vector<std::string> &defines, std::vector<std::string> &exports,
    std::vector<std::string> &subobjects) noexcept
{
    exports.emplace_back(kReferencePTRaygenShaderName);
    exports.emplace_back(kReferencePTMissShaderName);
    exports.emplace_back(kReferencePTShadowMissShaderName);
    exports.emplace_back(kReferencePTAnyHitShaderName);
    exports.emplace_back(kReferencePTShadowAnyHitShaderName);
    exports.emplace_back(kReferencePTClosestHitShaderName);

    subobjects.emplace_back("MyShaderConfig");
    subobjects.emplace_back("MyPipelineConfig");
    subobjects.emplace_back(kReferencePTHitGroupName);
    subobjects.emplace_back(kReferencePTShadowHitGroupName);
}

char const *ReferencePT::getProgramName() noexcept
{
    return "render_techniques/reference_path_tracer/reference_path_tracer";
}
} // namespace Capsaicin
