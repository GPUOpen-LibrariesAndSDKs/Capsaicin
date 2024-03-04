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

#include <array>

namespace Capsaicin
{
class Graph
{
public:
    Graph() noexcept = default;

    uint32_t getValueCount() const noexcept;
    void     addValue(float value) noexcept;
    float    getLastAddedValue() const noexcept;
    float    getValueAtIndex(uint32_t index) const noexcept;
    float    getAverageValue() const noexcept;
    void     reset() noexcept;

    static float GetValueAtIndex(void *object, int32_t index) noexcept;

private:
    uint32_t               current = 0;     /**< The current cursor into values circular buffer */
    std::array<float, 256> values  = {0.0}; /**< The stored list of values */
};
} // namespace Capsaicin
