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

#include "stratified_sampler.h"

#include "capsaicin_internal.h"
#include "stratified_sampler_data.h"

#include <random>

namespace Capsaicin
{
StratifiedSampler::StratifiedSampler() noexcept
    : Component(Name)
{}

StratifiedSampler::~StratifiedSampler() noexcept
{
    terminate();
}

RenderOptionList StratifiedSampler::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(stratified_sampler_deterministic, options));
    return newOptions;
}

StratifiedSampler::RenderOptions StratifiedSampler::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(stratified_sampler_deterministic, newOptions, options)
    return newOptions;
}

bool StratifiedSampler::init(CapsaicinInternal const &capsaicin) noexcept
{
    uint64_t const seedBufferSize =
        sizeof(uint32_t) * std::max(capsaicin.getWidth(), 1920u) * std::max(capsaicin.getHeight(), 1080u);

    std::vector<uint32_t>                   seedBufferData;
    seedBufferData.reserve(seedBufferSize);
    if (options.stratified_sampler_deterministic)
    {
        std::mt19937 gen(5489U);
        for (uint32_t i = 0; i < seedBufferSize; i += 4)
        {
            seedBufferData.push_back(gen());
        }
    }
    else
    {
        std::random_device                      rd;
        std::mt19937                            gen(rd());
        for (uint32_t i = 0; i < seedBufferSize; i += 4)
        {
            seedBufferData.push_back(gen());
        }
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
    // Check for option changed
    auto const optionsNew = convertOptions(capsaicin.getOptions());
    bool update = optionsNew.stratified_sampler_deterministic != options.stratified_sampler_deterministic;
    options = optionsNew;

    // Check if seed buffer needs to be re-initialised
    uint64_t const seedBufferSize =
        sizeof(uint32_t) * std::max(capsaicin.getWidth(), 1920u) * std::max(capsaicin.getHeight(), 1080u);

    if (update || seedBufferSize > seedBuffer.getSize())
    {
        GfxCommandEvent const command_event(gfx_, "InitStratifiedSampler");

        gfxDestroyBuffer(gfx_, seedBuffer);

        init(capsaicin);
    }
}

void StratifiedSampler::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, seedBuffer);
    seedBuffer = {};
    gfxDestroyBuffer(gfx_, sobolBuffer);
    sobolBuffer = {};
}

void StratifiedSampler::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_SeedBuffer", seedBuffer);
    gfxProgramSetParameter(gfx_, program, "g_SobolXorsBuffer", sobolBuffer);
}
} // namespace Capsaicin
