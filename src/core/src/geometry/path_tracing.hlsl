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

#ifndef PATH_TRACING_HLSL
#define PATH_TRACING_HLSL

#include "path_tracing_shared.h"

#include "../components/stratified_sampler/stratified_sampler.hlsl"
#include "../geometry/intersection.hlsl"
#include "../geometry/mis.hlsl"
#include "../geometry/ray_intersection.hlsl"
#include "../materials/material_sampling.hlsl"
#include "../math/transform.hlsl"
#include "../math/random.hlsl"

#ifndef USE_INLINE_RT
#define USE_INLINE_RT 1
#endif

/**
 * The default payload for shading functions is just a standard float3 radiance.
 * However if USE_CUSTOM_HIT_FUNCTIONS is defined by any code including this header then instead
 * the payload will be the user supplied CustomPayLoad struct. It is also expected that if defined the
 * user must also provide shadePathMissCustom, shadePathHitCustom and shadeLightHit functions to
 * be used in place of the defaults.
 */
#ifdef USE_CUSTOM_HIT_FUNCTIONS
typedef CustomPayLoad pathPayload;
#else
typedef float3 pathPayload;
#endif

struct ShadowRayPayload
{
    bool visible;
};

struct PathData
{
    LightSampler lightSampler;    /**< Sampler used for lighting (essentially just wraps a random number generator) */
    StratifiedSampler randomStratified; /**< Stratified random number generator instance */
    float3 throughput;  /**< Accumulated ray throughput for current path segment */
    pathPayload radiance; /**< Accumulated radiance for the current path segment */
    float samplePDF;    /**< The PDF of the last sampled BRDF */
    float3 normal;      /**< The surface normal at the location the current path originated from */
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    float3 sampleReflectance;  /**< The evaluated material at the point at the start of the path */
#endif
    uint bounce;        /**< Bounce depth of current path segment */
    float3 origin;      /**< Return value for new path segment start location */
    float3 direction;   /**< Return value for new path segment direction */
    bool terminated;    /**< Return value to indicated current paths terminates */
};

/**
 * Calculate any radiance from a missed path segment.
 * @param ray               The traced ray that missed any surfaces.
 * @param currentBounce     The current number of bounces along path for current segment.
 * @param lightSampler      Light sampler.
 * @param normal            Shading normal vector at start of path segment.
 * @param samplePDF         The PDF of sampling the current paths direction.
 * @param throughput        The current paths combined throughput.
 * @param [in,out] radiance The combined radiance. Any new radiance is added to the existing value and returned.
 */
void shadePathMiss(RayDesc ray, uint currentBounce, inout LightSampler lightSampler, float3 normal, float samplePDF,
    float3 throughput,
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    float3 sampleReflectance,
#endif
    inout float3 radiance)
{
#if !defined(DISABLE_NON_NEE) && !defined(DISABLE_ENVIRONMENT_LIGHTS)
#   ifdef DISABLE_DIRECT_LIGHTING
    if (currentBounce == 1) return;
#   endif // DISABLE_DIRECT_LIGHTING
    if (hasEnvironmentLight())
    {
        // If nothing was hit then load the environment map
        LightEnvironment light = getEnvironmentLight();
        float3 lightRadiance = evaluateEnvironmentLight(light, ray.Direction);
        if (currentBounce != 0)
        {
            // Add lighting contribution
#   ifndef DISABLE_NEE
            // Account for light contribution along sampled direction
            float lightPDF = sampleEnvironmentLightPDF(light, ray.Direction, float3(0.0f.xxx));
            lightPDF *= lightSampler.sampleLightPDF(0, ray.Origin, normal);
#       ifdef ENABLE_NEE_RESERVOIR_SAMPLING
            float weight = luminance(sampleReflectance * lightRadiance); // This must match Reservoir_EvaluateTargetPdf
            lightPDF = (weight != 0.0f)? lightPDF / weight : 0.0f;
#       endif
            if (lightPDF != 0.0f)
            {
                float weight = heuristicMIS(samplePDF, lightPDF);
                radiance += throughput * lightRadiance * weight.xxx;
            }
#   else  // !DISABLE_NON_NEE
            radiance += throughput * lightRadiance;
#   endif // !DISABLE_NON_NEE
        }
        else
        {
            radiance += throughput * lightRadiance;
        }
    }
#endif // !DISABLE_NON_NEE && !DISABLE_ENVIRONMENT_LIGHTS
}

