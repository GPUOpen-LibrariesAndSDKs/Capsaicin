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

#ifndef LIGHTS_SHARED_H
#define LIGHTS_SHARED_H

#include "../gpu_shared.h"

enum LightType
{
    kLight_Point = 0xFFF0FF80,
    kLight_Spot,
    kLight_Direction,
    kLight_Environment,
    kLight_Area,
};

struct Light
{
    float4 radiance; // .xyz = radiance, .w = radiance_map
    float4 v1;       // .xyz = 1st vertex pos., .w = 1st packed UVs
    float4 v2;       // .xyz = 2nd vertex pos., .w = 2nd packed UVs
    float4 v3;       // .xyz = 3rd vertex pos., .w = 3rd packed UVs

    /**
     * Get the type of the current light.
     * @returns LightType enum specifying type of light.
     */
    LightType get_light_type()
    {
        // All delta lights have a pack value of 0.0.xxx with the type of light in w
        // To avoid increasing the size of the struct we use this pack variable to distinguish if the light is
        // a delta light or an area light There is the potential for an area light to have v3==0.0.xxx so the
        // light types are represented using bit patterns that cannot occur in float32/16 numbers
#ifndef __cplusplus
        uint index = asuint(v3.w);
#else
        uint index = *reinterpret_cast<uint *>(&v3.w);
#endif
        if (index < kLight_Point)
        {
            return kLight_Area;
        }
        return (LightType)index;
    }

    // The member variable can be interpreted differently depending on the actual type of light being stored
    // The following lists the interpretation for each light type
    // Note: float3s are considered to have same storage size as float4s
    //       All lights apart (from AreaLights) must have asfloat(-1) in the radiance.w variable
    // struct PointLight
    //{
    //    float3 intensity; /**< The light luminous intensity (lm/sr) */
    //    float4 position; /**< The light world space position in xyz with w=range */
    //};
    // struct SpotLight
    //{
    //    float3 intensity; /**< The light luminous intensity (lm/sr) in xyz */
    //    float4 position; /**< The light world space direction in xyz with w=range */
    //    float3 direction; /**< The light world space direction to the light */
    //    float2 angles; /**< x=angle cutoff scale, y=angle cutoff offset */
    //    /**< The cutoff angles are the cosine of the light cutoff angle from center of light (radians) */
    //};
    // struct DirectionLight
    //{
    //    float3 irradiance; /**< The light illuminance (lm/m^2) */
    //    float3 direction; /**< The light world space direction to the light in xyz */
    //};
    // struct EnvironmentLight
    //{
    //    uint lods; /**< The number of mip map levels in each face of the texture */
    //};
};

#ifdef __cplusplus
/**
 * Make a light type from a area light.
 * @param radiance Colour and value of light.
 * @param vertex1  1st vertex position (vertices are expected counter-clockwise in right-handed system).
 * @param vertex2  2nd vertex position.
 * @param vertex3  3rd vertex position.
 * @return A light with the correctly set internal values.
 */
inline Light MakeAreaLight(float3 radiance, float3 vertex1, float3 vertex2, float3 vertex3)
{
    // Create the new light
    Light light;
    light.radiance = float4(radiance, glm::uintBitsToFloat(UINT_MAX));
    light.v1       = float4(vertex1, 0.0f);
    light.v2       = float4(vertex2, 0.0f);
    light.v3       = float4(vertex3, 0.0f);
    return light;
}

/**
 * Make a light type from a textured area light.
 * @param radiance Colour and value of light texture multiplier.
 * @param vertex1  1st vertex position (vertices are expected counter-clockwise in right-handed system).
 * @param vertex2  2nd vertex position.
 * @param vertex3  3rd vertex position.
 * @param texture  Index of radiance map texture.
 * @param uv1      1st vertex uv texture parameter.
 * @param uv2      2nd vertex uv texture parameter.
 * @param uv3      3rd vertex uv texture parameter.
 * @returns A light with the correctly set internal values.
 */
inline Light MakeAreaLight(float3 radiance, float3 vertex1, float3 vertex2, float3 vertex3, uint texture,
    float2 uv1, float2 uv2, float2 uv3)
{
    // Create the new light
    Light light;
    light.radiance = float4(radiance, glm::uintBitsToFloat(texture));
    light.v1       = float4(vertex1, glm::uintBitsToFloat(packHalf2x16(uv1)));
    light.v2       = float4(vertex2, glm::uintBitsToFloat(packHalf2x16(uv2)));
    light.v3       = float4(vertex3, glm::uintBitsToFloat(packHalf2x16(uv3)));
    return light;
}

/**
 * Make a light type from a point light.
 * @param intensity Colour and intensity of light.
 * @param position  Position of the light.
 * @param range     Maximum distance from the light where lighting has an effect.
 * @returns A light with the correctly set internal values.
 */
