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

#include "gpu_sort.h"

#include "buffer_view.h"
#include "capsaicin_internal.h"

#define FFX_CPU
#ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wmissing-braces"
#    pragma clang diagnostic ignored "-Wunused-function"
#endif
#include <FidelityFX/gpu/ffx_core.h>
#include <FidelityFX/host/ffx_util.h>
// Order of includes is important
#include <FidelityFX/gpu/parallelsort/ffx_parallelsort.h>
#ifdef __clang__
#    pragma clang diagnostic pop
#endif

namespace Capsaicin
{
GPUSort::~GPUSort() noexcept
{
    terminate();
}

bool GPUSort::initialise(GfxContext const &gfxIn, std::vector<std::string> const &shaderPaths,
    Type const type, Operation const operation) noexcept
{
    gfx = gfxIn;

    if (!parallelSortCBBuffer)
    {
        // Currently we just allocate enough for a max number of 16 segments
        parallelSortCBBuffer = gfxCreateBuffer<FfxParallelSortConstants>(gfx, 1 * 16);
        parallelSortCBBuffer.setName("ParallelSortCBBuffer");
        countScatterArgsBuffer = gfxCreateBuffer<uint>(gfx, 3 * 16);
        countScatterArgsBuffer.setName("CountScatterArgsBuffer");
        reduceScanArgsBuffer = gfxCreateBuffer<uint>(gfx, 3 * 16);
        reduceScanArgsBuffer.setName("ReduceScanArgsBuffer");
    }

    if (type != currentType || operation != currentOperation)
    {
        // If configuration has changed then need to recompile kernels
        gfxDestroyProgram(gfx, sortProgram);
        sortProgram = {};
        gfxDestroyKernel(gfx, setupIndirect);
        setupIndirect = {};
        gfxDestroyKernel(gfx, count);
        count = {};
        gfxDestroyKernel(gfx, countReduce);
        countReduce = {};
        gfxDestroyKernel(gfx, scan);
        scan = {};
        gfxDestroyKernel(gfx, scanAdd);
        scanAdd = {};
        gfxDestroyKernel(gfx, scatter);
        scatter = {};
        gfxDestroyKernel(gfx, scatterPayload);
        scatterPayload = {};
    }
    currentType      = type;
    currentOperation = operation;

    if (!sortProgram)
    {
        gfxDestroyKernel(gfx, setupIndirect);
        gfxDestroyKernel(gfx, count);
        gfxDestroyKernel(gfx, countReduce);
        gfxDestroyKernel(gfx, scan);
        gfxDestroyKernel(gfx, scanAdd);
        gfxDestroyKernel(gfx, scatter);
        gfxDestroyKernel(gfx, scatterPayload);

        std::vector<char const *> includePaths;
        includePaths.reserve(shaderPaths.size());
        for (auto const &path : shaderPaths)
        {
            includePaths.push_back(path.c_str());
        }
        sortProgram = gfxCreateProgram(gfx, "utilities/gpu_sort", includePaths[0], nullptr,
            includePaths.data(), static_cast<uint32_t>(includePaths.size()));
        std::vector<char const *> baseDefines;
        switch (currentType)
        {
        case Type::Float: baseDefines.push_back("TYPE=float"); break;
        case Type::UInt: baseDefines.push_back("TYPE=uint"); break;
        default: break;
        }
        switch (currentOperation)
        {
        case Operation::Ascending: baseDefines.push_back("OP=0"); break;
        case Operation::Descending: baseDefines.push_back("OP=1"); break;
        default: break;
        }
        setupIndirect = gfxCreateComputeKernel(gfx, sortProgram, "setupIndirectParameters",
            baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
        count         = gfxCreateComputeKernel(
            gfx, sortProgram, "count", baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
        countReduce = gfxCreateComputeKernel(
            gfx, sortProgram, "countReduce", baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
        scan = gfxCreateComputeKernel(
            gfx, sortProgram, "scan", baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
        scanAdd = gfxCreateComputeKernel(
            gfx, sortProgram, "scanAdd", baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
        scatter = gfxCreateComputeKernel(
            gfx, sortProgram, "scatter", baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
        scatterPayload = gfxCreateComputeKernel(gfx, sortProgram, "scatterPayload", baseDefines.data(),
            static_cast<uint32_t>(baseDefines.size()));
    }

    return !!scatterPayload;
}

bool GPUSort::initialise(
    CapsaicinInternal const &capsaicin, Type const type, Operation const operation) noexcept
{
    return initialise(capsaicin.getGfx(), capsaicin.getShaderPaths(), type, operation);
}

void GPUSort::terminate() noexcept
{
    gfxDestroyBuffer(gfx, parallelSortCBBuffer);
    parallelSortCBBuffer = {};
    gfxDestroyBuffer(gfx, countScatterArgsBuffer);
    countScatterArgsBuffer = {};
    gfxDestroyBuffer(gfx, reduceScanArgsBuffer);
    reduceScanArgsBuffer = {};

    gfxDestroyBuffer(gfx, scratchBuffer);
    scratchBuffer = {};
    gfxDestroyBuffer(gfx, reducedScratchBuffer);
    reducedScratchBuffer = {};

    gfxDestroyBuffer(gfx, sourcePongBuffer);
    sourcePongBuffer = {};
    gfxDestroyBuffer(gfx, payloadPongBuffer);
    payloadPongBuffer = {};

    gfxDestroyProgram(gfx, sortProgram);
    sortProgram = {};
    gfxDestroyKernel(gfx, setupIndirect);
    setupIndirect = {};
    gfxDestroyKernel(gfx, count);
    count = {};
    gfxDestroyKernel(gfx, countReduce);
    countReduce = {};
    gfxDestroyKernel(gfx, scan);
    scan = {};
    gfxDestroyKernel(gfx, scanAdd);
    scanAdd = {};
    gfxDestroyKernel(gfx, scatter);
    scatter = {};
    gfxDestroyKernel(gfx, scatterPayload);
    scatterPayload = {};
}

void GPUSort::sortIndirect(
    GfxBuffer const &sourceBuffer, GfxBuffer const &numKeys, uint const maxNumKeys) noexcept
{
    sortInternal(sourceBuffer, maxNumKeys, &numKeys);
}

void GPUSort::sortIndirectPayload(GfxBuffer const &sourceBuffer, GfxBuffer const &numKeys,
    uint const maxNumKeys, GfxBuffer const &sourcePayload) noexcept
{
    sortInternal(sourceBuffer, maxNumKeys, &numKeys, &sourcePayload);
}

void GPUSort::sort(GfxBuffer const &sourceBuffer, uint const numKeys) noexcept
{
    sortInternal(sourceBuffer, numKeys);
}

void GPUSort::sortPayload(
    GfxBuffer const &sourceBuffer, uint const numKeys, GfxBuffer const &sourcePayload) noexcept
{
    sortInternal(sourceBuffer, numKeys, nullptr, &sourcePayload);
}

void GPUSort::sortIndirectSegmented(GfxBuffer const &sourceBuffer, uint const numSegments,
    GfxBuffer const &numKeys, uint const maxNumKeys) noexcept
{
    sortInternalSegmented(sourceBuffer, {}, maxNumKeys, numSegments, &numKeys);
}

void GPUSort::sortIndirectPayloadSegmented(GfxBuffer const &sourceBuffer, uint const numSegments,
    GfxBuffer const &numKeys, uint const maxNumKeys, GfxBuffer const &sourcePayload) noexcept
{
    sortInternalSegmented(sourceBuffer, {}, maxNumKeys, numSegments, &numKeys, &sourcePayload);
}

void GPUSort::sortSegmented(GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeys) noexcept
{
    sortInternalSegmented(sourceBuffer, numKeys, 0);
}

void GPUSort::sortPayloadSegmented(
    GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeys, GfxBuffer const &sourcePayload) noexcept
{
    sortInternalSegmented(sourceBuffer, numKeys, 0, UINT_MAX, nullptr, &sourcePayload);
}

void GPUSort::sortInternal(GfxBuffer const &sourceBuffer, uint const maxNumKeys, GfxBuffer const *numKeys,
    GfxBuffer const *sourcePayload) noexcept
{
    // Check if we have payload to also sort
    bool const hasPayload = sourcePayload != nullptr;

    // Check if indirect
    bool const indirect = (numKeys != nullptr);

    uint numThreadGroupsToRun        = 0;
    uint numReducedThreadGroupsToRun = 0;
    if (indirect)
    {
        // Run the indirect sort setup kernel
        gfxProgramSetParameter(gfx, sortProgram, "CBufferUAV", parallelSortCBBuffer);
        gfxProgramSetParameter(gfx, sortProgram, "CountScatterArgs", countScatterArgsBuffer);
        gfxProgramSetParameter(gfx, sortProgram, "ReduceScanArgs", reduceScanArgsBuffer);
        gfxProgramSetParameter(gfx, sortProgram, "numKeys", *numKeys);

        gfxCommandBindKernel(gfx, setupIndirect);
        gfxCommandDispatch(gfx, 1, 1, 1);
    }
    else
    {
        FfxParallelSortConstants constantBufferData = {};
        ffxParallelSortSetConstantAndDispatchData(
            maxNumKeys, 800, constantBufferData, numThreadGroupsToRun, numReducedThreadGroupsToRun);
        gfxDestroyBuffer(gfx, parallelSortCBBuffer);
        parallelSortCBBuffer = gfxCreateBuffer<FfxParallelSortConstants>(gfx, 1, &constantBufferData);
    }

    // Make scratch buffers
    uint scratchBufferSize        = 0;
    uint reducedScratchBufferSize = 0;
    ffxParallelSortCalculateScratchResourceSize(maxNumKeys, scratchBufferSize, reducedScratchBufferSize);
    if (!scratchBuffer || (scratchBuffer.getSize() < scratchBufferSize))
    {
        gfxDestroyBuffer(gfx, scratchBuffer);
        scratchBuffer = gfxCreateBuffer(gfx, scratchBufferSize);
        scratchBuffer.setName("ScratchBuffer");
    }
    if (!reducedScratchBuffer || (reducedScratchBuffer.getSize() < reducedScratchBufferSize))
    {
        gfxDestroyBuffer(gfx, reducedScratchBuffer);
        reducedScratchBuffer = gfxCreateBuffer(gfx, reducedScratchBufferSize);
        reducedScratchBuffer.setName("ReducedScratchBuffer");
    }

    // Setup ping-pong buffers
    if (!sourcePongBuffer || ((sourcePongBuffer.getSize() / sizeof(uint)) < maxNumKeys))
    {
        gfxDestroyBuffer(gfx, sourcePongBuffer);
        sourcePongBuffer = gfxCreateBuffer<uint>(gfx, maxNumKeys);
        sourcePongBuffer.setName("SourcePongBuffer");
    }
    if (hasPayload && (!payloadPongBuffer || ((payloadPongBuffer.getSize() / sizeof(uint)) < maxNumKeys)))
    {
        gfxDestroyBuffer(gfx, payloadPongBuffer);
        payloadPongBuffer = gfxCreateBuffer<uint>(gfx, maxNumKeys);
        payloadPongBuffer.setName("PayloadPongBuffer");
    }
    GfxBuffer const *readBuffer(&sourceBuffer);
    GfxBuffer const *writeBuffer(&sourcePongBuffer);
    GfxBuffer const *readPayloadBuffer(sourcePayload);
    GfxBuffer const *writePayloadBuffer(&payloadPongBuffer);

    // Perform Radix Sort (currently only support 32-bit key/payload sorting
    for (uint32_t shift = 0; shift < 32U; shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS)
    {
        // Sort Count
        {
            gfxProgramSetParameter(gfx, sortProgram, "CShiftBit", shift);
            gfxProgramSetParameter(gfx, sortProgram, "CBuffer", parallelSortCBBuffer);
            gfxProgramSetParameter(gfx, sortProgram, "SrcBuffer", *readBuffer);
            gfxProgramSetParameter(gfx, sortProgram, "SumTable", scratchBuffer);

            gfxCommandBindKernel(gfx, count);
            if (indirect)
            {
                gfxCommandDispatchIndirect(gfx, countScatterArgsBuffer);
            }
            else
            {
                gfxCommandDispatch(gfx, numThreadGroupsToRun, 1, 1);
            }
        }

        // Sort Reduce
        {
            gfxProgramSetParameter(gfx, sortProgram, "ReduceTable", reducedScratchBuffer);

            gfxCommandBindKernel(gfx, countReduce);
            if (indirect)
            {
                gfxCommandDispatchIndirect(gfx, reduceScanArgsBuffer);
            }
            else
            {
                gfxCommandDispatch(gfx, numReducedThreadGroupsToRun, 1, 1);
            }
        }

        // Sort Scan
        {
            // First do scan prefix of reduced values
            gfxProgramSetParameter(gfx, sortProgram, "ScanSrc", reducedScratchBuffer);
            gfxProgramSetParameter(gfx, sortProgram, "ScanDst", reducedScratchBuffer);
            gfxCommandBindKernel(gfx, scan);
            gfxCommandDispatch(gfx, 1, 1, 1);

            // Next do scan prefix on the histogram with partial sums that we just did
            gfxProgramSetParameter(gfx, sortProgram, "ScanSrc", scratchBuffer);
            gfxProgramSetParameter(gfx, sortProgram, "ScanDst", scratchBuffer);
            gfxProgramSetParameter(gfx, sortProgram, "ScanScratch", reducedScratchBuffer);
            gfxCommandBindKernel(gfx, scanAdd);
            if (indirect)
            {
                gfxCommandDispatchIndirect(gfx, reduceScanArgsBuffer);
            }
            else
            {
                gfxCommandDispatch(gfx, numReducedThreadGroupsToRun, 1, 1);
            }
        }

        // Sort Scatter
        {
            gfxProgramSetParameter(gfx, sortProgram, "DstBuffer", *writeBuffer);
            if (hasPayload)
            {
                gfxProgramSetParameter(gfx, sortProgram, "SrcPayload", *readPayloadBuffer);
                gfxProgramSetParameter(gfx, sortProgram, "DstPayload", *writePayloadBuffer);
                gfxCommandBindKernel(gfx, scatterPayload);
            }
            else
            {
                gfxCommandBindKernel(gfx, scatter);
            }
            if (indirect)
            {
                gfxCommandDispatchIndirect(gfx, countScatterArgsBuffer);
            }
            else
            {
                gfxCommandDispatch(gfx, numThreadGroupsToRun, 1, 1);
            }
        }

        // Swap read/write sources
        std::swap(readBuffer, writeBuffer);
        std::swap(readPayloadBuffer, writePayloadBuffer);
    }
}

void GPUSort::sortInternalSegmented(GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeysList,
    uint const maxNumKeys, uint numSegments, GfxBuffer const *numKeys,
    GfxBuffer const *sourcePayload) noexcept
{
    // Check if we have payload to also sort
    bool const hasPayload = sourcePayload != nullptr;

    // Check if indirect
    bool const isIndirect = (numKeys != nullptr);

    numSegments = isIndirect ? numSegments : static_cast<uint>(numKeysList.size());

    std::vector<BufferView<FfxParallelSortConstants>> parallelSortCBBufferViews;
    parallelSortCBBufferViews.reserve(numSegments);
    std::vector<BufferView<uint>> countScatterArgsBufferViews;
    std::vector<BufferView<uint>> reduceScanArgsBufferViews;
    std::vector<uint>             numThreadGroupsToRun;
    std::vector<uint>             numReducedThreadGroupsToRun;
    if (isIndirect)
    {
        // Check if the buffers are big enough for all the requested segments
        if (countScatterArgsBuffer.getCount() < 4 * numSegments)
        {
            gfxDestroyBuffer(gfx, countScatterArgsBuffer);
            countScatterArgsBuffer = gfxCreateBuffer<uint>(gfx, 4 * numSegments); // Uses 4 for alignment
            countScatterArgsBuffer.setName("CountScatterArgsBuffer");
            gfxDestroyBuffer(gfx, reduceScanArgsBuffer);
            reduceScanArgsBuffer = gfxCreateBuffer<uint>(gfx, 4 * numSegments);
            reduceScanArgsBuffer.setName("ReduceScanArgsBuffer");
        }
        if (parallelSortCBBuffer.getCount() < numSegments)
        {
            gfxDestroyBuffer(gfx, parallelSortCBBuffer);
            parallelSortCBBuffer = gfxCreateBuffer<FfxParallelSortConstants>(gfx, numSegments);
            parallelSortCBBuffer.setName("ParallelSortCBBuffer");
        }
        // Create the individual views into the buffers
        countScatterArgsBufferViews.reserve(numSegments);
        reduceScanArgsBufferViews.reserve(numSegments);
        std::vector<BufferView<uint>> numKeysViews;
        numKeysViews.reserve(numSegments);
        for (uint i = 0; i < numSegments; ++i)
        {
            parallelSortCBBufferViews.emplace_back(gfx, parallelSortCBBuffer, i, 1);
            countScatterArgsBufferViews.emplace_back(gfx, countScatterArgsBuffer, i * 4, 3);
            reduceScanArgsBufferViews.emplace_back(gfx, reduceScanArgsBuffer, i * 4, 3);
            numKeysViews.emplace_back(gfx, *numKeys, i, 1);
        }

        // Run the indirect sort setup kernel
        gfxCommandBindKernel(gfx, setupIndirect);
        for (uint i = 0; i < numSegments; ++i)
        {
            gfxProgramSetParameter(gfx, sortProgram, "CBufferUAV", parallelSortCBBufferViews[i].buffer);
            gfxProgramSetParameter(
                gfx, sortProgram, "CountScatterArgs", countScatterArgsBufferViews[i].buffer);
            gfxProgramSetParameter(gfx, sortProgram, "ReduceScanArgs", reduceScanArgsBufferViews[i].buffer);
            gfxProgramSetParameter(gfx, sortProgram, "numKeys", numKeysViews[i].buffer);

            gfxCommandDispatch(gfx, 1, 1, 1);
        }
    }
    else
    {
        std::vector<FfxParallelSortConstants> constantBufferData;
        constantBufferData.resize(numSegments);
        numThreadGroupsToRun.resize(numSegments);
        numReducedThreadGroupsToRun.resize(numSegments);
        for (uint i = 0; i < numSegments; ++i)
        {
            memset(&constantBufferData[i], 0, sizeof(FfxParallelSortConstants));
            ffxParallelSortSetConstantAndDispatchData(numKeysList[i], 800, constantBufferData[i],
                numThreadGroupsToRun[i], numReducedThreadGroupsToRun[i]);
        }
        gfxDestroyBuffer(gfx, parallelSortCBBuffer);
        parallelSortCBBuffer =
            gfxCreateBuffer<FfxParallelSortConstants>(gfx, numSegments, constantBufferData.data());
        parallelSortCBBuffer.setName("ParallelSortCBBuffer");
        for (uint i = 0; i < numSegments; ++i)
        {
            parallelSortCBBufferViews.emplace_back(gfx, parallelSortCBBuffer, i, 1);
        }
    }

    // Make scratch buffers
    std::vector<uint> scratchBufferCount;
    std::vector<uint> reducedScratchBufferCount;
    uint              totalScratchBufferCount        = 0;
    uint              totalReducedScratchBufferCount = 0;
    uint              totalKeys                      = 0;
    // Size each buffer based on maxNumKeys or exact segment size of known
    scratchBufferCount.resize(numSegments);
    reducedScratchBufferCount.resize(numSegments);
    for (uint i = 0; i < numSegments; ++i)
    {
        uint const bufferSize = isIndirect ? maxNumKeys : numKeysList[i];
        ffxParallelSortCalculateScratchResourceSize(
            bufferSize, scratchBufferCount[i], reducedScratchBufferCount[i]);
        scratchBufferCount[i] /= sizeof(uint); // Convert from size to count
        reducedScratchBufferCount[i] /= sizeof(uint);
        totalScratchBufferCount += scratchBufferCount[i];
        totalReducedScratchBufferCount += reducedScratchBufferCount[i];
        totalKeys += bufferSize;
    }
    if (!scratchBuffer || (scratchBuffer.getCount() < totalScratchBufferCount))
    {
        gfxDestroyBuffer(gfx, scratchBuffer);
        scratchBuffer = gfxCreateBuffer<uint>(gfx, totalScratchBufferCount);
        scratchBuffer.setName("ScratchBuffer");
    }
    if (!reducedScratchBuffer || (reducedScratchBuffer.getCount() < totalReducedScratchBufferCount))
    {
        gfxDestroyBuffer(gfx, reducedScratchBuffer);
        reducedScratchBuffer = gfxCreateBuffer<uint>(gfx, totalReducedScratchBufferCount);
        reducedScratchBuffer.setName("ReducedScratchBuffer");
    }

    // Setup ping-pong buffers
    if (!sourcePongBuffer || (sourcePongBuffer.getCount() < totalKeys))
    {
        gfxDestroyBuffer(gfx, sourcePongBuffer);
        sourcePongBuffer = gfxCreateBuffer<uint>(gfx, totalKeys);
        sourcePongBuffer.setName("SourcePongBuffer");
    }
    if (hasPayload && (!payloadPongBuffer || (payloadPongBuffer.getCount() < totalKeys)))
    {
        gfxDestroyBuffer(gfx, payloadPongBuffer);
        payloadPongBuffer = gfxCreateBuffer<uint>(gfx, totalKeys);
        payloadPongBuffer.setName("PayloadPongBuffer");
    }
    std::vector<BufferView<uint>> sourceBufferViews;
    std::vector<BufferView<uint>> sourcePongBufferViews;
    std::vector<BufferView<uint>> scratchBufferViews;
    std::vector<BufferView<uint>> reducedScratchBufferViews;
    std::vector<BufferView<uint>> sourcePayloadViews;
    std::vector<BufferView<uint>> payloadPongBufferViews;
    // Set buffer views
    sourceBufferViews.reserve(numSegments);
    sourcePongBufferViews.reserve(numSegments);
    scratchBufferViews.reserve(numSegments);
    reducedScratchBufferViews.reserve(numSegments);
    sourcePayloadViews.reserve(numSegments);
    payloadPongBufferViews.reserve(numSegments);
    for (uint i = 0, offset = 0, scratchOffset = 0, reduceOffset = 0; i < numSegments; ++i)
    {
        uint const bufferSize = isIndirect ? maxNumKeys : numKeysList[i];
        sourceBufferViews.emplace_back(gfx, sourceBuffer, offset, bufferSize);
        sourcePongBufferViews.emplace_back(gfx, sourcePongBuffer, offset, bufferSize);
        scratchBufferViews.emplace_back(gfx, scratchBuffer, scratchOffset, scratchBufferCount[i]);
        reducedScratchBufferViews.emplace_back(
            gfx, reducedScratchBuffer, reduceOffset, reducedScratchBufferCount[i]);

        if (hasPayload)
        {
            sourcePayloadViews.emplace_back(gfx, *sourcePayload, offset, bufferSize);
            payloadPongBufferViews.emplace_back(gfx, payloadPongBuffer, offset, bufferSize);
        }
        offset += bufferSize;
        scratchOffset += scratchBufferCount[i];
        reduceOffset += reducedScratchBufferCount[i];
    }

    std::vector<BufferView<uint>> const *readBuffer(&sourceBufferViews);
    std::vector<BufferView<uint>> const *writeBuffer(&sourcePongBufferViews);
    std::vector<BufferView<uint>> const *readPayloadBuffer(&sourcePayloadViews);
    std::vector<BufferView<uint>> const *writePayloadBuffer(&payloadPongBufferViews);

    // Perform Radix Sort (currently only support 32-bit key/payload sorting
    for (uint32_t shift = 0; shift < 32U; shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS)
    {
        // Sort Count
        {
            gfxProgramSetParameter(gfx, sortProgram, "CShiftBit", shift);
            gfxCommandBindKernel(gfx, count);
            for (uint i = 0; i < numSegments; ++i)
            {
                gfxProgramSetParameter(gfx, sortProgram, "CBuffer", parallelSortCBBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "SrcBuffer", (*readBuffer)[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "SumTable", scratchBufferViews[i].buffer);
                if (isIndirect)
                {
                    gfxCommandDispatchIndirect(gfx, countScatterArgsBufferViews[i].buffer);
                }
                else
                {
                    gfxCommandDispatch(gfx, numThreadGroupsToRun[i], 1, 1);
                }
            }
        }

        // Sort Reduce
        {
            gfxCommandBindKernel(gfx, countReduce);
            for (uint i = 0; i < numSegments; ++i)
            {
                gfxProgramSetParameter(gfx, sortProgram, "CBuffer", parallelSortCBBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "SumTable", scratchBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "ReduceTable", reducedScratchBufferViews[i].buffer);
                if (isIndirect)
                {
                    gfxCommandDispatchIndirect(gfx, reduceScanArgsBufferViews[i].buffer);
                }
                else
                {
                    gfxCommandDispatch(gfx, numReducedThreadGroupsToRun[i], 1, 1);
                }
            }
        }

        // Sort Scan
        {
            // First do scan prefix of reduced values
            gfxCommandBindKernel(gfx, scan);
            for (uint i = 0; i < numSegments; ++i)
            {
                gfxProgramSetParameter(gfx, sortProgram, "CBuffer", parallelSortCBBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "ScanSrc", reducedScratchBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "ScanDst", reducedScratchBufferViews[i].buffer);
                gfxCommandDispatch(gfx, 1, 1, 1);
            }

            // Next do scan prefix on the histogram with partial sums that we just did
            gfxCommandBindKernel(gfx, scanAdd);
            for (uint i = 0; i < numSegments; ++i)
            {
                gfxProgramSetParameter(gfx, sortProgram, "CBuffer", parallelSortCBBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "ScanSrc", scratchBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "ScanDst", scratchBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "ScanScratch", reducedScratchBufferViews[i].buffer);
                if (isIndirect)
                {
                    gfxCommandDispatchIndirect(gfx, reduceScanArgsBufferViews[i].buffer);
                }
                else
                {
                    gfxCommandDispatch(gfx, numReducedThreadGroupsToRun[i], 1, 1);
                }
            }
        }

        // Sort Scatter
        {
            if (hasPayload)
            {
                gfxCommandBindKernel(gfx, scatterPayload);
            }
            else
            {
                gfxCommandBindKernel(gfx, scatter);
            }
            for (uint i = 0; i < numSegments; ++i)
            {
                gfxProgramSetParameter(gfx, sortProgram, "CBuffer", parallelSortCBBufferViews[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "SrcBuffer", (*readBuffer)[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "DstBuffer", (*writeBuffer)[i].buffer);
                gfxProgramSetParameter(gfx, sortProgram, "SumTable", scratchBufferViews[i].buffer);
                if (hasPayload)
                {
                    gfxProgramSetParameter(gfx, sortProgram, "SrcPayload", (*readPayloadBuffer)[i].buffer);
                    gfxProgramSetParameter(gfx, sortProgram, "DstPayload", (*writePayloadBuffer)[i].buffer);
                }
                if (isIndirect)
                {
                    gfxCommandDispatchIndirect(gfx, countScatterArgsBufferViews[i].buffer);
                }
                else
                {
                    gfxCommandDispatch(gfx, numThreadGroupsToRun[i], 1, 1);
                }
            }
        }

        // Swap read/write sources
        std::swap(readBuffer, writeBuffer);
        std::swap(readPayloadBuffer, writePayloadBuffer);
    }
}
} // namespace Capsaicin
