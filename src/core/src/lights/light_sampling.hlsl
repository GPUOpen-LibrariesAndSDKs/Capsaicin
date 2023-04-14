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

#ifndef LIGHT_SAMPLING_HLSL
#define LIGHT_SAMPLING_HLSL

#include "light_evaluation.hlsl"
#include "../math/math_constants.hlsl"
#include "../math/geometry.hlsl"
#include "../math/sampling.hlsl"

/*
// Requires the following data to be defined in any shader that uses this file
TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);

SamplerState g_LinearSampler;

// + data inherited from stratified_sampler.hlsl
*/

/**
 * Sample the direction, PDF and position on a given area light.
 * @param light          The light to be sampled.
 * @param samples        Random number samples used to sample light.
 * @param position       The position on the surface currently being shaded
 * @param pdf            (Out) The PDF for the calculated sample (with respect to current light only).
 * @param lightPosition  (Out) The sampled position on the surface of the light.
 * @param barycentric    (Out) The sampled barycentric coordinates on the surface.
 * @return The direction to the sampled position on the light.
 */
float3 sampleAreaLight(LightArea light, float2 samples, float3 position, out float pdf, out float3 lightPosition, out float2 barycentric)
{
    // Uniformly sample a triangle
    float sqrtU = sqrt(samples.x);
    barycentric = float2(1.0f - sqrtU, samples.y * sqrtU);

    // Determine position on surface of light
    lightPosition = interpolate(light.v0, light.v1, light.v2, barycentric);

    // Calculate lights surface normal vector
    float3 edge1 = light.v1 - light.v0;
    float3 edge2 = light.v2 - light.v0;
    float3 lightCross = cross(edge1, edge2);
    float3 lightNormal = normalize(lightCross);
    // Calculate surface area of triangle
    float lightArea = 0.5f * length(lightCross);

    // Offset the light position to prevent incorrect intersections when casting the lights shadow ray
    lightPosition = offsetPosition(lightPosition, lightNormal);

    // Determine light direction
    float3 lightVector = lightPosition - position;
    float3 lightDirection = normalize(lightVector);

    // Calculate PDF as solid angle measurement
    // The PDF is simply (1/area) converted to PDF of solid angle by dividing by (n.l/d^2)
    pdf = saturate(dot(lightNormal, -lightDirection)) * lightArea;
    pdf = (pdf != 0.0f) ? dot(lightVector, lightVector) / pdf : 0.0f;

    return lightDirection;
}

/**
 * Calculate the PDF of sampling a given area light.
 * @param light           The light that was sampled.
 * @param shadingPosition The position on the surface currently being shaded
 * @param lightPosition   The position on the surface of the light.
 * @return The calculated PDF with respect to the light.
 */
float sampleAreaLightPDF(LightArea light, float3 shadingPosition, float3 lightPosition)
{
    // Calculate lights surface normal vector
    float3 edge1 = light.v1 - light.v0;
    float3 edge2 = light.v2 - light.v0;
    float3 lightCross = cross(edge1, edge2);
    float3 lightNormal = normalize(lightCross);
    // Calculate surface area of triangle
    float lightArea = 0.5f * length(lightCross);

    // Evaluate PDF for current position and direction
    float3 lightVector = shadingPosition - lightPosition;
    float3 lightDirection = normalize(lightVector);
    float pdf = saturate(dot(lightNormal, lightDirection)) * lightArea;
    pdf = (pdf != 0.0f) ? dot(lightVector, lightVector) / pdf : 0.0f;
    return pdf;
}

/**
 * Sample the direction, PDF and position for a environment light.
 * @param light   The light to be sampled.
 * @param samples Random number samples used to sample light.
 * @param normal  Shading normal vector at current position.
 * @param pdf     (Out) The PDF for the calculated sample.
 * @return The direction to the sampled position on the light.
 */
float3 sampleEnvironmentLight(LightEnvironment light, float2 samples, float3 normal, out float pdf)
{
    // Currently just uses a uniform spherical sample
    // TODO improve sampling based on image contents

    // Sample uniform sphere
    float z = 1.0f - 2.0f * samples.x;
    float r = sqrt(1.0f - (z * z));
    float phi = 2.0f * PI * samples.y;
    float3 direction = float3(r * cos(phi), r * sin(phi), z);

    // Flip direction to ensure it is in the hemisphere oriented around the normal
    direction = dot(direction, normal) < 0.0f ? -direction : direction;

    // Set PDF
    pdf = INV_TWO_PI;

    return direction;
}

