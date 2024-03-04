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

#ifndef LIGHT_SAMPLING_VOLUME_HLSL
#define LIGHT_SAMPLING_VOLUME_HLSL

#include "light_evaluation.hlsl"
#include "../geometry/geometry.hlsl"
#include "../math/color.hlsl"

/*
// Requires the following data to be defined in any shader that uses this file
TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);

SamplerState g_TextureSampler;

StructuredBuffer<Light> g_LightBuffer;
StructuredBuffer<uint> g_LightBufferSize;
*/

/*
 * Supports the following config values:
 * LIGHT_SAMPLE_VOLUME_CENTROID = Sample volumes only at single position at centroid of volume
 * THRESHOLD_RADIANCE = A threshold value used to cull area lights, if defined then additional checks
 *   are performed to cull lights based on the size of the sphere of influence defined by the radius at
 *   which the lights contribution drop below the threshold value
 */

/**
 * Calculate the combined luminance(Y) of a light taken within a bounding box.
 * @param selectedLight The light to sample.
 * @param minBB         Bounding box minimum values.
 * @param extent        Bounding box size.
 * @return The calculated combined luminance.
 */
float sampleLightVolume(Light selectedLight, float3 minBB, float3 extent)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    float3 radiance;
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, (1.0f / 3.0f).xx);

        float3 emissivity = light.emissivity.xyz;
#       ifdef THRESHOLD_RADIANCE
        // Quick cull based on range of sphere falloff
        float3 extentCentre = extent * 0.5f.xxx;
        float3 centre = minBB + extentCentre;
        float radiusSqr = dot(extentCentre, extentCentre);
        float radius = sqrt(radiusSqr);
        const float range = sqrt(max(emissivity.x, max(emissivity.y, emissivity.z)) / THRESHOLD_RADIANCE);
        float3 lightDirection = centre - lightPosition;
        if (length(lightDirection) > (radius + range))
        {
            return 0.0f;
        }
#       endif // THRESHOLD_RADIANCE

        uint emissivityTex = asuint(light.emissivity.w);
        if (emissivityTex != uint(-1))
        {
            float2 edgeUV0 = light.uv1 - light.uv0;
            float2 edgeUV1 = light.uv2 - light.uv0;
            // Get texture dimensions in order to determine LOD of visible solid angle
            float2 size;
            g_TextureMaps[NonUniformResourceIndex(emissivityTex)].GetDimensions(size.x, size.y);
            float areaUV = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
            float lod = 0.5f * log2(areaUV);

            float2 uv = interpolate(light.uv0, light.uv1, light.uv2, 0.3333333333333f.xx);
            emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, lod).xyz;
        }

        // Calculate lights surface normal vector
        float3 edge1 = light.v1 - light.v0;
        float3 edge2 = light.v2 - light.v0;
        float3 lightCross = cross(edge1, edge2);
        // Calculate surface area of triangle
        float lightNormalLength = length(lightCross);
        float3 lightNormal = lightCross / lightNormalLength.xxx;
        float lightArea = 0.5f * lightNormalLength;

#       ifdef LIGHT_SAMPLE_VOLUME_CENTROID
        // Evaluate radiance at cell centre
#           ifndef THRESHOLD_RADIANCE
        float3 centre = minBB + (extent * 0.5f.xxx);
#           endif
        float3 lightVector = centre - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        float pdf = saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr).xxx))) * lightArea;
        pdf = pdf / (lightLengthSqr + FLT_EPSILON);
        radiance = emissivity * pdf;
