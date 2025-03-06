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

#ifndef LIGHT_SAMPLER_GRID_STREAM_HLSL
#define LIGHT_SAMPLER_GRID_STREAM_HLSL

/*
// Requires the following data to be defined in any shader that uses this file
TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;
*/

#include "components/light_sampler_grid_cdf/light_sampler_grid_shared.h"

RWStructuredBuffer<LightSamplingConfiguration> g_LightSampler_Configuration;
RWStructuredBuffer<uint> g_LightSampler_CellsIndex;
RWStructuredBuffer<float2> g_LightSampler_CellsReservoirs;

// If using GPU based bounds calculation then also:
RWStructuredBuffer<uint> g_LightSampler_BoundsLength;
RWStructuredBuffer<float3> g_LightSampler_MinBounds;
RWStructuredBuffer<float3> g_LightSampler_MaxBounds;

#include "components/light_sampler_grid_cdf/light_sampler_grid.hlsl"
#include "components/light_builder/light_builder.hlsl"
#include "lights/light_sampling.hlsl"
#include "materials/material_evaluation.hlsl"
#include "lights/light_sampling_volume.hlsl"
#include "math/random.hlsl"
#ifdef LIGHT_SAMPLER_ENABLE_RESERVOIR
#include "lights/reservoir.hlsl"
#endif

struct LightSamplerGridStream
{
    Random randomNG;

    /**
     * Sample the index and PDF for a sampled light.
     * @param cellIndex The index of the current cell.
     * @param numLights The number of lights in the cell.
     * @param position  Current position on surface.
     * @param normal    Shading normal vector at current position.
     * @param lightPDF  (Out) The PDF for the calculated sample.
     * @return The index of the sampled light.
     */
    uint getSampledLight(uint cellIndex, uint numLights, float3 position, float3 normal, out float lightPDF)
    {
#ifndef LIGHTSAMPLERSTREAM_RES_RANDOM_MERGE
        // Collapse reservoir down to a single sample
        uint storedLight = -1;
        float storedLightWeight = 0.0f;
        float totalWeight = 0.0f;
        float j = randomNG.rand();
        float pNone = 1.0f;
        for (uint currentIndex = cellIndex; currentIndex < cellIndex + numLights; ++currentIndex)
        {
#   ifdef LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
            // Use Sampling Importance Resampling to calculate new targetPDF
            float sampleWeightMod = g_LightSampler_CellsReservoirs[currentIndex].x; //Contains totalWeight/sampleTargetPDF
            uint sampledLight = g_LightSampler_CellsIndex[currentIndex];
            float sampleTargetPDF = sampleLightPointNormal(getLight(sampledLight), position, normal);
            float sampleWeight = sampleTargetPDF * sampleWeightMod;
#   else
            float2 reservoirWeights = g_LightSampler_CellsReservoirs[currentIndex];
            // Merge using totalWeight of input reservoir as sample weight
            float sampleWeight = reservoirWeights.y;
            float sampleTargetPDF = reservoirWeights.x;
#   endif
            // Must avoid 0 samples at the start of the stream to avoid division by 0
            if (sampleWeight > 0.0f)
            {
                totalWeight += sampleWeight;
                float p = sampleWeight / totalWeight;
                j -= p * pNone;
                pNone *= (1.0f - p);
                if (j <= 0.0f)
                {
#   ifdef LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
                    storedLight = sampledLight;
#   else
                    storedLight = g_LightSampler_CellsIndex[currentIndex];
#   endif
                    storedLightWeight = sampleTargetPDF;
                    j = randomNG.rand();
                    pNone = 1.0f;
                }
            }
        }

        // Final result has PDF = M * targetPDF / totalWeight
        //   The M term cancels out with weight calculation resulting in just targetPDF/totalWeight
        lightPDF = (totalWeight > 0.0f) ? storedLightWeight / totalWeight : 0.0f;
        return storedLight;
#else // !LIGHTSAMPLERSTREAM_RES_RANDOM_MERGE
        // Just randomly select one of the reservoirs
        uint storedLight = randomNG.randInt(numLights);
        uint currentIndex = cellIndex + storedLight;
        storedLight = g_LightSampler_CellsIndex[currentIndex];
        float2 reservoirWeights = g_LightSampler_CellsReservoirs[currentIndex];
        float sampleWeight = reservoirWeights.y;
        float sampleTargetPDF = reservoirWeights.x;
        lightPDF = (sampleWeight > 0.0f) ? sampleTargetPDF / (sampleWeight * (float)numLights) : 0.0f;
        return storedLight;
#endif // !LIGHTSAMPLERSTREAM_RES_RANDOM_MERGE
    }

