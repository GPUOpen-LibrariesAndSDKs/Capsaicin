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

#ifndef RESERVOIR_HLSL
#define RESERVOIR_HLSL

#include "../components/light_builder/light_builder.hlsl"
#include "../lights/light_sampling.hlsl"
#include "../materials/material_evaluation.hlsl"
#include "../math/math_constants.hlsl"
#include "../math/color.hlsl"
#include "../materials/material_sampling.hlsl"

/** Structure used to store light sample information. */
struct LightSample
{
    uint index;          /**< Index of the sampled light */
    float2 sampleParams; /**< Params used to reconstruct the light sample using sampledLightUnpack */
};

/**
 * Makes a new light sample from its member variables.
 * @param index        The index of the light being sampled.
 * @param sampleParams The sample parameters for the light (as output from the sampleLight
                       function used to create the light sample).
 * @return The new light sample.
 */
LightSample MakeLightSample(uint index, float2 sampleParams)
{
    LightSample ret =
    {
        index,
        sampleParams
    };
    return ret;
}

/** Reservoir data representing reservoir resampling for lights. */
struct Reservoir
{
    // Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting - Bitterli et al.
    LightSample lightSample; /**< Current light sample information */
    float M;                 /**< Confidence weight used for MIS. This value is propotional to the number of samples in the reservoir (M) */
    float W;                 /**< Weight of current sample (W) */
    float visibility;        /**< Estimated shadow-ray visibility for the sample */

    /**
     * Checks whether a reservoir contains a valid sample.
     * @return True if valid, False otherwise.
     */
    bool isValid()
    {
        return (M > 0 && W < FLT_MAX);
    }
};

/**
 * Makes a new reservoir and initialises it to zero.
 * @return The new reservoir.
 */
Reservoir MakeReservoir()
{
    Reservoir reservoir;
    reservoir.lightSample.index = 0;
    reservoir.lightSample.sampleParams = float2(0.0f, 0.0f);
    reservoir.M = 0.0f;
    reservoir.W = 0.0f;
    reservoir.visibility = 1.0f;
    return reservoir;
}

// Clamps the number of resampled samples on the previous reservoir to limit the temporal bias.
void Reservoir_ClampPrevious(inout Reservoir previous_reservoir)
{
    previous_reservoir.M = min(previous_reservoir.M, 20.0f);
}

// Evaluates the target PDF, i.e., the luminance of the unshadowed illumination.
float Reservoir_EvaluateTargetPdf(float3 view_direction, float3 normal, MaterialBRDF material, float3 light_direction, float3 light_radiance)
{
    float3 sampleReflectance = evaluateBRDF(material, normal, view_direction, light_direction);
    return luminance(sampleReflectance * light_radiance);
}

/**
 * Evaluate the combined BRDF * cosine term, normalized.
 * @param material       Material data describing BRDF.
 * @param normal         Shading normal vector at current position (must be normalised).
 * @param viewDirection  Outgoing ray view direction (must be normalised).
 * @param lightDirection The direction to the sampled light (must be normalised).
 * @return The calculated reflectance.
 */
float3 evaluateBRDFNormalized(MaterialBRDF material, float3 normal, float3 viewDirection, float3 lightDirection)
{
    float3 diffuse_albedo = material.albedo;
    float dotNL = clamp(dot(normal, lightDirection), -1.0f, 1.0f);
#ifndef DISABLE_SPECULAR_MATERIALS
    float dotNV = clamp(dot(normal, viewDirection), -1.0f, 1.0f);

    float2 brdf_lut = g_LutBuffer.SampleLevel(g_LinearSampler, float2(dotNV, material.roughnessAlpha), 0.0f).xy;
    float3 specular_albedo = saturate(material.F0 * brdf_lut.x + (1.0f - material.F0) * brdf_lut.y);

    float albedo = luminance(diffuse_albedo) + luminance(specular_albedo);
#else
    float albedo = luminance(diffuse_albedo);
#endif
    float3 brdf = evaluateBRDF(material, normal, viewDirection, lightDirection); // BRDF * cosine term.
    return albedo > 0.0f ? brdf / albedo : saturate(dotNL) * INV_PI;
}

