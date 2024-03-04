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

uint2    g_BufferDimensions;
float4x4 g_ViewProjectionInverse;

Texture2D g_EnvironmentMap;

SamplerState g_LinearSampler;

#include "../math/sampling.hlsl"

float2 SampleSphericalMap(in float3 rd)
{
    return float2(atan2(rd.z, rd.x) / (2.0f * PI) + 0.5f, 1.0f - acos(rd.y) / PI);
}

float4 DrawSky(in float4 pos : SV_Position) : SV_Target
{
    float2 uv  = pos.xy / g_BufferDimensions;
    float2 ndc = 2.0f * uv - 1.0f;

    float4 world = mul(g_ViewProjectionInverse, float4(ndc, 1.0f, 1.0f));
    world /= world.w;   // perspective divide

    uv = SampleSphericalMap(normalize(world.xyz));

    float3 color = g_EnvironmentMap.Sample(g_LinearSampler, uv).xyz;

    return float4(color, 1.0f);
}
