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

#ifndef VISIBILITY_BUFFER_SHARED_H
#define VISIBILITY_BUFFER_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus
struct VertexParams
{
    float4 position : SV_Position;
#    if defined(HAS_SHADING_NORMAL) || defined(HAS_VERTEX_NORMAL)
    float3 normal : NORMAL;
#    endif
    float2 uv : TEXCOORD;
    float3 world : POSITION0;
    float4 current : POSITION1;
    float4 previous : POSITION2;
};

struct PrimParams
{
    uint primitiveIndex : SV_PrimitiveID;
    uint instanceID : INSTANCE_ID;
    uint materialID : MATERIAL_ID;
    bool cullPrimitive : SV_CullPrimitive;
};

#    define MESHPAYLOADSIZE 32

struct MeshPayload
{
    uint meshletIDs[MESHPAYLOADSIZE];
    uint instanceIDs[MESHPAYLOADSIZE];
};
#endif

struct DrawData
{
    uint meshletIndex;
    uint instanceIndex;
};

struct DrawConstants
{
    float3   cameraPosition;
    uint     drawCount;
    float4   cameraFrustum[6];
    float4x4 viewProjection;
    float4x4 prevViewProjection;
    float3x4 view;
    uint2    dimensions;
    float2   projection0011;
    float    nearZ;
};

struct DrawConstantsRT
{
    float4x4 viewProjection;
    float4x4 prevViewProjection;
    uint2    dimensions;
    float2   jitter;
};

#endif
