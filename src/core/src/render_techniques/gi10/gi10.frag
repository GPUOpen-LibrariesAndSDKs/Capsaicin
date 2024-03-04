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

#include "gi10_shared.h"

float3 g_Eye;
float2 g_NearFar;
uint g_FrameIndex;
float4 g_InvDeviceZ;
uint2 g_BufferDimensions;
uint g_DisableAlbedoTextures;

Texture2D g_DepthBuffer;
Texture2D g_ShadingNormalBuffer;
Texture2D g_RoughnessBuffer;
Texture2D g_VisibilityBuffer;
Texture2D g_OcclusionAndBentNormalBuffer;

Texture2D g_LutBuffer;
uint g_LutSize;

StructuredBuffer<uint> g_IndexBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
StructuredBuffer<Mesh> g_MeshBuffer;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Material> g_MaterialBuffer;

RWTexture2D<float4> g_IrradianceBuffer;
RWTexture2D<float4> g_ReflectionBuffer;

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_NearestSampler;
SamplerState g_LinearSampler;
SamplerState g_TextureSampler;

ConstantBuffer<GI10Constants> g_GI10Constants;
ConstantBuffer<ScreenProbesConstants> g_ScreenProbesConstants;
ConstantBuffer<GlossyReflectionsConstants> g_GlossyReflectionsConstants;
ConstantBuffer<GlossyReflectionsAtrousConstants> g_GlossyReflectionsAtrousConstants;

#include "../../geometry/geometry.hlsl"
#include "../../geometry/mesh.hlsl"
#include "../../materials/material_evaluation.hlsl"
#include "../../materials/material_sampling.hlsl"

#include "gi10.hlsl"
#include "glossy_reflections.hlsl"
#include "screen_probes.hlsl"

struct PS_OUTPUT
{
    float4 lighting : SV_Target0;
};

PS_OUTPUT ResolveGI10(in float4 pos : SV_Position)
{
    uint2 did = uint2(pos.xy);

    float depth = g_DepthBuffer.Load(int3(did, 0)).x;
    float3 normal = normalize(2.0f * g_ShadingNormalBuffer.Load(int3(did, 0)).xyz - 1.0f);

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

    // Get UV values from buffers
    UVs uvs = fetchUVs(mesh, primitiveID);

    float2 uv = (did + 0.5f) / g_BufferDimensions;
    float3 world = InverseProject(g_GI10Constants.view_proj_inv, uv, depth);
    float3 view_direction = normalize(g_Eye - world);
    float2 mesh_uv = interpolate(uvs.uv0, uvs.uv1, uvs.uv2, visibility.xy);
    float dotNV = saturate(dot(normal, view_direction));

    Material material = g_MaterialBuffer[instance.material_index];
    MaterialEvaluated material_evaluated = MakeMaterialEvaluated(material, mesh_uv);
    MaterialEmissive emissiveMaterial = MakeMaterialEmissive(material, mesh_uv);

    MaterialBRDF materialBRDF = MakeMaterialBRDF(material_evaluated);
    if (g_DisableAlbedoTextures)
    {
        materialBRDF.albedo = 0.3f.xxx;
#ifndef DISABLE_SPECULAR_MATERIALS
        materialBRDF.F0 = 0.0f.xxx;
#endif // DISABLE_SPECULAR_MATERIALS
    }

    PS_OUTPUT output;

#ifdef DISABLE_SPECULAR_MATERIALS

    float3 irradiance = g_IrradianceBuffer[did].xyz;
    float3 diffuse = evaluateLambert(materialBRDF.albedo) * irradiance;
    output.lighting = float4(emissiveMaterial.emissive + diffuse, 1.0f);

#else // DISABLE_SPECULAR_MATERIALS

    // compute diffuse compensation term with specular dominant half-vector
    float3 specular_dominant_direction = calculateGGXSpecularDirection(normal, view_direction, sqrt(materialBRDF.roughnessAlpha));
    float3 specular_dominant_half_vector = normalize(view_direction + specular_dominant_direction);
    float dotHV = saturate(dot(view_direction, specular_dominant_half_vector));
    float3 diffuse_compensation = diffuseCompensation(materialBRDF.F0, dotHV);

    // diffuse term
    float3 irradiance = g_IrradianceBuffer[did].xyz;
    float3 diffuse = evaluateLambert(materialBRDF.albedo) * diffuse_compensation * irradiance;

    // compute specular term with split-sum approximation
    float4 radiance_sum = (material_evaluated.roughness > g_GlossyReflectionsConstants.high_roughness_threshold
        ? float4(irradiance, PI) : g_ReflectionBuffer[did]);    // fall back to filtered irradiance past threshold
    float2 lut = g_LutBuffer.SampleLevel(g_LinearSampler, float2(dotNV, material_evaluated.roughness), 0.0f).xy;
    float3 directional_albedo = saturate(materialBRDF.F0 * lut.x + (1.0f - materialBRDF.F0) * lut.y);
    float3 specular = directional_albedo * (radiance_sum.xyz / max(radiance_sum.w, 1.0f));
    output.lighting = float4(emissiveMaterial.emissive + diffuse + specular, 1.0f);

#endif // DISABLE_SPECULAR_MATERIALS

    return output;
}

float4 DebugReflection(in float4 pos : SV_Position) : SV_Target
{
    int2   full_pos     = int2(pos.xy);
    float3 normal       = g_ShadingNormalBuffer.Load(int3(full_pos, 0)).xyz;
    float  roughness    = g_RoughnessBuffer.Load(int3(full_pos, 0)).x;
    bool   is_sky_pixel = (dot(normal, normal) == 0.0f ? true : false);
    if (is_sky_pixel || roughness > g_GlossyReflectionsConstants.high_roughness_threshold)
    {
        // Better black than looking to noisy diffuse fallback when debugging
        return float4(0.f, 0.f, 0.f, 1.f);
    }

    float4 lighting = g_ReflectionBuffer[full_pos];
    return float4(lighting.xyz / max(lighting.w, 1.0f), 1.0f);
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
