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

#ifndef PACK_HLSL
#define PACK_HLSL

#include "../gpu_shared.h"

/**
 * Convert float value to single 8bit unorm.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float value.
 * @returns 8bit unorm in lower 8bits, high bits are all zero.
 */
uint packUnorm1x8(float value)
{
    uint packedValue = uint(saturate(value) * 255.0f);
    return packedValue;
}

/**
 * Pack 2 float values to 8bit unorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 8bit unorms in lower bits, high bits are all zero.
 */
uint packUnorm2x8(float2 value)
{
    uint2 packedValue = uint2(saturate(value) * 255.0f.xx);
    return packedValue.x | (packedValue.y << 8);
}

/**
 * Pack 3 float values to 8bit unorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 8bit unorms in lower bits, high bits are all zero.
 */
uint packUnorm3x8(float3 value)
{
    uint3 packedValue = uint3(saturate(value) * 255.0f.xxx);
    return packedValue.x | (packedValue.y << 8) | (packedValue.z << 16);
}

/**
 * Pack 4 float values to 8bit unorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 8bit unorms.
 */
uint packUnorm4x8(float4 value)
{
    uint4 packedValue = uint4(saturate(value) * 255.0f.xxxx);
    return packedValue.x | (packedValue.y << 8) | (packedValue.z << 16) | (packedValue.w << 24);
}

/**
 * Convert 8bit unorm to float.
 * @param packedValue Input unorm value to convert.
 * @returns Converted float value (range [0,1]).
 */
float unpackUnorm1x8(uint packedValue)
{
    return float(packedValue & 0xFFu) * (1.0f / 255.0f);
}

/**
 * Convert 8bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float2 unpackUnorm2x8(uint packedValue)
{
    uint2 value = uint2(packedValue, packedValue >> 8) & 0xFFu.xx;
    return float2(value) * (1.0f / 255.0f).xx;
}

/**
 * Convert 8bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float3 unpackUnorm3x8(uint packedValue)
{
    uint3 value = uint3(packedValue, packedValue >> 8,
        packedValue >> 16) & 0xFFu.xxx;
    return float3(value) * (1.0f / 255.0f).xxx;
}

/**
 * Convert 8bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float4 unpackUnorm4x8(uint packedValue)
{
    uint4 value = uint4(packedValue, packedValue >> 8, packedValue >> 16, packedValue >> 24) & 0xFFu.xxxx;
    return float4(value) * (1.0f / 255.0f).xxxx;
}

/**
 * Convert float value to single 8bit snorm.
 * @note Input values are clamped to the [-1, 1] range.
 * @param value Input float value.
 * @returns 8bit snorm in lower 8bits, high bits are all zero.
 */
uint packSnorm1x8(float value)
{
    uint packedValue = uint(clamp(value, -1.0f, 1.0f) * 127.0f + (0.5f * sign(value)));
    return packedValue;
}

/**
 * Pack 2 float values to 8bit snorm values.
 * @note Input values are clamped to the [-1, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 8bit snorms in lower bits, high bits are all zero.
 */
uint packSnorm2x8(float2 value)
{
    uint2 packedValue = uint2(clamp(value, -1.0f.xx, 1.0f.xx) * 127.0f.xx + (0.5f.xx * sign(value))) & 0xFFu.xx;
    return packedValue.x | (packedValue.y << 8);
}

/**
 * Pack 3 float values to 8bit snorm values.
 * @note Input values are clamped to the [-1, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 8bit snorms in lower bits, high bits are all zero.
 */
uint packSnorm3x8(float3 value)
{
    uint3 packedValue = uint3(clamp(value, -1.0f.xxx, 1.0f.xxx) * 127.0f.xxx + (0.5f.xxx * sign(value))) & 0xFFu.xxx;
    return packedValue.x | (packedValue.y << 8) | (packedValue.z << 16);
}

/**
 * Pack 4 float values to 8bit snorm values.
 * @note Input values are clamped to the [-1, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 8bit snorms.
 */
uint packSnorm4x8(float4 value)
{
    uint4 packedValue = uint4(clamp(value, -1.0f.xxxx, 1.0f.xxxx) * 127.0f.xxxx + (0.5f.xxxx * sign(value))) & 0xFFu.xxxx;
    return packedValue.x | (packedValue.y << 8) | (packedValue.z << 16) | (packedValue.w << 24);
}

