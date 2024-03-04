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

#include "../../gpu_shared.h"
#include "../../math/transform.hlsl"

float4x4 g_ViewProjection;
float4x4 g_PrevViewProjection;

StructuredBuffer<Mesh>     g_MeshBuffer;
StructuredBuffer<Material> g_MaterialBuffer;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<float3x4> g_TransformBuffer;
StructuredBuffer<uint>     g_InstanceIDBuffer;
StructuredBuffer<float3x4> g_PrevTransformBuffer;

struct Params
{
    float4 position : SV_Position;
#if defined(HAS_SHADING_NORMAL) || defined(HAS_VERTEX_NORMAL)
    float3 normal     : NORMAL;
#endif
    float2 uv         : TEXCOORD;
    float3 world      : POSITION0;
    float4 current    : POSITION1;
    float4 previous   : POSITION2;
    uint   instanceID : INSTANCE_ID;
    uint   materialID : MATERIAL_ID;
};

Params main(in Vertex vertex, in uint drawID : gfx_DrawID)
{
    uint     instanceID = g_InstanceIDBuffer[drawID];
    Instance instance   = g_InstanceBuffer[instanceID];
    Mesh     mesh       = g_MeshBuffer[instance.mesh_index];

    float3x4 transform      = g_TransformBuffer[instance.transform_index];
    float3x4 prev_transform = g_PrevTransformBuffer[instance.transform_index];

    float3 position      = transformPoint(vertex.position.xyz, transform);
    float3 prev_position = transformPoint(vertex.position.xyz, prev_transform);

    Params params;
    params.position = mul(g_ViewProjection, float4(position, 1.0f));
#if defined(HAS_SHADING_NORMAL) || defined(HAS_VERTEX_NORMAL)
    params.normal     = transformNormal(vertex.normal.xyz, transform);
#endif
    params.uv         = vertex.uv;
    params.world      = position;
    params.current    = params.position;
    params.previous   = mul(g_PrevViewProjection, float4(prev_position, 1.0f));
    params.instanceID = instanceID;
    params.materialID = instance.material_index;

    return params;
}
