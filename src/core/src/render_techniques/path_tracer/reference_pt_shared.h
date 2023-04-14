/**********************************************************************
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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

#ifndef REFERENCE_PT_SHARED_H
#define REFERENCE_PT_SHARED_H

#include "../../gpu_shared.h"

struct RayCamera
{
    float3 origin;      /**< The ray starting position */
    float3 directionTL; /**< The direction to the top left of the virtual screen */
    float3 directionX;  /**< The virtual screens horizontal direction (length of 1 pixel - left->right)*/
    float3 directionY;  /**< The virtual screens vertical direction (length of 1 pixel - top->bottom)*/
    float2 range;       /**< The rays near and far distances */
};

#endif // REFERENCE_PT_SHARED_H
