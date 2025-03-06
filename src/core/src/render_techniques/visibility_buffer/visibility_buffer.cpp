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
#include "visibility_buffer_shared.h"

namespace Capsaicin
{
VisibilityBuffer::VisibilityBuffer()
    : RenderTechnique("Visibility buffer")
{}

VisibilityBuffer::~VisibilityBuffer()
{
    VisibilityBuffer::terminate();
}

RenderOptionList VisibilityBuffer::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(visibility_buffer_disable_alpha_testing, options));
    newOptions.emplace(RENDER_OPTION_MAKE(visibility_buffer_use_rt, options));
    newOptions.emplace(RENDER_OPTION_MAKE(visibility_buffer_use_rt_dxr10, options));
    newOptions.emplace(RENDER_OPTION_MAKE(visibility_buffer_enable_hzb, options));
    return newOptions;
}

VisibilityBuffer::RenderOptions VisibilityBuffer::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(visibility_buffer_disable_alpha_testing, newOptions, options)
    RENDER_OPTION_GET(visibility_buffer_use_rt, newOptions, options)
    RENDER_OPTION_GET(visibility_buffer_use_rt_dxr10, newOptions, options)
    RENDER_OPTION_GET(visibility_buffer_enable_hzb, newOptions, options)
    return newOptions;
}

ComponentList VisibilityBuffer::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(BlueNoiseSampler));
    return components;
}

SharedBufferList VisibilityBuffer::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    buffers.push_back({"Meshlets", SharedBuffer::Access::Read});
    buffers.push_back({"MeshletPack", SharedBuffer::Access::Read});
    buffers.push_back({"MeshletCull", SharedBuffer::Access::Read});
    return buffers;
}

SharedTextureList VisibilityBuffer::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Debug", SharedTexture::Access::Write});
    textures.push_back({"Visibility", SharedTexture::Access::Write, SharedTexture::Flags::Clear,
        DXGI_FORMAT_R32G32B32A32_FLOAT});
    textures.push_back({"Depth", SharedTexture::Access::ReadWrite});
    textures.push_back({.name = "VisibilityDepth",
        .access               = SharedTexture::Access::Write,
        .flags                = SharedTexture::Flags::None,
        .format               = DXGI_FORMAT_R32_FLOAT,
        .backup_name          = "PrevVisibilityDepth"});
    textures.push_back({"GeometryNormal", SharedTexture::Access::Write, SharedTexture::Flags::Clear,
        DXGI_FORMAT_R8G8B8A8_UNORM});
    textures.push_back(
        {"Velocity", SharedTexture::Access::Write, SharedTexture::Flags::Clear, DXGI_FORMAT_R16G16_FLOAT});
    textures.push_back({"ShadingNormal", SharedTexture::Access::Write,
        (SharedTexture::Flags::Clear | SharedTexture::Flags::Optional), DXGI_FORMAT_R8G8B8A8_UNORM});
    textures.push_back({"VertexNormal", SharedTexture::Access::Write,
        (SharedTexture::Flags::Clear | SharedTexture::Flags::Optional), DXGI_FORMAT_R8G8B8A8_UNORM});
    textures.push_back({"Roughness", SharedTexture::Access::Write,
        (SharedTexture::Flags::Clear | SharedTexture::Flags::Optional), DXGI_FORMAT_R16_FLOAT});
    textures.push_back({"Gradients", SharedTexture::Access::Write,
        (SharedTexture::Flags::Clear | SharedTexture::Flags::Optional), DXGI_FORMAT_R16G16B16A16_FLOAT});
    textures.push_back({"DisocclusionMask", SharedTexture::Access::Write,
        (SharedTexture::Flags::None | SharedTexture::Flags::Optional), DXGI_FORMAT_R8_UNORM});
    return textures;
}

DebugViewList VisibilityBuffer::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("Meshlets");
    views.emplace_back("Wireframe");
    views.emplace_back("Velocity");
    views.emplace_back("DXR1.0");
    views.emplace_back("MaterialAlbedo");
    views.emplace_back("MaterialMetallicity");
    views.emplace_back("MaterialRoughness");
    return views;
}

bool VisibilityBuffer::init(CapsaicinInternal const &capsaicin) noexcept
{
    if (capsaicin.hasSharedTexture("DisocclusionMask"))
    {
        // Initialise disocclusion program
        disocclusion_mask_program_ =
            capsaicin.createProgram("render_techniques/visibility_buffer/disocclusion_mask");
        disocclusion_mask_kernel_ = gfxCreateComputeKernel(gfx_, disocclusion_mask_program_);
    }

    return initKernel(capsaicin);
}