/**
 * Convert 8bit snorm to float.
 * @param packedValue Input snorm value to convert.
 * @returns Converted float value (range [-1,1]).
 */
float unpackSnorm1x8(uint packedValue)
{
    return float(packedValue & 0xFFu) * (1.0f / 127.0f);
}

/**
 * Convert 8bit snorms to floats.
 * @param packedValue Input snorm values to convert.
 * @returns Converted float values (range [-1,1]).
 */
float2 unpackSnorm2x8(uint packedValue)
{
    uint2 value = uint2(packedValue, packedValue >> 8) & 0xFFu.xx;
    return float2(value) * (1.0f / 127.0f).xx;
}

/**
 * Convert 8bit snorms to floats.
 * @param packedValue Input snorm values to convert.
 * @returns Converted float values (range [-1,1]).
 */
float3 unpackSnorm3x8(uint packedValue)
{
    uint3 value = uint3(packedValue, packedValue >> 8,
        packedValue >> 16) & 0xFFu.xxx;
    return float3(value) * (1.0f / 127.0f).xxx;
}

/**
 * Convert 8bit snorms to floats.
 * @param packedValue Input snorm values to convert.
 * @returns Converted float values (range [-1,1]).
 */
float4 unpackSnorm4x8(uint packedValue)
{
    uint4 value = uint4(packedValue, packedValue >> 8, packedValue >> 16, packedValue >> 24) & 0xFFu.xxxx;
    return float4(value) * (1.0f / 127.0f).xxxx;
}

/**
 * Convert float value to single 16bit unorm.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float value.
 * @returns 16bit unorm in lower 16bits, high bits are all zero.
 */
uint packUnorm1x16(float value)
{
    uint packedValue = uint(saturate(value) * 65535.0f);
    return packedValue;
}

/**
 * Pack 2 float values to 16bit unorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 16bit unorms
 */
uint packUnorm2x16(float2 value)
{
    uint2 packedValue = uint2(saturate(value) * 65535.0f);
    return packedValue.x | (packedValue.y << 16);
}

/**
 * Pack 3 float values to 16bit unorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 16bit unorms.
 */
uint2 packUnorm3x16(float3 value)
{
    uint3 packedValue = uint3(saturate(value) * 65535.0f);
    return uint2(packedValue.x | (packedValue.y << 16), packedValue.z);
}

/**
 * Pack 4 float values to 16bit unorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 16bit unorms.
 */
uint2 packUnorm4x16(float4 value)
{
    uint4 packedValue = uint4(saturate(value) * 65535.0f);
    return uint2(packedValue.x | (packedValue.y << 16), packedValue.z | (packedValue.w << 16));
}

/**
 * Convert 16bit unorm to float.
 * @param packedValue Input unorm value to convert.
 * @returns Converted float value (range [0,1]).
 */
float unpackUnorm1x16(uint packedValue)
{
    return float(packedValue & 0xFFFFu) * (1.0f / 65535.0f);
}

/**
 * Convert 16bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float2 unpackUnorm2x16(uint packedValue)
{
    uint2 value = uint2(packedValue, packedValue >> 16) & 0xFFFFu.xx;
    return float2(value) * (1.0f / 65535.0f).xx;
}

/**
 * Convert 16bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float3 unpackUnorm3x16(uint packedValue)
{
    uint3 value = uint3(packedValue, packedValue >> 8,
        packedValue >> 16) & 0xFFFFu.xxx;
    return float3(value) * (1.0f / 65535.0f).xxx;
}

/**
 * Convert 16bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float4 unpackUnorm4x16(uint packedValue)
{
    uint4 value = uint4(packedValue, packedValue >> 8,
        packedValue >> 16, packedValue >> 24) & 0xFFFFu.xxxx;
    return float4(value) * (1.0f / 65535.0f).xxxx;
}

/**
 * Convert float value to single 16bit snorm.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float value.
 * @returns 16bit snorm in lower 16bits, high bits are all zero.
 */
uint packSnorm1x16(float value)
{
    uint packedValue = uint(clamp(value, -1.0f, 1.0f) * 32767.0f + (0.5f * sign(value)));
    return packedValue;
}

