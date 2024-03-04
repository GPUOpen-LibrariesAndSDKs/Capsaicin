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

#define USE_INLINE_RT 0

#include "reference_path_tracer.comp"

TriangleHitGroup ReferencePTHitGroup =
{
    "ReferencePTAnyHit", // AnyHit
    "ReferencePTClosestHit", // ClosestHit
};

TriangleHitGroup ReferencePTShadowHitGroup =
{
    "ReferencePTShadowAnyHit", // AnyHit
    "", // ClosestHit
};

RaytracingShaderConfig MyShaderConfig =
{
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    100, // max payload size
#else
    88, // max payload size
#endif
    8 // max attribute size
};

RaytracingPipelineConfig MyPipelineConfig =
{
    2 // max trace recursion depth
};

[shader("raygeneration")]
void ReferencePTRaygen()
{
    uint2 did = DispatchRaysIndex().xy;
    pathTracer(did, g_BufferDimensions);
}

[shader("anyhit")]
void ReferencePTAnyHit(inout PathData path, in BuiltInTriangleIntersectionAttributes attr)
{
    if (!AlphaTest(GetHitInfoRt(attr)))
    {
        IgnoreHit();
    }
}

[shader("anyhit")]
void ReferencePTShadowAnyHit(inout ShadowRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    if (!AlphaTest(GetHitInfoRt(attr)))
    {
        IgnoreHit();
    }
}

[shader("closesthit")]
void ReferencePTClosestHit(inout PathData path, in BuiltInTriangleIntersectionAttributes attr)
{
    // Setup configurable constants
    const uint minBounces = g_BounceRRCount; // Minimum bounces before early terminations are allowed
    const uint maxBounces = g_BounceCount;
    RayDesc ray = GetRayDescRt();
    HitInfo hitData = GetHitInfoRt(attr);
    IntersectData iData = MakeIntersectData(hitData);
    path.terminated = !pathHit(ray, hitData, iData, path.randomStratified, path.lightSampler,
        path.bounce, minBounces, maxBounces, path.normal, path.samplePDF, path.throughput,
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
        path.sampleReflectance,
#endif
        path.radiance);
    path.origin = ray.Origin;
    path.direction = ray.Direction;
}

[shader("miss")]
void ReferencePTMiss(inout PathData pathData)
{
    shadePathMiss(GetRayDescRt(), pathData.bounce, pathData.lightSampler, pathData.normal, pathData.samplePDF, pathData.throughput,
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
        pathData.sampleReflectance,
#endif
        pathData.radiance);
    pathData.terminated = true;
}

[shader("miss")]
void ReferencePTShadowMiss(inout ShadowRayPayload payload)
{
    payload.visible = true;
}
