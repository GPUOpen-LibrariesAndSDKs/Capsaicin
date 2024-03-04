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

#ifndef MATH_HLSL
#define MATH_HLSL

#include "math_constants.hlsl"

/**
 * Clamps a value between (0, 1].
 * @note This prevents values form being clamped to exactly zero but instead to near zero.
 * @param value Value to clamp.
 * @returns The clamped value.
 */
float clampRange(float value)
{
    return max(FLT_EPSILON, min(1.0f, value));
}
float3 clampRange(float3 value)
{
    return max(FLT_EPSILON.xxx, min(1.0f.xxx, value));
}

/**
 * Clamps a value to be greater than epsilon.
 * @note This prevents values form being clamped to exactly zero but instead to near zero.
 * @param value Value to clamp.
 * @returns The clamped value.
 */
float clampMax(float value)
{
    return max(FLT_EPSILON, value);
}
float3 clampMax(float3 value)
{
    return max(FLT_EPSILON.xxx, value);
}

/**
 * Raises a value to the power of 2 i.e. squared.
 * @param value Value to square.
 * @returns The squared value.
 */
float squared(const float value)
{
    return value * value;
}
float3 squared(const float3 value)
{
    return value * value;
}

/**
 * Get the squared length of a vector.
 * @param value Value to get squared length from.
 * @returns The squared value.
 */
float lengthSqr(const float3 value)
{
    return dot(value, value);
}

float2 ndcToUv(const float2 ndc)
{
    return 0.5f * ndc * float2(1.0f, -1.0f) + 0.5f;
}

float2 uvToNdc(const float2 uv)
{
    return float2(uv.x, 1.0f - uv.y) * 2.0f - 1.0f;
}

/**
 * Get the squared distance between 2 points.
 * @param a The first point.
 * @param b The second point.
 * @returns The squared distance.
 */
float distanceSqr(float3 a, float3 b)
{
    float3 c = a - b;
    return dot(c, c);
}

/**
 * Get the largest value of all elements in a vector.
 * @param val The input vector.
 * @returns The largest value.
 */
float hmax(float2 val)
{
    return max(val.x, val.y);
}
float hmax(float3 val)
{
    return max(val.x, max(val.y, val.z));
}
float hmax(float4 val)
{
    return max(max(val.x, val.z), max(val.y, val.w));
}

/**
 * Get the smallest value of all elements in a vector.
 * @param val The input vector.
 * @returns The smallest value.
 */
float hmin(float2 val)
{
    return min(val.x, val.y);
}
float hmin(float3 val)
{
    return min(val.x, min(val.y, val.z));
}
float hmin(float4 val)
{
    return min(min(val.x, val.z), min(val.y, val.w));
}

/**
 * Sum all elements of a vector.
 * @param val The input vector.
 * @returns The combined value.
 */
float hadd(float2 val)
{
    return val.x + val.y;
}
float hadd(float3 val)
{
    return val.x + val.y + val.z;
}
float hadd(float4 val)
{
    return val.x + val.y + val.z + val.w;
}

#if __HLSL_VERSION < 2021
float2 select(bool2 a, float2 b, float2 c)
{
    return a ? b : c;
}

float3 select(bool3 a, float3 b, float3 c)
{
    return a ? b : c;
}

float4 select(bool4 a, float4 b, float4 c)
{
    return a ? b : c;
}

int2 select(bool2 a, int2 b, int2 c)
{
    return a ? b : c;
}

int3 select(bool3 a, int3 b, int3 c)
{
    return a ? b : c;
}

int4 select(bool4 a, int4 b, int4 c)
{
    return a ? b : c;
}

uint2 select(bool2 a, uint2 b, uint2 c)
{
    return a ? b : c;
}

uint3 select(bool3 a, uint3 b, uint3 c)
{
    return a ? b : c;
}

uint4 select(bool4 a, uint4 b, uint4 c)
{
    return a ? b : c;
}

double2 select(bool2 a, double2 b, double2 c)
{
    return a ? b : c;
}

double3 select(bool3 a, double3 b, double3 c)
{
    return a ? b : c;
}

double4 select(bool4 a, double4 b, double4 c)
{
    return a ? b : c;
}

half2 select(bool2 a, half2 b, half2 c)
{
    return a ? b : c;
}

half3 select(bool3 a, half3 b, half3 c)
{
    return a ? b : c;
}

half4 select(bool4 a, half4 b, half4 c)
{
    return a ? b : c;
}

bool2 and(bool2 a, bool2 b)
{
    return a && b;
}

bool3 and(bool3 a, bool3 b)
{
    return a && b;
}

bool4 and(bool4 a, bool4 b)
{
    return a && b;
}

bool2 or(bool2 a, bool2 b)
{
    return a || b;
}

bool3 or(bool3 a, bool3 b)
{
    return a || b;
}

bool4 or(bool4 a, bool4 b)
{
    return a || b;
}
#endif

#endif // MATH_HLSL
