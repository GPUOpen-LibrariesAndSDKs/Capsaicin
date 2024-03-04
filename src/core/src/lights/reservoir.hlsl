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
    // Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting - Bitterli et al
    LightSample lightSample; /**< Current light sample information */
    float M;                 /**< Confidence weight used for MIS. This value is propotional to the number of samples in the reservoir (M) */
    float W;                 /**< Weight of current sample (W) */

    /**
     * Checks whether a reservoir contains a valid sample.
     * @returns True if valid, False otherwise.
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
    reservoir.M = 0;
    reservoir.W = 0.0f;
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

/** Structure used to encapsulate reservoir RIS weigh updating functionality. */
struct ReservoirUpdater
{
    Reservoir reservoir; /**< The internal reservoir to update (do not access directly) */
    float targetPDF;     /**< The (unnormalized) target distribution of the chosen sample (S) */
};

/**
 * Creates a reservoir updater for a new reservoir.
 * @returns The new reservoir updater.
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
 * Update a reservoir by passing it a new light sample and that samples contribution.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater         Reservoir updater containing the reservoir to update.
 * @param randomNG        Random number sampler used to sample light.
 * @param lightIndex      Index of the current light to sample.
 * @param sampledLightPDF The combined PDF for the light sample.
 * @param material        Material data describing BRDF.
 * @param normal          Shading normal vector at current position (must be normalised).
 * @param viewDirection   Outgoing ray view direction (must be normalised).
 * @param radiance        Total radiance received from the light.
 * @param lightDirection  Direction towards the light (must be normalised).
 * @param sampleParams    UV values that can be used to recalculate light parameters using @sampledLightUnpack.
 */
