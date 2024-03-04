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

#include "visibility_buffer.h"

#include "../../geometry/path_tracing_shared.h"
#include "capsaicin_internal.h"
#include "components/blue_noise_sampler/blue_noise_sampler.h"
#include "thread_pool.h"

namespace Capsaicin
{
VisibilityBuffer::VisibilityBuffer()
    : RenderTechnique("Visibility buffer")
{}

VisibilityBuffer::~VisibilityBuffer()
{
    terminate();
}

RenderOptionList VisibilityBuffer::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(visibility_buffer_use_rt, options));
    newOptions.emplace(RENDER_OPTION_MAKE(visibility_buffer_use_rt_dxr10, options));
    return newOptions;
}

VisibilityBuffer::RenderOptions VisibilityBuffer::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(visibility_buffer_use_rt, newOptions, options)
    RENDER_OPTION_GET(visibility_buffer_use_rt_dxr10, newOptions, options)
    return newOptions;
}

ComponentList VisibilityBuffer::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(BlueNoiseSampler));
    return components;
}

AOVList VisibilityBuffer::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"Debug", AOV::Write});
    aovs.push_back({"Visibility", AOV::Write, AOV::Clear, DXGI_FORMAT_R32G32B32A32_FLOAT});
    aovs.push_back({"Depth", AOV::ReadWrite});
    aovs.push_back({"VisibilityDepth", AOV::Write, AOV::Clear, DXGI_FORMAT_R32_FLOAT, "PrevVisibilityDepth"});
    aovs.push_back({"GeometryNormal", AOV::Write, AOV::Clear, DXGI_FORMAT_R8G8B8A8_UNORM});
    aovs.push_back({"Velocity", AOV::Write, AOV::Clear, DXGI_FORMAT_R16G16_FLOAT});
    aovs.push_back(
        {"ShadingNormal", AOV::Write, AOV::Flags(AOV::Clear | AOV::Optional), DXGI_FORMAT_R8G8B8A8_UNORM});
    aovs.push_back(
        {"VertexNormal", AOV::Write, AOV::Flags(AOV::Clear | AOV::Optional), DXGI_FORMAT_R8G8B8A8_UNORM});
    aovs.push_back({"Roughness", AOV::Write, AOV::Flags(AOV::Clear | AOV::Optional), DXGI_FORMAT_R16_FLOAT});
    aovs.push_back(
        {"DisocclusionMask", AOV::Write, AOV::Flags(AOV::None | AOV::Optional), DXGI_FORMAT_R8_UNORM});
    return aovs;
}

DebugViewList VisibilityBuffer::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("Velocity");
    views.emplace_back("DXR1.0");
    views.emplace_back("MaterialAlbedo");
    views.emplace_back("MaterialMetallicity");
    views.emplace_back("MaterialRoughness");
    return views;
}

bool VisibilityBuffer::init(CapsaicinInternal const &capsaicin) noexcept
{
    if (capsaicin.hasAOVBuffer("DisocclusionMask"))
    {
        // Initialise disocclusion program
        disocclusion_mask_program_ = gfxCreateProgram(
            gfx_, "render_techniques/visibility_buffer/disocclusion_mask", capsaicin.getShaderPath());
        disocclusion_mask_kernel_ = gfxCreateComputeKernel(gfx_, disocclusion_mask_program_);

        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_DepthBuffer", capsaicin.getAOVBuffer("VisibilityDepth"));
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_GeometryNormalBuffer",
            capsaicin.getAOVBuffer("GeometryNormal"));
        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_VelocityBuffer", capsaicin.getAOVBuffer("Velocity"));
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_PreviousDepthBuffer",
            capsaicin.getAOVBuffer("PrevVisibilityDepth"));

        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_DisocclusionMask",
            capsaicin.getAOVBuffer("DisocclusionMask"));

        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_NearestSampler", capsaicin.getNearestSampler());
    }

    return initKernel(capsaicin);
}

