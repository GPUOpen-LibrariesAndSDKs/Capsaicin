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

#ifndef MATERIAL_SAMPLING_HLSL
#define MATERIAL_SAMPLING_HLSL

#include "material_evaluation.hlsl"
#include "../math/quaternion.hlsl"
#include "../math/color.hlsl"

/**
 * Calculate a sampled direction for the GGX BRDF using Heitz VNDF sampling.
 * @note All calculations are done in the surfaces local tangent space. Only allows
 *  for view directions in top hemisphere (N.V>0).
 * @param roughnessAlpha The GGX roughness value.
 * @param localView      Outgoing ray view direction (in local space).
 * @param samples        Random number samples used to sample BRDF.
 * @return The sampled direction in local space.
 */
float3 sampleGGXVNDFUpper(float roughnessAlpha, float3 localView, float2 samples)
{
    // A Simpler and Exact Sampling Routine for the GGX Distribution of Visible Normals - Heitz 2017

    // Stretch the view vector as if roughness==1
    float3 stretchedView = normalize(float3(roughnessAlpha * localView.xy, localView.z));
    // Create an orthonormal basis (requires that viewDirection is always above surface i.e viewDirection.geomNormal > 0))
    float3 T1 = (stretchedView.z < 0.9999) ? normalize(cross(stretchedView, float3(0.0f, 0.0f, 1.0f))) : float3(1.0f, 0.0f, 0.0f);
    float3 T2 = cross(T1, stretchedView);
    // Sample a disk with each half of the disk weighted proportionally to its projection onto stretchedView
    //  This creates polar coordinates (r, phi)
    float a = 1.0f / (1.0f + stretchedView.z);
    float r = sqrt(samples.x);
    float phi = (samples.y < a) ? samples.y / a * PI : PI + (samples.y - a) / (1.0f - a) * PI;
    float P1 = r * cos(phi);
    float P2 = r * sin(phi) * ((samples.y < a) ? 1.0f : stretchedView.z);
    // Calculate normal (defined in the stretched tangent space)
    float3 normal = P1 * T1 + P2 * T2 + sqrt(max(0.0f, 1.0f - P1 * P1 - P2 * P2)) * stretchedView;
    // Convert normal to un-stretched and normalise
    normal = normalize(float3(roughnessAlpha * normal.xy, max(0.0f, normal.z)));
    return normal;
}

/**
 * Calculate a sampled direction for the GGX BRDF using Heitz VNDF sampling of full sphere.
 * @note All calculations are done in the surfaces local tangent space.
 * @param roughnessAlpha The GGX roughness value.
 * @param localView      Outgoing ray view direction (in local space).
 * @param samples        Random number samples used to sample BRDF.
 * @return The sampled direction in local space.
 */
float3 sampleGGXVNDFFull(float roughnessAlpha, float3 localView, float2 samples)
{
    // Sampling the GGX Distribution of Visible Normals - Heitz 2018

    // Stretch the view vector as if roughness==1
    float3 stretchedView = normalize(float3(roughnessAlpha * localView.xy, localView.z));
    // Create an orthonormal basis (with special case if cross product is zero)
    float lengthSqr = dot(stretchedView.xy, stretchedView.xy);
    float3 T1 = lengthSqr > 0 ? float3(-stretchedView.y, stretchedView.x, 0.0f) * rsqrt(lengthSqr) : float3(1.0f, 0.0f, 0.0f);
    float3 T2 = cross(stretchedView, T1);
    // Sample a disk with each half of the disk weighted proportionally to its projection onto stretchedView
    //  This creates polar coordinates (r, phi)
    float r = sqrt(samples.x);
    float phi = 2.0f * PI * samples.y;
    float P1 = r * cos(phi);
    float P2 = r * sin(phi);
    float s = 0.5f * (1.0f + stretchedView.z);
    P2 = (1.0f - s) * sqrt(1.0f - P1 * P1) + s * P2;
    // Calculate normal (defined in the stretched tangent space)
    float3 normal = P1 * T1 + P2 * T2 + sqrt(max(0.0f, 1.0f - P1 * P1 - P2 * P2)) * stretchedView;
    // Convert normal to un-stretched and normalise
    normal = normalize(float3(roughnessAlpha * normal.xy, max(0.0f, normal.z)));
    return normal;
}

