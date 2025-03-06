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

#include "visibility_buffer_shared.h"

ConstantBuffer<DrawConstants> g_VBConstants;

float4 main(VertexParams params, uint primitiveID : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) : SV_Target
{
    float3 albedo = float3(0.06f, 0.14f, 0.7f);
    float3 deltas = fwidth(barycentrics);
    float3 smoothing = deltas * 0.6f;
    float3 thickness = deltas * 0.4f;
    float3 wire = smoothstep(thickness, thickness + smoothing, barycentrics);
    float minWire = min(wire.x, min(wire.y, wire.z));
    float3 colour = lerp(float3(1.0f, 1.0f, 1.0f), albedo, minWire);
    return float4(colour, 1.0f);
}