/**
 * Pack 2 float values to 16bit snorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 16bit snorms
 */
uint packSnorm2x16(float2 value)
{
    uint2 packedValue = uint2(clamp(value, -1.0f.xx, 1.0f.xx) * 32767.0f.xx + (0.5f.xx * sign(value)));
    return packedValue.x | (packedValue.y << 8);
}

/**
 * Pack 3 float values to 16bit snorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 16bit snorms.
 */
uint2 packSnorm3x16(float3 value)
{
    uint3 packedValue = uint3(clamp(value, -1.0f.xxx, 1.0f.xxx) * 32767.0f.xxx + (0.5f.xxx * sign(value)));
    return packedValue.x | (packedValue.y << 8) | (packedValue.z << 16);
}

/**
 * Pack 4 float values to 16bit snorm values.
 * @note Input values are clamped to the [0, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 16bit snorms.
 */
uint2 packSnorm4x16(float4 value)
{
    uint4 packedValue = uint4(clamp(value, -1.0f.xxxx, 1.0f.xxxx) * 32767.0f.xxxx + (0.5f.xxxx * sign(value)));
    return packedValue.x | (packedValue.y << 8) | (packedValue.z << 16) | (packedValue.w << 24);
}

/**
 * Convert 16bit snorm to float.
 * @param packedValue Input snorm value to convert.
 * @returns Converted float value (range [0,1]).
 */
float unpackSnorm1x16(uint packedValue)
{
    return float(packedValue & 0xFFFFu) * (1.0f / 32767.0f);
}

/**
 * Convert 16bit snorms to floats.
 * @param packedValue Input snorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float2 unpackSnorm2x16(uint packedValue)
{
    uint2 value = uint2(packedValue, packedValue >> 8) & 0xFFFFu.xx;
    return float2(value) * (1.0f / 32767.0f).xx;
}

/**
 * Convert 16bit snorms to floats.
 * @param packedValue Input snorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float3 unpackSnorm3x16(uint packedValue)
{
    uint3 value = uint3(packedValue, packedValue >> 8,
        packedValue >> 16) & 0xFFFFu.xxx;
    return float3(value) * (1.0f / 32767.0f).xxx;
}

/**
 * Convert 16bit snorms to floats.
 * @param packedValue Input snorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float4 unpackSnorm4x16(uint packedValue)
{
    uint4 value = uint4(packedValue, packedValue >> 8, packedValue >> 16, packedValue >> 24) & 0xFFFFu.xxxx;
    return float4(value) * (1.0f / 32767.0f).xxxx;
}

/**
 * Pack 2 float values as half precision.
 * @param value Input float values to pack.
 * @returns Packed 16bit half values.
 */
uint packHalf2(float2 value)
{
    uint packedValue = f32tof16(value.x) | (f32tof16(value.y) << 16);
    return packedValue;
}

/**
 * Pack 3 float values as half precision.
 * @param value Input float values to pack.
 * @returns Packed 16bit half values.
 */
uint2 packHalf3(float3 value)
{
    uint2 packedValue = uint2(f32tof16(value.x) | (f32tof16(value.y) << 16),
        f32tof16(value.z));
    return packedValue;
}

/**
 * Pack 4 float values as half precision.
 * @param value Input float values to pack.
 * @returns Packed 16bit half values.
 */
uint2 packHalf4(float4 value)
{
    uint2 packedValue = uint2(f32tof16(value.x) | (f32tof16(value.y) << 16),
        f32tof16(value.z) | (f32tof16(value.w) << 16));
    return packedValue;
}

/**
 * Convert 8bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float2 unpackHalf2(uint packedValue)
{
    return float2(f16tof32(packedValue & 0xFFFFu), f16tof32(packedValue >> 16));
}

/**
 * Convert 8bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float3 unpackHalf3(uint2 packedValue)
{
    return float3(f16tof32(packedValue.x & 0xFFFFu), f16tof32(packedValue.x >> 16),
        f16tof32(packedValue.y & 0xFFFFu));
}

/**
 * Convert 8bit unorms to floats.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values (range [0,1]).
 */