// Evaluates the target PDF, i.e., the luminance of the unshadowed illumination, with BRDF normalization.
float Reservoir_EvaluateTargetPdfNormalized(float3 view_direction, float3 normal, MaterialBRDF material, float3 light_direction, float3 light_radiance)
{
    float3 sampleReflectance = evaluateBRDFNormalized(material, normal, view_direction, light_direction);
    return luminance(sampleReflectance * light_radiance);
}

/** Structure used to encapsulate reservoir RIS weigh updating functionality. */
struct ReservoirUpdater
{
    Reservoir reservoir; /**< The internal reservoir to update (do not access directly) */
    float targetPDF;     /**< The (unnormalized) target distribution of the chosen sample (S) */
};

/**
 * Creates a reservoir updater for a new reservoir.
 * @return The new reservoir updater.
 */
ReservoirUpdater MakeReservoirUpdater()
{
    ReservoirUpdater ret =
    {
        MakeReservoir(),
        0.0f,
    };
    return ret;
}

/**
 * Update a reservoir by passing it a new light sample and that samples contribution with BRDF sampling.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater          Reservoir updater containing the reservoir to update.
 * @param randomNG         Random number sampler used to sample light.
 * @param lightIndex       Index of the current light to sample.
 * @param sampledLightPDF  The combined PDF for the light sample.
 * @param samplePDF        The area measure PDF of sampling the current paths direction with respect to material BRDF (Unused if not using BRDF sampling).
 * @param material         Material data describing BRDF.
 * @param normal           Shading normal vector at current position (must be normalised).
 * @param viewDirection    Outgoing ray view direction (must be normalised).
 * @param radiance         Total radiance received from the light.
 * @param lightDirection   Direction towards the light (must be normalised).
 * @param sampleParams     UV values that can be used to recalculate light parameters using @sampledLightUnpack.
 * @param numSampledLights Total number of light samples that will be added in successive updateReservoir calls.
 */
template<typename RNG>
void updateReservoirRadianceBRDF(inout ReservoirUpdater updater, inout RNG randomNG, uint lightIndex, float sampledLightPDF,
    float samplePDF, MaterialBRDF material, float3 normal, float3 viewDirection, float3 radiance, float3 lightDirection,
    float2 sampleParams, uint numSampledLights)
{
    // Evaluate the sampling function for the new sample
    const float f = Reservoir_EvaluateTargetPdfNormalized(viewDirection, normal, material, lightDirection, radiance);

    // Compute MIS weights
    const float misWeight1 = updater.reservoir.M / (updater.reservoir.M + 1.0f);
    const float misWeight2 = 1.0f / (updater.reservoir.M + 1.0f);

    // Compute reservoir resampling weights
    const float weight1 = misWeight1 * updater.targetPDF * updater.reservoir.W;
#ifdef BRDF_SAMPLING
    const float weight2 = (misWeight2 * f * (numSampledLights + 1)) / max(sampledLightPDF * numSampledLights + samplePDF, FLT_MIN);
#else //BRDF_SAMPLING
    const float weight2 = (misWeight2 * f) / max(sampledLightPDF, FLT_MIN);
#endif //BRDF_SAMPLING
    const float weightSum = weight1 + weight2;

    // Check if new sample should replace the existing sample
    if ((randomNG.rand() * weightSum) < weight2)
    {
        // Update internal values to add new sample
        updater.reservoir.lightSample.index = lightIndex;
        updater.reservoir.lightSample.sampleParams = sampleParams;
        updater.targetPDF = f;
    }

    // Update the contribution weight
    updater.reservoir.W = weightSum / max(updater.targetPDF, FLT_MIN);

    // Increment number of samples
    updater.reservoir.M += 1.0f;
}

/**
 * Update a reservoir by passing it a new light sample and that samples contribution.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater          Reservoir updater containing the reservoir to update.
 * @param randomNG         Random number sampler used to sample light.
 * @param lightIndex       Index of the current light to sample.
 * @param sampledLightPDF  The combined PDF for the light sample.
 * @param material         Material data describing BRDF.
 * @param position         Current position on surface.
 * @param normal           Shading normal vector at current position (must be normalised).
 * @param viewDirection    Outgoing ray view direction (must be normalised).
 * @param radiance         Total radiance received from the light.
 * @param lightDirection   Direction towards the light (must be normalised).
 * @param sampleParams     UV values that can be used to recalculate light parameters using @sampledLightUnpack.
 * @param numSampledLights Total number of light samples that will be added in successive updateReservoir calls.
 */
