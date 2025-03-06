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
 * Transforms a mapped position in the unit square back to a 3D direction vector.
 * @param samples The input values on the unit square (range [0, 1)).
 * @return The 3D direction vector.
 */
float3 mapToSphereOctahedron(float2 samples)
{
    samples = 2.0f * samples - 1.0f;
    float3 v = float3(samples.xy, 1.0f - abs(samples.x) - abs(samples.y));
    if (v.z < 0.0f)
        v.xy = (1.0f - abs(v.yx)) * float2(v.x >= 0.0f ? 1.0f : -1.0f, v.y >= 0.0f ? 1.0f : -1.0f);
    return normalize(v);
}

/**
 * Transforms a 3D vector to a position in the unit square.
 * @param direction The input direction (must be normalised).
 * @return The 2D mapped value [0, 1).
 */
float2 mapToSphereOctahedronInverse(float3 direction)
{
    // Project the sphere onto the octahedron, and then onto the xy plane
    float2 p = direction.xy * (1.0f / (abs(direction.x) + abs(direction.y) + abs(direction.z)));
    // Reflect the folds of the lower hemisphere over the diagonals
    return 0.5f * (direction.z <= 0.0f ? ((1.0f - abs(p.yx)) * float2(p.x >= 0.0f ? 1.0f : -1.0f, p.y >= 0.0f ? 1.0f : -1.0f)) : p) + 0.5f;
}

/**
 * Transforms a mapped position in the unit square to a 3D direction vector.
 * @param samples The input values on the unit square (range [0, 1)).
 * @return The 3D direction vector.
 */
