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

#define FFX_CPP
#include "FFX_ParallelSort.h"
#include "buffer_view.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
GPUSort::~GPUSort() noexcept
{
    terminate();
}

bool GPUSort::initialise(
    GfxContext gfxIn, std::string_view const &shaderPath, Type type, Operation operation) noexcept
{
    gfx = gfxIn;

    if (!parallelSortCBBuffer)
    {
        // Currently we just allocate enough for a max number of 16 segments
        parallelSortCBBuffer = gfxCreateBuffer<FFX_ParallelSortCB>(gfx, 1 * 16);
        parallelSortCBBuffer.setName("Capsaicin_ParallelSortCBBuffer");
        countScatterArgsBuffer = gfxCreateBuffer<uint>(gfx, 3 * 16);
        countScatterArgsBuffer.setName("Capsaicin_CountScatterArgsBuffer");
        reduceScanArgsBuffer = gfxCreateBuffer<uint>(gfx, 3 * 16);
        reduceScanArgsBuffer.setName("Capsaicin_ReduceScanArgsBuffer");
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
        sortProgram = gfxCreateProgram(gfx, "utilities/gpu_sort", shaderPath.data());
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

bool GPUSort::initialise(CapsaicinInternal const &capsaicin, Type type, Operation operation) noexcept
{
    return initialise(capsaicin.getGfx(), capsaicin.getShaderPath(), type, operation);
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
    GfxBuffer const &sourceBuffer, GfxBuffer const &numKeys, const uint maxNumKeys) noexcept
{
    sortInternal(sourceBuffer, maxNumKeys, &numKeys);
}

void GPUSort::sortIndirectPayload(GfxBuffer const &sourceBuffer, GfxBuffer const &numKeys,
    const uint maxNumKeys, GfxBuffer const &sourcePayload) noexcept
{
    sortInternal(sourceBuffer, maxNumKeys, &numKeys, &sourcePayload);
}

void GPUSort::sort(GfxBuffer const &sourceBuffer, const uint numKeys) noexcept
{
    sortInternal(sourceBuffer, numKeys);
}

void GPUSort::sortPayload(
    GfxBuffer const &sourceBuffer, const uint numKeys, GfxBuffer const &sourcePayload) noexcept
{
    sortInternal(sourceBuffer, numKeys, nullptr, &sourcePayload);
}

void GPUSort::sortIndirectSegmented(GfxBuffer const &sourceBuffer, const uint numSegments,
    GfxBuffer const &numKeys, const uint maxNumKeys) noexcept
{
    sortInternalSegmented(sourceBuffer, {}, maxNumKeys, numSegments, &numKeys);
}

void GPUSort::sortIndirectPayloadSegmented(GfxBuffer const &sourceBuffer, const uint numSegments,
    GfxBuffer const &numKeys, const uint maxNumKeys, GfxBuffer const &sourcePayload) noexcept
{
    sortInternalSegmented(sourceBuffer, {}, maxNumKeys, numSegments, &numKeys, &sourcePayload);
}

void GPUSort::sortSegmented(
    GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeys, const uint maxNumKeys) noexcept
{
    sortInternalSegmented(sourceBuffer, numKeys, maxNumKeys);
}

void GPUSort::sortPayloadSegmented(GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeys,
    const uint maxNumKeys, GfxBuffer const &sourcePayload) noexcept
{
    sortInternalSegmented(sourceBuffer, numKeys, maxNumKeys, UINT_MAX, nullptr, &sourcePayload);
}

void GPUSort::sortInternal(GfxBuffer const &sourceBuffer, const uint maxNumKeys, GfxBuffer const *numKeys,
    GfxBuffer const *sourcePayload) noexcept
{
    // Check if we have payload to also sort
    bool hasPayload = sourcePayload;

    // Check if indirect
    bool indirect = (numKeys != nullptr);

    uint numThreadgroupsToRun        = 0;
    uint numReducedThreadgroupsToRun = 0;
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
        FFX_ParallelSortCB constantBufferData = {0};
        FFX_ParallelSort_SetConstantAndDispatchData(
            maxNumKeys, 800, constantBufferData, numThreadgroupsToRun, numReducedThreadgroupsToRun);
        gfxDestroyBuffer(gfx, parallelSortCBBuffer);
        parallelSortCBBuffer = gfxCreateBuffer<FFX_ParallelSortCB>(gfx, 1, &constantBufferData);
    }

    // Make scratch buffers
    uint scratchBufferSize;
    uint reducedScratchBufferSize;
    FFX_ParallelSort_CalculateScratchResourceSize(maxNumKeys, scratchBufferSize, reducedScratchBufferSize);
    if (!scratchBuffer || (scratchBuffer.getSize() < scratchBufferSize))
    {
        gfxDestroyBuffer(gfx, scratchBuffer);
        scratchBuffer = gfxCreateBuffer(gfx, scratchBufferSize);
        scratchBuffer.setName("Capsaicin_ScratchBuffer");
    }
    if (!reducedScratchBuffer || (reducedScratchBuffer.getSize() < reducedScratchBufferSize))
    {
        gfxDestroyBuffer(gfx, reducedScratchBuffer);
        reducedScratchBuffer = gfxCreateBuffer(gfx, reducedScratchBufferSize);
        reducedScratchBuffer.setName("Capsaicin_ReducedScratchBuffer");
    }

    // Setup ping-pong buffers
    if (!sourcePongBuffer || ((sourcePongBuffer.getSize() / sizeof(uint)) < maxNumKeys))
    {
        gfxDestroyBuffer(gfx, sourcePongBuffer);
        sourcePongBuffer = gfxCreateBuffer<uint>(gfx, maxNumKeys);
        sourcePongBuffer.setName("Capsaicin_SourcePongBuffer");
    }
    if (hasPayload && (!payloadPongBuffer || ((payloadPongBuffer.getSize() / sizeof(uint)) < maxNumKeys)))
    {
        gfxDestroyBuffer(gfx, payloadPongBuffer);
        payloadPongBuffer = gfxCreateBuffer<uint>(gfx, maxNumKeys);
        payloadPongBuffer.setName("Capsaicin_PayloadPongBuffer");
    }
    GfxBuffer const *readBuffer(&sourceBuffer);
    GfxBuffer const *writeBuffer(&sourcePongBuffer);
    GfxBuffer const *readPayloadBuffer(sourcePayload);
    GfxBuffer const *writePayloadBuffer(&payloadPongBuffer);

    // Perform Radix Sort (currently only support 32-bit key/payload sorting
    for (uint32_t shift = 0; shift < 32u; shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS)
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
                gfxCommandDispatch(gfx, numThreadgroupsToRun, 1, 1);
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
                gfxCommandDispatch(gfx, numReducedThreadgroupsToRun, 1, 1);
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
                gfxCommandDispatch(gfx, numReducedThreadgroupsToRun, 1, 1);
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
                gfxCommandDispatch(gfx, numThreadgroupsToRun, 1, 1);
            }
        }

        // Swap read/write sources
        std::swap(readBuffer, writeBuffer);
        std::swap(readPayloadBuffer, writePayloadBuffer);
    }
}