/**
 * Calculate a sampled direction for the GGX BRDF using spherical cap sampling.
 * @note All calculations are done in the surfaces local tangent space.
 * @param roughnessAlpha The GGX roughness value.
 * @param localView      Outgoing ray view direction (in local space).
 * @param samples        Random number samples used to sample BRDF.
 * @return The sampled direction in local space.
 */
float3 sampleGGXVNDFSphericalCap(float roughnessAlpha, float3 localView, float2 samples)
{
    // Sampling Visible GGX Normals with Spherical Caps - Jonathan Dupuy and Anis Benyoub 2023
    // https://doi.org/10.1111/cgf.14867

    // Stretch the view vector as if roughness==1
    float3 wiStd = normalize(float3(roughnessAlpha * localView.xy, localView.z));

    float phi = 2.0f * PI * samples.y;
    float z = mad(-wiStd.z, samples.x, 1.0f - samples.x);
    float sinTheta = sqrt(saturate(1.0f - z * z));
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float3 c = float3(x, y, z);
    float3 wmStd = c + wiStd;

    // Convert normal to un-stretched and normalise
    float3 wm = normalize(float3(roughnessAlpha * wmStd.xy, wmStd.z));
    return wm;
}

/**
 * Calculate a sampled direction for the GGX BRDF using bounded spherical cap sampling.
 * @note All calculations are done in the surfaces local tangent space.
 * @param roughnessAlpha The GGX roughness value.
 * @param localView      Outgoing ray view direction (in local space).
 * @param samples        Random number samples used to sample BRDF.
 * @return The sampled direction in local space.
 */
float3 sampleGGXVNDFBounded(float roughnessAlpha, float3 localView, float2 samples)
{
    // Bounded VNDF Sampling for Smith-GGX Reflections - Kenta Eto and Yusuke Tokuyoshi 2023
    // https://doi.org/10.1145/3610543.3626163

    // Stretch the view vector as if roughness==1
    float3 wiStd = normalize(float3(roughnessAlpha * localView.xy, localView.z));

    float phi = 2.0f * PI * samples.y;
    float a = roughnessAlpha; // Use a = saturate(min(roughnessAlpha.x, roughnessAlpha.y)) for anisotropic roughness.
    float s = 1.0f + sign(1.0f - a) * length(float2(localView.x, localView.y));
    float a2 = a * a;
    float s2 = s * s;
    float k = (1.0f - a2) * s2 / (s2 + a2 * localView.z * localView.z);
    float b = localView.z > 0.0f ? k * wiStd.z : wiStd.z;

    float z = mad(-b, samples.x, 1.0f - samples.x);
    float sinTheta = sqrt(saturate(1.0f - z * z));
    float x = sinTheta * cos(phi);
    float y = sinTheta * sin(phi);
    float3 c = float3(x, y, z);
    float3 wmStd = c + wiStd;

    // Convert normal to un-stretched and normalise
    float3 wm = normalize(float3(roughnessAlpha * wmStd.xy, wmStd.z));
    return wm;
}

/**
 * Calculate a random direction around a +z axis oriented hemisphere.
 * @note Uses a cosine-weighted distribution
 * @param samples Random number samples used to generate direction.
 * @return The sampled direction in local space.
 */
float3 sampleHemisphere(float2 samples)
{
    // Ray Tracing Gems - Sampling Transformations Zoo - Shirley
    float a = sqrt(samples.x);
    float b = TWO_PI * samples.y;
    return float3(a * cos(b), a * sin(b), sqrt(1.0f - samples.x));
}

/**
 * Calculate a sampled direction for the GGX BRDF.
 * @note All calculations are done in the surfaces local tangent space
 * @param roughnessAlpha The GGX roughness value.
 * @param localView      Outgoing ray view direction (in local space).
 * @param samples        Random number samples used to sample BRDF.
 * @return The sampled direction in local space.
 */
