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

#include "gi10_shared.h"
#include "gi10.comp"

TriangleHitGroup PopulateScreenProbesHitGroup =
{
    "PopulateScreenProbesAnyHit",     // AnyHit
    "PopulateScreenProbesClosestHit", // ClosestHit
};

TriangleHitGroup PopulateCellsHitGroup =
{
    "PopulateCellsAnyHit",     // AnyHit
    "PopulateCellsClosestHit", // ClosestHit
};

TriangleHitGroup TraceReflectionsHitGroup =
{
    "TraceReflectionsAnyHit",     // AnyHit
    "TraceReflectionsClosestHit", // ClosestHit
};

RaytracingShaderConfig MyShaderConfig =
{
    64, // max payload size
    8   // max attribute size
};

RaytracingPipelineConfig MyPipelineConfig =
{
    1 // max trace recursion depth
};

[shader("raygeneration")]
void PopulateScreenProbesRaygen()
{
    PopulateScreenProbes(DispatchRaysIndex().x);
}

[shader("anyhit")]
void PopulateScreenProbesAnyHit(inout PopulateScreenProbesPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    if (!AlphaTest(GetHitInfoRt(attr)))
    {
        IgnoreHit();
    }
}

[shader("closesthit")]
void PopulateScreenProbesClosestHit(inout PopulateScreenProbesPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.hit_dist = RayTCurrent();
    PopulateScreenProbesHandleHit(DispatchRaysIndex().x, payload, GetRayDescRt(), GetHitInfoRt(attr));
}

[shader("miss")]
void PopulateScreenProbesMiss(inout PopulateScreenProbesPayload payload)
{
    payload.hit_dist = 1e9f;
    PopulateScreenProbesHandleMiss(payload, GetRayDescRt());
}

[shader("raygeneration")]
void PopulateCellsRaygen()
{
    PopulateCells(DispatchRaysIndex().x);
}

[shader("anyhit")]
void PopulateCellsAnyHit(inout PopulateCellsPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    if (!AlphaTest(GetHitInfoRt(attr)))
    {
        IgnoreHit();
    }
}

[shader("closesthit")]
void PopulateCellsClosestHit(inout PopulateCellsPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    PopulateCellsHandleHit(DispatchRaysIndex().x, payload, GetRayDescRt());
}

[shader("miss")]
void PopulateCellsMiss(inout PopulateCellsPayload payload)
{
    PopulateCellsHandleMiss(DispatchRaysIndex().x, payload, GetRayDescRt());
}

[shader("raygeneration")]
void TraceReflectionsRaygen()
{
    TraceReflections(DispatchRaysIndex().x);
}

[shader("anyhit")]
void TraceReflectionsAnyHit(inout TraceReflectionsPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    if (!AlphaTest(GetHitInfoRt(attr)))
    {
        IgnoreHit();
    }
}

[shader("closesthit")]
void TraceReflectionsClosestHit(inout TraceReflectionsPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    TraceReflectionsHandleHit(DispatchRaysIndex().x, payload, GetRayDescRt(), GetHitInfoRt(attr), RayTCurrent());
}

[shader("miss")]
void TraceReflectionsMiss(inout TraceReflectionsPayload payload)
{
    TraceReflectionsHandleMiss(DispatchRaysIndex().x, payload, GetRayDescRt());
}
