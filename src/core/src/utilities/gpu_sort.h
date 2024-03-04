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

class GPUSort
{
public:
    GPUSort() noexcept = default;

    ~GPUSort() noexcept;

    /** Type of value to sort. */
    enum class Type : uint32_t
    {
        Float = 0, /* Does not support negative values */
        UInt,
    };

    /** Type of sort operation to perform. */
    enum class Operation
    {
        Ascending,
        Descending,
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
     * @param type      The object type to sort.
     * @param operation The type of operation to perform.
     * @return True, if any initialisation/changes succeeded.
     */
    bool initialise(CapsaicinInternal const &capsaicin, Type type, Operation operation) noexcept;

    /**
     * Sort a list of keys from smallest to largest using indirect execution.
     * @param sourceBuffer The buffer containing the keys to sort (only 32bit uint or float>=0 are supported).
     * @param numKeys      A buffer containing the number of keys in the source buffer.
     * @param maxNumKeys   Value containing the number of keys in the source buffer, if exact value is unknown
     *  then this should be the maximum possible number of values in the source.
     */
    void sortIndirect(GfxBuffer const &sourceBuffer, GfxBuffer const &numKeys, uint maxNumKeys) noexcept;

    /**
     * Sort a list of keys and associated payload from smallest to largest using indirect execution.
     * @param sourceBuffer  The buffer containing the keys to sort (only 32bit uint or float>=0 are
     * supported).
     * @param numKeys       A buffer containing the number of keys in the source buffer.
     * @param maxNumKeys    Value containing the number of keys in the source buffer, if exact value is
     * unknown then this should be the maximum possible number of values in the source.
     * @param sourcePayload The buffer containing the payload for each key (only 32bit payloads per key are
     *  supported).
     */
    void sortIndirectPayload(GfxBuffer const &sourceBuffer, GfxBuffer const &numKeys, uint maxNumKeys,
        GfxBuffer const &sourcePayload) noexcept;

    /**
     * Sort a list of keys from smallest to largest.
     * @param sourceBuffer The buffer containing the keys to sort (only 32bit uint or float>=0 are supported).
     * @param numKeys      Value containing the number of keys in the source buffer.
     */
    void sort(GfxBuffer const &sourceBuffer, uint numKeys) noexcept;

    /**
     * Sort a list of keys and associated payload from smallest to largest.
     * @param sourceBuffer  The buffer containing the keys to sort (only 32bit uint or float>=0 are
     * supported).
     * @param numKeys       Value containing the number of keys in the source buffer.
     * @param sourcePayload The buffer containing the payload for each key (only 32bit payloads per key are
     *  supported).
     */
    void sortPayload(GfxBuffer const &sourceBuffer, uint numKeys, GfxBuffer const &sourcePayload) noexcept;

    /**
     * Sort a segmented list of keys from smallest to largest using indirect execution.
     * @param sourceBuffer The buffer containing the keys to sort (only 32bit uint or float>=0 are supported).
     * @param numSegments  The number of segments to sort.
     * @param numKeys      A buffer containing the number of keys in each segment of the source buffer (must
     *  have @numSegments values).
     * @param maxNumKeys   Value containing the number of keys in each segment, if exact value is unknown then
     *  this should be the maximum possible number of values in each segment.
     */
    void sortIndirectSegmented(
        GfxBuffer const &sourceBuffer, uint numSegments, GfxBuffer const &numKeys, uint maxNumKeys) noexcept;

    /**
     * Sort a segmented list of keys and associated payload from smallest to largest using indirect execution.
     * @param sourceBuffer  The buffer containing the keys to sort (only 32bit uint or float>=0 are
     * supported).
     * @param numSegments   The number of segments to sort.
     * @param numKeys       A buffer containing the number of keys in each segment of the source buffer (must
     *  have @numSegments values).
     * @param maxNumKeys    Value containing the number of keys in each segment, if exact value is unknown
     * then this should be the maximum possible number of values in each segment.
     * @param sourcePayload The buffer containing the payload for each key (only 32bit payloads per key are
     *  supported).
     */
    void sortIndirectPayloadSegmented(GfxBuffer const &sourceBuffer, uint numSegments,
        GfxBuffer const &numKeys, uint maxNumKeys, GfxBuffer const &sourcePayload) noexcept;

