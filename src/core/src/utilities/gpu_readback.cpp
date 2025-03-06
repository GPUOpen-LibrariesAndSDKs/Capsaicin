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
#include "gpu_readback.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
GPUReadback::GPUReadback() noexcept
    : readback_frame_index_(0)
    , readback_buffer_index_(0)
    , readback_buffer_data_(nullptr)
{}

GPUReadback::~GPUReadback() noexcept
{
    clear();
}

void const *GPUReadback::readback(CapsaicinInternal const &capsaicin, GfxBuffer const &buffer) noexcept
{
    if (!*readback_buffers_ || readback_buffers_->getSize() != buffer.getSize())
    {
        gfx_ = capsaicin.getGfx();

        free(readback_buffer_data_);
        readback_buffer_data_ = malloc(buffer.getSize());
        if (readback_buffer_data_ == nullptr)
        {
            return nullptr;
        }
        memset(readback_buffer_data_, 0, buffer.getSize());

        for (uint32_t i = 0; i < ARRAYSIZE(readback_buffers_); ++i)
        {
            char name[64];
            GFX_SNPRINTF(name, sizeof(name), "Capsaicin_ReadbackBuffer%u", i);

            gfxDestroyBuffer(gfx_, readback_buffers_[i]);

            readback_buffers_[i] =
                gfxCreateBuffer(gfx_, buffer.getSize(), readback_buffer_data_, kGfxCpuAccess_Read);
            readback_buffers_[i].setStride(buffer.getStride());
            readback_buffers_[i].setName(name);

            readback_frame_index_  = capsaicin.getFrameIndex() - 1;
            readback_buffer_index_ = 0; // reset cursor
        }
    }

    if (hasReadback(capsaicin)) // only read back if we've advanced to a new frame
    {
        GfxBuffer const readback_buffer =
            readback_buffers_[readback_buffer_index_ % ARRAYSIZE(readback_buffers_)];

        if (readback_buffer_index_ >= ARRAYSIZE(readback_buffers_))
        {
            memcpy(readback_buffer_data_, gfxBufferGetData(gfx_, readback_buffer), buffer.getSize());
        }

        gfxCommandCopyBuffer(gfx_, readback_buffer, buffer);

        if (++readback_buffer_index_ >= 2 * ARRAYSIZE(readback_buffers_))
        {
            readback_buffer_index_ -= ARRAYSIZE(readback_buffers_);
        }

        readback_frame_index_ = capsaicin.getFrameIndex();
    }

    return readback_buffer_data_;
}

bool GPUReadback::hasReadback(CapsaicinInternal const &capsaicin) const noexcept
{
    return readback_frame_index_ != capsaicin.getFrameIndex();
}

void GPUReadback::reset() noexcept
{
    readback_buffer_index_ = 0; // reset cursor

    memset(readback_buffer_data_, 0, readback_buffers_->getSize());
}

void GPUReadback::clear() noexcept
{
    free(readback_buffer_data_);
    readback_buffer_data_ = nullptr;

    for (GfxBuffer &readback_buffer : readback_buffers_)
    {
        gfxDestroyBuffer(gfx_, readback_buffer);
        readback_buffer = {};
    }
}
} // namespace Capsaicin
