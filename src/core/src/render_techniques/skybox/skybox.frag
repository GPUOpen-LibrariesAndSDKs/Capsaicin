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

float3   g_Eye;
uint2    g_BufferDimensions;
float4x4 g_ViewProjectionInverse;

TextureCube g_EnvironmentBuffer;

SamplerState g_LinearSampler;

#include "../../math/transform.hlsl"

float4 main(in float4 pos : SV_Position) : SV_Target
{
    float2 uv  = pos.xy / g_BufferDimensions;
    float2 ndc = 2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f;

    float3 world = transformPointProjection(float3(ndc, 1.0f), g_ViewProjectionInverse);

    float3 sky_sample = g_EnvironmentBuffer.Sample(g_LinearSampler, world - g_Eye).xyz;

    return float4(sky_sample, 1.0f);
}
