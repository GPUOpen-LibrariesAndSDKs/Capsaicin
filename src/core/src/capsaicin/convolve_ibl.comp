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

RWTexture2DArray<float4> g_InEnvironmentBuffer;
RWTexture2DArray<float4> g_OutEnvironmentBuffer;

[numthreads(8, 8, 1)]
void BlurSky(in uint3 did : SV_DispatchThreadID)
{
    uint3 dims;
    g_InEnvironmentBuffer.GetDimensions(dims.x, dims.y, dims.z);

    if (any(did.xy >= max(dims.xy >> 1, 1)))
    {
        return; // out of bounds
    }

    float4 result = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float  weight = 0.0f;

    for (uint y = 0; y < 2; ++y)
    {
        for (uint x = 0; x < 2; ++x)
        {
            uint2 pix = (did.xy << 1) + uint2(x, y);

            if (any(pix >= dims.xy))
            {
                break;  // out of bounds
            }

            result += g_InEnvironmentBuffer[uint3(pix, did.z)];
            weight += 1.0f;
        }
    }

    g_OutEnvironmentBuffer[did] = (result / weight);
}
