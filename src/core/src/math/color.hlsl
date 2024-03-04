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

#ifndef COLOR_HLSL
#define COLOR_HLSL

#include "math.hlsl"

/**
 * Calculate the luminance (Y) from an input colour.
 * Uses CIE 1931 assuming Rec709 RGB values.
 * @param color Input RGB colour to get value from.
 * @return The calculated luminance.
 */
float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

/**
 * Convert an RGB value to sRGB.
 * Requires input RGB using Rec709 RGB values.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertToSRGB(float3 color)
{
    return select(color < 0.0031308f, 12.92f * color, 1.055f * pow(abs(color), 1.0f / 2.4f) - 0.055f);
}

/**
 * Convert an RGB value to YCoCg.
 * Requires input RGB using Rec709 RGB values.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertRGBToYCoCg(float3 color)
{
    return float3(color.rrr * float3(0.25f, 0.5f, -0.25f)
        + color.ggg * float3(0.5f, 0.0f, 0.5f)
        + color.bbb * float3(0.25f, -0.5f, -0.25f));
}

/**
 * Convert an YCoCg value to RGB.
 * Converts to output RGB using Rec709 RGB values.
 * @param color Input YCoCG colour to convert.
 * @return The converted colour value.
 */
float3 convertYCoCgToRGB(float3 color)
{
    return float3(color.rrr
        + color.ggg * float3(1.0f, 0.0f, -1.0f)
        + color.bbb * float3(-1.0f, 1.0f, -1.0f));
}

/**
 * Encode a value using ITU Rec2100 Perceptual Quantizer (PQ) EOTF.
 * @param value Input value to encode.
 * @return The converted luminance value.
 */
float encodePQEOTF(float value)
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f;

    float powM2 = pow(value, 1.0f / m2);
    return pow(max(powM2 - c1, 0) / (c2 - c3 * powM2), 1.0f / m1);
}

/**
 * Decode a value using ITU Rec2100 Perceptual Quantizer (PQ) EOTF.
 * @param value Input value (should be luminance) to decode.
 * @return The converted value.
 */
float decodePQEOTF(float value)
{
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f;

    float powM1 = pow(value, m1);
    return pow((c1 + c2 * powM1) / (1.0f + c3 * powM1), m2);
}

/**
 * Tonemap an input colour using simple Reinhard.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapSimpleReinhard(float3 color)
{
    return color / (color + 1.0f.xxx);
}

/**
 * Inverse Tonemap an input colour using simple Reinhard.
 * @param color Input colour value to inverse tonemap.
 * @return The inverse tonemapped value.
 */
float3 tonemapInverseSimpleReinhard(float3 color)
{
    return color / (1.0f.xxx - color);
}

/**
 * Tonemap an input colour using luminance based extended Reinhard.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapReinhardLuminance(float3 color)
{
    return color / (1.0f + luminance(color)).xxx;
}

/**
 * Inverse Tonemap an input colour using luminance based extended Reinhard.
 * @param color Input colour value to inverse tonemap.
 * @return The inverse tonemapped value.
 */
float3 tonemapInverseReinhardLuminance(float3 color)
{
    return color / (1.0f - luminance(color)).xxx;
}

/**
 * Tonemap an input colour using ACES.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapACES(float3 color)
{
    return saturate((color * (2.51f.xxx * color + 0.03f.xxx)) / (color * (2.43f.xxx * color + 0.59f.xxx) + 0.14f.xxx));
}

/**
 * Inverse Tonemap an input colour using ACES.
 * @param color Input colour value to inverse tonemap.
 * @return The inverse tonemapped value.
 */
float3 tonemapInverseACES(float3 color)
{
    const float param1 = 0.59f * 0.59f - 4.0f * 2.43f * 0.14f;
    const float param2 = 4.0f * 2.51f * 0.14f - 2.0f * 0.03f * 0.59f;
    return 0.5f * (0.59f * color - sqrt((param1.xxx * color + param2.xxx) * color + squared(0.03f).xxx) - 0.03f) / (2.51f.xxx - 2.43f.xxx * color);
}
#endif // COLOR_HLSL
