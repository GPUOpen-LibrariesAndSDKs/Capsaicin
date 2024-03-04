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

#ifndef GLOSSY_REFLECTIONS_HLSL
#define GLOSSY_REFLECTIONS_HLSL

//!
//! Glossy reflections shader bindings.
//!

RWTexture2D<float>  g_GlossyReflections_TextureFloat[]  : register(space94);
RWTexture2D<float4> g_GlossyReflections_TextureFloat4[] : register(space95);

#define             g_GlossyReflections_FirefliesBuffer       g_GlossyReflections_TextureFloat [GLOSSY_REFLECTION_FIREFLIES_BUFFER]
#define             g_GlossyReflections_SpecularBuffer        g_GlossyReflections_TextureFloat4[GLOSSY_REFLECTION_SPECULAR_BUFFER]
#define             g_GlossyReflections_DirectionBuffer       g_GlossyReflections_TextureFloat4[GLOSSY_REFLECTION_DIRECTION_BUFFER]
#define             g_GlossyReflections_ReflectionsBuffer     g_GlossyReflections_TextureFloat4[GLOSSY_REFLECTION_REFLECTION_BUFFER]
#define             g_GlossyReflections_StandardDevBuffer     g_GlossyReflections_TextureFloat4[GLOSSY_REFLECTION_STANDARD_DEV_BUFFER]
#define             g_GlossyReflections_ReflectionsBuffer0    g_GlossyReflections_TextureFloat4[GLOSSY_REFLECTION_REFLECTIONS_BUFFER_0     + g_GlossyReflectionsAtrousConstants.ping_pong]
#define             g_GlossyReflections_AverageSquaredBuffer0 g_GlossyReflections_TextureFloat4[GLOSSY_REFLECTION_AVERAGE_SQUARED_BUFFER_0 + g_GlossyReflectionsAtrousConstants.ping_pong]
#define             g_GlossyReflections_ReflectionsBuffer1    g_GlossyReflections_TextureFloat4[GLOSSY_REFLECTION_REFLECTIONS_BUFFER_1     - g_GlossyReflectionsAtrousConstants.ping_pong]
#define             g_GlossyReflections_AverageSquaredBuffer1 g_GlossyReflections_TextureFloat4[GLOSSY_REFLECTION_AVERAGE_SQUARED_BUFFER_1 - g_GlossyReflectionsAtrousConstants.ping_pong]
#define             g_GlossyReflections_ReflectionsBufferX    g_GlossyReflections_ReflectionsBuffer0        // BE CAREFUL: aliases...
#define             g_GlossyReflections_AverageSquaredBufferX g_GlossyReflections_AverageSquaredBuffer0     //

RWStructuredBuffer<uint> g_GlossyReflections_RtSampleBuffer;
RWStructuredBuffer<uint> g_GlossyReflections_RtSampleCountBuffer;

//!
//! Glossy reflections common functions.
//!

// 
int  GlossyReflections_FullRadius()
{
    return g_GlossyReflectionsConstants.half_res
        ? g_GlossyReflectionsConstants.half_radius      // BECAREFUL: Full res pixel count for half res
        : g_GlossyReflectionsConstants.full_radius;
}

//
int  GlossyReflections_MarkFireflies_FullRadius()
{
    return g_GlossyReflectionsConstants.half_res
        ? g_GlossyReflectionsConstants.mark_fireflies_half_radius      // BECAREFUL: Full res pixel count for half res
        : g_GlossyReflectionsConstants.mark_fireflies_full_radius;
}

//
float GlossyReflections_MarkFireflies_LowThreshold()
{
    return g_GlossyReflectionsConstants.half_res
        ? g_GlossyReflectionsConstants.mark_fireflies_half_low_threshold
        : g_GlossyReflectionsConstants.mark_fireflies_full_low_threshold;
}

//
float GlossyReflections_MarkFireflies_HighThreshold()
{
    return g_GlossyReflectionsConstants.half_res
        ? g_GlossyReflectionsConstants.mark_fireflies_half_high_threshold
        : g_GlossyReflectionsConstants.mark_fireflies_full_high_threshold;
}

//
int  GlossyReflections_CleanupFireflies_FullRadius()
{
    return g_GlossyReflectionsConstants.half_res
        ? g_GlossyReflectionsConstants.cleanup_fireflies_half_radius      // BECAREFUL: Full res pixel count for half res
        : g_GlossyReflectionsConstants.cleanup_fireflies_full_radius;
}

//
int2 GlossyReflections_HalfRes()
{
    return g_GlossyReflectionsConstants.half_res
        ? (g_GlossyReflectionsConstants.full_res + 1) >> 1
        : (g_GlossyReflectionsConstants.full_res);
}

//
int2 GlossyReflections_FullRes()
{
    return g_GlossyReflectionsConstants.full_res;
}

//
int2 GlossyReflections_SplitRes()
{
    return g_GlossyReflectionsConstants.half_res
        ? (g_GlossyReflectionsConstants.full_res + int2(0, 1)) >> int2(0, 1)
        : (g_GlossyReflectionsConstants.full_res);
}

// Maps the input 2D coordinate from half resolution to the full resolution sample.
int2 GlossyReflections_HalfToFullRes(in int2 half_pos)
{
    return g_GlossyReflectionsConstants.half_res
        ? (half_pos << int2(1, 1)) + int2((g_FrameIndex >> 0) & 1,
                                          (g_FrameIndex >> 1) & 1)
        : (half_pos);
}

// 
int2 GlossyReflections_SplitToFullRes(in int2 split_pos)
{
    return g_GlossyReflectionsConstants.half_res
        ? (split_pos << int2(0, 1))
        : (split_pos);
}

// 
int2 GlossyReflections_FullToHalfRes(in int2 full_pos)
{
    return g_GlossyReflectionsConstants.half_res ? full_pos >> 1 : full_pos;
}

// 
int  GlossyReflections_FullToHalfRadius(in int full_radius)
{
    return g_GlossyReflectionsConstants.half_res ? (full_radius + 1) >> 1 : full_radius;
}

// 
bool GlossyReflections_QueueSample(in int2 full_pos)
{
    return g_GlossyReflectionsConstants.half_res ? all(GlossyReflections_HalfToFullRes(full_pos >> 1) == full_pos) : true;
}

// 
uint GlossyReflections_PackSample(in uint2 seed)
{
    return ((seed.y & 0xFFFF) <<  0) |
           ((seed.x & 0xFFFF) << 16);
}

//
uint2 GlossyReflections_UnpackSample(in uint packed_sample)
{
    return uint2((packed_sample >> 16) & 0xFFFF,
                 (packed_sample >>  0) & 0xFFFF);
}

// 
float GlossyReflections_NeighborhoodFilter(in float i, in float radius)
{
    const float k = 3.0f;
    return exp(-k * (i * i) / pow(radius + 1.0f, 2.0f));
}

#endif // GLOSSY_REFLECTIONS_HLSL