/**
 * Calculate any radiance from a hit path segment.
 * @param ray               The traced ray that hit a surface.
 * @param hitData           Data associated with the hit surface.
 * @param iData             Retrieved data associated with the hit surface.
 * @param lightSampler      Light sampler.
 * @param currentBounce     The current number of bounces along path for current segment.
 * @param normal            Shading normal vector at start of path segment (Only valid if bounce > 0).
 * @param samplePDF         The PDF of sampling the current paths direction.
 * @param throughput        The current paths combined throughput.
 * @param [in,out] radiance The combined radiance. Any new radiance is added to the existing value and returned.
 */
void shadePathHit(RayDesc ray, HitInfo hitData, IntersectData iData, inout LightSampler lightSampler,
    uint currentBounce, float3 normal, float samplePDF, float3 throughput,
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    float3 sampleReflectance,
#endif
    inout float3 radiance)
{
#if !defined(DISABLE_NON_NEE) && !defined(DISABLE_AREA_LIGHTS)
#   ifdef DISABLE_DIRECT_LIGHTING
    if (currentBounce == 1) {/*ignore emissive hit*/} else {
#   endif // DISABLE_DIRECT_LIGHTING

    // Get material emissive values
    MaterialEmissive materialEmissive = MakeMaterialEmissive(iData.material, iData.uv);
    if (any(materialEmissive.emissive > 0.0f))
    {
        // Get light contribution
        float3 lightRadiance = materialEmissive.emissive;
        if (currentBounce != 0)
        {
            // Account for light contribution along sampled direction
#   ifndef DISABLE_NEE
            // Get material properties at intersection
            LightArea emissiveLight = MakeLightArea(iData.vertex0, iData.vertex1, iData.vertex2,
                // The following parameters are irrelevant for calculating PDF
                0.0f.xxxx, 0.0f, 0.0f, 0.0f);

            float lightPDF = sampleAreaLightPDF(emissiveLight, ray.Origin, iData.position);
            lightPDF *= lightSampler.sampleLightPDF(getAreaLightIndex(hitData.instanceIndex, hitData.primitiveIndex), ray.Origin, normal);
#   ifdef ENABLE_NEE_RESERVOIR_SAMPLING
            float3 newRadiance = evaluateAreaLight(emissiveLight, iData.barycentrics);
            float weight = luminance(sampleReflectance * newRadiance); // This must match Reservoir_EvaluateTargetPdf
            lightPDF = (weight != 0.0f)? lightPDF / weight : 0.0f;
#   endif
            if (lightPDF != 0.0f)
            {
                float weight = heuristicMIS(samplePDF, lightPDF);
                radiance += throughput * lightRadiance * weight.xxx;
            }
#else  // !DISABLE_NON_NEE
            radiance += throughput * lightRadiance;
#endif // !DISABLE_NON_NEE
        }
        else
        {
            radiance += throughput * lightRadiance;
        }
    }
#   ifdef DISABLE_DIRECT_LIGHTING
    }
#   endif // DISABLE_DIRECT_LIGHTING
#endif // !DISABLE_NON_NEE && !DISABLE_AREA_LIGHTS
}

/**
 * Calculate any radiance from a hit path segment.
 * @param ray               The traced ray that hit a surface.
 * @param material          Material data describing BRDF of surface.
 * @param normal            Shading normal vector at current position.
 * @param viewDirection     Outgoing ray view direction.
 * @param throughput        The current paths combined throughput.
 * @param lightPDF          The PDF of sampling the returned light direction.
 * @param radianceLi        The radiance visible along sampled light.
 * @param selectedLight     The light that was selected for sampling.
 * @param [in,out] radiance The combined radiance. Any new radiance is added to the existing value and returned.
 */
