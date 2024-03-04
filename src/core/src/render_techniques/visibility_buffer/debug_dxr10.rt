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

float3 g_Eye;
float4x4 g_ViewProjectionInverse;

struct MyConstantBuffer
{
    float4 color;
};

RaytracingAccelerationStructure g_Scene : register(t0);
RWTexture2D<float4> g_RenderTarget      : register(u0);
ConstantBuffer<MyConstantBuffer> g_MyCB : register(b0, space1);


TriangleHitGroup MyHitGroup =
{
    "",                     // AnyHit
    "MyClosestHitShader",   // ClosestHit
};

TriangleHitGroup MyHitGroup2 =
{
    "",                     // AnyHit
    "MyClosestHitShader2",   // ClosestHit
};

RaytracingShaderConfig  MyShaderConfig =
{
    16, // max payload size
    8   // max attribute size
};

RaytracingPipelineConfig MyPipelineConfig =
{
    1 // max trace recursion depth
};

struct CubeConstantBuffer
{
    float4 color;
};

struct RayPayload
{
    float4 color;
};

[shader("raygeneration")]
void MyRaygenShader()
{
    float2 xy = DispatchRaysIndex().xy + 0.5f;
    float2 screen_pos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;
    screen_pos.y = -screen_pos.y;

    // Unproject the pixel coordinate into a ray.
    float4 world = mul(g_ViewProjectionInverse, float4(screen_pos, 0, 1));
    world.xyz /= world.w;
    
    RayDesc ray;
    ray.Origin = g_Eye;
    ray.Direction = normalize(world.xyz - g_Eye);
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    RayPayload payload = { float4(0, 0, 0, 0) };
    TraceRay(g_Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    g_RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.color = g_MyCB.color;
}


[shader("closesthit")]
void MyClosestHitShader2(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.color = float4(1, 0, 0, 0);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    float4 background = float4(0.0f, 0.2f, 0.4f, 1.0f);
    payload.color = background;
}
