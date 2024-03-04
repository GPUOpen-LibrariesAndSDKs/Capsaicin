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

#ifndef WORLD_SPACE_RESTIR_HLSL
#define WORLD_SPACE_RESTIR_HLSL

#include "../../lights/reservoir.hlsl"

// A define for the number of samples to draw during RIS:
#define kReservoir_SampleCount        8

// A define for the amount of spatial jittering on reservoir reuse:
#define kReservoir_SpatialJitter      sqrt(2.0f)

// A define for the bilateral threshold for temporal reservoir resampling:
#define kReservoir_BilateralThreshold 0.5f

RWStructuredBuffer<uint>  g_Reservoir_HashBuffer;
RWStructuredBuffer<uint>  g_Reservoir_HashCountBuffer;
RWStructuredBuffer<uint>  g_Reservoir_HashIndexBuffer;
RWStructuredBuffer<uint>  g_Reservoir_HashValueBuffer;
RWStructuredBuffer<uint4> g_Reservoir_HashListBuffer;
RWStructuredBuffer<uint>  g_Reservoir_HashListCountBuffer;
RWStructuredBuffer<uint>  g_Reservoir_PreviousHashBuffer;
RWStructuredBuffer<uint>  g_Reservoir_PreviousHashCountBuffer;
RWStructuredBuffer<uint>  g_Reservoir_PreviousHashIndexBuffer;
RWStructuredBuffer<uint>  g_Reservoir_PreviousHashValueBuffer;

RWStructuredBuffer<float4> g_Reservoir_IndirectSampleBuffer;
RWStructuredBuffer<uint>   g_Reservoir_IndirectSampleNormalBuffer;
RWStructuredBuffer<uint>   g_Reservoir_IndirectSampleMaterialBuffer;
RWStructuredBuffer<uint4>  g_Reservoir_IndirectSampleReservoirBuffer;
RWStructuredBuffer<uint>   g_Reservoir_PreviousIndirectSampleNormalBuffer;
RWStructuredBuffer<uint4>  g_Reservoir_PreviousIndirectSampleReservoirBuffer;

// Gets the size of the reservoir cell for the given world-space position.
float Reservoir_GetCellSize(in float3 position)
{
    float distance_to_cell = distance(g_Eye, position);
    float cell_size_step = distance_to_cell * g_WorldSpaceReSTIRConstants.cell_size;
    uint log_step_multiplier = uint(log2(1e3f * cell_size_step));
    return 1e-3f * exp2(log_step_multiplier);
}

// Gets the index and checksum for the reservoir cell in the current frame structure.
uint2 Reservoir_GetIndexAndHash(in float3 position)
{
    uint2 index_and_hash;
    float distance_to_cell = distance(g_Eye, position);
    float cell_size_step = distance_to_cell * g_WorldSpaceReSTIRConstants.cell_size;
    uint log_step_multiplier = uint(log2(1e3f * cell_size_step));
    float cell_size = 1e-3f * exp2(log_step_multiplier);
    int3 c = int3(floor(position / cell_size));
    index_and_hash.x = pcgHash(log_step_multiplier +
                       pcgHash(c.x + pcgHash(c.y + pcgHash(c.z))));
    index_and_hash.y = xxHash(log_step_multiplier +
                       xxHash(c.x + xxHash(c.y + xxHash(c.z))));
    index_and_hash.x = (index_and_hash.x % g_WorldSpaceReSTIRConstants.num_cells);
    index_and_hash.y = max(index_and_hash.y, 1);
    return index_and_hash;
}