void shadeLightHit(RayDesc ray, MaterialBRDF material, float3 normal, float3 viewDirection, float3 throughput,
    float lightPDF, float3 radianceLi, Light selectedLight, inout float3 radiance)
{
#ifdef DISABLE_NON_NEE
    float3 sampleReflectance = evaluateBRDF(material, normal, viewDirection, ray.Direction);
    radiance += throughput * sampleReflectance * radianceLi / lightPDF.xxx;
#else
    // Evaluate BRDF for new light direction and calculate combined PDF for current sample
    float3 sampleReflectance;
    float samplePDF = sampleBRDFPDFAndEvalute(material, normal, viewDirection, ray.Direction, sampleReflectance);
    if (samplePDF != 0.0f)
    {
        bool deltaLight = isDeltaLight(selectedLight);
        float weight = (!deltaLight) ? heuristicMIS(lightPDF, samplePDF) : 1.0f;
        radiance += throughput * sampleReflectance * radianceLi * (weight / lightPDF).xxx;
    }
#endif // DISABLE_NON_NEE
}

/**
 * Calculates a new light ray direction from a surface by sampling the scenes lighting.
 * @tparam RNG The type of random number sampler to be used.
 * @param material         Material data describing BRDF of surface.
 * @param randomStratified Random number sampler used to sample light.
 * @param lightSampler     Light sampler.
 * @param position         Current position on surface.
 * @param normal           Shading normal vector at current position.
 * @param geometryNormal   Surface normal vector at current position.
 * @param viewDirection    Outgoing ray view direction.
 * @param ray              (Out) The ray containing the new light ray parameters.
 * @param lightPDF         (Out) The PDF of sampling the returned light direction.
 * @param radianceLi       (Out) The radiance visible along sampled light.
 * @param selectedLight    (Out) The light that was selected for sampling.
 * @return True if light path was generated, False if no ray returned.
 */
bool sampleLightsNEEDirection(MaterialBRDF material, inout StratifiedSampler randomStratified, LightSampler lightSampler,
    float3 position, float3 normal, float3 geometryNormal, float3 viewDirection, out RayDesc ray, out float lightPDF, out float3 radianceLi, out Light selectedLight)
{
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    Reservoir res = lightSampler.sampleLightList<8>(position, normal, viewDirection, material);
    uint lightIndex = res.lightSample.index;
    lightPDF = (res.W != 0.0f) ? rcp(res.W) : 0.0f; // Need to invert so it can be used in MIS
#else
    uint lightIndex = lightSampler.sampleLights(position, normal, lightPDF);
#endif

    if (lightPDF == 0.0f)
    {
        return false;
    }

    // Initialise returned radiance
    float3 lightPosition;
    float3 lightDirection;
    selectedLight = getLight(lightIndex);
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    radianceLi = evaluateLightSampled(selectedLight, position, res.lightSample.sampleParams, lightDirection, lightPosition);
#else
    float sampledLightPDF;
    float2 unused;
    radianceLi = sampleLight(selectedLight, randomStratified, position, normal, lightDirection, sampledLightPDF, lightPosition, unused);

    // Combine PDFs
    lightPDF *= sampledLightPDF;
#endif

    // Early discard lights behind surface
    if (dot(lightDirection, geometryNormal) < 0.0f || dot(lightDirection, normal) < 0.0f || lightPDF == 0.0f)
    {
        return false;
    }

    // Create shadow ray
    ray.Origin = position;
    ray.Direction = lightDirection;
    ray.TMin = 0.0f;
    ray.TMax = hasLightPosition(selectedLight) ? length(lightPosition - position) : FLT_MAX;
    return true;
}

