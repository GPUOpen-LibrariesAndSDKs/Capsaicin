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

#include "ssgi_shared.h"

float2 g_NearFar;    //
float4 g_InvDeviceZ; //

ConstantBuffer<SSGIConstants> g_SSGIConstants;
Texture2D<float> g_DepthBuffer;
Texture2D<float3> g_ShadingNormalBuffer;
Texture2D<float3> g_LightingBuffer;
RWTexture2D<float4> g_OcclusionAndBentNormalBuffer;
RWTexture2D<float4> g_NearFieldGlobalIlluminationBuffer;
SamplerState g_PointSampler;

#ifdef UNROLL_SLICE_LOOP
    #define SLICE_COUNT 1
#endif

#ifdef UNROLL_STEP_LOOP
    #define STEP_COUNT 2
#endif

#ifndef SLICE_COUNT
    #define SLICE_COUNT g_SSGIConstants.slice_count
    #define UNROLL_SLICE
#else
    #define UNROLL_SLICE [unroll]
#endif

#ifndef STEP_COUNT
    #define STEP_COUNT g_SSGIConstants.step_count
    #define UNROLL_STEP
#else
    #define UNROLL_STEP [unroll]
#endif

//!
//! Shader includes.
//!

#include "../../math/math_constants.hlsl"
#include "../../components/blue_noise_sampler/blue_noise_sampler.hlsl"
#include "../../render_techniques/gi10/gi10.hlsl"

//!
//! SSGI kernels.
//!

// From h3r2tic's demo
// https://github.com/h3r2tic/rtoy-samples/blob/main/assets/shaders/ssgi/ssgi.glsl
float IntersectDirPlaneOneSided(float3 dir, float3 normal, float3 pt)
{
    float d = -dot(pt, normal);
    float t = d / max(1e-5f, -dot(dir, normal));
    return t;
}

// From Open Asset Import Library
// https://github.com/assimp/assimp/blob/master/include/assimp/matrix3x3.inl
float3x3 RotFromToMatrix(float3 from, float3 to)
{
    float e = dot(from, to);
    float f = abs(e);

    if (f > 1.0f - 0.0003f)
        return float3x3(1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f);

    float3 v   = cross(from, to);
    float h    = 1.f / (1.f + e);      /* optimization by Gottfried Chen */
    float hvx  = h * v.x;
    float hvz  = h * v.z;
    float hvxy = hvx * v.y;
    float hvxz = hvx * v.z;
    float hvyz = hvz * v.y;

    float3x3 mtx;
    mtx[0][0] = e + hvx * v.x;
    mtx[0][1] = hvxy - v.z;
    mtx[0][2] = hvxz + v.y;

    mtx[1][0] = hvxy + v.z;
    mtx[1][1] = e + h * v.y * v.y;
    mtx[1][2] = hvyz - v.x;

    mtx[2][0] = hvxz - v.y;
    mtx[2][1] = hvyz + v.x;
    mtx[2][2] = e + hvz * v.z;

    return mtx;
}

float IntegrateHalfArc(float horizon_angle, float normal_angle)
{
    return (cos(normal_angle) + 2.f * horizon_angle * sin(normal_angle) - cos(2.f * horizon_angle - normal_angle)) / 4.f;
}

float3 IntegrateBentNormal(float horizon_angle0, float horizon_angle1, float normal_angle, float3 world_eye_dir, float3 slice_view_dir)
{
    float t0 = (6.0f * sin(horizon_angle0 - normal_angle) - sin(3.f * horizon_angle0 - normal_angle) +
                6.0f * sin(horizon_angle1 - normal_angle) - sin(3.f * horizon_angle1 - normal_angle) +
                16.f * sin(normal_angle) - 3.f * (sin(horizon_angle0 + normal_angle) + sin(horizon_angle1 + normal_angle))) / 12.f;
    float t1 = (-cos(3.f * horizon_angle0 - normal_angle)
                -cos(3.f * horizon_angle1 - normal_angle) +
                 8.f * cos(normal_angle) - 3.f * (cos(horizon_angle0 + normal_angle) + cos(horizon_angle1 + normal_angle))) / 12.f;

    float3 view_bent_normal  = float3(slice_view_dir.x * t0, slice_view_dir.y * t0, t1);
    float3 world_bent_normal = mul(g_SSGIConstants.inv_view, float4(view_bent_normal, 0.f)).xyz;
    return mul(RotFromToMatrix(-g_SSGIConstants.forward.xyz, world_eye_dir), world_bent_normal);
}

