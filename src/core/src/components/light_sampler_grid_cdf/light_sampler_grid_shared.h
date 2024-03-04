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

#ifndef LIGHT_SAMPLER_GRID_SHARED_H
#define LIGHT_SAMPLER_GRID_SHARED_H

#include "../../gpu_shared.h"

struct LightSamplingConstants
{
    uint maxCellsPerAxis;
    uint maxNumLightsPerCell;
};

#ifdef __cplusplus
#    pragma warning(push)
#    pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif
struct LightSamplingConfiguration
{
    uint4 numCells; /*< Number of cells in the x,y,z directions respectively */
    /*< 4th value contains maximum number of lights to store per cell (must be no more than numLights) */
    float3 cellSize; /*< Size of each cell in the x,y,z directions respectively */
#ifndef __cplusplus
    float pad;
#endif
    float3 sceneMin; /*< World space position of the first cell in the x,y,z directions respectively */
#ifndef __cplusplus
    float pad2;
#endif
    float3 sceneExtent; /*< World space size of the scene bounding box (sceneMax - sceneMin) */
};
#ifdef __cplusplus
#pragma warning(pop)
#endif

#endif