float3 sampleGGX(float roughnessAlpha, float3 localView, float2 samples)
{
    // Sample the local space micro-facet normal
    // float3 sampledNormal = sampleGGXVNDFUpper(roughnessAlpha, localView, samples);
    // float3 sampledNormal = sampleGGXVNDFFull(roughnessAlpha, localView, samples); // Use this for shading normals.
    // float3 sampledNormal = sampleGGXVNDFSphericalCap(roughnessAlpha, localView, samples);
    float3 sampledNormal = sampleGGXVNDFBounded(roughnessAlpha, localView, samples);

    // Calculate light direction
    float3 sampledLight = reflect(-localView, sampledNormal);
    return sampledLight;
}

/**
 * Calculate the BVNDF sampling PDF for given values for the GGX BRDF.
 * @param roughnessAlphaSqr The GGX roughness value squared.
 * @param dotNH             The dot product of the local normal and half vector (range [-1, 1]).
 * @return The calculated PDF.
 */
float sampleGGXVNDFPDF(float roughnessAlphaSqr, float dotNH, float dotNV)
{
    // Calculate NDF function
    float d = evaluateNDFTrowbridgeReitz(roughnessAlphaSqr, dotNH);

    float dotNV2 = saturate(dotNV * dotNV);
    float s = roughnessAlphaSqr * (1.0f - dotNV2);
    float t = sqrt(s + dotNV2);

    // Calculate the normalization factor considering backfacing shading normals.
    // [Tokuyoshi 2021 "Unbiased VNDF Sampling for Backfacing Shading Normals"]
    // https://gpuopen.com/download/publications/Unbiased_VNDF_Sampling_for_Backfacing_Shading_Normals.pdf
    // The normalization factor for the Smith-GGX VNDF is (t + dotNV) / 2.
    // But t + dotNV can have catastrophic cancellation when dotNV < 0.
    // Therefore, we avoid the catastrophic cancellation by equivarently rewriting the form as follows:
    // t + dotNV = (t + dotNV) * (t - dotNV) / (t - dotNV) = s / (t - dotNV).
    // In this implementation, we clamp dotNV for the case in abs(dotNV) > 1.
    float recipNormFactor = dotNV >= 0.0f ? t + saturate(dotNV) : s / (t + saturate(abs(dotNV)));
    return d / (2.0f * recipNormFactor);
}

/**
 * Calculate the BVNDF sampling PDF for given values for the GGX BRDF.
 * @param roughnessAlphaSqr The GGX roughness value squared.
 * @param dotNH             The dot product of the local normal and half vector (range [-1, 1]).
 * @param localView         Outgoing ray view direction (in local space).
 * @return The calculated PDF.
 */
float sampleGGXVNDFBoundedPDF(float roughnessAlphaSqr, float dotNH, float3 localView)
{
    // Kenta Eto and Yusuke Tokuyoshi. 2023. Bounded VNDF Sampling for Smith-GGX Reflections. SIGGRAPH Asia 2023 Technical Communications. https://doi.org/10.1145/3610543.3626163
    float ndf = evaluateNDFTrowbridgeReitz(roughnessAlphaSqr, dotNH);
    float roughnessAlpha = sqrt(roughnessAlphaSqr);
    float2 ai = roughnessAlpha * localView.xy;
    float len2 = dot(ai, ai);
    float t = sqrt(len2 + localView.z * localView.z);
    if (localView.z >= 0.0f)
    {
        float a = roughnessAlpha; // Use a = saturate(min(roughnessAlpha.x, roughnessAlpha.y)) for anisotropic roughness.
        float s = 1.0f + sign(1.0f - a) * length(float2(localView.x, localView.y));
        float a2 = a * a;
        float s2 = s * s;
        float k = (1.0f - a2) * s2 / (s2 + a2 * localView.z * localView.z);
        return ndf / (2.0f * (k * localView.z + t));
    }
    return ndf * (t - localView.z) / (2.0f * len2);
}

