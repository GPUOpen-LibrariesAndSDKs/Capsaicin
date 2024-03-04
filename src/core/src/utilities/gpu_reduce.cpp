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

#include "gpu_reduce.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
GPUReduce::~GPUReduce() noexcept
{
    terminate();
}

bool GPUReduce::initialise(
    GfxContext gfxIn, std::string_view const &shaderPath, Type type, Operation operation) noexcept
{
    gfx = gfxIn;

    if (!indirectBuffer)
    {
        // Free just in case
        gfxDestroyBuffer(gfx, indirectBuffer);
        gfxDestroyBuffer(gfx, indirectBuffer2);
        gfxDestroyBuffer(gfx, indirectCountBuffer);
        gfxDestroyBuffer(gfx, indirectCountBuffer2);
        // Create required buffers
        indirectBuffer = gfxCreateBuffer<uint>(gfx, 4);
        indirectBuffer.setName("Capsaicin_Reduce_IndirectBuffer");
        indirectBuffer2 = gfxCreateBuffer<uint>(gfx, 4);
        indirectBuffer2.setName("Capsaicin_Reduce_IndirectBuffer2");
        indirectCountBuffer = gfxCreateBuffer<uint>(gfx, 1);
        indirectCountBuffer.setName("Capsaicin_Reduce_IndirectCountBuffer");
        indirectCountBuffer2 = gfxCreateBuffer<uint>(gfx, 1);
        indirectCountBuffer2.setName("Capsaicin_Reduce_IndirectCountBuffer2");
    }

    if (type != currentType || operation != currentOperation)
    {
        // If configuration has changed then need to recompile kernels
        gfxDestroyProgram(gfx, reduceProgram);
        reduceProgram = {};
        gfxDestroyKernel(gfx, reduceKernel);
        reduceKernel = {};
    }
    currentType      = type;
    currentOperation = operation;
    if (!reduceProgram)
    {
        gfxDestroyProgram(gfx, reduceProgram);
        gfxDestroyKernel(gfx, reduceKernel);
        gfxDestroyKernel(gfx, reduceIndirectKernel);
        gfxDestroyKernel(gfx, dispatchIndirectKernel);
        reduceProgram = gfxCreateProgram(gfx, "utilities/gpu_reduce", shaderPath.data());
        std::vector<char const *> baseDefines;
        switch (currentType)
        {
        case Type::Float: baseDefines.push_back("TYPE=float"); break;
        case Type::Float2: baseDefines.push_back("TYPE=float2"); break;
        case Type::Float3: baseDefines.push_back("TYPE=float3"); break;
        case Type::Float4: baseDefines.push_back("TYPE=float4"); break;
        case Type::UInt: baseDefines.push_back("TYPE=uint"); break;
        case Type::UInt2: baseDefines.push_back("TYPE=uint2"); break;
        case Type::UInt3: baseDefines.push_back("TYPE=uint3"); break;
        case Type::UInt4: baseDefines.push_back("TYPE=uint4"); break;
        case Type::Int: baseDefines.push_back("TYPE=int"); break;
        case Type::Int2: baseDefines.push_back("TYPE=int2"); break;
        case Type::Int3: baseDefines.push_back("TYPE=int3"); break;
        case Type::Int4: baseDefines.push_back("TYPE=int4"); break;
        case Type::Double:  baseDefines.push_back("TYPE=double"); break;
        case Type::Double2: baseDefines.push_back("TYPE=double2"); break;
        case Type::Double3: baseDefines.push_back("TYPE=double3"); break;
        case Type::Double4: baseDefines.push_back("TYPE=double4"); break;
        default: break;
        }
        switch (currentOperation)
        {
        case Operation::Sum: baseDefines.push_back("OP=0"); break;
        case Operation::Min: baseDefines.push_back("OP=1"); break;
        case Operation::Max: baseDefines.push_back("OP=2"); break;
        case Operation::Product: baseDefines.push_back("OP=3"); break;
        }
        reduceKernel = gfxCreateComputeKernel(
            gfx, reduceProgram, "BlockReduce", baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
        reduceIndirectKernel   = gfxCreateComputeKernel(gfx, reduceProgram, "BlockReduceIndirect",
              baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
        dispatchIndirectKernel = gfxCreateComputeKernel(gfx, reduceProgram, "GenerateDispatches",
            baseDefines.data(), static_cast<uint32_t>(baseDefines.size()));
    }

    return !!dispatchIndirectKernel;
}

bool GPUReduce::initialise(CapsaicinInternal const &capsaicin, Type type, Operation operation) noexcept
{
    return initialise(capsaicin.getGfx(), capsaicin.getShaderPath(), type, operation);
}

void GPUReduce::terminate() noexcept
{
    gfxDestroyBuffer(gfx, scratchBuffer);
    scratchBuffer = {};
    gfxDestroyBuffer(gfx, indirectBuffer);
    indirectBuffer = {};
    gfxDestroyBuffer(gfx, indirectBuffer2);
    indirectBuffer2 = {};
    gfxDestroyBuffer(gfx, indirectCountBuffer);
    indirectCountBuffer = {};
    gfxDestroyBuffer(gfx, indirectCountBuffer2);
    indirectCountBuffer2 = {};

    gfxDestroyProgram(gfx, reduceProgram);
    reduceProgram = {};
    gfxDestroyKernel(gfx, reduceKernel);
    reduceKernel = {};
    gfxDestroyKernel(gfx, reduceIndirectKernel);
    reduceIndirectKernel = {};
    gfxDestroyKernel(gfx, dispatchIndirectKernel);
    dispatchIndirectKernel = {};
}

bool GPUReduce::reduceIndirect(
    GfxBuffer const &sourceBuffer, GfxBuffer const &numKeys, const uint maxNumKeys) noexcept
{
    return reduceInternal(sourceBuffer, maxNumKeys, &numKeys);
}

bool GPUReduce::reduce(GfxBuffer const &sourceBuffer, const uint numKeys) noexcept
{
    return reduceInternal(sourceBuffer, numKeys);
}

bool GPUReduce::reduceInternal(
    GfxBuffer const &sourceBuffer, const uint maxNumKeys, GfxBuffer const *numKeys) noexcept
{
    // Check if indirect
    bool indirect = (numKeys != nullptr);

    // Calculate number of loops
    uint32_t const *numThreads    = gfxKernelGetNumThreads(gfx, reduceKernel);
    const uint      groupSize     = numThreads[0];
    const uint      keysPerThread = 4; // Must match KEYS_PER_THREAD in shader
    const uint      keysPerGroup  = groupSize * keysPerThread;
    const uint32_t  numGroups1    = (maxNumKeys + keysPerGroup - 1) / keysPerGroup;
    const uint32_t  numGroups2    = (numGroups1 + keysPerGroup - 1) / keysPerGroup;
    if (numGroups2 > numThreads[0])
    {
        // To many keys as we only support 2 loops
        return false;
    }

    if (indirect)
    {
        // Call indirect setup kernel
        gfxProgramSetParameter(gfx, reduceProgram, "g_InputLength", *numKeys);
        gfxProgramSetParameter(gfx, reduceProgram, "g_Dispatch1", indirectBuffer);
        gfxProgramSetParameter(gfx, reduceProgram, "g_Dispatch2", indirectBuffer2);
        gfxProgramSetParameter(gfx, reduceProgram, "g_InputLength1", indirectCountBuffer);
        gfxProgramSetParameter(gfx, reduceProgram, "g_InputLength2", indirectCountBuffer2);
        gfxCommandBindKernel(gfx, dispatchIndirectKernel);
        gfxCommandDispatch(gfx, 1, 1, 1);
        gfxCommandBindKernel(gfx, reduceIndirectKernel);
    }
    else
    {
        gfxCommandBindKernel(gfx, reduceKernel);
    }

    gfxProgramSetParameter(gfx, reduceProgram, "g_InputBuffer", sourceBuffer);
    if (indirect)
    {
        gfxProgramSetParameter(gfx, reduceProgram, "g_InputLength", *numKeys);
    }
    else
    {
        gfxProgramSetParameter(gfx, reduceProgram, "g_Count", maxNumKeys);
    }
    if (numGroups1 > 1)
    {
        // Create scratch buffer needed for loops
        const uint64_t typeSize = (((uint64_t)currentType % 4) + 1) * (currentType >= Type::Double ? sizeof(double) : sizeof(float));
        const uint64_t scratchBufferSize = numGroups1 * typeSize;
        if (!scratchBuffer || (scratchBuffer.getSize() < scratchBufferSize))
        {
            gfxDestroyBuffer(gfx, scratchBuffer);
            scratchBuffer = gfxCreateBuffer(gfx, scratchBufferSize);
            scratchBuffer.setStride(static_cast<uint32_t>(typeSize));
            scratchBuffer.setName("Capsaicin_Reduce_ScratchBuffer");
        }

        gfxProgramSetParameter(gfx, reduceProgram, "g_OutputBuffer", scratchBuffer);
        if (numGroups1 > 1)
        {
            // Run first loop
            if (indirect)
            {
                gfxCommandDispatchIndirect(gfx, indirectBuffer);
            }
            else
            {
                gfxCommandDispatch(gfx, numGroups1, 1, 1);
            }
            // Setup parameters for next loop
            gfxProgramSetParameter(gfx, reduceProgram, "g_InputBuffer", scratchBuffer);
            gfxProgramSetParameter(gfx, reduceProgram, "g_OutputBuffer", sourceBuffer);
            if (indirect)
            {
                gfxProgramSetParameter(gfx, reduceProgram, "g_InputLength", indirectCountBuffer);
            }
            else
            {
                gfxProgramSetParameter(gfx, reduceProgram, "g_Count", numGroups1);
            }
        }
        if (numGroups2 > 1)
        {
            // Run second loop
            if (indirect)
            {
                gfxCommandDispatchIndirect(gfx, indirectBuffer2);
            }
            else
            {
                gfxCommandDispatch(gfx, numGroups2, 1, 1);
            }
            // Setup parameters for next loop
            gfxProgramSetParameter(gfx, reduceProgram, "g_InputBuffer", sourceBuffer);
            if (indirect)
            {
                gfxProgramSetParameter(gfx, reduceProgram, "g_InputLength", indirectCountBuffer2);
            }
            else
            {
                gfxProgramSetParameter(gfx, reduceProgram, "g_Count", numGroups2);
            }
        }
    }

    // Perform final loop and put final result back into source buffer
    gfxProgramSetParameter(gfx, reduceProgram, "g_OutputBuffer", sourceBuffer);
    gfxCommandDispatch(gfx, 1, 1, 1);

    return true;
}
} // namespace Capsaicin