/**
 * Calculates radiance from a new light ray direction from a surface by sampling the scenes lighting.
 * @tparam RNG The type of random number sampler to be used.
 * @param material          Material data describing BRDF of surface.
 * @param randomStratified  Random number sampler used to sample light.
 * @param lightSampler      Light sampler.
 * @param position          Current position on surface.
 * @param normal            Shading normal vector at current position.
 * @param geometryNormal    Surface normal vector at current position.
 * @param viewDirection     Outgoing ray view direction.
 * @param throughput        The current paths combined throughput.
 * @param [in,out] radiance The combined radiance. Any new radiance is added to the existing value and returned.
 */
void sampleLightsNEE(MaterialBRDF material, inout StratifiedSampler randomStratified, LightSampler lightSampler,
    float3 position, float3 normal, float3 geometryNormal, float3 viewDirection, float3 throughput, inout pathPayload radiance)
{
    // Get sampled light direction
    float lightPDF;
    RayDesc ray;
    float3 radianceLi;
    Light selectedLight;
    if (!sampleLightsNEEDirection(material, randomStratified, lightSampler, position, normal, geometryNormal, viewDirection, ray, lightPDF, radianceLi, selectedLight))
    {
        return;
    }

    // Trace shadow ray
#if USE_INLINE_RT
    ShadowRayQuery rayShadowQuery = TraceRay<ShadowRayQuery>(ray);
    bool hit = rayShadowQuery.CommittedStatus() == COMMITTED_NOTHING;
#else
    ShadowRayPayload payload = {false};
    TraceRay(g_Scene, SHADOW_RAY_FLAGS, 0xFFu, 1, 0, 1, ray, payload);
    bool hit = payload.visible;
#endif

    // If nothing was hit then we have hit the light
    if (hit)
    {
        // Add lighting contribution
#ifdef USE_CUSTOM_HIT_FUNCTIONS
        shadeLightHitCustom(ray, material, normal, viewDirection, throughput, lightPDF, radianceLi, selectedLight, radiance);
#else
        shadeLightHit(ray, material, normal, viewDirection, throughput, lightPDF, radianceLi, selectedLight, radiance);
#endif
    }
}

/**
 * Calculate the next segment along a path after a valid surface hit.
 * @param materialBRDF        The material on the hit surface.
 * @param randomStratified    Random number sampler used for sampling.
 * @param lightSampler        Light sampler.
 * @param currentBounce       The current number of bounces along path for current segment.
 * @param minBounces          The minimum number of allowed bounces along path segment before termination.
 * @param maxBounces          The maximum number of allowed bounces along path segment.
 * @param normal              Shading normal vector at current position.
 * @param geometryNormal      Surface normal vector at current position.
 * @param viewDirection       Outgoing ray view direction.
 * @param [in,out] throughput Combined throughput for current path.
 * @param [out] rayDirection  New outgoing path segment direction.
 * @param [out] samplePDF     The PDF of sampling the new paths direction.
 * @return True if path has new segment, False if path should be terminated.
 */
bool pathNext(MaterialBRDF materialBRDF, inout StratifiedSampler randomStratified,
    inout LightSampler lightSampler, uint currentBounce, uint minBounces, uint maxBounces, float3 normal,
    float3 geometryNormal, float3 viewDirection, inout float3 throughput, out float3 rayDirection, out float samplePDF
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    , out float3 sampleReflectance
#endif
)
{
    // Sample BRDF to get next ray direction
#ifndef ENABLE_NEE_RESERVOIR_SAMPLING
    float3 sampleReflectance;
#endif
    rayDirection = sampleBRDF(materialBRDF, randomStratified, normal, viewDirection, sampleReflectance, samplePDF);

    // Prevent tracing directions below the surface
    if (dot(geometryNormal, rayDirection) <= 0.0f || samplePDF == 0.0f)
    {
        return false;
    }

    // Add sampling weight to current weight
    throughput *= sampleReflectance / samplePDF.xxx;

    // Russian Roulette early termination
    if (currentBounce > minBounces)
    {
        float rrSample = hmax(throughput);
        if (rrSample <= lightSampler.randomNG.rand())
        {
            return false;
        }
        throughput /= rrSample.xxx;
    }
    return true;
}