void VisibilityBuffer::render(CapsaicinInternal &capsaicin) noexcept
{
    // Check for option change
    RenderOptions newOptions = convertOptions(capsaicin.getOptions());
    bool          recompile  = options.visibility_buffer_use_rt != newOptions.visibility_buffer_use_rt
                  || (options.visibility_buffer_use_rt
                      && options.visibility_buffer_use_rt_dxr10 != newOptions.visibility_buffer_use_rt_dxr10);

    options = newOptions;
    if (recompile)
    {
        gfxDestroyProgram(gfx_, visibility_buffer_program_);
        gfxDestroyKernel(gfx_, visibility_buffer_kernel_);
        gfxDestroySbt(gfx_, visibility_buffer_sbt_);
        visibility_buffer_sbt_ = {};

        initKernel(capsaicin);
    }

    auto        blue_noise_sampler = capsaicin.getComponent<BlueNoiseSampler>();
    auto const &camera             = capsaicin.getCameraMatrices(
        capsaicin.hasOption<bool>("taa_enable") && capsaicin.getOption<bool>("taa_enable"));

    if (!options.visibility_buffer_use_rt)
    {
        // Render using raster pass
        uint32_t const instance_count = gfxSceneGetObjectCount<GfxInstance>(capsaicin.getScene());
        GfxBuffer      draw_command_buffer =
            capsaicin.allocateConstantBuffer<D3D12_DRAW_INDEXED_ARGUMENTS>(instance_count);
        D3D12_DRAW_INDEXED_ARGUMENTS *draw_commands =
            (D3D12_DRAW_INDEXED_ARGUMENTS *)gfxBufferGetData(gfx_, draw_command_buffer);

        for (uint32_t i = 0; i < instance_count; ++i)
        {
            uint32_t const  instance_index = capsaicin.getInstanceIdData()[i];
            Instance const &instance       = capsaicin.getInstanceData()[instance_index];
            Mesh const     &mesh           = capsaicin.getMeshData()[instance.mesh_index];

            draw_commands[i].IndexCountPerInstance = mesh.index_count;
            draw_commands[i].InstanceCount         = 1;
            draw_commands[i].StartIndexLocation    = mesh.index_offset_idx;
            draw_commands[i].BaseVertexLocation    = mesh.vertex_offset_idx;
            draw_commands[i].StartInstanceLocation = i; // <- drawID
        }

        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_Eye", capsaicin.getCamera().eye);
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_FrameIndex", capsaicin.getFrameIndex());
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_ViewProjection", camera.view_projection);
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_PrevViewProjection", camera.view_projection_prev);

        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_MeshBuffer", capsaicin.getMeshBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TransformBuffer", capsaicin.getTransformBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_InstanceIDBuffer", capsaicin.getInstanceIdBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_PrevTransformBuffer", capsaicin.getPrevTransformBuffer());

        blue_noise_sampler->addProgramParameters(capsaicin, visibility_buffer_program_);

        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_TextureMaps", capsaicin.getTextures(),
            capsaicin.getTextureCount());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TextureSampler", capsaicin.getAnisotropicSampler());

        gfxCommandBindKernel(gfx_, visibility_buffer_kernel_);
        gfxCommandMultiDrawIndexedIndirect(gfx_, draw_command_buffer, instance_count);

        gfxDestroyBuffer(gfx_, draw_command_buffer);
        gfxCommandCopyTexture(
            gfx_, capsaicin.getAOVBuffer("VisibilityDepth"), capsaicin.getAOVBuffer("Depth"));
    }
    else
    {
        // Render using ray tracing pass
        auto &cam = capsaicin.getCamera();
        auto  cameraData =
            caclulateRayCamera({cam.eye, cam.center, cam.up, cam.aspect, cam.fovY, cam.nearZ, cam.farZ},
                capsaicin.getWidth(), capsaicin.getHeight());
        auto bufferDimensions = uint2(capsaicin.getWidth(), capsaicin.getHeight());

        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_BufferDimensions", bufferDimensions);
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_RayCamera", cameraData);
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_FrameIndex", capsaicin.getFrameIndex());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_MeshBuffer", capsaicin.getMeshBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TransformBuffer", capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_IndexBuffer", capsaicin.getIndexBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_VertexBuffer", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_PrevTransformBuffer", capsaicin.getPrevTransformBuffer());

        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_Scene", capsaicin.getAccelerationStructure());

        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_TextureMaps", capsaicin.getTextures(),
            capsaicin.getTextureCount());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TextureSampler", capsaicin.getLinearWrapSampler());

        GfxBuffer cameraMatrixBuffer = capsaicin.allocateConstantBuffer<float4x4>(1);
        gfxBufferGetData<float4x4>(gfx_, cameraMatrixBuffer)[0] = camera.view_projection;
        GfxBuffer cameraPrevMatrixBuffer = capsaicin.allocateConstantBuffer<float4x4>(1);
        gfxBufferGetData<float4x4>(gfx_, cameraPrevMatrixBuffer)[0] = camera.view_projection_prev;
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_ViewProjection", camera.view_projection);
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_PrevViewProjection", camera.view_projection_prev);
        // Need to correctly jitter ray camera equivalent to raster camera
        float2 jitter = float2(camera.projection[2][0] * capsaicin.getWidth(),
                            camera.projection[2][1] * capsaicin.getHeight())
                      * 0.5f;
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_Jitter", jitter);

        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_Visibility", capsaicin.getAOVBuffer("Visibility"));
        // Write to VisibilityDepth as its not possible to write directly to a depth buffer from a compute
        // shader
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_Depth", capsaicin.getAOVBuffer("VisibilityDepth"));
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_GeometryNormal", capsaicin.getAOVBuffer("GeometryNormal"));
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_Velocity", capsaicin.getAOVBuffer("Velocity"));
        if (capsaicin.hasAOVBuffer("ShadingNormal"))
        {
            gfxProgramSetParameter(
                gfx_, visibility_buffer_program_, "g_ShadingNormal", capsaicin.getAOVBuffer("ShadingNormal"));
        }
        if (capsaicin.hasAOVBuffer("VertexNormal"))
        {
            gfxProgramSetParameter(
                gfx_, visibility_buffer_program_, "g_VertexNormal", capsaicin.getAOVBuffer("VertexNormal"));
        }
        if (capsaicin.hasAOVBuffer("Roughness"))
        {
            gfxProgramSetParameter(
                gfx_, visibility_buffer_program_, "g_Roughness", capsaicin.getAOVBuffer("Roughness"));
        }

        if (options.visibility_buffer_use_rt_dxr10)
        {
            TimedSection const timed_section(*this, "VisibilityBufferRT1.0");

            // Populate shader binding table
            gfxSbtSetShaderGroup(
                gfx_, visibility_buffer_sbt_, kGfxShaderGroupType_Raygen, 0, "VisibilityRTRaygen");
            gfxSbtSetShaderGroup(
                gfx_, visibility_buffer_sbt_, kGfxShaderGroupType_Miss, 0, "VisibilityRTMiss");
            for (uint32_t i = 0; i < gfxAccelerationStructureGetRaytracingPrimitiveCount(
                                     gfx_, capsaicin.getAccelerationStructure());
                 i++)
            {
                gfxSbtSetShaderGroup(gfx_, visibility_buffer_sbt_, kGfxShaderGroupType_Hit,
                    i * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit) + 0, "VisibilityRTHitGroup");
            }

            gfxCommandBindKernel(gfx_, visibility_buffer_kernel_);
            gfxCommandDispatchRays(gfx_, visibility_buffer_sbt_, bufferDimensions.x, bufferDimensions.y, 1);
        }
        else
        {
            TimedSection const timed_section(*this, "VisibilityBufferRT");

            uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, visibility_buffer_kernel_);
            uint32_t const  num_groups_x = (bufferDimensions.x + num_threads[0] - 1) / num_threads[0];
            uint32_t const  num_groups_y = (bufferDimensions.y + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx_, visibility_buffer_kernel_);
            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
        }
        gfxDestroyBuffer(gfx_, cameraMatrixBuffer);
        gfxDestroyBuffer(gfx_, cameraPrevMatrixBuffer);
        // Copy The F32 VisibilityDepth into D32 Depth buffer for later passes
        gfxCommandCopyTexture(
            gfx_, capsaicin.getAOVBuffer("Depth"), capsaicin.getAOVBuffer("VisibilityDepth"));
    }

    if (capsaicin.hasAOVBuffer("DisocclusionMask"))
    {
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_Eye", capsaicin.getCamera().eye);
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_NearFar",
            float2(capsaicin.getCamera().nearZ, capsaicin.getCamera().farZ));
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_TexelSize",
            float2(1.0f / capsaicin.getWidth(), 1.0f / capsaicin.getHeight()));
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_Reprojection", camera.reprojection);
        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_ViewProjectionInverse", camera.inv_view_projection);

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, disocclusion_mask_kernel_);
        uint32_t const  num_groups_x = (capsaicin.getWidth() + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (capsaicin.getHeight() + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, disocclusion_mask_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }

    auto const debugView = capsaicin.getCurrentDebugView();
    if (debugView == "Velocity")
    {
        if (!debug_velocities_program_)
        {
            debug_velocities_program_ = gfxCreateProgram(
                gfx_, "render_techniques/visibility_buffer/debug_velocity", capsaicin.getShaderPath());

            GfxDrawState debug_state;
            gfxDrawStateSetColorTarget(debug_state, 0, capsaicin.getAOVBuffer("Debug"));
            debug_velocities_kernel_ = gfxCreateGraphicsKernel(gfx_, debug_velocities_program_, debug_state);
        }

        const GfxCommandEvent command_event(gfx_, "DrawDebugVelocities");
        gfxProgramSetParameter(
            gfx_, debug_velocities_program_, "VelocityBuffer", capsaicin.getAOVBuffer("Velocity"));
        gfxCommandBindKernel(gfx_, debug_velocities_kernel_);
        gfxCommandDraw(gfx_, 3);
    }
    else if (debugView.starts_with("Material"))
    {
        if (!debug_material_program_)
        {
            debug_material_program_ = gfxCreateProgram(
                gfx_, "render_techniques/visibility_buffer/debug_material", capsaicin.getShaderPath());

            GfxDrawState debug_material_draw_state;
            gfxDrawStateSetColorTarget(debug_material_draw_state, 0, capsaicin.getAOVBuffer("Debug"));
            debug_material_kernel_ = gfxCreateGraphicsKernel(
                gfx_, debug_material_program_, debug_material_draw_state, "DebugMaterial");
        }

        enum class MaterialMode : uint32_t
        {
            ALBEDO = 0,
            METALLICITY,
            ROUGHNESS,
        };
        MaterialMode materialMode = MaterialMode::ALBEDO;
        if (debugView == "MaterialMetallicity")
        {
            materialMode = MaterialMode::METALLICITY;
        }
        else if (debugView == "MaterialRoughness")
        {
            materialMode = MaterialMode::ROUGHNESS;
        }

        const GfxCommandEvent command_event(gfx_, "DebugMaterial");
        gfxProgramSetParameter(gfx_, debug_material_program_, "g_MaterialMode", materialMode);

        gfxProgramSetParameter(
            gfx_, debug_material_program_, "g_VisibilityBuffer", capsaicin.getAOVBuffer("Visibility"));
        gfxProgramSetParameter(
            gfx_, debug_material_program_, "g_DepthBuffer", capsaicin.getAOVBuffer("VisibilityDepth"));

        gfxProgramSetParameter(
            gfx_, debug_material_program_, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, debug_material_program_, "g_MeshBuffer", capsaicin.getMeshBuffer());
        gfxProgramSetParameter(gfx_, debug_material_program_, "g_IndexBuffer", capsaicin.getIndexBuffer());
        gfxProgramSetParameter(gfx_, debug_material_program_, "g_VertexBuffer", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(
            gfx_, debug_material_program_, "g_MaterialBuffer", capsaicin.getMaterialBuffer());

        gfxProgramSetParameter(gfx_, debug_material_program_, "g_TextureMaps", capsaicin.getTextures(),
            capsaicin.getTextureCount());
        gfxProgramSetParameter(
            gfx_, debug_material_program_, "g_TextureSampler", capsaicin.getAnisotropicSampler());
        gfxCommandBindKernel(gfx_, debug_material_kernel_);
        gfxCommandDraw(gfx_, 3);
    }
    else if (debugView == "DXR1.0")
    {
        if (!debug_dxr10_program_)
        {
            debug_dxr10_program_ = gfxCreateProgram(
                gfx_, "render_techniques/visibility_buffer/debug_dxr10", capsaicin.getShaderPath());
            // Associate space1 with local root signature for MyHitGroup
            GfxLocalRootSignatureAssociation local_root_signature_associations[] = {
                {1, kGfxShaderGroupType_Hit, "MyHitGroup"}
            };
            debug_dxr10_kernel_ =
                gfxCreateRaytracingKernel(gfx_, debug_dxr10_program_, local_root_signature_associations, 1);
            uint32_t entry_count[kGfxShaderGroupType_Count] {
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Raygen),
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Miss),
                gfxSceneGetInstanceCount(capsaicin.getScene())
                    * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit),
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Callable)};
            debug_dxr10_sbt_ = gfxCreateSbt(gfx_, &debug_dxr10_kernel_, 1, entry_count);
        }

        const GfxCommandEvent command_event(gfx_, "DrawDebugDXR1.0");
        gfxProgramSetParameter(gfx_, debug_dxr10_program_, "g_Eye", capsaicin.getCamera().eye);
        gfxProgramSetParameter(
            gfx_, debug_dxr10_program_, "g_ViewProjectionInverse", camera.inv_view_projection);
        gfxProgramSetParameter(gfx_, debug_dxr10_program_, "g_Scene", capsaicin.getAccelerationStructure());
        gfxProgramSetParameter(gfx_, debug_dxr10_program_, "g_RenderTarget", capsaicin.getAOVBuffer("Debug"));
        // Populate shader binding table
        gfxSbtSetShaderGroup(gfx_, debug_dxr10_sbt_, kGfxShaderGroupType_Raygen, 0, "MyRaygenShader");
        gfxSbtSetShaderGroup(gfx_, debug_dxr10_sbt_, kGfxShaderGroupType_Miss, 0, "MyMissShader");
        for (uint32_t i = 0; i < gfxAccelerationStructureGetRaytracingPrimitiveCount(
                                 gfx_, capsaicin.getAccelerationStructure());
             i++)
        {
            gfxSbtSetShaderGroup(gfx_, debug_dxr10_sbt_, kGfxShaderGroupType_Hit,
                i * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit),
                i % 2 == 0 ? "MyHitGroup" : "MyHitGroup2");
            // Populate local root signature parameters
            glm::vec4 test_data = i % 4 == 0 ? glm::vec4(0, 0, 1, 0) : glm::vec4(0, 1, 0, 0);
            gfxSbtSetConstants(gfx_, debug_dxr10_sbt_, kGfxShaderGroupType_Hit,
                i * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit), "g_MyCB",
                (void const *)&test_data, sizeof(test_data));
        }
        gfxCommandBindKernel(gfx_, debug_dxr10_kernel_);
        gfxCommandDispatchRays(gfx_, debug_dxr10_sbt_, capsaicin.getWidth(), capsaicin.getHeight(), 1);
    }
}

