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

#ifndef SHERICAL_HARMONICS_HLSL
#define SHERICAL_HARMONICS_HLSL

#include "sampling.hlsl"

void SH_GetCoefficients(in float3 direction, out float coefficients[9])
{
    float fC0, fC1, fS0, fS1, fTmpA, fTmpB, fTmpC;
    float pz2 = direction.z * direction.z;
    coefficients[0] = 0.2820947917738781f;
    coefficients[2] = 0.4886025119029199f * direction.z;
    coefficients[6] = 0.9461746957575601f * pz2 + -0.3153915652525201f;
    fC0 = direction.x;
    fS0 = direction.y;
    fTmpA = -0.48860251190292f;
    coefficients[3] = fTmpA * fC0;
    coefficients[1] = fTmpA * fS0;
    fTmpB = -1.092548430592079f * direction.z;
    coefficients[7] = fTmpB * fC0;
    coefficients[5] = fTmpB * fS0;
    fC1 = direction.x * fC0 - direction.y * fS0;
    fS1 = direction.x * fS0 + direction.y * fC0;
    fTmpC = 0.5462742152960395f;
    coefficients[8] = fTmpC * fC1;
    coefficients[4] = fTmpC * fS1;
}

// Check Capsaicin wiki SH page for derivations
void SH_GetCoefficients_ClampedCosine(in float3 cosine_lobe_dir, out float coefficients[9])
{
    // cosine_lobe_dir is normalized
    const float CosineA0 = PI;
    const float CosineA1 = (2.0f * PI) / 3.0f;
    const float CosineA2 = PI / 4.0f;
    float fC0, fC1, fS0, fS1, fTmpA, fTmpB, fTmpC;
    float pz2 = cosine_lobe_dir.z * cosine_lobe_dir.z;
    coefficients[0] = 0.2820947917738781f * CosineA0;
    coefficients[2] = 0.4886025119029199f * cosine_lobe_dir.z * CosineA1;
    coefficients[6] = 0.7431238683011271f * pz2 + -0.2477079561003757f;
    fC0 = cosine_lobe_dir.x;
    fS0 = cosine_lobe_dir.y;
    fTmpA = -0.48860251190292f;
    coefficients[3] = fTmpA * fC0 * CosineA1;
    coefficients[1] = fTmpA * fS0 * CosineA1;
    fTmpB = -1.092548430592079f * cosine_lobe_dir.z;
    coefficients[7] = fTmpB * fC0 * CosineA2;
    coefficients[5] = fTmpB * fS0 * CosineA2;
    fC1 = cosine_lobe_dir.x * fC0 - cosine_lobe_dir.y * fS0;
    fS1 = cosine_lobe_dir.x * fS0 + cosine_lobe_dir.y * fC0;
    fTmpC = 0.5462742152960395f;
    coefficients[8] = fTmpC * fC1 * CosineA2;
    coefficients[4] = fTmpC * fS1 * CosineA2;
}

// Check Capsaicin wiki SH page for derivations
void SH_GetCoefficients_ClampedCosine_Cone(in float3 cosine_lobe_dir, in float cone_theta_max, out float coefficients[9])
{
    // cosine_lobe_dir is normalized
    // cone_theta_max is <= pi
    float sin_theta_max;
    float cos_theta_max;
    sincos(cone_theta_max, sin_theta_max, cos_theta_max);
    float sin_theta_max2 = sin_theta_max * sin_theta_max;
    float sin_theta_max3 = sin_theta_max2 * sin_theta_max;
    float cos_theta_max2 = cos_theta_max * cos_theta_max;
    float cos_theta_max3 = cos_theta_max2 * cos_theta_max;

    float band1_factor = 1.023326707946489f * (1.f - cos_theta_max3);
    float band2_factor = (4.f - 3.f * sin_theta_max3) * sin_theta_max2;

    coefficients[0] = 0.886226925452758f * sin_theta_max2;

    coefficients[1] = -band1_factor * cosine_lobe_dir.y;
    coefficients[2] = +band1_factor * cosine_lobe_dir.z;
    coefficients[3] = -band1_factor * cosine_lobe_dir.x;

    coefficients[4] = +0.8580855308097834f * band2_factor * cosine_lobe_dir.x * cosine_lobe_dir.y;
    coefficients[5] = -0.8580855308097834f * band2_factor * cosine_lobe_dir.y * cosine_lobe_dir.z;
    coefficients[6] = +0.2477079561003757f * band2_factor * (3.f * cosine_lobe_dir.z * cosine_lobe_dir.z - 1.f);
    coefficients[7] = -0.8580855308097834f * band2_factor * cosine_lobe_dir.x * cosine_lobe_dir.z;
    coefficients[8] = +0.4290427654048917f * band2_factor * (cosine_lobe_dir.x * cosine_lobe_dir.x - cosine_lobe_dir.y * cosine_lobe_dir.y);
}

void SH_GetCoefficients_ClampedCosine_Cone_Window(in float3 cosine_lobe_dir, in float cone_theta_max, in float window, out float coefficients[9])
{
    // cosine_lobe_dir is normalized
    // cone_theta_max is <= pi
    // window is >= 0 and small in practice
    float sin_theta_max;
    float cos_theta_max;
    sincos(cone_theta_max, sin_theta_max, cos_theta_max);
    float sin_theta_max2 = sin_theta_max * sin_theta_max;
    float sin_theta_max3 = sin_theta_max2 * sin_theta_max;
    float cos_theta_max2 = cos_theta_max * cos_theta_max;
    float cos_theta_max3 = cos_theta_max2 * cos_theta_max;

    coefficients[0] = 0.886226925452758f * sin_theta_max2;

    float band1_factor = 1.023326707946489f * (1.f - cos_theta_max3);
    float window1 = 1.f / (4.f * window + 1.f);
    coefficients[1] = -band1_factor * window1 * cosine_lobe_dir.y;
    coefficients[2] = +band1_factor * window1 * cosine_lobe_dir.z;
    coefficients[3] = -band1_factor * window1 * cosine_lobe_dir.x;

    float band2_factor = (4.f - 3.f * sin_theta_max3) * sin_theta_max2;
    float window2 = 1.f / (36.f * window + 1.f);
    coefficients[4] = +0.8580855308097834f * band2_factor * window2 * cosine_lobe_dir.x * cosine_lobe_dir.y;
    coefficients[5] = -0.8580855308097834f * band2_factor * window2 * cosine_lobe_dir.y * cosine_lobe_dir.z;
    coefficients[6] = +0.2477079561003757f * band2_factor * window2 * (3.f * cosine_lobe_dir.z * cosine_lobe_dir.z - 1.f);
    coefficients[7] = -0.8580855308097834f * band2_factor * window2 * cosine_lobe_dir.x * cosine_lobe_dir.z;
    coefficients[8] = +0.4290427654048917f * band2_factor * window2 * (cosine_lobe_dir.x * cosine_lobe_dir.x - cosine_lobe_dir.y * cosine_lobe_dir.y);
}

#endif // SHERICAL_HARMONICS_HLSL