template<typename RNG>
void updateReservoirRadiance(inout ReservoirUpdater updater, inout RNG randomNG, uint lightIndex, float sampledLightPDF,
    MaterialBRDF material, float3 position, float3 normal, float3 viewDirection, float3 radiance, float3 lightDirection,
    float2 sampleParams, uint numSampledLights)
{
#ifdef BRDF_SAMPLING
    float brdfPDF =0.f;
    Light light = getLight(lightIndex);
    if (!(isDeltaLight(light)))
    {
        brdfPDF = sampleBRDFPDF(material, normal, viewDirection, lightDirection);
        if (light.get_light_type() == kLight_Area)
        {
            // Determine position on surface of light
            float3 lightPosition = interpolate(light.v1.xyz, light.v2.xyz, light.v3.xyz, sampleParams);

            // Calculate lights surface normal vector
            float3 edge1 = light.v2.xyz - light.v1.xyz;
            float3 edge2 = light.v3.xyz - light.v1.xyz;
            float3 lightNormal = normalize(cross(edge1, edge2));

            // Multiply geometry term
            float weight = distanceSqr(lightPosition, position);
            weight = (weight != 0.0F) ? saturate(abs(dot(lightNormal, lightDirection))) / weight : 0.0F;
            brdfPDF *= weight;
        }
    }
#else
    const float brdfPDF = 0.0F; // Unused so can be set to anything
#endif
    updateReservoirRadianceBRDF(updater, randomNG, lightIndex, sampledLightPDF, brdfPDF, material, normal, viewDirection, radiance, lightDirection, sampleParams, numSampledLights);
}

/**
 * Update a reservoir by passing it a new light sample.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater          Reservoir updater containing the reservoir to update.
 * @param randomNG         Random number sampler used to sample light.
 * @param lightIndex       Index of the current light to sample.
 * @param lightPDF         The PDF for the light sample.
 * @param material         Material data describing BRDF.
 * @param position         Current position on surface.
 * @param normal           Shading normal vector at current position (must be normalised).
 * @param viewDirection    Outgoing ray view direction (must be normalised).
 * @param numSampledLights Total number of light samples that will be added in successive updateReservoir calls.
 */
template<typename RNG>
void updateReservoir(inout ReservoirUpdater updater, inout RNG randomNG, uint lightIndex, float lightPDF, MaterialBRDF material, float3 position, float3 normal, float3 viewDirection, uint numSampledLights)
{
    // Sample light
    Light light = getLight(lightIndex);
    float sampledLightPDF;
    float3 lightDirection;
    float2 sampleParams;
    float3 unused;
    float3 radiance = sampleLightAM(light, randomNG, position, normal, lightDirection, sampledLightPDF, unused, sampleParams);

    // Combine PDFs
    sampledLightPDF *= lightPDF;

    updateReservoirRadiance(updater, randomNG, lightIndex, sampledLightPDF, material, position, normal, viewDirection,
        radiance, lightDirection, sampleParams, numSampledLights);
}

/**
 * Update a reservoir by passing it a new light sample.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater       Reservoir updater containing the reservoir to update.
 * @param randomNG      Random number sampler used to sample light.
 * @param lightIndex    Index of the current light to sample.
 * @param lightPDF      The PDF for the light sample.
 * @param material      Material data describing BRDF.
 * @param position      Current position on surface.
 * @param normal        Shading normal vector at current position (must be normalised).
 * @param viewDirection Outgoing ray view direction (must be normalised).
 * @param solidAngle    Solid angle around view direction of visible ray cone.
 */
template<typename RNG>
void updateReservoirCone(inout ReservoirUpdater updater, inout RNG randomNG, uint lightIndex, float lightPDF, MaterialBRDF material, float3 position, float3 normal, float3 viewDirection, float solidAngle, uint numSampledLights)
{
    // Sample light
    Light light = getLight(lightIndex);
    float sampledLightPDF;
    float3 lightDirection;
    float2 sampleParams;
    float3 unused;
    float3 radiance = sampleLightConeAM(light, randomNG, position, normal, solidAngle, lightDirection, sampledLightPDF, unused, sampleParams);

    // Combine PDFs
    sampledLightPDF *= lightPDF;

    updateReservoirRadiance(updater, randomNG, lightIndex, sampledLightPDF, material, position, normal, viewDirection,
        radiance, lightDirection, sampleParams, numSampledLights);
}