/**
 * Calculate the PDF for given values for the GGX BRDF.
 * @param roughnessAlphaSqr The GGX roughness value squared.
 * @param dotNH             The dot product of the local normal and half vector (range [-1, 1]).
 * @param dotNV             The dot product of the local normal and light direction (range [-1, 1]).
 * @param localView         Outgoing ray view direction (in local space).
 * @return The calculated PDF.
 */
float sampleGGXPDF(float roughnessAlphaSqr, float dotNH, float dotNV, float3 localView)
{
    // Can change the sampling method
    // return sampleGGXVNDFPDF(roughnessAlphaSqr, dotNH, dotNV);
    return sampleGGXVNDFBoundedPDF(roughnessAlphaSqr, dotNH, localView);
}

/**
 * Calculate the approximate direction of the specular peak.
 * @note This can be used in place of a light direction vector when calculating
 * shading half-vectors when the light direction is unknown.
 * @param normal         Shading normal vector at current position (must be normalised).
 * @param viewDirection  Outgoing ray view direction (must be normalised).
 * @param roughness      The GGX perceptual roughness (roughness = sqrt(roughnessAlpha).
 * @return The calculated direction.
 */
float3 calculateGGXSpecularDirection(float3 normal, float3 viewDirection, float roughness)
{
    // Moving Frostbite to Physically Based Rendering 3.0, page 69
    float3 reflection = reflect(-viewDirection, normal);
    float smoothness = saturate(1 - roughness);
    float lerpFactor = smoothness * (sqrt(smoothness) + roughness);
    return normalize(lerp(normal, reflection, lerpFactor));
}

/**
 * Calculate a sampled direction for the Lambert BRDF.
 * @note All calculations are done in the surfaces local tangent space
 * @param albedo  The diffuse colour term.
 * @param samples Random number samples used to sample BRDF.
 * @return The sampled direction in local space.
 */
float3 sampleLambert(float3 albedo, float2 samples)
{
    // Sample the local space uniform hemisphere
    return sampleHemisphere(samples);
}

/**
 * Calculate the PDF for given values for the Lambert BRDF.
 * @param dotNL The dot product of the local normal and view direction (range [-1, 1]).
 * @return The calculated PDF.
 */
float sampleLambertPDF(float dotNL)
{
    return saturate(dotNL) / PI; // PDF for the upper hemisphere.
}

/**
 * Calculates the probability of selecting the specular component over the diffuse component of a BRDF.
 * @param F0     The fresnel reflectance at grazing angle.
 * @param dotHV  The dot product of the half-vector and view direction (range [-1, 1]).
 * @param albedo The diffuse colour term.
 * @return The probability of selecting the specular direction.
 */
float calculateBRDFProbability(float3 F0, float dotHV, float3 albedo)
{
#ifndef DISABLE_SPECULAR_MATERIALS
    // To determine if we are sampling the diffuse or the specular component of the BRDF we need a way to
    //    weight each components contributions. To do this we use a Fresnel blend using the diffuseCompensation
    //    for the diffuse component
    float3 f = fresnel(F0, dotHV);

    // Approximate the contribution of each component using the fresnel blend
    float specular = luminance(f);
    float diffuse = luminance(albedo * diffuseCompensationTerm(f, dotHV));

    // Calculate probability of selecting specular component over the diffuse
    float probability = saturate(specular / clampMax(specular + diffuse));
#else
    float probability = 0.0f;
#endif
    return probability;
}

/**
 * Calculate the PDF for given values for the combined BRDF.
 * @param material      Material data describing BRDF.
 * @param dotNH         The dot product of the normal and half vector (range [-1, 1]).
 * @param dotNL         The dot product of the normal and light direction (range [-1, 1]).
 * @param dotHV         The dot product of the half-vector and view direction (range [-1, 1]).
 * @param dotNV         The dot product of the normal and view direction (range [-1, 1]).
 * @param localView     Outgoing ray view direction (in local space).
 * @return The calculated PDF.
 */