    /**
     * Get a sample light.
     * @param position Current position on surface.
     * @param normal   Shading normal vector at current position.
     * @param lightPDF (Out) The PDF for the calculated sample (is equal to zero if no valid samples could be found).
     * @return The index of the new light sample
     */
    uint sampleLights(float3 position, float3 normal, out float lightPDF)
    {
        // Get the current cell buffer index
#ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndexFromJitteredPosition(position, normal, randomNG);
#else
        const uint cellIndex = LightSamplerGrid::getCellIndexFromJitteredPosition(position, randomNG);
#endif
        const uint numLights = min(g_LightSampler_Configuration[0].numCells.w, getNumberLights());

        // Return invalid sample if the cell doesn't contain any lights
        if (numLights == 0)
        {
            lightPDF = 0.0f;
            return 0;
        }

        // Choose a light to sample from
        uint lightIndex = LightSamplerGridStream::getSampledLight(cellIndex, numLights, position, normal, lightPDF);
        return lightIndex;
    }

    /**
     * Calculate the PDF of sampling a given light.
     * @param lightID  The index of the given light.
     * @param position The position on the surface currently being shaded.
     * @param normal   Shading normal vector at current position.
     * @return The calculated PDF with respect to the light.
     */
    float sampleLightPDF(uint lightID, float3 position, float3 normal)
    {
        // Calculate which cell we are in based on input point
        const uint3 cell = LightSamplerGrid::getCellFromJitteredPosition(position, randomNG);

        // Calculate position of current cell in output buffer
#ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndex(cell, LightSamplerGrid::getCellFace(normal));
#else
        const uint cellIndex = LightSamplerGrid::getCellIndex(cell);
#endif
        const uint reservoirsPerCell = g_LightSampler_Configuration[0].numCells.w;
        const uint numLights = min(reservoirsPerCell, getNumberLights());

        // Since the stream sampler is stochastic we calculate the lights original sampling weight and
        //  calculate the PDF assuming that the cell hasn't already been constructed
        // Calculate the bounding box for the current cell
        float3 extent;
        const float3 minBB = LightSamplerGrid::getCellBB(cell, extent);
        Light selectedLight = getLight(lightID);
#ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        float newSampleTargetPDF = sampleLightVolumeNormal(selectedLight, minBB, extent, normal);
#else
        float newSampleTargetPDF = sampleLightVolume(selectedLight, minBB, extent);
#endif

#ifndef LIGHTSAMPLERSTREAM_RES_RANDOM_MERGE
        // Using reservoir resampling the probability of sampling a given index is based on its
        //  weight with respect to the total cell weight.
        float totalWeight = 0.0f;
        for (uint currentIndex = cellIndex; currentIndex < cellIndex + numLights; ++currentIndex)
        {
            uint sampledLight = g_LightSampler_CellsIndex[currentIndex];
#   ifdef LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
            float sampleWeightMod = g_LightSampler_CellsReservoirs[currentIndex].x; //Contains totalWeight/sampleTargetPDF
            float sampleTargetPDF = sampleLightPointNormal(getLight(sampledLight), position, normal);
            float sampleWeight = sampleTargetPDF * sampleWeightMod;
#   else
            float2 reservoirWeights = g_LightSampler_CellsReservoirs[currentIndex];
            float sampleWeight = reservoirWeights.y;
#   endif // LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
            totalWeight += sampleWeight;
        }

#   ifdef LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
        uint lightIndex = cellIndex + (lightID % reservoirsPerCell);
        float cellOriginalWeight = g_LightSampler_CellsReservoirs[lightIndex].y;
        float resampledTargetPDF = sampleLightPointNormal(selectedLight, position, normal);
        float resampledWeight = newSampleTargetPDF != 0.0f ? resampledTargetPDF * (cellOriginalWeight / newSampleTargetPDF) : 0.0f;
        // Final PDF is probability of being in corresponding cell reservoir (newSampleTargetPDF / cellOriginalWeight)
        //   multiplied by the resampled weight (resampleWeight / totalResampledWeight)
        float divisor = cellOriginalWeight * totalWeight;
        return divisor != 0.0f ? (newSampleTargetPDF * resampledWeight) / divisor : 0.0f;
#   else
        return newSampleTargetPDF / totalWeight;
#   endif
#else
        // PDF is probability of being selected in corresponding cell reservoir multiplied by random selection probability
        uint lightIndex = cellIndex + (lightID % reservoirsPerCell);
        float sampleWeight = g_LightSampler_CellsReservoirs[lightIndex].y;
        const float divisor = sampleWeight * (float)numLights;
        return divisor != 0.0f ? newSampleTargetPDF / divisor : 0.0f;
#endif
    }

#ifdef LIGHT_SAMPLER_ENABLE_RESERVOIR
    /**
     * Sample multiple lights into a reservoir.
     * @tparam numSampledLights Number of lights to sample.
     * @param position      Current position on surface.
     * @param normal        Shading normal vector at current position.
     * @param viewDirection View direction vector at current position.
     * @param material      Material for current surface position.
     * @return Reservoir containing combined samples.
     */
    template<uint numSampledLights>
    Reservoir sampleLightList(float3 position, float3 normal, float3 viewDirection, MaterialBRDF material)
    {
        // Get the current cell buffer index
#   ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndexFromJitteredPosition(position, normal, randomNG);
#   else
        const uint cellIndex = LightSamplerGrid::getCellIndexFromJitteredPosition(position, randomNG);
#   endif
        const uint numLights = min(g_LightSampler_Configuration[0].numCells.w, getNumberLights());
#   ifdef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
        // Fast merge does not use sample replacement so its limited to the actual number of cell lights
        const uint newLights = min(numLights, numSampledLights);
#   else
        const uint newLights = numSampledLights;
#   endif

        // Return invalid sample if the cell doesn't contain any lights
        if (numLights == 0)
        {
            return MakeReservoir();
        }

        // Create reservoir updater
        ReservoirUpdater updater = MakeReservoirUpdater();

        // Loop through until we have the requested number of lights
#   ifdef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
        const uint lightsPerSegment = (numLights + numSampledLights - 1) / numSampledLights;
        const uint maxCellIndex = cellIndex + numLights;
        uint startIndex = cellIndex;
        struct LightResSample
        {
            uint lightIndex;
            float lightPDF;
        };
        LightResSample lightSamples[numSampledLights];
        uint lightsAdded = 0;
#   endif
        for (uint i = 0; i < newLights; ++i)
        {
            // Choose a light to sample from
            float lightPDF;
#   ifndef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
            uint lightIndex = LightSamplerGridStream::getSampledLight(cellIndex, numLights, position, normal, lightPDF);

            // Add the light sample to the reservoir
            updateReservoir(updater, randomNG, lightIndex, lightPDF, material, position, normal, viewDirection, newLights);
#   else
            // Divide total light range into newLights segments and only collapse those
            uint endIndex = startIndex + lightsPerSegment;
            uint segmentLights = (endIndex <= maxCellIndex) ? lightsPerSegment : maxCellIndex - startIndex;
            uint lightIndex = LightSamplerGridStream::getSampledLight(startIndex, segmentLights, position, normal, lightPDF);
            // Increment current cell index
            startIndex = endIndex;
            // Some reservoirs may not contain any valid lights, we skip these to ensure correct final PDFs
            if (lightIndex == -1)
            {
                continue;
            }
            lightSamples[lightsAdded].lightIndex = lightIndex;
            lightSamples[lightsAdded].lightPDF = lightPDF;
            ++lightsAdded;
#   endif
        }

#   ifdef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
        for (uint i = 0; i < lightsAdded; ++i)
        {
            // We are now resampling without replacement so the sample PDFs need to be corrected.
            // The actual light PDF is effected by the value of M(lightsAdded) which may not always be equal to newLights
            //    with the final PDF being scaled by the number of actual final valid reservoirs
            float lightPDF = lightSamples[i].lightPDF / (float)lightsAdded;
            // Add the light sample to the reservoir
            updateReservoir(updater, randomNG, lightSamples[i].lightIndex, lightPDF, material, position, normal, viewDirection, lightsAdded);
        }
#   endif

        // Get finalised reservoir for return
        return updater.reservoir;
    }

