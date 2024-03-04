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

#ifndef LIGHT_SAMPLER_UNIFORM_HLSL
#define LIGHT_SAMPLER_UNIFORM_HLSL

/*
// Requires the following data to be defined in any shader that uses this file
TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;
*/

#include "../../lights/light_sampling.hlsl"
#include "../../lights/reservoir.hlsl"
#include "../../math/random.hlsl"

struct LightSamplerUniform
{
    Random randomNG;

    /**
     * Get a sample light.
     * @param position Current position on surface.
     * @param normal   Shading normal vector at current position.
     * @param lightPDF (Out) The PDF for the calculated sample (is equal to zero if no valid samples could be found).
     * @returns The index of the new light sample
     */
    uint sampleLights(float3 position, float3 normal, out float lightPDF)
    {
        float totalLights = getNumberLights();

        // Return invalid sample if there are no lights
        if (totalLights == 0)
        {
            lightPDF = 0.0f;
            return 0;
        }

        // Choose a light to sample from
        lightPDF = 1.0f / totalLights;
        uint lightIndex = randomNG.randInt(totalLights);
        return lightIndex;
    }

    /**
     * Calculate the PDF of sampling a given light.
     * @param lightID  The index of the given light.
     * @param position The position on the surface currently being shaded.
     * @param normal   Shading normal vector at current position.
     * @returns The calculated PDF with respect to the light.
     */
    float sampleLightPDF(uint lightID, float3 position, float3 normal)
    {
        return 1.0f / getNumberLights();
    }

    /**
     * Sample multiple lights into a reservoir.
     * @tparam numSampledLights Number of lights to sample.
     * @param position      Current position on surface.
     * @param normal        Shading normal vector at current position.
     * @param viewDirection View direction vector at current position.
     * @param solidAngle    Solid angle around view direction of visible ray cone.
     * @param material      Material for current surface position.
     * @returns Reservoir containing combined samples.
     */
    template<uint numSampledLights>
    Reservoir sampleLightList(float3 position, float3 normal, float3 viewDirection, MaterialBRDF material)
    {
        // Check if we actually have any lights
        const uint totalLights = getNumberLights();
        const uint numLights = numSampledLights;

        // Return invalid sample if there are no lights
        if (numLights == 0)
        {
            return MakeReservoir();
        }

        // Create reservoir updater
        ReservoirUpdater updater = MakeReservoirUpdater();

        // Loop through until we have the requested number of lights
        float lightPDF = 1.0f / totalLights;

        for (uint lightsAdded = 0; lightsAdded < numLights; ++lightsAdded)
        {
            // Choose a light to sample from
            const uint lightIndex = randomNG.randInt(totalLights);

            // Add the light sample to the reservoir
            updateReservoir(updater, randomNG, lightIndex, lightPDF, material, position, normal, viewDirection);
        }

        // Get finalised reservoir for return
        return updater.reservoir;
    }

    /**
     * Sample multiple lights into a reservoir using cone angle.
     * @tparam numSampledLights Number of lights to sample.
     * @param position      Current position on surface.
     * @param normal        Shading normal vector at current position.
     * @param viewDirection View direction vector at current position.
     * @param solidAngle    Solid angle around view direction of visible ray cone.
     * @param material      Material for current surface position.
     * @returns Reservoir containing combined samples.
     */
    template<uint numSampledLights>
    Reservoir sampleLightListCone(float3 position, float3 normal, float3 viewDirection, float solidAngle, MaterialBRDF material)
    {
        // Check if we actually have any lights
        const uint totalLights = getNumberLights();
        const uint numLights = numSampledLights;

        // Return invalid sample if there are no lights
        if (numLights == 0)
        {
            return MakeReservoir();
        }

        // Create reservoir updater
        ReservoirUpdater updater = MakeReservoirUpdater();

        // Loop through until we have the requested number of lights
        float lightPDF = 1.0f / totalLights;

        for (uint lightsAdded = 0; lightsAdded < numLights; ++lightsAdded)
        {
            // Choose a light to sample from
            const uint lightIndex = randomNG.randInt(totalLights);

            // Add the light sample to the reservoir
            updateReservoirCone(updater, randomNG, lightIndex, lightPDF, material, position, normal, viewDirection, solidAngle);
        }

        // Get finalised reservoir for return
        return updater.reservoir;
    }
};

LightSamplerUniform MakeLightSampler(Random random)
{
    LightSamplerUniform ret;
    ret.randomNG = random;
    return ret;
}

typedef LightSamplerUniform LightSampler;

#endif // LIGHT_SAMPLER_UNIFORM_HLSL
