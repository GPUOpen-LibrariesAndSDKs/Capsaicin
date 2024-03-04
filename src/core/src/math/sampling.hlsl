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

#ifndef SAMPLING_HLSL
#define SAMPLING_HLSL

#include "math_constants.hlsl"
#include "math.hlsl"

/**
 * Transforms a 3D vector to a position in the unit square.
 * @param direction The input direction (must be normalised).
 * @returns The 2D mapped value [0, 1].
 */
float2 mapToHemiOctahedron(float3 direction)
{
    // Modified version of "Fast Equal-Area Mapping of the (Hemi)Sphere using SIMD" - Clarberg
    float3 absDir = abs(direction);

    float radius = sqrt(1.0f - absDir.z);
    float a = hmax(absDir.xy);
    float b = hmin(absDir.xy);
    b = a == 0.0f ? 0.0f : b / a;

    float phi = atan(b) * (2.0f / PI);
    phi = (absDir.x >= absDir.y) ? phi : 1.0f - phi;

    float t = phi * radius;
    float s = radius - t;
    float2 st = float2(s, t);
    st *= sign(direction).xy;

    // Since we only care about the hemisphere above the surface we rescale and center the output
    //   value range to the it occupies the whole unit square
    st = float2(st.x + st.y, st.x - st.y);

    // Transform from [-1,1] to [0,1]
    st = 0.5f.xx * st + 0.5f.xx;

    return st;
}

/**
 * Transforms a mapped position in the unit square back to a 3D direction vector.
 * @param mapped The mapped position created using mapToHemiOctahedron.
 * @returns The 3D direction vector.
 */
float3 mapToHemiOctahedronInverse(float2 mapped)
{
    // Transform from [0,1] to [-1,1]
    float2 st = 2.0f.xx * mapped - 1.0f.xx;

    // Transform from unit square to diamond corresponding to +hemisphere
    st = float2(st.x + st.y, st.x - st.y) * 0.5f;

    float2 absMapped = abs(st);
    float distance = 1.0f - hadd(absMapped);
    float radius = 1.0f - abs(distance);

    float phi = (radius == 0.0f) ? 0.0f : QUARTER_PI * ((absMapped.y - absMapped.x) / radius + 1.0f);
    float radiusSqr = radius * radius;
    float sinTheta = radius * sqrt(2.0f - radiusSqr);
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    float x = sinTheta * sign(st.x) * cosPhi;
    float y = sinTheta * sign(st.y) * sinPhi;
    float z = sign(distance) * (1.0f - radiusSqr);

    return float3(x, y, z);
}

void GetOrthoVectors(in float3 n, out float3 b1, out float3 b2)
{
    bool sel = abs(n.z) > 0;
    float3 p2 = sel ? n : n.zyx;
    float k = 1.0f / sqrt(squared(p2.z) + squared(n.y));
    b1 = float3(0.0f, -p2.z * k, n.y * k);
    b1 = sel ? b1 : b1.zyx;
    b2 = cross(n, b1);
}

float2 MapToDisk(in float2 s)
{
    float r = sqrt(s.x);
    float theta = TWO_PI * s.y;

    return float2(r * cos(theta), r * sin(theta));
}

float3 MapToHemisphere(in float2 s, in float3 n, in float e)
{
    float3 u, v;
    GetOrthoVectors(n, u, v);

    float r1 = s.x;
    float r2 = s.y;

    float sinpsi = sin(TWO_PI * r1);
    float cospsi = cos(TWO_PI * r1);
    float costheta = pow(1.0f - r2, 1.0f / (e + 1.0f));
    float sintheta = sqrt(1.0f - costheta * costheta);

    return normalize(u * sintheta * cospsi + v * sintheta * sinpsi + n * costheta);
}

float2 MapToTriangleLowDistortion(in float2 s)
{
    return s.y > s.x ?
        float2(s.x / 2.0f, s.y - s.x / 2.0f) :
        float2(s.x - s.y / 2.0f, s.y / 2.0f);
}

float3 MapToSphere(in float2 s)
{
    float theta = TWO_PI * s.x;
    float phi = acos(1.0f - 2.0f * s.y);

    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);

    return float3(x, y, z);
}

float2 MapToSphereInverse(in float3 p)
{
    float tmp = atan2(p.y, p.x);
    float theta = (tmp < 0.0 ? (tmp + TWO_PI) : tmp);

    float x = theta / TWO_PI;
    float y = (1.0f - p.z) / 2.0f;

    return float2(x, y);
}

float3x3 CreateTBN(in float3 n)
{
    float3 u, v;
    GetOrthoVectors(n, u, v);

    float3x3 TBN;
    TBN[0] = u;
    TBN[1] = v;
    TBN[2] = n;

    return transpose(TBN);
}

#endif // SAMPLING_H
