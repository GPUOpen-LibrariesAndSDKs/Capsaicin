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
uint g_VertexDataIndex;
// Additionally requires following data if HAS_PREVIOUS_INTERSECT is defined
uint g_PrevVertexDataIndex;
*/

#include "gpu_shared.h"

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
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
Triangle fetchVertices(Instance instance, uint primitiveIndex)
{
    // Get index values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_VertexDataIndex];

    // Get position values from buffers
    float3 v0 = g_VertexBuffer[i0].getPosition();
    float3 v1 = g_VertexBuffer[i1].getPosition();
    float3 v2 = g_VertexBuffer[i2].getPosition();

    Triangle ret = {v0, v1, v2};
    return ret;
}

/**
 * Fetch the vertices and UVs for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleUV fetchVerticesUV(Instance instance, uint primitiveIndex)
{
    // Get index values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_VertexDataIndex];

    // Get position values from buffers
    float3 v0 = g_VertexBuffer[i0].getPosition();
    float3 v1 = g_VertexBuffer[i1].getPosition();
    float3 v2 = g_VertexBuffer[i2].getPosition();

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].getUV();
    float2 uv1 = g_VertexBuffer[i1].getUV();
    float2 uv2 = g_VertexBuffer[i2].getUV();

    TriangleUV ret = {v0, v1, v2, uv0, uv1, uv2};
    return ret;
}

/**
 * Fetch the vertices and normals for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleNorm fetchVerticesNorm(Instance instance, uint primitiveIndex)
{
    // Get index values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_VertexDataIndex];

    // Get position values from buffers
    float3 v0 = g_VertexBuffer[i0].getPosition();
    float3 v1 = g_VertexBuffer[i1].getPosition();
    float3 v2 = g_VertexBuffer[i2].getPosition();

    // Get normal values from buffers
    float3 n0 = g_VertexBuffer[i0].getNormal();
    float3 n1 = g_VertexBuffer[i1].getNormal();
    float3 n2 = g_VertexBuffer[i2].getNormal();

    TriangleNorm ret = {v0, v1, v2, n0, n1, n2};
    return ret;
}

/**
 * Fetch the vertices, normals and UVs for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleNormUV fetchVerticesNormUV(Instance instance, uint primitiveIndex)
{
    // Get index values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_VertexDataIndex];

    // Get position values from buffers
    float3 v0 = g_VertexBuffer[i0].getPosition();
    float3 v1 = g_VertexBuffer[i1].getPosition();
    float3 v2 = g_VertexBuffer[i2].getPosition();

    // Get normal values from buffers
    float3 n0 = g_VertexBuffer[i0].getNormal();
    float3 n1 = g_VertexBuffer[i1].getNormal();
    float3 n2 = g_VertexBuffer[i2].getNormal();

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].getUV();
    float2 uv1 = g_VertexBuffer[i1].getUV();
    float2 uv2 = g_VertexBuffer[i2].getUV();

    TriangleNormUV ret = {v0, v1, v2, n0, n1, n2, uv0, uv1, uv2};
    return ret;
}

/**
 * Fetch the UVs for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
UVs fetchUVs(Instance instance, uint primitiveIndex)
{
    // Get index buffer values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_VertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_VertexDataIndex];

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].getUV();
    float2 uv1 = g_VertexBuffer[i1].getUV();
    float2 uv2 = g_VertexBuffer[i2].getUV();

    UVs ret = {uv0, uv1, uv2};
    return ret;
}

#ifdef HAS_PREVIOUS_INTERSECT

/**
 * Fetch the vertices from last frame for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
Triangle fetchPrevVertices(Instance instance, uint primitiveIndex)
{
    // Get index values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];

    // Get position values from buffers
    float3 v0 = g_VertexBuffer[i0].getPosition();
    float3 v1 = g_VertexBuffer[i1].getPosition();
    float3 v2 = g_VertexBuffer[i2].getPosition();

    Triangle ret = {v0, v1, v2};
    return ret;
}

/**
 * Checks if instance was previously animated last frame.
 * @param instance The instance the triangle belongs to.
 */