float4 unpackHalf4(uint2 packedValue)
{
    return float4(f16tof32(packedValue.x & 0xFFFFu), f16tof32(packedValue.x >> 16),
        f16tof32(packedValue.y & 0xFFFFu), f16tof32(packedValue.y >> 16));
}

/**
 * Pack normal vector values to 10bit snorm values.
 * @note Input values are clamped to the [-1, 1] range.
 * @param value Input float values to pack.
 * @returns Packed 10bit snorms in lower bits, high bits are all zero.
 */
uint packNormal(float3 value)
{
    uint3 packedValue = uint3(clamp(value, -1.0f.xxx, 1.0f.xxx) * 511.0f.xxx + (0.5f.xxx * sign(value))) & 0x3FFu.xxx;
    return packedValue.x | (packedValue.y << 10) | (packedValue.z << 20);
}

/**
 * Convert 10bit snorms to normal vector.
 * @param packedValue Input snorm values to convert.
 * @returns Converted float values (range [-1,1]).
 */
float3 unpackNormal(uint packedValue)
{
    uint3 value = uint3(packedValue, packedValue >> 10,
        packedValue >> 20) & 0x3FFu.xxx;
    return float3(value) * (1.0f / 511.0f).xxx;
}

/**
 * Pack UV values values to 16bit floats.
 * @param value Input float values to pack.
 * @returns Packed 16 half precision values as single float.
 */
float packUVs(float2 value)
{
    return asfloat(packHalf2(value));
}

/**
 * Convert 16bit halfs to UV values.
 * @param packedValue Input unorm values to convert.
 * @returns Converted float values.
 */
float2 unpackUVs(float packedValue)
{
    uint uv = asuint(packedValue);
    return float2(unpackHalf2(uv));
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


/**
 * Load 2 elements from the buffer.
 * @param buffer Buffer to be read.
 * @param index Index of fetch.
 * @return Requested result.
 */
uint2 Load2(in RWStructuredBuffer<uint> buffer, in uint index)
{
    uint2 value;
    value.x = buffer[2 * index + 0];
    value.y = buffer[2 * index + 1];
    return value;
}

/**
 * Load 3 elements from the buffer.
 * @param buffer Buffer to be read.
 * @param index Index of fetch.
 * @return Requested result.
 */
uint3 Load3(in RWStructuredBuffer<uint> buffer, in uint index)
{
    uint3 value;
    value.x = buffer[3 * index + 0];
    value.y = buffer[3 * index + 1];
    value.z = buffer[3 * index + 2];
    return value;
}

/**
 * Load 4 elements from the buffer.
 * @param buffer Buffer to be read.
 * @param index Index of fetch.
 * @return Requested result.
 */
uint4 Load4(in RWStructuredBuffer<uint> buffer, in uint index)
{
    uint4 value;
    value.x = buffer[4 * index + 0];
    value.y = buffer[4 * index + 1];
    value.z = buffer[4 * index + 2];
    value.w = buffer[4 * index + 3];
    return value;
}

/**
 * Store 2 elements into the buffer.
 * @param buffer Buffer to be written.
 * @param index Index for writing.
 * @param value Value to be stored.
 */
void Store2(in RWStructuredBuffer<uint> buffer, in uint index, in uint2 value)
{
    buffer[2 * index + 0] = value.x;
    buffer[2 * index + 1] = value.y;
}

/**
 * Store 3 elements into the buffer.
 * @param buffer Buffer to be written.
 * @param index Index for writing.
 * @param value Value to be stored.
 */
void Store3(in RWStructuredBuffer<uint> buffer, in uint index, in uint3 value)
{
    buffer[3 * index + 0] = value.x;
    buffer[3 * index + 1] = value.y;
    buffer[3 * index + 2] = value.z;
}

/**
 * Store 4 elements into the buffer.
 * @param buffer Buffer to be written.
 * @param index Index for writing.
 * @param value Value to be stored.
 */
void Store4(in RWStructuredBuffer<uint> buffer, in uint index, in uint4 value)
{
    buffer[4 * index + 0] = value.x;
    buffer[4 * index + 1] = value.y;
    buffer[4 * index + 2] = value.z;
    buffer[4 * index + 3] = value.w;
}

#endif // PACK_HLSL