struct ResamplingWeights
{
    float weight1; // Weight for the first reservoir.
    float weight2; // Weight for the second reservoir.
};

/**
 * Resampling from 2 reservoirs.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater        Reservoir updater containing the first reservoir to update.
 * @param reservoir2     The second reservoir to add to the first.
 * @param randomNG       Random number sampler used to sample light.
 * @param weights        Resampling weights.
 * @param pdf12          Shifted target distribution: p_{domain2 -> domain1}(x_2).
 */
template<typename RNG>
void resampleReservoir(inout ReservoirUpdater updater, const Reservoir reservoir2, inout RNG randomNG, const ResamplingWeights weights, const float pdf12)
{
    const float weightSum = weights.weight1 + weights.weight2;

    // Check if new sample should replace the existing sample
    if (randomNG.rand() * weightSum < weights.weight2)
    {
        // Update internal values to add new sample
        updater.reservoir.lightSample = reservoir2.lightSample;
        updater.targetPDF = pdf12;
    }

    //Update visibility
    updater.reservoir.visibility = saturate(max(updater.reservoir.visibility * weights.weight1 + reservoir2.visibility * weights.weight2, FLT_MIN) / max(weightSum, FLT_MIN));

    // Update the contribution weight.
    updater.reservoir.W = weightSum / max(updater.targetPDF, FLT_MIN);

    // Increment number of samples
    updater.reservoir.M += reservoir2.M;
}

/**
 * Merge 2 reservoirs together using pre-calculated light contribution.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater        Reservoir updater containing the first reservoir to update.
 * @param reservoir2     The second reservoir to add to the first.
 * @param randomNG       Random number sampler used to sample light.
 * @param material       Material data describing BRDF.
 * @param normal         Shading normal vector at current position (must be normalised).
 * @param viewDirection  Outgoing ray view direction (must be normalised).
 * @param radiance       Total radiance received from the light.
 * @param lightDirection Direction towards the light (must be normalised).
 */
template<typename RNG>
void mergeReservoirsRadiance(inout ReservoirUpdater updater, Reservoir reservoir2, inout RNG randomNG, MaterialBRDF material, float3 normal, float3 viewDirection, float3 radiance, float3 lightDirection)
{
    // Evaluate the sampling function for the new sample
    const float f = Reservoir_EvaluateTargetPdfNormalized(viewDirection, normal, material, lightDirection, radiance);

    // Compute MIS weights.
    const float misWeight1 = updater.reservoir.M / max(updater.reservoir.M + reservoir2.M, FLT_MIN);
    const float misWeight2 = reservoir2.M / max(updater.reservoir.M + reservoir2.M, FLT_MIN);

    // Compute reservoir resampling weights
    ResamplingWeights weights;
    weights.weight1 = misWeight1 * updater.targetPDF * updater.reservoir.W;
    weights.weight2 = misWeight2 * f * reservoir2.W;

    resampleReservoir(updater, reservoir2, randomNG, weights, f);
}

struct ShiftedTargetPDFs
{
    float pdf11; // p_{domain1 -> domain1}(x_1)
    float pdf12; // p_{domain2 -> domain1}(x_2)
    float pdf21; // p_{domain1 -> domain2}(x_1)
    float pdf22; // p_{domain2 -> domain2}(x_2)
};

/**
 * Calculate resampling weights using Talbot MIS.
 * @param reservoir1     The first reservoir (i.e., canonical sample).
 * @param reservoir2     The second reservoir to add to the first.
 * @param pdfs           Shifted target distributions.
 * @return               Two resampling weights.
 */
ResamplingWeights calculateResamplingWeightsTalbotMIS(const Reservoir reservoir1, const Reservoir reservoir2, const ShiftedTargetPDFs pdfs)
{
    // Compute Talbot MIS weights.
    const float misWeight1 = reservoir1.M * pdfs.pdf11 / max(reservoir1.M * pdfs.pdf11 + reservoir2.M * pdfs.pdf21, FLT_MIN);
    const float misWeight2 = reservoir2.M * pdfs.pdf22 / max(reservoir1.M * pdfs.pdf12 + reservoir2.M * pdfs.pdf22, FLT_MIN);

    // Compute reservoir resampling weights
    ResamplingWeights weights;
    weights.weight1 = misWeight1 * pdfs.pdf11 * reservoir1.W;
    weights.weight2 = misWeight2 * pdfs.pdf12 * reservoir2.W;

    return weights;
}