    /**
     * Sample multiple lights into a reservoir using cone angle.
     * @tparam numSampledLights Number of lights to sample.
     * @param position      Current position on surface.
     * @param normal        Shading normal vector at current position.
     * @param viewDirection View direction vector at current position.
     * @param solidAngle    Solid angle around view direction of visible ray cone.
     * @param material      Material for current surface position.
     * @return Reservoir containing combined samples.
     */
    template<uint numSampledLights>
    Reservoir sampleLightListCone(float3 position, float3 normal, float3 viewDirection, float solidAngle, MaterialBRDF material)
    {
        // Get the current cell buffer index
#   ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndexFromJitteredPosition(position, normal, randomNG);
#   else
        const uint cellIndex = LightSamplerGrid::getCellIndexFromJitteredPosition(position, randomNG);
#   endif
        const uint numLights = min(g_LightSampler_Configuration[0].numCells.w, getNumberLights());
#   ifdef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
        const uint newLights = min(numLights, numSampledLights);
#   else
        const uint newLights = numSampledLights;
#   endif

        // Return invalid sample if the cell doesn't contain any lights
        if (numLights == 0)
        {
            return MakeReservoir();
        }

        // Create reservoir updater
        ReservoirUpdater updater = MakeReservoirUpdater();

        // Loop through until we have the requested number of lights
#   ifdef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
        const uint lightsPerSegment = (numLights + numSampledLights - 1) / numSampledLights;
        const uint maxCellIndex = cellIndex + numLights;
        uint startIndex = cellIndex;
        struct LightResSample
        {
            uint lightIndex;
            float lightPDF;
        };
        LightResSample lightSamples[numSampledLights];
        uint lightsAdded = 0;
#   endif
        for (uint i = 0; i < newLights; ++i)
        {
            // Choose a light to sample from
            float lightPDF;
#   ifndef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
            uint lightIndex = LightSamplerGridStream::getSampledLight(cellIndex, numLights, position, normal, lightPDF);

            // Add the light sample to the reservoir
            updateReservoirCone(updater, randomNG, lightIndex, lightPDF, material, position, normal, viewDirection, solidAngle, newLights);
#   else
            // Divide total light range into newLights segments and only collapse those
            uint endIndex = startIndex + lightsPerSegment;
            uint segmentLights = (endIndex <= maxCellIndex) ? lightsPerSegment : maxCellIndex - startIndex;
            uint lightIndex = LightSamplerGridStream::getSampledLight(startIndex, segmentLights, position, normal, lightPDF);
            // Increment current cell index
            startIndex = endIndex;
            if (lightIndex == -1)
            {
                continue;
            }
            lightSamples[lightsAdded].lightIndex = lightIndex;
            lightSamples[lightsAdded].lightPDF = lightPDF;
            ++lightsAdded;
#   endif
        }

#   ifdef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
        for (uint i = 0; i < lightsAdded; ++i)
        {
            // We are now resampling without replacement so the sample PDFs need to be corrected.
            // The actual light PDF is effected by the value of M(lightsAdded) which may not always be equal to newLights
            //    with the final PDF being scaled by the number of actual final valid reservoirs
            float lightPDF = lightSamples[i].lightPDF / (float)lightsAdded;
            // Add the light sample to the reservoir
            updateReservoirCone(updater, randomNG, lightSamples[i].lightIndex, lightPDF, material, position, normal, viewDirection, solidAngle, lightsAdded);
        }
#   endif

        // Get finalised reservoir for return
        return updater.reservoir;
    }