float sampleBRDFPDF(MaterialBRDF material, float dotNH, float dotNL, float dotHV, float dotNV, float3 localView)
{
#ifndef DISABLE_SPECULAR_MATERIALS
    float probabilityBRDF = calculateBRDFProbability(material.F0, dotHV, material.albedo);
    float pdf = lerp(sampleLambertPDF(dotNL), sampleGGXPDF(material.roughnessAlphaSqr, dotNH, dotNV, localView), probabilityBRDF);
#else
    float pdf = sampleLambertPDF(dotNL);
#endif
    return pdf;
}

/**
 * Calculate the PDF for given values for the combined BRDF when the sampling probability is already known.
 * @param material        Material data describing BRDF.
 * @param dotNH           The dot product of the normal and half vector (range [-1, 1]).
 * @param dotNL           The dot product of the normal and light direction (range [-1, 1]).
 * @param dotNV           The dot product of the normal and view direction (range [-1, 1]).
 * @param probabilityBRDF The calculated probability of selecting the specular direction.
 * @param localView       Outgoing ray view direction (in local space).
 * @return The calculated PDF.
 */
float sampleBRDFPDF2(MaterialBRDF material, float dotNH, float dotNL, float dotNV, float probabilityBRDF, float3 localView)
{
#ifndef DISABLE_SPECULAR_MATERIALS
    float pdf = lerp(sampleLambertPDF(dotNL), sampleGGXPDF(material.roughnessAlphaSqr, dotNH, dotNV, localView), probabilityBRDF);
#else
    float pdf = sampleLambertPDF(dotNL);
#endif
    return pdf;
}

/**
 * Calculate the PDF and evaluate radiance for given values for the combined BRDF.
 * @param material       Material data describing BRDF.
 * @param normal         Shading normal vector at current position.
 * @param viewDirection  Outgoing ray view direction.
 * @param lightDirection Incoming ray light direction.
 * @param reflectance    (Out) Evaluated reflectance associated with the sampled ray direction.
 * @return The calculated PDF.
 */
float sampleBRDFPDFAndEvalute(MaterialBRDF material, float3 normal, float3 viewDirection,
    float3 lightDirection, out float3 reflectance)
{
    // Evaluate BRDF for new light direction
    float dotNL = clamp(dot(normal, lightDirection), -1.0f, 1.0f);
    // Calculate half vector
    float3 halfVector = normalize(viewDirection + lightDirection);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, viewDirection));
    float dotNH = clamp(dot(normal, halfVector), -1.0f, 1.0f);
    float dotNV = clamp(dot(normal, viewDirection), -1.0f, 1.0f);
    reflectance = evaluateBRDF(material, dotHV, dotNH, dotNL, dotNV);

    // Transform the view direction into the surfaces tangent coordinate space (oriented so that z axis is aligned to normal)
    Quaternion localRotation = QuaternionRotationZ(normal);
    float3 localView = localRotation.transform(viewDirection);

    // Calculate combined PDF for current sample
    // Note: has some duplicated calculations in evaluateBRDF and sampleBRDFPDF
    float samplePDF = sampleBRDFPDF(material, dotNH, dotNL, dotHV, dotNV, localView);
    return samplePDF;
}

/**
 * Calculate the PDF and evaluate radiance for given values for the diffuse and specular BRDF components separately.
 * @param material            Material data describing BRDF.
 * @param normal              Shading normal vector at current position.
 * @param viewDirection       Outgoing ray view direction.
 * @param lightDirection      Incoming ray light direction.
 * @param reflectanceDiffuse  (Out) Evaluated diffuse reflectance associated with the sampled ray direction.
 * @param reflectanceSpecular (Out) Evaluated specular reflectance associated with the sampled ray direction.
 * @return The calculated PDF.
 */
float sampleBRDFPDFAndEvaluteSplit(MaterialBRDF material, float3 normal, float3 viewDirection,
    float3 lightDirection, out float3 reflectanceDiffuse, out float3 reflectanceSpecular)
{
    // Evaluate BRDF for new light direction
    float dotNL = clamp(dot(normal, lightDirection), -1.0f, 1.0f);
    // Calculate half vector
    float3 halfVector = normalize(viewDirection + lightDirection);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, viewDirection));
    float dotNH = clamp(dot(normal, halfVector), -1.0f, 1.0f);
    float dotNV = clamp(dot(normal, viewDirection), -1.0f, 1.0f);
    reflectanceDiffuse = evaluateBRDFDiffuse(material, dotHV, dotNL);
