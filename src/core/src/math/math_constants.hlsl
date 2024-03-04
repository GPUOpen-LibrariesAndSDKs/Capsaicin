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

#ifndef MATH_CONSTANTS_HLSL
#define MATH_CONSTANTS_HLSL

#ifndef QUARTER_PI   // pi/4
#define QUARTER_PI   0.78539816339744830961566084581988
#endif

#ifndef HALF_PI      // pi/2
#define HALF_PI      1.5707963267948966192313216916398
#endif

#ifndef PI           // pi
#define PI           3.1415926535897932384626433832795
#endif

#ifndef TWO_PI       // 2pi
#define TWO_PI       6.283185307179586476925286766559
#endif

#ifndef FOUR_PI      // 4pi
#define FOUR_PI      12.566370614359172953850573533118
#endif

#ifndef INV_TWO_PI   // 1/(2pi)
#define INV_TWO_PI   0.15915494309189533576888376337251
#endif

#ifndef INV_FOUR_PI  // 1/(4pi)
#define INV_FOUR_PI  0.07957747154594766788444188168626
#endif

#ifndef UINT_MAX
#define UINT_MAX     0xFFFFFFFF
#endif

#ifndef INT_MAX
#define INT_MAX      2147483647
#endif

#ifndef INT_MIN
#define INT_MIN      -2147483649
#endif

#ifndef FLT_MAX
#define FLT_MAX      3.402823466e+38
#endif

#ifndef FLT_MIN
#define FLT_MIN      1.175494351e-38
#endif

#ifndef FLT_EPSILON
#define FLT_EPSILON  1.192092896e-07
#endif

#ifndef DBL_MAX
#define DBL_MAX      1.7976931348623158e+308
#endif

#ifndef DBL_MIN
#define DBL_MIN      2.2250738585072014e-308
#endif

#ifndef DBL_EPSILON
#define DBL_EPSILON  2.2204460492503131e-016
#endif

#endif // MATH_CONSTANTS_HLSL