template<typename RNG>
void updateReservoirRadiance(inout ReservoirUpdater updater, inout RNG randomNG, uint lightIndex, float sampledLightPDF, MaterialBRDF material, float3 normal, float3 viewDirection, float3 radiance, float3 lightDirection, float2 sampleParams)
{
    // Evaluate the sampling function for the new sample
    const float f = (sampledLightPDF != 0.0f) ? Reservoir_EvaluateTargetPdf(viewDirection, normal, material, lightDirection, radiance) : 0.0f;

    // Compute MIS weights
    const float misWeight1 = updater.reservoir.M / (updater.reservoir.M + 1.0f);
    const float misWeight2 = 1.0f / (updater.reservoir.M + 1.0f);

    // Compute reservoir resampling weights
    const float weight1 = misWeight1 * updater.targetPDF * updater.reservoir.W;
    const float weight2 = misWeight2 * f / max(sampledLightPDF, FLT_MIN);
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
 */
template<typename RNG>
void updateReservoir(inout ReservoirUpdater updater, inout RNG randomNG, uint lightIndex, float lightPDF, MaterialBRDF material, float3 position, float3 normal, float3 viewDirection)
{
    // Sample light
    Light light = getLight(lightIndex);
    float sampledLightPDF;
    float3 lightDirection;
    float2 sampleParams;
    float3 unused;
    float3 radiance = sampleLight(light, randomNG, position, normal, lightDirection, sampledLightPDF, unused, sampleParams);

    // Combine PDFs
    sampledLightPDF *= lightPDF;

    updateReservoirRadiance(updater, randomNG, lightIndex, sampledLightPDF, material, normal, viewDirection, radiance, lightDirection, sampleParams);
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
void updateReservoirCone(inout ReservoirUpdater updater, inout RNG randomNG, uint lightIndex, float lightPDF, MaterialBRDF material, float3 position, float3 normal, float3 viewDirection, float solidAngle)
{
    // Sample light
    Light light = getLight(lightIndex);
    float sampledLightPDF;
    float3 lightDirection;
    float2 sampleParams;
    float3 unused;
    float3 radiance = sampleLightCone(light, randomNG, position, normal, solidAngle, lightDirection, sampledLightPDF, unused, sampleParams);

    // Combine PDFs
    sampledLightPDF *= lightPDF;

    updateReservoirRadiance(updater, randomNG, lightIndex, sampledLightPDF, material, normal, viewDirection, radiance, lightDirection, sampleParams);
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
    const float f = Reservoir_EvaluateTargetPdf(viewDirection, normal, material, lightDirection, radiance);

    // Compute MIS weights.
    const float misWeight1 = updater.reservoir.M / (updater.reservoir.M + reservoir2.M);
    const float misWeight2 = reservoir2.M / (updater.reservoir.M + reservoir2.M);

    // Compute reservoir resampling weights
    const float weight1 = misWeight1 * updater.targetPDF * updater.reservoir.W;
    const float weight2 = misWeight2 * f * reservoir2.W;
    const float weightSum = weight1 + weight2;

    // Check if new sample should replace the existing sample
    if ((randomNG.rand() * weightSum) < weight2)
    {
        // Update internal values to add new sample
        updater.reservoir.lightSample = reservoir2.lightSample;
        updater.targetPDF = f;
    }

    // Update the contribution weight.        
    updater.reservoir.W = weightSum / max(updater.targetPDF, FLT_MIN);

    // Increment number of samples
    updater.reservoir.M += reservoir2.M; // This differs from the pseudocode from the original paper as that appears to be incorrect
}

/**
 * Merge 2 reservoirs together using pre-calculated target distributions.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater        Reservoir updater containing the first reservoir to update.
 * @param reservoir2     The second reservoir to add to the first.
 * @param randomNG       Random number sampler used to sample light.
 * @param pdf11          Target distribution: p_{domain1 -> domain1}(x_1).
 * @param pdf12          Target distribution: p_{domain2 -> domain1}(x_2).
 * @param pdf21          Target distribution: p_{domain1 -> domain2}(x_1).
 * @param pdf22          Target distribution: p_{domain2 -> domain2}(x_2).
 */
template<typename RNG>
void mergeReservoirsRadianceTalbotMIS(inout ReservoirUpdater updater, Reservoir reservoir2, inout RNG randomNG, float pdf11, float pdf12, float pdf21, float pdf22)
{
    // Compute Talbot MIS weights.
    const float misWeight1 = updater.reservoir.M * pdf11 / max(updater.reservoir.M * pdf11 + reservoir2.M * pdf21, FLT_MIN);
    const float misWeight2 = reservoir2.M * pdf22 / max(updater.reservoir.M * pdf12 + reservoir2.M * pdf22, FLT_MIN);

    // Compute reservoir resampling weights
    const float weight1 = misWeight1 * pdf11 * updater.reservoir.W;
    const float weight2 = misWeight2 * pdf12 * reservoir2.W;
    const float weightSum = weight1 + weight2;

    // Check if new sample should replace the existing sample
    if (randomNG.rand() * weightSum < weight2)
    {
        // Update internal values to add new sample
        updater.reservoir.lightSample = reservoir2.lightSample;
        updater.targetPDF = pdf12;
    }

    // Update the contribution weight.
    updater.reservoir.W = weightSum / max(updater.targetPDF, FLT_MIN);

    // Increment number of samples
    updater.reservoir.M += reservoir2.M;
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
    float3 radiance = evaluateLightSampled(selectedLight, position, reservoir2.lightSample.sampleParams, lightDirection, lightPosition);

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
    float3 radiance = evaluateLightConeSampled(selectedLight, position, reservoir2.lightSample.sampleParams, solidAngle, lightDirection, lightPosition);

    mergeReservoirsRadiance(updater, reservoir2, randomNG, material, normal, viewDirection, radiance, lightDirection);
}

/**
 * Merge 2 reservoirs together.
 * @tparam RNG The type of random number sampler to be used.
 * @param updater        Reservoir updater containing the first reservoir to update.
 * @param reservoir2     The second reservoir to add to the first.
 * @param randomNG       Random number sampler used to sample light.
 * @param material1      Material data describing BRDF at the current position.
 * @param position1      Current position on surface.
 * @param normal1        Shading normal vector at current position (must be normalised).
 * @param viewDirection1 Outgoing ray view direction from the current position (must be normalised).
 * @param material2      Material data describing BRDF at the current position.
 * @param position2      Candiadte sample position on surface.
 * @param normal2        Shading normal vector at the candidate sample position (must be normalised).
 * @param viewDirection2 Outgoing ray view direction from the candidate sample position (must be normalised).
 * @param solidAngle     Solid angle around view direction of visible ray cone.
 */
template<typename RNG>
void mergeReservoirsConeTalbotMIS(inout ReservoirUpdater updater, Reservoir reservoir2, inout RNG randomNG, MaterialBRDF material1, float3 position1, float3 normal1, float3 viewDirection1, MaterialBRDF material2, float3 position2, float3 normal2, float3 viewDirection2, float solidAngle)
{
    const Light selectedLight1 = getLight(updater.reservoir.lightSample.index);
    const Light selectedLight2 = getLight(reservoir2.lightSample.index);

    float3 lightDirection12;
    float3 lightPosition12;
    const float3 radiance12 = evaluateLightConeSampled(selectedLight2, position1, reservoir2.lightSample.sampleParams, solidAngle, lightDirection12, lightPosition12);

    float3 lightDirection21;
    float3 lightPosition21;
    const float3 radiance21 = evaluateLightConeSampled(selectedLight1, position2, updater.reservoir.lightSample.sampleParams, solidAngle, lightDirection21, lightPosition21);

    float3 lightDirection22;
    float3 lightPosition22;
    const float3 radiance22 = evaluateLightConeSampled(selectedLight2, position2, reservoir2.lightSample.sampleParams, solidAngle, lightDirection22, lightPosition22);

    // Evaluate the target distributions.
    const float pdf11 = updater.targetPDF;
    const float pdf12 = Reservoir_EvaluateTargetPdf(viewDirection1, normal1, material1, lightDirection12, radiance12);
    const float pdf21 = Reservoir_EvaluateTargetPdf(viewDirection2, normal2, material2, lightDirection21, radiance21);
    const float pdf22 = Reservoir_EvaluateTargetPdf(viewDirection2, normal2, material2, lightDirection22, radiance22);

    mergeReservoirsRadianceTalbotMIS(updater, reservoir2, randomNG, pdf11, pdf12, pdf21, pdf22);
}

/**
 * Packs a reservoir into a smaller storable format.
 * @param reservoir2 The reservoir to pack.
 * @returns A uint4 containing packed values.
 */
uint4 packReservoir(Reservoir reservoir)
{
    return uint4(reservoir.lightSample.index,
                 f32tof16(reservoir.lightSample.sampleParams.x) | (f32tof16(reservoir.lightSample.sampleParams.y) << 16),
                 asuint(reservoir.W),
                 f32tof16(reservoir.M));
}

/**
 * UnPacks a reservoir from a smaller storable format (created using packReservoir).
 * @param reservoirData The packed reservoir data to unpack.
 * @returns A reservoir containing the unpacked data.
 */
Reservoir unpackReservoir(uint4 reservoirData)
{
    Reservoir reservoir;
    reservoir.lightSample.index = reservoirData.x;
    reservoir.lightSample.sampleParams = float2(f16tof32(reservoirData.y & 0xFFFFu), f16tof32(reservoirData.y >> 16));
    reservoir.W = asfloat(reservoirData.z);
    reservoir.M = f16tof32(reservoirData.w & 0xFFFFu);
    return reservoir;
}
#endif
