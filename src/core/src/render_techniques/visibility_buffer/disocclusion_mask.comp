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

#include "../../gpu_shared.h"
#include "../../math/transform.hlsl"

float3   g_Eye;
float2   g_NearFar;
float2   g_TexelSize;
float4x4 g_Reprojection;
float4x4 g_ViewProjectionInverse;

Texture2D g_DepthBuffer;
Texture2D g_GeometryNormalBuffer;
Texture2D g_VelocityBuffer;
Texture2D g_PreviousDepthBuffer;

RWTexture2D<float> g_DisocclusionMask;

SamplerState g_NearestSampler;

float GetLinearDepth(in float depth)
{
    return -g_NearFar.x * g_NearFar.y / (depth * (g_NearFar.y - g_NearFar.x) - g_NearFar.y);
}

float2 GetClosestVelocity(in float2 uv, in float2 texel_size)
{
    float2 velocity;
    float closest_depth = 9.9f;

    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            float2 st   = uv + float2(x, y) * texel_size;
            float depth = g_DepthBuffer.SampleLevel(g_NearestSampler, st, 0.0f).x;

            if (depth < closest_depth)
            {
                velocity = g_VelocityBuffer.SampleLevel(g_NearestSampler, st, 0.0f).xy;
                closest_depth = depth;
            }
        }

    return velocity;
}

[numthreads(16, 16, 1)]
void main(in uint2 did : SV_DispatchThreadID)
{
    float2 uv = (did + 0.5f) * g_TexelSize;

    float2 velocity    = GetClosestVelocity(uv, g_TexelSize);
    float2 previous_uv = uv - velocity;

    float depth             = g_DepthBuffer.SampleLevel(g_NearestSampler, uv, 0.0f).x;
    float disocclusion_mask = (depth < 1.0f ? 1.0f : 0.0f);

    if (depth < 1.0f && all(previous_uv > 0.0f) && all(previous_uv < 1.0f))
    {
        float3 clip_space = transformPointProjection(float3(2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f, depth), g_Reprojection);
        float3 normal     = normalize(2.0f * g_GeometryNormalBuffer.SampleLevel(g_NearestSampler, uv, 0.0f).xyz - 1.0f);

        float3 world_pos = transformPointProjection(float3(2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f, depth), g_ViewProjectionInverse);
        float3 view_dir   = normalize(g_Eye - world_pos);

        float z_alignment     = pow(1.0f - max(dot(view_dir, normal), 0.0f), 6.0f);
        depth                 = GetLinearDepth(clip_space.z);   // get linear depth
        float previous_depth  = GetLinearDepth(g_PreviousDepthBuffer.SampleLevel(g_NearestSampler, previous_uv, 0.0f).x);

        float depth_error     = abs(previous_depth - depth) / depth;
        float depth_tolerance = lerp(1e-2f, 1e-1f, z_alignment) + 1e1f * length(velocity);
        disocclusion_mask     = (depth_error < depth_tolerance ? 0.0f : 1.0f);
    }

    g_DisocclusionMask[did] = disocclusion_mask;
}