#       else
        // Contribution is emission scaled by surface area converted to solid angle
        // The light is sampled at all 8 corners of the AABB and then interpolated to fill in the internal volume
        float3 maxBB = minBB + extent;
        float3 lightVector = minBB - lightPosition;
        float lightLengthSqr = rcp(lengthSqr(lightVector));
        float pdf = saturate(abs(dot(lightNormal, lightVector * sqrt(lightLengthSqr).xxx))) * lightLengthSqr;
        lightVector = float3(minBB.x, minBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        pdf += saturate(abs(dot(lightNormal, lightVector * sqrt(lightLengthSqr).xxx))) * lightLengthSqr;
        lightVector = float3(minBB.x, maxBB.y, minBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        pdf += saturate(abs(dot(lightNormal, lightVector * sqrt(lightLengthSqr).xxx))) * lightLengthSqr;
        lightVector = float3(minBB.x, maxBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        pdf += saturate(abs(dot(lightNormal, lightVector * sqrt(lightLengthSqr).xxx))) * lightLengthSqr;
        lightVector = float3(maxBB.x, minBB.y, minBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        pdf += saturate(abs(dot(lightNormal, lightVector * sqrt(lightLengthSqr).xxx))) * lightLengthSqr;
        lightVector = float3(maxBB.x, minBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        pdf += saturate(abs(dot(lightNormal, lightVector * sqrt(lightLengthSqr).xxx))) * lightLengthSqr;
        lightVector = float3(maxBB.x, maxBB.y, minBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        pdf += saturate(abs(dot(lightNormal, lightVector * sqrt(lightLengthSqr).xxx))) * lightLengthSqr;
        lightVector = maxBB - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        pdf += saturate(abs(dot(lightNormal, lightVector * sqrt(lightLengthSqr).xxx))) * lightLengthSqr;
        radiance = (emissivity * (lightArea * 0.125f)) * pdf;
#       endif // LIGHT_SAMPLE_VOLUME_CENTROID
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Quick cull based on range of sphere
        float3 extentCentre = extent * 0.5f.xxx;
        float3 centre = minBB + extentCentre;
        float radiusSqr = dot(extentCentre, extentCentre);
        float radius = sqrt(radiusSqr);
        float3 lightDirection = centre - light.position;
        if (length(lightDirection) > (radius + light.range))
        {
            return 0.0f;
        }

        if (lightType == kLight_Spot)
        {
            // Check if spot cone intersects current cell
            // Uses fast cone-sphere test (Hale)
            bool intersect = false;
            float3 coneNormal = selectedLight.v2.xyz;
            float sinAngle = selectedLight.v2.w;
            float tanAngleSqPlusOne = selectedLight.v3.z;
            if (dot(lightDirection + (coneNormal * sinAngle * radius), coneNormal) < 0.0f)
            {
                float3 cd = sinAngle * lightDirection - coneNormal * radius;
                const float lenA = dot(cd, coneNormal);
                intersect = dot(cd, cd) <= lenA * lenA * tanAngleSqPlusOne;
            }
            else
            {
                intersect = dot(lightDirection, lightDirection) <= radiusSqr;
            }
            if (!intersect)
            {
                return 0.0f;
            }
        }

#       ifdef LIGHT_SAMPLE_VOLUME_CENTROID
        // Evaluate radiance at cell centre
        float dist = distance(light.position, centre);
        float distMod = dist / light.range;
        float rad = saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        radiance = light.intensity * rad.xxx;
#       else // LIGHT_SAMPLE_VOLUME_CENTROID
        // For each corner of the cell evaluate the radiance
        float3 maxBB = minBB + extent;
        float recipRange = 1.0f / light.range;
        float dist = distance(light.position, minBB);
        float distMod = dist * recipRange;
        float rad = saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        dist = distance(light.position, float3(minBB.x, minBB.y, maxBB.z));
        distMod = dist * recipRange;
        rad += saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        dist = distance(light.position, float3(minBB.x, maxBB.y, minBB.z));
        distMod = dist * recipRange;
        rad += saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        dist = distance(light.position, float3(minBB.x, maxBB.y, maxBB.z));
        distMod = dist * recipRange;
        rad += saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        dist = distance(light.position, float3(maxBB.x, minBB.y, minBB.z));
        distMod = dist * recipRange;
        rad += saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        dist = distance(light.position, float3(maxBB.x, minBB.y, maxBB.z));
        distMod = dist * recipRange;
        rad += saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        dist = distance(light.position, float3(maxBB.x, maxBB.y, minBB.z));
        distMod = dist * recipRange;
        rad += saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        dist = distance(light.position, maxBB);
        distMod = dist * recipRange;
        rad += saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        radiance = light.intensity * (rad * 0.125f).xxx;
#       endif // LIGHT_SAMPLE_VOLUME_CENTROID
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        radiance = light.irradiance;
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to use of cube map all 6 sides must be individually sampled
        radiance = g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, 1.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, -1.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 1.0f, 0.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, -1.0f, 0.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(1.0f, 0.0f, 0.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(-1.0f, 0.0f, 0.0f), light.lods).xyz;
        radiance *= FOUR_PI / 6.0f;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
    return luminance(radiance);
#endif
}


/**
 * Calculate the combined luminance(Y) of a light taken within a bounding box visible from a surface orientation.
 * @param selectedLight The light to sample.
 * @param minBB         Bounding box minimum values.
 * @param extent        Bounding box size.
 * @param normal        The face normal of the bounding box region.
 * @return The calculated combined luminance.
 */
float sampleLightVolumeNormal(Light selectedLight, float3 minBB, float3 extent, float3 normal)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    float3 radiance;
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, (1.0f / 3.0f).xx);

        // Check if inside AABB
        float3 maxBB = minBB + extent;
        float3 extentCentre = extent * 0.5f.xxx;
        float3 centre = minBB + extentCentre;
        bool insideAABB = all(lightPosition >= minBB) && all(lightPosition <= maxBB);

        if (!insideAABB)
        {
            // Cull by visibility by checking if triangle is above plane
            if (dot(light.v0 - centre, normal) <= -0.7071f && dot(light.v1 - centre, normal) <= -0.7071f && dot(light.v2 - centre, normal) <= -0.7071f)
            {
                return 0.0f;
            }
        }

        float3 emissivity = light.emissivity.xyz;
#       ifdef THRESHOLD_RADIANCE
        // Quick cull based on range of sphere falloff
        float radiusSqr = dot(extentCentre, extentCentre);
        float radius = sqrt(radiusSqr);
        const float range = sqrt(max(emissivity.x, max(emissivity.y, emissivity.z)) / THRESHOLD_RADIANCE);
        float3 lightDirection = centre - lightPosition;
        if (length(lightDirection) > (radius + range))
        {
            return 0.0f;
        }
#       endif // THRESHOLD_RADIANCE

        uint emissivityTex = asuint(light.emissivity.w);
        if (emissivityTex != uint(-1))
        {
            float2 edgeUV0 = light.uv1 - light.uv0;
            float2 edgeUV1 = light.uv2 - light.uv0;
            // Get texture dimensions in order to determine LOD of visible solid angle
            float2 size;
            g_TextureMaps[NonUniformResourceIndex(emissivityTex)].GetDimensions(size.x, size.y);
            float areaUV = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
            float lod = 0.5f * log2(areaUV);

            float2 uv = interpolate(light.uv0, light.uv1, light.uv2, 0.3333333333333f.xx);
            emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, lod).xyz;
        }

        // Calculate lights surface normal vector
        float3 edge1 = light.v1 - light.v0;
        float3 edge2 = light.v2 - light.v0;
        float3 lightCross = cross(edge1, edge2);
        // Calculate surface area of triangle
        float lightNormalLength = length(lightCross);
        float3 lightNormal = lightCross / lightNormalLength.xxx;
        float lightArea = 0.5f * lightNormalLength;

#       ifdef LIGHT_SAMPLE_VOLUME_CENTROID
        // Evaluate radiance at cell centre
        float3 lightVector = centre - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        float pdf = saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr).xxx))) * lightArea;
        pdf = pdf / (lightLengthSqr + FLT_EPSILON);
        radiance = emissivity * pdf;
