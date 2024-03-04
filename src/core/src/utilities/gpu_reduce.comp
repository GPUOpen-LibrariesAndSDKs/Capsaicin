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

#include "../gpu_shared.h"

#ifndef TYPE
#define TYPE float
#endif
#ifndef OP
#define OP 0
#endif
#define GROUP_SIZE 256
#define KEYS_PER_THREAD 4

StructuredBuffer<TYPE> g_InputBuffer;
RWStructuredBuffer<TYPE> g_OutputBuffer;

#if OP==0
TYPE Combine(TYPE a, TYPE b)
{
    return a + b;
}

TYPE CombineWave(TYPE a)
{
    return WaveActiveSum(a);
}
#elif OP==1
TYPE Combine(TYPE a, TYPE b)
{
    return min(a, b);
}

TYPE CombineWave(TYPE a)
{
    return WaveActiveMin(a);
}
#elif OP==2
TYPE Combine(TYPE a, TYPE b)
{
    return max(a, b);
}

TYPE CombineWave(TYPE a)
{
    return WaveActiveMax(a);
}
#elif OP==3
TYPE Combine(TYPE a, TYPE b)
{
    return a * b;
}

TYPE CombineWave(TYPE a)
{
    return WaveActiveProduct(a);
}
#endif

groupshared TYPE lds[GROUP_SIZE / 16]; //Assume 16 as smallest possible wave size

// Reduce sum template function
void BlockReduceType(uint gtid, uint gid, uint count)
{
    // Loop through each input key and combine
    const uint prefixGroupKeys = gid * KEYS_PER_THREAD * GROUP_SIZE;
    uint index = gtid + prefixGroupKeys;
    if (index >= count)
    {
        return;
    }
    TYPE result = g_InputBuffer[index];;
    for (uint i = 1; i < KEYS_PER_THREAD; i++)
    {
        index += GROUP_SIZE;
        if (index >= count)
        {
            break;
        }
        TYPE value = g_InputBuffer[index];
        result = Combine(result, value);
    }

    // Combine values across the wave
    result = CombineWave(result);

    // Combine values across the group
    const uint keysInGroup = count - prefixGroupKeys;
    const uint validThreadsInGroup = min(keysInGroup, GROUP_SIZE);
    for (uint j = validThreadsInGroup; j > WaveGetLaneCount();)
    {
        // Use local data share to combine across waves
        if (WaveIsFirstLane())
        {
            const uint waveID = gtid / WaveGetLaneCount();
            lds[waveID] = result;
        }
        GroupMemoryBarrierWithGroupSync();

        j = (j + WaveGetLaneCount() - 1) / WaveGetLaneCount();
        if (gtid >= j)
        {
            break;
        }

        // Use the current wave to combine across group
        result = lds[gtid];
        result = CombineWave(result);
    }

    // Write out final result
    if (gtid == 0)
    {
        g_OutputBuffer[gid] = result;
    }
}

uint g_Count;

[numthreads(GROUP_SIZE, 1, 1)]
void BlockReduce(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID)
{
    BlockReduceType(gtid, gid, g_Count);
}

StructuredBuffer<uint> g_InputLength;

[numthreads(GROUP_SIZE, 1, 1)]
void BlockReduceIndirect(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID)
{
    BlockReduceType(gtid, gid, g_InputLength[0]);
}

RWStructuredBuffer<uint4> g_Dispatch1;
RWStructuredBuffer<uint4> g_Dispatch2;
RWStructuredBuffer<uint> g_InputLength1;
RWStructuredBuffer<uint> g_InputLength2;

[numthreads(1, 1, 1)]
void GenerateDispatches()
{
    const uint keysPerGroup = GROUP_SIZE * KEYS_PER_THREAD;
    uint groupsPass1 = (g_InputLength[0] + keysPerGroup - 1) / keysPerGroup;
    uint groupsPass2 = (groupsPass1 + keysPerGroup - 1) / keysPerGroup;
    g_Dispatch1[0] = uint4(groupsPass1, 1, 1, 0);
    g_Dispatch2[0] = uint4(groupsPass2, 1, 1, 0);
    g_InputLength1[0] = groupsPass1;
    g_InputLength2[0] = groupsPass2;
}
