/**********************************************************************
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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
#include "../math/geometry.hlsl"
#include "../math/color.hlsl"

/*
// Requires the following data to be defined in any shader that uses this file
TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);

SamplerState g_TextureSampler;

StructuredBuffer<Light> g_LightBuffer;
StructuredBuffer<uint> g_LightBufferSize;
*/

/**
 * Calculate the combined luminance(Y) of a light taken within a bounding box.
 * @param selectedLight The light to sample.
 * @param minBB         Bounding box minimum values.
 * @param maxBB         Bounding box maximum values.
 * @return The calculated combined luminance.
 */
float evaluateLightVolume(Light selectedLight, float3 minBB, float3 maxBB)
{
    float3 radiance;
#ifndef DISABLE_AREA_LIGHTS
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (selectedLight.get_light_type() == kLight_Area)
#   endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, 0.3333333333333f.xx);

        float3 emissivity = light.emissivity.xyz;
#ifdef THRESHOLD_RADIANCE
        // Quick cull based on range of sphere falloff
        float3 extent = (maxBB - minBB) * 0.5f.xxx;
        float3 center = minBB + extent;
        float radiusSqr = dot(extent, extent);
        float radius = sqrt(radiusSqr);
        const float range = sqrt(max(emissivity.x, max(emissivity.y, emissivity.z)) / THRESHOLD_RADIANCE);
        float3 lightDirection = center - lightPosition;
        if (length(lightDirection) > (radius + range))
        {
            return 0.0f;
        }
#endif

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
        float3 lightNormal = lightCross / lightNormalLength;
        float lightArea = 0.5f * lightNormalLength;

        // Contribution is emission scaled by surface area converted to solid angle
        float3 lightVector = minBB - lightPosition;
        float pdf = saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(minBB.x, minBB.y, maxBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(minBB.x, maxBB.y, minBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(minBB.x, maxBB.y, maxBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(maxBB.x, minBB.y, minBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(maxBB.x, minBB.y, maxBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(maxBB.x, maxBB.y, minBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = maxBB - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        radiance = (emissivity * (lightArea / 8.0f)) * pdf;
    }
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#   endif
#endif
#ifndef DISABLE_DELTA_LIGHTS
    if (selectedLight.get_light_type() == kLight_Point || selectedLight.get_light_type() == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Quick cull based on range of sphere
        float3 extent = (maxBB - minBB) * 0.5f.xxx;
        float3 center = minBB + extent;
        float radiusSqr = dot(extent, extent);
        float radius = sqrt(radiusSqr);
        float3 lightDirection = center - light.position;
        if (length(lightDirection) > (radius + light.range))
        {
            return 0.0f;
        }

        if (selectedLight.get_light_type() == kLight_Spot)
        {
            // Check if spot cone intersects current cell
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

        // For each corner of the cell evaluate the radiance
        float dist2 = distanceSqr(light.position, minBB);
        float rad = 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(minBB.x, minBB.y, maxBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(minBB.x, maxBB.y, minBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(minBB.x, maxBB.y, maxBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(maxBB.x, minBB.y, minBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(maxBB.x, minBB.y, maxBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(maxBB.x, maxBB.y, minBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, maxBB);
        radiance = light.intensity * (rad / 8.0f).xxx;
    }
    else
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (selectedLight.get_light_type() == kLight_Direction)
#   endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        radiance = light.irradiance;
    }
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#   endif
#endif
#ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*selectedLight.get_light_type() == kLight_Environment*/
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
        radiance *= FOUR_PI;
    }
#endif
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    return luminance(radiance);
#endif
}


/**
 * Calculate the combined luminance(Y) of a light taken within a bounding box.
 * @param selectedLight The light to sample.
 * @param minBB         Bounding box minimum values.
 * @param maxBB         Bounding box maximum values.
 * @param normal        The face normal of the bounding box region.
 * @return The calculated combined luminance.
 */
float evaluateLightVolumeNormal(Light selectedLight, float3 minBB, float3 maxBB, float3 normal)
{
    float3 radiance;
#ifndef DISABLE_AREA_LIGHTS
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (selectedLight.get_light_type() == kLight_Area)
#   endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

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
        float3 lightNormal = lightCross / lightNormalLength;
        float lightArea = 0.5f * lightNormalLength;

        // Contribution is emission scaled by surface area converted to solid angle
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, 0.3333333333333f.xx);
        float3 lightVector = minBB - lightPosition;
        float pdf = saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(minBB.x, minBB.y, maxBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(minBB.x, maxBB.y, minBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(minBB.x, maxBB.y, maxBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(maxBB.x, minBB.y, minBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(maxBB.x, minBB.y, maxBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = float3(maxBB.x, maxBB.y, minBB.z) - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        lightVector = maxBB - lightPosition;
        pdf += saturate(dot(lightNormal, normalize(lightVector))) / dot(lightVector, lightVector);
        radiance = (emissivity * (lightArea / 8.0f)) * pdf;
    }
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#   endif
#endif
#ifndef DISABLE_DELTA_LIGHTS
    if (selectedLight.get_light_type() == kLight_Point || selectedLight.get_light_type() == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Quick cull based on range of sphere
        float3 extent = (maxBB - minBB) * 0.5f.xxx;
        float3 center = minBB + extent;
        float radiusSqr = dot(extent, extent);
        float radius = sqrt(radiusSqr);
        float3 lightDirection = center - light.position;
        if (length(lightDirection) > (radius + light.range))
        {
            return 0.0f;
        }
        if (selectedLight.get_light_type() == kLight_Spot)
        {
            // Check if spot cone intersects current cell
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

        // For each corner of the cell evaluate the radiance
        float dist2 = distanceSqr(light.position, minBB);
        float rad = 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(minBB.x, minBB.y, maxBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(minBB.x, maxBB.y, minBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(minBB.x, maxBB.y, maxBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(maxBB.x, minBB.y, minBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(maxBB.x, minBB.y, maxBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, float3(maxBB.x, maxBB.y, minBB.z));
        rad += 1.0f / (dist2 + 1);
        dist2 = distanceSqr(light.position, maxBB);
        radiance = light.intensity * (rad / 8.0f).xxx;
    }
    else
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (selectedLight.get_light_type() == kLight_Direction)
#   endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Fast check to cull lights based on cell normal
        if (dot(light.direction, normal) < 0.0f)
        {
            return 0.0f;
        }

        // Directional light is constant at all points
        radiance = light.irradiance;
    }
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#   endif
#endif
#ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*selectedLight.get_light_type() == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to normal based sampling the directions straddle multiple cube faces
        radiance = 0.0f;
        if (normal.z > 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, normal.z), light.lods).xyz;
        }
        if (normal.y > 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, normal.y, 0.0f), light.lods).xyz;
        }
        if (normal.x > 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(normal.x, 0.0f, 0.0f), light.lods).xyz;
        }
        radiance *= TWO_PI;
    }
#endif
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    return luminance(radiance);
#endif
}

#endif // LIGHT_SAMPLING_HLSL
