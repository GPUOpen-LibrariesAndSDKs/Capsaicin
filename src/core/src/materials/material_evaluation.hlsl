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

#ifndef MATERIAL_EVALUATION_HLSL
#define MATERIAL_EVALUATION_HLSL

#include "materials.hlsl"
#include "../math/math_constants.hlsl"

/**
 * Calculates schlick fresnel term.
 * @param F0    The fresnel reflectance an grazing angle.
 * @param dotHV The dot product of the half-vector and view direction (range [-1, 1]).
 * @return The calculated fresnel term.
 */
float3 fresnel(float3 F0, float dotHV)
{
    // The half-vector may be incorrectly flipped or invisible to the view direction in some cases, and thus
    // dotHV may be negative. For this case, we use abs(dotHV) to correct flipping and avoid NaN.
    float3 F90 = 1.0f.xxx;
    return F0 + (F90 - F0) * pow(1.0f - saturate(abs(dotHV)), 5.0f);
}

/**
 * Calculates the amount to modify the diffuse component of a combined BRDF.
 * @param f     Pre-calculated fresnel value.
 * @param dotHV The dot product of the half-vector and view direction (range [-1, 1]).
 * @return The amount to modify diffuse component by.
 */
float3 diffuseCompensationTerm(float3 f, float dotHV)
{
    // PBR Diffuse Lighting for GGX + Smith Microsurfaces - Hammon 2017

    // The half-vector may be incorrectly flipped or invisible to the view direction in some cases, and thus
    // dotHV may be negative. For this case, we use abs(dotHV) to correct flipping and avoid NaN.
    return (1.0f.xxx - f) * 1.05 * (1.0f - pow(1.0f - saturate(abs(dotHV)), 5.0f));
}

/**
 * Calculates the amount to modify the diffuse component of a combined BRDF.
 * @param F0    The fresnel reflectance at grazing angle.
 * @param dotHV The dot product of the half-vector and view direction (range [-1, 1]).
 * @return The amount to modify diffuse component by.
 */
float3 diffuseCompensation(float3 F0, float dotHV)
{
    return diffuseCompensationTerm(fresnel(F0, dotHV), dotHV);
}

/**
 * Evaluate the Trowbridge-Reitz Normal Distribution Function.
 * @param roughnessAlphaSqr The NDF roughness value squared.
 * @param dotNH             The dot product of the normal and half vector (range [-1, 1]).
 * @return The calculated NDF value.
 */
float evaluateNDFTrowbridgeReitz(float roughnessAlphaSqr, float dotNH)
{
    // Heaviside function for microfacet normal in the upper hemisphere.
    if (dotNH < 0.0f)
    {
        return 0.0f;
    }

    float denom = dotNH * dotNH * (roughnessAlphaSqr - 1.0f) + 1.0f;
    float d = roughnessAlphaSqr / (PI * denom * denom);
    return d;
}

/**
 * Evaluate the GGX Visibility function.
 * @param roughnessAlphaSqr The GGX roughness value squared.
 * @param dotNL             The dot product of the normal and light direction (range [-1, 1]).
 * @param dotNV             The dot product of the normal and view direction (range [-1, 1]).
 * @return The calculated visibility value.
 */
float evaluateVisibilityGGX(float roughnessAlphaSqr, float dotNL, float dotNV)
{
    // The masking-shadowing function is indefinite for back-facing shading normals.
    // So we use abs(dotNL) and abs(dotNV) for this case.
    // This is hacky, but still satisfies the reciprocity and energy conservation.
    float rMod = 1.0f - roughnessAlphaSqr;
    float recipG1 = abs(dotNL) + sqrt(roughnessAlphaSqr + (rMod * dotNL * dotNL));
    float recipG2 = abs(dotNV) + sqrt(roughnessAlphaSqr + (rMod * dotNV * dotNV));
    float recipV = recipG1 * recipG2;
    return recipV;
}

/**
 * Evaluate the GGX BRDF.
 * @param roughnessAlpha    The GGX roughness value.
 * @param roughnessAlphaSqr The GGX roughness value squared.
 * @param F0                The fresnel reflectance at grazing angle.
 * @param dotHV             The dot product of the half-vector and view direction (range [-1, 1]).
 * @param dotNH             The dot product of the normal and half vector (range [-1, 1]).
 * @param dotNL             The dot product of the normal and light direction (range [-1, 1]).
 * @param dotNV             The dot product of the normal and view direction (range [-1, 1]).
 * @param fresnelOut        (Out) The returned fresnel value.
 * @return The calculated reflectance.
 */
float3 evaluateGGX(float roughnessAlpha, float roughnessAlphaSqr, float3 F0, float dotHV, float dotNH, float dotNL, float dotNV, out float3 fOut)
{
    // Calculate Fresnel
    fOut = fresnel(F0, dotHV);

    // Calculate Trowbridge-Reitz Distribution function
    float d = evaluateNDFTrowbridgeReitz(roughnessAlphaSqr, dotNH);

    // Calculate GGX Visibility function
    float recipV = evaluateVisibilityGGX(roughnessAlphaSqr, dotNL, dotNV);

    return (fOut * d) / recipV;
}

/**
 * Evaluate the Lambert BRDF.
 * @param albedo The diffuse colour term.
 * @return The calculated reflectance.
 */
float3 evaluateLambert(float3 albedo)
{
    return albedo / PI;
}

/**
 * Evaluate the combined BRDF.
 * @param material Material data describing BRDF.
 * @param dotHV    The dot product of the half-vector and view direction (range [-1, 1]).
 * @param dotNH    The dot product of the normal and half vector (range [-1, 1]).
 * @param dotNL    The dot product of the normal and light direction (range [-1, 1]).
 * @param dotNV    The dot product of the normal and view direction (range [-1, 1]).
 * @return The calculated reflectance.
 */