/**
 * Handle case when a traced ray hits a surface.
 * @param ray                 The traced ray that hit a surface.
 * @param hitData             Data associated with the hit surface.
 * @param iData               Retrieved data associated with the hit surface.
 * @param randomStratified    Random number sampler used for sampling.
 * @param lightSampler        Light sampler.
 * @param currentBounce       The current number of bounces along path for current segment.
 * @param minBounces          The minimum number of allowed bounces along path segment before termination.
 * @param maxBounces          The maximum number of allowed bounces along path segment.
 * @param [in,out] normal     Shading normal vector at path segments origin (returns shading normal at current position).
 * @param [in,out] samplePDF  The PDF of sampling the current path segments direction (returns the PDF of sampling the new paths direction).
 * @param [in,out] throughput Combined throughput for current path.
 * @param [in,out] radiance      The visible radiance contribution of the path hit.
 * @return True if path has new segment, False if path should be terminated.
 */
bool pathHit(inout RayDesc ray, HitInfo hitData, IntersectData iData, inout StratifiedSampler randomStratified,
    inout LightSampler lightSampler, uint currentBounce, uint minBounces, uint maxBounces, inout float3 normal,
    inout float samplePDF, inout float3 throughput,
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    inout float3 sampleReflectance,
#endif
    inout pathPayload radiance)
{
    // Shade current position
#ifdef USE_CUSTOM_HIT_FUNCTIONS
    shadePathHitCustom(ray, hitData, iData, lightSampler, currentBounce, normal, samplePDF, throughput,
#else
    shadePathHit(ray, hitData, iData, lightSampler, currentBounce, normal, samplePDF, throughput,
#endif
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
        sampleReflectance,
#endif
        radiance);

    // Terminate early if no more bounces
    if (currentBounce == maxBounces)
    {
        return false;
    }

    float3 viewDirection = -ray.Direction;
    // Stop if surface normal places ray behind surface (note surface normal != geometric normal)
    //  Currently disabled due to incorrect normals generated by normal mapping when not using displacement/parallax
    //if (dot(iData.normal, viewDirection) <= 0.0f)
    //{
    //    return false;
    //}

    // Offset the intersection position to prevent self intersection on generated rays
    float3 offsetOrigin = offsetPosition(iData.position, iData.geometryNormal);

    MaterialBRDF materialBRDF = MakeMaterialBRDF(iData.material, iData.uv);
#ifdef DISABLE_ALBEDO_MATERIAL
    // Disable material albedo if requested
    if (currentBounce == 0)
    {
        materialBRDF.albedo = 0.3f.xxx;
#   ifndef DISABLE_SPECULAR_MATERIALS
        materialBRDF.F0 = 0.0f.xxx;
#   endif // !DISABLE_SPECULAR_MATERIALS
    }
#endif // DISABLE_ALBEDO_MATERIAL

#ifndef DISABLE_NEE
#   ifdef DISABLE_DIRECT_LIGHTING
    // Disable direct lighting if requested
    if (currentBounce > 0)
#   endif // DISABLE_DIRECT_LIGHTING
    {
        // Sample a single light
        sampleLightsNEE(materialBRDF, randomStratified, lightSampler, offsetOrigin,
            iData.normal, iData.geometryNormal, viewDirection, throughput, radiance);
    }
#endif // DISABLE_NEE

    // Sample BRDF to get next ray direction
    float3 rayDirection;
    bool ret = pathNext(materialBRDF, randomStratified, lightSampler, currentBounce, minBounces, maxBounces,
        iData.normal, iData.geometryNormal, viewDirection, throughput, rayDirection, samplePDF
#ifdef ENABLE_NEE_RESERVOIR_SAMPLING
        , sampleReflectance
#endif
    );

    // Update path information
    ray.Origin = offsetOrigin;
    ray.Direction = rayDirection;
    ray.TMin = 0.0f;
    ray.TMax = FLT_MAX;
    normal = iData.normal;
    return ret;
}

/**
 * Trace a new path.
 * @param ray               The ray for the first path segment.
 * @param randomStratified  Random number sampler used for sampling.
 * @param lightSampler      Light sampler.
 * @param currentBounce     The current number of bounces along path for current segment.
 * @param minBounces        The minimum number of allowed bounces along path segment before termination.
 * @param maxBounces        The maximum number of allowed bounces along path segment.
 * @param normal            The shading normal of the current surface (Only valid if bounce > 0).
 * @param throughput        Initial combined throughput for current path.
 * @param [in,out] radiance The visible radiance contribution of the path hit.
 */
void tracePath(RayDesc ray, inout StratifiedSampler randomStratified, inout LightSampler lightSampler,
    uint currentBounce, uint minBounces, uint maxBounces, float3 normal, float3 throughput, inout pathPayload radiance)
{
    // Initialise per-sample path tracing values
#if USE_INLINE_RT
    float samplePDF = 1.0f; // The PDF of the last sampled BRDF
#   ifdef ENABLE_NEE_RESERVOIR_SAMPLING
    float3 sampleReflectance = 0.0f;
#   endif
#else
    PathData pathData;
    pathData.radiance = radiance;
    pathData.throughput = throughput;
    pathData.samplePDF = 1.0f;
    pathData.terminated = false;
    pathData.lightSampler = lightSampler;
    pathData.randomStratified = randomStratified;
#endif

    for (uint bounce = currentBounce; bounce <= maxBounces; ++bounce)
    {
        // Trace the ray through the scene
#if USE_INLINE_RT
        ClosestRayQuery rayQuery = TraceRay<ClosestRayQuery>(ray);

        // Check for valid intersection
        if (rayQuery.CommittedStatus() == COMMITTED_NOTHING)
        {
#   ifdef USE_CUSTOM_HIT_FUNCTIONS
            shadePathMissCustom(ray, bounce, lightSampler, normal, samplePDF, throughput
#   else
            shadePathMiss(ray, bounce, lightSampler, normal, samplePDF, throughput
#   endif
#   ifdef ENABLE_NEE_RESERVOIR_SAMPLING
                , sampleReflectance
#   endif
                , radiance);
            break;
        }
        else
        {
            // Get the intersection data
            HitInfo hitData = GetHitInfoRtInlineCommitted(rayQuery);
            IntersectData iData = MakeIntersectData(hitData);
            if (!pathHit(ray, hitData, iData, randomStratified, lightSampler,
                bounce, minBounces, maxBounces, normal, samplePDF, throughput
#   ifdef ENABLE_NEE_RESERVOIR_SAMPLING
                , sampleReflectance
#   endif
                , radiance))
            {
                break;
            }
        }
#else
        pathData.bounce = bounce;
        TraceRay(g_Scene, CLOSEST_RAY_FLAGS, 0xFFu, 0, 0, 0, ray, pathData);
        // Create new ray
        ray.Origin = pathData.origin;
        ray.Direction = pathData.direction;
        ray.TMin = 0.0f;
        ray.TMax = FLT_MAX;

        if (pathData.terminated)
        {
            break;
        }
#endif
    }

#if !USE_INLINE_RT
    radiance = pathData.radiance;
#endif
}

/**
 * Trace a new path from beginning.
 * @param ray               The ray for the first path segment.
 * @param randomStratified  Random number sampler used for sampling.
 * @param lightSampler      Light sampler.
 * @param minBounces        The minimum number of allowed bounces along path segment before termination.
 * @param maxBounces        The maximum number of allowed bounces along path segment.
 * @param [in,out] radiance The visible radiance contribution of the path hit.
 */
void traceFullPath(RayDesc ray, inout StratifiedSampler randomStratified, inout LightSampler lightSampler,
    uint minBounces, uint maxBounces, inout pathPayload radiance)
{
    tracePath(ray, randomStratified, lightSampler, 0, minBounces, maxBounces, 0.0f.xxx, 1.0f.xxx, radiance);
}

#endif // PATH_TRACING_HLSL
