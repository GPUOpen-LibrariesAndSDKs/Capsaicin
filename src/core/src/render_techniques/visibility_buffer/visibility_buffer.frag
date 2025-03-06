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

#include "visibility_buffer_shared.h"

uint  g_FrameIndex;
float g_RenderScale;

StructuredBuffer<Material> g_MaterialBuffer;

#include "components/blue_noise_sampler/blue_noise_sampler.hlsl"

Texture2D    g_TextureMaps[];
SamplerState g_TextureSampler;
SamplerState g_LinearSampler;

#include "geometry/geometry.hlsl"
#include "math/math.hlsl"
#include "materials/materials.hlsl"
#include "materials/material_evaluation.hlsl"

struct Pixel
{
    float4 visibility  : SV_Target0;
    float4 geom_normal : SV_Target1;
    float2 velocity    : SV_Target2;
#ifdef HAS_SHADING_NORMAL
    float4 shad_normal : SV_Target3;
#endif
#ifdef HAS_VERTEX_NORMAL
    float4 vert_normal : SV_Target4;
#endif
#ifdef HAS_ROUGHNESS
    float  roughness   : SV_Target5;
#endif
#ifdef HAS_GRADIENTS
    float4 gradients   : SV_Target6;
#endif
};

Pixel main(in VertexParams params, in float3 barycentrics : SV_Barycentrics, in uint primitiveID : SV_PrimitiveID, in uint instanceID : INSTANCE_ID, in uint materialID : MATERIAL_ID)
{
    Pixel pixel;
    Material material = g_MaterialBuffer[materialID];

#ifndef DISABLE_ALPHA_TESTING
    if (asuint(material.normal_alpha_side.w) != 0)
    {
        float alpha = material.normal_alpha_side.y;
        uint alphaMap = asuint(material.albedo.w);
        if (alphaMap != uint(-1))
        {
            alpha *= g_TextureMaps[NonUniformResourceIndex(alphaMap)].Sample(g_LinearSampler, params.uv).w;
        }
        float alpha_threshold = 0.5f;
        if (alpha <= alpha_threshold)
        {
            discard;
        }
    }
#endif // DISABLE_ALPHA_TESTING

    float3 dFdxPos = ddx_fine(params.world);
    float3 dFdyPos = ddy_fine(params.world);
    float3 face_normal = normalize(cross(dFdyPos, dFdxPos));

#if defined(HAS_SHADING_NORMAL) || defined(HAS_VERTEX_NORMAL)
    float3 vertex_normal = normalize(params.normal);
    // SV_IsFrontFace incorrectly flips normals when the geometry has non uniform negative(mirrored) scaling
    vertex_normal = dot(vertex_normal, face_normal) >= 0.0f ? vertex_normal : -vertex_normal;

#   ifdef HAS_SHADING_NORMAL
    float3 details = vertex_normal;
    uint normalMap = asuint(material.normal_alpha_side.x);
    if (normalMap != uint(-1))
    {
        float2 dFdxUV = ddx(params.uv);
        float2 dFdyUV = ddy(params.uv);

        float determinate = dFdxUV.x * dFdyUV.y - dFdyUV.x * dFdxUV.y;
        float3 normalTan = 2.0f * g_TextureMaps[NonUniformResourceIndex(normalMap)].Sample(g_TextureSampler, params.uv).xyz - 1.0f;
        // If the determinate is zero then the matrix is non invertable
        if (determinate != 0.0f && dot(normalTan, normalTan) > 0.0f)
        {
            determinate = rcp(determinate);
            float3 tangentBasis = (dFdxPos * dFdyUV.yyy - dFdyPos * dFdxUV.yyy) * determinate;
            float3 bitangentBasis = (dFdyPos * dFdxUV.xxx - dFdxPos * dFdyUV.xxx) * determinate;

            // Gram-Schmidt orthogonalise tangent
            float3 tangent = normalize(tangentBasis - vertex_normal * dot(vertex_normal, tangentBasis));
            float3 bitangent = cross(vertex_normal, tangent);

            // Correct handedness
            bitangent = dot(bitangent, bitangentBasis) >= 0.0f ? -bitangent : bitangent;

            float3x3 tbn = float3x3(tangent, bitangent, vertex_normal);
            details = normalize(mul(normalTan, tbn));
        }
    }
#   endif // HAS_SHADING_NORMAL
#endif // HAS_SHADING_NORMAL || HAS_VERTEX_NORMAL

    pixel.visibility  = float4(barycentrics.yz, asfloat(instanceID), asfloat(primitiveID));
    pixel.geom_normal = float4(0.5f * face_normal + 0.5f, 1.0f);
    pixel.velocity    = CalculateMotionVector(params.current, params.previous);
#ifdef HAS_SHADING_NORMAL
    pixel.shad_normal = float4(0.5f * details + 0.5f, 1.0f);
#endif
#ifdef HAS_VERTEX_NORMAL
    pixel.vert_normal = float4(0.5f * vertex_normal + 0.5f, 1.0f);
#endif
#ifdef HAS_ROUGHNESS
    float roughness = material.metallicity_roughness.z;
    const uint roughnessTex = asuint(material.metallicity_roughness.w);
    if (roughnessTex != uint(-1))
    {
        roughness *= g_TextureMaps[NonUniformResourceIndex(roughnessTex)].Sample(g_TextureSampler, params.uv).x;
    }

    // NDF filtering for specular AA.
    // [Y. Tokuyoshi and A. S. Kaplanyan 2021 "Stable Geometric Specular Antialiasing with Projected-Space NDF Filtering"]
    const float roughnessAlpha = ConvertPerceptualRoughnessToAlpha(roughness);
    const float filteredAlpha2 = IsotropicNDFFiltering(ddx(vertex_normal), ddy(vertex_normal), roughnessAlpha * roughnessAlpha);
    roughness = ConvertAlphaToPerceptualRoughness(sqrt(filteredAlpha2));

    pixel.roughness = roughness;
#endif
#ifdef HAS_GRADIENTS
    pixel.gradients = float4(ddx_fine(params.uv), ddy_fine(params.uv)) * g_RenderScale;
#endif

    return pixel;
}
