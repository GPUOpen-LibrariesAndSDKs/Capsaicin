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

#include "../light_sampler_grid_cdf/light_sampler_grid_shared.h"

ConstantBuffer<LightSamplingConstants> g_LightSampler_Constants;
RWStructuredBuffer<DispatchCommand> g_DispatchCommandBuffer;

TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler; // Is a linear sampler

uint g_FrameIndex;

#include "light_sampler_grid_stream.hlsl"
#include "../../math/random.hlsl"

#define THREADX 128
#define THREADY 1
#define THREADZ 1

#ifdef LIGHTSAMPLERSTREAM_RES_MANYLIGHTS
groupshared uint lightsIDs[THREADX / 16]; //Assume 16 as smallest possible wave size
groupshared float lightsWeights[THREADX / 16];
groupshared float lightsTotalWeights[THREADX / 16];

#define LS_GRID_STREAM_THREADREDUCE 128 /**< Size of thread group when using parallel build */
#endif

/**
 * Create required internal configuration values.
 * Calculates the required number of grid cells that need to be dispatched through an indirect call to Build.
 */
[numthreads(1, 1, 1)]
void CalculateBounds()
{
    // Get the scene bounds
    const float3 sceneMin = g_LightSampler_MinBounds[0];
    const float3 sceneMax = g_LightSampler_MaxBounds[0];

    // Ensure each cell is square
    const float3 sceneExtent = sceneMax - sceneMin;
    const float largestAxis = max(sceneExtent.x, max(sceneExtent.y, sceneExtent.z));
    const float cellScale = largestAxis / g_LightSampler_Constants.maxCellsPerAxis;
    const float3 cellNum = ceil(sceneExtent / cellScale);

    // Clamp max number of lights to those actually available
    const uint lightsPerCell = min(g_LightSampler_Constants.maxNumLightsPerCell, getNumberLights());

    // Update internal configuration values
    uint3 groups = (uint3)cellNum;
    g_LightSampler_Configuration[0].numCells = uint4(groups, lightsPerCell);
    g_LightSampler_Configuration[0].cellSize = sceneExtent / cellNum;
    g_LightSampler_Configuration[0].sceneMin = sceneMin;
    g_LightSampler_Configuration[0].sceneExtent = sceneExtent;

    // Get total number of grid cells
    groups.x *= lightsPerCell;
#ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
    groups.x *= 8;
#endif
#ifdef LIGHTSAMPLERSTREAM_RES_MANYLIGHTS
    groups.x *= LS_GRID_STREAM_THREADREDUCE;
#endif

    const uint3 dispatch = uint3(THREADX, THREADY, THREADZ);
    groups = (groups + dispatch - 1.xxx) / dispatch;
    g_DispatchCommandBuffer[0].num_groups_x = groups.x;
    g_DispatchCommandBuffer[0].num_groups_y = groups.y;
    g_DispatchCommandBuffer[0].num_groups_z = groups.z;
}

/**
 * Build an internal grid.
 */
