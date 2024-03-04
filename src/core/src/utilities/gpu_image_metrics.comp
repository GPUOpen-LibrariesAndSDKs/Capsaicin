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

#include "../math/math.hlsl"
#include "../math/color.hlsl"

uint2 g_ImageDimensions;

#if defined(INPUT_MULTICHANNEL)
Texture2D g_SourceImage;
Texture2D g_ReferenceImage;
#else
Texture2D<float> g_SourceImage;
Texture2D<float> g_ReferenceImage;
#endif

RWStructuredBuffer<float> g_MetricBuffer;

#define GROUP_SIZE 16

groupshared float lds[(GROUP_SIZE * GROUP_SIZE) / 16]; //Assume 16 as smallest possible wave size
groupshared uint ldsWrites;

// Reduce sum template function
void BlockReduceSum(float value, uint2 did, uint gtid, uint2 gid)
{
    // Combine values across the wave
    value = WaveActiveSum(value);

    // Combine values across the group
    const uint groupSize = GROUP_SIZE * GROUP_SIZE;
    for (uint j = WaveGetLaneCount(); j < groupSize; j *= WaveGetLaneCount())
    {
        // Since we work on square tiles its possible that some waves don't write to lds as they have no valid pixels
        //   To ensure that invalid values arent read from lds we use an atomic to count the actual lds writes
        if (gtid == 0)
        {
            // Clear atomic
            InterlockedAnd(ldsWrites, 0);
        }
        GroupMemoryBarrierWithGroupSync();

        // Use local data share to combine across waves
        if (WaveIsFirstLane())
        {
            uint waveID;
            InterlockedAdd(ldsWrites, 1, waveID);
            lds[waveID] = value;

        }
        GroupMemoryBarrierWithGroupSync();

        uint numWaves = ldsWrites;
        if (gtid >= numWaves)
        {
            break;
        }

        // Use the current wave to combine across group
        value = lds[gtid];
        value = WaveActiveSum(value);
    }

    // Write out final result
    if (gtid == 0)
    {
        const uint blockCount = (g_ImageDimensions.x + GROUP_SIZE - 1) / GROUP_SIZE;
        const uint blockIndex = gid.x + gid.y * blockCount;
        g_MetricBuffer[blockIndex] = value;
    }
}