/**
 * Sample the direction, PDF and position for a environment light.
 * @param light   The light to be sampled.
 * @param samples Random number samples used to sample light.
 * @param normal  Shading normal vector at current position.
 * @param pdf     (Out) The PDF for the calculated sample.
 * @return The direction to the sampled position on the light.
 */
float3 sampleEnvironmentLight(LightEnvironment light, float2 samples, out float pdf)
{
    // Sample uniform sphere
    float z = 1.0f - 2.0f * samples.x;
    float r = sqrt(1.0f - (z * z));
    float phi = 2.0f * PI * samples.y;
    float3 direction = float3(r * cos(phi), r * sin(phi), z);

    // Set PDF
    pdf = INV_FOUR_PI;

    return direction;
}

/**
 * Calculate the PDF of sampling an environment light.
 * @param light          The light to be sampled.
 * @param lightDirection The direction from the shadingPosition to the lightPosition.
 * @param normal         Shading normal vector at current position.
 * @return The calculated PDF with respect to the light.
 */
float sampleEnvironmentLightPDF(LightEnvironment light, float3 lightDirection, float3 normal)
{
    // Currently just uniformly samples a hemisphere
    float pdf = INV_TWO_PI;
    return pdf;
}

/**
 * Calculate the PDF of sampling an environment light.
 * @param light          The light to be sampled.
 * @param lightDirection The direction from the shadingPosition to the lightPosition.
 * @return The calculated PDF with respect to the light.
 */
float sampleEnvironmentLightPDF(LightEnvironment light, float3 lightDirection)
{
    // Currently just uniformly samples a sphere when no normal is provided
    float pdf = INV_FOUR_PI;
    return pdf;
}

/**
 * Sample the direction, PDF and position for a spot light.
 * @param light         The light to be sampled.
 * @param position      The current surface position.
 * @param pdf           (Out) The PDF for the calculated sample.
 * @param lightPosition (Out) The sampled position on the surface of the light.
 * @return The direction to the sampled light.
 */
float3 samplePointLight(LightPoint light, float3 position, out float pdf, out float3 lightPosition)
{
    // Calculate direction to the light
    float3 direction = light.position - position;
    float directionLength = length(direction);
    direction = direction / directionLength;
    // PDF is a constant as there is only 1 possible direction to the light.
    // The PDF is either 1 or 0 depending on if the light is within the specified range.
    pdf = (directionLength <= light.range) ? 1.0f : 0.0f;
    // Set light position
    lightPosition = light.position;
    return direction;
}

/**
 * Sample the direction, PDF and position for a spot light.
 * @param light         The light to be sampled.
 * @param position      The current surface position.
 * @param pdf           (Out) The PDF for the calculated sample.
 * @param lightPosition (Out) The sampled position on the surface of the light.
 * @return The direction to the sampled light.
 */
float3 sampleSpotLight(LightSpot light, float3 position, out float pdf, out float3 lightPosition)
{
    // Calculate direction to the light
    float3 direction = light.position - position;
    float directionLength = length(direction);
    direction = direction / directionLength;
    // PDF is a constant as there is only 1 possible direction to the light.
    // The PDF is either 1 or 0 depending on if the light is within the specified range.
    pdf = (directionLength <= light.range) ? 1.0f : 0.0f;
    // Set light position
    lightPosition = light.position;
    return direction;
}

/**
 * Sample the direction and PDF for a directional light.
 * @param light The light to be sampled.
 * @param pdf   (Out) The PDF for the calculated sample.
 * @return The direction to the sampled light.
 */
float3 sampleDirectionalLight(LightDirectional light, out float pdf)
{
    // PDF is a constant as there is only 1 possible direction to the light
    pdf = 1.0f;
    return light.direction;
}

