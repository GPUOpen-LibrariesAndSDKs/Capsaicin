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
#include "../../math/color.hlsl"

uint2 g_BufferDimensions;
uint  g_FrameIndex;
float g_Exposure;

RWTexture2D<float4> g_InputBuffer;
RWTexture2D<float4> g_OutputBuffer;

#include "../../components/blue_noise_sampler/blue_noise_sampler.hlsl"

float3 EvalLogContrastFunc(in float3 color, in float epsilon, in float logMidpoint, in float contrast)
{
    float3 logColor = log2(color + epsilon);
    float3 adjColor = logMidpoint + (logColor - logMidpoint) * contrast;

    return max(exp2(adjColor) - epsilon, 0.0f);
}

float3 tonemapSimple(in float3 color)
{
    float peak = max(color.x, max(color.y, color.z));
    float3 ratio = (color / peak);
    color = saturate(color / (color + 1.0f));
    float blend_amount = luminance(color);
    return lerp((peak / (peak + 1.0f)) * ratio, color, blend_amount);
}

float3 ditherColor(in uint2 pixel, in float3 color)
{
    float v = BlueNoise_Sample1D(pixel, g_FrameIndex);
    float o = 2.0f * v - 1.0f; // to (-1, 1) range
    v = max(o / sqrt(abs(o)), -1.0f);
    return color + v / 255.0f;
}

[numthreads(8, 8, 1)]
void Tonemap(in uint2 did : SV_DispatchThreadID)
{
    float3 color = g_InputBuffer[did].xyz;
    float2 uv = (did + 0.5f) / g_BufferDimensions;

    color *= exp2(g_Exposure);
    color = saturate(tonemapSimple(color));
    color = EvalLogContrastFunc(1.2f * color, 1e-5f, 0.18f, 1.2f);
    color = convertToSRGB(color);
    color = ditherColor(did, color);

    g_OutputBuffer[did] = float4(color, 1.0f);
}
