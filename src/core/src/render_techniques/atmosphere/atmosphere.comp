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

#include "atmosphere.hlsl"
#include "../../math/transform.hlsl"

float3   g_Eye;
uint     g_FaceIndex;
uint     g_FrameIndex;
uint2    g_BufferDimensions;
float4x4 g_ViewProjectionInverse;

RWTexture2DArray<float4> g_InEnvironmentBuffer;
RWTexture2DArray<float4> g_OutEnvironmentBuffer;

[numthreads(8, 8, 1)]
void DrawAtmosphere(in uint2 did : SV_DispatchThreadID)
{
    if (any(did >= g_BufferDimensions))
    {
        return; // out of bounds
    }

    float2 uv  = (did + 0.5f) / g_BufferDimensions;
    float2 ndc = 2.0f * uv - 1.0f;

    float3 world = transformPointProjection(float3(ndc, 1.0f), g_ViewProjectionInverse);

    float3 ray_origin    = g_Eye;
    float3 ray_direction = normalize(world - g_Eye);
    float  ray_length    = 1e9f;

    float t = g_FrameIndex / 360.0f;
    float m = cos(2.0f * t);

    m = 20.0f * m * m * sign(m);

    float3 light_direction = normalize(float3(m * sin(t), 1.0f, m * cos(t)));
    float3 light_color     = light_direction.y * float3(1.0f, 1.0f, 1.0f) + 0.1f;

    float3 transmittance;
    float3 color = IntegrateScattering(ray_origin, ray_direction, ray_length, light_direction, light_color, transmittance);

    g_OutEnvironmentBuffer[uint3(did, g_FaceIndex)] = float4(color, 1.0f);
}

[numthreads(8, 8, 1)]
void FilterAtmosphere(in uint3 did : SV_DispatchThreadID)
{
    if (any((did.xy << 1) >= g_BufferDimensions))
    {
        return; // out of bounds
    }

    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (uint y = 0; y <= 1; ++y)
    {
        for (uint x = 0; x <= 1; ++x)
        {
            uint2 pos = (did.xy << 1) + uint2(x, y);

            if (any(pos >= g_BufferDimensions))
            {
                break;  // out of bounds
            }

            color += float4(g_InEnvironmentBuffer[uint3(pos, did.z)].xyz, 1.0f);
        }
    }

    g_OutEnvironmentBuffer[did] = float4(color.xyz / color.w, 1.0f);
}
