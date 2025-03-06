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

#ifndef LIGHT_SAMPLER_HLSL
#define LIGHT_SAMPLER_HLSL

#include "lights/lights.hlsl"

// Requires the following data to be defined in any shader that uses this file
StructuredBuffer<uint> g_LightBufferSize;
StructuredBuffer<Light> g_LightBuffer;
#ifdef ENABLE_PREVIOUS_LIGHTS
StructuredBuffer<Light> g_PrevLightBuffer;
#endif
RWStructuredBuffer<uint> g_LightInstanceBuffer;

/**
 * Check if the current scene has an environment light.
 * @return True if environment light could be found.
 */
bool hasLights()
{
    return (g_LightBufferSize[0] > 0);
}

/**
 * Get number of lights.
 * @return The number of lights currently in the scene.
 */
uint getNumberLights()
{
    return g_LightBufferSize[0];
}

/**
 * Get a light corresponding to a light index.
 * @param index The index of the light to retrieve (range [0, getNumberLights())).
 * @return The number of lights currently in the scene.
 */
Light getLight(uint index)
{
    return g_LightBuffer[index];
}

#ifdef ENABLE_PREVIOUS_LIGHTS
/**
 * Get a light corresponding to a light index.
 * @param index The index of the light to retrieve (range [0, getNumberLights())).
 * @return The number of lights currently in the scene.
 */
Light getPreviousLight(uint index)
{
    return g_PrevLightBuffer[index];
}
#endif // ENABLE_PREVIOUS_LIGHTS

/**
 * Check if the current scene has an environment light.
 * @return True if environment light could be found.
 */
bool hasEnvironmentLight()
{
#ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (g_LightBufferSize[0] == 0)
    {
        return false;
    }
    // Assumes that the environment light is always first
    Light selectedLight = g_LightBuffer[0];
    return selectedLight.get_light_type() == kLight_Environment;
#else
    return false;
#endif // DISABLE_ENVIRONMENT_LIGHTS
}

/**
 * Get the current environment map.
 * @note This is only valid if the scene contains a valid environment map.
 * @return The environment map.
 */
LightEnvironment getEnvironmentLight()
{
    return MakeLightEnvironment(g_LightBuffer[0]);
}

/**
 * Get the environment map ID.
 * @return the ID.
 */
uint getEnvironmentLightIndex()
{
    return 0;
}

/**
 * Get the light ID for a specific area light.
 * @note The inputs are not checked to ensure they actually map to a valid emissive surface.
 * @param instanceIndex  Instance ID of requested area light.
 * @param primitiveIndex Primitive ID of requested area light.
 * @return The light ID, undefined if inputs are invalid.
 */
uint getAreaLightIndex(uint instanceIndex, uint primitiveIndex)
{
    return primitiveIndex + g_LightInstanceBuffer[instanceIndex];
}

#endif
