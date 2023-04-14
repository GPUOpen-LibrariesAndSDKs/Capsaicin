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
#include "visibility_buffer.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
VisibilityBuffer::VisibilityBuffer()
    : RenderTechnique("Visibility buffer")
{}

VisibilityBuffer::~VisibilityBuffer()
{
    terminate();
}

AOVList VisibilityBuffer::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"VisibilityDepth", AOV::Write, AOV::Clear, DXGI_FORMAT_D32_FLOAT, "PrevVisibilityDepth"});
    aovs.push_back({"Normal", AOV::Write, AOV::Clear, DXGI_FORMAT_R8G8B8A8_UNORM});
    aovs.push_back({"Details", AOV::Write, AOV::Clear, DXGI_FORMAT_R8G8B8A8_UNORM});
    aovs.push_back({"Velocity", AOV::Write, AOV::Clear, DXGI_FORMAT_R16G16_FLOAT});
    aovs.push_back({"Visibility", AOV::Write, AOV::Clear, DXGI_FORMAT_R32G32B32A32_FLOAT});
    aovs.push_back({"DisocclusionMask", AOV::Write, AOV::None, DXGI_FORMAT_R8_UNORM});
    aovs.push_back({"Depth", AOV::ReadWrite});
    return aovs;
}

DebugViewList VisibilityBuffer::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("Velocity");
    return views;
}

bool VisibilityBuffer::init(CapsaicinInternal const &capsaicin) noexcept
{
    disocclusion_mask_program_ = gfxCreateProgram(
        gfx_, "render_techniques/visibility_buffer/disocclusion_mask", capsaicin.getShaderPath());
    disocclusion_mask_kernel_ = gfxCreateComputeKernel(gfx_, disocclusion_mask_program_);

    gfxProgramSetParameter(
        gfx_, disocclusion_mask_program_, "g_DepthBuffer", capsaicin.getAOVBuffer("VisibilityDepth"));
    gfxProgramSetParameter(
        gfx_, disocclusion_mask_program_, "g_NormalBuffer", capsaicin.getAOVBuffer("Normal"));
    gfxProgramSetParameter(
        gfx_, disocclusion_mask_program_, "g_VelocityBuffer", capsaicin.getAOVBuffer("Velocity"));
    gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_PreviousDepthBuffer",
        capsaicin.getAOVBuffer("PrevVisibilityDepth"));

    gfxProgramSetParameter(
        gfx_, disocclusion_mask_program_, "g_DisocclusionMask", capsaicin.getAOVBuffer("DisocclusionMask"));

    gfxProgramSetParameter(
        gfx_, disocclusion_mask_program_, "g_NearestSampler", capsaicin.getNearestSampler());

    GfxDrawState visibility_buffer_draw_state = {};
    gfxDrawStateSetCullMode(visibility_buffer_draw_state, D3D12_CULL_MODE_NONE);

    gfxDrawStateSetColorTarget(visibility_buffer_draw_state, 0, capsaicin.getAOVBuffer("Visibility"));
    gfxDrawStateSetColorTarget(visibility_buffer_draw_state, 1, capsaicin.getAOVBuffer("Normal"));
    gfxDrawStateSetColorTarget(visibility_buffer_draw_state, 2, capsaicin.getAOVBuffer("Details"));
    gfxDrawStateSetColorTarget(visibility_buffer_draw_state, 3, capsaicin.getAOVBuffer("Velocity"));
    gfxDrawStateSetDepthStencilTarget(visibility_buffer_draw_state, capsaicin.getAOVBuffer("Depth"));

    visibility_buffer_program_ = gfxCreateProgram(
        gfx_, "render_techniques/visibility_buffer/visibility_buffer", capsaicin.getShaderPath());
    visibility_buffer_kernel_ =
        gfxCreateGraphicsKernel(gfx_, visibility_buffer_program_, visibility_buffer_draw_state);

    return !!visibility_buffer_program_;
}

