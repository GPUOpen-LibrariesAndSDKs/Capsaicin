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

// NOTE: these are not used, but need for shader compilation
uint g_FrameIndex;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;

#include "../../materials/material_sampling.hlsl"
#include "../../math/transform.hlsl"

uint2 g_BufferDimensions;
float4x4 g_ViewProjectionInverse;
TextureCube g_EnvironmentBuffer;
SamplerState g_LinearSampler;
float g_Roughness;
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

float4 PrefilterIBL(in float4 pos : SV_Position) : SV_Target
{
    float2 uv = pos.xy / g_BufferDimensions;
    float2 ndc = 2.0f * uv - 1.0f;

    float3 world = transformPointProjection(float3(ndc, 1.0f), g_ViewProjectionInverse);

    float3 wo = normalize(world);
    float3 normal = wo;
    float roughness = g_Roughness * g_Roughness;
    float alpha = roughness * roughness;

    Quaternion localRotation = QuaternionRotationZ(normal);
    float3 wo_local = normalize(localRotation.transform(wo));

    float3 color = 0.0f;
    float total_weight = 0.0f;
    for (uint i = 0; i < g_SampleSize; ++i)
    {
        float2 xi = Hammersley2D(i, g_SampleSize);
        float3 wi_local = sampleGGX(alpha, wo_local, xi);
        float3 wi = normalize(localRotation.inverse().transform(wi_local));
        float weight = saturate(wi_local.z);
        wi.x = -wi.x;
        wi.z = -wi.z;
        color += weight * g_EnvironmentBuffer.SampleLevel(g_LinearSampler, wi, 0.0f).xyz;
        total_weight += weight;
    }
    color /= total_weight;

    return float4(color, 1.0f);
}