bool isInstanceVolatile(Instance instance)
{
    return (g_VertexDataIndex != g_PrevVertexDataIndex) &&
        (instance.vertex_offset_idx[0] != instance.vertex_offset_idx[1]);
}

/**
 * Fetch the vertices from last frame for a given triangle. Avoids redundant
 * fetch if these are guaranteed to be equal to current frame vertices.
 * @param instance       The instance the triangle belongs to.
 * @param vertices       The current frame vertices of the given triangle.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
Triangle fetchPrevVertices(Instance instance, Triangle vertices, uint primitiveIndex)
{
    if (!isInstanceVolatile(instance))
        return vertices;

    return fetchPrevVertices(instance, primitiveIndex);
}

/**
 * Fetch the vertices and UVs from last frame for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleUV fetchPrevVerticesUV(Instance instance, uint primitiveIndex)
{
    // Get index values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];

    // Get position values from buffers
    float3 v0 = g_VertexBuffer[i0].getPosition();
    float3 v1 = g_VertexBuffer[i1].getPosition();
    float3 v2 = g_VertexBuffer[i2].getPosition();

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].getUV();
    float2 uv1 = g_VertexBuffer[i1].getUV();
    float2 uv2 = g_VertexBuffer[i2].getUV();

    TriangleUV ret = {v0, v1, v2, uv0, uv1, uv2};
    return ret;
}

/**
 * Fetch the vertices and normals from last frame for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleNorm fetchPrevVerticesNorm(Instance instance, uint primitiveIndex)
{
    // Get index values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];

    // Get position values from buffers
    float3 v0 = g_VertexBuffer[i0].getPosition();
    float3 v1 = g_VertexBuffer[i1].getPosition();
    float3 v2 = g_VertexBuffer[i2].getPosition();

    // Get normal values from buffers
    float3 n0 = g_VertexBuffer[i0].getNormal();
    float3 n1 = g_VertexBuffer[i1].getNormal();
    float3 n2 = g_VertexBuffer[i2].getNormal();

    TriangleNorm ret = {v0, v1, v2, n0, n1, n2};
    return ret;
}

/**
 * Fetch the vertices, normals and UVs from last frame for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
TriangleNormUV fetchPrevVerticesNormUV(Instance instance, uint primitiveIndex)
{
    // Get index values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];

    // Get position values from buffers
    float3 v0 = g_VertexBuffer[i0].getPosition();
    float3 v1 = g_VertexBuffer[i1].getPosition();
    float3 v2 = g_VertexBuffer[i2].getPosition();

    // Get normal values from buffers
    float3 n0 = g_VertexBuffer[i0].getNormal();
    float3 n1 = g_VertexBuffer[i1].getNormal();
    float3 n2 = g_VertexBuffer[i2].getNormal();

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].getUV();
    float2 uv1 = g_VertexBuffer[i1].getUV();
    float2 uv2 = g_VertexBuffer[i2].getUV();

    TriangleNormUV ret = {v0, v1, v2, n0, n1, n2, uv0, uv1, uv2};
    return ret;
}

/**
 * Fetch the UVs from last frame for a given triangle.
 * @param instance       The instance the triangle belongs to.
 * @param primitiveIndex The index of the primitive within the mesh.
 * @return The triangle data.
 */
UVs fetchPrevUVs(Instance instance, uint primitiveIndex)
{
    // Get index buffer values
    uint i0 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 0] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i1 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 1] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];
    uint i2 = g_IndexBuffer[instance.index_offset_idx + 3 * primitiveIndex + 2] +
        instance.vertex_offset_idx[g_PrevVertexDataIndex];

    // Get UV values from buffers
    float2 uv0 = g_VertexBuffer[i0].getUV();
    float2 uv1 = g_VertexBuffer[i1].getUV();
    float2 uv2 = g_VertexBuffer[i2].getUV();

    UVs ret = {uv0, uv1, uv2};
    return ret;
}

#endif // HAS_PREVIOUS_INTERSECT

#endif // MESH_HLSL