/**
 * Sample the direction and PDF for a specified light.
 * @tparam RNG The type of random number sampler to be used.
 * @param selectedLight  The light to be sampled.
 * @param random         Random number sampler used to sample light.
 * @param position       Current position on surface.
 * @param normal         Shading normal vector at current position.
 * @param lightDirection (Out) The sampled direction to the light.
 * @param lightPDF       (Out) The PDF for the calculated sample.
 * @param lightPosition  (Out) The position of the light sample (only valid if sampled light has a position).
 * @param sampleParams   (Out) UV values that can be used to recalculate light parameters using @sampledLightUnpack.
 * @return The radiance returned from sampled light direction.
 */
template<typename RNG>
float3 sampleLight(Light selectedLight, inout RNG random, float3 position, float3 normal, out float3 lightDirection, out float lightPDF, out float3 lightPosition, out float2 sampleParams)
{
    float3 radiance;
#if !defined(DISABLE_AREA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    float2 randomValues = random.rand2();
#endif
#ifndef DISABLE_AREA_LIGHTS
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (selectedLight.get_light_type() == kLight_Area)
#   endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Sample the selected area light
        lightDirection = sampleAreaLight(light, randomValues, position, lightPDF, lightPosition, sampleParams);

        // Early discard back facing area lights
        //if (lightPDF == 0.0f)
        //{
        //    return 0.0f.xxx;
        //}

        radiance = evaluateAreaLight(light, sampleParams);
    }
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#   endif
#endif
#ifndef DISABLE_DELTA_LIGHTS
    if (selectedLight.get_light_type() == kLight_Point)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Sample the selected point light
        lightDirection = samplePointLight(light, position, lightPDF, lightPosition);

        radiance = evaluatePointLight(light, position);
    } else if (selectedLight.get_light_type() == kLight_Spot) {
        // Get the spot light
        LightSpot light = MakeLightSpot(selectedLight);

        // Sample the selected spot light
        lightDirection = sampleSpotLight(light, position, lightPDF, lightPosition);

        radiance = evaluateSpotLight(light, position);
    }
    else
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (selectedLight.get_light_type() == kLight_Direction)
#   endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Sample the selected directional light
        lightDirection = sampleDirectionalLight(light, lightPDF);

        radiance = evaluateDirectionalLight(light);
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

        // Sample the environment map
        lightDirection = sampleEnvironmentLight(light, randomValues, normal, lightPDF);

        radiance = evaluateEnvironmentLight(light, lightDirection);

        // Pack light direction into storable UV
        sampleParams = MapToSphereInverse(lightDirection);
    }
#endif
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    lightDirection = 0.0f.xxx;
    lightPDF = 0.0f;
    sampleParams = 0.0f.xx;
    return 0.0f.xxx;
#else
    // Discard lights behind surface
    if (dot(lightDirection, normal) < 0.0f)
    {
        lightPDF = 0.0f;
    }
    return radiance;
#endif
}

/**
 * Sample the direction and PDF for a specified light within a ray cone.
 * @tparam RNG The type of random number sampler to be used.
 * @param selectedLight  The light to be sampled.
 * @param random         Random number sampler used to sample light.
 * @param position       Current position on surface.
 * @param normal         Shading normal vector at current position.
 * @param solidAngle     Solid angle around view direction of visible ray cone.
 * @param lightDirection (Out) The sampled direction to the light.
 * @param lightPDF       (Out) The PDF for the calculated sample.
 * @param lightPosition  (Out) The position of the light sample (only valid if sampled light has a position).
 * @param sampleParams   (Out) UV values that can be used to recalculate light parameters using @sampledLightUnpack.
 * @return The radiance returned from sampled light direction.
 */
