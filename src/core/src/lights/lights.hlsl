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

#ifndef LIGHTS_HLSL
#define LIGHTS_HLSL

#include "../lights/lights_shared.h"
#include "../math/pack.hlsl"

/** Light data representing a area light */
struct LightArea
{
    float3 v0; /**< The light triangles first vertex */
    float3 v1; /**< The light triangles second vertex */
    float3 v2; /**< The light triangles third vertex */
    float4 emissivity; /**< The lights emissivity */
    float2 uv0; /**< The lights triangles first vertex uv coordinate */
    float2 uv1; /**< The lights triangles second vertex uv coordinate  */
    float2 uv2; /**< The lights triangles third vertex uv coordinate  */
};

/**
 * Calculates the area light data from an existing light type.
 * @param light The light to retrieve data from.
 * @return The new light.
 */
LightArea MakeLightArea(Light light)
{
    LightArea ret =
    {
        light.v1.xyz, light.v2.xyz, light.v3.xyz, light.radiance, unpackUVs(light.v1.w), unpackUVs(light.v2.w), unpackUVs(light.v3.w)
    };
    return ret;
}

/**
 * Calculates the area light data from member variables.
 * @param vertex0    The surface triangles first vertex.
 * @param vertex1    The surface triangles second vertex.
 * @param vertex2    The surface triangles third vertex.
 * @param emissivity The emitted radiance from the surface (Note: w=lightmap texture).
 * @param uv0        The uv coordinate of the first vertex.
 * @param uv1        The uv coordinate of the second vertex.
 * @param uv2        The uv coordinate of the third vertex.
 */
LightArea MakeLightArea(float3 vertex0, float3 vertex1, float3 vertex2, float4 emissivity, float2 uv0, float2 uv1, float2 uv2)
{
    LightArea ret =
    {
        vertex0, vertex1, vertex2, emissivity, uv0, uv1, uv2
    };
    return ret;
}

/** Light data representing a environment light */
struct LightEnvironment
{
    uint lods; /**< The number of mip map levels in each face of the texture */
};

/**
 * Calculates the environment light data for a light.
 * @param light The light to retrieve data from.
 * @return The new light.
 */
LightEnvironment MakeLightEnvironment(Light light)
{
    LightEnvironment ret =
    {
        asuint(light.radiance.x)
    };
    return ret;
}

/** Light data representing a point light */
struct LightPoint
{
    float3 position; /**< The light world space position */
    float range; /**< The maximum distance from light that can be illuminated */
    float3 intensity; /**< The light luminous intensity (lm/sr) */
};

/**
 * Calculates the point light data from an existing light type.
 * @param light The light to retrieve data from.
 * @return The new light.
 */
LightPoint MakeLightPoint(Light light)
{
    LightPoint ret =
    {
        light.v1.xyz, light.v1.w, light.radiance.xyz
    };
    return ret;
}

/** Light data representing a spot light */
struct LightSpot
{
    float3 position; /**< The light world space position */
    float range; /**< The maximum distance from light that can be illuminated */
    float3 intensity; /**< The light luminous intensity (lm/sr) */
    float3 direction; /**< The light world space direction to the light */
    float angleCutoffScale; /**< The light angle cutoff scale (1 / (cos(innerAngle) - cos(outerAngle))) */
    float angleCutoffOffset; /**< The light angle cutoff offset (-cos(outerAngle) * angleCutoffScale) */
};

/**
 * Calculates the spot light data from an existing light type.
 * @param light The light to retrieve data from.
 * @return The new light.
 */
LightSpot MakeLightSpot(Light light)
{
    LightSpot ret =
    {
        light.v1.xyz, light.v1.w, light.radiance.xyz, light.v2.xyz, light.v3.x, light.v3.y
    };
    return ret;
}

/** Light data representing a directional light */
struct LightDirectional
{
    float3 direction; /**< The light world space direction to the light */
    float range; /**< The maximum distance from light that can be illuminated */
    float3 irradiance; /**< The light illuminance  (lm/m^2) */
};

/**
 * Calculates the spot light data from an existing light type.
 * @param light The light to retrieve data from.
 * @return The new light.
 */
LightDirectional MakeLightDirectional(Light light)
{
    LightDirectional ret =
    {
        light.v2.xyz, light.v2.w, light.radiance.xyz
    };
    return ret;
}

#endif // LIGHTS_HLSL
