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

#include "../../geometry/path_tracing_shared.h"

uint2 g_BufferDimensions;
RayCamera g_RayCamera;
uint g_FrameIndex;

StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Mesh> g_MeshBuffer;
StructuredBuffer<float3x4> g_TransformBuffer;
StructuredBuffer<uint> g_IndexBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
StructuredBuffer<Material> g_MaterialBuffer;
StructuredBuffer<float3x4> g_PrevTransformBuffer;

RaytracingAccelerationStructure g_Scene;

Texture2D g_TextureMaps[];
SamplerState g_TextureSampler; // Should be a linear sampler

struct CameraMatrix
{
    float4x4 data;
};
ConstantBuffer<CameraMatrix> g_ViewProjection;
ConstantBuffer<CameraMatrix> g_PrevViewProjection;
float2 g_Jitter;

RWTexture2D<float4> g_Visibility;
RWTexture2D<float> g_Depth;
RWTexture2D<float4> g_GeometryNormal;
RWTexture2D<float2> g_Velocity;
#ifdef HAS_SHADING_NORMAL
RWTexture2D<float4> g_ShadingNormal;
#endif
#ifdef HAS_VERTEX_NORMAL
RWTexture2D<float4> g_VertexNormal;
#endif
#ifdef HAS_ROUGHNESS
RWTexture2D<float> g_Roughness;
#endif

#include "../../geometry/geometry.hlsl"
#include "../../geometry/intersection.hlsl"
#include "../../geometry/ray_intersection.hlsl"
#include "../../geometry/ray_tracing.hlsl"
#include "../../math/math.hlsl"
#include "../../math/transform.hlsl"
#include "../../materials/materials.hlsl"

struct PathData
{
    uint2 pixel;
};

void pathHit(PathData pathData, RayDesc ray, HitInfo hitData)
{
    // Get instance information for current object
    Instance instance = g_InstanceBuffer[hitData.instanceIndex];
    Mesh mesh = g_MeshBuffer[instance.mesh_index];
    float3x4 transform = g_TransformBuffer[instance.transform_index];

    // Fetch vertex data
#if defined(HAS_SHADING_NORMAL) || defined(HAS_VERTEX_NORMAL)
    TriangleNormUV triData = fetchVerticesNormUV(mesh, hitData.primitiveIndex);
#else
    TriangleUV triData = fetchVerticesUV(mesh, hitData.primitiveIndex);
#endif

    // Set material
    Material material = g_MaterialBuffer[instance.material_index];
    // Calculate UV coordinates
    float2 uv = interpolate(triData.uv0, triData.uv1, triData.uv2, hitData.barycentrics);
    // Calculate intersection position
    float3 position = transformPoint(interpolate(triData.v0, triData.v1, triData.v2, hitData.barycentrics), transform);

    // Calculate geometry normal (assume CCW winding)
    float3 edge10 = triData.v1 - triData.v0;
    float3 edge20 = triData.v2 - triData.v0;
    float3x3 normalTransform = getNormalTransform((float3x3)transform);
    float3 localGeometryNormal = cross(edge10, edge20) * (hitData.frontFace ? 1.0f : -1.0f);
    float3 geometryNormal = normalize(mul(normalTransform, localGeometryNormal));

#if defined(HAS_SHADING_NORMAL) || defined(HAS_VERTEX_NORMAL)
    float3 normal = interpolate(triData.n0, triData.n1, triData.n2, hitData.barycentrics) * (hitData.frontFace ? 1.0f : -1.0f);

#   ifdef HAS_VERTEX_NORMAL
    // Calculate shading normal
    float3 vertexNormal = normalize(mul(normalTransform, normal));
#   endif
#endif // HAS_SHADING_NORMAL || HAS_VERTEX_NORMAL
#ifdef HAS_SHADING_NORMAL
    // Check for normal mapping
    float3 shadingNormal = normal;
    uint normalTex = asuint(material.normal_alpha_side.x);
    if (normalTex != uint(-1))
    {
        // Get normal from texture map
        float3 normalTan = 2.0f * g_TextureMaps[NonUniformResourceIndex(normalTex)].SampleLevel(g_TextureSampler, uv, 0.0f).xyz - 1.0f;
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
            shadingNormal = mul(normalTan, tbn);
        }
    }
    shadingNormal = normalize(mul(normalTransform, shadingNormal));
#endif // HAS_SHADING_NORMAL
    // Write out buffers
    g_Visibility[pathData.pixel] = float4(hitData.barycentrics, asfloat(hitData.instanceIndex), asfloat(hitData.primitiveIndex));
    g_Depth[pathData.pixel] = transformPointProjection(position, g_ViewProjection.data).z;
    g_GeometryNormal[pathData.pixel] = float4(0.5f * geometryNormal + 0.5f, 1.0f);

    float3 prevPosition = transformPoint(interpolate(triData.v0, triData.v1, triData.v2, hitData.barycentrics), g_PrevTransformBuffer[instance.transform_index]);
    float4 positionVP = mul(g_ViewProjection.data, float4(position, 1.0f));
    float4 prevPositionVP = mul(g_PrevViewProjection.data, float4(prevPosition, 1.0f));
    g_Velocity[pathData.pixel] = CalculateMotionVector(positionVP, prevPositionVP);
#ifdef HAS_SHADING_NORMAL
    g_ShadingNormal[pathData.pixel] = float4(0.5f * shadingNormal + 0.5f, 1.0f);
#endif
#ifdef HAS_VERTEX_NORMAL
    g_VertexNormal[pathData.pixel] = float4(0.5f * vertexNormal + 0.5f, 1.0f);
#endif
#ifdef HAS_ROUGHNESS
    MaterialEvaluated materialEvaluated = MakeMaterialEvaluated(material, uv);
    g_Roughness[pathData.pixel] = materialEvaluated.roughness;
#endif
}

void visibilityRT(uint2 did, uint2 dimensions)
{
    //Check if valid pixel
    uint2 pixel = did;
    if (any(pixel >= dimensions))
    {
        return;
    }

    // Offset pixel to pixel center
    float2 pixelRay = (float2)pixel + 0.5f.xx + g_Jitter;

    // Calculate primary ray
    RayDesc ray = generateCameraRay(pixelRay, g_RayCamera);

    PathData pathData;
    pathData.pixel = pixel;

#ifdef USE_INLINE_RT
    ClosestRayQuery rayQuery = TraceRay<ClosestRayQuery>(ray);

    // Check for valid intersection
    if (rayQuery.CommittedStatus() != COMMITTED_NOTHING)
    {
        pathHit(pathData, ray, GetHitInfoRtInlineCommitted(rayQuery));
    }
#else
    TraceRay(g_Scene, RAY_FLAG_NONE, 0xFFu, 0, 0, 0, ray, pathData);
#endif
}

[numthreads(4, 8, 1)]
void VisibilityBufferRT(in uint2 did : SV_DispatchThreadID)
{
    visibilityRT(did, g_BufferDimensions);
}
