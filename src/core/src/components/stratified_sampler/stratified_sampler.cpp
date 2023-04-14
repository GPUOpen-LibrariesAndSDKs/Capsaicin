/**********************************************************************
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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

#include "stratified_sampler.h"

#include "capsaicin_internal.h"
#include "stratified_sampler_data.h"

#include <random>

namespace Capsaicin
{
StratifiedSampler::~StratifiedSampler() noexcept
{
    gfxDestroyBuffer(gfx_, seedBuffer);
    gfxDestroyBuffer(gfx_, sobolBuffer);
}

bool StratifiedSampler::init(CapsaicinInternal const &capsaicin) noexcept
{
    uint64_t const seedBufferSize =
        sizeof(uint32_t) * std::max(capsaicin.getWidth(), 1920u) * std::max(capsaicin.getHeight(), 1080u);

    std::vector<uint32_t>                   seedBufferData;
    std::random_device                      rd;
    std::mt19937                            gen(rd());
    std::uniform_int_distribution<uint32_t> distrib(0, std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < seedBufferSize; i += 4)
    {
        seedBufferData.push_back(distrib(gen));
    }
    seedBuffer = gfxCreateBuffer<uint32_t>(gfx_, (uint32_t)seedBufferData.size(), seedBufferData.data());
    seedBuffer.setName("StratifiedSampler_SeedBuffer");

    if (!sobolBuffer)
    {
        sobolBuffer = gfxCreateBuffer<uint32_t>(gfx_, (uint32_t)sizeof(sobolData), sobolData);
        sobolBuffer.setName("StratifiedSampler_SobolBuffer");
    }
    return true;
}

void StratifiedSampler::run(CapsaicinInternal &capsaicin) noexcept
{
    // Check if seed buffer needs to be re-initialised
    uint64_t const seedBufferSize =
        sizeof(uint32_t) * std::max(capsaicin.getWidth(), 1920u) * std::max(capsaicin.getHeight(), 1080u);

    if (seedBufferSize > seedBuffer.getSize())
    {
        GfxCommandEvent const command_event(gfx_, "InitStratifiedSampler");

        gfxDestroyBuffer(gfx_, seedBuffer);

        init(capsaicin);
    }
}

void StratifiedSampler::addProgramParameters(
    CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_SeedBuffer", seedBuffer);
    gfxProgramSetParameter(gfx_, program, "g_SobolXorsBuffer", sobolBuffer);
}
} // namespace Capsaicin
