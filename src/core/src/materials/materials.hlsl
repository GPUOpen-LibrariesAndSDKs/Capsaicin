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

#ifndef MATERIALS_HLSL
#define MATERIALS_HLSL
/*
// Requires the following data to be defined in any shader that uses this file
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;
*/

#include "gpu_shared.h"

/** Material data representing a material already evaluated at a specific UV coordinate. */
struct MaterialEvaluated
{
    float3 albedo;
#ifndef DISABLE_SPECULAR_MATERIALS
    float metallicity;
    float roughness;
#endif
};

/**
 * Calculates material data by evaluating any texture data.
 * @param material The material to evaluate.
 * @param uv       The UV coordinates to evaluate the material at.
 * @return The new material data.
 */
MaterialEvaluated MakeMaterialEvaluated(Material material, float2 uv)
{
    // Load initial values and any textures
    float3 albedo = material.albedo.xyz;
    uint albedoTex = asuint(material.albedo.w);
    if (albedoTex != uint(-1))
    {
        albedo *= g_TextureMaps[NonUniformResourceIndex(albedoTex)].SampleLevel(g_TextureSampler, uv, 0.0f).xyz;
    }

#ifndef DISABLE_SPECULAR_MATERIALS
    float metallicity = material.metallicity_roughness.x;
    uint metallicityTex = asuint(material.metallicity_roughness.y);
    if (metallicityTex != uint(-1))
    {
        metallicity *= g_TextureMaps[NonUniformResourceIndex(metallicityTex)].SampleLevel(g_TextureSampler, uv, 0.0f).x;
    }

    float roughness = material.metallicity_roughness.z;
    uint roughnessTex = asuint(material.metallicity_roughness.w);
    if (roughnessTex != uint(-1))
    {
        roughness *= g_TextureMaps[NonUniformResourceIndex(roughnessTex)].SampleLevel(g_TextureSampler, uv, 0.0f).x;
    }
#endif

    MaterialEvaluated ret =
    {
        albedo,
#ifndef DISABLE_SPECULAR_MATERIALS
        metallicity,
        roughness
#endif
    };
    return ret;
}

/**
 * Calculates material data by evaluating any texture data.
 * @param material The material to evaluate.
 * @param uv       The UV coordinates to evaluate the material at.
 * @param gradX    The UV partial derivatives along the X axis.
 * @param gradY    The UV partial derivatives along the Y axis.
 * @return The new material data.
 */
MaterialEvaluated MakeMaterialEvaluated(Material material, float2 uv, float2 gradX, float2 gradY)
{
    // Load initial values and any textures
    float3 albedo = material.albedo.xyz;
    uint albedoTex = asuint(material.albedo.w);
    if (albedoTex != uint(-1))
    {
        albedo *= g_TextureMaps[NonUniformResourceIndex(albedoTex)].SampleGrad(g_TextureSampler, uv, gradX, gradY).xyz;
    }

#ifndef DISABLE_SPECULAR_MATERIALS
    float metallicity = material.metallicity_roughness.x;
    uint metallicityTex = asuint(material.metallicity_roughness.y);
    if (metallicityTex != uint(-1))
    {
        metallicity *= g_TextureMaps[NonUniformResourceIndex(metallicityTex)].SampleGrad(g_TextureSampler, uv, gradX, gradY).x;
    }

    float roughness = material.metallicity_roughness.z;
    uint roughnessTex = asuint(material.metallicity_roughness.w);
    if (roughnessTex != uint(-1))
    {
        roughness *= g_TextureMaps[NonUniformResourceIndex(roughnessTex)].SampleGrad(g_TextureSampler, uv, gradX, gradY).x;
    }
#endif

    MaterialEvaluated ret =
    {
        albedo,
#ifndef DISABLE_SPECULAR_MATERIALS
        metallicity,
        roughness
#endif
    };
    return ret;
}

/** Material data required for current BRDF type. */
struct MaterialBRDF
{
    float3 albedo;
#ifndef DISABLE_SPECULAR_MATERIALS
    float roughnessAlpha;
    float3 F0;
    float roughnessAlphaSqr;
#endif
};

