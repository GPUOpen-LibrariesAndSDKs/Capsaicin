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

#ifndef BLUE_NOISE_SAMPLER_HLSL
#define BLUE_NOISE_SAMPLER_HLSL
// Requires the following data to be defined in any shader that uses this file
StructuredBuffer<uint> g_SobolBuffer;
StructuredBuffer<uint> g_RankingTile;
StructuredBuffer<uint> g_ScramblingTile;

#define GOLDEN_RATIO 1.61803398874989484820f

float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(in int pixel_i, in int pixel_j, in int sampleIndex, in int sampleDimension)
{
    // A Low-Discrepancy Sampler that Distributes Monte Carlo Errors as a Blue Noise in Screen Space - Heitz etal

    // wrap arguments
    pixel_i         = (pixel_i & 127);
    pixel_j         = (pixel_j & 127);
    sampleIndex     = (sampleIndex & 255);
    sampleDimension = (sampleDimension & 255);

    // xor index based on optimized ranking
    int rankedSampleIndex = sampleIndex ^ g_RankingTile[sampleDimension + (pixel_i + pixel_j * 128) * 8];

    // fetch value in sequence
    int value = g_SobolBuffer[sampleDimension + rankedSampleIndex * 256];

    // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ g_ScramblingTile[(sampleDimension % 8) + (pixel_i + pixel_j * 128) * 8];

    // convert to float and return
    return (0.5f + value) / 256.0f;
}

float BlueNoise_Sample1D(in uint2 pixel, in uint sample_index, in uint dimension_offset)
{
    // https://blog.demofox.org/2017/10/31/animating-noise-for-integration-over-time/
    float s = samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel.x, pixel.y, 0, dimension_offset);

    return fmod(s + (sample_index & 255) * GOLDEN_RATIO, 1.0f);
}

float BlueNoise_Sample1D(in uint2 pixel, in uint sample_index)
{
    return BlueNoise_Sample1D(pixel, sample_index, 0);
}

float2 BlueNoise_Sample2D(in uint2 pixel, in uint sample_index, in uint dimension_offset)
{
    // https://blog.demofox.org/2017/10/31/animating-noise-for-integration-over-time/
    float2 s = float2(samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel.x, pixel.y, 0, dimension_offset + 0),
                      samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel.x, pixel.y, 0, dimension_offset + 1));

    return fmod(s + (sample_index & 255) * GOLDEN_RATIO, 1.0f);
}

float2 BlueNoise_Sample2D(in uint2 pixel, in uint sample_index)
{
    return BlueNoise_Sample2D(pixel, sample_index, 0);
}

float3 BlueNoise_Sample3D(in uint2 pixel, in uint sample_index, in uint dimension_offset)
{
    // https://blog.demofox.org/2017/10/31/animating-noise-for-integration-over-time/
    float3 s = float3(samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel.x, pixel.y, 0, dimension_offset + 0),
                      samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel.x, pixel.y, 0, dimension_offset + 1),
                      samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel.x, pixel.y, 0, dimension_offset + 2));

    return fmod(s + (sample_index & 255) * GOLDEN_RATIO, 1.0f);
}

float3 BlueNoise_Sample3D(in uint2 pixel, in uint sample_index)
{
    return BlueNoise_Sample3D(pixel, sample_index, 0);
}

#endif // BLUE_NOISE_SAMPLER_HLSL
