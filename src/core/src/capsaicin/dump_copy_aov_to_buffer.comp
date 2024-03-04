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

uint2 g_BufferDimensions;
Texture2D<float4> g_DumpedBuffer;
RWBuffer<float> g_CopyBuffer;

[numthreads(8, 8, 1)]
void CopyAOVToBuffer(in uint2 did : SV_DispatchThreadID)
{
    uint2 input_index = did;

    if (any(input_index >= g_BufferDimensions))
    {
        return; // out of bounds
    }

    float4 color        = g_DumpedBuffer[input_index];
    uint   output_index = input_index.x + input_index.y * g_BufferDimensions.x;

    g_CopyBuffer[(output_index << 2) + 0] = color.x;
    g_CopyBuffer[(output_index << 2) + 1] = color.y;
    g_CopyBuffer[(output_index << 2) + 2] = color.z;
    g_CopyBuffer[(output_index << 2) + 3] = color.w;
}
