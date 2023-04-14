/**********************************************************************
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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

float3 g_Eye;
uint   g_FrameIndex;

StructuredBuffer<Material> g_MaterialBuffer;

Texture2D    g_TextureMaps[];
SamplerState g_TextureSampler;

#include "../../math/math.hlsl"
#include "../../materials/materials.hlsl"

struct Params
{
    float4 position   : SV_Position;
    float3 normal     : NORMAL;
    float2 uv         : TEXCOORD;
    float3 world      : POSITION0;
    float4 current    : POSITION1;
    float4 previous   : POSITION2;
    uint   instanceID : INSTANCE_ID;
    uint   materialID : MATERIAL_ID;
};

struct Pixel
{
    float4 visibility : SV_Target0;
    float4 normal     : SV_Target1;
    float4 details    : SV_Target2;
    float2 velocity   : SV_Target3;
};

float2 CalculateMotionVector(in float4 current_pos, in float4 previous_pos)
{
    float2 current_uv  = 0.5f * current_pos.xy  / current_pos.w;
    float2 previous_uv = 0.5f * previous_pos.xy / previous_pos.w;

    return (current_uv - previous_uv) * float2(1.0f, -1.0f);
}

Pixel main(in Params params, in float3 barycentrics : SV_Barycentrics, in uint primitiveID : SV_PrimitiveID, in bool is_front_face : SV_IsFrontFace)
{
    Pixel    pixel;
    Material material = g_MaterialBuffer[params.materialID];

    uint alphaMap  = asuint(material.albedo.w);
    uint normalMap = asuint(material.normal_ao.x);

    if (alphaMap != uint(-1))
    {
        float alpha = g_TextureMaps[alphaMap].SampleLevel(g_TextureSampler, params.uv, 0).w;
        if (alpha < 0.5f)
        {
            discard;
        }
    }

    float3 normal  = normalize(params.normal);
    float3 details = normal;

    if (normalMap != uint(-1))
    {
        float3 view_direction = normalize(g_Eye - params.world);

        float3 dp1  = ddx(-view_direction);
        float3 dp2  = ddy(-view_direction);
        float2 duv1 = ddx(params.uv);
        float2 duv2 = ddy(params.uv);

        float3 dp2perp   = normalize(cross(dp2, normal));
        float3 dp1perp   = normalize(cross(normal, dp1));
        float3 tangent   = dp2perp * duv1.x + dp1perp * duv2.x;
        float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;

        float    invmax  = rsqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
        float3x3 tbn     = transpose(float3x3(tangent * invmax, bitangent * invmax, normal));
        float3   disturb = 2.0f * g_TextureMaps[normalMap].Sample(g_TextureSampler, params.uv).xyz - 1.0f;

        details = normalize(mul(tbn, disturb));
    }

    if (!is_front_face)
    {
        normal  = -normal;
        details = -details;
    }

    pixel.visibility = float4(barycentrics.yz, asfloat(params.instanceID), asfloat(primitiveID));
    pixel.normal     = float4(0.5f * normal  + 0.5f, 1.0f);
    pixel.details    = float4(0.5f * details + 0.5f, 1.0f);
    pixel.velocity   = CalculateMotionVector(params.current, params.previous);

    return pixel;
}