void VisibilityBuffer::render(CapsaicinInternal &capsaicin) noexcept
{
    // Check for option change
    RenderOptions newOptions = convertOptions(capsaicin.getOptions());
    auto const    debugView  = capsaicin.getCurrentDebugView();
    if (debugView == "Wireframe")
    {
        newOptions.visibility_buffer_disable_alpha_testing = true;
    }
    bool const recompile =
        options.visibility_buffer_use_rt != newOptions.visibility_buffer_use_rt
        || (options.visibility_buffer_use_rt
            && options.visibility_buffer_use_rt_dxr10 != newOptions.visibility_buffer_use_rt_dxr10)
        || options.visibility_buffer_disable_alpha_testing
               != newOptions.visibility_buffer_disable_alpha_testing
        || (!options.visibility_buffer_use_rt
            && options.visibility_buffer_enable_hzb != newOptions.visibility_buffer_enable_hzb);

    options = newOptions;
    if (recompile)
    {
        gfxDestroyProgram(gfx_, visibility_buffer_program_);
        gfxDestroyKernel(gfx_, visibility_buffer_kernel_);
        gfxDestroySbt(gfx_, visibility_buffer_sbt_);
        visibility_buffer_sbt_ = {};

        initKernel(capsaicin);

        gfxDestroyBuffer(gfx_, constants_buffer);
        constants_buffer = {};
    }

    auto        blue_noise_sampler = capsaicin.getComponent<BlueNoiseSampler>(); // Used for stochastic alpha
    auto const &cameraMatrices     = capsaicin.getCameraMatrices(
        capsaicin.hasOption<bool>("taa_enable") && capsaicin.getOption<bool>("taa_enable"));

    if (!options.visibility_buffer_use_rt || debugView == "Meshlets" || debugView == "Wireframe")
    {
        if (!draw_data_buffer || capsaicin.getMeshesUpdated() || capsaicin.getInstancesUpdated())
        {
            std::vector<DrawData> drawData;
            for (auto const &index : capsaicin.getInstanceIdData())
            {
                Instance const &instance = capsaicin.getInstanceData()[index];

                for (uint32_t j = 0; j < instance.meshlet_count; ++j)
                {
                    drawData.emplace_back(instance.meshlet_offset_idx + j, index);
                }
            }
            drawCount = static_cast<uint32_t>(drawData.size());
            gfxDestroyBuffer(gfx_, draw_data_buffer);
            draw_data_buffer = gfxCreateBuffer<DrawData>(gfx_, drawCount, drawData.data());
        }

        {
            DrawConstants constants;
            auto const   &vp           = transpose(cameraMatrices.view_projection);
            constants.cameraFrustum[0] = (vp[3] + vp[0]); // left
            constants.cameraFrustum[0] /= length(float3(constants.cameraFrustum[0]));
            constants.cameraFrustum[1] = (vp[3] - vp[0]); // right
            constants.cameraFrustum[1] /= length(float3(constants.cameraFrustum[1]));
            constants.cameraFrustum[2] = (vp[3] + vp[1]); // bottom
            constants.cameraFrustum[2] /= length(float3(constants.cameraFrustum[2]));
            constants.cameraFrustum[3] = (vp[3] - vp[1]); // top
            constants.cameraFrustum[3] /= length(float3(constants.cameraFrustum[3]));
            constants.cameraFrustum[4] = (vp[3] + vp[2]); // near
            constants.cameraFrustum[4] /= length(float3(constants.cameraFrustum[4]));
            constants.cameraFrustum[5] = (vp[3] - vp[2]); // far
            constants.cameraFrustum[5] /= length(float3(constants.cameraFrustum[5]));
            auto const &camera           = capsaicin.getCamera();
            constants.cameraPosition     = camera.eye;
            constants.drawCount          = drawCount;
            constants.viewProjection     = cameraMatrices.view_projection;
            constants.prevViewProjection = cameraMatrices.view_projection_prev;
            constants.nearZ              = camera.nearZ;
            constants.dimensions         = capsaicin.getRenderDimensions();
            constants.projection0011 =
                float2(cameraMatrices.projection[0][0], cameraMatrices.projection[1][1]);
            constants.view = cameraMatrices.view;
            gfxDestroyBuffer(gfx_, constants_buffer);
            constants_buffer = gfxCreateBuffer<DrawConstants>(gfx_, 1, &constants);
        }
    }

    if (!options.visibility_buffer_use_rt)
    {
        // Render using raster pass
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_VBConstants", constants_buffer);
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_DrawDataBuffer", draw_data_buffer);
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_FrameIndex", capsaicin.getFrameIndex());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_RenderScale", capsaicin.getRenderDimensionsScale());

        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_MeshletCullBuffer",
            capsaicin.getSharedBuffer("MeshletCull"));
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TransformBuffer", capsaicin.getTransformBuffer());

        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_MeshletBuffer", capsaicin.getSharedBuffer("Meshlets"));
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_MeshletPackBuffer",
            capsaicin.getSharedBuffer("MeshletPack"));
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_VertexBuffer", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_VertexDataIndex", capsaicin.getVertexDataIndex());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_PrevVertexDataIndex", capsaicin.getPrevVertexDataIndex());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TransformBuffer", capsaicin.getTransformBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_PrevTransformBuffer", capsaicin.getPrevTransformBuffer());

        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
        blue_noise_sampler->addProgramParameters(capsaicin, visibility_buffer_program_);
        auto const &textures = capsaicin.getTextures();
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_TextureMaps", textures.data(),
            static_cast<uint32_t>(textures.size()));
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TextureSampler", capsaicin.getAnisotropicSampler());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_LinearSampler", capsaicin.getLinearWrapSampler());

        gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("Visibility"));
        gfxCommandBindColorTarget(gfx_, 1, capsaicin.getSharedTexture("GeometryNormal"));
        gfxCommandBindColorTarget(gfx_, 2, capsaicin.getSharedTexture("Velocity"));
        if (capsaicin.hasSharedTexture("ShadingNormal"))
        {
            gfxCommandBindColorTarget(gfx_, 3, capsaicin.getSharedTexture("ShadingNormal"));
        }
        if (capsaicin.hasSharedTexture("VertexNormal"))
        {
            gfxCommandBindColorTarget(gfx_, 4, capsaicin.getSharedTexture("VertexNormal"));
        }
        if (capsaicin.hasSharedTexture("Roughness"))
        {
            gfxCommandBindColorTarget(gfx_, 5, capsaicin.getSharedTexture("Roughness"));
        }
        if (capsaicin.hasSharedTexture("Gradients"))
        {
            gfxCommandBindColorTarget(gfx_, 6, capsaicin.getSharedTexture("Gradients"));
        }
        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));

        if (options.visibility_buffer_enable_hzb)
        {
            if (auto packedDrawSize = glm::max(drawCount >> 5, 1U);
                !meshlet_visibility_buffer || meshlet_visibility_buffer.getSize() < packedDrawSize)
            {
                gfxDestroyBuffer(gfx_, meshlet_visibility_buffer);
                meshlet_visibility_buffer = gfxCreateBuffer<uint32_t>(gfx_, packedDrawSize);
                gfxCommandClearBuffer(gfx_, meshlet_visibility_buffer, 0);
            }

            gfxProgramSetParameter(
                gfx_, visibility_buffer_program_, "g_MeshletVisibilityHistory", meshlet_visibility_buffer);
            gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_MeshletPreviousVisibilityHistory",
                meshlet_visibility_buffer);

            if (!depth_pyramid)
            {
                // Reduce the number of needed mips by ignoring the last as this is smaller than we need
                // anyway
                auto renderDims = capsaicin.getRenderDimensions();
                auto mips       = std::max(gfxCalculateMipCount(renderDims.x, renderDims.y) - 1, 1U);
                depth_pyramid =
                    capsaicin.createRenderTexture(DXGI_FORMAT_R32_FLOAT, "VisibilityBuffer_HzB", mips);
                depth_pyramid_sampler = gfxCreateSamplerState(gfx_, D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT);
                depth_pyramid_mip.initialise(capsaicin, GPUMip::Type::DepthMin);
            }
            else if (capsaicin.getRenderDimensionsUpdated())
            {
                auto renderDims = capsaicin.getRenderDimensions();
                auto mips       = std::max(gfxCalculateMipCount(renderDims.x, renderDims.y) - 1, 1U);
                depth_pyramid   = capsaicin.resizeRenderTexture(depth_pyramid, false, mips);
            }
            gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_DepthPyramid", depth_pyramid);
            gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_DepthSampler", depth_pyramid_sampler);

            gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_FirstPass", true);
        }

        // Run first pass
        {
            TimedSection const timed_section(*this, "VisibilityBufferPass1");

            uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, visibility_buffer_kernel_);
            uint32_t const  num_groups_x = (drawCount + num_threads[0] - 1) / num_threads[0];

            gfxCommandBindKernel(gfx_, visibility_buffer_kernel_);
            gfxCommandDrawMesh(gfx_, num_groups_x, 1, 1);
        }

        if (options.visibility_buffer_enable_hzb)
        {
            // Create depth pyramid
            {
                TimedSection const timed_section(*this, "VisibilityBufferDepthPyramid");
                gfxCommandCopyTexture(gfx_, depth_pyramid, capsaicin.getSharedTexture("Depth"));
                depth_pyramid_mip.mip(depth_pyramid);
            }

            // Run HzB second pass
            {
                gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_FirstPass", false);

                TimedSection const timed_section(*this, "VisibilityBufferPass2");

                uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, visibility_buffer_kernel_);
                uint32_t const  num_groups_x = (drawCount + num_threads[0] - 1) / num_threads[0];

                gfxCommandBindKernel(gfx_, visibility_buffer_kernel_);
                gfxCommandDrawMesh(gfx_, num_groups_x, 1, 1);
            }
        }

        gfxCommandCopyTexture(
            gfx_, capsaicin.getSharedTexture("VisibilityDepth"), capsaicin.getSharedTexture("Depth"));
    }
    else
    {
        auto        bufferDimensions = capsaicin.getRenderDimensions();
        auto const &cam              = capsaicin.getCamera();
        {
            DrawConstantsRT const constants {cameraMatrices.view_projection,
                cameraMatrices.view_projection_prev, bufferDimensions,
                float2(cameraMatrices.projection[2][0] * static_cast<float>(bufferDimensions.x),
                    cameraMatrices.projection[2][1] * static_cast<float>(bufferDimensions.y))
                    * 0.5F};
            gfxDestroyBuffer(gfx_, constants_buffer);
            constants_buffer = gfxCreateBuffer<DrawConstantsRT>(gfx_, 1, &constants);
        }

        // Render using ray tracing pass
        gfxCommandClearTexture(gfx_, capsaicin.getSharedTexture("VisibilityDepth"));

        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_VBConstants", constants_buffer);
        auto cameraData = caclulateRayCamera(
            {cam.eye, cam.center, cam.up, cam.aspect, cam.fovY, cam.nearZ, cam.farZ}, bufferDimensions);

        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_RayCamera", cameraData);

        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TransformBuffer", capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_IndexBuffer", capsaicin.getIndexBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_VertexBuffer", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_VertexDataIndex", capsaicin.getVertexDataIndex());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_PrevVertexDataIndex", capsaicin.getPrevVertexDataIndex());
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_PreviousTransformBuffer",
            capsaicin.getPrevTransformBuffer());

        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_Scene", capsaicin.getAccelerationStructure());

        auto const &textures = capsaicin.getTextures();
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_TextureMaps", textures.data(),
            static_cast<uint32_t>(textures.size()));
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_TextureSampler", capsaicin.getLinearWrapSampler());

        GfxBuffer const cameraMatrixBuffer = capsaicin.allocateConstantBuffer<float4x4>(1);
        gfxBufferGetData<float4x4>(gfx_, cameraMatrixBuffer)[0] = cameraMatrices.view_projection;
        GfxBuffer const cameraPrevMatrixBuffer = capsaicin.allocateConstantBuffer<float4x4>(1);
        gfxBufferGetData<float4x4>(gfx_, cameraPrevMatrixBuffer)[0] = cameraMatrices.view_projection_prev;
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_ViewProjection", cameraMatrices.view_projection);
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_PrevViewProjection", cameraMatrices.view_projection_prev);

        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_Visibility", capsaicin.getSharedTexture("Visibility"));
        // Write to VisibilityDepth as it's not possible to write directly to a depth buffer from a compute
        // shader
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_Depth", capsaicin.getSharedTexture("VisibilityDepth"));
        gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_GeometryNormal",
            capsaicin.getSharedTexture("GeometryNormal"));
        gfxProgramSetParameter(
            gfx_, visibility_buffer_program_, "g_Velocity", capsaicin.getSharedTexture("Velocity"));
        if (capsaicin.hasSharedTexture("ShadingNormal"))
        {
            gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_ShadingNormal",
                capsaicin.getSharedTexture("ShadingNormal"));
        }
        if (capsaicin.hasSharedTexture("VertexNormal"))
        {
            gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_VertexNormal",
                capsaicin.getSharedTexture("VertexNormal"));
        }
        if (capsaicin.hasSharedTexture("Roughness"))
        {
            gfxProgramSetParameter(
                gfx_, visibility_buffer_program_, "g_Roughness", capsaicin.getSharedTexture("Roughness"));
        }

        if (options.visibility_buffer_use_rt_dxr10)
        {
            TimedSection const timed_section(*this, "VisibilityBufferRT1.0");

            // Populate shader binding table
            gfxSbtSetShaderGroup(
                gfx_, visibility_buffer_sbt_, kGfxShaderGroupType_Raygen, 0, "VisibilityRTRaygen");
            gfxSbtSetShaderGroup(
                gfx_, visibility_buffer_sbt_, kGfxShaderGroupType_Miss, 0, "VisibilityRTMiss");
            for (uint32_t i = 0; i < capsaicin.getRaytracingPrimitiveCount(); i++)
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
            gfx_, capsaicin.getSharedTexture("Depth"), capsaicin.getSharedTexture("VisibilityDepth"));
    }

    if (capsaicin.hasSharedTexture("DisocclusionMask"))
    {
        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_DepthBuffer", capsaicin.getSharedTexture("VisibilityDepth"));
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_GeometryNormalBuffer",
            capsaicin.getSharedTexture("GeometryNormal"));
        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_VelocityBuffer", capsaicin.getSharedTexture("Velocity"));
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_PreviousDepthBuffer",
            capsaicin.getSharedTexture("PrevVisibilityDepth"));

        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_DisocclusionMask",
            capsaicin.getSharedTexture("DisocclusionMask"));

        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_NearestSampler", capsaicin.getNearestSampler());

        auto const &camera = capsaicin.getCamera();
        gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_Eye", camera.eye);
        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_NearFar", float2(camera.nearZ, camera.farZ));
        auto bufferDimensions = capsaicin.getRenderDimensions();
        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_TexelSize", float2(1.0F) / float2(bufferDimensions));
        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_Reprojection", cameraMatrices.reprojection);
        gfxProgramSetParameter(
            gfx_, disocclusion_mask_program_, "g_ViewProjectionInverse", cameraMatrices.inv_view_projection);

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, disocclusion_mask_kernel_);
        uint32_t const  num_groups_x = (bufferDimensions.x + num_threads[0] - 1) / num_threads[0];
        uint32_t const  num_groups_y = (bufferDimensions.y + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx_, disocclusion_mask_kernel_);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
    }

    if (debugView == "Meshlets")
    {
        if (debug_program_view != debugView)
        {
            gfxDestroyKernel(gfx_, debug_kernel);
            gfxDestroyProgram(gfx_, debug_program);
            gfxDestroySbt(gfx_, debug_sbt);
            debug_sbt = {};

            debug_program = capsaicin.createProgram("render_techniques/visibility_buffer/debug_meshlets");

            GfxDrawState const debug_state;
            gfxDrawStateSetCullMode(debug_state, D3D12_CULL_MODE_NONE);
            gfxDrawStateSetColorTarget(debug_state, 0, capsaicin.getSharedTexture("Debug").getFormat());
            gfxDrawStateSetDepthStencilTarget(debug_state, capsaicin.getSharedTexture("Depth").getFormat());
            gfxDrawStateSetDepthWriteMask(debug_state, D3D12_DEPTH_WRITE_MASK_ZERO);
            gfxDrawStateSetDepthFunction(debug_state, D3D12_COMPARISON_FUNC_EQUAL);
            std::vector defines = {"DEBUG_MESHLETS"};
            debug_kernel = gfxCreateMeshKernel(gfx_, debug_program, debug_state, nullptr, defines.data(),
                static_cast<uint32_t>(defines.size()));
            debug_program_view = debugView;
        }

        GfxCommandEvent const command_event(gfx_, "DrawDebugMeshlets");

        gfxProgramSetParameter(gfx_, debug_program, "g_VBConstants", constants_buffer);
        gfxProgramSetParameter(gfx_, debug_program, "g_DrawDataBuffer", draw_data_buffer);

        gfxProgramSetParameter(
            gfx_, debug_program, "g_MeshletCullBuffer", capsaicin.getSharedBuffer("MeshletCull"));
        gfxProgramSetParameter(gfx_, debug_program, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_TransformBuffer", capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_MeshletBuffer", capsaicin.getSharedBuffer("Meshlets"));
        gfxProgramSetParameter(
            gfx_, debug_program, "g_MeshletPackBuffer", capsaicin.getSharedBuffer("MeshletPack"));
        gfxProgramSetParameter(gfx_, debug_program, "g_IndexBuffer", capsaicin.getIndexBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_VertexBuffer", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_TransformBuffer", capsaicin.getTransformBuffer());

        gfxProgramSetParameter(gfx_, debug_program, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
        gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("Debug"));
        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));

        {
            TimedSection const timed_section(*this, "DebugMeshlets");

            uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, debug_kernel);
            uint32_t const  num_groups_x = (drawCount + num_threads[0] - 1) / num_threads[0];

            gfxCommandBindKernel(gfx_, debug_kernel);
            gfxCommandDrawMesh(gfx_, num_groups_x, 1, 1);
        }
    }
    else if (debugView == "Wireframe")
    {
        if (debug_program_view != debugView)
        {
            gfxDestroyKernel(gfx_, debug_kernel);
            gfxDestroyProgram(gfx_, debug_program);
            gfxDestroySbt(gfx_, debug_sbt);
            debug_sbt = {};

            debug_program = capsaicin.createProgram("render_techniques/visibility_buffer/debug_wireframe");

            GfxDrawState const debug_state;
            gfxDrawStateSetCullMode(debug_state, D3D12_CULL_MODE_NONE);
            gfxDrawStateSetColorTarget(debug_state, 0, capsaicin.getSharedTexture("Debug").getFormat());
            gfxDrawStateSetDepthStencilTarget(debug_state, capsaicin.getSharedTexture("Depth").getFormat());
            gfxDrawStateSetDepthWriteMask(debug_state, D3D12_DEPTH_WRITE_MASK_ZERO);
            gfxDrawStateSetDepthFunction(debug_state, D3D12_COMPARISON_FUNC_EQUAL);
            debug_kernel       = gfxCreateMeshKernel(gfx_, debug_program, debug_state);
            debug_program_view = debugView;
        }

        GfxCommandEvent const command_event(gfx_, "DrawDebugWireframe");

        gfxProgramSetParameter(gfx_, debug_program, "g_VBConstants", constants_buffer);
        gfxProgramSetParameter(gfx_, debug_program, "g_DrawDataBuffer", draw_data_buffer);

        gfxProgramSetParameter(
            gfx_, debug_program, "g_MeshletCullBuffer", capsaicin.getSharedBuffer("MeshletCull"));
        gfxProgramSetParameter(gfx_, debug_program, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_TransformBuffer", capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_MeshletBuffer", capsaicin.getSharedBuffer("Meshlets"));
        gfxProgramSetParameter(
            gfx_, debug_program, "g_MeshletPackBuffer", capsaicin.getSharedBuffer("MeshletPack"));
        gfxProgramSetParameter(gfx_, debug_program, "g_IndexBuffer", capsaicin.getIndexBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_VertexBuffer", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_TransformBuffer", capsaicin.getTransformBuffer());

        gfxProgramSetParameter(gfx_, debug_program, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
        gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("Debug"));
        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));

        {
            TimedSection const timed_section(*this, "DebugWireframe");

            uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, debug_kernel);
            uint32_t const  num_groups_x = (drawCount + num_threads[0] - 1) / num_threads[0];

            gfxCommandBindKernel(gfx_, debug_kernel);
            gfxCommandDrawMesh(gfx_, num_groups_x, 1, 1);
        }
    }
    else if (debugView == "Velocity")
    {
        if (debug_program_view != debugView)
        {
            gfxDestroyKernel(gfx_, debug_kernel);
            gfxDestroyProgram(gfx_, debug_program);
            gfxDestroySbt(gfx_, debug_sbt);
            debug_sbt = {};

            debug_program = capsaicin.createProgram("render_techniques/visibility_buffer/debug_velocity");

            GfxDrawState const debug_state;
            gfxDrawStateSetColorTarget(debug_state, 0, capsaicin.getSharedTexture("Debug").getFormat());
            debug_kernel       = gfxCreateGraphicsKernel(gfx_, debug_program, debug_state);
            debug_program_view = debugView;
        }

        GfxCommandEvent const command_event(gfx_, "DrawDebugVelocities");
        gfxProgramSetParameter(gfx_, debug_program, "VelocityBuffer", capsaicin.getSharedTexture("Velocity"));
        gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("Debug"));
        gfxCommandBindKernel(gfx_, debug_kernel);
        gfxCommandDraw(gfx_, 3);
    }
    else if (debugView.starts_with("Material"))
    {
        if (debug_program_view != debugView)
        {
            gfxDestroyKernel(gfx_, debug_kernel);
            gfxDestroyProgram(gfx_, debug_program);
            gfxDestroySbt(gfx_, debug_sbt);
            debug_sbt = {};

            debug_program = capsaicin.createProgram("render_techniques/visibility_buffer/debug_material");

            GfxDrawState const debug_material_draw_state;
            gfxDrawStateSetColorTarget(
                debug_material_draw_state, 0, capsaicin.getSharedTexture("Debug").getFormat());
            debug_kernel =
                gfxCreateGraphicsKernel(gfx_, debug_program, debug_material_draw_state, "DebugMaterial");
            debug_program_view = debugView;
        }

        enum class MaterialMode : uint32_t
        {
            Albedo = 0,
            Metallicity,
            Roughness,
        };
        auto materialMode = MaterialMode::Albedo;
        if (debugView == "MaterialMetallicity")
        {
            materialMode = MaterialMode::Metallicity;
        }
        else if (debugView == "MaterialRoughness")
        {
            materialMode = MaterialMode::Roughness;
        }

        GfxCommandEvent const command_event(gfx_, "DebugMaterial");
        gfxProgramSetParameter(gfx_, debug_program, "g_MaterialMode", materialMode);

        gfxProgramSetParameter(
            gfx_, debug_program, "g_VisibilityBuffer", capsaicin.getSharedTexture("Visibility"));
        gfxProgramSetParameter(
            gfx_, debug_program, "g_DepthBuffer", capsaicin.getSharedTexture("VisibilityDepth"));

        gfxProgramSetParameter(gfx_, debug_program, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_IndexBuffer", capsaicin.getIndexBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_VertexBuffer", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, debug_program, "g_VertexDataIndex", capsaicin.getVertexDataIndex());
        gfxProgramSetParameter(gfx_, debug_program, "g_MaterialBuffer", capsaicin.getMaterialBuffer());

        auto const &textures = capsaicin.getTextures();
        gfxProgramSetParameter(
            gfx_, debug_program, "g_TextureMaps", textures.data(), static_cast<uint32_t>(textures.size()));
        gfxProgramSetParameter(gfx_, debug_program, "g_TextureSampler", capsaicin.getAnisotropicSampler());
        gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("Debug"));
        gfxCommandBindKernel(gfx_, debug_kernel);
        gfxCommandDraw(gfx_, 3);
    }
    else if (debugView == "DXR1.0")
    {
        if (debug_program_view != debugView)
        {
            gfxDestroyKernel(gfx_, debug_kernel);
            gfxDestroyProgram(gfx_, debug_program);
            gfxDestroySbt(gfx_, debug_sbt);
            debug_sbt = {};

            debug_program = capsaicin.createProgram("render_techniques/visibility_buffer/debug_dxr10");
            // Associate space1 with local root signature for MyHitGroup
            GfxLocalRootSignatureAssociation local_root_signature_associations[] = {
                {1, kGfxShaderGroupType_Hit, "MyHitGroup"}
            };
            debug_kernel =
                gfxCreateRaytracingKernel(gfx_, debug_program, local_root_signature_associations, 1);
            uint32_t entry_count[kGfxShaderGroupType_Count] {
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Raygen),
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Miss),
                gfxSceneGetInstanceCount(capsaicin.getScene())
                    * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit),
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Callable)};
            debug_sbt          = gfxCreateSbt(gfx_, &debug_kernel, 1, entry_count);
            debug_program_view = debugView;
        }

        GfxCommandEvent const command_event(gfx_, "DrawDebugDXR1.0");
        gfxProgramSetParameter(gfx_, debug_program, "g_Eye", capsaicin.getCamera().eye);
        gfxProgramSetParameter(
            gfx_, debug_program, "g_ViewProjectionInverse", cameraMatrices.inv_view_projection);
        gfxProgramSetParameter(gfx_, debug_program, "g_Scene", capsaicin.getAccelerationStructure());
        gfxProgramSetParameter(gfx_, debug_program, "g_RenderTarget", capsaicin.getSharedTexture("Debug"));
        // Populate shader binding table
        gfxSbtSetShaderGroup(gfx_, debug_sbt, kGfxShaderGroupType_Raygen, 0, "MyRaygenShader");
        gfxSbtSetShaderGroup(gfx_, debug_sbt, kGfxShaderGroupType_Miss, 0, "MyMissShader");
        for (uint32_t i = 0; i < capsaicin.getRaytracingPrimitiveCount(); i++)
        {
            gfxSbtSetShaderGroup(gfx_, debug_sbt, kGfxShaderGroupType_Hit,
                i * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit),
                i % 2 == 0 ? "MyHitGroup" : "MyHitGroup2");
            // Populate local root signature parameters
            glm::vec4 test_data = i % 4 == 0 ? glm::vec4(0, 0, 1, 0) : glm::vec4(0, 1, 0, 0);
            gfxSbtSetConstants(gfx_, debug_sbt, kGfxShaderGroupType_Hit,
                i * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit), "g_MyCB", &test_data,
                sizeof(test_data));
        }
        gfxCommandBindKernel(gfx_, debug_kernel);
        auto bufferDimensions = capsaicin.getRenderDimensions();
        gfxCommandDispatchRays(gfx_, debug_sbt, bufferDimensions.x, bufferDimensions.y, 1);
    }
    else if (!debug_program_view.empty())
    {
        gfxDestroyKernel(gfx_, debug_kernel);
        debug_kernel = {};
        gfxDestroyProgram(gfx_, debug_program);
        debug_program = {};
        gfxDestroySbt(gfx_, debug_sbt);
        debug_sbt          = {};
        debug_program_view = "";
    }
}

