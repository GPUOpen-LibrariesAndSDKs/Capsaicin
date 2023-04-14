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

#ifndef LIGHT_BOUNDS_SAMPLER_HLSL
#define LIGHT_BOUNDS_SAMPLER_HLSL

/*
// Requires the following data to be defined in any shader that uses this file
TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_LinearSampler;
*/

#ifdef LIGHTSAMPLERBOUNDS_USE_UNIFORM_SAMPLING
#include "../light_sampler/light_sampler_uniform.hlsl"

/** Dummy function call that does nothing */
void LightBounds_StorePosition(in float3 position)
{}
#else
#include "light_sampler_bounds_shared.h"

RWStructuredBuffer<LightSamplingConfiguration> g_LightSampler_Configuration;
RWStructuredBuffer<uint> g_LightSampler_CellsIndex;
RWStructuredBuffer<float> g_LightSampler_CellsCDF;

// If using GPU based bounds calculation then also:
RWStructuredBuffer<uint> g_LightSampler_BoundsLength;
RWStructuredBuffer<float3> g_LightSampler_MinBounds;
RWStructuredBuffer<float3> g_LightSampler_MaxBounds;

#include "../light_sampler/light_sampler.hlsl"
#include "../../lights/light_sampling.hlsl"
#include "../../lights/reservoir.hlsl"
#include "../../materials/material_evaluation.hlsl"
namespace LightSamplerBounds
{
    /**
     * Calculate the start index for a requested cell within a continuous 1D buffer.
     * @param cell Index of requested cell (0-indexed).
     * @return The index of the cell.
     */
    uint getCellIndex(uint3 cell)
    {
        const uint3 numCells = g_LightSampler_Configuration[0].numCells.xyz;
        const uint maxLightsPerCell = g_LightSampler_Configuration[0].numCells.w;
        uint index = cell.x + numCells.x * (cell.y + numCells.y * cell.z);
        index *= (maxLightsPerCell + 1); // There is 1 extra slot used for cell table header
        return index;
    }

    /**
     * Calculate which cell a position falls within.
     * @param position  The world space position.
     * @return The index of the grid cell.
     */
    uint3 getCellFromPosition(float3 position)
    {
        const float3 numCells = (float3)g_LightSampler_Configuration[0].numCells.xyz;
        float3 relativePos = position - g_LightSampler_Configuration[0].sceneMin;
        relativePos /= g_LightSampler_Configuration[0].sceneExtent;
        const uint3 cell = clamp(floor(relativePos * numCells), 0.0f, numCells - 1.0f.xxx);
        return cell;
    }

    /**
     * Get the current index into the light sampling cell buffer for a given position.
     * @param position Current position on surface.
     * @return The index of the current cell.
     */
    uint getCellIndexFromPosition(float3 position)
    {
        // Calculate which cell we are in based on input point
        const uint3 cell = getCellFromPosition(position);

        // Calculate position of current cell in output buffer
        const uint cellIndex = getCellIndex(cell);
        return cellIndex;
    }

    /**
     * Get the current index into the light sampling cell buffer for a given jittered position.
     * @note The current position will be jittered by +- half the current cell size.
     * @tparam RNG The type of random number sampler to be used.
     * @param position Current position on surface.
     * @return The index of the current cell.
     */
    template<typename RNG>
    uint getCellIndexFromJitteredPosition(float3 position, inout RNG randomNG)
    {
        // Jitter current position by +-half cell size
        position += (randomNG.rand3() - 0.5f) * g_LightSampler_Configuration[0].cellSize;

        return getCellIndexFromPosition(position);
    }

    /**
     * Perform a search of the light list for a light with CDF closest to a given value.
     * @param startIndex The index of the first item to start searching at.
     * @param numLights  The number of values to search through.
     * @param value      The CDF value of item to find.
     * @return The index of the sampled light.
     */
    uint binarySearch(uint startIndex, uint numLights, float value)
    {
        // Search through looking for last element with cdf >= value
        uint first = 0;
        uint len = numLights;
        while (len > 0)
        {
            uint halfed = len >> 1;
            uint middle = first + halfed;
            // Bisect range based on value at middle
            if (g_LightSampler_CellsCDF[startIndex + middle] < value)
            {
                first = middle + 1;
                len -= halfed + 1;
            }
            else
            {
                len = halfed;
            }
        }
        const uint sampledIndexBase = min(first, numLights - 1);
        // Add cell index to found position
        const uint sampledIndex = sampledIndexBase + startIndex;
        return sampledIndex;
    }