/**
 * Merge 2 reservoirs together.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater       Reservoir updater containing the first reservoir to update.
 * @param reservoir2    The second reservoir to add to the first.
 * @param randomNG      Random number sampler used to sample light.
 * @param material      Material data describing BRDF.
 * @param position      Current position on surface.
 * @param normal        Shading normal vector at current position (must be normalised).
 * @param viewDirection Outgoing ray view direction (must be normalised).
 */
template<typename RNG>
void mergeReservoirs(inout ReservoirUpdater updater, Reservoir reservoir2, inout RNG randomNG, MaterialBRDF material, float3 position, float3 normal, float3 viewDirection)
{
    float3 lightDirection;
    const Light selectedLight = getLight(reservoir2.lightSample.index);
    float3 lightPosition;
    float3 radiance = evaluateLightSampledAM(selectedLight, position, normal, reservoir2.lightSample.sampleParams, lightDirection, lightPosition);

    mergeReservoirsRadiance(updater, reservoir2, randomNG, material, normal, viewDirection, radiance, lightDirection);
}

/**
 * Merge 2 reservoirs together.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater       Reservoir updater containing the first reservoir to update.
 * @param reservoir2    The second reservoir to add to the first.
 * @param randomNG      Random number sampler used to sample light.
 * @param material      Material data describing BRDF.
 * @param position      Current position on surface.
 * @param normal        Shading normal vector at current position (must be normalised).
 * @param viewDirection Outgoing ray view direction (must be normalised).
 * @param solidAngle    Solid angle around view direction of visible ray cone.
 */
template<typename RNG>
void mergeReservoirsCone(inout ReservoirUpdater updater, Reservoir reservoir2, inout RNG randomNG, MaterialBRDF material, float3 position, float3 normal, float3 viewDirection, float solidAngle)
{
    float3 lightDirection;
    const Light selectedLight = getLight(reservoir2.lightSample.index);
    float3 lightPosition;
    float3 radiance = evaluateLightConeSampledAM(selectedLight, position, normal, reservoir2.lightSample.sampleParams, solidAngle, lightDirection, lightPosition);
    mergeReservoirsRadiance(updater, reservoir2, randomNG, material, normal, viewDirection, radiance, lightDirection);
}

/**
 * Shift target distribution between two domains.
 * @param updater          Reservoir updater containing the first reservoir to update.
 * @param reservoir2       The second reservoir to add to the first.
 * @param material1        Material data describing BRDF at the current position.
 * @param position1        Current position on surface.
 * @param normal1          Shading normal vector at current position (must be normalised).
 * @param viewDirection1   Outgoing ray view direction from the current position (must be normalised).
 * @param material2        Material data describing BRDF at the current position.
 * @param position2        Candiadte sample position on surface.
 * @param normal2          Shading normal vector at the candidate sample position (must be normalised).
 * @param viewDirection2   Outgoing ray view direction from the candidate sample position (must be normalised).
 * @param usePreviousLight Tells if the previous light buffer must be used for the second reservoir reservoir2.
 * @return                 Shifted target distributions.
 */