void VisibilityBuffer::terminate() noexcept
{
    gfxDestroyKernel(gfx_, disocclusion_mask_kernel_);
    disocclusion_mask_kernel_ = {};
    gfxDestroyProgram(gfx_, disocclusion_mask_program_);
    disocclusion_mask_program_ = {};

    gfxDestroyKernel(gfx_, visibility_buffer_kernel_);
    visibility_buffer_kernel_ = {};
    gfxDestroyProgram(gfx_, visibility_buffer_program_);
    visibility_buffer_program_ = {};
    gfxDestroySbt(gfx_, visibility_buffer_sbt_);
    visibility_buffer_sbt_ = {};

    gfxDestroyKernel(gfx_, debug_kernel);
    debug_kernel = {};
    gfxDestroyProgram(gfx_, debug_program);
    debug_program = {};
    gfxDestroySbt(gfx_, debug_sbt);
    debug_sbt          = {};
    debug_program_view = "";

    gfxDestroyBuffer(gfx_, draw_data_buffer);
    draw_data_buffer = {};
    gfxDestroyBuffer(gfx_, constants_buffer);
    constants_buffer = {};
    gfxDestroyBuffer(gfx_, meshlet_visibility_buffer);
    meshlet_visibility_buffer = {};
    gfxDestroyTexture(gfx_, depth_pyramid);
    depth_pyramid = {};
    gfxDestroySamplerState(gfx_, depth_pyramid_sampler);
    depth_pyramid_sampler = {};
}

