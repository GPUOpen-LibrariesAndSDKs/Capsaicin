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

#ifndef INTERSECTION_HLSL
#define INTERSECTION_HLSL

/*
// Requires the following data to be defined in any shader that uses this file
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Mesh> g_MeshBuffer;
StructuredBuffer<float3x4> g_TransformBuffer;
StructuredBuffer<uint> g_IndexBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
StructuredBuffer<Material> g_MaterialBuffer;
RaytracingAccelerationStructure g_Scene;
uint g_FrameIndex;
*/

#include "geometry.hlsl"
#include "mesh.hlsl"
#include "ray_tracing.hlsl"
#include "../materials/materials.hlsl"
#include "../math/hash.hlsl"

bool AlphaTest(HitInfo hit_info)
{
    // Get instance information for current object
    Instance instance = g_InstanceBuffer[hit_info.instanceIndex];

    // Get material
    Material material = g_MaterialBuffer[instance.material_index];

    // Check back facing
    //  We currently only check back facing on alpha flagged surfaces as a performance optimisation. For normal
    //  geometry we should never intersect the back side of any opaque objects due to visibility being occluded
    //  by the front of the object (situations were camera is inside an object is ignored).
    if (!hit_info.frontFace && asuint(material.normal_alpha_side.z) == 0)
    {
        return false;
    }

    // Get vertices
    Mesh mesh = g_MeshBuffer[instance.mesh_index + hit_info.geometryIndex];
    TriangleUV vertices = fetchVerticesUV(mesh, hit_info.primitiveIndex);

    // Calculate UV coordinates
    float2 uv = interpolate(vertices.uv0, vertices.uv1, vertices.uv2, hit_info.barycentrics);
    MaterialAlpha mask = MakeMaterialAlpha(material, uv);

    // Check the alpha mask
    return mask.alpha > 0.5f;
}

#ifdef DISABLE_ALPHA_TESTING
typedef RayQuery<RAY_FLAG_FORCE_OPAQUE| RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ClosestRayQuery;
typedef RayQuery<RAY_FLAG_FORCE_OPAQUE| RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ShadowRayQuery;

template<typename RayQueryType>
RayQueryType TraceRay(RayDesc incommingRay)
{
    RayQueryType ray_query;
    ray_query.TraceRayInline(g_Scene, RAY_FLAG_NONE, 0xFFu, incommingRay);
    while (ray_query.Proceed())
    {
    }

    return ray_query;
}
#else // DISABLE_ALPHA_TESTING
typedef RayQuery<RAY_FLAG_NONE /*| RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES*/> ClosestRayQuery;
typedef RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ShadowRayQuery;

template<typename RayQueryType>
RayQueryType TraceRay(RayDesc incommingRay)
{
    RayQueryType ray_query;
    ray_query.TraceRayInline(g_Scene, RAY_FLAG_NONE, 0xFFu, incommingRay);
    while (ray_query.Proceed())
    {
        if (ray_query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        {
            if (AlphaTest(GetHitInfoRtInlineCandidate(ray_query)))
            {
                ray_query.CommitNonOpaqueTriangleHit();
            }
        }
        else
        {
            // Should never get here as we don't support non-triangle geometry
            // However if this conditional is removed the driver crashes
            ray_query.Abort();
        }
    }

    return ray_query;
}
#endif // DISABLE_ALPHA_TESTING

#endif // INTERSECTION_HLSL
