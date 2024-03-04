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

#ifndef GEOMETRY_HLSL
#define GEOMETRY_HLSL

#include "../math/math.hlsl"

/**
 * Offset ray function from Ray Tracing Gems (chapter 6).
 * @note Offsets ray start position from surface to prevent self intersections
 * @param position Current position.
 * @param normal   Geometric normal vector at current position.
 * @returns The offset position.
 */
float3 offsetPosition(const float3 position, const float3 normal)
{
    static const float origin = 1.0f / 32.0f;
    static const float floatScale = 1.0f / 65536.0f;
    static const float intScale = 256.0f;

    int3 intN = int3(normal * intScale.xxx);

    float3 positionI = asfloat(asint(position) + select(position < 0, -intN, intN));

    float3 posOffset = position + (normal * floatScale.xxx);
    return select(abs(position) < origin.xxx, posOffset, positionI);
}

/**
 * Determine the location within a 2D triangle using barycentric coordinates.
 * @param v1 Triangles first vertex.
 * @param v2 Triangles second vertex.
 * @param v3 Triangles third vertex.
 * @returns The position at the barycentric coordinates.
 */
float2 interpolate(const float2 v0, const float2 v1, const float2 v2, const float2 barycentrics)
{
    return (1.0f - barycentrics.x - barycentrics.y) * v0 + barycentrics.x * v1 + barycentrics.y * v2;
}

/**
 * Determine the location within a 3D triangle using barycentric coordinates.
 * @param v1 Triangles first vertex.
 * @param v2 Triangles second vertex.
 * @param v3 Triangles third vertex.
 * @returns The position at the barycentric coordinates.
 */
float3 interpolate(const float3 v0, const float3 v1, const float3 v2, const float2 barycentrics)
{
    return (1.0f - barycentrics.x - barycentrics.y) * v0 + barycentrics.x * v1 + barycentrics.y * v2;
}

float2 CalculateMotionVector(float4 current_pos, float4 previous_pos)
{
    float2 current_uv = 0.5f * current_pos.xy / current_pos.w;
    float2 previous_uv = 0.5f * previous_pos.xy / previous_pos.w;

    return (current_uv - previous_uv) * float2(1.0f, -1.0f);
}

#endif // GEOMETRY_HLSL
