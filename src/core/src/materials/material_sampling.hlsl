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

#ifndef MATERIAL_SAMPLING_HLSL
#define MATERIAL_SAMPLING_HLSL

#include "material_evaluation.hlsl"
#include "../math/quaternion.hlsl"
#include "../math/color.hlsl"

// Allows for view directions in top hemisphere (N.V >0)
float3 sampleGGXVNDFUpper(float roughnessAlpha, float3 viewDirection, float2 samples)
{
    // A Simpler and Exact Sampling Routine for the GGX Distribution of Visible Normals - Heitz 2017
    // Stretch the view vector as if roughness==1
    float3 stretchedView = normalize(float3(roughnessAlpha * viewDirection.xy, viewDirection.z));
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

// Allows for view directions in complete sphere
float3 sampleGGXVNDFFull(float roughnessAlpha, float3 viewDirection, float2 samples)
{
    // Sampling the GGX Distribution of Visible Normals - Heitz 2018
    // Stretch the view vector as if roughness==1
    float3 stretchedView = normalize(float3(roughnessAlpha * viewDirection.xy, viewDirection.z));
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
 * Calculate the sampling direction for a GGX BRDF.
 * @param roughnessAlpha The GGX roughness value.
 * @param viewDirection  Outgoing ray view direction (in local space).
 * @param samples        Random number samples used to sample BRDF.
 * @return The sampled direction in local space.
 */
float3 sampleGGXVNDF(float roughnessAlpha, float3 viewDirection, float2 samples)
{
    //Can change between Upper or Full
    return sampleGGXVNDFUpper(roughnessAlpha, viewDirection, samples);
    //return sampleGGXVNDFFull(roughnessAlpha, viewDirection, samples);
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
 * @param viewDirection  Outgoing ray view direction (in local space).
 * @param samples        Random number samples used to sample BRDF.
 * @return The sampled direction in local space.
 */
float3 sampleGGX(float roughnessAlpha, float3 viewDirection, float2 samples)
{
    // Sample the local space micro-facet normal
    float3 sampledNormal = sampleGGXVNDF(roughnessAlpha, viewDirection, samples);

    // Calculate light direction
    float3 sampledLight = reflect(-viewDirection, sampledNormal);
    return sampledLight;
}

/**
 * Calculate the PDF for given values for the GGX BRDF.
 * @param roughnessAlphaSqr The GGX roughness value squared.
 * @param dotNH             The dot product of the local normal and half vector.
 * @param dotNL             The dot product of the local normal and light direction.
 * @return The calculated PDF.
 */
float sampleGGXPDF(float roughnessAlphaSqr, float dotNH, float dotNV)
{
    // PDF of VNDF distribution is G1(V)D(H)/4N.V

    // Calculate NDF function
    float d = evaluateNDFTrowbridgeReitz(roughnessAlphaSqr, dotNH);

    // Calculate GGX Visibility G1 function
    float recipG1 = sqrt(roughnessAlphaSqr + ((1.0f - roughnessAlphaSqr) * dotNV * dotNV)) + dotNV;
    return d / (2.0f * recipG1);
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
 * @param dotNL The dot product of the local normal and view direction.
 * @return The calculated PDF.
 */
float sampleLambertPDF(float dotNL)
{
    return dotNL / PI;
}

/**
 * Calculates the probability of selecting the specular component over the diffuse component of a BRDF.
 * @param F0     The fresnel reflectance at grazing angle.
 * @param dotHV  The dot product of the half-vector and view direction.
 * @param albedo The diffuse colour term.
 * @return The probability of selecting the specular direction.
 */
float calculateBRDFProbability(float3 F0, float dotHV, float3 albedo)
{
#ifndef DISABLE_SPECULAR_LIGHTING
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
 * @param material Material data describing BRDF.
 * @param dotNH    The dot product of the normal and half vector.
 * @param dotNL    The dot product of the normal and light direction.
 * @param dotHV    The dot product of the half-vector and view direction.
 * @param dotNV    The dot product of the normal and view direction.
 * @return The calculated PDF.
 */
float sampleBRDFPDF(MaterialBRDF material, float dotNH, float dotNL, float dotHV, float dotNV)
{
#ifndef DISABLE_SPECULAR_LIGHTING
    float probabilityBRDF = calculateBRDFProbability(material.F0, dotHV, material.albedo);
    float pdf = lerp(sampleLambertPDF(dotNL), sampleGGXPDF(material.roughnessAlphaSqr, dotNH, dotNV), probabilityBRDF);
#else
    float pdf = sampleLambertPDF(dotNL);
#endif
    return pdf;
}

/**
 * Calculate the PDF for given values for the combined BRDF when the sampling probability is already known.
 * @param material        Material data describing BRDF.
 * @param dotNH           The dot product of the normal and half vector.
 * @param dotNL           The dot product of the normal and light direction.
 * @param dotNV           The dot product of the normal and view direction.
 * @param probabilityBRDF The calculated probability of selecting the specular direction.
 * @return The calculated PDF.
 */
float sampleBRDFPDF2(MaterialBRDF material, float dotNH, float dotNL, float dotNV, float probabilityBRDF)
{
#ifndef DISABLE_SPECULAR_LIGHTING
    float pdf = lerp(sampleLambertPDF(dotNL), sampleGGXPDF(material.roughnessAlphaSqr, dotNH, dotNV), probabilityBRDF);
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
    float dotNL = saturate(dot(normal, lightDirection));
    // Calculate half vector
    float3 halfVector = normalize(viewDirection + lightDirection);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, viewDirection));
    float dotNH = saturate(dot(normal, halfVector));
    float dotNV = saturate(dot(normal, viewDirection));
    reflectance = evaluateBRDF(material, dotHV, dotNH, dotNL, dotNV);

    // Calculate combined PDF for current sample
    // Note: has some duplicated calculations in evaluateBRDF and sampleBRDFPDF
    float samplePDF = sampleBRDFPDF(material, dotNH, dotNL, dotHV, dotNV);
    return samplePDF;

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
    // Transform the view direction into the surfaces tangent coordinate space (oriented so that z axis is aligned to normal)
    Quaternion localRotation = QuaternionRotationZ(normal);
    float3 localView = localRotation.transform(viewDirection);

    // Check which BRDF component to sample
    float3 newLight;
    float2 samples = randomNG.rand2();
#ifndef DISABLE_SPECULAR_LIGHTING
    float3 specularLightDirection = calculateGGXSpecularDirection(normal, viewDirection, sqrt(material.roughnessAlpha));
    float3 specularHalfVector = normalize(viewDirection + specularLightDirection);
    // Calculate shading angles
    float specularDotHV = saturate(dot(specularHalfVector, viewDirection));
    float probabilityBRDF = calculateBRDFProbability(material.F0, specularDotHV, material.albedo);
    if (randomNG.rand() < probabilityBRDF)
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
#endif

    // Evaluate BRDF for new light direction
    float dotNL = saturate(newLight.z);
    // Calculate half vector
    float3 halfVector = normalize(localView + newLight);
    // Calculate shading angles
    float dotHV = saturate(dot(halfVector, localView));
    float dotNH = saturate(halfVector.z);
    float dotNV = saturate(localView.z);
    reflectance = evaluateBRDF(material, dotHV, dotNH, dotNL, dotNV);

    // Calculate combined PDF for current sample
    // Note: has some duplicated calculations in evaluateBRDF and sampleBRDFPDF
#ifndef DISABLE_SPECULAR_LIGHTING
    pdf = sampleBRDFPDF2(material, dotNH, dotNL, dotNV, probabilityBRDF);
#else
    pdf = sampleLambertPDF(dotNL);
#endif

    // Transform the new direction back into world space
    float3 lightDirection = normalize(localRotation.inverse().transform(newLight));
    return lightDirection;
}

#endif // MATERIAL_SAMPLING_HLSL