ShiftedTargetPDFs shiftTargetPDFs(const ReservoirUpdater updater, const Reservoir reservoir2, const MaterialBRDF material1, const float3 position1, const float3 normal1, const float3 viewDirection1, const MaterialBRDF material2, const float3 position2, const float3 normal2, const float3 viewDirection2, bool usePreviousLight)
{
    const Light selectedLight1 = getLight(updater.reservoir.lightSample.index);
    const Light selectedLight2 = getLight(reservoir2.lightSample.index);

    Light previousSelectedLight1 = selectedLight1;
    Light previousSelectedLight2 = selectedLight2;

#ifdef ENABLE_PREVIOUS_LIGHTS
    if (usePreviousLight)
    {
        previousSelectedLight1 = getPreviousLight(updater.reservoir.lightSample.index);
        previousSelectedLight2 = getPreviousLight(reservoir2.lightSample.index);
    }
#endif

    float3 lightDirection12;
    float3 lightPosition12;
    const float3 radiance12 = evaluateLightSampledAM(selectedLight2, position1, normal1, reservoir2.lightSample.sampleParams, lightDirection12, lightPosition12);

    float3 lightDirection21;
    float3 lightPosition21;
    const float3 radiance21 = evaluateLightSampledAM(previousSelectedLight1, position2, normal2, updater.reservoir.lightSample.sampleParams, lightDirection21, lightPosition21);

    float3 lightDirection22;
    float3 lightPosition22;
    const float3 radiance22 = evaluateLightSampledAM(previousSelectedLight2, position2, normal2, reservoir2.lightSample.sampleParams, lightDirection22, lightPosition22);

    // Evaluate the target distributions.
    ShiftedTargetPDFs pdfs;
    pdfs.pdf11 = updater.targetPDF;
    pdfs.pdf12 = Reservoir_EvaluateTargetPdfNormalized(viewDirection1, normal1, material1, lightDirection12, radiance12);
    pdfs.pdf21 = Reservoir_EvaluateTargetPdfNormalized(viewDirection2, normal2, material2, lightDirection21, radiance21);
    pdfs.pdf22 = Reservoir_EvaluateTargetPdfNormalized(viewDirection2, normal2, material2, lightDirection22, radiance22);

    return pdfs;
}

/**
 * Merge 2 reservoirs together.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater          Reservoir updater containing the first reservoir to update.
 * @param reservoir2       The second reservoir to add to the first.
 * @param randomNG         Random number sampler used to sample light.
 * @param material1        Material data describing BRDF at the current position.
 * @param position1        Current position on surface.
 * @param normal1          Shading normal vector at current position (must be normalised).
 * @param viewDirection1   Outgoing ray view direction from the current position (must be normalised).
 * @param material2        Material data describing BRDF at the current position.
 * @param position2        Candiadte sample position on surface.
 * @param normal2          Shading normal vector at the candidate sample position (must be normalised).
 * @param viewDirection2   Outgoing ray view direction from the candidate sample position (must be normalised).
 * @param usePreviousLight Tells if the previous light buffer must be used for the second reservoir reservoir2.
 */
template<typename RNG>
void mergeReservoirsTalbotMIS(inout ReservoirUpdater updater, Reservoir reservoir2, inout RNG randomNG, MaterialBRDF material1, float3 position1, float3 normal1, float3 viewDirection1, MaterialBRDF material2, float3 position2, float3 normal2, float3 viewDirection2, bool usePreviousLight)
{
    const ShiftedTargetPDFs pdfs = shiftTargetPDFs(updater, reservoir2, material1, position1, normal1, viewDirection1, material2, position2, normal2, viewDirection2, usePreviousLight);
    const ResamplingWeights weights = calculateResamplingWeightsTalbotMIS(updater.reservoir, reservoir2, pdfs);
    resampleReservoir(updater, reservoir2, randomNG, weights, pdfs.pdf12);
}

/**
 * Packs a reservoir into a smaller storable format.
 * @param reservoir2 The reservoir to pack.
 * @return A uint4 containing packed values.
 */
uint4 packReservoir(Reservoir reservoir)
{
    return uint4(reservoir.lightSample.index,
                 f32tof16(reservoir.lightSample.sampleParams.x) | (f32tof16(reservoir.lightSample.sampleParams.y) << 16),
                 asuint(reservoir.W),
                 f32tof16(reservoir.M) | (f32tof16(reservoir.visibility) << 16));
}

/**
 * UnPacks a reservoir from a smaller storable format (created using packReservoir).
 * @param reservoirData The packed reservoir data to unpack.
 * @return A reservoir containing the unpacked data.
 */
Reservoir unpackReservoir(uint4 reservoirData)
{
    Reservoir reservoir;
    reservoir.lightSample.index = reservoirData.x;
    reservoir.lightSample.sampleParams = float2(f16tof32(reservoirData.y & 0xFFFFu), f16tof32(reservoirData.y >> 16));
    reservoir.W = asfloat(reservoirData.z);
    reservoir.M = f16tof32(reservoirData.w & 0xFFFFu);
    reservoir.visibility = f16tof32(reservoirData.w >> 16);
    return reservoir;
}
#endif
