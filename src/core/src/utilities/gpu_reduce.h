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

/** A helper utility class to perform GPU accelerate reduce operations on input data arrays. */
class GPUReduce
{
public:
    /** Defaulted constructor. */
    GPUReduce() noexcept = default;

    /** Destructor. */
    ~GPUReduce() noexcept;

    /** Type of value to reduce. */
    enum class Type : uint32_t
    {
        Float = 0,
        Float2,
        Float3,
        Float4,
        UInt,
        UInt2,
        UInt3,
        UInt4,
        Int,
        Int2,
        Int3,
        Int4,
        Double,
        Double2,
        Double3,
        Double4,
    };

    /** Type of reduce operation to perform. */
    enum class Operation
    {
        Sum,
        Min,
        Max,
        Product,
    };

    /**
     * Initialise the internal data based on current configuration.
     * @param gfx        Active gfx context.
     * @param shaderPath Path to shader files based on current working directory.
     * @param type       The object type to reduce.
     * @param operation  The type of operation to perform.
     * @return True, if any initialisation/changes succeeded.
     */
    bool initialise(
        GfxContext gfx, std::string_view const &shaderPath, Type type, Operation operation) noexcept;

    /**
     * Initialise the internal data based on current configuration.
     * @param capsaicin Current framework context.
     * @param type      The object type to reduce.
     * @param operation The type of operation to perform.
     * @return True, if any initialisation/changes succeeded.
     */
    bool initialise(CapsaicinInternal const &capsaicin, Type type, Operation operation) noexcept;

    /**
     * Reduce a list of keys using indirect execution.
     * @param sourceBuffer The buffer containing the keys to reduce.
     * @param numKeys      A buffer containing the number of keys in the source buffer.
     * @param maxNumKeys   Value containing the number of keys in the source buffer, if exact value is unknown
     *  then this should be the maximum possible number of values in the source.
     * @return True, if operation succeeded.
     */
    bool reduceIndirect(GfxBuffer const &sourceBuffer, GfxBuffer const &numKeys, uint maxNumKeys) noexcept;

    /**
     * Reduce a list of keys using selected operation.
     * @param sourceBuffer The buffer containing the keys to reduce.
     * @param numKeys      Value containing the number of keys in the source buffer.
     * @return True, if operation succeeded.
     */
    bool reduce(GfxBuffer const &sourceBuffer, uint numKeys) noexcept;

private:
    /** Terminates and cleans up this object. */
    void terminate() noexcept;

    /**
     * Internal reduce implementation used to handle multiple reduce cases.
     * @param sourceBuffer The buffer containing the keys to reduce.
     * @param maxNumKeys   Value containing the number of keys in the source buffer, if using indirect
     *  execution and exact value is unknown then this should be the maximum possible number of values in the
     *  source.
     * @param numKeys      (Optional) If non-null, a buffer containing the number of keys in the source buffer
     *  used for indirect execution. If null then @maxNumKeys is used instead.
     * @return True, if operation succeeded.
     */
    bool reduceInternal(
        GfxBuffer const &sourceBuffer, uint maxNumKeys, GfxBuffer const *numKeys = nullptr) noexcept;

    GfxContext gfx;

    Type       currentType      = Type::Float;
    Operation  currentOperation = Operation::Sum;
    GfxBuffer  scratchBuffer;
    GfxBuffer  indirectBuffer;
    GfxBuffer  indirectBuffer2;
    GfxBuffer  indirectCountBuffer;
    GfxBuffer  indirectCountBuffer2;
    GfxProgram reduceProgram;
    GfxKernel  reduceKernel;
    GfxKernel  reduceIndirectKernel;
    GfxKernel  dispatchIndirectKernel;
};
} // namespace Capsaicin