float3 evaluateBRDF(MaterialBRDF material, float dotHV, float dotNH, float dotNL, float dotNV)
{
    // Calculate diffuse component
    float3 diffuse = evaluateLambert(material.albedo);

#ifndef DISABLE_SPECULAR_MATERIALS
    // Calculate specular component
    float3 f;
    float3 specular = evaluateGGX(material.roughnessAlpha, material.roughnessAlphaSqr, material.F0, dotHV, dotNH, dotNL, dotNV, f);

    // Add the weight of the diffuse compensation term
    diffuse *= diffuseCompensationTerm(f, dotHV);
    float3 brdf = (specular + diffuse) * saturate(dotNL); // saturate(dotNL) = abs(dotNL) * Heaviside function for the upper hemisphere.
#else
    // Add the weight of the diffuse compensation term to prevent excessive brightness compared to specular
    diffuse *= diffuseCompensation(fresnel(0.04f.xxx, dotHV), dotHV);
    float3 brdf = diffuse * saturate(dotNL);
#endif
    return brdf;
}

/**
 * Evaluate the diffuse component of the BRDF.
 * @param material Material data describing BRDF.
 * @param dotHV    The dot product of the half-vector and view direction (range [-1, 1]).
 * @param dotNL    The dot product of the normal and light direction (range [-1, 1]).
 * @return The calculated reflectance.
 */
float3 evaluateBRDFDiffuse(MaterialBRDF material, float dotHV, float dotNL)
{
    // Calculate diffuse component
    float3 diffuse = evaluateLambert(material.albedo);

    // Add the weight of the diffuse compensation term to prevent excessive brightness compared to specular
    diffuse *= diffuseCompensation(fresnel(0.04f.xxx, dotHV), dotHV);
    float3 brdf = diffuse * saturate(dotNL);
    return brdf;
}

#ifndef DISABLE_SPECULAR_MATERIALS
/**
 * Evaluate the specular component of the BRDF.
 * @param material Material data describing BRDF.
 * @param dotNH    The dot product of the normal and half vector (range [-1, 1]).
 * @param dotNL    The dot product of the normal and light direction (range [-1, 1]).
 * @param dotHV    The dot product of the half-vector and view direction (range [-1, 1]).
 * @param dotNV    The dot product of the normal and view direction (range [-1, 1]).
 * @return The calculated reflectance.
 */
float3 evaluateBRDFSpecular(MaterialBRDF material, float dotHV, float dotNH, float dotNL, float dotNV)
{
    // Calculate specular component
    float3 f;
    float3 specular = evaluateGGX(material.roughnessAlpha, material.roughnessAlphaSqr, material.F0, dotHV, dotNH, dotNL, dotNV, f);

    float3 brdf = specular * saturate(dotNL); // saturate(dotNL) = abs(dotNL) * Heaviside function for the upper hemisphere.
    return brdf;
}
#endif

/**
 * Evaluate the combined BRDF.
 * @param material       Material data describing BRDF.
 * @param normal         Shading normal vector at current position (must be normalised).
 * @param viewDirection  Outgoing ray view direction (must be normalised).
 * @param lightDirection The direction to the sampled light (must be normalised).
 * @return The calculated reflectance.
 */
float3 evaluateBRDF(MaterialBRDF material, float3 normal, float3 viewDirection, float3 lightDirection, out float3 diffuse, out float3 specular)
{
    // Calculate diffuse component
    diffuse = evaluateLambert(material.albedo);

    // Calculate shading angles
    float dotNL = clamp(dot(normal, lightDirection), -1.0f, 1.0f);
    // Calculate half vector
    float3 halfVector = normalize(viewDirection + lightDirection);
    float dotHV = saturate(dot(halfVector, viewDirection));
#ifndef DISABLE_SPECULAR_MATERIALS
    float dotNH = clamp(dot(normal, halfVector), -1.0f, 1.0f);
    float dotNV = clamp(dot(normal, viewDirection), -1.0f, 1.0f);

    // Calculate specular component
    float3 f;
    specular = evaluateGGX(material.roughnessAlpha, material.roughnessAlphaSqr, material.F0, dotHV, dotNH, dotNL, dotNV, f);

    // Add the weight of the diffuse compensation term
    specular *= saturate(dotNL);    // saturate(dotNL) = abs(dotNL) * Heaviside function for the upper hemisphere.
    diffuse *= saturate(dotNL);
    diffuse *= diffuseCompensationTerm(f, dotHV);
    float3 brdf = specular + diffuse;
#else
    // Add the weight of the diffuse compensation term to prevent excessive brightness compared to specular
    diffuse *= diffuseCompensation(0.04f.xxx, dotHV);
    specular = 0.f;
    float3 brdf = diffuse * saturate(dotNL);
#endif
    return brdf;
}

/**
 * Evaluate the combined BRDF.
 * @param material       Material data describing BRDF.
 * @param normal         Shading normal vector at current position (must be normalised).
 * @param viewDirection  Outgoing ray view direction (must be normalised).
 * @param lightDirection The direction to the sampled light (must be normalised).
 * @return The calculated reflectance.
 */
float3 evaluateBRDF(MaterialBRDF material, float3 normal, float3 viewDirection, float3 lightDirection)
{
    float3 diffuse;
    float3 specular;
    return evaluateBRDF(material, normal, viewDirection, lightDirection, diffuse, specular);
}

#endif // MATERIAL_EVALUATION_HLSL