    /**
     * Sample the index and PDF for a sampled light.
     * @tparam RNG The type of random number sampler to be used.
     * @param randomNG    Random number sampler used to sample light.
     * @param cellIndex   The index of the current cell.
     * @param numLights   The number of lights in the cell.
     * @param normal      Shading normal vector at current position.
     * @param lightPDF    (Out) The PDF for the calculated sample.
     * @return The index of the sampled light.
     */
    template<typename RNG>
    uint getSampledLight(inout RNG randomNG, uint cellIndex, uint numLights, float3 normal, out float lightPDF)
    {
#ifndef LIGHTSAMPLERBOUNDS_USE_CDF
        // Get total weight (=Wsum / M)
        const float totalWeight = g_LightSampler_CellsCDF[cellIndex];

        // Collapse reservoir down to a single sample
        const uint startIndex = cellIndex + 1;
        uint firstLightIndex = startIndex;
        uint sampledIndex = g_LightSampler_CellsIndex[firstLightIndex];
        float sampledPDF = g_LightSampler_CellsCDF[firstLightIndex];
        float Wsum = sampledPDF;
        float j = randomNG.rand();
        float pNone = 1.0f;
        for (uint currentIndex = startIndex + 1; currentIndex < startIndex + numLights; ++currentIndex)
        {
            float targetPDF = g_LightSampler_CellsCDF[currentIndex];
            Wsum += targetPDF;
            float p = targetPDF / Wsum;
            j -= p * pNone;
            pNone = pNone * (1.0f - p);
            if (j <= 0.0f)
            {
                sampledIndex = g_LightSampler_CellsIndex[currentIndex];
                sampledPDF = targetPDF;
                j = randomNG.rand();
                pNone = 1.0f;
            }
        }

        // Final result has PDF = (Wsum / M) / targetPDF
        lightPDF = sampledPDF / totalWeight;
        return sampledIndex;
#else
        const uint startIndex = cellIndex + 1;
        const uint sampledIndex = binarySearch(startIndex, numLights, randomNG.rand());

        // Calculate pdf, The pdf is the contribution of the given light divided by the total contribution of all lights multiplied by the number of lights
        // This is actually just the difference between the current cdf and the previous
        const float previousCDF = (sampledIndex > startIndex) ? g_LightSampler_CellsCDF[sampledIndex - 1] : 0.0f;
        lightPDF = g_LightSampler_CellsCDF[sampledIndex] - previousCDF;
        lightPDF *= g_LightSampler_CellsCDF[cellIndex];
        return g_LightSampler_CellsIndex[sampledIndex];
#endif
    }
}

/**
 * Records the position of future light lookups.
 * @param position Current position on surface.
 */
void LightBounds_StorePosition(in float3 position)
{
    const float3 position_min = WaveActiveMin(position);
    const float3 position_max = WaveActiveMax(position);
    if (WaveIsFirstLane())
    {
        uint offset;
        InterlockedAdd(g_LightSampler_BoundsLength[0], 1, offset);
        g_LightSampler_MinBounds[offset] = position_min;
        g_LightSampler_MaxBounds[offset] = position_max;
    }
}

/**
 * Get a sample light.
 * @tparam RNG The type of random number sampler to be used.
 * @param randomNG Random number sampler used to sample light.
 * @param position Current position on surface.
 * @param normal   Shading normal vector at current position.
 * @param lightPDF (Out) The PDF for the calculated sample (is equal to zero if no valid samples could be found).
 * @returns The index of the new light sample
 */
template<typename RNG>
uint sampleLights(inout RNG randomNG, float3 position, float3 normal, out float lightPDF)
{
    // Get the current cell buffer index
    const uint cellIndex = LightSamplerBounds::getCellIndexFromJitteredPosition(position, randomNG);
    const uint numLights = g_LightSampler_CellsIndex[cellIndex];

    // Return invalid sample if the cell doesn't contain any lights
    if (numLights == 0)
    {
        lightPDF = 0.0f;
        return 0;
    }

    // Choose a light to sample from
    uint lightIndex = LightSamplerBounds::getSampledLight(randomNG, cellIndex, numLights, normal, lightPDF);
    return lightIndex;
}

/**
 * Calculate the PDF of sampling a given light.
 * @param position The position on the surface currently being shaded.
 * @returns The calculated PDF with respect to the light.
 */
