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

#ifndef HASH_HLSL
#define HASH_HLSL

#include "math.hlsl"

/**
 * Hash an input value based on PCG hashing function.
 * @param value The input value to hash.
 * @return The calculated hash value.
 */
uint pcgHash(uint value)
{
    // https://www.pcg-random.org/
    uint state = value * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

/**
 * Hash two input values based on PCG hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
uint pcgHash(uint2 values)
{
    values = values * 1664525u + 1013904223u;
    values.x += values.y * 1664525u;
    values.y += values.x * 1664525u;
    values = values ^ (values >> 16u);
    values.x += values.y * 1664525u;
    values.y += values.x * 1664525u;
    values = values ^ (values >> 16u);
    return hadd(values);
}

/**
 * Hash three input values based on PCG hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
uint pcgHash(uint3 values)
{
    // Hash Functions for GPU Rendering - Jarzynski
    values = values * 1664525u + 1013904223u;
    values.x += values.y * values.z;
    values.y += values.z * values.x;
    values.z += values.x * values.y;
    values ^= values >> 16u;
    values.x += values.y * values.z;
    values.y += values.z * values.x;
    values.z += values.x * values.y;
    return hadd(values);
}

/**
 * Hash four input values based on PCG hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
uint pcgHash(uint4 values)
{
    values = values * 1664525u + 1013904223u;
    values.x += values.y * values.w;
    values.y += values.z * values.x;
    values.z += values.x * values.y;
    values.w += values.y * values.z;
    values ^= values >> 16u;
    values.x += values.y * values.w;
    values.y += values.z * values.x;
    values.z += values.x * values.y;
    values.w += values.y * values.z;
    return hadd(values);
}

/**
 * Hash an input value based on xxHash hashing function.
 * @param value The input value to hash.
 * @return The calculated hash value.
 */
uint xxHash(uint value)
{
    // xxhash (https://github.com/Cyan4973/xxHash)
    const uint prime32_2 = 2246822519u, prime32_3 = 3266489917u;
    const uint prime32_4 = 668265263u,  prime32_5 = 374761393u;
    uint ret = value + prime32_5;
    ret = prime32_4 * ((ret << 17) | (ret >> (32 - 17)));
    ret = prime32_2 * (ret ^ (ret >> 15));
    ret = prime32_3 * (ret ^ (ret >> 13));
    return ret ^ (ret >> 16);
}

/**
 * Hash two input values based on xxHash hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
uint xxHash(uint2 values)
{
    // xxhash (https://github.com/Cyan4973/xxHash)
    const uint prime32_2 = 2246822519u, prime32_3 = 3266489917u;
    const uint prime32_4 = 668265263u,  prime32_5 = 374761393u;
    uint ret = values.y + prime32_5 + values.x * prime32_3;
    ret = prime32_4 * ((ret << 17) | (ret >> (32 - 17)));
    ret = prime32_2 * (ret ^ (ret >> 15));
    ret = prime32_3 * (ret ^ (ret >> 13));
    return ret ^ (ret >> 16);
}

/**
 * Hash three input values based on xxHash hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
uint xxHash(uint3 values)
{
    // xxhash (https://github.com/Cyan4973/xxHash)
    const uint prime32_2 = 2246822519u, prime32_3 = 3266489917u;
    const uint prime32_4 = 668265263u,  prime32_5 = 374761393u;
    uint ret = values.z + prime32_5 + values.x * prime32_3;
    ret = prime32_4 * ((ret << 17) | (ret >> (32 - 17)));
    ret += values.y * prime32_3;
    ret = prime32_4 * ((ret << 17) | (ret >> (32 - 17)));
    ret = prime32_2 * (ret ^ (ret >> 15));
    ret = prime32_3 * (ret ^ (ret >> 13));
    return ret ^ (ret >> 16);
}

/**
 * Hash four input values based on xxHash hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
uint xxHash(uint4 values)
{
    // xxhash (https://github.com/Cyan4973/xxHash)
    const uint prime32_2 = 2246822519u, prime32_3 = 3266489917u;
    const uint prime32_4 = 668265263u,  prime32_5 = 374761393u;
    uint ret = values.w + prime32_5 + values.x * prime32_3;
    ret = prime32_4 * ((ret << 17) | (ret >> (32 - 17)));
    ret += values.y * prime32_3;
    ret = prime32_4 * ((ret << 17) | (ret >> (32 - 17)));
    ret += values.z * prime32_3;
    ret = prime32_4 * ((ret << 17) | (ret >> (32 - 17)));
    ret = prime32_2 * (ret ^ (ret >> 15));
    ret = prime32_3 * (ret ^ (ret >> 13));
    return ret ^ (ret >> 16);
}

/**
 * Converts an input integer [0, UINT_MAX) to float [0, 1).
 * @param value The input value to convert.
 * @return The converted float value.
 */
float hashToFloat(uint value)
{
    // Note: Use the upper 24 bits to avoid a bias due to floating point rounding error.
    float ret = (float)(value >> 8) * 0x1.0p-24f;
    return ret;
}

/**
 * Hash two input values based on trig hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
float trigHash(float2 value)
{
    // On generating random numbers, with help of y= [(a+x)sin(bx)] mod 1 - Rey
    return frac(43757.5453f * sin(dot(value, float2(12.9898f, 78.233f))));
}

/**
 * Hash three input values based on trig hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
float trigHash(float3 values)
{
    return trigHash(float2(trigHash(values.xy), values.z));
}

/**
 * Hash four input values based on trig hashing function.
 * @param values The input values to hash.
 * @return The calculated hash value.
 */
float trigHash(float4 values)
{
    return trigHash(float3(trigHash(values.xy), values.z, values.w));
}

#endif // HASH_HLSL
