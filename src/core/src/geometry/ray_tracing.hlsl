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

#ifndef RAY_TRACING_HLSL
#define RAY_TRACING_HLSL

struct HitInfo
{
    uint instanceIndex;
    uint geometryIndex;
    uint primitiveIndex;
    float2 barycentrics;
    bool frontFace;
};

RayDesc GetRayDescRt()
{
    RayDesc ray;
    ray.Origin    = WorldRayOrigin();
    ray.TMin      = RayTMin();
    ray.Direction = WorldRayDirection();
    ray.TMax      = RayTCurrent();
    return ray;
}

HitInfo GetHitInfoRt(in BuiltInTriangleIntersectionAttributes attr)
{
    HitInfo hit_info;
    hit_info.instanceIndex  = InstanceIndex();
    hit_info.geometryIndex  = GeometryIndex();
    hit_info.primitiveIndex = PrimitiveIndex();
    hit_info.barycentrics   = attr.barycentrics;
    hit_info.frontFace      = HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE;
    return hit_info;
}

template<RAY_FLAG RAY_FLAGS>
HitInfo GetHitInfoRtInlineCommitted(in RayQuery<RAY_FLAGS> ray_query)
{
    HitInfo hit_info;
    hit_info.instanceIndex  = ray_query.CommittedInstanceIndex();
    hit_info.primitiveIndex = ray_query.CommittedPrimitiveIndex();
    hit_info.geometryIndex  = ray_query.CommittedGeometryIndex();
    hit_info.barycentrics   = ray_query.CommittedTriangleBarycentrics();
    hit_info.frontFace      = ray_query.CommittedTriangleFrontFace();
    return hit_info;
}

template<RAY_FLAG RAY_FLAGS>
HitInfo GetHitInfoRtInlineCandidate(in RayQuery<RAY_FLAGS> ray_query)
{
    HitInfo hit_info;
    hit_info.instanceIndex  = ray_query.CandidateInstanceIndex();
    hit_info.primitiveIndex = ray_query.CandidatePrimitiveIndex();
    hit_info.geometryIndex  = ray_query.CandidateGeometryIndex();
    hit_info.barycentrics   = ray_query.CandidateTriangleBarycentrics();
    hit_info.frontFace      = ray_query.CandidateTriangleFrontFace();
    return hit_info;
}

#endif // RAY_TRACING_HLSL