inline Light MakePointLight(float3 intensity, float3 position, float range)
{
    Light light;
    light.radiance = float4(intensity, glm::uintBitsToFloat(UINT_MAX));
    light.v1       = float4(position, range);
    light.v3       = float4(float3(0.0f), glm::uintBitsToFloat(static_cast<glm::uint>(kLight_Point)));
    return light;
}

/**
 * Make a light type from a spot light.
 * @param intensity       Colour and intensity of light.
 * @param position        Position of the light.
 * @param range           Maximum distance from the light where lighting has an effect.
 * @param direction       The direction to the light along cones view axis.
 * @param outterConeAngle The maximum angle from the cones view axis to the outside of the cone.
 * @param innerConeAngle  The angle from the cones view axis to the inside of the cones penumbra region.
 * @returns A light with the correctly set internal values.
 */
inline Light MakeSpotLight(float3 intensity, float3 position, float range, float3 direction,
    float outterConeAngle, float innerConeAngle)
{
    float cosOutter         = -cosf(outterConeAngle);
    float lightAngleScale   = 1.0f / std::max(0.001f, cosf(innerConeAngle) + cosOutter);
    float sinAngle          = sinf(outterConeAngle);
    float tanAngle          = tanf(outterConeAngle);
    float lightAngleOffset  = cosOutter * lightAngleScale;
    float tanAngleSqPlusOne = 1.0f + (tanAngle * tanAngle);
    Light light;
    light.radiance = float4(intensity, glm::uintBitsToFloat(UINT_MAX));
    light.v1       = float4(position, range);
    light.v2       = float4(normalize(direction), sinAngle);
    light.v3       = float4(lightAngleScale, lightAngleOffset, tanAngleSqPlusOne,
              glm::uintBitsToFloat(static_cast<glm::uint>(kLight_Spot)));
    return light;
}

/**
 * Make a light type from a directional light.
 * @param radiance  Colour and value of light.
 * @param direction Direction to the light along lights view axis.
 * @param range     Maximum distance from the light where lighting has an effect.
 * @return A light with the correctly set internal values.
 */
inline Light MakeDirectionalLight(float3 radiance, float3 direction, float range)
{
    Light light;
    light.radiance = float4(radiance, glm::uintBitsToFloat(UINT_MAX));
    light.v2       = float4(direction, range);
    light.v3       = float4(float3(0.0f), glm::uintBitsToFloat(static_cast<glm::uint>(kLight_Direction)));
    return light;
}

/**
 * Make a light type from a environment light.
 * @param width  The width of each face in the environment cube map.
 * @param height The height of each face in the environment cube map.
 * @return A light with the correctly set internal values.
 */
inline Light MakeEnvironmentLight(uint width, uint height)
{
    Light light;
    uint  lod      = glm::findMSB(glm::max(width, height));
    light.radiance = float4(float3(glm::uintBitsToFloat(lod), 0.0f, 0.0f), glm::uintBitsToFloat(UINT_MAX));
    light.v3 = float4(float3(0.0f), glm::uintBitsToFloat(static_cast<glm::uint>(kLight_Environment)));
    return light;
}
#endif

/**
 * Check if a light is a delta light.
 * @param light The light to be checked.
 * @returns True if a delta light.
 */
inline bool isDeltaLight(Light light)
{
#if defined(DISABLE_DELTA_LIGHTS)
    return false;
#elif !defined(DISABLE_AREA_LIGHTS) && !defined(DISABLE_ENVIRONMENT_LIGHTS)
    return light.get_light_type() != kLight_Area && light.get_light_type() != kLight_Environment;
#elif !defined(DISABLE_AREA_LIGHTS)
    return light.get_light_type() != kLight_Area;
#elif defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return true;
#else /*defined(DISABLE_AREA_LIGHTS)*/
    return light.get_light_type() != kLight_Environment;
#endif
}

/**
 * Check if a light has a known position.
 * @note Lights such as directional and environment do not have positions and only directions.
 * @param light The light to be checked.
 * @returns True if light has a position.
 */
inline bool hasLightPosition(Light light)
{
#if !defined(DISABLE_DELTA_LIGHTS) && !defined(DISABLE_ENVIRONMENT_LIGHTS)
    return light.get_light_type() != kLight_Direction && light.get_light_type() != kLight_Environment;
#elif defined(DISABLE_DELTA_LIGHTS) && !defined(DISABLE_ENVIRONMENT_LIGHTS) && !defined(DISABLE_AREA_LIGHTS)
    return light.get_light_type() != kLight_Environment;
#elif !defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return light.get_light_type() != kLight_Direction;
#elif defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS) && !defined(DISABLE_AREA_LIGHTS)
    return true;
#else
    return false;
#endif
}

#endif
