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

#ifndef QUATERNION_HLSL
#define QUATERNION_HLSL

class Quaternion
{
    float4 values;

    /**
     * Calculates the inverse of a quaternion.
     * @param quaternion The input quaternion.
     * @return The inverted quaternion.
     */
    Quaternion inverse()
    {
        Quaternion ret;
        ret.values = float4(-values.x, -values.y, -values.z, values.w);
        return ret;
    }

    /**
     * Calculates the transformation of a vector and a quaternion.
     * @param quaternion The input quaternion.
     * @param direction The input direction.
     * @return The inverted quaternion.
     */
    float3 transform(float3 direction)
    {
        const float3 qAxis = values.xyz;
        return 2.0f * dot(qAxis, direction) * qAxis + (values.w * values.w - dot(qAxis, qAxis)) * direction + 2.0f * values.w * cross(qAxis, direction);
    }
};

/**
 * Calculates a rotation quaternion based on rotation around positive Z axis.
 * @param values The direction vector to calculate the angle between it and Z axis.
 * @return The created quaternion.
 */
Quaternion QuaternionRotationZ(float3 direction)
{
    Quaternion ret;
    // Handle special case when input is exact or near opposite of (0, 0, 1)
    ret.values = (direction.z >= -0.99999f) ? normalize(float4(direction.y, -direction.x, 0.0f, 1.0f + direction.z)) : float4(1.0f, 0.0f, 0.0f, 0.0f);
    return ret;
}

#endif // QUATERNION_HLSL
