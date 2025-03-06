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
#include "materials/materials.hlsl"
#include "math/hash.hlsl"

bool AlphaTest(HitInfo hit_info)
{
    // Get instance information for current object
    Instance instance = g_InstanceBuffer[hit_info.instanceIndex];

    // Get material
    Material material = g_MaterialBuffer[instance.material_index];

    // Check back facing
    //  We currently only check back facing on alpha flagged surfaces as a performance optimisation. For normal
    //  geometry we should never intersect the back side of any opaque objects due to visibility being occluded
    //  by the front of the object (situations where camera is inside an object is ignored).
    if (!hit_info.frontFace && asuint(material.normal_alpha_side.z) == 0)
    {
        return false;
    }

    if (asuint(material.normal_alpha_side.w) != 0)
    {
        // Get vertices
        TriangleUV vertices = fetchVerticesUV(instance, hit_info.primitiveIndex);

        // Calculate UV coordinates
        float2 uv = interpolate(vertices.uv0, vertices.uv1, vertices.uv2, hit_info.barycentrics);
        MaterialAlpha mask = MakeMaterialAlpha(material, uv);

        // Check the alpha mask
        float alpha = 0.5F;
        return mask.alpha > alpha;
    }

    return true;
}

// A small value to avoid intersection with a light for shadow rays. This value is obtained empirically.
#define SHADOW_RAY_EPSILON (1.0f / (1 << 14))

#ifdef DISABLE_ALPHA_TESTING
#define CLOSEST_RAY_FLAGS RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES
#define SHADOW_RAY_FLAGS  RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
typedef RayQuery<CLOSEST_RAY_FLAGS> ClosestRayQuery;
typedef RayQuery<SHADOW_RAY_FLAGS> ShadowRayQuery;

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
#define CLOSEST_RAY_FLAGS RAY_FLAG_NONE /*| RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES*/
#define SHADOW_RAY_FLAGS  RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
typedef RayQuery<CLOSEST_RAY_FLAGS> ClosestRayQuery;
typedef RayQuery<SHADOW_RAY_FLAGS> ShadowRayQuery;

template<typename RayQueryType>
RayQueryType TraceRay(RayDesc incomingRay)
{
    RayQueryType ray_query;
    ray_query.TraceRayInline(g_Scene, RAY_FLAG_NONE, 0xFFu, incomingRay);
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
            ray_query.Abort();
        }
    }

    return ray_query;
}
#endif // DISABLE_ALPHA_TESTING

#endif // INTERSECTION_HLSL
