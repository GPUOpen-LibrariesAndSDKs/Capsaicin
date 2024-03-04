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

Texture2D g_VisibilityBuffer;
Texture2D g_DepthBuffer;

StructuredBuffer<uint> g_IndexBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
StructuredBuffer<Mesh> g_MeshBuffer;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Material> g_MaterialBuffer;

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;

uint g_MaterialMode;

#include "../../materials/material_evaluation.hlsl"
#include "../../geometry/geometry.hlsl"
#include "../../geometry/mesh.hlsl"

float4 DebugMaterial(in float4 pos : SV_Position) : SV_Target
{
    uint2 did = uint2(pos.xy);

    if (g_DepthBuffer[did].x >= 1.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float4 visibility = g_VisibilityBuffer.Load(int3(did, 0));
    uint instanceID = asuint(visibility.z);
    uint primitiveID = asuint(visibility.w);

    Instance instance = g_InstanceBuffer[instanceID];
    Mesh mesh = g_MeshBuffer[instance.mesh_index];

    // Get UV values from buffers
    UVs uvs = fetchUVs(mesh, primitiveID);
    float2 mesh_uv = interpolate(uvs.uv0, uvs.uv1, uvs.uv2, visibility.xy);

    Material material = g_MaterialBuffer[instance.material_index];
    MaterialEvaluated materialEvaluated = MakeMaterialEvaluated(material, mesh_uv);
    switch (g_MaterialMode)
    {
        case 0:
            return float4(materialEvaluated.albedo, 1.0f);
        case 1:
            return float4(materialEvaluated.metallicity.xxx, 1.0f);
        case 2:
            return float4(materialEvaluated.roughness.xxx, 1.0f);
        default:
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}