#ifndef DISABLE_SPECULAR_MATERIALS
    reflectanceSpecular = evaluateBRDFSpecular(material, dotHV, dotNH, dotNL, dotNV);
#else
    reflectanceSpecular = 0.0f.xxx;
#endif

    // Transform the view direction into the surfaces tangent coordinate space (oriented so that z axis is aligned to normal)
    Quaternion localRotation = QuaternionRotationZ(normal);
    float3 localView = localRotation.transform(viewDirection);

    // Calculate combined PDF for current sample
    // Note: has some duplicated calculations in evaluateBRDF and sampleBRDFPDF
    float samplePDF = sampleBRDFPDF(material, dotNH, dotNL, dotHV, dotNV, localView);
    return samplePDF;
}

/**
 * Calculates a reflected ray direction from a surface by sampling its BRDF.
 * @tparam RNG The type of random number sampler to be used.
 * @param material        Material data describing BRDF of surface.
 * @param randomNG        Random number sampler used to sample BRDF.
 * @param normal          Shading normal vector at current position.
 * @param viewDirection   Outgoing ray view direction.
 * @param reflectance     (Out) Evaluated reflectance associated with the sampled ray direction.
 * @param pdf             (Out) PDF weight associated with the sampled ray direction.
 * @param specularSampled (Out) True if the specular component was sampled, False if diffuse.
 * @return The new outgoing light ray direction.
 */
template<typename RNG>
float3 sampleBRDFType(MaterialBRDF material, inout RNG randomNG, float3 normal, float3 viewDirection,
    out float3 reflectance, out float pdf, out bool specularSampled)
{
    // Transform the view direction into the surfaces tangent coordinate space (oriented so that z axis is aligned to normal)
    Quaternion localRotation = QuaternionRotationZ(normal);
    float3 localView = localRotation.transform(viewDirection);

    // Check which BRDF component to sample
    float3 newLight;
    float2 samples = randomNG.rand2();
#ifndef DISABLE_SPECULAR_MATERIALS
    float3 specularLightDirection = calculateGGXSpecularDirection(normal, viewDirection, sqrt(material.roughnessAlpha));
    float3 specularHalfVector = normalize(viewDirection + specularLightDirection);
    // Calculate shading angles
    float specularDotHV = saturate(dot(specularHalfVector, viewDirection));
    float probabilityBRDF = calculateBRDFProbability(material.F0, specularDotHV, material.albedo);
    specularSampled = randomNG.rand() < probabilityBRDF;
    if (specularSampled)
    {
        // Sample specular BRDF component
        newLight = sampleGGX(material.roughnessAlpha, localView, samples);
    }
    else
    {
        // Sample diffuse BRDF component
        newLight = sampleLambert(material.albedo, samples);
    }
#else
    // Sample diffuse BRDF component
    newLight = sampleLambert(material.albedo, samples);
    specularSampled = false;
#endif

    // Evaluate BRDF for new light direction
    float dotNL = clamp(newLight.z, -1.0f, 1.0f);
    // Calculate half vector
    float3 halfVector = normalize(localView + newLight);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, localView));
    float dotNH = clamp(halfVector.z, -1.0f, 1.0f);
    float dotNV = clamp(localView.z, -1.0f, 1.0f);
    reflectance = evaluateBRDF(material, dotHV, dotNH, dotNL, dotNV);

    // Calculate combined PDF for current sample
    // Note: has some duplicated calculations in evaluateBRDF and sampleBRDFPDF
#ifndef DISABLE_SPECULAR_MATERIALS
    pdf = sampleBRDFPDF2(material, dotNH, dotNL, dotNV, probabilityBRDF, localView);
#else
    pdf = sampleLambertPDF(dotNL);
#endif

    // Transform the new direction back into world space
    float3 lightDirection = normalize(localRotation.inverse().transform(newLight));
    return lightDirection;
}

