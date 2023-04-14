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

#ifndef PACK_HLSL
#define PACK_HLSL

#include "../gpu_shared.h"

uint packUnorm4x8(in float4 value)
{
    uint4 packed_value = uint4(saturate(value) * 255.0f);

    return (packed_value.x << 0) | (packed_value.y << 8) | (packed_value.z << 16) | (packed_value.w << 24);
}

float4 unpackUnorm4x8(in uint packed_value)
{
    uint4 value = uint4((packed_value >> 0) & 0xFFu, (packed_value >> 8) & 0xFFu,
        (packed_value >> 16) & 0xFFu, (packed_value >> 24) & 0xFFu);

    return value / 255.0f;
}

uint packNormal(in float3 normal)
{
    return packUnorm4x8(float4(0.5f * normal + 0.5, 0.0f));
}

float3 unpackNormal(in uint packed_normal)
{
    return 2.0f * unpackUnorm4x8(packed_normal).xyz - 1.0f;
}

float packUVs(in float2 uv)
{
    uint packed_uv = (f32tof16(uv.x) << 16) | f32tof16(uv.y);

    return asfloat(packed_uv);
}

float2 unpackUVs(in float packed_uv)
{
    uint uv = asuint(packed_uv);

    return float2(f16tof32(uv >> 16), f16tof32(uv & 0xFFFFu));
}

/**
 * Pack SDR (0->1) color values to a single uint.
 * @param color Input colour value to pack.
 * @return The packed values.
 */
uint packColor(float3 color)
{
    float3 rgb = clamp(color, 0.0f, asfloat(0x3FFFFFFF)) / 256.0f;
    uint r = ((f32tof16(rgb.r) + 2) >> 2) & 0x000007FF;
    uint g = ((f32tof16(rgb.g) + 2) << 9) & 0x003FF800;
    uint b = ((f32tof16(rgb.b) + 4) << 19) & 0xFFC00000;
    return r | g | b;
}

/**
 * UnPack SDR (0->1) color values created by @packColor.
 * @param packed Input packed value.
 * @return The unpacked values.
 */
float3 unpackColor(uint packed)
{
    float r = f16tof32((packed << 2) & 0x1FFC);
    float g = f16tof32((packed >> 9) & 0x1FFC);
    float b = f16tof32((packed >> 19) & 0x1FF8);
    return float3(r, g, b) * 256.0;
}

/**
 * Pack float3 values to a single uint.
 * @param input Input values to pack.
 * @return The packed values.
 */
uint packFloat3(float3 input)
{
    float3 packed = min(input, asfloat(0x477C0000));
    uint x = ((f32tof16(packed.x) + 8) >> 4) & 0x000007FF;
    uint y = ((f32tof16(packed.y) + 8) << 7) & 0x003FF800;
    uint z = ((f32tof16(packed.z) + 16) << 17) & 0xFFC00000;
    return x | y | z;
}

/**
 * UnPack packed float3 values created by @packFloat3.
 * @param packed Input packed value.
 * @return The unpacked values.
 */
float3 unpackFloat3(uint packed)
{
    float x = f16tof32((packed << 4) & 0x7FF0);
    float y = f16tof32((packed >> 7) & 0x7FF0);
    float z = f16tof32((packed >> 17) & 0x7FE0);
    return float3(x, y, z);
}
#endif // PACK_HLSL
