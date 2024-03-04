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
#ifndef SSGI_GPU_SHARED_H
#define SSGI_GPU_SHARED_H

#ifdef __cplusplus
#    include <glm/gtx/compatibility.hpp>
#    include <glm/gtx/type_aligned.hpp>
namespace
{
using namespace glm;
#endif // __cplusplus

struct SSGIConstants
{
    float4x4 view;
    float4x4 proj;
    float4x4 inv_view;
    float4x4 inv_proj;
    float4x4 inv_view_proj;
    float4   eye;
    float4   forward;
    int2     buffer_dimensions;
    int      frame_index;
    int      slice_count;
    int      step_count;
    float    uv_radius;
    float    view_radius;
    float    falloff_mul;
    float    falloff_add;
};

#ifdef __cplusplus
}
#endif

#endif // SSGI_GPU_SHARED_H
