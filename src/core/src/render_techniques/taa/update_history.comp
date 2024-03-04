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
#include "../../gpu_shared.h"

#ifdef HAS_DIRECT_LIGHTING_BUFFER
Texture2D g_DirectLightingBuffer;
#endif
#ifdef HAS_GLOBAL_ILLUMINATION_BUFFER
Texture2D g_GlobalIlluminationBuffer;
#endif

RWTexture2D<float4> g_PrevCombinedIlluminationBuffer;

[numthreads(8, 8, 1)]
void UpdateHistory(in uint2 did : SV_DispatchThreadID)
{
    float3 colour = 0.0f.xxx;
#ifdef HAS_DIRECT_LIGHTING_BUFFER
    colour += g_DirectLightingBuffer.Load(int3(did, 0)).xyz;
#endif
#ifdef HAS_GLOBAL_ILLUMINATION_BUFFER
    colour += g_GlobalIlluminationBuffer.Load(int3(did, 0)).xyz;
#endif

    g_PrevCombinedIlluminationBuffer[did] = float4(colour, 1.0f);
}