void VisibilityBuffer::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    ImGui::Checkbox(
        "Disable Alpha Testing", &capsaicin.getOption<bool>("visibility_buffer_disable_alpha_testing"));
}

bool VisibilityBuffer::initKernel(CapsaicinInternal const &capsaicin) noexcept
{
    if (!options.visibility_buffer_use_rt)
    {
        // Initialise the raster variant of visibility buffer kernel
        GfxDrawState const visibility_buffer_draw_state = {};
        gfxDrawStateSetCullMode(visibility_buffer_draw_state, D3D12_CULL_MODE_NONE);
        gfxDrawStateSetDepthFunction(visibility_buffer_draw_state, D3D12_COMPARISON_FUNC_GREATER);

        gfxDrawStateSetColorTarget(
            visibility_buffer_draw_state, 0, capsaicin.getSharedTexture("Visibility").getFormat());
        gfxDrawStateSetColorTarget(
            visibility_buffer_draw_state, 1, capsaicin.getSharedTexture("GeometryNormal").getFormat());
        gfxDrawStateSetColorTarget(
            visibility_buffer_draw_state, 2, capsaicin.getSharedTexture("Velocity").getFormat());
        std::vector<char const *> defines;
        if (capsaicin.hasSharedTexture("ShadingNormal"))
        {
            gfxDrawStateSetColorTarget(
                visibility_buffer_draw_state, 3, capsaicin.getSharedTexture("ShadingNormal").getFormat());
            defines.push_back("HAS_SHADING_NORMAL");
        }
        if (capsaicin.hasSharedTexture("VertexNormal"))
        {
            gfxDrawStateSetColorTarget(
                visibility_buffer_draw_state, 4, capsaicin.getSharedTexture("VertexNormal").getFormat());
            defines.push_back("HAS_VERTEX_NORMAL");
        }
        if (capsaicin.hasSharedTexture("Roughness"))
        {
            gfxDrawStateSetColorTarget(
                visibility_buffer_draw_state, 5, capsaicin.getSharedTexture("Roughness").getFormat());
            defines.push_back("HAS_ROUGHNESS");
        }
        if (capsaicin.hasSharedTexture("Gradients"))
        {
            gfxDrawStateSetColorTarget(
                visibility_buffer_draw_state, 6, capsaicin.getSharedTexture("Gradients").getFormat());
            defines.push_back("HAS_GRADIENTS");
        }
        if (options.visibility_buffer_disable_alpha_testing)
        {
            defines.push_back("DISABLE_ALPHA_TESTING");
        }
        if (options.visibility_buffer_enable_hzb)
        {
            defines.push_back("VISIBILITY_ENABLE_HZB");
        }
        gfxDrawStateSetDepthStencilTarget(
            visibility_buffer_draw_state, capsaicin.getSharedTexture("Depth").getFormat());

        visibility_buffer_program_ =
            capsaicin.createProgram("render_techniques/visibility_buffer/visibility_buffer");
        visibility_buffer_kernel_ = gfxCreateMeshKernel(gfx_, visibility_buffer_program_,
            visibility_buffer_draw_state, nullptr, defines.data(), static_cast<uint32_t>(defines.size()));
    }
    else
    {
        // Initialise the ray tracing variant of visibility buffer kernel
        std::vector<char const *> defines;
        defines.push_back("HAS_RT");
        if (capsaicin.hasSharedTexture("ShadingNormal"))
        {
            defines.push_back("HAS_SHADING_NORMAL");
        }
        if (capsaicin.hasSharedTexture("VertexNormal"))
        {
            defines.push_back("HAS_VERTEX_NORMAL");
        }
        if (capsaicin.hasSharedTexture("Roughness"))
        {
            defines.push_back("HAS_ROUGHNESS");
        }
        if (options.visibility_buffer_disable_alpha_testing)
        {
            defines.push_back("DISABLE_ALPHA_TESTING");
        }
        visibility_buffer_program_ =
            capsaicin.createProgram("render_techniques/visibility_buffer/visibility_buffer_rt");
        if (options.visibility_buffer_use_rt_dxr10)
        {
            std::vector exports = {
                "VisibilityRTRaygen", "VisibilityRTMiss", "VisibilityRTAnyHit", "VisibilityRTClosestHit"};
            std::vector subobjects = {
                "VisibilityShaderConfig", "VisibilityPipelineConfig", "VisibilityRTHitGroup"};
            visibility_buffer_kernel_ = gfxCreateRaytracingKernel(gfx_, visibility_buffer_program_, nullptr,
                0, exports.data(), static_cast<uint32_t>(exports.size()), subobjects.data(),
                static_cast<uint32_t>(subobjects.size()), defines.data(),
                static_cast<uint32_t>(defines.size()));

            uint32_t entry_count[kGfxShaderGroupType_Count] {
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Raygen),
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Miss),
                gfxSceneGetInstanceCount(capsaicin.getScene())
                    * capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Hit),
                capsaicin.getSbtStrideInEntries(kGfxShaderGroupType_Callable)};
            std::vector const sbt_kernels = {visibility_buffer_kernel_};
            visibility_buffer_sbt_        = gfxCreateSbt(
                gfx_, sbt_kernels.data(), static_cast<uint32_t>(sbt_kernels.size()), entry_count);
        }
        else
        {
            defines.push_back("USE_INLINE_RT");
            visibility_buffer_kernel_ = gfxCreateComputeKernel(gfx_, visibility_buffer_program_,
                "VisibilityBufferRT", defines.data(), static_cast<uint32_t>(defines.size()));
        }
    }

    return !!visibility_buffer_program_;
}
} // namespace Capsaicin
