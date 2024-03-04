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

#ifndef BRDF_LUT_HLSL
#define BRDF_LUT_HLSL

// NOTE: these are not used, but need for shader compilation
uint g_FrameIndex;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;

#include "../../materials/material_sampling.hlsl"

uint g_LutSize;
RWTexture2D<float2> g_LutBuffer;
uint g_SampleSize;

// Returns position of i-th element in 2D Hammersley Point Set of N elements
float2 Hammersley2D(uint i, uint N)
{
    // Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) / float(N), rdi);
}

[numthreads(8, 8, 1)]
void ComputeBrdfLut(uint2 did : SV_DispatchThreadID)
{
    if (any(did >= g_LutSize))
    {
        return;
    }

    float2 uv = (did + 0.5f) / g_LutSize;

    float dotNV = uv.x;
    float roughness = uv.y;
    float alpha = roughness * roughness;
    float3 wo = float3(sqrt(1.0f - dotNV * dotNV), 0.0f, dotNV);

    float2 lut_value = 0.0f;
    for (uint i = 0; i < g_SampleSize; ++i)
    {
        float2 xi = Hammersley2D(i, g_SampleSize);
        float3 wi = sampleGGX(alpha, wo, xi);

        float3 h = normalize(wo + wi);

        float dotHV = saturate(dot(h, wo));
        float dotNH = clamp(h.z, -1.0f, 1.0f);
        float dotNL = clamp(wi.z, -1.0f, 1.0f);
        float3 F;
        float3 FGD = evaluateGGX(alpha, alpha * alpha, 0.0f, dotHV, dotNH, dotNL, dotNV, F);
        float3 GD = FGD / F;

        float pdf = sampleGGXPDF(alpha * alpha, dotNH, dotNV, wo);

        lut_value += float2(GD.x, FGD.x) * saturate(dotNL) / pdf; // saturate(dotNL) = abs(dotNL) * Heaviside function for the upper hemisphere.
    }
    lut_value /= float(g_SampleSize);

    g_LutBuffer[did] = lut_value;
}

#endif
