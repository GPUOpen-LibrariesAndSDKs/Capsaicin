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

#include "math/math.hlsl"

/**
 * Offset ray function from Ray Tracing Gems (chapter 6).
 * @note Offsets ray start position from surface to prevent self intersections
 * @param position Current position.
 * @param normal   Geometric normal vector at current position.
 * @return The offset position.
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
 * @param v0           Triangles first vertex.
 * @param v1           Triangles second vertex.
 * @param v2           Triangles third vertex.
 * @param barycentrics Barycentric coordinates of position in triangle to interpolate to.
 * @return The position at the barycentric coordinates.
 */
float2 interpolate(const float2 v0, const float2 v1, const float2 v2, const float2 barycentrics)
{
    return (1.0f - barycentrics.x - barycentrics.y) * v0 + barycentrics.x * v1 + barycentrics.y * v2;
}

/**
 * Determine the location within a 3D triangle using barycentric coordinates.
 * @param v0           Triangles first vertex.
 * @param v1           Triangles second vertex.
 * @param v2           Triangles third vertex.
 * @param barycentrics Barycentric coordinates of position in triangle to interpolate to.
 * @return The position at the barycentric coordinates.
 */
float3 interpolate(const float3 v0, const float3 v1, const float3 v2, const float2 barycentrics)
{
    return (1.0f - barycentrics.x - barycentrics.y) * v0 + barycentrics.x * v1 + barycentrics.y * v2;
}

//
/**
 * Compute barycentric coordinates (x,y) for point p with respect to triangle (v0,v1,v2)
 * @param v0       Triangles first vertex.
 * @param v1       Triangles second vertex.
 * @param v2       Triangles third vertex.
 * @param position Position in triangle
 * @return The barycentric coordinates.
 */
float2 triangleBarycentric(const float3 v0, const float3 v1, const float3 v2, const float3 position)
{
    float3 l0 = v1 - v0, l1 = v2 - v0, l2 = position - v0;
    float d00 = dot(l0, l0);
    float d01 = dot(l0, l1);
    float d11 = dot(l1, l1);
    float d20 = dot(l2, l0);
    float d21 = dot(l2, l1);
    float denom = d00 * d11 - d01 * d01;
    float x = (d11 * d20 - d01 * d21) / denom;
    float y = (d00 * d21 - d01 * d20) / denom;
    return float2(x, y);
}

/**
 * Calculate UV space motion vector between 2 points.
 * @param currentPos  Homogeneous clip space coordinate for the first position.
 * @param previousPos Homogeneous clip space coordinate for the second position.
 * @return The screen space distance between the 2 points
 */
float2 CalculateMotionVector(float4 currentPos, float4 previousPos)
{
    float2 current_uv = 0.5f * currentPos.xy / currentPos.w;
    float2 previous_uv = 0.5f * previousPos.xy / previousPos.w;

    return (current_uv - previous_uv) * float2(1.0f, -1.0f);
}

/**
 * Calculate UV space motion vector between 2 points.
 * @note This differs from the other version of this function in that the inputs are expected in
 *   non-homogeneous clip space i.e. already divided b 'w'.
 * @param currentPos  Clip space coordinate for the first position.
 * @param previousPos Clip space coordinate for the second position.
 * @return The screen space distance between the 2 points
 */
float2 CalculateMotionVector(float3 currentPos, float3 previousPos)
{
    float2 current_uv = 0.5f * currentPos.xy;
    float2 previous_uv = 0.5f * previousPos.xy;

    return (current_uv - previous_uv) * float2(1.0f, -1.0f);
}

#endif // GEOMETRY_HLSL