void GPUSort::sortInternalSegmented(GfxBuffer const &sourceBuffer, std::vector<uint> const &numKeysList,
    const uint maxNumKeys, uint numSegments, GfxBuffer const *numKeys,
    GfxBuffer const *sourcePayload) noexcept
{
    // Check if we have payload to also sort
    bool hasPayload = sourcePayload != nullptr;

    // Check if indirect
    bool isIndirect = (numKeys != nullptr);

    numSegments = isIndirect ? numSegments : (uint)numKeysList.size();

    std::vector<BufferView<FFX_ParallelSortCB>> parallelSortCBBufferViews;
    std::vector<BufferView<uint>>               countScatterArgsBufferViews;
    std::vector<BufferView<uint>>               reduceScanArgsBufferViews;
    std::vector<BufferView<uint>>               numKeysViews;
    std::vector<uint>                           numThreadgroupsToRun;
    std::vector<uint>                           numReducedThreadgroupsToRun;
    if (isIndirect)
    {
        // Check if the buffers are big enough for all the requested segments
        if (countScatterArgsBuffer.getCount() < 3 * numSegments)
        {
            gfxDestroyBuffer(gfx, countScatterArgsBuffer);
            countScatterArgsBuffer = gfxCreateBuffer<uint>(gfx, 4 * numSegments); // Uses 4 for alignment
            countScatterArgsBuffer.setName("Capsaicin_CountScatterArgsBuffer");
            gfxDestroyBuffer(gfx, reduceScanArgsBuffer);
            reduceScanArgsBuffer = gfxCreateBuffer<uint>(gfx, 4 * numSegments);
            reduceScanArgsBuffer.setName("Capsaicin_ReduceScanArgsBuffer");
        }
        if (parallelSortCBBuffer.getCount() < numSegments)
        {
            gfxDestroyBuffer(gfx, parallelSortCBBuffer);
            parallelSortCBBuffer = gfxCreateBuffer<FFX_ParallelSortCB>(gfx, numSegments);
            parallelSortCBBuffer.setName("Capsaicin_ParallelSortCBBuffer");
        }
        // Create the individual views into the buffers
        parallelSortCBBufferViews.reserve(numSegments);
        countScatterArgsBufferViews.reserve(numSegments);
        reduceScanArgsBufferViews.reserve(numSegments);
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
        std::vector<FFX_ParallelSortCB> constantBufferData;
        constantBufferData.reserve(numSegments);
        for (uint i = 0; i < numSegments; ++i)
        {
            constantBufferData.push_back({0});
            FFX_ParallelSort_SetConstantAndDispatchData(numKeysList[i], 800, constantBufferData[i],
                numThreadgroupsToRun[i], numReducedThreadgroupsToRun[i]);
        }
        gfxDestroyBuffer(gfx, parallelSortCBBuffer);
        parallelSortCBBuffer =
            gfxCreateBuffer<FFX_ParallelSortCB>(gfx, numSegments, constantBufferData.data());
        parallelSortCBBuffer.setName("Capsaicin_ParallelSortCBBuffer");
    }

    // Make scratch buffers
    std::vector<uint> scratchBufferCount;
    std::vector<uint> reducedScratchBufferCount;
    uint              totalScratchBufferCount        = 0;
    uint              totalReducedScratchBufferCount = 0;
    uint              totalKeys                      = maxNumKeys * numSegments;
    // Size each buffer based on maxNumKeys or exact segment size of known
    scratchBufferCount.reserve(numSegments);
    reducedScratchBufferCount.reserve(numSegments);
    for (uint i = 0; i < numSegments; ++i)
    {
        uint bufferSize = isIndirect ? maxNumKeys : numKeysList[i];
        scratchBufferCount.push_back(0); // Initialise vector element
        reducedScratchBufferCount.push_back(0);
        FFX_ParallelSort_CalculateScratchResourceSize(
            bufferSize, scratchBufferCount[i], reducedScratchBufferCount[i]);
        scratchBufferCount[i] /= sizeof(uint); // Convert from size to count
        reducedScratchBufferCount[i] /= sizeof(uint);
        totalScratchBufferCount += scratchBufferCount[i];
        totalReducedScratchBufferCount += reducedScratchBufferCount[i];
    }
    if (!scratchBuffer || (scratchBuffer.getCount() < totalScratchBufferCount))
    {
        gfxDestroyBuffer(gfx, scratchBuffer);
        scratchBuffer = gfxCreateBuffer<uint>(gfx, totalScratchBufferCount);
        scratchBuffer.setName("Capsaicin_ScratchBuffer");
    }
    if (!reducedScratchBuffer || (reducedScratchBuffer.getCount() < totalReducedScratchBufferCount))
    {
        gfxDestroyBuffer(gfx, reducedScratchBuffer);
        reducedScratchBuffer = gfxCreateBuffer<uint>(gfx, totalReducedScratchBufferCount);
        reducedScratchBuffer.setName("Capsaicin_ReducedScratchBuffer");
    }

    // Setup ping-pong buffers
    if (!sourcePongBuffer || (sourcePongBuffer.getCount() < totalKeys))
    {
        gfxDestroyBuffer(gfx, sourcePongBuffer);
        sourcePongBuffer = gfxCreateBuffer<uint>(gfx, totalKeys);
        sourcePongBuffer.setName("Capsaicin_SourcePongBuffer");
    }
    if (hasPayload && (!payloadPongBuffer || (payloadPongBuffer.getCount() < totalKeys)))
    {
        gfxDestroyBuffer(gfx, payloadPongBuffer);
        payloadPongBuffer = gfxCreateBuffer<uint>(gfx, totalKeys);
        payloadPongBuffer.setName("Capsaicin_PayloadPongBuffer");
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
        sourceBufferViews.emplace_back(gfx, sourceBuffer, offset, maxNumKeys);
        sourcePongBufferViews.emplace_back(gfx, sourcePongBuffer, offset, maxNumKeys);
        scratchBufferViews.emplace_back(gfx, scratchBuffer, scratchOffset, scratchBufferCount[i]);
        reducedScratchBufferViews.emplace_back(
            gfx, reducedScratchBuffer, reduceOffset, reducedScratchBufferCount[i]);

        if (hasPayload)
        {
            sourcePayloadViews.emplace_back(gfx, *sourcePayload, offset, maxNumKeys);
            payloadPongBufferViews.emplace_back(gfx, payloadPongBuffer, offset, maxNumKeys);
        }
        offset += maxNumKeys;
        scratchOffset += scratchBufferCount[i];
        reduceOffset += reducedScratchBufferCount[i];
    }

    std::vector<BufferView<uint>> const *readBuffer(&sourceBufferViews);
    std::vector<BufferView<uint>> const *writeBuffer(&sourcePongBufferViews);
    std::vector<BufferView<uint>> const *readPayloadBuffer(&sourcePayloadViews);
    std::vector<BufferView<uint>> const *writePayloadBuffer(&payloadPongBufferViews);

    // Perform Radix Sort (currently only support 32-bit key/payload sorting
    for (uint32_t shift = 0; shift < 32u; shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS)
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
                    gfxCommandDispatch(gfx, numThreadgroupsToRun[i], 1, 1);
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
                    gfxCommandDispatch(gfx, numReducedThreadgroupsToRun[i], 1, 1);
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
                    gfxCommandDispatch(gfx, numReducedThreadgroupsToRun[i], 1, 1);
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
                    gfxCommandDispatch(gfx, numThreadgroupsToRun[i], 1, 1);
                }
            }
        }

        // Swap read/write sources
        std::swap(readBuffer, writeBuffer);
        std::swap(readPayloadBuffer, writePayloadBuffer);
    }
}
} // namespace Capsaicin