template<typename RNG>
float3 sampleLightCone(Light selectedLight, inout RNG random, float3 position, float3 normal, float solidAngle, out float3 lightDirection, out float lightPDF, out float3 lightPosition, out float2 sampleParams)
{
    float3 radiance;
#if !defined(DISABLE_AREA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    float2 randomValues = random.rand2();
#endif
#ifndef DISABLE_AREA_LIGHTS
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (selectedLight.get_light_type() == kLight_Area)
#   endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Sample the selected area light
        lightDirection = sampleAreaLight(light, randomValues, position, lightPDF, lightPosition, sampleParams);

        radiance = evaluateAreaLightCone(light, sampleParams, position, solidAngle);
    }
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#   endif
#endif
#ifndef DISABLE_DELTA_LIGHTS
    if (selectedLight.get_light_type() == kLight_Point)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Sample the selected point light
        lightDirection = samplePointLight(light, position, lightPDF, lightPosition);

        radiance = evaluatePointLight(light, position);
    } else if (selectedLight.get_light_type() == kLight_Spot) {
        // Get the spot light
        LightSpot light = MakeLightSpot(selectedLight);

        // Sample the selected spot light
        lightDirection = sampleSpotLight(light, position, lightPDF, lightPosition);

        radiance = evaluateSpotLight(light, position);
    }
    else
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (selectedLight.get_light_type() == kLight_Direction)
#   endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Sample the selected directional light
        lightDirection = sampleDirectionalLight(light, lightPDF);

        radiance = evaluateDirectionalLight(light);
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

        // Sample the environment map
        lightDirection = sampleEnvironmentLight(light, randomValues, normal, lightPDF);

        radiance = evaluateEnvironmentLightCone(light, lightDirection, solidAngle);

        // Pack light direction into storable UV
        sampleParams = MapToSphereInverse(lightDirection);
    }
#endif
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    lightDirection = 0.0f.xxx;
    lightPDF = 0.0f;
    sampleParams = 0.0f.xx;
    return 0.0f.xxx;
#else
    // Discard lights behind surface
    if (dot(lightDirection, normal) < 0.0f)
    {
        lightPDF = 0.0f;
    }
    if (lightPDF == 0.0f)
    {
        return 0.0f.xxx;
    }
    return radiance;
#endif
}

/**
 * Sample the direction and PDF for a specified light.
 * @tparam RNG The type of random number sampler to be used.
 * @param selectedLight  The light to be sampled.
 * @param random         Random number sampler used to sample light.
 * @param position       Current position on surface.
 * @param solidAngle     Solid angle around view direction of visible ray cone.
 * @param lightDirection (Out) The sampled direction to the light.
 * @param lightPDF       (Out) The PDF for the calculated sample.
 * @param lightPosition  (Out) The position of the light sample (only valid if sampled light has a position).
 * @param sampleParams   (Out) UV values that can be used to recalculate light parameters using @sampledLightUnpack.
 * @return The radiance returned from sampled light direction.
 */
template<typename RNG>
float3 sampleLightConeUnorm(Light selectedLight, inout RNG random, float3 position, float solidAngle, out float3 lightDirection, out float lightPDF, out float3 lightPosition, out float2 sampleParams)
{
    float3 radiance;
#if !defined(DISABLE_AREA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    float2 randomValues = random.rand2();
#endif
#ifndef DISABLE_AREA_LIGHTS
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (selectedLight.get_light_type() == kLight_Area)
#   endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Sample the selected area light
        lightDirection = sampleAreaLight(light, randomValues, position, lightPDF, lightPosition, sampleParams);
        //TODO: lightPDF is incorrect when sampling over a solid_angle

        radiance = evaluateAreaLightCone(light, sampleParams, position, solidAngle);
    }
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#   endif
#endif
#ifndef DISABLE_DELTA_LIGHTS
    if (selectedLight.get_light_type() == kLight_Point)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Sample the selected point light
        lightDirection = samplePointLight(light, position, lightPDF, lightPosition);

        radiance = evaluatePointLight(light, position);
    } else if (selectedLight.get_light_type() == kLight_Spot) {
        // Get the spot light
        LightSpot light = MakeLightSpot(selectedLight);

        // Sample the selected spot light
        lightDirection = sampleSpotLight(light, position, lightPDF, lightPosition);

        radiance = evaluateSpotLight(light, position);
    }
    else
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (selectedLight.get_light_type() == kLight_Direction)
#   endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Sample the selected directional light
        lightDirection = sampleDirectionalLight(light, lightPDF);

        radiance = evaluateDirectionalLight(light);
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

        // Sample the environment map
        lightDirection = sampleEnvironmentLight(light, randomValues, lightPDF);

        radiance = evaluateEnvironmentLightCone(light, lightDirection, solidAngle);

        // Pack light direction into storable UV
        sampleParams = MapToSphereInverse(lightDirection);
    }
