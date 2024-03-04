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

#ifndef PATH_TRACING_SHARED_H
#define PATH_TRACING_SHARED_H

#include "../gpu_shared.h"

#ifdef __cplusplus
#    pragma warning(push)
#    pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif
struct RayCamera
{
    float3 origin;      /**< The ray starting position */
    float3 directionTL; /**< The direction to the top left of the virtual screen */
    float3 directionX;  /**< The virtual screens horizontal direction (length of 1 pixel - left->right)*/
    float3 directionY;  /**< The virtual screens vertical direction (length of 1 pixel - top->bottom)*/
    float2 range;       /**< The rays near and far distances */
};

struct Camera
{
    float3 origin;
    float3 lookAt;
    float3 up;

    float aspect;
    float fovY;
    float nearZ;
    float farZ;
};
#ifdef __cplusplus
#    pragma warning(pop)
#endif

/*
 * Converts a camera description to corresponding ray generation camera format.
 * @param camera The camera to convert.
 * @param width  The screen width.
 * @param height The screen height.
 * @returns The created ray camera.
 */
static inline RayCamera caclulateRayCamera(Camera camera, uint32_t width, uint32_t height)
{
    float3 origin = camera.origin;
    float2 range  = float2(camera.nearZ, camera.farZ);

    // Get the size of the screen in the X and Y screen direction
    float size = tan(camera.fovY / 2.0f);
    size *= range.x;
    float sizeHalfX = size * camera.aspect;
    float sizeHalfY = size;

    // Generate view direction
    float3 forward = camera.lookAt - origin;
    forward        = normalize(forward);
    // Generate proper horizontal direction
    float3 right = cross(forward, camera.up);
    right        = normalize(right);
    // Generate proper up direction
    float3 down = cross(forward, right);
    // Normalize vectors
    down = normalize(down);

    // Set each of the camera vectors to an orthonormal basis
    float3 directionX = right;
    float3 directionY = down;
    float3 directionZ = forward;

    // Get weighted distance vector
    directionZ = directionZ * range.x;

    // Get the Scaled Horizontal and up vectors
    directionX *= sizeHalfX;
    directionY *= sizeHalfY;

    // Offset the direction vector
    float3 directionTL = directionZ - directionX - directionY;

    // Scale the direction X and Y vectors from half size
    directionX += directionX;
    directionY += directionY;

    // Scale the X and Y vectors to be pixel length
    directionX /= (float)width;
    directionY /= (float)height;

    RayCamera ret = {origin, directionTL, directionX, directionY, range};
    return ret;
}

#ifndef __cplusplus
/**
 * Generate a primary ray originating from the camera for a given pixel.
 * @param pixel Requested pixel (pixel center is at 0.5 +-0.5)
 * @param rayCamera Camera raytracing parameters.
 * @return The generated ray.
 */
RayDesc generateCameraRay(float2 pixel, in RayCamera rayCamera)
{
    // Setup the ray
    RayDesc ray;

    // Get direction from origin to current pixel in screen plane
    float3 direction =
        (pixel.x * rayCamera.directionX) + (pixel.y * rayCamera.directionY) + rayCamera.directionTL;

    // Set the ray origin
    ray.Origin = rayCamera.origin;

    // Compute the ray direction for this pixel
    ray.Direction = normalize(direction);

    // Get adjusted range values
    ray.TMin = rayCamera.range.x;
    ray.TMax = rayCamera.range.y;

    return ray;
}
#endif

#endif // PATH_TRACING_SHARED_H