/**
 * Calculates a reflected ray direction from a surface by sampling its BRDF.
 * @tparam RNG The type of random number sampler to be used.
 * @param material       Material data describing BRDF of surface.
 * @param randomNG       Random number sampler used to sample BRDF.
 * @param normal         Shading normal vector at current position.
 * @param viewDirection  Outgoing ray view direction.
 * @param reflectance    (Out) Evaluated reflectance associated with the sampled ray direction.
 * @param pdf            (Out) PDF weight associated with the sampled ray direction.
 * @return The new outgoing light ray direction.
 */
template<typename RNG>
float3 sampleBRDF(MaterialBRDF material, inout RNG randomNG, float3 normal, float3 viewDirection,
    out float3 reflectance, out float pdf)
{
    bool unused;
    return sampleBRDFType(material, randomNG, normal, viewDirection, reflectance, pdf, unused);
}

/**
 * Calculates a reflected ray direction from a surface by sampling its BRDFs diffuse component.
 * @tparam RNG The type of random number sampler to be used.
 * @param material       Material data describing BRDF of surface.
 * @param randomNG       Random number sampler used to sample BRDF.
 * @param normal         Shading normal vector at current position.
 * @param viewDirection  Outgoing ray view direction.
 * @param reflectance    (Out) Evaluated reflectance associated with the sampled ray direction.
 * @param pdf            (Out) PDF weight associated with the sampled ray direction.
 * @return The new outgoing light ray direction.
 */
template<typename RNG>
float3 sampleBRDFDiffuse(MaterialBRDF material, inout RNG randomNG, float3 normal, float3 viewDirection,
    out float3 reflectance, out float pdf)
{
    // Transform the view direction into the surfaces tangent coordinate space (oriented so that z axis is aligned to normal)
    Quaternion localRotation = QuaternionRotationZ(normal);
    float3 localView = localRotation.transform(viewDirection);

    // Check which BRDF component to sample
    float3 newLight;
    float2 samples = randomNG.rand2();

    // Sample diffuse BRDF component
    newLight = sampleLambert(material.albedo, samples);

    // Evaluate BRDF for new light direction
    float dotNL = clamp(newLight.z, -1.0f, 1.0f);
    // Calculate half vector
    float3 halfVector = normalize(localView + newLight);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, localView));
    float dotNH = clamp(halfVector.z, -1.0f, 1.0f);
    float dotNV = clamp(localView.z, -1.0f, 1.0f);
    reflectance = evaluateBRDF(material, dotHV, dotNH, dotNL, dotNV);

    // Calculate combined PDF for current sample
    pdf = sampleLambertPDF(dotNL);

    // Transform the new direction back into world space
    float3 lightDirection = normalize(localRotation.inverse().transform(newLight));
    return lightDirection;
}

/**
 * Calculate the PDF and evaluate radiance for given values for the diffuse BRDF component.
 * @remark This should only be used for rays generated by sampleBRDFDiffuse as otherwise the PDF
 *  values will be incorrect. When combining diffuse+specular rays with NEE then 3 component form
 *  of MIS must be used.
 * @param material       Material data describing BRDF.
 * @param normal         Shading normal vector at current position.
 * @param viewDirection  Outgoing ray view direction.
 * @param lightDirection Incoming ray light direction.
 * @param reflectance    (Out) Evaluated reflectance associated with the sampled ray direction.
 * @return The calculated PDF.
 */
float sampleBRDFPDFAndEvaluteDiffuse(MaterialBRDF material, float3 normal, float3 viewDirection,
    float3 lightDirection, out float3 reflectance)
{
    // Evaluate BRDF for new light direction
    float dotNL = clamp(dot(normal, lightDirection), -1.0f, 1.0f);
    // Calculate half vector
    float3 halfVector = normalize(viewDirection + lightDirection);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, viewDirection));
    reflectance = evaluateBRDFDiffuse(material, dotHV, dotNL);

    // Calculate combined PDF for current sample
    // Note: has some duplicated calculations in evaluateBRDF and sampleBRDFPDF
    float samplePDF = sampleLambertPDF(dotNL);
    return samplePDF;
}