void VisibilityBuffer::render(CapsaicinInternal &capsaicin) noexcept
{
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
        draw_commands[i].StartIndexLocation    = mesh.index_offset / mesh.index_stride;
        draw_commands[i].BaseVertexLocation    = mesh.vertex_offset / mesh.vertex_stride;
        draw_commands[i].StartInstanceLocation = i; // <- drawID
    }

    gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_Eye", capsaicin.getCamera().eye);
    gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_FrameIndex", capsaicin.getFrameIndex());
    auto const &camera =
        capsaicin.getCameraMatrices(capsaicin.getRenderSettings().getOption<bool>("taa_enable"));
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

    gfxProgramSetParameter(gfx_, visibility_buffer_program_, "g_TextureMaps", capsaicin.getTextures(),
        capsaicin.getTextureCount());
    gfxProgramSetParameter(
        gfx_, visibility_buffer_program_, "g_TextureSampler", capsaicin.getAnisotropicSampler());

    gfxCommandBindKernel(gfx_, visibility_buffer_kernel_);
    gfxCommandMultiDrawIndexedIndirect(gfx_, draw_command_buffer, instance_count);
    gfxCommandCopyTexture(gfx_, capsaicin.getAOVBuffer("VisibilityDepth"), capsaicin.getAOVBuffer("Depth"));

    gfxDestroyBuffer(gfx_, draw_command_buffer);

    glm::mat4 const view_projection_inverse = glm::inverse(glm::dmat4(camera.view_projection));
    glm::mat4 const reprojection_matrix =
        glm::dmat4(camera.view_projection_prev) * glm::dmat4(view_projection_inverse);

    gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_Eye", capsaicin.getCamera().eye);
    gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_NearFar",
        float2(capsaicin.getCamera().nearZ, capsaicin.getCamera().farZ));
    gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_TexelSize",
        float2(1.0f / capsaicin.getWidth(), 1.0f / capsaicin.getHeight()));
    gfxProgramSetParameter(gfx_, disocclusion_mask_program_, "g_Reprojection", reprojection_matrix);
    gfxProgramSetParameter(
        gfx_, disocclusion_mask_program_, "g_ViewProjectionInverse", view_projection_inverse);

    uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, disocclusion_mask_kernel_);
    uint32_t const  num_groups_x = (capsaicin.getWidth() + num_threads[0] - 1) / num_threads[0];
    uint32_t const  num_groups_y = (capsaicin.getHeight() + num_threads[1] - 1) / num_threads[1];

    gfxCommandBindKernel(gfx_, disocclusion_mask_kernel_);
    gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);

    if (capsaicin.getRenderSettings().debug_view_ == "Velocity")
    {
        if (!debug_velocities_program_)
        {
            char const *screen_triangle_vs =
                "struct VS_OUTPUT { float4 pos : SV_POSITION; float2 texcoord : TEXCOORD; };"
                "VS_OUTPUT main(in uint idx : SV_VertexID) { VS_OUTPUT output; output.texcoord = float2(1.0f - 2.0f * (idx & 1), 2.0f * (idx >> 1));"
                "output.pos = 1.0f - float4(4.0f * (idx & 1), 4.0f * (idx >> 1), 1.0f, 0.0f); return output; }";

            GfxProgramDesc debug_velocities_program_desc = {};
            debug_velocities_program_desc.vs             = screen_triangle_vs;
            debug_velocities_program_desc.ps =
                "Texture2D VelocityBuffer; float4 main(in float4 pos : SV_Position) : SV_Target {"
                "float2 velocity = VelocityBuffer.Load(int3(pos.xy, 0)).xy;"
                "return float4(pow(float3((velocity.y >= 0.0f ?  velocity.y : 0.0f) + (velocity.y < 0.0f ? -velocity.y : 0.0f),"
                "                         (velocity.x <  0.0f ? -velocity.x : 0.0f) + (velocity.y < 0.0f ? -velocity.y : 0.0f),"
                "                         (velocity.x >= 0.0f ?  velocity.x : 0.0f)), 0.4f), 1.0f); }";
            debug_velocities_program_ =
                gfxCreateProgram(gfx_, debug_velocities_program_desc, "Capsaicin_DebugVelocitiesProgram");

            GfxDrawState debug_state;
            gfxDrawStateSetColorTarget(debug_state, 0, capsaicin.getAOVBuffer("Debug"));
            debug_velocities_kernel_ = gfxCreateGraphicsKernel(gfx_, debug_velocities_program_, debug_state);
        }

        const GfxCommandEvent command_event(gfx_, "DrawDebugView");
        gfxProgramSetParameter(
            gfx_, debug_velocities_program_, "VelocityBuffer", capsaicin.getAOVBuffer("Velocity"));
        gfxCommandBindKernel(gfx_, debug_velocities_kernel_);
        gfxCommandDraw(gfx_, 3);
    }
}

void VisibilityBuffer::terminate()
{
    gfxDestroyKernel(gfx_, disocclusion_mask_kernel_);
    gfxDestroyProgram(gfx_, disocclusion_mask_program_);

    gfxDestroyKernel(gfx_, visibility_buffer_kernel_);
    gfxDestroyProgram(gfx_, visibility_buffer_program_);

    gfxDestroyKernel(gfx_, debug_velocities_kernel_);
    gfxDestroyProgram(gfx_, debug_velocities_program_);
}
} // namespace Capsaicin
