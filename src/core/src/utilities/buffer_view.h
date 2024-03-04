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

#include <gfx.h>

namespace Capsaicin
{
/**
 * RAII wrapper around buffer views.
 * @tparam T Generic type parameter of objects stored within buffer.
 */
template<typename T>
class BufferView
{
public:
    GfxBuffer  buffer; /**< The buffer holding the current view */
    GfxContext gfx;

    /** Defaulted constructor. */
    BufferView() noexcept = default;

    /**
     * Construct a new view into an existing buffer.
     * @param context       The gfx context the created the input buffer.
     * @param bufferIn      The input to create a view within.
     * @param elementOffset The element offset into input buffer to start new buffer view.
     * @param count         Number of elements from input to add to new buffer view.
     */
    inline BufferView(
        GfxContext const &context, GfxBuffer const &bufferIn, uint elementOffset, uint count) noexcept
        : buffer(gfxCreateBufferRange<T>(context, bufferIn, elementOffset, count))
        , gfx(context)
    {
        std::string       name = bufferIn.getName();
        const std::string number(std::to_string(elementOffset));
        if ((name.length() + number.length() + 1) > kGfxConstant_MaxNameLength)
        {
            name.resize(-1LL - number.length() + kGfxConstant_MaxNameLength);
        }
        name.append(number);
        buffer.setName(name.data());
    }

    /** Destructor. */
    inline ~BufferView() noexcept { gfxDestroyBuffer(gfx, buffer); }
};
} // namespace Capsaicin
