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

#ifndef TRANSFORM_HLSL
#define TRANSFORM_HLSL

#include "math.hlsl"

/**
 * Determine a transformation matrix to correctly transform normal vectors.
 * @param transform The original transform matrix.
 * @returns The new transform matrix.
 */
float3x3 getNormalTransform(float3x3 transform)
{
    // The transform for a normal is transpose(inverse(M))
    // The inverse is calculated as [1/det(A)]*transpose(C) where C is the cofactor matrix
    // This simplifies down to [1/det(A)]*C
    float3x3 result;
    result._m00 = determinant(float2x2(transform._m11_m12, transform._m21_m22));
    result._m01 = -determinant(float2x2(transform._m10_m12, transform._m20_m22));
    result._m02 = determinant(float2x2(transform._m10_m11, transform._m20_m21));
    result._m10 = -determinant(float2x2(transform._m01_m02, transform._m21_m22));
    result._m11 = determinant(float2x2(transform._m00_m02, transform._m20_m22));
    result._m12 = -determinant(float2x2(transform._m00_m01, transform._m20_m21));
    result._m20 = determinant(float2x2(transform._m01_m02, transform._m11_m12));
    result._m21 = -determinant(float2x2(transform._m00_m02, transform._m10_m12));
    result._m22 = determinant(float2x2(transform._m00_m01, transform._m10_m11));
    const float3 det3 = transform._m00_m01_m02 * result._m00_m01_m02;
    const float det = 1.0f / hadd(det3);
    return (result * det);
}

/**
 * Transform a normal vector.
 * @note This correctly handles converting the transform to operate correctly on a surface normal.
 * @param normal    The normal vector.
 * @param transform The transform matrix.
 * @returns The transformed normal.
 */
float3 transformNormal(const float3 normal, const float3x4 transform)
{
    const float3x3 normalTransform = getNormalTransform((float3x3)transform);
    return mul(normalTransform, normal);
}

/**
 * Transform a 3D direction vector.
 * @param values    The direction vector.
 * @param transform The transform matrix.
 * @returns The new transform matrix.
 */
float3 transformVector(const float3 values, const float3x4 transform)
{
    return mul((float3x3)transform, values);
}

/**
 * Transform a 3D point by an affine matrix.
 * @param direction The direction vector.
 * @param transform The transform matrix.
 * @returns The new transform matrix.
 */
float3 transformPoint(const float3 values, const float3x4 transform)
{
    return mul(transform, float4(values, 1.0f));
}

/**
 * Transform a 3D point.
 * @note This version of transforming a point assumes a non-affine matrix and will handle
 *  normalisation of the result by the 'w' component.
 * @param direction The direction vector.
 * @param transform The transform matrix.
 * @returns The new transform matrix.
 */
float3 transformPointProjection(const float3 values, const float4x4 transform)
{
    float4 ret = mul(transform, float4(values, 1.0f));
    ret.xyz /= ret.w; // perspective divide
    return ret.xyz;
}

#endif // MATH_HLSL