void VisibilityBuffer::terminate() noexcept
{
    gfxDestroyKernel(gfx_, disocclusion_mask_kernel_);
    gfxDestroyProgram(gfx_, disocclusion_mask_program_);

    gfxDestroyKernel(gfx_, visibility_buffer_kernel_);
    gfxDestroyProgram(gfx_, visibility_buffer_program_);
    gfxDestroySbt(gfx_, visibility_buffer_sbt_);
    visibility_buffer_sbt_ = {};

    gfxDestroyKernel(gfx_, debug_velocities_kernel_);
    gfxDestroyProgram(gfx_, debug_velocities_program_);
    debug_velocities_program_ = {};

    gfxDestroyKernel(gfx_, debug_material_kernel_);
    gfxDestroyProgram(gfx_, debug_material_program_);
    debug_material_kernel_ = {};

    gfxDestroyProgram(gfx_, debug_dxr10_program_);
    gfxDestroyKernel(gfx_, debug_dxr10_kernel_);
    debug_dxr10_program_ = {};
    gfxDestroySbt(gfx_, debug_dxr10_sbt_);
}

bool VisibilityBuffer::initKernel(CapsaicinInternal const &capsaicin) noexcept
{
    if (!options.visibility_buffer_use_rt)
    {
        // Initialise the raster variant of visibility buffer kernel
        GfxDrawState visibility_buffer_draw_state = {};
        gfxDrawStateSetCullMode(visibility_buffer_draw_state, D3D12_CULL_MODE_NONE);

        gfxDrawStateSetColorTarget(visibility_buffer_draw_state, 0, capsaicin.getAOVBuffer("Visibility"));
        gfxDrawStateSetColorTarget(visibility_buffer_draw_state, 1, capsaicin.getAOVBuffer("GeometryNormal"));
        gfxDrawStateSetColorTarget(visibility_buffer_draw_state, 2, capsaicin.getAOVBuffer("Velocity"));
        std::vector<char const *> defines;
        if (capsaicin.hasAOVBuffer("ShadingNormal"))
        {
            gfxDrawStateSetColorTarget(
                visibility_buffer_draw_state, 3, capsaicin.getAOVBuffer("ShadingNormal"));
            defines.push_back("HAS_SHADING_NORMAL");
        }
        if (capsaicin.hasAOVBuffer("VertexNormal"))
        {
            gfxDrawStateSetColorTarget(
                visibility_buffer_draw_state, 4, capsaicin.getAOVBuffer("VertexNormal"));
            defines.push_back("HAS_VERTEX_NORMAL");
        }
        if (capsaicin.hasAOVBuffer("Roughness"))
        {
            gfxDrawStateSetColorTarget(visibility_buffer_draw_state, 5, capsaicin.getAOVBuffer("Roughness"));
            defines.push_back("HAS_ROUGHNESS");
        }
        gfxDrawStateSetDepthStencilTarget(visibility_buffer_draw_state, capsaicin.getAOVBuffer("Depth"));

        visibility_buffer_program_ = gfxCreateProgram(
            gfx_, "render_techniques/visibility_buffer/visibility_buffer", capsaicin.getShaderPath());
        visibility_buffer_kernel_ = gfxCreateGraphicsKernel(gfx_, visibility_buffer_program_,
            visibility_buffer_draw_state, nullptr, defines.data(), (uint32_t)defines.size());
    }
    else
    {
        // Initialise the ray tracing variant of visibility buffer kernel
        std::vector<char const *> defines;
        defines.push_back("HAS_RT");
        if (capsaicin.hasAOVBuffer("ShadingNormal"))
        {
            defines.push_back("HAS_SHADING_NORMAL");
        }
        if (capsaicin.hasAOVBuffer("VertexNormal"))
        {
            defines.push_back("HAS_VERTEX_NORMAL");
        }
        if (capsaicin.hasAOVBuffer("Roughness"))
        {
            defines.push_back("HAS_ROUGHNESS");
        }
        visibility_buffer_program_ = gfxCreateProgram(
            gfx_, "render_techniques/visibility_buffer/visibility_buffer_rt", capsaicin.getShaderPath());
        if (options.visibility_buffer_use_rt_dxr10)
        {
            std::vector<char const *> exports = {
                "VisibilityRTRaygen", "VisibilityRTMiss", "VisibilityRTAnyHit", "VisibilityRTClosestHit"};
            std::vector<char const *> subobjects = {
                "VisibilityShaderConfig", "VisibilityPipelineConfig", "VisibilityRTHitGroup"};
            visibility_buffer_kernel_ = gfxCreateRaytracingKernel(gfx_, visibility_buffer_program_, nullptr,
                0, exports.data(), (uint32_t)exports.size(), subobjects.data(), (uint32_t)subobjects.size(),
                defines.data(), (uint32_t)defines.size());

            uint32_t entry_count[kGfxShaderGroupType_Count] {
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Raygen),
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Miss),
                gfxSceneGetInstanceCount(capsaicin.getScene())
                    * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit),
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Callable)};
            std::vector<GfxKernel> sbt_kernels = {visibility_buffer_kernel_};
            visibility_buffer_sbt_ =
                gfxCreateSbt(gfx_, sbt_kernels.data(), (uint32_t)sbt_kernels.size(), entry_count);
        }
        else
        {
            defines.push_back("USE_INLINE_RT");
            visibility_buffer_kernel_ = gfxCreateComputeKernel(gfx_, visibility_buffer_program_,
                "VisibilityBufferRT", defines.data(), (uint32_t)defines.size());
        }
    }

    return !!visibility_buffer_program_;
}
} // namespace Capsaicin
