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

#include "light_sampler_grid_shared.h"

TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler; // Is a linear sampler

uint g_FrameIndex;

#ifdef LIGHTSAMPLERCDF_USE_THRESHOLD
#define THRESHOLD_RADIANCE (1.0f / 2048.0f)
#endif

#include "light_sampler_grid_cdf.hlsl"
#include "../../math/random.hlsl"

#define THREADX 4
#define THREADY 4
#define THREADZ 4

/**
 * Build an internal grid.
 */
[numthreads(THREADX, THREADY, THREADZ)]
void Build(in uint3 did : SV_DispatchThreadID)
{
#ifdef LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
    // Called per cell per octahedron face
    const uint3 cellID = uint3(did.x / 8, did.y, did.z);
#else
    // Called per cell
    const uint3 cellID = did;
#endif // LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
    if (any(cellID >= g_LightSampler_Configuration[0].numCells.xyz))
    {
        return;
    }

    // Calculate the bounding box for the current cell
    float3 extent;
    const float3 minBB = LightSamplerGrid::getCellBB(cellID, extent);

    // Get total and max supported number of lights
    uint totalLights = getNumberLights();
    const uint maxLightsPerCell = g_LightSampler_Configuration[0].numCells.w - 1;

    // Loop through all lights
#ifdef LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
    const uint cellFace = did.x - (cellID.x * 8);
    uint cellIndex = LightSamplerGrid::getCellOctaIndex(cellID, cellFace);
    const float3 normal = LightSamplerGrid::getCellNormal(cellFace);
#else
    const uint cellIndex = LightSamplerGrid::getCellIndex(cellID);
#endif // LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
    const uint startIndex = cellIndex + 1;
    Random randomNG = MakeRandom(cellIndex, g_FrameIndex);
    uint storedLights = 0; //Num of stored lights in cell
    float totalWeight = 0.0f;
    for (uint lightIndex = 0; lightIndex < totalLights; ++lightIndex)
    {
        // Calculate sampled contribution for light
        Light selectedLight = getLight(lightIndex);
#ifdef LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING
        float y = sampleLightVolumeNormal(selectedLight, minBB, extent, normal);
#else
        float y = sampleLightVolume(selectedLight, minBB, extent);
#endif // LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING

        if (y > 0.0f)
        {
            // Store only the most important lights
            totalWeight += y;
            if (storedLights < maxLightsPerCell)
            {
                ++storedLights;
                g_LightSampler_CellsIndex[cellIndex + storedLights] = lightIndex;
                g_LightSampler_CellsCDF[cellIndex + storedLights] = y;
            }
            else
            {
                // Find the lowest contributing light and replace
                uint smallestLight = -1;
                float smallestCDF = y;
                uint writeIndex = startIndex + storedLights;
                for (uint light = startIndex; light < writeIndex; ++light)
                {
                    if (g_LightSampler_CellsCDF[light] < smallestCDF)
                    {
                        smallestLight = light;
                        smallestCDF = g_LightSampler_CellsCDF[light];
                    }
                }
                if (smallestLight != -1)
                {
                    g_LightSampler_CellsIndex[smallestLight] = lightIndex;
                    g_LightSampler_CellsCDF[smallestLight] = y;
                }
            }
        }
    }

    // Add table for cells light list
    g_LightSampler_CellsIndex[cellIndex] = storedLights;

    // Convert to CDF
    float runningCDF = 0.0f;
    for (uint i = startIndex; i <= cellIndex + storedLights; ++i)
    {
        runningCDF = runningCDF + g_LightSampler_CellsCDF[i];
        g_LightSampler_CellsCDF[i] = runningCDF;
    }
    // Normalise CDF
    float recipMaxCDF = 1.0f / runningCDF;
    for (uint j = startIndex; j < cellIndex + storedLights; ++j)
    {
        g_LightSampler_CellsCDF[j] *= recipMaxCDF;
    }
    g_LightSampler_CellsCDF[cellIndex + storedLights] = 1.0f;

    // Write out max cdf to cell table
#ifndef LIGHTSAMPLERCDF_HAS_ALL_LIGHTS
    g_LightSampler_CellsCDF[cellIndex] = runningCDF / totalWeight;
#endif
}
