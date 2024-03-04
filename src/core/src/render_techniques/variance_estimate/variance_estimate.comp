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
#define GROUP_SIZE 16

#include "../../math/math.hlsl"
#include "../../math/color.hlsl"

uint2 g_BufferDimensions;

Texture2D g_ColorBuffer;

RWStructuredBuffer<float> g_MeanBuffer;
RWStructuredBuffer<float> g_SquareBuffer;
RWStructuredBuffer<float> g_ResultBuffer;

groupshared float lds_BlockBuffer[GROUP_SIZE * GROUP_SIZE];
groupshared uint lds_writes;

float BlockReduceSum(in float value, in uint local_id, in uint group_size)
{
    value = WaveActiveSum(value);

    // Combine values across the group
    const uint groupSize = GROUP_SIZE * GROUP_SIZE;
    for (uint j = WaveGetLaneCount(); j < groupSize; j *= WaveGetLaneCount())
    {
        // Since we work on square tiles its possible that some waves don't write to lds as they have no valid pixels
        //   To ensure that invalid values arent read from lds we use an atomic to count the actual lds writes
        if (local_id == 0)
        {
            // Clear atomic
            InterlockedAnd(lds_writes, 0);
        }
        GroupMemoryBarrierWithGroupSync();

        // Use local data share to combine across waves
        if (WaveIsFirstLane())
        {
            uint waveID;
            InterlockedAdd(lds_writes, 1, waveID);
            lds_BlockBuffer[waveID] = value;

        }
        GroupMemoryBarrierWithGroupSync();

        uint numWaves = lds_writes;
        if (local_id >= numWaves)
        {
            break;
        }

        // Use the current wave to combine across group
        value = lds_BlockBuffer[local_id];
        value = WaveActiveSum(value);
    }

    return value;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ComputeMean(in uint2 did : SV_DispatchThreadID, in uint local_id : SV_GroupIndex, in uint2 group_id : SV_GroupID)
{
    float3 color = (all(did < g_BufferDimensions) ? g_ColorBuffer[did].xyz : (float3)0);
    float  luma  = (any(isnan(color)) ? 0.0f : luminance(color));

    luma = BlockReduceSum(luma, local_id, GROUP_SIZE * GROUP_SIZE);

    if (local_id == 0)
    {
        uint block_count = (g_BufferDimensions.x + GROUP_SIZE - 1) / GROUP_SIZE;
        uint block_index = group_id.x + group_id.y * block_count;

        g_MeanBuffer[block_index] = luma;
    }
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ComputeDistance(in uint2 did : SV_DispatchThreadID, in uint local_id : SV_GroupIndex, in uint2 group_id : SV_GroupID)
{
    uint   pixel_count = g_BufferDimensions.x * g_BufferDimensions.y;
    float3 color       = (all(did < g_BufferDimensions) ? g_ColorBuffer[did].xyz : (float3)0);
    float  dist2       = squared((any(isnan(color)) ? 0.0f : luminance(color)) - g_MeanBuffer[0] / pixel_count);

    dist2 = BlockReduceSum(dist2, local_id, GROUP_SIZE * GROUP_SIZE);

    if (local_id == 0)
    {
        uint block_count = (g_BufferDimensions.x + GROUP_SIZE - 1) / GROUP_SIZE;
        uint block_index = group_id.x + group_id.y * block_count;

        g_SquareBuffer[block_index] = dist2;
    }
}

[numthreads(1, 1, 1)]
void ComputeDeviation()
{
    uint  pixel_count = g_BufferDimensions.x * g_BufferDimensions.y;
    float std_dev     = sqrt(g_SquareBuffer[0] / pixel_count);
    float mean        = g_MeanBuffer[0] / pixel_count;

    g_ResultBuffer[0] = (mean != 0.0f ? std_dev / mean : 0.0f); // coefficient of variation
}
