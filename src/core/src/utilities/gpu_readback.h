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

#include "gpu_shared.h"

#include <gfx.h>

namespace Capsaicin
{
class CapsaicinInternal;

/** A helper utility class to reading back information from the GPU. */
class GPUReadback
{
public:
    /** Default constructor. */
    GPUReadback() noexcept;

    /** Destructor. */
    ~GPUReadback() noexcept;

    GPUReadback(GPUReadback const &other)                = delete;
    GPUReadback(GPUReadback &&other) noexcept            = delete;
    GPUReadback &operator=(GPUReadback const &other)     = delete;
    GPUReadback &operator=(GPUReadback &&other) noexcept = delete;

    /**
     * Reads back the GPU data.
     * @tparam TYPE Generic type of the requested data.
     * @param capsaicin Current framework context.
     * @param buffer The buffer to be read.
     * @return The requested buffer data.
     */
    template<typename TYPE>
    TYPE readback(CapsaicinInternal const &capsaicin, GfxBuffer const &buffer) noexcept
    {
        return *static_cast<TYPE const *>(readback(capsaicin, buffer));
    }

    /**
     * Reads back the GPU data.
     * @param capsaicin Current framework context.
     * @param buffer The buffer to be read.
     * @return The requested buffer data.
     */
    void const *readback(CapsaicinInternal const &capsaicin, GfxBuffer const &buffer) noexcept;

    /**
     * Check whether some readback information is available.
     * @return true if new readback information is available.
     */
    [[nodiscard]] bool hasReadback(CapsaicinInternal const &capsaicin) const noexcept;

    /** Reset the readback buffers. */
    void reset() noexcept;

    /** Clears the readback buffers. */
    void clear() noexcept;

private:
    GfxContext gfx_; //

    uint32_t readback_frame_index_;  //
    uint32_t readback_buffer_index_; //

    GfxBuffer readback_buffers_[kGfxConstant_BackBufferCount]; //
    void     *readback_buffer_data_;                           //
};
} // namespace Capsaicin
