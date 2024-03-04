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

Texture2D VelocityBuffer;

float4 main(in float4 pos : SV_Position) : SV_Target
{
    float2 velocity = VelocityBuffer.Load(int3(pos.xy, 0)).xy;
    float3 velocity_vec = float3((velocity.y >= 0.0f ? velocity.y : 0.0f) + (velocity.y < 0.0f ? -velocity.y : 0.0f),
            (velocity.x < 0.0f ? -velocity.x : 0.0f) + (velocity.y < 0.0f ? -velocity.y : 0.0f),
            (velocity.x >= 0.0f ? velocity.x : 0.0f));
    return float4(pow(velocity_vec, 0.4f), 1.0f);
}