/**
 * Convert perceptual roughness to alpha roughness.
 * @param perceptualRoughness Perceptual roughness.
 * @return Alpha roughness.
 */
float ConvertPerceptualRoughnessToAlpha(const float perceptualRoughness)
{
    return perceptualRoughness * perceptualRoughness;
}

/**
 * Convert alpha roughness to perceptual roughness.
 * @param alpha Alpha roughness.
 * @return Perceptual roughness.
 */
float ConvertAlphaToPerceptualRoughness(const float alpha)
{
    return sqrt(alpha);
}

/**
 * Clamp alpha roughness for numerical stability.
 * @param alpha Alpha roughness.
 * @return Clamped alpha roughness.
 */
float ClampAlphaRoughness(const float alpha)
{
    const float ALPHA_MIN = 0.000001;
    return max(alpha, ALPHA_MIN);
}

/**
 * Calculates the material data directly from an evaluated material.
 * @param albedo      Materials albedo value.
 * @param metallicity Materials metallicity value.
 * @param roughness   Materials roughness value.
 * @return The new material data.
 */
MaterialBRDF MakeMaterialBRDF(MaterialEvaluated material)
{
    float3 albedo = material.albedo;
#ifndef DISABLE_SPECULAR_MATERIALS
    // Calculate albedo/F0 using metallicity
    float3 F0 = lerp(0.04f.xxx, albedo, material.metallicity);
    albedo *= (1.0f - material.metallicity);
    float roughnessAlpha = ClampAlphaRoughness(ConvertPerceptualRoughnessToAlpha(material.roughness));
    float roughnessAlphaSqr = ClampAlphaRoughness(roughnessAlpha * roughnessAlpha);
#endif

    MaterialBRDF ret =
    {
        albedo,
#ifndef DISABLE_SPECULAR_MATERIALS
        roughnessAlpha,
        F0,
        roughnessAlphaSqr
#endif
    };
    return ret;
}

/**
 * Calculates the material reflectance data for the internal material type.
 * @param material Renderer data describing material.
 * @param uv       The texture UV values at intersected position.
 * @return The new material data.
 */
MaterialBRDF MakeMaterialBRDF(Material material, float2 uv)
{
    return MakeMaterialBRDF(MakeMaterialEvaluated(material, uv));
}

/**
 * Calculates the material reflectance data for the internal material type.
 * @param material Renderer data describing material.
 * @param uv       The texture UV values at intersected position.
 * @param gradX    The UV partial derivatives along the X axis.
 * @param gradY    The UV partial derivatives along the Y axis.
 * @return The new material data.
 */
MaterialBRDF MakeMaterialBRDF(Material material, float2 uv, float2 gradX, float2 gradY)
{
    return MakeMaterialBRDF(MakeMaterialEvaluated(material, uv, gradX, gradY));
}

/** Material data required for checking current alpha mask/blend. */
struct MaterialAlpha
{
    float alpha;
};

/**
 * Calculates the material mask/blend data for the internal material type.
 * @param material Renderer data describing material.
 * @param uv       The texture UV values at intersected position.
 * @return The new material data.
 */
MaterialAlpha MakeMaterialAlpha(Material material, float2 uv)
{
    float alpha = material.normal_alpha_side.y;
    uint albedoTex = asuint(material.albedo.w);
    if (albedoTex != uint(-1))
    {
        alpha *= g_TextureMaps[NonUniformResourceIndex(albedoTex)].SampleLevel(g_TextureSampler, uv, 0.0f).w;
    }
    MaterialAlpha ret =
    {
        alpha
    };
    return ret;
}

/**
 * Calculates the material mask/blend data for the internal material type.
 * @param material Renderer data describing material.
 * @param uv       The texture UV values at intersected position.
 * @param gradX    The UV partial derivatives along the X axis.
 * @param gradY    The UV partial derivatives along the Y axis.
 * @return The new material data.
 */
