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
 * @note Uses CIE 1931 assuming BT709 linear RGB input values.
 * @param color Input RGB colour to get value from.
 * @return The calculated luminance.
 */
float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

/**
 * Convert a color from linear BT709 to BT2020 color space.
 * @param color Input value to convert.
 * @return The converted value.
 */
float3 convertBT709ToBT2020(float3 color)
{
    const float3x3 mat = float3x3(0.6274178028f, 0.3292815089f, 0.04330066592f,
        0.06909923255f, 0.919541657f, 0.01135913096f,
        0.01639600657f, 0.08803547174f, 0.89556849f);
    return mul(mat, color);
}

/**
 * Convert a color from linear BT2020 to BT709 color space.
 * @param color Input value to convert.
 * @return The converted value.
 */
float3 convertBT2020ToBT709(float3 color)
{
    const float3x3 mat = float3x3(1.660454154f, -0.5876246095f, -0.0728295669f,
        -0.1245510504f, 1.132898331f, -0.00834732037f,
        -0.01815596037f, -0.1006070971f, 1.118763089f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to XYZ.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToXYZ(float3 color)
{
    const float3x3 mat = float3x3(0.4123907983f, 0.3575843275f, 0.1804807931f,
        0.212639004f, 0.7151686549f, 0.07219231874f,
        0.01933081821f, 0.1191947833f, 0.9505321383f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to XYZ with chromatic adaptation.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToXYZAdaptation(float3 color)
{
    // Matrix includes conversion from D65 white-point to E using Bradfords
    const float3x3 mat = float3x3(0.4384595752f, 0.3921765387f, 0.1693638712f,
        0.2228332907f, 0.7086937428f, 0.06847295165f,
        0.01731903292f, 0.1104649305f, 0.8722160459f);
    return mul(mat, color);
}

/**
 * Convert an XYZ value to RGB BT709 linear color space.
 * @param color Input XYZ colour to convert.
 * @return The converted colour value.
 */
float3 convertXYZToBT709(float3 color)
{
    const float3x3 mat = float3x3(3.240835667f, -1.537319541f, -0.4985901117f,
        -0.9692294598f, 1.875940084f, 0.04155444726f,
        0.05564493686f, -0.2040314376f, 1.057253838f);
    return mul(mat, color);
}

/**
 * Convert an XYZ value to RGB BT709 linear color space with chromatic adaptation.
 * @param color Input XYZ colour to convert.
 * @return The converted colour value.
 */
float3 convertXYZToBT709Adaptation(float3 color)
{
    // Matrix includes conversion from E white-point to D65 using Bradfords
    const float3x3 mat = float3x3(3.14657712f, -1.666406274f, -0.480170846f,
        -0.9955176115f, 1.955746531f, 0.03977108747f,
        0.06360134482f, -0.2146037817f, 1.151002407f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to DCIP3.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToDCIP3(float3 color)
{
    const float3x3 mat = float3x3(0.8989887834f, 0.1940520406f, -1.110223025e-16f,
        0.0318220742f, 0.9268168211f, 1.387778781e-17f,
        0.01965498365f, 0.08329702914f, 1.047305584f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to DCIP3 with chromatic adaptation.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToDCIP3Adaptation(float3 color)
{
    // Matrix includes conversion from D65 white-point to DCIP3 using Bradfords
    const float3x3 mat = float3x3(0.8685989976f, 0.1289096773f, 0.002491327235f,
        0.03454159573f, 0.961815834f, 0.003642550204f,
        0.01677691936f, 0.07106060535f, 0.9121624827f);
    return mul(mat, color);
}

/**
 * Convert an DCIP3 value to RGB BT709 linear color space.
 * @param color Input DCIP3 colour to convert.
 * @return The converted colour value.
 */
float3 convertDCIP3ToBT709(float3 color)
{
    const float3x3 mat = float3x3(1.120666623f, -0.2346393019f, 0.0f,
        -0.03847786784f, 1.087018132f, -6.938893904e-18f,
        -0.01797144115f, -0.08205203712f, 0.954831183f);
    return mul(mat, color);
}

/**
 * Convert an DCIP3 value to RGB BT709 linear color space with chromatic adaptation.
 * @param color Input DCIP3 colour to convert.
 * @return The converted colour value.
 */
float3 convertDCIP3ToBT709Adaptation(float3 color)
{
    // Matrix includes conversion from DCIP3 white-point to D65 using Bradfords
    const float3x3 mat = float3x3(1.157490134f, -0.1549475342f, -0.002542620059f,
        -0.04150044546f, 1.045562387f, -0.004061910324f,
        -0.01805607416f, -0.0786030516f, 1.096659064f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to ACEScg.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToACEScg(float3 color)
{
    const float3x3 mat = float3x3(0.6031317711f, 0.3263393044f, 0.04798280075f,
        0.07012086362f, 0.919929862f, 0.01276017074f,
        0.0221798066f, 0.116080001f, 0.9407673478f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to ACEScg with chromatic adaptation.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToACEScgAdaptation(float3 color)
{
    // Matrix includes conversion from D65 white-point to ACEScg using Bradfords
    const float3x3 mat = float3x3(0.6131113768f, 0.3395187259f, 0.04736991972f,
        0.0701957345f, 0.9163579345f, 0.01344634499f,
        0.0206209477f, 0.1095895767f, 0.8697894812f);
    return mul(mat, color);
}

/**
 * Convert an ACEScg value to RGB BT709 linear color space.
 * @param color Input ACEScg colour to convert.
 * @return The converted colour value.
 */
float3 convertACEScgToBT709(float3 color)
{
    const float3x3 mat = float3x3(1.731182098f, -0.6040180922f, -0.0801043883f,
        -0.1316169947f, 1.134824872f, -0.008679305203f,
        -0.02457481436f, -0.1257839948f, 1.065921545f);
    return mul(mat, color);
}

/**
 * Convert an ACEScg value to RGB BT709 linear color space with chromatic adaptation.
 * @param color Input ACEScg colour to convert.
 * @return The converted colour value.
 */
float3 convertACEScgToBT709Adaptation(float3 color)
{
    // Matrix includes conversion from ACEScg white-point to D65 using Bradfords
    const float3x3 mat = float3x3(1.705011487f, -0.6217663884f, -0.0832451731f,
        -0.1302566081f, 1.140798569f, -0.01054200716f,
        -0.02401062287f, -0.1289946884f, 1.153005362f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to YCoCg.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToYCoCg(float3 color)
{
    return float3(color.rrr * float3(0.25f, 0.5f, -0.25f)
        + color.ggg * float3(0.5f, 0.0f, 0.5f)
        + color.bbb * float3(0.25f, -0.5f, -0.25f));
}

/**
 * Convert an YCoCg value to RGB BT709 linear color space.
 * @param color Input YCoCG colour to convert.
 * @return The converted colour value.
 */
float3 convertYCoCgToBT709(float3 color)
{
    return float3(color.rrr
        + color.ggg * float3(1.0f, 0.0f, -1.0f)
        + color.bbb * float3(-1.0f, 1.0f, -1.0f));
}

/**
 * Convert an XYZ value to Lab.
 * @param color Input XYZ colour to convert.
 * @return The converted colour value.
 */
float3 convertXYZToLab(float3 color)
{
    const float3 labWhitePointInverse = float3(1.0f / 0.95047f, 1.0f, 1.0f / 1.08883f);
    float3 lab = color * labWhitePointInverse;
    lab = select(color > (216.0f / 24389.0f), pow(color, 1.0f / 3.0f), ((24389.0f / 27.0f) * color + 16.0f) / 116.0f);
    return float3(116.0f * lab.y, 500.0f * (lab.x - lab.y), 200.0f * (lab.y - lab.z));
}

/**
 * Convert an XYZ value to xyY.
 * @param color Input XYZ colour to convert.
 * @return The converted colour value.
 */
float3 convertXYZToXYY(float3 color)
{
    float divisor = max(color.x + color.y + color.z, FLT_EPSILON);
    return float3(color.xy / divisor.xx, color.z);
}

/**
 * Convert an xyY value to XYZ.
 * @param color Input xyY colour to convert.
 * @return The converted colour value.
 */
float3 convertXYYToXYZ(float3 color)
{
    float yDiv = max(color.y, FLT_EPSILON);
    return float3((color.x * color.z) / yDiv, color.z, ((1.0f - color.x - color.y) * color.z) / yDiv);
}

/**
 * Convert an xy value to XYZ.
 * @param color Input xy colour to convert.
 * @return The converted colour value.
 */
float3 convertXYToXYZ(float2 color)
{
    float yDiv = max(color.y, FLT_EPSILON);
    return float3(color.x / yDiv, 1.0f, (1.0f - color.x - color.y) / yDiv);
}

/**
 * Encode a color using sRGB EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFSRGB(float3 color)
{
    return select(color < 0.03929337067685376f, color / 12.92f, pow((color + 0.055010718947587f) / 1.055010718947587f, 2.4f));
}

/**
 * Decode a color using sRGB EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFSRGB(float3 color)
{
    return select(color < 0.003041282560128f, 12.92f * color, 1.055010718947587f * pow(color, 1.0f / 2.4f) - 0.055010718947587f);
}

/**
 * Encode a color using Rec470m (aka gamma 2.2) EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFRec470m(float3 color)
{
    return pow(color, 2.2f);
}

/**
 * Decode a color using Rec470m (aka gamma 2.2) EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFRec470m(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

/**
 * Encode a color using Rec1886 (aka gamma 2.4) EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFRec1886(float3 color)
{
    return pow(color, 2.4f);
}

/**
 * Decode a color using Rec1886 (aka gamma 2.4) EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFRec1886(float3 color)
{
    return pow(color, 1.0f / 2.4f);
}

/**
 * Encode a color using Rec709 EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFRec709(float3 color)
{
    return select(color < 0.018053968510807f, 4.5f * color, 1.09929682680944f * pow(color, 0.45f) - 0.09929682680944f);
}

/**
 * Decode a color using Rec709 EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFRec709(float3 color)
{
    return select(color < 7.311857246876835f, color / 4.5f, pow((color + 0.09929682680944f) / 1.09929682680944f, 1.0f / 0.45f));
}

/**
 * Encode a color using ST2084 EOTF.
 * @param value Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFST2048(float3 color)
{
    float3 powM2 = pow(color, 1.0f / 78.84375f);
    return pow(max(powM2 - 0.8359375f, 0) / max(18.8515625f - 18.6875f * powM2, FLT_MIN), 1.0f / 0.1593017578125f);
}

/**
 * Decode a color using ST2084 EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFST2048(float3 color)
{
    float3 powM1 = pow(color, 0.1593017578125f);
    return pow((0.8359375f + 18.8515625f * powM1) / (1.0f + 18.6875f * powM1), 78.84375f);
}

/**
 * Convert an RGB value to sRGB.
 * @note Requires input RGB using BT709 linear RGB values.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertToSRGB(float3 color)
{
    return decodeEOTFSRGB(color);
}

/**
 * Convert an sRGB value to RGB.
 * @note Outputs RGB using BT709 linear RGB values.
 * @param color Input sRGB colour to convert.
 * @return The converted colour value.
 */
float3 convertFromSRGB(float3 color)
{
    return encodeEOTFSRGB(color);
}

/**
 * Convert an RGB value to HDR10.
 * @note Requires input RGB using BT709 linear RGB values.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertToHDR10(float3 color)
{
    //Uses BT2020 color space with ST2048 PQ EOTF
    return decodeEOTFST2048(convertBT709ToBT2020(color));
}

/**
 * Convert an HDR10 value to RGB.
 * @note Outputs RGB using BT709 linear RGB values.
 * @param color Input sRGB colour to convert.
 * @return The converted colour value.
 */
float3 convertFromHDR10(float3 color)
{
    return convertBT2020ToBT709(encodeEOTFST2048(color));
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
    return color / (1.0f.xxx - min(color, 0.99999995f));
}

/**
 * Tonemap an input colour using luminance based Reinhard.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapReinhardLuminance(float3 color)
{
    return color / (1.0f + luminance(color)).xxx;
}

/**
 * Inverse Tonemap an input colour using luminance based Reinhard.
 * @param color Input colour value to inverse tonemap.
 * @return The inverse tonemapped value.
 */
float3 tonemapInverseReinhardLuminance(float3 color)
{
    return color / (1.0f - luminance(color)).xxx;
}

/**
 * Tonemap an input colour using extended Reinhard.
 * @param color    Input colour value to tonemap.
 * @param maxWhite Value used to map to 1 in the output (i.e. anything above this is mapped to white).
 * @return The tonemapped value.
 */
float3 tonemapReinhardExtended(float3 color, float maxWhite)
{
    return color * (1.0f + (color / (maxWhite * maxWhite))) / (color + 1.0f.xxx);
}

/**
 * Tonemap an input colour using luminance based extended Reinhard.
 * @param color        Input colour value to tonemap.
 * @param maxLuminance Luminance value used to map to 1 in the output (i.e. anything above this is mapped to white).
 * @return The tonemapped value.
 */
float3 tonemapReinhardExtendedLuminance(float3 color, float maxLuminance)
{
    float lum = luminance(color);
    return color * (lum * (1.0f + (lum / (maxLuminance * maxLuminance))) / (1.0f + lum)) / lum;
}

/**
 * Tonemap an input colour using approximated ACES.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapACESFast(float3 color)
{
    // Based of curve fit by Krzysztof Narkowicz
    return (color * (2.51f.xxx * color + 0.03f.xxx)) / (color * (2.43f.xxx * color + 0.59f.xxx) + 0.14f.xxx);
}

/**
 * Inverse Tonemap an input colour using approximated ACES.
 * @param color Input colour value to inverse tonemap.
 * @return The inverse tonemapped value.
 */
float3 tonemapInverseACESFast(float3 color)
{
    const float param1 = 0.59f * 0.59f - 4.0f * 2.43f * 0.14f;
    const float param2 = 4.0f * 2.51f * 0.14f - 2.0f * 0.03f * 0.59f;
    return 0.5f * (0.59f * color - sqrt((param1.xxx * color + param2.xxx) * color + squared(0.03f).xxx) - 0.03f) / (2.51f.xxx - 2.43f.xxx * color);
}

/**
 * Tonemap an input colour using approximated ACES to 0-1000nit HDR range.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapACESFast1000(float3 color)
{
    // Based of curve fit by Krzysztof Narkowicz
    return (color * (15.8f * color + 2.12f)) / (color * (1.2f * color + 5.92f) + 1.9f);
}

/**
 * Tonemap an input colour using fitted ACES (more precise than approximated version).
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapACESFitted(float3 color)
{
    // Fitted by Stephen Hill
    const float exposureBias = 2.2f;
    const float3x3 RGBtoACES = float3x3(0.59719f, 0.35458f, 0.04823f,
        0.07600f, 0.90834f, 0.01566f,
        0.02840f, 0.13383f, 0.83777f) * exposureBias;
    const float3x3 ACESToRGB = float3x3(1.60475f, -0.53108f, -0.07367f,
        -0.10208f, 1.10813f, -0.00605f,
        -0.00327f, -0.07276f, 1.07602f);
    color = mul(RGBtoACES, color);
    // RRT and ODT curve fitting
    float3 a = color * (color + 0.0245786f) - 0.000090537f;
    float3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
    color = a / b;
    return mul(ACESToRGB, color);
}

/**
 * Tonemap an input colour using ACES 1.1 output transform.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapACES(float3 color)
{
    // This is not the full ACES 1.1 output transform as we skip the glow and red hue passes for performance
    // Single matrix containing 709->XYZ->D65ToD60->AP1->RRT_SAT
    const float3x3 RGBToAPI1RRT = float3x3(0.5962520838f, 0.3408067524f, 0.05560863018f,
        0.08636811376f, 0.9165210128f, 0.03800068051f,
        0.02130785212f, 0.108138442f, 0.8369964361f);
    color = mul(RGBToAPI1RRT, color);

    // Apply SSTS curve (using pre-calculated values with default SDR min and max cinema luminance)
    const float3x3 M1 = {0.5f, -1.0f, 0.5f, -1.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f};
    const float maxLuminance = 48.0f;
    const float minLuminance = 0.02f;
    float cLow[5] = { -1.69896996f, -1.69896996f, -0.8658960462f, 0.1757616997f, 1.186720729f };
    float cHigh[5] = { 0.1757616997f, 1.186720729f, 1.57124126f, 1.681241274f, 1.681241274f };
    const float logMinX = -3.093823671;
    const float logMidX = -1.137128711f;
    const float logMaxX = 0.8195661902f;
    float3 logx = log10(max(color, FLT_MIN));
    float3 logy;
    for (uint ind = 0; ind < 3; ++ind)
    {
        if (logx[ind] <= logMinX)
        {
            logy[ind] = -1.69896996f;
        }
        else if (logx[ind] < logMidX)
        {
            float knot = 3.0f * (logx[ind] - logMinX) / (logMidX - logMinX);
            uint j = knot;
            float t = knot - (float) j;
            float3 cf = float3(cLow[j], cLow[j + 1], cLow[j + 2]);
            float3 monomials = float3(t * t, t, 1.0f);
            logy[ind] = dot(monomials, mul(M1, cf));
        }
        else if (logx[ind] < logMaxX)
        {
            float knot = 3.0f * (logx[ind] - logMidX) / (logMaxX - logMidX);
            uint j = knot;
            float t = knot - (float) j;
            float3 cf = float3(cHigh[j], cHigh[j + 1], cHigh[j + 2]);
            float3 monomials = float3(t * t, t, 1.0f);
            logy[ind] = dot(monomials, mul(M1, cf));
        }
        else
        {
            logy[ind] = 1.681241274f;
        }
    }
    color = pow(10.0f, logy);

    // Apply linear luminance scale
    color = (color - minLuminance) / (maxLuminance - minLuminance);

    // Convert back to input color space
    return convertACEScgToBT709Adaptation(color);
}

/**
 * Tonemap an input colour using ACES 1.1 output transform.
 * @param color        Input colour value to tonemap.
 * @param maxLuminance Max luminance value of the output range.
 * @return The tonemapped value.
 */
float3 tonemapACES(float3 color, float maxLuminance)
{
    // This is not the full ACES 1.1 output transform as we skip the glow and red hue passes for performance
    // Single matrix containing 709->XYZ->D65ToD60->AP1->RRT_SAT
    const float3x3 RGBToAPI1RRT = float3x3(0.5962520838f, 0.3408067524f, 0.05560863018f,
        0.08636811376f, 0.9165210128f, 0.03800068051f,
        0.02130785212f, 0.108138442f, 0.8369964361f);
    color = mul(RGBToAPI1RRT, color);

    // Calculate SSTS curve values based on display luminance (Note: These can be pre-calculated)
    const float3x3 M1 = {0.5f, -1.0f, 0.5f, -1.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f};
    const float minLuminance = 0.0001f;
    const float logMaxLuminance = log10(maxLuminance);
    float acesMax = 0.18f * pow(2.0f, lerp(6.5f, 18.0f, (logMaxLuminance - 1.681241274f) / 2.318758726f));
    float acesMin = 0.000005493164281f;
    float cLow[5] = { -4.0f, -4.0f, -3.157376528f, -0.4852499962f, 1.847732425f };
    float cHigh[5];
    const float logAcesMax = log10(acesMax);
    float knotIncHigh = (logAcesMax + 0.7447274923f) / 6.0f;
    cHigh[0] = (1.55f * (-0.7447274923f - knotIncHigh)) + 1.835568905f;
    cHigh[1] = (1.55f * (-0.7447274923f + knotIncHigh)) + 1.835568905f;
    float p = log2(acesMax / 0.18f);
    float s = saturate((p - 6.5f) / (18.0f - 6.5f));
    float pctHigh = 0.89f * (1.0f - s) + 0.90f * s;
    cHigh[3] = logMaxLuminance;
    cHigh[4] = cHigh[3];
    cHigh[2] = 0.6812412143f + pctHigh * (cHigh[3] - 0.6812412143f);
    const float log15 = 1.176091313f;
    uint j = (log15 <= (cHigh[1] + cHigh[2]) / 2.0f) ? 0 : ((log15 <= (cHigh[2] + cHigh[3]) / 2.0f) ? 1 : 2);
    float3 tmp = mul(M1, float3(cHigh[j], cHigh[j + 1], cHigh[j + 2]));
    tmp.z -= log15;
    const float t = (2.0f * tmp.z) / (-sqrt(tmp.y * tmp.y - 4.0f * tmp.x * tmp.z) - tmp.y);
    const float knotHigh = (logAcesMax + 0.7447274923f) / 3.0f;
    float expShift = log2(pow(10.0f, -0.7447274923f + (t + (float)j) * knotHigh)) + 2.473931074f;
    acesMin = pow(2.0f, (log2(acesMin) - expShift));
    float acesMid = pow(2.0f, (-2.473931074f - expShift));
    acesMax = pow(2.0f, (log2(acesMax) - expShift));
    const float logMinX = log10(acesMin);
    const float logMidX = log10(acesMid);
    const float logMaxX = log10(acesMax);

    // Apply SSTS curve
    const float3 logx = log10(max(color, FLT_MIN));
    float3 logy;
    for (uint ind = 0; ind < 3; ++ind)
    {
        if (logx[ind] <= logMinX)
        {
            logy[ind] = -4.0f;
        }
        else if (logx[ind] < logMidX)
        {
            float knot = 3.0f * (logx[ind] - logMinX) / (logMidX - logMinX);
            uint j = knot;
            float t = knot - (float) j;
            float3 cf = float3(cLow[j], cLow[j + 1], cLow[j + 2]);
            float3 monomials = float3(t * t, t, 1.0f);
            logy[ind] = dot(monomials, mul(M1, cf));
        }
        else if (logx[ind] < logMaxX)
        {
            float knot = 3.0f * (logx[ind] - logMidX) / (logMaxX - logMidX);
            uint j = knot;
            float t = knot - (float) j;
            float3 cf = float3(cHigh[j], cHigh[j + 1], cHigh[j + 2]);
            float3 monomials = float3(t * t, t, 1.0f);
            logy[ind] = dot(monomials, mul(M1, cf));
        }
        else
        {
            logy[ind] = logMaxLuminance;
        }
    }
    color = pow(10.0f, logy);

    // Apply linear luminance scale
    color = (color - minLuminance) / (maxLuminance - minLuminance);

    // Convert back to input color space
    return convertACEScgToBT709Adaptation(color);
}

/**
 * Tonemap an input colour using Hable Uncharted 2 tonemapper.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapUncharted2(float3 color)
{
    const float A = 0.15f;
    const float B = 0.50f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.02f;
    const float F = 0.30f;
    const float white = 11.2f;

    float exposure_bias = 2.0f;
    color *= exposure_bias;
    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;

    float whiteScale = 1.0f / ((white * (A * white + C * B) + D * E) / (white * (A * white + B) + D * F)) - E / F;
    return color * whiteScale;
}

/**
 * Tonemap an input colour using Khronos PBR neutral.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapPBRNeutral(float3 color)
{
    const float F90 = 0.04F;
    const float ks = 0.8F - F90;
    const float kd = 0.15F;

    float x = hmin(color);
    float offset = x < (2.0F * F90) ? x - (1.0F / (4.0F * F90)) * x * x : 0.04F;
    color -= offset;

    float p = hmax(color);
    if (p <= ks)
    {
        return color;
    }

    float d = 1.0F - ks;
    float pn = 1.0F - d * d / (p + d - ks);

    float g = 1.0F / (kd * (p - pn) + 1.0F);
    return lerp(pn.xxx, color * (pn / p), g);
}

/**
 * Tonemap an input colour using fitted Agx.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapAgxFitted(float3 color)
{
    // Apply input transform
    const float3x3 RGBToAgx = float3x3(0.8566271663f, 0.09512124211f, 0.04825160652f,
        0.1373189688f, 0.7612419724f, 0.1014390364f,
        0.1118982136f, 0.07679941505f, 0.8113023639f);
    color = mul(RGBToAgx, color);

    // Convert to log2 space
    const float minEV = -12.47393f;
    const float maxEV = 4.026069f;
    color = log2(color);
    color = (color - minEV) / (maxEV - minEV);
    color = saturate(color);

    // Apply sigmoid curve fit
    // Use polynomial fit of original Agx LUT by Benjamin Wrensch
    float3 colorX2 = color * color;
    float3 colorX4 = colorX2 * colorX2;
    color = 15.5f * colorX4 * colorX2 - 40.14f * colorX4 * color + 31.96f * colorX4 - 6.868f
        * colorX2 * color + 0.4298f * colorX2 + 0.1191f * color - 0.00232f;

    // Apply inverse transform
    const float3x3 AgxToRGB = float3x3(1.127100587f, -0.1106066406f, -0.01649393886f,
        -0.1413297653f, 1.157823682f, -0.01649393886f,
        -0.1413297653f, -0.1106066406f, 1.251936436f);
    color = mul(AgxToRGB, color);

    // Linearise color
    color = pow(color, 2.2f);

    return color;
}

/**
 * Tonemap an input colour using fitted Agx.
 * @param color        Input colour value to tonemap.
 * @param maxLuminance Max luminance value of the output range.
 * @return The tonemapped value.
 */
float3 tonemapAgxFitted(float3 color, float maxLuminance)
{
    // Apply input transform
    const float3x3 RGBToAgx = float3x3(0.8566271663f, 0.09512124211f, 0.04825160652f,
        0.1373189688f, 0.7612419724f, 0.1014390364f,
        0.1118982136f, 0.07679941505f, 0.8113023639f);
    color = mul(RGBToAgx, color);

    // Convert to log2 space
    const float minEV = -12.47393f;
    const float maxEV = log2(maxLuminance * 0.18f); // Scaled by middle grey
    color = log2(color);
    color = (color - minEV) / (maxEV - minEV);
    color = saturate(color);

    // Apply sigmoid curve fit
    // Use polynomial fit of original Agx LUT by Benjamin Wrensch
    float3 colorX2 = color * color;
    float3 colorX4 = colorX2 * colorX2;
    color = 15.5f * colorX4 * colorX2 - 40.14f * colorX4 * color + 31.96f * colorX4 - 6.868f
        * colorX2 * color + 0.4298f * colorX2 + 0.1191f * color - 0.00232f;

    // Apply inverse transform
    const float3x3 AgxToRGB = float3x3(1.127100587f, -0.1106066406f, -0.01649393886f,
        -0.1413297653f, 1.157823682f, -0.01649393886f,
        -0.1413297653f, -0.1106066406f, 1.251936436f);
    color = mul(AgxToRGB, color);

    // Linearise color
    color = pow(color, 2.2f);

    // Scale back to max luminance
    color *= maxLuminance;

    return color;
}


/**
 * Tonemap an input colour using Agx.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapAgx(float3 color)
{
    // Apply input transform
    const float3x3 RGBToAgx = float3x3(0.8566271663f, 0.09512124211f, 0.04825160652f,
        0.1373189688f, 0.7612419724f, 0.1014390364f,
        0.1118982136f, 0.07679941505f, 0.8113023639f);
    color = mul(RGBToAgx, color);

    // Convert to log2 space
    const float minEV = -12.47393f;
    const float maxEV = 4.026069f;
    color = log2(color);
    color = (color - minEV) / (maxEV - minEV);
    color = saturate(color);

    // Apply sigmoid curve fit
    // Uses factored version of original Agx curve
    for (uint ind = 0; ind < 3; ++ind)
    {
        float numerator = 2.0f * (-0.6060606241f + color[ind]);
        if (color[ind] >= 0.6060606241f)
        {
            color[ind] = numerator / pow(1.0f + 69.86278914f * pow(color[ind] - 0.6060606241f, 3.25f), 0.3076923192f);
        }
        else
        {
            color[ind] = numerator / pow(1.0 - 59.507875f * pow(color[ind] - 0.6060606241f, 3), 0.3333333433f);
        }
        color[ind] += 0.5f;
    }

    // Apply inverse transform
    const float3x3 AgxToRGB = float3x3(1.127100587f, -0.1106066406f, -0.01649393886f,
        -0.1413297653f, 1.157823682f, -0.01649393886f,
        -0.1413297653f, -0.1106066406f, 1.251936436f);
    color = mul(AgxToRGB, color);

    // Linearise color
    color = pow(color, 2.2f);

    return color;
}

/**
 * Tonemap an input colour using Agx.
 * @param color        Input colour value to tonemap.
 * @param maxLuminance Max luminance value of the output range.
 * @return The tonemapped value.
 */
float3 tonemapAgx(float3 color, float maxLuminance)
{
    // Apply input transform
    const float3x3 RGBToAgx = float3x3(0.8566271663f, 0.09512124211f, 0.04825160652f,
        0.1373189688f, 0.7612419724f, 0.1014390364f,
        0.1118982136f, 0.07679941505f, 0.8113023639f);
    color = mul(RGBToAgx, color);

    // Convert to log2 space
    const float minEV = -12.47393f;
    const float maxEV = log2(maxLuminance * 0.18f); // Scaled by middle grey
    color = log2(color);
    color = (color - minEV) / (maxEV - minEV);
    color = saturate(color);

    // Apply sigmoid curve fit
    // Uses factored version of original Agx curve
    for (uint ind = 0; ind < 3; ++ind)
    {
        float numerator = 2.0f * (-0.6060606241f + color[ind]);
        if (color[ind] >= 0.6060606241f)
        {
            color[ind] = numerator / pow(1.0f + 69.86278914f * pow(color[ind] - 0.6060606241f, 3.25f), 0.3076923192f);
        }
        else
        {
            color[ind] = numerator / pow(1.0 - 59.507875f * pow(color[ind] - 0.6060606241f, 3), 0.3333333433f);
        }
        color[ind] += 0.5f;
    }

    // Apply inverse transform
    const float3x3 AgxToRGB = float3x3(1.127100587f, -0.1106066406f, -0.01649393886f,
        -0.1413297653f, 1.157823682f, -0.01649393886f,
        -0.1413297653f, -0.1106066406f, 1.251936436f);
    color = mul(AgxToRGB, color);

    // Linearise color
    color = pow(color, 2.2f);

    // Scale back to max luminance
    color *= maxLuminance;

    return color;
}

#endif // COLOR_HLSL
