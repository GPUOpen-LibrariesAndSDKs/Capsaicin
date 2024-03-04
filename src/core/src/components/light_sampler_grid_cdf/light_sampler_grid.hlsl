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

#ifndef LIGHT_SAMPLER_GRID_HLSL
#define LIGHT_SAMPLER_GRID_HLSL

namespace LightSamplerGrid
{
    /**
     * Calculate the start index for a requested cell within a continuous 1D buffer.
     * @param cell Index of requested cell (0-indexed).
     * @return The index of the cell.
     */
    uint getCellIndex(uint3 cell)
    {
        const uint3 numCells = g_LightSampler_Configuration[0].numCells.xyz;
        const uint maxLightsPerCell = g_LightSampler_Configuration[0].numCells.w;
        uint index = cell.x + numCells.x * (cell.y + numCells.y * cell.z);
        index *= maxLightsPerCell;
        return index;
    }

    /**
     * Calculate the start index for a requested cell within a continuous 1D buffer when using octahedron sampling.
     * @param cell Index of requested cell (0-indexed).
     * @param face Face of octahedron based on shading normal.
     * @return The index of the cell.
     */
    uint getCellOctaIndex(uint3 cell, uint face)
    {
        const uint3 numCells = g_LightSampler_Configuration[0].numCells.xyz;
        const uint maxLightsPerCell = g_LightSampler_Configuration[0].numCells.w;
        uint index = cell.x + numCells.x * (cell.y + numCells.y * cell.z);
        index *= 8;
        index += face;
        index *= maxLightsPerCell;
        return index;
    }

    /**
     * Calculate which octahedron face a given direction is oriented within.
     * @param normal The direction vector.
     * @return The index of the octahedron face.
     */
    uint getCellFace(float3 normal)
    {
        // Faces are mapped to the integer values 0..7 where the 3 bits that
        //  make up the integer value are taken from the sign bits of the normal direction
        //  with the x component mapping to bit 0, y component to bit 1 and z to bit 2
        bool3 faceOrientation = normal < 0.0f.xxx;
        uint index = faceOrientation.x ? 0x1 : 0;
        index += faceOrientation.y ? 0x2 : 0;
        index += faceOrientation.z ? 0x4 : 0;
        return index;
    }

    /**
    * Calculate the octahedron face normal.
    * @param cellFace The cell face to get normal for.
    * @return The normalised normal direction.
    */
    float3 getCellNormal(uint cellFace)
    {
        const float3 faceNormal = normalize(float3(cellFace & 0x1 ? -1.0f : 1.0f,
        cellFace & 0x2 ? -1.0f : 1.0f,
        cellFace & 0x4 ? -1.0f : 1.0f));
        return faceNormal;
    }

    /**
     * Calculate which cell a position falls within.
     * @param position The world space position.
     * @return The index of the grid cell.
     */
    uint3 getCellFromPosition(float3 position)
    {
        const float3 numCells = (float3)g_LightSampler_Configuration[0].numCells.xyz;
        float3 relativePos = position - g_LightSampler_Configuration[0].sceneMin;
        relativePos /= g_LightSampler_Configuration[0].sceneExtent;
        const uint3 cell = clamp(floor(relativePos * numCells), 0.0f, numCells - 1.0f.xxx);
        return cell;
    }

    /**
     * Calculate which cell a position falls within for a given jittered position.
     * @note The current position will be jittered by +-quarter the current cell size.
     * @param position The world space position.
     * @tparam RNG The type of random number sampler to be used.
     * @return The index of the grid cell.
     */
    template<typename RNG>
    uint3 getCellFromJitteredPosition(float3 position, inout RNG randomNG)
    {
        // Jitter current position by +-quarter cell size
        position += (randomNG.rand3() - 0.25f) * g_LightSampler_Configuration[0].cellSize;

        return getCellFromPosition(position);
    }

    /**
     * Get the current index into the light sampling cell buffer for a given position.
     * @param position Current position on surface.
     * @return The index of the current cell.
     */
    uint getCellIndexFromPosition(float3 position)
    {
        // Calculate which cell we are in based on input point
        const uint3 cell = getCellFromPosition(position);

        // Calculate position of current cell in output buffer
        const uint cellIndex = getCellIndex(cell);
        return cellIndex;
    }

    /**
     * Get the current index into the light sampling octahedron cell buffer for a given position.
     * @param position Current position on surface.
     * @param normal   Shading normal vector at current position.
     * @return The index of the current cell.
     */
    uint getCellOctaIndexFromPosition(float3 position, float3 normal)
    {
        // Calculate which cell we are in based on input point
        const uint3 cell = getCellFromPosition(position);

        // Calculate position of current cell in output buffer
        const uint cellIndex = getCellOctaIndex(cell, getCellFace(normal));
        return cellIndex;
    }

    /**
     * Get the bounding box for a specific grid cell.
     * @param cellID The ID of the grid cell.
     * @param extent (Out) The return bounding box size.
     * @return The bounding box min values.
     */
    float3 getCellBB(uint3 cellID, out float3 extent)
    {
        const float3 minBB = ((float3)cellID * g_LightSampler_Configuration[0].cellSize) + g_LightSampler_Configuration[0].sceneMin;
        extent = g_LightSampler_Configuration[0].cellSize;
        return minBB;
    }

    /**
     * Get the current index into the light sampling cell buffer for a given jittered position.
     * @note The current position will be jittered by +-quarter the current cell size.
     * @tparam RNG The type of random number sampler to be used.
     * @param position Current position on surface.
     * @param randomNG The random number generator.
     * @return The index of the current cell.
     */
    template<typename RNG>
    uint getCellIndexFromJitteredPosition(float3 position, inout RNG randomNG)
    {
        // Jitter current position by +-quarter cell size
        position += (randomNG.rand3() - 0.25f) * g_LightSampler_Configuration[0].cellSize;

        return getCellIndexFromPosition(position);
    }

    /**
     * Get the current index into the light sampling octahedron cell buffer for a given jittered position.
     * @note The current position will be jittered by +-quarter the current cell size.
     * @tparam RNG The type of random number sampler to be used.
     * @param position Current position on surface.
     * @param normal   Shading normal vector at current position.
     * @param randomNG The random number generator.
     * @return The index of the current cell.
     */
    template<typename RNG>
    uint getCellOctaIndexFromJitteredPosition(float3 position, float3 normal, inout RNG randomNG)
    {
        // Jitter current position by +-quarter cell size
        position += (randomNG.rand3() - 0.25f) * g_LightSampler_Configuration[0].cellSize;

        return getCellOctaIndexFromPosition(position, normal);
    }
}

#endif // LIGHT_SAMPLER_GRID_HLSL
