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

#include "graph.h"

namespace Capsaicin
{

uint32_t Graph::getValueCount() const noexcept
{
    return static_cast<uint32_t>(values.size());
}

void Graph::addValue(float value) noexcept
{
    values[current] = value;
    current         = (current + 1) % static_cast<uint32_t>(values.size());
}

float Graph::getLastAddedValue() const noexcept
{
    if (current == 0) return getValueAtIndex(static_cast<uint32_t>(values.size() - 1));
    return getValueAtIndex(current - 1);
}

float Graph::getValueAtIndex(uint32_t index) const noexcept
{
    return values[index];
}

float Graph::getAverageValue() const noexcept
{
    double   runningCount = 0.0;
    uint32_t validFrames  = 0;
    for (uint32_t i = 0; i < getValueCount(); ++i)
    {
        runningCount += (double)getValueAtIndex(i);
        if (getValueAtIndex(i) != 0.0f)
        {
            ++validFrames;
        }
    }
    return static_cast<float>(runningCount / (double)validFrames);
}

void Graph::reset() noexcept
{
    current = 0;
    values.fill(0.0f);
}

float Graph::GetValueAtIndex(void *object, int32_t index) noexcept
{
    Graph const  &graph      = *static_cast<Graph const *>(object);
    const int32_t offset     = (int32_t)(graph.values.size()) - index;
    const int32_t newIndex   = (int32_t)(graph.current) - offset;
    const int32_t fixedIndex = (newIndex < 0 ? (int32_t)(graph.values.size()) + newIndex : newIndex);
    return graph.values[fixedIndex];
}
} // namespace Capsaicin