#       else // LIGHT_SAMPLE_VOLUME_CENTROID
        // Contribution is emission scaled by surface area converted to solid angle
        // The light is sampled at all 8 corners of the AABB and then interpolated to fill in the internal volume
        float3 lightVector = minBB - lightPosition;
        float lightLengthSqr = rcp(lengthSqr(lightVector));
        lightVector *= sqrt(lightLengthSqr).xxx;
        float pdf = saturate(abs(dot(lightNormal, lightVector))) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, minBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        lightVector *= sqrt(lightLengthSqr).xxx;
        pdf += saturate(abs(dot(lightNormal, lightVector))) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, maxBB.y, minBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        lightVector *= sqrt(lightLengthSqr).xxx;
        pdf += saturate(abs(dot(lightNormal, lightVector))) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, maxBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        lightVector *= sqrt(lightLengthSqr).xxx;
        pdf += saturate(abs(dot(lightNormal, lightVector))) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, minBB.y, minBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        lightVector *= sqrt(lightLengthSqr).xxx;
        pdf += saturate(abs(dot(lightNormal, lightVector))) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, minBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        lightVector *= sqrt(lightLengthSqr).xxx;
        pdf += saturate(abs(dot(lightNormal, lightVector))) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, maxBB.y, minBB.z) - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        lightVector *= sqrt(lightLengthSqr).xxx;
        pdf += saturate(abs(dot(lightNormal, lightVector))) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = maxBB - lightPosition;
        lightLengthSqr = rcp(lengthSqr(lightVector));
        lightVector *= sqrt(lightLengthSqr).xxx;
        pdf += saturate(abs(dot(lightNormal, lightVector))) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        radiance = (emissivity * (lightArea * 0.125f)) * pdf;