MaterialAlpha MakeMaterialAlpha(Material material, float2 uv, float2 gradX, float2 gradY)
{
    float alpha = material.normal_alpha_side.y;
    uint albedoTex = asuint(material.albedo.w);
    if (albedoTex != uint(-1))
    {
        alpha *= g_TextureMaps[NonUniformResourceIndex(albedoTex)].SampleGrad(g_TextureSampler, uv, gradX, gradY).w;
    }
    MaterialAlpha ret =
    {
        alpha
    };
    return ret;
}

/** Material data representing entire combined surface properties for the internal material type. */
struct MaterialBSDF : MaterialBRDF
{
    float3 emissive;
};

/**
 * Calculates the dimming of emissive materials due to alpha blending.
 * @param material Renderer data describing material.
 * @param uv       The texture UV values at intersected position.
 * @return The emissive values with alpha scaling applied.
 */
float4 emissiveAlphaScaled(Material material, float2 uv)
{
    float4 areaEmissivity = material.emissivity;
#ifdef DISABLE_ALPHA_TESTING
    // We don't check if the current UV's have been alpha clipped as it is assumed that clipped geometry will never be rasterised/intersected
    if (asuint(material.normal_alpha_side.w) == 2)
    {
        // Hit surfaces currently use stochastic alpha which reduces the likelihood of a surface intersection
        //   by its relative alpha value. This has the effect of reducing emission with decreasing alpha.
        //   When changing to a different alpha blending mode then this function must be updated to scale
        //   emission by alpha
        MaterialAlpha alpha = MakeMaterialAlpha(material, uv);
        areaEmissivity.xyz *= alpha.alpha;
    }
#endif
    return areaEmissivity;
}

/**
 * Calculates the dimming of emissive materials due to alpha blending.
 * @param material Renderer data describing material.
 * @param uv       The texture UV values at intersected position.
 * @param gradX    The UV partial derivatives along the X axis.
 * @param gradY    The UV partial derivatives along the Y axis.
 * @return The emissive values with alpha scaling applied.
 */
float4 emissiveAlphaScaled(Material material, float2 uv, float2 gradX, float2 gradY)
{
    float4 areaEmissivity = material.emissivity;
#ifdef DISABLE_ALPHA_TESTING
    if (asuint(material.normal_alpha_side.w) == 2)
    {
        MaterialAlpha alpha = MakeMaterialAlpha(material, uv, gradX, gradY);
        areaEmissivity.xyz *= alpha.alpha;
    }
#endif
    return areaEmissivity;
}

/**
 * Calculates the material data for the internal material type.
 * @param material Renderer data describing material.
 * @param uv       The texture UV values at intersected position.
 * @return The new material data.
 */
MaterialBSDF MakeMaterialBSDF(Material material, float2 uv)
{
    MaterialBRDF materialBRDF = MakeMaterialBRDF(material, uv);

    float4 areaEmissivity = emissiveAlphaScaled(material, uv);
    float3 emissivity = areaEmissivity.xyz;

    // Get any light maps
    uint emissivityTex = asuint(areaEmissivity.w);
    if (emissivityTex != uint(-1))
    {
        // Determine texture UVs
        emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, 0.0f).xyz;
    }

    MaterialBSDF ret =
    {
        materialBRDF.albedo,
#ifndef DISABLE_SPECULAR_MATERIALS
        materialBRDF.roughnessAlpha,
        materialBRDF.F0,
        materialBRDF.roughnessAlphaSqr,
#endif
        emissivity
    };
    return ret;
}

/**
 * Calculates the material data for the internal material type.
 * @param material Renderer data describing material.
 * @param uv       The texture UV values at intersected position.
 * @param gradX    The UV partial derivatives along the X axis.
 * @param gradY    The UV partial derivatives along the Y axis.
 * @return The new material data.
 */
