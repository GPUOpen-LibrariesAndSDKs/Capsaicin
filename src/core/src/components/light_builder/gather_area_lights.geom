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

#include "../../lights/lights_shared.h"
#include "../../geometry/geometry.hlsl"
#include "../../math/pack.hlsl"

RWStructuredBuffer<Light> g_LightBuffer;
RWStructuredBuffer<uint>  g_LightBufferSize;
RWStructuredBuffer<uint>  g_LightInstanceBuffer;
RWStructuredBuffer<uint>  g_LightInstancePrimitiveBuffer;

StructuredBuffer<Material> g_MaterialBuffer;
Texture2D                  g_TextureMaps[] : register(space99);
SamplerState               g_TextureSampler;
uint                       g_LightCount;

struct Params
{
    float4 position   : SV_Position;
    float2 uv         : TEXCOORD;
    uint   instanceID : INSTANCE_ID;
    uint   materialID : MATERIAL_ID;
};

bool CheckIsEmissive(in uint material_index, in float2 uv0, in float2 uv1, in float2 uv2, out float4 radiance)
{
    // Get any light maps
    radiance = g_MaterialBuffer[material_index].emissivity;
    uint tex = asuint(radiance.w);
    float3 emissivity = radiance.xyz;
    if (tex != uint(-1))
    {
        // Get texture dimensions in order to determine LOD of visible solid angle
        float2 size;
        g_TextureMaps[NonUniformResourceIndex(tex)].GetDimensions(size.x, size.y);

        // Approximate ray cone projection (ray tracing gems chapter 20)
        float2 edgeUV0 = uv1 - uv0;
        float2 edgeUV1 = uv2 - uv0;
        float area_uv = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
        float offset = 0.5f * log2(area_uv);

        // Calculate texture LOD based on projected area
        float2 uv = interpolate(uv0, uv1, uv2, (1.0f / 3.0f).xx);
        emissivity *= g_TextureMaps[NonUniformResourceIndex(tex)].SampleLevel(g_TextureSampler, uv, offset).xyz;
    }

    return any(emissivity > 0.0f);
}

[maxvertexcount(3)]
void CountAreaLights(in triangle Params params[3], in uint primitiveID : SV_PrimitiveID, inout TriangleStream<Params> triangle_stream)
{
    uint instance_index = params[0].instanceID;
    uint material_index = params[0].materialID;

    // Check whether the primitive is emissive
    float4 radiance;
    bool   is_emissive =
        CheckIsEmissive(material_index, params[0].uv, params[1].uv, params[2].uv, radiance);

    // Compute number of items to append for the whole wave
    uint append_count = WaveActiveCountBits(is_emissive);
    if (WaveIsFirstLane())
    {
        // This way, we only issue one atomic for the entire wave,
        // which reduces contention
        InterlockedAdd(g_LightBufferSize[0], append_count);
    }

    // Write out the number of emissive primitives
    uint idx = primitiveID + g_LightInstanceBuffer[instance_index];
    g_LightInstancePrimitiveBuffer[idx] = (is_emissive ? 1 : 0);

    // This is useless since there's no pixel shader nor render target,
    // but required by DXC so the shader compiles
    if (is_emissive)
    {
        triangle_stream.Append(params[0]);
        triangle_stream.Append(params[1]);
        triangle_stream.Append(params[2]);

        triangle_stream.RestartStrip();
    }
}

[maxvertexcount(3)]
void ScatterAreaLights(in triangle Params params[3], in uint primitiveID : SV_PrimitiveID, inout TriangleStream<Params> triangle_stream)
{
    uint instance_index = params[0].instanceID;
    uint material_index = params[0].materialID;

    // Check whether the primitive is emissive
    float4 radiance;
    bool   is_emissive =
        CheckIsEmissive(material_index, params[0].uv, params[1].uv, params[2].uv, radiance);

    // Write the area light to memory if needed
    if (is_emissive)
    {
        Light light;
        light.radiance = radiance;
        light.v1       = float4(params[0].position.xyz, packUVs(params[0].uv));
        light.v2       = float4(params[1].position.xyz, packUVs(params[1].uv));
        light.v3       = float4(params[2].position.xyz, packUVs(params[2].uv));

        uint idx           = primitiveID + g_LightInstanceBuffer[instance_index];
        uint append_offset = (g_LightInstancePrimitiveBuffer[idx] += g_LightCount);
        g_LightBuffer[append_offset] = light; // write to the offset location for this lane

        triangle_stream.Append(params[0]);
        triangle_stream.Append(params[1]);
        triangle_stream.Append(params[2]);

        triangle_stream.RestartStrip();
    }
}