#       endif // LIGHT_SAMPLE_VOLUME_CENTROID
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Check if inside AABB
        float3 maxBB = minBB + extent;
        float3 extentCentre = extent * 0.5f.xxx;
        float3 centre = minBB + extentCentre;
        const bool insideAABB = all(light.position >= minBB) && all(light.position <= maxBB);

        // Cull by visibility by checking if triangle is above plane
        if (!insideAABB && dot(light.position - centre, normal) <= -0.7071f)
        {
            return 0.0f;
        }

        // Quick cull based on range of sphere
        float radiusSqr = dot(extentCentre, extentCentre);
        float radius = sqrt(radiusSqr);
        float3 lightDirection = centre - light.position;
        if (length(lightDirection) > (radius + light.range))
        {
            return 0.0f;
        }

        if (lightType == kLight_Spot)
        {
            // Check if spot cone intersects current cell
            // Uses fast cone-sphere test (Hale)
            bool intersect = false;
            float3 coneNormal = selectedLight.v2.xyz;
            float sinAngle = selectedLight.v2.w;
            float tanAngleSqPlusOne = selectedLight.v3.z;

            // Fast check to cull lights based on cell normal
            if (dot(coneNormal, normal) <= -0.7071f)
            {
                return 0.0f;
            }

            if (dot(lightDirection + (coneNormal * sinAngle * radius), coneNormal) < 0.0f)
            {
                float3 cd = sinAngle * lightDirection - coneNormal * radius;
                const float lenA = dot(cd, coneNormal);
                intersect = dot(cd, cd) <= lenA * lenA * tanAngleSqPlusOne;
            }
            else
            {
                intersect = dot(lightDirection, lightDirection) <= radiusSqr;
            }
            if (!intersect)
            {
                return 0.0f;
            }
        }

#       ifdef LIGHT_SAMPLE_VOLUME_CENTROID
        // Evaluate radiance at cell centre
        float dist = distance(light.position, centre);
        float distMod = dist / light.range;
        float rad = saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        radiance = light.intensity * rad.xxx;
