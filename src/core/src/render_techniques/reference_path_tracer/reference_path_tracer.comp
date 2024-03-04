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

#ifndef USE_INLINE_RT
#define USE_INLINE_RT 1
#endif

#include "../../geometry/path_tracing_shared.h"

uint2 g_BufferDimensions;
RayCamera g_RayCamera;
uint g_BounceCount;
uint g_BounceRRCount;
uint g_SampleCount;
uint g_Accumulate;

uint g_FrameIndex;

StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Mesh> g_MeshBuffer;
StructuredBuffer<float3x4> g_TransformBuffer;
StructuredBuffer<uint> g_IndexBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
StructuredBuffer<Material> g_MaterialBuffer;

RWTexture2D<float4> g_AccumulationBuffer;
RWTexture2D<float4> g_OutputBuffer;

RaytracingAccelerationStructure g_Scene;

TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);

SamplerState g_TextureSampler; // Should be a linear sampler

#include "../../components/light_sampler/light_sampler.hlsl"
#include "../../geometry/path_tracing.hlsl"

/**
 * Calculate illumination information for a specific pixel
 * @note This writes a single sample per pixel directly to the output buffer
 * @param pixel      The current pixel.
 * @param dimensions The maximum pixel dimensions.
 */
void pathTracer(in uint2 pixel, in uint2 dimensions)
{
    // Setup configurable constants
    const uint minBounces = g_BounceRRCount; //Minimum bounces before early terminations are allowed
    const uint maxBounces = g_BounceCount;
    const uint maxSamples = g_SampleCount;

    //Check if valid pixel
    if (any(pixel >= dimensions))
    {
        return;
    }
    const uint id = pixel.x + pixel.y * dimensions.x;

    // Offset pixel to pixel center
    float2 pixelRay = pixel;
    pixelRay += 0.5f;

    // Initialise per-pixel path tracing values
    float3 radiance = 0.0f;

    // Loop over requested number of samples per pixel
    for (uint sample = 0; sample < maxSamples; ++sample)
    {
        // Intialise random number sampler
        const uint id = pixel.x + pixel.y * dimensions.x;
        const uint frameID = g_FrameIndex * maxSamples + sample;
        LightSampler lightSampler = MakeLightSampler(MakeRandom(id, frameID));

        // Calculate jittered pixel position
        StratifiedSampler randomStratified = MakeStratifiedSampler(id, frameID);
        float2 newPixelRay = pixelRay + lerp(-0.5.xx, 0.5.xx, randomStratified.rand2());

        // Calculate primary ray
        RayDesc ray = generateCameraRay(newPixelRay, g_RayCamera);

        traceFullPath(ray, randomStratified, lightSampler, minBounces, maxBounces, radiance);
    }

    // Check for incorrect calculations
    //if (any(isnan(radiance)) || any(isinf(radiance)) || any(radiance < 0.0f))
    //{
    //    radiance = 0.0f.xxx;
    //}

    // Accumulate previous samples
    if (g_Accumulate != 0)
    {
        // Get previous values
        float4 accumulator = g_AccumulationBuffer[pixel];
        uint previousCount = asuint(accumulator.w);
        // Increment sample count
        uint sampleCount = previousCount + maxSamples;
        radiance += accumulator.xyz;
        // Write out new radiance and sample count to accumulation buffer
        g_AccumulationBuffer[pixel] = float4(radiance, asfloat(sampleCount));
        // Average out radiance so its ready for final output
        radiance /= (float)sampleCount;
    }
    else
    {
        // Write out current value so its ready for next frame
        g_AccumulationBuffer[pixel] = float4(radiance, asfloat(maxSamples));
        // Average out radiance so its ready for final output
        radiance /= (float)maxSamples;
    }

    // Output average accumulation
    g_OutputBuffer[pixel] = float4(radiance, 1.0f);
}

[numthreads(4, 8, 1)]
void ReferencePT(in uint2 did : SV_DispatchThreadID)
{
    pathTracer(did, g_BufferDimensions);
}
