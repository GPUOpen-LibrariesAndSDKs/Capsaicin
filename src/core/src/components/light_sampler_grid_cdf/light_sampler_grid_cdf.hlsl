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

#ifndef LIGHT_SAMPLER_GRID_CDF_HLSL
#define LIGHT_SAMPLER_GRID_CDF_HLSL

/*
// Requires the following data to be defined in any shader that uses this file
TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;
*/

#include "light_sampler_grid_shared.h"

RWStructuredBuffer<LightSamplingConfiguration> g_LightSampler_Configuration;
RWStructuredBuffer<uint> g_LightSampler_CellsIndex;
RWStructuredBuffer<float> g_LightSampler_CellsCDF;

#include "light_sampler_grid.hlsl"
#include "../light_builder/light_builder.hlsl"
#include "../../lights/light_sampling.hlsl"
#include "../../lights/reservoir.hlsl"
#include "../../materials/material_evaluation.hlsl"
#include "../../lights/light_sampling_volume.hlsl"
#include "../../math/random.hlsl"


class LightSamplerGridCDF
{
    Random randomNG;

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
     * @param cellIndex The index of the current cell.
     * @param numLights The number of lights in the cell.
     * @param position  Current position on surface.
     * @param normal    Shading normal vector at current position.
     * @param lightPDF  (Out) The PDF for the calculated sample.
     * @return The index of the sampled light.
     */
    uint getSampledLight(uint cellIndex, uint numLights, float3 position, float3 normal, out float lightPDF)
    {
        const uint startIndex = cellIndex + 1;
        const uint sampledIndex = binarySearch(startIndex, numLights, randomNG.rand());

        // Calculate pdf, The pdf is the contribution of the given light divided by the total contribution of all lights multiplied by the number of lights
        // This is actually just the difference between the current cdf and the previous
        const float previousCDF = (sampledIndex > startIndex) ? g_LightSampler_CellsCDF[sampledIndex - 1] : 0.0f;
        lightPDF = g_LightSampler_CellsCDF[sampledIndex] - previousCDF;
#ifndef LIGHTSAMPLERCDF_HAS_ALL_LIGHTS
        lightPDF *= g_LightSampler_CellsCDF[cellIndex];
#endif
        return g_LightSampler_CellsIndex[sampledIndex];
    }

    /**
     * Get a sample light.
     * @param position Current position on surface.
     * @param normal   Shading normal vector at current position.
     * @param lightPDF (Out) The PDF for the calculated sample (is equal to zero if no valid samples could be found).
     * @returns The index of the new light sample
     */
    uint sampleLights(float3 position, float3 normal, out float lightPDF)
    {
        // Get the current cell buffer index
#ifdef LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndexFromJitteredPosition(position, normal, randomNG);
#else
        const uint cellIndex = LightSamplerGrid::getCellIndexFromJitteredPosition(position, randomNG);
#endif
        const uint numLights = g_LightSampler_CellsIndex[cellIndex];

        // Return invalid sample if the cell doesn't contain any lights
        if (numLights == 0)
        {
            lightPDF = 0.0f;
            return 0;
        }

        // Choose a light to sample from
        uint lightIndex = getSampledLight(cellIndex, numLights, position, normal, lightPDF);
        return lightIndex;
    }