MaterialBSDF MakeMaterialBSDF(Material material, float2 uv, float2 gradX, float2 gradY)
{
    MaterialBRDF materialBRDF = MakeMaterialBRDF(material, uv);

    float4 areaEmissivity = emissiveAlphaScaled(material, uv);
    float3 emissivity = areaEmissivity.xyz;

    // Get any light maps
    uint emissivityTex = asuint(areaEmissivity.w);
    if (emissivityTex != uint(-1))
    {
        // Determine texture UVs
        emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleGrad(g_TextureSampler, uv, gradX, gradY).xyz;
    }

    MaterialBSDF ret =
    {
        materialBRDF.albedo,
#ifndef DISABLE_SPECULAR_MATERIALS
        materialBRDF.roughnessAlpha,
        materialBRDF.F0,
        materialBRDF.roughnessAlphaSqr,
#endif
        emissivity
    };
    return ret;
}

/**
 * Convert a BSDF material to its sub BRDF components.
 * @param material BSDF material.
 * @return The new material data.
 */
MaterialBRDF MakeMaterialBRDF(MaterialBSDF material)
{
    MaterialBRDF ret =
    {
        material.albedo,
#ifndef DISABLE_SPECULAR_MATERIALS
        material.roughnessAlpha,
        material.F0,
        material.roughnessAlphaSqr,
#endif
    };
    return ret;
}

/**
 * Packs a material into a compressed storage format.
 * @param material Evaluated material.
 * @return Packed data that can be unpacked using unpackMaterial.
 */
uint packMaterial(MaterialEvaluated material)
{
#ifdef DISABLE_SPECULAR_MATERIALS
    // Pack albedo color onto 10-10-10 format, i.e. 30 bits
    uint albedo = (uint(pow(saturate(material.albedo.x), 1.0f / 2.2f) * 1023.0f) << 20)
                | (uint(pow(saturate(material.albedo.y), 1.0f / 2.2f) * 1023.0f) << 10)
                | (uint(pow(saturate(material.albedo.z), 1.0f / 2.2f) * 1023.0f));
    return albedo;
#else
    // Pack albedo color onto 5-6-5 format, i.e. 16 bits
    uint albedo = (uint(pow(saturate(material.albedo.x), 1.0f / 2.2f) * 31.0f) << 11)
                | (uint(pow(saturate(material.albedo.y), 1.0f / 2.2f) * 63.0f) << 5)
                | (uint(pow(saturate(material.albedo.z), 1.0f / 2.2f) * 31.0f) << 0);

    // Pack metallicity and roughness onto 8 bits each
    uint metallicityRoughness = (uint(saturate(material.metallicity) * 255.0f) << 8)
                              | (uint(saturate(material.roughness) * 255.0f) << 0);

    return (albedo << 16) | metallicityRoughness;
#endif
}

/**
 * Unpacks a material from a compressed storage format.
 * @param packedMaterial The packed material created using packMaterial.
 * @return BRDF material corresponding to the unpacked parameters.
 */
MaterialBRDF unpackMaterial(in uint packedMaterial)
{
    MaterialBRDF material;

#ifdef DISABLE_SPECULAR_MATERIALS
    // Unpack the albedo
    material.albedo = float3(
        pow(((packedMaterial >> 20) & 0x3FFu) / 1023.0f, 2.2f),
        pow(((packedMaterial >> 10) & 0x3FFu) / 1023.0f, 2.2f),
        pow(((packedMaterial) & 0x3FFu) / 1023.0f, 2.2f)
    );
#else
    MaterialEvaluated material2;
    // Unpack the albedo
    uint albedo = (packedMaterial >> 16);
    material2.albedo = float3(
        pow(((albedo >> 11) & 0x1Fu) / 31.0f, 2.2f),
        pow(((albedo >> 5) & 0x3Fu) / 63.0f, 2.2f),
        pow(((albedo >> 0) & 0x1Fu) / 31.0f, 2.2f)
    );

    // Unpack the metallicity and roughness
    uint metallicityRoughness = (packedMaterial & 0xFFFFu);

    material2.metallicity = ((metallicityRoughness >> 8) & 0xFFu) / 255.0f;
    material2.roughness = ((metallicityRoughness >> 0) & 0xFFu) / 255.0f;

    material = MakeMaterialBRDF(material2);
#endif
    return material;
}

#endif // MATERIALS_HLSL