float sampleLightPDF(float3 position)
{
    // Get the current cell buffer index
    const uint cellIndex = LightSamplerBounds::getCellIndexFromPosition(position);
    const uint numLights = g_LightSampler_CellsIndex[cellIndex];

    // TODO: This is technically incorrect as it doesnt take into account the specific PDF for the current light as we need to know what light we actually hit which is currently not available.
    return 1.0f / numLights;
}

/**
 * Sample multiple lights into a reservoir.
 * @tparam numSampledLights Number of lights to sample.
 * @tparam RNG The type of random number sampler to be used.
 * @param randomNG      Random number sampler used to sample light.
 * @param position      Current position on surface.
 * @param normal        Shading normal vector at current position.
 * @param viewDirection View direction vector at current position.
 * @param solidAngle    Solid angle around view direction of visible ray cone.
 * @param material      Material for current surface position.
 * @returns Reservoir containing combined samples.
 */
template<uint numSampledLights, typename RNG>
Reservoir sampleLightListCone(/*inout TODO: dxc crash*/ RNG randomNG, float3 position, float3 normal, float3 viewDirection, float solidAngle, MaterialBRDF material)
{
    // Get the current cell buffer index
    const uint cellIndex = LightSamplerBounds::getCellIndexFromJitteredPosition(position, randomNG);
    const uint numLights = g_LightSampler_CellsIndex[cellIndex];
    const uint newLights = min(numLights, numSampledLights);

    // Return invalid sample if the cell doesn't contain any lights
    if (numLights == 0)
    {
        return MakeReservoir();
    }

    // Create reservoir updater
    ReservoirUpdater updater = MakeReservoirUpdater();

#ifndef LIGHTSAMPLERBOUNDS_USE_CDF
    uint sampledIndexes[numSampledLights]; // Final samples
    float sampledPDFs[numSampledLights];

    // Collapse reservoir down to N samples where N=numSampledLights
    const uint startIndex = cellIndex + 1;
    uint currentIndex = startIndex;
    uint lightsAdded = 0;
    float Wsum = 0;
    for (; currentIndex < startIndex + newLights; ++currentIndex)
    {
        // Must pre-fill the N sized final reservoir
        sampledIndexes[lightsAdded] = g_LightSampler_CellsIndex[currentIndex];
        float targetPDF = g_LightSampler_CellsCDF[currentIndex];
        sampledPDFs[lightsAdded] = targetPDF;
        Wsum += targetPDF;
        ++lightsAdded;
    }
    // Use A-Chao with jumps to sample remainder of cells light list
    float j = randomNG.rand();
    float pNone = 1.0f;
    for (; currentIndex < startIndex + numLights; ++currentIndex)
    {
        float targetPDF = g_LightSampler_CellsCDF[currentIndex];
        Wsum += targetPDF;
        float p = targetPDF / Wsum;
        j -= p * pNone;
        pNone = pNone * (1.0f - p);
        if (j <= 0.0f)
        {
            uint replaceIndex = randomNG.randInt(numSampledLights);
            sampledIndexes[replaceIndex] = g_LightSampler_CellsIndex[currentIndex];
            sampledPDFs[replaceIndex] = targetPDF;
            j = randomNG.rand();
            pNone = 1.0f;
        }
    }

    // Get total weight (=Wsum / M)
    const float totalWeight = g_LightSampler_CellsCDF[cellIndex];
    const float modifierPDF = lightsAdded / totalWeight;
    // Loop through the collapsed reservoir and add samples
    for (uint i = 0; i < lightsAdded; ++i)
    {
        // Add the light sample to the reservoir
        uint lightIndex = sampledIndexes[i];
        float lightPDF = sampledPDFs[i] * modifierPDF;
        updateReservoir(updater, randomNG, lightIndex, lightPDF, material, position, normal, viewDirection, solidAngle);
    }
#else
    // Loop through until we have the requested number of lights
    float lightPDF;
    for (uint lightsAdded = 0; lightsAdded < newLights; ++lightsAdded)
    {
        // Choose a light to sample from
        uint lightIndex = LightSamplerBounds::getSampledLight(randomNG, cellIndex, numLights, normal, lightPDF);

        // Add the light sample to the reservoir
        updateReservoir(updater, randomNG, lightIndex, lightPDF, material, position, normal, viewDirection, solidAngle);
    }
#endif

    // Get finalised reservoir for return
    return getUpdatedReservoir(updater);
}
#endif // LIGHTSAMPLERBOUNDS_USE_UNIFORM_SAMPLING

#endif // LIGHT_BOUNDS_SAMPLER_HLSL