float2 GetImageValues(uint2 pixel)
{
#if defined(INPUT_MULTICHANNEL)
    float3 inputSource = g_SourceImage[pixel].xyz;
    float3 referenceSource = g_ReferenceImage[pixel].xyz;
#   if !defined(INPUT_HDR)
    // Most image comparison uses luma instead of luminance which requires converting to gamma corrected values
    // Textures containing sRGB wil get auto linearized when reading so must manually convert back
    inputSource = convertToSRGB(inputSource);
    referenceSource = convertToSRGB(referenceSource);
#   endif
    // Although MSE etc. can operate on RGB values by just extending the sum to include all 3 channels,
    //   this has been shown to be inaccurate when compared to measured perceptual results. As such we
    //   instead operate on luma
    float input = luminance(inputSource);
    float reference = luminance(referenceSource);
#else
    float input = g_SourceImage[pixel].x;
    float reference = g_ReferenceImage[pixel].x;
#   if !defined(INPUT_HDR) && defined(INPUT_LINEAR)
    input = select(input < 0.0031308f, 12.92f * input, 1.055f * pow(abs(input), 1.0f / 2.4f) - 0.055f);
    reference = select(reference < 0.0031308f, 12.92f * reference, 1.055f * pow(abs(reference), 1.0f / 2.4f) - 0.055f);
#   endif
#endif
#if defined(INPUT_HDR)
    // Standard MSE metrics dont work well with HDR data
    // As such the input values need to be converted to non-linear perceptual values using a conversion metric
    // Here we use the ITU Rec2100 Perceptual Quantizer (PQ) transfer function
    input = decodePQEOTF(input);
    reference = decodePQEOTF(reference);
#endif
    return float2(input, reference);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void ComputeMetric(uint2 did : SV_DispatchThreadID, uint gtid : SV_GroupIndex, uint2 gid : SV_GroupID)
{
    if (any(did >= g_ImageDimensions))
    {
        return;
    }


#ifdef CALCULATE_SSIM
    // Each pixel samples from a 11x11 window centered around the pixel. Each sample is weighted by
    //   a Gaussian with sigma=1.5
    // Note: The Gaussian is symmetric so most of these weight are duplicates
    const float gaussianWeights[11][11]={
    {1.057565598e-06, 7.814411533e-06, 3.702247708e-05, 0.0001124643551, 0.0002190506529, 0.0002735611601, 0.0002190506529, 0.0001124643551, 3.702247708e-05, 7.814411533e-06, 1.057565598e-06},
    {7.814411533e-06, 5.77411252e-05, 0.0002735611601, 0.0008310054291, 0.001618577563, 0.002021358758, 0.001618577563, 0.0008310054291, 0.0002735611601, 5.77411252e-05, 7.814411533e-06},
    {3.702247708e-05, 0.0002735611601, 0.001296055594, 0.003937069263, 0.007668363825, 0.00957662749, 0.007668363825, 0.003937069263, 0.001296055594, 0.0002735611601, 3.702247708e-05},
    {0.0001124643551, 0.0008310054291, 0.003937069263, 0.01195976041, 0.02329443247, 0.02909122565, 0.02329443247, 0.01195976041, 0.003937069263, 0.0008310054291, 0.0001124643551},
    {0.0002190506529, 0.001618577563, 0.007668363825, 0.02329443247, 0.0453713591, 0.05666197049, 0.0453713591, 0.02329443247, 0.007668363825, 0.001618577563, 0.0002190506529},
    {0.0002735611601, 0.002021358758, 0.00957662749, 0.02909122565, 0.05666197049, 0.07076223776, 0.05666197049, 0.02909122565, 0.00957662749, 0.002021358758, 0.0002735611601},
    {0.0002190506529, 0.001618577563, 0.007668363825, 0.02329443247, 0.0453713591, 0.05666197049, 0.0453713591, 0.02329443247, 0.007668363825, 0.001618577563, 0.0002190506529},
    {0.0001124643551, 0.0008310054291, 0.003937069263, 0.01195976041, 0.02329443247, 0.02909122565, 0.02329443247, 0.01195976041, 0.003937069263, 0.0008310054291, 0.0001124643551},
    {3.702247708e-05, 0.0002735611601, 0.001296055594, 0.003937069263, 0.007668363825, 0.00957662749, 0.007668363825, 0.003937069263, 0.001296055594, 0.0002735611601, 3.702247708e-05},
    {7.814411533e-06, 5.77411252e-05, 0.0002735611601, 0.0008310054291, 0.001618577563, 0.002021358758, 0.001618577563, 0.0008310054291, 0.0002735611601, 5.77411252e-05, 7.814411533e-06},
    {1.057565598e-06, 7.814411533e-06, 3.702247708e-05, 0.0001124643551, 0.0002190506529, 0.0002735611601, 0.0002190506529, 0.0001124643551, 3.702247708e-05, 7.814411533e-06, 1.057565598e-06}};
    const float sumSquaredWeights = 0.0353944717; // The sum of the squared weights needed for weighted variance

    const int offset=5;
    float width, height;
    g_SourceImage.GetDimensions(width, height);

    // Note: The current approach is much slower than it could be as each window recalculates pixel values that
    //   are shared with neighboring windows. Splitting this into separate passes will improve performance but we
    //   instead go for simplicity as we are not concerned with performance here as image metrics are expected to be
    //   performed outside of benchmark runs.

    // Calculate the weighted pixel mean over the window as the sum of each weighted pixel
    //  MeanImg = Sum(Img.x.y * Weight.x.y) / Sum(Weight.x.y)
    //  The Gaussian weights are normalised so the total weight is 1
    float sampleMeanInput = 0.0f;
    float sampleMeanReference = 0.0f;
    // Clamp window to avoid going over image edges
    uint minX = (uint)max(offset - (int)did.x, 0);
    uint maxX = (uint)min(offset + (int)width - (int)did.x, 11);
    uint minY = (uint)max(offset - (int)did.y, 0);
    uint maxY = (uint)min(offset + (int)height - (int)did.y, 11);
    for(int x = minX; x < maxX; ++x)
    {
        for(int y = minY; y < maxY; ++y)
        {
            uint2 pixels = uint2(did.x + x - offset, did.y + y - offset);
            float2 values = GetImageValues(pixels);
            float input = values.x;
            float reference = values.y;
            float pixelWeight =  gaussianWeights[x][y];
            sampleMeanInput += input * pixelWeight;
            sampleMeanReference += reference * pixelWeight;
        }
    }

    // Calculate the weighted unbiased variance and cross-correlation
    //  VarianceImg = Sum([Img.x.y - MeanImg]^2 * Weight.x.y) / (1 - Sum(Weight.x.y^2))
    //  CrossCorrelation = Sum([Img.x.y - MeanImg] * [Img2.x.y - MeanImg2] * Weight.x.y) / (1 - Sum(Weight.x.y^2))
    // Note: We don't use two-pass method for variance due to precision error considerations.
    float varianceInput = 0.0f;
    float varianceReference = 0.0f;
    float crossCorrelation = 0.0f;
    for(int x = minX; x < maxX; ++x)
    {
        for(int y = minY; y < maxY; ++y)
        {
            uint2 pixels = uint2(did.x + x - offset, did.y + y - offset);
            float2 values = GetImageValues(pixels);
            float input = values.x;
            float reference = values.y;
            float pixelWeight =  gaussianWeights[x][y];
            float inputSq = input - sampleMeanInput;
            float referenceSq = reference - sampleMeanReference;
            varianceInput += inputSq * inputSq * pixelWeight;
            varianceReference += referenceSq * referenceSq * pixelWeight;
            crossCorrelation += inputSq * referenceSq * pixelWeight;
        }
    }
    varianceInput /= 1.0f - sumSquaredWeights;
    varianceReference /= 1.0f - sumSquaredWeights;
    crossCorrelation /= 1.0f - sumSquaredWeights;
    // Clamp negative variance
    varianceInput = max(varianceInput, 0);
    varianceReference = max(varianceReference, 0);

    // Calculate final SSIM
    //  SSIM = (2 * MeanImg * MeanImg2 + c1) * (2 * CrossCorrelation + c2)
    //                                     /
    //       (MeanImg^2 + MeanImg2^2 + c1) * (VarianceImg + VarianceImg2 + c2)
    //  where
    //    c1 = (k1 * L)^2, c2 = (k2 * L), k1 = 0.01, k2 = 0.03
    //    Input range is [0,1] due to PQ-luma so L=1
    const float k1 = 0.01f;
    const float k2 = 0.03f;
    const float c1 = k1 * k1;
    const float c2 = k2 * k2;
    float value1 = (2.0f * sampleMeanInput * sampleMeanReference) + c1;
    float value2 = (2.0f * crossCorrelation) + c2;
    float divisor1 = (sampleMeanInput * sampleMeanInput) + (sampleMeanReference * sampleMeanReference) + c1;
    float divisor2 = varianceInput + varianceReference + c2;
    float value = value1 * value2;
    value /= divisor1 * divisor2;
#else
    float2 values = GetImageValues(did);
    float input = values.x;
    float reference = values.y;
    // MSE = [1/(width*height)]Sum([Ref.x.y - Src.x.y]^2)
    // RMSE = sqrt(MSE)
    // PSNR = 20log10(MaxValue) - 10log10(MSE)
    // RMAE = [1/(width*height)]Sum(Abs(Src.x.y - Ref.x.y)/Ref.x.y)
    // SMAPE = [100/(width*height)]Sum(Abs(Ref.x.y - Src.x.y)/([abs(Ref.x.y)+Abs(Src.x.y)]/2)
    float value = reference - input;
#   ifdef CALCULATE_RMAE
    value = abs(value) / reference;
    value = (reference != 0) ? value : 0.0f;
#   elif defined(CALCULATE_SMAPE)
    float divisor = (abs(reference) + abs(input)) / 2.0f;
    value = abs(value) / divisor;
    value = (divisor != 0) ? value : 0.0f;
#   else
    value *= value;
#   endif
#endif
    BlockReduceSum(value, did, gtid, gid);
}
