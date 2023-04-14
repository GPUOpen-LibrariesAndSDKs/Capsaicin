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
#ifndef MESH_H
#define MESH_H

// Fetches the transform at the given index.
float4x4 FetchTransform(in uint transform_index)
{
    float4x4 transform;

    float4 m0 = g_TransformBuffer[4 * transform_index + 0];
    float4 m1 = g_TransformBuffer[4 * transform_index + 1];
    float4 m2 = g_TransformBuffer[4 * transform_index + 2];
    float4 m3 = g_TransformBuffer[4 * transform_index + 3];

    transform[0] = float4(m0.x, m1.x, m2.x, m3.x);
    transform[1] = float4(m0.y, m1.y, m2.y, m3.y);
    transform[2] = float4(m0.z, m1.z, m2.z, m3.z);
    transform[3] = float4(m0.w, m1.w, m2.w, m3.w);

    return transform;
}

// Fetches the vertices of the given mesh primitive.
void FetchVertices(in Mesh mesh, in uint primitive_index, out float3 v0, out float3 v1, out float3 v2)
{
    uint3 indices = g_IndexBuffers[0].Load3(mesh.index_offset + 3 * primitive_index * mesh.index_stride);

    v0 = asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.x * mesh.vertex_stride));
    v1 = asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.y * mesh.vertex_stride));
    v2 = asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.z * mesh.vertex_stride));
}

// Fetches the vertices of the given mesh primitive.
void FetchVertices(in Mesh mesh, in uint primitive_index, out Vertex v0, out Vertex v1, out Vertex v2)
{
    uint3 indices = g_IndexBuffers[0].Load3(mesh.index_offset + 3 * primitive_index * mesh.index_stride);

    v0.position = float4(asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.x * mesh.vertex_stride)), 1.0f);
    v1.position = float4(asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.y * mesh.vertex_stride)), 1.0f);
    v2.position = float4(asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.z * mesh.vertex_stride)), 1.0f);

    v0.normal = float4(asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.x * mesh.vertex_stride + 16)), 0.0f);
    v1.normal = float4(asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.y * mesh.vertex_stride + 16)), 0.0f);
    v2.normal = float4(asfloat(g_VertexBuffers[0].Load3(mesh.vertex_offset + indices.z * mesh.vertex_stride + 16)), 0.0f);

    v0.uv = asfloat(g_VertexBuffers[0].Load2(mesh.vertex_offset + indices.x * mesh.vertex_stride + 32));
    v1.uv = asfloat(g_VertexBuffers[0].Load2(mesh.vertex_offset + indices.y * mesh.vertex_stride + 32));
    v2.uv = asfloat(g_VertexBuffers[0].Load2(mesh.vertex_offset + indices.z * mesh.vertex_stride + 32));
}

#endif // MESH_H
