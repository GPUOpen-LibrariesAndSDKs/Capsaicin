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

#ifndef RESERVOIR_HLSL
#define RESERVOIR_HLSL

#include "../components/light_sampler/light_sampler.hlsl"
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
    uint M;                  /**< Number of samples in the reservoir (M) */
    float W;                 /**< Weight of current sample (W) */
    float Wsum;              /**< Running sum of the weights of streamed samples (Wsum) */

    /**
     * Checks whether a reservoir contains a valid sample.
     * @returns True if valid, False otherwise.
     */
    bool isValid()
    {
        return (M > 0 && W < FLT_MAX);
    }

    /**
     * Get the source PDF for current sample.
     * @returns The source PDF value.
     */
    float getSourcePDF()
    {
        return (M > 0 && W > 0.0f) ? Wsum / (M * W) : 0.0f;
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
    reservoir.Wsum = 0.0f;
    return reservoir;

}

// Clamps the number of resampled samples on the previous reservoir to limit the temporal bias.
void Reservoir_ClampPrevious(Reservoir current_reservoir, inout Reservoir previous_reservoir)
{
    previous_reservoir.M = min(previous_reservoir.M, 5.0f * current_reservoir.M);
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
    float sourcePDF;     /**< The PDF of the chosen sample (S) */
};

/**
 * Creates a reservoir updater from an existing reservoir.
 * @param reservoir The reservoir to update.
 * @returns The new reservoir updater.
 */
ReservoirUpdater MakeReservoirUpdater(Reservoir reservoir)
{
    ReservoirUpdater ret =
    {
        reservoir,
        reservoir.getSourcePDF(),
    };
    return ret;
}

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
 * Get the updated reservoir from reservoir updater.
 * @note Retrieving the reservoir causes its RIS weights to be updated accordingly.
 * This function should be called once after all updated operations have been performed.
 * @param updater The reservoir updater to retrieve the reservoir from.
 * @returns The updated reservoir.
 */
Reservoir getUpdatedReservoir(ReservoirUpdater updater)
{
    if (updater.reservoir.M > 0)
    {
        updater.reservoir.W = (updater.sourcePDF > 0.0f ? updater.reservoir.Wsum / (updater.reservoir.M * updater.sourcePDF) : 0.0f);
    }
    return updater.reservoir;
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
void updateReservoir(inout ReservoirUpdater updater, inout RNG randomNG, uint lightIndex, float lightPDF, MaterialBRDF material, float3 position, float3 normal, float3 viewDirection, float solidAngle)
{
    // Sample light
    Light light = getLight(lightIndex);
    float sampledLightPDF;
    float3 lightDirection;
    float2 sampleParams;
    float3 unused;
    float3 radiance = sampleLightCone(light, randomNG, position, normal, solidAngle, lightDirection, sampledLightPDF, unused, sampleParams);

    // Increment number of samples
    updater.reservoir.M += 1;

    // Discard any invalid values
    if (any(radiance > 0.0f) && lightPDF != 0.0f)
    {
        // Combine PDFs
        sampledLightPDF *= lightPDF;

        // Evaluate the sampling function for the new sample
        const float f = Reservoir_EvaluateTargetPdf(viewDirection, normal, material, lightDirection, radiance);
        const float w = f / sampledLightPDF;

        // Increment the total weight of all samples in the reservoir
        updater.reservoir.Wsum += w;

        // Check if new sample should replace the existing sample
        if ((randomNG.rand() * updater.reservoir.Wsum) < w)
        {
            // Update internal values to add new sample
            updater.reservoir.lightSample.index = lightIndex;
            updater.reservoir.lightSample.sampleParams = sampleParams;
            updater.sourcePDF = f;
        }
    }
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
void mergeReservoirs(inout ReservoirUpdater updater, Reservoir reservoir2, inout RNG randomNG, MaterialBRDF material, float3 position, float3 normal, float3 viewDirection, float solidAngle)
{
    float3 lightDirection;
    const Light selectedLight = getLight(reservoir2.lightSample.index);
    float3 lightPosition;
    float3 radiance = evaluateLightConeSampled(selectedLight, position, reservoir2.lightSample.sampleParams, solidAngle, lightDirection, lightPosition);

    // Increment number of samples
    updater.reservoir.M += reservoir2.M; // This differs from the pseudocode from the original paper as that appears to be incorrect

    // Discard any invalid values
    if (any(radiance > 0.0f))
    {
        // Evaluate the sampling function for the new sample
        const float f = Reservoir_EvaluateTargetPdf(viewDirection, normal, material, lightDirection, radiance);
        const float w = f * reservoir2.W * reservoir2.M;

        // Increment the total weight of all samples in the reservoir
        updater.reservoir.Wsum += w;

        // Check if new sample should replace the existing sample
        if ((randomNG.rand() * updater.reservoir.Wsum) < w)
        {
            // Update internal values to add new sample
            updater.reservoir.lightSample.index = reservoir2.lightSample.index;
            updater.reservoir.lightSample.sampleParams = reservoir2.lightSample.sampleParams;
            updater.sourcePDF = f;
        }
    }
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
                 reservoir.M,
                 f32tof16(reservoir.W) | (f32tof16(reservoir.Wsum) << 16));
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
    reservoir.M = reservoirData.z;
    reservoir.W = f16tof32(reservoirData.w & 0xFFFFu);
    reservoir.Wsum = f16tof32(reservoirData.w >> 16);
    return reservoir;
}
#endif