#       else // LIGHT_SAMPLE_VOLUME_CENTROID
        // For each corner of the cell evaluate the radiance
        float recipRange = 1.0f / light.range;
        float3 lightVector = minBB - light.position;
        float lightLengthSqr = lengthSqr(lightVector);
        float dist = sqrt(lightLengthSqr);
        lightLengthSqr = rcp(lightLengthSqr);
        lightVector *= dist.xxx;
        float distMod = dist * recipRange;
        float pdf = saturate(1.0f - (distMod * distMod * distMod * distMod)) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightLengthSqr = lengthSqr(lightVector);
        dist = sqrt(lightLengthSqr);
        lightLengthSqr = rcp(lightLengthSqr);
        lightVector *= dist.xxx;
        distMod = dist * recipRange;
        pdf += saturate(1.0f - (distMod * distMod * distMod * distMod)) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, maxBB.y, minBB.z) - light.position;
        lightLengthSqr = lengthSqr(lightVector);
        dist = sqrt(lightLengthSqr);
        lightLengthSqr = rcp(lightLengthSqr);
        lightVector *= dist.xxx;
        distMod = dist * recipRange;
        pdf += saturate(1.0f - (distMod * distMod * distMod * distMod)) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, maxBB.y, maxBB.z) - light.position;
        lightLengthSqr = lengthSqr(lightVector);
        dist = sqrt(lightLengthSqr);
        lightLengthSqr = rcp(lightLengthSqr);
        lightVector *= dist.xxx;
        distMod = dist * recipRange;
        pdf += saturate(1.0f - (distMod * distMod * distMod * distMod)) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, minBB.y, minBB.z) - light.position;
        lightLengthSqr = lengthSqr(lightVector);
        dist = sqrt(lightLengthSqr);
        lightLengthSqr = rcp(lightLengthSqr);
        lightVector *= dist.xxx;
        distMod = dist * recipRange;
        pdf += saturate(1.0f - (distMod * distMod * distMod * distMod)) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, minBB.y, maxBB.z) - light.position;
        lightLengthSqr = lengthSqr(lightVector);
        dist = sqrt(lightLengthSqr);
        lightLengthSqr = rcp(lightLengthSqr);
        lightVector *= dist.xxx;
        distMod = dist * recipRange;
        pdf += saturate(1.0f - (distMod * distMod * distMod * distMod)) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, maxBB.y, minBB.z) - light.position;
        lightLengthSqr = lengthSqr(lightVector);
        dist = sqrt(lightLengthSqr);
        lightLengthSqr = rcp(lightLengthSqr);
        lightVector *= dist.xxx;
        distMod = dist * recipRange;
        pdf += saturate(1.0f - (distMod * distMod * distMod * distMod)) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = maxBB - light.position;
        lightLengthSqr = lengthSqr(lightVector);
        dist = sqrt(lightLengthSqr);
        lightLengthSqr = rcp(lightLengthSqr);
        lightVector *= dist.xxx;
        distMod = dist * recipRange;
        pdf += saturate(1.0f - (distMod * distMod * distMod * distMod)) * lightLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        radiance = light.intensity * 0.125f * pdf;
#       endif // LIGHT_SAMPLE_VOLUME_CENTROID
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Fast check to cull lights based on cell normal
        if (dot(light.direction, normal) <= -0.7071f)
        {
            return 0.0f;
        }

        // Directional light is constant at all points
        radiance = light.irradiance;
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to normal based sampling the directions straddle multiple cube faces
        radiance = 0.0f;
        float count = 0.0f;
        if (normal.z != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, normal.z), light.lods).xyz;
            ++count;
        }
        if (normal.y != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, normal.y, 0.0f), light.lods).xyz;
            ++count;
        }
        if (normal.x != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(normal.x, 0.0f, 0.0f), light.lods).xyz;
            ++count;
        }
        radiance *= FOUR_PI / count;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
    return luminance(radiance);
#endif
}

/**
 * Calculate the combined luminance(Y) of a light taken at a specific location.
 * @param selectedLight The light to sample.
 * @param position      Current position on surface.
 * @return The calculated combined luminance.
 */
float sampleLightPoint(Light selectedLight, float3 position)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    float3 radiance;
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, (1.0f / 3.0f).xx);

        float3 emissivity = light.emissivity.xyz;
        uint emissivityTex = asuint(light.emissivity.w);
        if (emissivityTex != uint(-1))
        {
            float2 edgeUV0 = light.uv1 - light.uv0;
            float2 edgeUV1 = light.uv2 - light.uv0;
            // Get texture dimensions in order to determine LOD of visible solid angle
            float2 size;
            g_TextureMaps[NonUniformResourceIndex(emissivityTex)].GetDimensions(size.x, size.y);
            float areaUV = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
            float lod = 0.5f * log2(areaUV);

            float2 uv = interpolate(light.uv0, light.uv1, light.uv2, 0.3333333333333f.xx);
            emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, lod).xyz;
        }

        // Calculate lights surface normal vector
        float3 edge1 = light.v1 - light.v0;
        float3 edge2 = light.v2 - light.v0;
        float3 lightCross = cross(edge1, edge2);
        // Calculate surface area of triangle
        float lightNormalLength = length(lightCross);
        float3 lightNormal = lightCross / lightNormalLength.xxx;
        float lightArea = 0.5f * lightNormalLength;

        // Evaluate radiance at specified point
        float3 lightVector = position - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(lightLengthSqr).xxx;
        float pdf = saturate(abs(dot(lightNormal, lightVector))) * lightArea;
        pdf = pdf / (lightLengthSqr + FLT_EPSILON);
        radiance = emissivity * pdf;
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Evaluate radiance at specified point
        float3 lightVector = light.position - position;
        float dist = length(lightVector);
        lightVector /= dist;
        float distMod = dist / light.range;
        float rad = saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        radiance = light.intensity * rad.xxx;
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        radiance = light.irradiance;
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to use of cube map all 6 sides must be individually sampled
        radiance = g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, 1.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, -1.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 1.0f, 0.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, -1.0f, 0.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(1.0f, 0.0f, 0.0f), light.lods).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(-1.0f, 0.0f, 0.0f), light.lods).xyz;
        radiance *= FOUR_PI / 6.0f;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
    return luminance(radiance);