    /**
     * Calculate the PDF of sampling a given light.
     * @param lightID  The index of the given light.
     * @param position The position on the surface currently being shaded.
     * @param normal   Shading normal vector at current position.
     * @returns The calculated PDF with respect to the light.
     */
    float sampleLightPDF(uint lightID, float3 position, float3 normal)
    {
        // Calculate which cell we are in based on input point
        const uint3 cell = LightSamplerGrid::getCellFromJitteredPosition(position, randomNG);

        // Calculate position of current cell in output buffer
#ifdef LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndex(cell, LightSamplerGrid::getCellFace(normal));
#else
        const uint cellIndex = LightSamplerGrid::getCellIndex(cell);
#endif

#ifndef LIGHTSAMPLERCDF_HAS_ALL_LIGHTS
        // For CDF sampling the probability is based on finding it in the current CDF list
        const uint numLights = g_LightSampler_CellsIndex[cellIndex];
        const uint startIndex = cellIndex + 1;
        for (uint currentIndex = startIndex; currentIndex < startIndex + numLights; ++currentIndex)
        {
            if (g_LightSampler_CellsIndex[currentIndex] == lightID)
            {
                const float sourcePDF = g_LightSampler_CellsCDF[currentIndex];
                const float previousCDF = (currentIndex > startIndex) ? g_LightSampler_CellsCDF[currentIndex - 1] : 0.0f;
                float lightPDF = sourcePDF - previousCDF;
                lightPDF *= g_LightSampler_CellsCDF[cellIndex];
                return lightPDF;
            }
        }
        // A light not in the CDF list has zero probability of being selected.
        return FLT_EPSILON;
#else
        // Instead of searching for the light in the list we just re-calculate its weight as this
        //   makes it a constant time operation.
        // Calculate the bounding box for the current cell
        float3 extent;
        const float3 minBB = LightSamplerGrid::getCellBB(cell, extent);
        Light selectedLight = getLight(lightID);
#ifdef LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
        float weight = sampleLightVolumeNormal(selectedLight, minBB, extent, normal);
#else
        float weight = sampleLightVolume(selectedLight, minBB, extent);
#endif // LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
        return weight;
#endif
    }

    /**
     * Sample multiple lights into a reservoir.
     * @tparam numSampledLights Number of lights to sample.
     * @param position      Current position on surface.
     * @param normal        Shading normal vector at current position.
     * @param viewDirection View direction vector at current position.
     * @param material      Material for current surface position.
     * @returns Reservoir containing combined samples.
     */
    template<uint numSampledLights>
    Reservoir sampleLightList(float3 position, float3 normal, float3 viewDirection, MaterialBRDF material)
    {
        // Get the current cell buffer index
#ifdef LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndexFromJitteredPosition(position, normal, randomNG);
#else
        const uint cellIndex = LightSamplerGrid::getCellIndexFromJitteredPosition(position, randomNG);
#endif
        const uint numLights = g_LightSampler_CellsIndex[cellIndex];
        const uint newLights = min(numLights, numSampledLights);

        // Return invalid sample if the cell doesn't contain any lights
        if (numLights == 0)
        {
            return MakeReservoir();
        }

        // Create reservoir updater
        ReservoirUpdater updater = MakeReservoirUpdater();

        // Loop through until we have the requested number of lights
        for (uint i = 0; i < newLights; ++i)
        {
            // Choose a light to sample from
            float lightPDF;
            uint lightIndex = getSampledLight(cellIndex, numLights, position, normal, lightPDF);

            // Add the light sample to the reservoir
            updateReservoir(updater, randomNG, lightIndex, lightPDF, material, position, normal, viewDirection);
        }

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
     * @returns Reservoir containing combined samples.
     */
    template<uint numSampledLights>
    Reservoir sampleLightListCone(float3 position, float3 normal, float3 viewDirection, float solidAngle, MaterialBRDF material)
    {
        // Get the current cell buffer index
#ifdef LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
        const uint cellIndex = LightSamplerGrid::getCellOctaIndexFromJitteredPosition(position, normal, randomNG);
#else
        const uint cellIndex = LightSamplerGrid::getCellIndexFromJitteredPosition(position, randomNG);
#endif
        const uint numLights = g_LightSampler_CellsIndex[cellIndex];
        const uint newLights = min(numLights, numSampledLights);

        // Return invalid sample if the cell doesn't contain any lights
        if (numLights == 0)
        {
            return MakeReservoir();
        }

        // Create reservoir updater
        ReservoirUpdater updater = MakeReservoirUpdater();

        // Loop through until we have the requested number of lights
        for (uint i = 0; i < newLights; ++i)
        {
            // Choose a light to sample from
            float lightPDF;
            uint lightIndex = getSampledLight(cellIndex, numLights, position, normal, lightPDF);

            // Add the light sample to the reservoir
            updateReservoirCone(updater, randomNG, lightIndex, lightPDF, material, position, normal, viewDirection, solidAngle);
        }

        // Get finalised reservoir for return
        return updater.reservoir;
    }
};

LightSamplerGridCDF MakeLightSampler(Random random)
{
    LightSamplerGridCDF ret;
    ret.randomNG = random;
    return ret;
}

typedef LightSamplerGridCDF LightSampler;

#endif // LIGHT_BOUNDS_SAMPLER_HLSL
