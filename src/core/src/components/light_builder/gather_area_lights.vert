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
#include "../../math/transform.hlsl"

StructuredBuffer<Mesh>     g_MeshBuffer;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<float3x4> g_TransformBuffer;

StructuredBuffer<uint> g_InstanceIDBuffer;

struct Params
{
    float4 position   : SV_Position;
    float2 uv         : TEXCOORD;
    uint   instanceID : INSTANCE_ID;
    uint   materialID : MATERIAL_ID;
};

Params VS(in Vertex vertex, in uint drawID)
{
    uint     instanceID = g_InstanceIDBuffer[drawID];
    Instance instance   = g_InstanceBuffer[instanceID];

    float3x4 transform = g_TransformBuffer[instance.transform_index];
    float3   position  = transformPoint(vertex.position.xyz, transform);

    Params params;
    params.position   = float4(position, 1.0f);
    params.uv         = vertex.uv.xy;
    params.instanceID = instanceID;
    params.materialID = instance.material_index;

    return params;
}

Params CountAreaLights(in Vertex vertex, in uint drawID : gfx_DrawID)
{
    return VS(vertex, drawID);
}

Params ScatterAreaLights(in Vertex vertex, in uint drawID : gfx_DrawID)
{
    return VS(vertex, drawID);
}
