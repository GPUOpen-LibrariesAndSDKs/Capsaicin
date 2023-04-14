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

#include "../../lights/lights_shared.h"
#include "../../math/geometry.hlsl"
#include "../../math/pack.hlsl"

RWStructuredBuffer<Light> g_LightBuffer;
RWStructuredBuffer<uint> g_LightBufferSize;

StructuredBuffer<Material> g_MaterialBuffer;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_LinearSampler;

struct Params
{
    float4 position    : SV_Position;
    float2 uv          : TEXCOORD;
    uint   materialID  : MATERIAL_ID;
};

[maxvertexcount(3)]
void main(in triangle Params params[3], in uint primitiveID : SV_PrimitiveID, inout TriangleStream<Params> triangle_stream)
{
    uint material_index = params[0].materialID;

    // Get any light maps
    float4 radiance = g_MaterialBuffer[material_index].emissivity;
    uint tex = asuint(radiance.w);
    float3 emissivity = radiance.xyz;
    if (tex != uint(-1))
    {
        // Get texture dimensions in order to determine LOD of visible solid angle
        float2 size;
        g_TextureMaps[NonUniformResourceIndex(tex)].GetDimensions(size.x, size.y);

        // Approximate ray cone projection (ray tracing gems chapter 20)
        float2 edgeUV0 = params[1].uv - params[0].uv;
        float2 edgeUV1 = params[2].uv - params[0].uv;
        float area_uv = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
        float offset = 0.5f * log2(area_uv);

        // Calculate texture LOD based on projected area
        float2 uv = interpolate(params[0].uv, params[1].uv, params[2].uv, (1.0f / 3.0f).xx);
        emissivity *= g_TextureMaps[NonUniformResourceIndex(tex)].SampleLevel(g_LinearSampler, uv, offset).xyz;
    }

    bool is_emissive = any(emissivity > 0.0f);
    // Compute number of items to append for the whole wave
    uint lane_append_offset = WavePrefixCountBits(is_emissive);
    uint append_count = WaveActiveCountBits(is_emissive);
    // Update the output location for this whole wave
    uint append_offset;
    if (WaveIsFirstLane())
    {
        // this way, we only issue one atomic for the entire wave, which reduces contention
        // and keeps the output data for each lane in this wave together in the output buffer
        InterlockedAdd(g_LightBufferSize[0], append_count, append_offset);
    }
    append_offset = WaveReadLaneFirst(append_offset); // broadcast value
    append_offset += lane_append_offset; // and add in the offset for this lane
    if (is_emissive)
    {
        Light light;
        light.radiance = radiance;
        light.v1       = float4(params[0].position.xyz, packUVs(params[0].uv));
        light.v2       = float4(params[1].position.xyz, packUVs(params[1].uv));
        light.v3       = float4(params[2].position.xyz, packUVs(params[2].uv));

        g_LightBuffer[append_offset] = light; // write to the offset location for this lane

        triangle_stream.Append(params[0]);
        triangle_stream.Append(params[1]);
        triangle_stream.Append(params[2]);

        triangle_stream.RestartStrip();
    }
}