    /**
     * Sort a segmented list of keys from smallest to largest.
     * @param sourceBuffer The buffer containing the keys to sort (only 32bit uint or float>=0 are supported).
     * @param numKeys      List containing the number of keys in each segment of the source buffer.
     * @param maxNumKeys   Value containing the max number of keys in any segment.
     */
    void sortSegmented(
        GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeys, uint maxNumKeys) noexcept;

    /**
     * Sort a segmented list of keys and associated payload from smallest to largest.
     * @param sourceBuffer  The buffer containing the keys to sort (only 32bit uint or float>=0 are
     * supported).
     * @param numKeys       List containing the number of keys in each segment of the source buffer.
     * @param maxNumKeys    Value containing the max number of keys in any segment.
     * @param sourcePayload The buffer containing the payload for each key (only 32bit payloads per key are
     *  supported).
     */
    void sortPayloadSegmented(GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeys,
        uint maxNumKeys, GfxBuffer const &sourcePayload) noexcept;

private:
    /** Terminates and cleans up this object. */
    void terminate() noexcept;

    /**
     * Internal sort implementation used to handle multiple sort cases.
     * @param sourceBuffer  The buffer containing the keys to sort (only 32bit uint or float>=0 are
     * supported).
     * @param maxNumKeys    Value containing the number of keys in the source buffer, if using indirect
     *  execution and exact value is unknown then this should be the maximum possible number of values in the
     *  source.
     * @param numKeys       (Optional) If non-null, a buffer containing the number of keys in the source
     * buffer used for indirect execution. If null then @maxNumKeys is used instead.
     * @param sourcePayload (Optional) The buffer containing the payload for each key (only 32bit payloads per
     *  key are supported).
     */
    void sortInternal(GfxBuffer const &sourceBuffer, uint maxNumKeys, GfxBuffer const *numKeys = nullptr,
        GfxBuffer const *sourcePayload = nullptr) noexcept;

    /**
     * Internal sort implementation used to handle multiple sort cases.
     * @param sourceBuffer  The buffer containing the keys to sort (only 32bit uint or float>=0 are
     * supported).
     * @param numKeysList   List containing the number of keys in each segment of the source buffer.
     * @param maxNumKeys    Value containing the number of keys in each segment of the source buffer, if using
     *  indirect execution and exact value is unknown then this should be the maximum possible number of
     *  values in the segment.
     * @param numSegments   (Optional) The number of segments to sort. If not supplied then length of
     *  @numKeysList is used instead.
     * @param numKeys       (Optional) If non-null, a buffer containing the number of keys in each segment of
     *  the source buffer (must have @numSegments values). If null then @numKeysList is used instead.
     * @param sourcePayload (Optional) The buffer containing the payload for each key (only 32bit payloads per
     *  key are supported).
     */
    void sortInternalSegmented(GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeysList,
        uint maxNumKeys, uint numSegments = UINT_MAX, GfxBuffer const *numKeys = nullptr,
        GfxBuffer const *sourcePayload = nullptr) noexcept;

    GfxContext gfx;

    Type      currentType      = Type::Float;
    Operation currentOperation = Operation::Ascending;

    GfxBuffer parallelSortCBBuffer;
    GfxBuffer countScatterArgsBuffer;
    GfxBuffer reduceScanArgsBuffer;

    GfxBuffer scratchBuffer;
    GfxBuffer reducedScratchBuffer;

    GfxBuffer sourcePongBuffer;
    GfxBuffer payloadPongBuffer;

    GfxProgram sortProgram;
    GfxKernel  setupIndirect;
    GfxKernel  count;
    GfxKernel  countReduce;
    GfxKernel  scan;
    GfxKernel  scanAdd;
    GfxKernel  scatter;
    GfxKernel  scatterPayload;
};
} // namespace Capsaicin