float3 mapToHemiOctahedron(float2 samples)
{
    // Transform from [0,1] to [-1,1]
    float2 st = 2.0f.xx * samples - 1.0f.xx;

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

/**
 * Transforms a 3D vector to a position in the unit square.
 * @param direction The input direction (must be normalised).
 * @return The 2D mapped value [0, 1).
 */
float2 mapToHemiOctahedronInverse(float3 direction)
{
    // Modified version of "Fast Equal-Area Mapping of the (Hemi)Sphere using SIMD" - Clarberg
    float3 absDir = abs(direction);

    float radius = sqrt(1.0f - absDir.z);
    float a = hmax(absDir.xy);
    float b = hmin(absDir.xy);
    b = a == 0.0f ? 0.0f : b / a;

    float phi = atan(b) * TWO_INV_PI;
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

void GetOrthoVectors(in float3 n, out float3 b1, out float3 b2)
{
    bool sel = abs(n.z) > 0;
    float3 p2 = sel ? n : n.zyx;
    float k = 1.0f / sqrt(squared(p2.z) + squared(n.y));
    b1 = float3(0.0f, -p2.z * k, n.y * k);
    b1 = sel ? b1 : b1.zyx;
    b2 = cross(n, b1);
}

/**
 * Transforms a mapped position in the unit square to a disk using polar mapping.
 * @param samples The input values on the unit square (range [0, 1)).
 * @return The 2D disk coordinates.
 */
float2 mapToDisk(float2 samples)
{
    float r = sqrt(samples.x);
    float theta = TWO_PI * samples.y;
    float sinTheta, cosTheta;
    sincos(theta, sinTheta, cosTheta);
    return float2(r * cosTheta, r * sinTheta);
}

/**
 * Transforms coordinates in a unit disk to a position in the unit square.
 * @param coordinates The input disk coordinates.
 * @return The 2D mapped value [0, 1).
 */
float2 mapToDiskInverse(float2 coordinates)
{
    float rSqr = lengthSqr(coordinates);
    float theta = atan2(coordinates.y, coordinates.x);
    float2 samples = float2(rSqr, theta * INV_TWO_PI);
    samples.y = samples.y >= 0.0f ? samples.y : samples.y + 1.0f;
    return samples;
}

/**
 * Transforms a mapped position in the unit square to a cosine weighted direction around the +z axis oriented hemisphere.
 * @param samples The input values on the unit square (range [0, 1)).
 * @return The sampled direction (with PDF = z/PI).
 */
float3 mapToCosineHemisphere(float2 samples)
{
    // Ray Tracing Gems - Sampling Transformations Zoo - Shirley
    float sinTheta = sqrt(samples.x);
    float phi = TWO_PI * samples.y;
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    return float3(sinTheta * cosPhi, sinTheta * sinPhi, sqrt(1.0f - samples.x));
}

/**
 * Transforms coordinates in a +z axis oriented hemisphere to a position in the unit square.
 * @param direction The input hemisphere direction.
 * @return The 2D mapped value [0, 1).
 */
float2 mapToCosineHemisphereInverse(float3 direction)
{
    float r = sqrt(direction.x);
    float theta = TWO_PI * direction.y;
    float sinTheta, cosTheta;
    sincos(theta, sinTheta, cosTheta);
    return float2(r * cosTheta, r * sinTheta);
}

/**
 * Transforms a mapped position in the unit square to a cosine weighted direction around a oriented hemisphere.
 * @param samples The input values on the unit square (range [0, 1)).
 * @param normal  Normal direction used to orient the hemisphere.
 * @return The sampled direction.
 */
float3 mapToCosineHemisphere(float2 samples, float3 normal)
{
    float3 u, v;
    GetOrthoVectors(normal, u, v);

    float phi = TWO_PI * samples.y;
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    float sinTheta = sqrt(samples.x);
    float cosTheta = sqrt(1.0f - samples.x);

    return normalize(u * sinTheta * cosPhi + v * sinTheta * sinPhi + normal * cosTheta);
}

/**
 * Transforms a cosine weighted direction around a oriented hemisphere to a position in the unit square.
 * @param direction The input hemisphere direction.
 * @param normal    Normal direction used to orient the hemisphere.
 * @return The 2D mapped value [0, 1).
 */
float2 mapToCosineHemisphereInverse(float3 direction, float3 normal)
{
    float3 u, v;
    GetOrthoVectors(normal, u, v);

    float3 local = float3(dot(u, direction),
        dot(v, direction),
        dot(normal, direction));
    float phi = atan2(local.y, local.x) * INV_TWO_PI;
    phi = phi >= 0.0f ? phi : 1.0f + phi;
    return float2(1.0f - local.z * local.z, phi);
}

/**
 * Transforms a position in the unit square uniformly to a position on the unit hemisphere.
 * @param samples The input values on the unit square (range [0, 1)).
 * @param normal  Normal direction used to orient the hemisphere.
 * @return The position on the unit hemisphere (a 3D direction vector).
 */
float3 mapToHemisphere(float2 samples, float3 normal)
{
    float3 u, v;
    GetOrthoVectors(normal, u, v);

    float theta = TWO_PI * samples.x;
    // Ensure uniform mapping of values by redistributing phi to prevent 'clumping' at the poles
    float phiUniform = 1.0f - samples.y;
    float phi = acos(phiUniform);
    float sinTheta, cosTheta;
    sincos(theta, sinTheta, cosTheta);
    float sinPhi = sin(phi);
    float cosPhi = phiUniform;
    float x = sinPhi * cosTheta;
    float y = sinPhi * sinTheta;
    float z = cosPhi;

    return normalize(u * x + v * y + normal * z);
}

/**
 * Transforms a position on the unit hemisphere uniformly to a position in the unit square.
 * @param direction The input values on the unit hemisphere (normalised direction vector).
 * @param normal    Normal direction used to orient the hemisphere.
 * @return The mapped values in the unit square [0, 1).
 */
float2 mapToHemisphereInverse(float3 direction, float3 normal)
{
    float3 u, v;
    GetOrthoVectors(normal, u, v);

    float3 local = float3(dot(u, direction),
        dot(v, direction),
        dot(normal, direction));

    float tmp = atan2(local.y, local.x);
    float theta = (tmp < 0.0 ? (tmp + TWO_PI) : tmp);

    float x = theta * INV_TWO_PI;
    float y = 1.0f - local.z;

    return float2(x, y);
}

/**
 * Transforms a position in the unit square uniformly to a position on the unit sphere.
 * @param samples The input values on the unit square (range [0, 1)).
 * @return The position on the unit sphere (a 3D direction vector).
 */
float3 mapToSphere(float2 samples)
{
    float theta = TWO_PI * samples.x;
    // Ensure uniform mapping of values by redistributing phi to prevent 'clumping' at the poles
    float phi = acos(1.0f - 2.0f * samples.y);
    float sinTheta, cosTheta;
    sincos(theta, sinTheta, cosTheta);
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);

    float x = sinPhi * cosTheta;
    float y = sinPhi * sinTheta;
    float z = cosPhi;

    return float3(x, y, z);
}

/**
 * Transforms a position on the unit sphere uniformly to a position in the unit square.
 * @param direction The input values on the unit sphere (normalised direction vector).
 * @return The mapped values in the unit square [0, 1).
 */
float2 mapToSphereInverse(float3 direction)
{
    float tmp = atan2(direction.y, direction.x);
    float theta = (tmp < 0.0 ? (tmp + TWO_PI) : tmp);

    float x = theta * INV_TWO_PI;
    float y = (1.0f - direction.z) / 2.0f;

    return float2(x, y);
}

/**
 * Transforms a position in the unit square to a Guassian distributed position centered in the unit square.
 * @param samples The input values on the unit square (range [0, 1)).
 * @return The Guassian distributed position.
 */
float2 mapToGaussian(float2 samples)
{
    // Marsaglia polar method
    float2 v = samples * 2.0f.xx - 1.0f.xx;
    float s = clamp(lengthSqr(v), FLT_MIN, 1.0f - FLT_EPSILON);

    s = sqrt((-2.0f * log(s)) / s);

    float2 points = v * s;
    const float mean = 0.5f;
    const float deviation = 0.5f / 3.0f; //~99.7% of samples within +-0.5 of mean
    return mean.xx + points * deviation.xx;
}

float2 MapToTriangleLowDistortion(in float2 s)
{
    return s.y > s.x ?
        float2(s.x / 2.0f, s.y - s.x / 2.0f) :
        float2(s.x - s.y / 2.0f, s.y / 2.0f);
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
