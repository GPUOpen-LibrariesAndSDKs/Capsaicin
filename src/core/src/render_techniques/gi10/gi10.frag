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

#include "gi10_shared.h"

float3 g_Eye;
float2 g_NearFar;
uint g_FrameIndex;
float4 g_InvDeviceZ;
uint2 g_BufferDimensions;
uint g_DisableAlbedoTextures;

Texture2D g_DepthBuffer;
Texture2D g_NormalBuffer;
Texture2D g_DetailsBuffer;
Texture2D g_VisibilityBuffer;
Texture2D g_OcclusionAndBentNormalBuffer;

StructuredBuffer<uint> g_IndexBuffers[] : register(space1);
StructuredBuffer<Vertex> g_VertexBuffers[] : register(space2);

StructuredBuffer<Mesh> g_MeshBuffer;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Material> g_MaterialBuffer;

RWTexture2D<float4> g_IrradianceBuffer;

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_NearestSampler;
SamplerState g_LinearSampler;
SamplerState g_TextureSampler;

ConstantBuffer<GI10Constants> g_GI10Constants;
ConstantBuffer<ScreenProbesConstants> g_ScreenProbesConstants;

#include "../../materials/material_evaluation.hlsl"
#include "../../materials/material_sampling.hlsl"
#include "../../math/geometry.hlsl"

#include "gi10.hlsl"
#include "screen_probes.hlsl"

struct PS_OUTPUT
{
    float4 lighting : SV_Target0;
};

PS_OUTPUT ResolveGI10(in float4 pos : SV_Position)
{
    uint2 did = uint2(pos.xy);

    float depth = g_DepthBuffer.Load(int3(did, 0)).x;
    float3 normal = normalize(2.0f * g_DetailsBuffer.Load(int3(did, 0)).xyz - 1.0f);

    if (depth >= 1.0f)
    {
        PS_OUTPUT output;
        output.lighting = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return output;
    }

    float4 visibility = g_VisibilityBuffer.Load(int3(did, 0));
    uint instanceID = asuint(visibility.z);
    uint primitiveID = asuint(visibility.w);

    Instance instance = g_InstanceBuffer[instanceID];
    Mesh mesh = g_MeshBuffer[instance.mesh_index];

    uint i0 = g_IndexBuffers[0][mesh.index_offset / mesh.index_stride + 3 * primitiveID + 0] + mesh.vertex_offset / mesh.vertex_stride;
    uint i1 = g_IndexBuffers[0][mesh.index_offset / mesh.index_stride + 3 * primitiveID + 1] + mesh.vertex_offset / mesh.vertex_stride;
    uint i2 = g_IndexBuffers[0][mesh.index_offset / mesh.index_stride + 3 * primitiveID + 2] + mesh.vertex_offset / mesh.vertex_stride;

    float2 uv0 = g_VertexBuffers[0][i0].uv;
    float2 uv1 = g_VertexBuffers[0][i1].uv;
    float2 uv2 = g_VertexBuffers[0][i2].uv;

    float2 uv = (did + 0.5f) / g_BufferDimensions;
    float3 world = InverseProject(g_GI10Constants.view_proj_inv, uv, depth);
    float3 view_direction = normalize(g_Eye - world);
    float2 mesh_uv = interpolate(uv0, uv1, uv2, visibility.xy);
    float dotNV = saturate(dot(normal, view_direction));

    Material material = g_MaterialBuffer[mesh.material_index];
    MaterialEvaluated material_evaluated = MakeMaterialEvaluated(material, mesh_uv);
    MaterialEmissive emissiveMaterial = MakeMaterialEmissive(material, mesh_uv);

#ifndef DISABLE_SPECULAR_LIGHTING
    material_evaluated.metallicity = 0.0f;
#endif
    MaterialBRDF materialBRDF = MakeMaterialBRDF(material_evaluated);
    if (g_DisableAlbedoTextures)
    {
        materialBRDF.albedo = 0.3f.xxx;
#ifndef DISABLE_SPECULAR_LIGHTING
        materialBRDF.F0 = 0.0f.xxx;
#endif
    }

    PS_OUTPUT output;

    float3 irradiance = g_IrradianceBuffer[did].xyz;
    float3 diffuse = evaluateLambert(materialBRDF.albedo) * irradiance;
    output.lighting = float4(emissiveMaterial.emissive + diffuse, 1.0f);

    return output;
}

float4 DebugScreenProbes(in float4 pos : SV_Position) : SV_Target
{
    uint2 did = uint2(pos.xy);
    uint2 probe_tile;
    uint2 probe_pos;
    switch (g_ScreenProbesConstants.debug_mode)
    {
        default:
        case SCREENPROBES_DEBUG_RADIANCE:
            probe_tile = (did / g_ScreenProbesConstants.probe_size);
            probe_pos = (probe_tile * g_ScreenProbesConstants.probe_size)
                     + (did % g_ScreenProbesConstants.probe_size);
            break;
        case SCREENPROBES_DEBUG_RADIANCE_PER_DIRECTION:
            probe_tile = (did % g_ScreenProbesConstants.probe_count);
            probe_pos = (probe_tile * g_ScreenProbesConstants.probe_size)
                       + (did / g_ScreenProbesConstants.probe_count);
            break;
    }

    if (g_ScreenProbes_ProbeMaskBuffer[probe_tile] == kGI10_InvalidId)
    {
        return float4(1e4f, 0.0f, 0.0f, 1.0f);
    }

    float3 radiance = g_ScreenProbes_ProbeBuffer[probe_pos].xyz;

    return float4(radiance / PI, 1.0f);
}

struct DebugHashGridCells_Params
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

float4 DebugHashGridCells(in DebugHashGridCells_Params params) : SV_Target
{
    if (params.color.w == 0.0f)
    {
        discard; // eliminate transparent pixels
    }

    return float4(params.color.xyz, 1.0f);
}
