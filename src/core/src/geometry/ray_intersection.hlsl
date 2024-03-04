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

#ifndef RAY_INTERSECTION_HLSL
#define RAY_INTERSECTION_HLSL

/*
// Requires the following data to be defined in any shader that uses this file
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Mesh> g_MeshBuffer;
StructuredBuffer<float3x4> g_TransformBuffer;
StructuredBuffer<uint> g_IndexBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
StructuredBuffer<Material> g_MaterialBuffer;

TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[];
SamplerState g_TextureSampler; // Should be a linear sampler
*/

#include "../materials/materials.hlsl"
#include "../geometry/geometry.hlsl"
#include "../geometry/mesh.hlsl"
#include "../geometry/ray_tracing.hlsl"
#include "../math/transform.hlsl"

/** Data representing a surface intersection with full details */
struct IntersectData
{
    Material material; /**< The material associated with intersected surface */
    float2 uv; /**< The texture UV values at intersected position */
    float3 vertex0; /**< The surface triangles first vertex */
    float3 vertex1; /**< The surface triangles second vertex */
    float3 vertex2; /**< The surface triangles third vertex */
    float2 uv0; /**< The uv coordinate at the first vertex */
    float2 uv1; /**< The uv coordinate at the second vertex */
    float2 uv2; /**< The uv coordinate at the third vertex */
    float3 position; /**< The ray intersection location */
    float3 normal; /**< The shading normal at position */
    float3 geometryNormal; /**< The normal of actual intersected triangle at position */
    float2 barycentrics; /**< The barycentric coordinates within the intersected primitive */
};

/**
 * Determine the complete intersection data for a ray hit.
 * @param hitData The intersection information for a ray hit.
 * @return The data associated with the intersection.
 */
IntersectData MakeIntersectData(HitInfo hitData)
{
    // Get instance information for current object
    Instance instance = g_InstanceBuffer[hitData.instanceIndex];
    Mesh mesh = g_MeshBuffer[instance.mesh_index + hitData.geometryIndex];
    float3x4 transform = g_TransformBuffer[instance.transform_index];
    float3x3 normalTransform = getNormalTransform((float3x3)transform);

    // Fetch vertex data
    TriangleNormUV triData = fetchVerticesNormUV(mesh, hitData.primitiveIndex);

    IntersectData iData;
    // Set material
    iData.material = g_MaterialBuffer[instance.material_index];
    // Calculate UV coordinates
    iData.uv = interpolate(triData.uv0, triData.uv1, triData.uv2, hitData.barycentrics);
    // Add vertex information needed for lights
    iData.vertex0 = transformPoint(triData.v0, transform).xyz;
    iData.vertex1 = transformPoint(triData.v1, transform).xyz;
    iData.vertex2 = transformPoint(triData.v2, transform).xyz;
    iData.uv0 = triData.uv0;
    iData.uv1 = triData.uv1;
    iData.uv2 = triData.uv2;
    // Calculate intersection position
    iData.position = interpolate(iData.vertex0, iData.vertex1, iData.vertex2, hitData.barycentrics);
    // Calculate geometry normal (assume CCW winding)
    float3 edge10 = triData.v1 - triData.v0;
    float3 edge20 = triData.v2 - triData.v0;
    float3 localGeometryNormal = cross(edge10, edge20) * (hitData.frontFace ? 1.0f : -1.0f);
    // Calculate shading normal
    float3 normal = interpolate(triData.n0, triData.n1, triData.n2, hitData.barycentrics) * (hitData.frontFace ? 1.0f : -1.0f);
    iData.normal = normal;
    // Check for normal mapping
    uint normalTex = asuint(iData.material.normal_alpha_side.x);
    if (normalTex != uint(-1))
    {
        // Get normal from texture map
        float3 normalTan = 2.0f * g_TextureMaps[NonUniformResourceIndex(normalTex)].SampleLevel(g_TextureSampler, iData.uv, 0.0f).xyz - 1.0f;
        normal = normalize(normal);
        // Ensure normal is in same hemisphere as geometry normal (This is required when non-uniform negative(mirrored) scaling is applied to a backface surface)
        normal = dot(normal, normalize(localGeometryNormal)) >= 0.0f ? normal : -normal;

        // Calculate tangent and bi-tangent basis vectors
        float2 edgeUV1 = triData.uv1 - triData.uv0;
        float2 edgeUV2 = triData.uv2 - triData.uv0;
        float determinate = edgeUV1.x * edgeUV2.y - edgeUV1.y * edgeUV2.x;
        // If the determinate is zero then the matrix is non invertable
        if (determinate != 0.0f && dot(normalTan, normalTan) > 0.0f)
        {
            determinate = rcp(determinate);
            float3 tangentBasis = (edge10 * edgeUV2.yyy - edge20 * edgeUV1.yyy) * determinate;
            float3 bitangentBasis = (edge20 * edgeUV1.xxx - edge10 * edgeUV2.xxx) * determinate;

            // Gram-Schmidt orthogonalise tangent
            float3 tangent = normalize(tangentBasis - normal * dot(normal, tangentBasis));
            float3 bitangent = cross(normal, tangent);

            // Correct handedness
            bitangent = dot(bitangent, bitangentBasis) >= 0.0f ? -bitangent : bitangent;

            // Convert from tangent space
            float3x3 tbn = float3x3(tangent, bitangent, normal);
            iData.normal = mul(normalTan, tbn);
        }
    }
    iData.geometryNormal = normalize(mul(normalTransform, localGeometryNormal));
    iData.normal = normalize(mul(normalTransform, iData.normal));
    iData.barycentrics = hitData.barycentrics;
    return iData;
}

/**
 * Determine the complete intersection data for a ray hit using visibility buffer information.
 * @param hitData The intersection information for a ray hit.
 * @return The data associated with the intersection.
 */
IntersectData MakeIntersectData(HitInfo hitData, float3 geometryNormal, float3 shadingNormal)
{
    // Get instance information for current object
    Instance instance = g_InstanceBuffer[hitData.instanceIndex];
    Mesh mesh = g_MeshBuffer[instance.mesh_index + hitData.geometryIndex];
    float3x4 transform = g_TransformBuffer[instance.transform_index];

    // Fetch vertex data
    TriangleNormUV triData = fetchVerticesNormUV(mesh, hitData.primitiveIndex);

    IntersectData iData;
    // Set material
    iData.material = g_MaterialBuffer[instance.material_index];
    // Calculate UV coordinates
    iData.uv = interpolate(triData.uv0, triData.uv1, triData.uv2, hitData.barycentrics);
    // Add vertex information needed for lights
    iData.vertex0 = transformPoint(triData.v0, transform).xyz;
    iData.vertex1 = transformPoint(triData.v1, transform).xyz;
    iData.vertex2 = transformPoint(triData.v2, transform).xyz;
    iData.uv0 = triData.uv0;
    iData.uv1 = triData.uv1;
    iData.uv2 = triData.uv2;
    // Calculate intersection position
    iData.position = interpolate(iData.vertex0, iData.vertex1, iData.vertex2, hitData.barycentrics);
    iData.geometryNormal = geometryNormal;
    iData.normal = shadingNormal;
    iData.barycentrics = hitData.barycentrics;
    return iData;
}

#endif // RAY_INTERSECTION_HLSL