[numthreads(THREADX, THREADY, THREADZ)]
void Build(in uint3 did : SV_DispatchThreadID, in uint gid : SV_GroupIndex)
{
    uint reservoirsPerCell = g_LightSampler_Configuration[0].numCells.w;
#ifdef LIGHTSAMPLERSTREAM_RES_MANYLIGHTS
    // Use multiple threads to generate reservoirs then collapse them into final
    // Called per cell per reservoir per parralel thread
    reservoirsPerCell *= LS_GRID_STREAM_THREADREDUCE;
#endif
    uint3 cellID = did;
#ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
    // Called per cell per reservoir per octahedron face
    const uint resCellOffset = 8 * reservoirsPerCell;
    cellID.x /= resCellOffset;
#else
    // Called per cell per reservoir
    cellID.x /= reservoirsPerCell;
#endif
    if (any(cellID >= g_LightSampler_Configuration[0].numCells.xyz))
    {
        return;
    }

    // Check reservoirID against total lights
    const uint totalLights = getNumberLights();
#ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
    const uint cellFace = (did.x - (cellID.x * resCellOffset)) / reservoirsPerCell;
    uint reservoirID = did.x - (cellID.x * resCellOffset) - (cellFace * reservoirsPerCell);
#else
    uint reservoirID = did.x - (cellID.x * reservoirsPerCell);
#endif

    // Determine actual write location for current reservoir
#ifdef LIGHTSAMPLERSTREAM_RES_MANYLIGHTS
    const uint reservoirIndex = reservoirID / LS_GRID_STREAM_THREADREDUCE;
    // Ensure that reservoirs contain the same samples as when building with the non-parallel builds.
    //   This is importatant in order to distribute the potentially high importance lights stored near
    //   start of light list across all stored reservoirs. It does come at cost of preventing linear
    //   memory access from neighbouring threads
    reservoirID = reservoirIndex + ((reservoirID - (reservoirIndex * LS_GRID_STREAM_THREADREDUCE)) * g_LightSampler_Configuration[0].numCells.w);
#else
    const uint reservoirIndex = reservoirID;
#endif
    if (reservoirIndex >= totalLights || reservoirID >= reservoirsPerCell)
    {
        return;
    }

    // Calculate the bounding box for the current cell
    float3 extent;
    const float3 minBB = LightSamplerGrid::getCellBB(cellID, extent);

#ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
    uint cellIndex = LightSamplerGrid::getCellOctaIndex(cellID, cellFace) + reservoirIndex;
    const float3 normal = LightSamplerGrid::getCellNormal(cellFace);
#else
    const uint cellIndex = LightSamplerGrid::getCellIndex(cellID) + reservoirIndex;
#endif // LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING

    // Loop through all lights and sample
    Random randomNG = MakeRandom(cellIndex, g_FrameIndex);
    uint storedLight = -1;
    float storedLightWeight = 0.0f;
    float totalWeight = 0.0f;
    float j = randomNG.rand();
    float pNone = 1.0f;
    for (uint lightIndex = reservoirID; lightIndex < totalLights; lightIndex += reservoirsPerCell)
    {
        // Calculate sampled contribution for light
        Light selectedLight = getLight(lightIndex);
#ifdef LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        float sampleWeight = sampleLightVolumeNormal(selectedLight, minBB, extent, normal);
#else
        float sampleWeight = sampleLightVolume(selectedLight, minBB, extent);
#endif // LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING
        // Must avoid 0 samples at the start of the stream to avoid division by 0
        if (sampleWeight > 0.0f)
        {
            // weight = targetPDF/sourcePDF where sourcePDF = 1/numLights
            //   The numLights term cancels out later on
            totalWeight += sampleWeight;
            float p = sampleWeight / totalWeight;
            j -= p * pNone;
            pNone *= (1.0f - p);
            if (j <= 0.0f)
            {
                storedLight = lightIndex;
                storedLightWeight = sampleWeight;
                j = randomNG.rand();
                pNone = 1.0f;
            }
        }
    }

#ifdef LIGHTSAMPLERSTREAM_RES_MANYLIGHTS
    // Must get all parallel reservoirs and collapse into a single one.
    //  This is done by generating a CDF of all lights total weights and importance sampling it
    const float waveCDF = WavePrefixSum(totalWeight) + totalWeight;
    // Get total weight of all lanes
    totalWeight = WaveActiveSum(totalWeight);
    j = randomNG.rand();
    // A sample is considered to be the final sample if it passes the reservoir sample check and
    //   it is the first reservoir within the wave to do so
    bool valid = (j * totalWeight) <= waveCDF;
    uint isFirstCheck = WavePrefixCountBits(valid);
    // If the parallel thread count is equal to the wave size then we can just use wave
    //   instructions to collapse the full group. Otherwise we must use group shared memory
    //   to combine samples across waves.
    if (!valid || isFirstCheck != 0)
    {
        return;
    }
    if (WaveGetLaneCount() != LS_GRID_STREAM_THREADREDUCE)
    {
        // Only 1 thread is valid for each wave and it writes its sample out to the LDS
        //   The first valid thread in the first wave then reads in all the samples from LDS and combines them
        const uint waveID = gid / WaveGetLaneCount();
        // Need to use LDS to combine waves
        lightsTotalWeights[waveID] = totalWeight;
        lightsWeights[waveID] = storedLightWeight;
        lightsIDs[waveID] = storedLight;
        GroupMemoryBarrierWithGroupSync();
        if (waveID != 0)
        {
            return;
        }
        const uint waveCount = LS_GRID_STREAM_THREADREDUCE / WaveGetLaneCount();
        // Merge reservoirs from each wave using standard reservoir merge
        storedLight = -1;
        storedLightWeight = 0.0f;
        totalWeight = 0.0f;
        j = randomNG.rand();
        pNone = 1.0f;
        for (uint i = 0; i < waveCount; ++i)
        {
            float sampleWeight = lightsTotalWeights[i];
            if (sampleWeight > 0.0f)
            {
                totalWeight += sampleWeight;
                float p = sampleWeight / totalWeight;
                j -= p * pNone;
                pNone *= (1.0f - p);
                if (j <= 0.0f)
                {
                    storedLight = lightsIDs[i];
                    storedLightWeight = lightsWeights[i];
                    j = randomNG.rand();
                    pNone = 1.0f;
                }
            }
        }
    }
    else
    {
        // The parallel group size is equal to the wave size so the thread with the valid sample can
        //    just write direct to memory
    }
#endif
    // Write out store data
    g_LightSampler_CellsIndex[cellIndex] = storedLight;
#ifdef LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE
    float storeValue = (storedLightWeight > 0.0f) ? totalWeight / storedLightWeight : 0.0f;
    g_LightSampler_CellsReservoirs[cellIndex] = storeValue;
#else
    g_LightSampler_CellsReservoirs[cellIndex] = float2(storedLightWeight, totalWeight);
#endif
}
