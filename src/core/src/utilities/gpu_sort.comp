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

#if OP==1
#define kRS_Descending
#endif
#define FFX_HLSL
#include "../../../../third_party/ffx-parallelsort/FFX_ParallelSort.h"
// Need to include ffx-parallelsort twice to get 2 different versions (with and without payload)
namespace Payload
{
#define kRS_ValueCopy
#include "../../../../third_party/ffx-parallelsort/FFX_ParallelSort.h"
#undef kRS_ValueCopy
}

StructuredBuffer<FFX_ParallelSortCB> CBuffer; // Constant buffer
uint CShiftBit;

RWStructuredBuffer<uint> SrcBuffer; // The unsorted keys or scan data
RWStructuredBuffer<uint> SrcPayload; // The payload data

RWStructuredBuffer<uint> SumTable; // The sum table we will write sums to
RWStructuredBuffer<uint> ReduceTable; // The reduced sum table we will write sums to

RWStructuredBuffer<uint> DstBuffer; // The sorted keys or prefixed data
RWStructuredBuffer<uint> DstPayload; // The sorted payload data

RWStructuredBuffer<uint> ScanSrc; // Source for Scan Data
RWStructuredBuffer<uint> ScanDst; // Destination for Scan Data
RWStructuredBuffer<uint> ScanScratch; // Scratch data for Scan

StructuredBuffer<uint> numKeys; // Number of keys to sort for indirect execution
RWStructuredBuffer<FFX_ParallelSortCB> CBufferUAV; // UAV for constant buffer parameters for indirect execution
RWStructuredBuffer<uint> CountScatterArgs; // Count and Scatter Args for indirect execution
RWStructuredBuffer<uint> ReduceScanArgs; // Reduce and Scan Args for indirect execution

[numthreads(1, 1, 1)]
void setupIndirectParameters(uint localID : SV_GroupThreadID)
{
    FFX_ParallelSort_SetupIndirectParams(numKeys[0], 800, CBufferUAV, CountScatterArgs, ReduceScanArgs);
}

[numthreads(FFX_PARALLELSORT_THREADGROUP_SIZE, 1, 1)]
void count(uint localID : SV_GroupThreadID, uint groupID : SV_GroupID)
{
    // Call the uint version of the count part of the algorithm
    FFX_ParallelSort_Count_uint(localID, groupID, CBuffer[0], CShiftBit, SrcBuffer, SumTable);
}

[numthreads(FFX_PARALLELSORT_THREADGROUP_SIZE, 1, 1)]
void countReduce(uint localID : SV_GroupThreadID, uint groupID : SV_GroupID)
{
    // Call the reduce part of the algorithm
    FFX_ParallelSort_ReduceCount(localID, groupID, CBuffer[0], SumTable, ReduceTable);
}

[numthreads(FFX_PARALLELSORT_THREADGROUP_SIZE, 1, 1)]
void scan(uint localID : SV_GroupThreadID, uint groupID : SV_GroupID)
{
    uint BaseIndex = FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE * groupID;
    FFX_ParallelSort_ScanPrefix(CBuffer[0].NumScanValues, localID, groupID, 0, BaseIndex, false,
                                CBuffer[0], ScanSrc, ScanDst, ScanScratch);
}

[numthreads(FFX_PARALLELSORT_THREADGROUP_SIZE, 1, 1)]
void scanAdd(uint localID : SV_GroupThreadID, uint groupID : SV_GroupID)
{
    // When doing adds, we need to access data differently because reduce
    // has a more specialized access pattern to match optimized count
    // Access needs to be done similarly to reduce
    // Figure out what bin data we are reducing
    uint BinID = groupID / CBuffer[0].NumReduceThreadgroupPerBin;
    uint BinOffset = BinID * CBuffer[0].NumThreadGroups;

    // Get the base index for this thread group
    uint BaseIndex = (groupID % CBuffer[0].NumReduceThreadgroupPerBin) * FFX_PARALLELSORT_ELEMENTS_PER_THREAD * FFX_PARALLELSORT_THREADGROUP_SIZE;

    FFX_ParallelSort_ScanPrefix(CBuffer[0].NumThreadGroups, localID, groupID, BinOffset, BaseIndex, true,
                                CBuffer[0], ScanSrc, ScanDst, ScanScratch);
}

[numthreads(FFX_PARALLELSORT_THREADGROUP_SIZE, 1, 1)]
void scatter(uint localID : SV_GroupThreadID, uint groupID : SV_GroupID)
{
    FFX_ParallelSort_Scatter_uint(localID, groupID, CBuffer[0], CShiftBit, SrcBuffer, DstBuffer, SumTable);
}

[numthreads(FFX_PARALLELSORT_THREADGROUP_SIZE, 1, 1)]
void scatterPayload(uint localID : SV_GroupThreadID, uint groupID : SV_GroupID)
{
    Payload::FFX_ParallelSortCB PayloadCBuffer;
    PayloadCBuffer.NumKeys = CBuffer[0].NumKeys;
    PayloadCBuffer.NumBlocksPerThreadGroup = CBuffer[0].NumBlocksPerThreadGroup;
    PayloadCBuffer.NumThreadGroups = CBuffer[0].NumThreadGroups;
    PayloadCBuffer.NumThreadGroupsWithAdditionalBlocks = CBuffer[0].NumThreadGroupsWithAdditionalBlocks;
    PayloadCBuffer.NumReduceThreadgroupPerBin = CBuffer[0].NumReduceThreadgroupPerBin;
    PayloadCBuffer.NumScanValues = CBuffer[0].NumScanValues;
    Payload::FFX_ParallelSort_Scatter_uint(localID, groupID, PayloadCBuffer, CShiftBit, SrcBuffer, DstBuffer, SumTable, SrcPayload, DstPayload);
}
