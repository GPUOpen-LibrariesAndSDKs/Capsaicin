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

#ifndef MESH_HLSL
#define MESH_HLSL

/*
// Requires the following data to be defined in any shader that uses this file
StructuredBuffer<uint> g_IndexBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
*/

#include "../gpu_shared.h"

struct Triangle
{
    float3 v0;
    float3 v1;
    float3 v2;
};

struct TriangleUV
{
    float3 v0;
    float3 v1;
    float3 v2;
    float2 uv0;
    float2 uv1;
    float2 uv2;
};

struct TriangleNorm
{
    float3 v0;
    float3 v1;
    float3 v2;
    float3 n0;
    float3 n1;
    float3 n2;
};

struct TriangleNormUV
{
    float3 v0;
    float3 v1;
    float3 v2;
    float3 n0;
    float3 n1;
    float3 n2;
    float2 uv0;
    float2 uv1;
    float2 uv2;
};

struct UVs
{
    float2 uv0;
    float2 uv1;
    float2 uv2;
};

/**
 * Fetch the vertices for a given triangle.
 * @param mesh           The mesh the triangle is located within.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
Triangle fetchVertices(Mesh mesh, uint primitiveIndex)
{
    // Get index buffer values
    uint i0 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 0] + mesh.vertex_offset_idx;
    uint i1 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 1] + mesh.vertex_offset_idx;
    uint i2 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 2] + mesh.vertex_offset_idx;

    // Get vertex values from buffers
    float3 v0 = g_VertexBuffer[i0].position.xyz;
    float3 v1 = g_VertexBuffer[i1].position.xyz;
    float3 v2 = g_VertexBuffer[i2].position.xyz;

    Triangle ret = {v0, v1, v2};
    return ret;
}

/**
 * Fetch the vertices and UVs for a given triangle.
 * @param mesh           The mesh the triangle is located within.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleUV fetchVerticesUV(Mesh mesh, uint primitiveIndex)
{
    // Get index buffer values
    uint i0 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 0] + mesh.vertex_offset_idx;
    uint i1 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 1] + mesh.vertex_offset_idx;
    uint i2 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 2] + mesh.vertex_offset_idx;

    // Get vertex values from buffers
    float3 v0 = g_VertexBuffer[i0].position.xyz;
    float3 v1 = g_VertexBuffer[i1].position.xyz;
    float3 v2 = g_VertexBuffer[i2].position.xyz;

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].uv;
    float2 uv1 = g_VertexBuffer[i1].uv;
    float2 uv2 = g_VertexBuffer[i2].uv;

    TriangleUV ret = {v0, v1, v2, uv0, uv1, uv2};
    return ret;
}

/**
 * Fetch the vertices and normals for a given triangle.
 * @param mesh           The mesh the triangle is located within.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleNorm fetchVerticesNorm(Mesh mesh, uint primitiveIndex)
{
    // Get index buffer values
    uint i0 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 0] + mesh.vertex_offset_idx;
    uint i1 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 1] + mesh.vertex_offset_idx;
    uint i2 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 2] + mesh.vertex_offset_idx;

    // Get vertex values from buffers
    float3 v0 = g_VertexBuffer[i0].position.xyz;
    float3 v1 = g_VertexBuffer[i1].position.xyz;
    float3 v2 = g_VertexBuffer[i2].position.xyz;

    // Get normal values from buffers
    float3 n0 = g_VertexBuffer[i0].normal.xyz;
    float3 n1 = g_VertexBuffer[i1].normal.xyz;
    float3 n2 = g_VertexBuffer[i2].normal.xyz;

    TriangleNorm ret = {v0, v1, v2, n0, n1, n2};
    return ret;
}

/**
 * Fetch the vertices, normals and UVs for a given triangle.
 * @param mesh           The mesh the triangle is located within.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleNormUV fetchVerticesNormUV(Mesh mesh, uint primitiveIndex)
{
    // Get index buffer values
    uint i0 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 0] + mesh.vertex_offset_idx;
    uint i1 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 1] + mesh.vertex_offset_idx;
    uint i2 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 2] + mesh.vertex_offset_idx;

    // Get vertex values from buffers
    float3 v0 = g_VertexBuffer[i0].position.xyz;
    float3 v1 = g_VertexBuffer[i1].position.xyz;
    float3 v2 = g_VertexBuffer[i2].position.xyz;

    // Get normal values from buffers
    float3 n0 = g_VertexBuffer[i0].normal.xyz;
    float3 n1 = g_VertexBuffer[i1].normal.xyz;
    float3 n2 = g_VertexBuffer[i2].normal.xyz;

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].uv;
    float2 uv1 = g_VertexBuffer[i1].uv;
    float2 uv2 = g_VertexBuffer[i2].uv;

    TriangleNormUV ret = {v0, v1, v2, n0, n1, n2, uv0, uv1, uv2};
    return ret;
}

/**
 * Fetch the UVs for a given triangle.
 * @param mesh           The mesh the triangle is located within.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
UVs fetchUVs(Mesh mesh, uint primitiveIndex)
{
    // Get index buffer values
    uint i0 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 0] + mesh.vertex_offset_idx;
    uint i1 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 1] + mesh.vertex_offset_idx;
    uint i2 = g_IndexBuffer[mesh.index_offset_idx + 3 * primitiveIndex + 2] + mesh.vertex_offset_idx;

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].uv;
    float2 uv1 = g_VertexBuffer[i1].uv;
    float2 uv2 = g_VertexBuffer[i2].uv;

    UVs ret = {uv0, uv1, uv2};
    return ret;
}

#endif // MESH_HLSL