// Gets the index and checksum for the reservoir cell in the previous frame structure.
uint2 Reservoir_GetPreviousIndexAndHash(in float3 position)
{
    uint2 index_and_hash;
    float distance_to_cell = distance(g_PreviousEye, position);
    float cell_size_step = distance_to_cell * g_WorldSpaceReSTIRConstants.cell_size;
    uint log_step_multiplier = uint(log2(1e3f * cell_size_step));
    float cell_size = 1e-3f * exp2(log_step_multiplier);
    int3 c = int3(floor(position / cell_size));
    index_and_hash.x = pcgHash(log_step_multiplier +
                       pcgHash(c.x + pcgHash(c.y + pcgHash(c.z))));
    index_and_hash.y = xxHash(log_step_multiplier +
                       xxHash(c.x + xxHash(c.y + xxHash(c.z))));
    index_and_hash.x = (index_and_hash.x % g_WorldSpaceReSTIRConstants.num_cells);
    index_and_hash.y = max(index_and_hash.y, 1);
    return index_and_hash;
}

// Inserts the entry inside the reservoir hash grid.
bool Reservoir_InsertEntry(in uint reservoir_index, in float3 position)
{
    uint i;
    uint2 index_and_hash = Reservoir_GetIndexAndHash(position);

    // Insert the reservoir into the hash table
    for (i = 0; i < g_WorldSpaceReSTIRConstants.num_entries_per_cell; ++i)
    {
        uint previous_hash;
        uint entry_index = i + index_and_hash.x * g_WorldSpaceReSTIRConstants.num_entries_per_cell;
        InterlockedCompareExchange(g_Reservoir_HashBuffer[entry_index], 0, index_and_hash.y, previous_hash);
        if (previous_hash == 0 || previous_hash == index_and_hash.y)
            break;  // found a suitable slot
    }
    if (i >= g_WorldSpaceReSTIRConstants.num_entries_per_cell)
        return false;   // out of memory :'(

    // Append a new hash entry...
    uint index_in_entry;
    uint entry_index = i + index_and_hash.x * g_WorldSpaceReSTIRConstants.num_entries_per_cell;
    InterlockedAdd(g_Reservoir_HashCountBuffer[entry_index], 1, index_in_entry);

    // ... and a new list element for compaction
    uint  list_index;
    uint4 list_element = uint4(reservoir_index, entry_index, index_in_entry, 0);
    InterlockedAdd(g_Reservoir_HashListCountBuffer[0], 1, list_index);

    g_Reservoir_HashListBuffer[list_index] = list_element;

    return true;
}

// Finds a reservoir cell entry inside the previous frame hash-grid cache.
uint Reservoir_FindPreviousEntry(in float3 position)
{
    uint i;
    uint2 index_and_hash = Reservoir_GetPreviousIndexAndHash(position);
    for (i = 0; i < g_WorldSpaceReSTIRConstants.num_entries_per_cell; ++i)
    {
        uint entry_index   = i + index_and_hash.x * g_WorldSpaceReSTIRConstants.num_entries_per_cell;
        uint previous_hash = g_Reservoir_PreviousHashBuffer[entry_index];
        if (previous_hash == 0)
            return kGI10_InvalidId; // not found
        if (previous_hash == index_and_hash.y)
            break;  // found entry
    }
    if (i >= g_WorldSpaceReSTIRConstants.num_entries_per_cell)
        return kGI10_InvalidId; // not found
    return i + index_and_hash.x * g_WorldSpaceReSTIRConstants.num_entries_per_cell;
}

// Packs the indirect sample.
float4 Reservoir_PackIndirectSample(in float3 origin, in float3 hit_position)
{
    uint4 packed_origin       = f32tof16(float4(origin, 1.0f));
    uint4 packed_hit_position = f32tof16(float4(hit_position, 1.0f));

    return asfloat((packed_origin << 16) | packed_hit_position);
}

// Unpacks the indirect sample.
void Reservoir_UnpackIndirectSample(in float4 packed_indirect_sample, out float3 origin, out float3 hit_position)
{
    uint4 packed_origin       = (asuint(packed_indirect_sample) >> 16);
    uint4 packed_hit_position = (asuint(packed_indirect_sample) & 0xFFFFu);

    origin       = f16tof32(packed_origin).xyz;
    hit_position = f16tof32(packed_hit_position).xyz;
}

#endif // WORLD_SPACE_RESTIR_HLSL
