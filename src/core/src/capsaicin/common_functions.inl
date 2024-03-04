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
#pragma once

namespace Capsaicin
{
static constexpr float const kPi = 3.14159265358979323846f;

inline float DegreesToRadians(float degrees)
{
    return degrees * kPi / 180.0f;
}

inline float RadiansToDegrees(float radians)
{
    return radians * 180.0f / kPi;
}

inline bool IsPowerOfTwo(uint32_t value)
{
    return (value & (value - 1)) == 0;
}

inline float CalculateHaltonNumber(uint32_t index, uint32_t base)
{
    float f = 1.0f, result = 0.0f;
    for (uint32_t i = index; i > 0;)
    {
        f /= base;
        result = result + f * (i % base);
        i      = (uint32_t)(i / (float)base);
    }
    return result;
}

inline void CalculateTransformedBounds(glm::vec3 const &min_bounds, glm::vec3 const &max_bounds,
    glm::mat4 const &transform, glm::vec3 &out_min_bounds, glm::vec3 &out_max_bounds)
{
    glm::vec3 const bounds_size = max_bounds - min_bounds;

    glm::vec3 const edgeX = glm::vec3(transform[0] * bounds_size.x),
                    edgeY = glm::vec3(transform[1] * bounds_size.y),
                    edgeZ = glm::vec3(transform[2] * bounds_size.z);

    glm::vec3 const point0 = glm::vec3(transform * glm::vec4(min_bounds, 1.0f)), point1 = point0 + edgeX,
                    point2 = point0 + edgeY, point3 = point1 + edgeY, point4 = point0 + edgeZ,
                    point5 = point1 + edgeZ, point6 = point2 + edgeZ, point7 = point3 + edgeZ;

    out_min_bounds = glm::min(glm::min(glm::min(point0, point1), glm::min(point2, point3)),
        glm::min(glm::min(point4, point5), glm::min(point6, point7)));
    out_max_bounds = glm::max(glm::max(glm::max(point0, point1), glm::max(point2, point3)),
        glm::max(glm::max(point4, point5), glm::max(point6, point7)));
}
} // namespace Capsaicin