    /**
     * Calculate the PDF of sampling a given light using one of the reservoir list sampling functions.
     * @tparam numSampledLights Number of lights sampled.
     * @param lightID  The index of the given light.
     * @param position The position on the surface currently being shaded.
     * @param normal   Shading normal vector at current position.
     * @return The calculated PDF with respect to the light.
     */
    template<uint numSampledLights>
    float sampleLightListPDF(uint lightID, float3 position, float3 normal)
    {
#   ifdef LIGHTSAMPLERSTREAM_RES_FAST_MERGE
        // Calculate which cell we are in based on input point
        const uint3 cell = LightSamplerGrid::getCellFromJitteredPosition(position, randomNG);

        // Calculate position of current cell in output buffer
#       ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndex(cell, LightSamplerGrid::getCellFace(normal));
#       else
        const uint cellIndex = LightSamplerGrid::getCellIndex(cell);
#       endif
        const uint reservoirsPerCell = g_LightSampler_Configuration[0].numCells.w;
        const uint numLights = min(reservoirsPerCell, getNumberLights());
        const uint newLights = min(numLights, numSampledLights);

        // Calculate light sample weight
        float3 extent;
        const float3 minBB = LightSamplerGrid::getCellBB(cell, extent);
        Light selectedLight = getLight(lightID);
#       ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        float newSampleTargetPDF = sampleLightVolumeNormal(selectedLight, minBB, extent, normal);
#       else
        float newSampleTargetPDF = sampleLightVolume(selectedLight, minBB, extent);
#       endif

        // Determine the range of cell reservoirs that would have been combined when sampling
        const uint lightsPerSegment = (numLights + numSampledLights - 1) / numSampledLights;
        const uint maxCellIndex = cellIndex + numLights;
        uint lightStartOffset = lightID % reservoirsPerCell;
        uint lightIndex = cellIndex + lightStartOffset;
        // The start index for current segment is calculated by rounding down the light offset to segment size
        uint startSegmentIndex = cellIndex + ((lightStartOffset / lightsPerSegment) * lightsPerSegment);
        uint endIndex = min(startSegmentIndex + lightsPerSegment, maxCellIndex);

        // Using reservoir resampling the probability of sampling a given index is based on its
        //  weight with respect to the total weight of all sampled reservoirs in the segment.
        float totalWeight = 0.0f;
        for (uint currentIndex = startSegmentIndex; currentIndex < endIndex; ++currentIndex)
        {
            uint sampledLight = g_LightSampler_CellsIndex[currentIndex];
#       ifdef LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
            float sampleWeightMod = g_LightSampler_CellsReservoirs[currentIndex].x; //Contains totalWeight/sampleTargetPDF
            float sampleTargetPDF = sampleLightPointNormal(getLight(sampledLight), position, normal);
            float sampleWeight = sampleTargetPDF * sampleWeightMod;
#       else
            float2 reservoirWeights = g_LightSampler_CellsReservoirs[currentIndex];
            float sampleWeight = reservoirWeights.y;
#       endif // LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
            totalWeight += sampleWeight;
        }

#       ifdef LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
        float cellOriginalWeight = g_LightSampler_CellsReservoirs[lightIndex].y;
        float resampledTargetPDF = sampleLightPointNormal(selectedLight, position, normal);
        float resampledWeight = newSampleTargetPDF != 0.0f ? resampledTargetPDF * (cellOriginalWeight / newSampleTargetPDF) : 0.0f;
        // Final PDF is probability of being in corresponding cell reservoir (newSampleTargetPDF / cellOriginalWeight)
        //   multiplied by the resampled weight (resampleWeight / totalResampledWeight)
        float divisor = cellOriginalWeight * totalWeight;
        float lightPDF = divisor != 0.0f ? (newSampleTargetPDF * resampledWeight) / divisor : 0.0f;
#       else
        float lightPDF = newSampleTargetPDF / totalWeight;
#       endif

        // Determine final PDF by multiplying by selection probability
        uint startIndex = cellIndex;
        uint lightsAdded = 0;
        for (uint i = 0; i < newLights; ++i)
        {
            if (startIndex == startSegmentIndex)
            {
                // If the current segment was the same as the lights segment then it should always be
                //   considered as containing valid lights
                ++lightsAdded;
                continue;
            }
            // Divide total light range into newLights segments and only collapse those
            endIndex = min(startIndex + lightsPerSegment, maxCellIndex);
            float totalSegmentWeight = 0.0f;
            for (uint currentIndex = startIndex; currentIndex < endIndex; ++currentIndex)
            {
#       ifdef LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
                float sampleWeightMod = g_LightSampler_CellsReservoirs[currentIndex].x;
                uint sampledLight = g_LightSampler_CellsIndex[currentIndex];
                float sampleTargetPDF = sampleLightPointNormal(getLight(sampledLight), position, normal);
                float sampleWeight = sampleTargetPDF * sampleWeightMod;
#       else
                float2 reservoirWeights = g_LightSampler_CellsReservoirs[currentIndex];
                float sampleWeight = reservoirWeights.y;
#       endif
                totalSegmentWeight += sampleWeight;
            }
            // Increment current cell index
            startIndex = endIndex;
            // Some reservoirs may not contain any valid lights, we skip these to ensure correct final PDFs
            if (totalSegmentWeight == 0.0f)
            {
                continue;
            }
            ++lightsAdded;
        }
        return lightPDF / (float)lightsAdded;
#   else
        return sampleLightPDF(lightID, position, normal);
#   endif
    }
#endif // LIGHT_SAMPLER_ENABLE_RESERVOIR
};

/**
 * Records the position of future light lookups.
 * @param position Current position on surface.
 */
void requestLightSampleLocation(in float3 position)
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

LightSamplerGridStream MakeLightSampler(Random random)
{
    LightSamplerGridStream ret;
    ret.randomNG = random;
    return ret;
}

typedef LightSamplerGridStream LightSampler;

#endif // LIGHT_SAMPLER_GRID_STREAM_HLSL