#endif
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    lightDirection = 0.0f.xxx;
    lightPDF = 0.0f;
    sampleParams = 0.0f.xx;
    return 0.0f.xxx;
#else
    if (lightPDF == 0.0f)
    {
        return 0.0f.xxx;
    }
    return radiance;
#endif
}

/**
 * Sample the direction and PDF for a specified light.
 * @tparam RNG The type of random number sampler to be used.
 * @param selectedLight  The light to be sampled.
 * @param random         Random number sampler used to sample light.
 * @param position       Current position on surface.
 * @param lightDirection (Out) The sampled direction to the light.
 * @param lightPDF       (Out) The PDF for the calculated sample.
 * @param lightPosition  (Out) The position of the light sample (only valid if sampled light has a position).
 * @param sampleParams   (Out) UV values that can be used to recalculate light parameters using @sampledLightUnpack.
 * @return The radiance returned from sampled light direction.
 */
template<typename RNG>
float3 sampleLightUnorm(Light selectedLight, inout RNG random, float3 position, out float3 lightDirection, out float lightPDF, out float3 lightPosition, out float2 sampleParams)
{
    float3 radiance;
#if !defined(DISABLE_AREA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    float2 randomValues = random.rand2();
#endif
#ifndef DISABLE_AREA_LIGHTS
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (selectedLight.get_light_type() == kLight_Area)
#   endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Sample the selected area light
        lightDirection = sampleAreaLight(light, randomValues, position, lightPDF, lightPosition, sampleParams);

        radiance = evaluateAreaLight(light, sampleParams);
    }
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#   endif
#endif
#ifndef DISABLE_DELTA_LIGHTS
    if (selectedLight.get_light_type() == kLight_Point)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Sample the selected point light
        lightDirection = samplePointLight(light, position, lightPDF, lightPosition);

        radiance = evaluatePointLight(light, position);
    } else if (selectedLight.get_light_type() == kLight_Spot) {
        // Get the spot light
        LightSpot light = MakeLightSpot(selectedLight);

        // Sample the selected spot light
        lightDirection = sampleSpotLight(light, position, lightPDF, lightPosition);

        radiance = evaluateSpotLight(light, position);
    }
    else
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (selectedLight.get_light_type() == kLight_Direction)
#   endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Sample the selected directional light
        lightDirection = sampleDirectionalLight(light, lightPDF);

        radiance = evaluateDirectionalLight(light);
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

        // Sample the environment map
        lightDirection = sampleEnvironmentLight(light, randomValues, lightPDF);

        radiance = evaluateEnvironmentLight(light, lightDirection);

        // Pack light direction into storable UV
        sampleParams = MapToSphereInverse(lightDirection);
    }
#endif
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    lightDirection = 0.0f.xxx;
    lightPDF = 0.0f;
    sampleParams = 0.0f.xx;
    return 0.0f.xxx;
#else
    return radiance;
#endif
}

/**
 * Get the direction to a sampled light using a uv value returned from @sampleLightSampled.
 * @param light        The light that was sampled.
 * @param sampleParams UV values returned from @sampleLightSampled.
 * @param position     Current position on surface to get direction from.
 * @return The updated light direction.
 */
float3 sampledLightUnpack(Light light, float2 sampleParams, float3 position)
{
#ifndef DISABLE_AREA_LIGHTS
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (light.get_light_type() == kLight_Area)
#   endif
    {
        // Calculate direction
        float3 lightPosition = interpolate(light.v1.xyz, light.v2.xyz, light.v3.xyz, sampleParams);
        return normalize(lightPosition - position);
    }
#   if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#   endif
#endif
#ifndef DISABLE_DELTA_LIGHTS
    if (light.get_light_type() < kLight_Direction) /* Faster check for point or spot light */
    {
        // Calculate direction
        return normalize(light.v1.xyz - position);
    } else
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (light.get_light_type() == kLight_Direction)
#   endif
    {
        return light.v2.xyz;
    }
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#   endif
#endif
#ifndef DISABLE_ENVIRONMENT_LIGHTS
    {
        // Convert stored uv back to direction
        return MapToSphere(sampleParams);
    }
#endif
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f.xxx;
#endif
}

#endif // LIGHT_SAMPLING_HLSL
