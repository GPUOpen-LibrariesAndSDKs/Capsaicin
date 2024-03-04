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
#ifndef GPU_SHARED_H
#define GPU_SHARED_H

#ifdef __cplusplus

#    include <glm/gtx/compatibility.hpp>
#    include <glm/gtx/type_aligned.hpp>

typedef uint32_t uint;

typedef glm::ivec2         int2;
typedef glm::aligned_ivec3 int3;
typedef glm::ivec4         int4;

typedef glm::uvec2         uint2;
typedef glm::aligned_uvec3 uint3;
typedef glm::uvec4         uint4;

typedef glm::i8vec2         byte2;
typedef glm::aligned_i8vec3 byte3;
typedef glm::i8vec4         byte4;

typedef glm::u8vec2         ubyte2;
typedef glm::aligned_u8vec3 ubyte3;
typedef glm::u8vec4         ubyte4;

typedef glm::i16vec2         short2;
typedef glm::aligned_i16vec3 short3;
typedef glm::i16vec4         short4;

typedef glm::u16vec2         ushort2;
typedef glm::aligned_u16vec3 ushort3;
typedef glm::u16vec4         ushort4;

typedef glm::aligned_vec4 float4;
typedef glm::aligned_vec3 float3;
typedef glm::aligned_vec2 float2;

typedef glm::dvec4 double4;
typedef glm::dvec3 double3;
typedef glm::dvec2 double2;

typedef glm::bool3 bool3;

typedef glm::mat4   float4x4;
typedef glm::mat4x3 float3x4;
typedef glm::mat3   float3x3;

#    define SEMANTIC(X)

namespace Capsaicin
{
#else // __cplusplus

#    define SEMANTIC(X) : X

#endif // __cplusplus

struct DispatchCommand
{
    uint num_groups_x;
    uint num_groups_y;
    uint num_groups_z;
    uint padding;
};

struct DrawCommand
{
    uint count;
    uint first_index;
    uint base_vertex;
    uint padding;
};

struct GpuVirtualAddressRange
{
    uint64_t start_address;
    uint64_t size_in_bytes;
};

struct GpuVirtualAddressRangeAndStride
{
    uint64_t start_address;
    uint64_t size_in_bytes;
    uint64_t stride_in_bytes;
};

struct DispatchRaysCommand
{
    GpuVirtualAddressRange          ray_generation_shader_record;
    GpuVirtualAddressRangeAndStride miss_shader_table;
    GpuVirtualAddressRangeAndStride hit_group_table;
    GpuVirtualAddressRangeAndStride callable_shader_table;
    uint                            width;
    uint                            height;
    uint                            depth;
    uint                            padding[3];
};

struct Instance
{
    uint mesh_index;
    uint material_index;
    uint transform_index;
};

struct Material
{
    float4 albedo;     // .xyz = albedo, .w = albedo_map
    float4 emissivity; // .xyz = emissivity, .w = emissivity_map
    float4
        metallicity_roughness; // .x = metallicity, .y = metallicity_map, .z = roughness, .w = roughness_map
    float4 normal_alpha_side;  // .x = normal_map, .y = alpha, .z = double_sided, .w = unused padding
};

struct Mesh
{
    uint vertex_offset_idx;
    uint index_offset_idx;
    uint index_count;
};

struct Vertex
{
    float4 position SEMANTIC(POSITION);
    float4 normal   SEMANTIC(NORMAL);
    float2 uv       SEMANTIC(TEXCOORDS);
    float2 unused   SEMANTIC(UNUSED);
};

struct CameraMatrices
{
    float4x4 view;
    float4x4 view_prev;
    float4x4 inv_view;
    float4x4 projection;
    float4x4 projection_prev;
    float4x4 inv_projection;
    float4x4 view_projection;
    float4x4 view_projection_prev;
    float4x4 inv_view_projection_prev;
    float4x4 inv_view_projection;
    float4x4 reprojection;
};

#ifdef __cplusplus
} // namespace Capsaicin
#endif // __cplusplus

#include "lights/lights_shared.h"

#endif // GPU_SHARED_H
