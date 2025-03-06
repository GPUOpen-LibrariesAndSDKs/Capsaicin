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

#ifndef GI1_HLSL
#define GI1_HLSL

#include "math/transform.hlsl"
#include "math/math_constants.hlsl"

// A define for marking invalid identifiers:
#define kGI1_InvalidId 0xFFFFFFFFu

//!
//! GI-1.0 common functions.
//!

#define origin()        (1.0f / 32.0f)
#define float_scale()   (1.0f / 65536.0f)
#define int_scale()     (256.0f)

#undef origin
#undef float_scale
#undef int_scale

float CalculateHaltonNumber(in uint index, in uint base)
{
    float f      = 1.0f;
    float result = 0.0f;

    for (uint i = index; i > 0;)
    {
        f /= base;
        result = result + f * (i % base);
        i = uint(i / float(base));
    }

    return result;
}

float2 CalculateHaltonSequence(in uint index)
{
    return float2(CalculateHaltonNumber((index & 0xFFu) + 1, 2),
                  CalculateHaltonNumber((index & 0xFFu) + 1, 3));
}

#endif // GI1_HLSL