#ifndef DISABLE_SPECULAR_MATERIALS
/**
 * Calculates a reflected ray direction from a surface by sampling its BRDFs specular component.
 * @tparam RNG The type of random number sampler to be used.
 * @param material       Material data describing BRDF of surface.
 * @param randomNG       Random number sampler used to sample BRDF.
 * @param normal         Shading normal vector at current position.
 * @param viewDirection  Outgoing ray view direction.
 * @param reflectance    (Out) Evaluated reflectance associated with the sampled ray direction.
 * @param pdf            (Out) PDF weight associated with the sampled ray direction.
 * @return The new outgoing light ray direction.
 */
template<typename RNG>
float3 sampleBRDFSpecular(MaterialBRDF material, inout RNG randomNG, float3 normal, float3 viewDirection,
    out float3 reflectance, out float pdf)
{
    // Transform the view direction into the surfaces tangent coordinate space (oriented so that z axis is aligned to normal)
    Quaternion localRotation = QuaternionRotationZ(normal);
    float3 localView = localRotation.transform(viewDirection);

    // Check which BRDF component to sample
    float3 newLight;
    float2 samples = randomNG.rand2();

    // Sample specular BRDF component
    newLight = sampleGGX(material.roughnessAlpha, localView, samples);

    // Evaluate BRDF for new light direction
    float dotNL = clamp(newLight.z, -1.0f, 1.0f);
    // Calculate half vector
    float3 halfVector = normalize(localView + newLight);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, localView));
    float dotNH = clamp(halfVector.z, -1.0f, 1.0f);
    float dotNV = clamp(localView.z, -1.0f, 1.0f);
    reflectance = evaluateBRDF(material, dotHV, dotNH, dotNL, dotNV);

    // Calculate combined PDF for current sample
    // Note: has some duplicated calculations in evaluateBRDF and sampleBRDFPDF
    pdf = sampleGGXPDF(material.roughnessAlphaSqr, dotNH, dotNV, localView);

    // Transform the new direction back into world space
    float3 lightDirection = normalize(localRotation.inverse().transform(newLight));
    return lightDirection;
}

/**
 * Calculate the PDF and evaluate radiance for given values for the specular BRDF component.
 * @remark This should only be used for rays generated by sampleBRDFSpecular as otherwise the PDF
 *  values will be incorrect. When combining diffuse+specular rays with NEE then 3 component form
 *  of MIS must be used.
 * @param material       Material data describing BRDF.
 * @param normal         Shading normal vector at current position.
 * @param viewDirection  Outgoing ray view direction.
 * @param lightDirection Incoming ray light direction.
 * @param reflectance    (Out) Evaluated reflectance associated with the sampled ray direction.
 * @return The calculated PDF.
 */
float sampleBRDFPDFAndEvaluteSpecular(MaterialBRDF material, float3 normal, float3 viewDirection,
    float3 lightDirection, out float3 reflectance)
{
    // Evaluate BRDF for new light direction
    float dotNL = clamp(dot(normal, lightDirection), -1.0f, 1.0f);
    // Calculate half vector
    float3 halfVector = normalize(viewDirection + lightDirection);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, viewDirection));
    float dotNH = clamp(dot(normal, halfVector), -1.0f, 1.0f);
    float dotNV = clamp(dot(normal, viewDirection), -1.0f, 1.0f);
    reflectance = evaluateBRDFSpecular(material, dotHV, dotNH, dotNL, dotNV);

    // Transform the view direction into the surfaces tangent coordinate space (oriented so that z axis is aligned to normal)
    Quaternion localRotation = QuaternionRotationZ(normal);
    float3 localView = localRotation.transform(viewDirection);

    // Calculate combined PDF for current sample
    // Note: has some duplicated calculations in evaluateBRDFSpecular and sampleGGXPDF
    float samplePDF = sampleGGXPDF(material.roughnessAlphaSqr, dotNH, dotNV, localView);
    return samplePDF;
}
#endif // !DISABLE_SPECULAR_MATERIALS

#endif // MATERIAL_SAMPLING_HLSL