#endif
}

/**
 * Calculate the combined luminance(Y) of a light taken at a specific location visible from a surface orientation.
 * @param selectedLight The light to sample.
 * @param position      Current position on surface.
 * @param normal        Shading normal vector at current position.
 * @return The calculated combined luminance.
 */
float sampleLightPointNormal(Light selectedLight, float3 position, float3 normal)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    float3 radiance;
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, (1.0f / 3.0f).xx);

        float3 emissivity = light.emissivity.xyz;
        uint emissivityTex = asuint(light.emissivity.w);
        if (emissivityTex != uint(-1))
        {
            float2 edgeUV0 = light.uv1 - light.uv0;
            float2 edgeUV1 = light.uv2 - light.uv0;
            // Get texture dimensions in order to determine LOD of visible solid angle
            float2 size;
            g_TextureMaps[NonUniformResourceIndex(emissivityTex)].GetDimensions(size.x, size.y);
            float areaUV = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
            float lod = 0.5f * log2(areaUV);

            float2 uv = interpolate(light.uv0, light.uv1, light.uv2, 0.3333333333333f.xx);
            emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, lod).xyz;
        }

        // Calculate lights surface normal vector
        float3 edge1 = light.v1 - light.v0;
        float3 edge2 = light.v2 - light.v0;
        float3 lightCross = cross(edge1, edge2);
        // Calculate surface area of triangle
        float lightNormalLength = length(lightCross);
        float3 lightNormal = lightCross / lightNormalLength.xxx;
        float lightArea = 0.5f * lightNormalLength;

        // Evaluate radiance at specified point
        float3 lightVector = position - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(lightLengthSqr).xxx;
        float pdf = saturate(abs(dot(lightNormal, lightVector))) * lightArea;
        pdf = pdf / (lightLengthSqr + FLT_EPSILON);
        radiance = emissivity * pdf * saturate(-dot(lightVector, normal));
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Evaluate radiance at specified point
        float3 lightVector = light.position - position;
        float dist = length(lightVector);
        lightVector /= dist;
        float distMod = dist / light.range;
        float rad = saturate(1.0f - (distMod * distMod * distMod * distMod)) / (dist * dist);
        radiance = light.intensity * rad.xxx * saturate(dot(lightVector, normal));
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        radiance = light.irradiance * saturate(dot(light.direction, normal));
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to normal based sampling the directions straddle multiple cube faces
        radiance = 0.0f;
        float count = 0.0f;
        if (normal.z != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, normal.z), light.lods).xyz;
            ++count;
        }
        if (normal.y != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, normal.y, 0.0f), light.lods).xyz;
            ++count;
        }
        if (normal.x != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(normal.x, 0.0f, 0.0f), light.lods).xyz;
            ++count;
        }
        radiance *= TWO_PI / count;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
    return luminance(radiance);
#endif
}


/**
 * Calculate a quick weighting based on the cosine light angle.
 * @param selectedLight The light to sample.
 * @param position      Current position on surface.
 * @param normal        Shading normal vector at current position.
 * @return The calculated angle weight.
 */
float sampleLightPointNormalFast(Light selectedLight, float3 position, float3 normal)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, (1.0f / 3.0f).xx);

        // Evaluate radiance at specified point
        float3 lightVector = normalize(lightPosition - position);
        return saturate(dot(lightVector, normal));
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Evaluate radiance at specified point
        float3 lightVector = light.position - position;
        return saturate(dot(lightVector, normal));
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        return saturate(dot(light.direction, normal));
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        return 1.0f;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
#endif
}

#endif // LIGHT_SAMPLING_VOLUME_HLSL