[numthreads(8, 8, 1)]
void Main(int2 did : SV_DispatchThreadID)
{
    if (any(did >= g_SSGIConstants.buffer_dimensions))
    {
        return; // out of bounds
    }

    float2 uv            = (did + 0.5f) / (g_SSGIConstants.buffer_dimensions);
    float3 normal = g_ShadingNormalBuffer.SampleLevel(g_PointSampler, uv, 0);
    if (dot(normal, normal) == 0.0f)
    {
        g_OcclusionAndBentNormalBuffer[did] = float4(0.f, 0.f, 0.f, 0.f);
        g_NearFieldGlobalIlluminationBuffer[did] = float4(0.f, 0.f, 0.f, 0.f);
        return; // discard sky pixels
    }

    float  depth         = g_DepthBuffer.SampleLevel(g_PointSampler, uv, 0);
    float  linear_depth  = GetLinearDepth(depth);

    float3 world_pos     = InverseProject(g_SSGIConstants.inv_view_proj, uv, depth);
    float3 world_normal  = normalize(2.0f * normal - 1.0f);
    float3 world_eye_dir = normalize(g_SSGIConstants.eye.xyz - world_pos);

    float2 noise = BlueNoise_Sample2D(did, g_SSGIConstants.frame_index);        // in [0, 1]

    float  slice_uv_radius = g_SSGIConstants.uv_radius / linear_depth;
    float  ambient_occlusion = 0.f;
    float3 bent_normal = float3(0.f, 0.f, 0.f);
    float3 global_lighting = float3(0.f, 0.f, 0.f);
    UNROLL_SLICE
    for(int slice_index = 0; slice_index < SLICE_COUNT; slice_index++)
    {
        float  slice_angle  = ((slice_index + noise.x) / SLICE_COUNT) * PI;
        float  slice_cos    = cos(slice_angle);
        float  slice_sin    = sin(slice_angle);
        float2 slice_uv_dir = float2(slice_cos, -slice_sin) * slice_uv_radius;

        float3 slice_view_dir  = float3(slice_cos, slice_sin, 0.f);
        float3 slice_world_dir = mul(g_SSGIConstants.inv_view, float4(slice_view_dir, 0.f)).xyz;
        float3 ortho_world_dir = slice_world_dir - dot(slice_world_dir, world_eye_dir) * world_eye_dir;
        float3 proj_axis_dir   = normalize(cross(ortho_world_dir, world_eye_dir));
        float3 proj_world_normal       = world_normal - dot(world_normal, proj_axis_dir) * proj_axis_dir;
        float  proj_world_normal_len   = length(proj_world_normal);
        float  proj_world_normal_cos   = saturate(dot(proj_world_normal, world_eye_dir) / proj_world_normal_len);
        float  proj_world_normal_angle = sign(dot(ortho_world_dir, proj_world_normal)) * acos(proj_world_normal_cos);

        float  side_signs[2] = {1.f, -1.f};
        float  horizon_angles[2];
        [unroll]
        for (int side_index = 0; side_index < 2; ++side_index)
        {
            float  horizon_min = cos(proj_world_normal_angle + side_signs[side_index] * HALF_PI);
            float  horizon_cos = horizon_min;
            float3 prev_sample_world_pos = float3(0.f, 0.f, 0.f);
            UNROLL_STEP
            for(int step_index = 0; step_index < STEP_COUNT; step_index++)
            {
                float  sample_step      = ((step_index + noise.y) / STEP_COUNT);
                float2 sample_uv_offset = sample_step * slice_uv_dir;

                float2 sample_uv        = uv + side_signs[side_index] * sample_uv_offset;
                float  sample_depth     = g_DepthBuffer.SampleLevel(g_PointSampler, sample_uv, 0);
                float3 sample_world_pos = InverseProject(g_SSGIConstants.inv_view_proj, sample_uv, sample_depth);

                float3 horizon_world_dir = sample_world_pos - world_pos;
                float  horizon_world_len = length(horizon_world_dir);       // BE CAREFUL: main source of precision errors
                       horizon_world_dir /= horizon_world_len;
                float  sample_weight     = saturate(horizon_world_len * g_SSGIConstants.falloff_mul + g_SSGIConstants.falloff_add);
                float  sample_cos        = lerp(horizon_min, dot(horizon_world_dir, world_eye_dir), sample_weight);
                float  prev_horizon_cos  = horizon_cos;
                horizon_cos = max(horizon_cos, sample_cos);

                [branch]
                if (sample_cos >= prev_horizon_cos)
                {
                    // From h3r2tic's demo
                    // https://github.com/h3r2tic/rtoy-samples/blob/main/assets/shaders/ssgi/ssgi.glsl
                    float3 sample_lighting     = g_LightingBuffer.SampleLevel(g_PointSampler, sample_uv, 0);
                    float3 sample_world_normal = normalize(2.0f * g_ShadingNormalBuffer.SampleLevel(g_PointSampler, sample_uv, 0) - 1.f);

                    if (step_index > 0)
                    {
                        float3 closest_world_pos = prev_sample_world_pos * min(
                            IntersectDirPlaneOneSided(prev_sample_world_pos, sample_world_normal, sample_world_pos),
                            IntersectDirPlaneOneSided(prev_sample_world_pos, world_normal, world_pos)
                        );

                        prev_horizon_cos = clamp(dot(normalize(closest_world_pos - world_pos), world_eye_dir), prev_horizon_cos, horizon_cos);
                    }

                    float horizon_angle0 = proj_world_normal_angle + max(side_signs[side_index] * acos(prev_horizon_cos) - proj_world_normal_angle, -HALF_PI);
                    float horizon_angle1 = proj_world_normal_angle + min(side_signs[side_index] * acos(horizon_cos)      - proj_world_normal_angle, +HALF_PI);
                    float sample_occlusion = IntegrateHalfArc(horizon_angle0, proj_world_normal_angle) - IntegrateHalfArc(horizon_angle1, proj_world_normal_angle);

                    global_lighting += sample_lighting * sample_occlusion * step(0, dot(-horizon_world_dir, sample_world_normal));
                }

                prev_sample_world_pos = sample_world_pos;
            }

            // Ambient Occlusion
            horizon_angles[side_index] = side_signs[side_index] * acos(horizon_cos);
            ambient_occlusion += proj_world_normal_len * IntegrateHalfArc(horizon_angles[side_index], proj_world_normal_angle);

            // Global Lighting
            global_lighting *= proj_world_normal_len;
        }

        // Bent normal
        bent_normal += proj_world_normal_len * IntegrateBentNormal(horizon_angles[0], horizon_angles[1], proj_world_normal_angle, world_eye_dir, slice_view_dir);
    }

    ambient_occlusion /= SLICE_COUNT;
    bent_normal = normalize(bent_normal);
    global_lighting /= SLICE_COUNT;

    g_OcclusionAndBentNormalBuffer[did] = float4(0.5f * bent_normal + 0.5f, ambient_occlusion);
    g_NearFieldGlobalIlluminationBuffer[did] = float4(global_lighting, 0.f);
}
