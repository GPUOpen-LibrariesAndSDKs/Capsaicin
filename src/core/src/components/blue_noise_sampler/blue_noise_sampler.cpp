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

#include "blue_noise_sampler.h"

#include "blue_noise_sampler_samples.h" // Separate file as does not play well with IDEs
#include "capsaicin_internal.h"

namespace Capsaicin
{
/** Constructor. */

inline BlueNoiseSampler::BlueNoiseSampler() noexcept
    : Component(Name)
{}

BlueNoiseSampler::~BlueNoiseSampler() noexcept
{
    terminate();
}

bool BlueNoiseSampler::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    sobolBuffer          = gfxCreateBuffer(gfx_, sizeof(Sobol256x256), Sobol256x256);
    rankingTileBuffer    = gfxCreateBuffer(gfx_, sizeof(RankingTiles), RankingTiles);
    scramblingTileBuffer = gfxCreateBuffer(gfx_, sizeof(ScramblingTiles), ScramblingTiles);
    return true;
}

void BlueNoiseSampler::run([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    // Nothing to do
}

void BlueNoiseSampler::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, sobolBuffer);
    gfxDestroyBuffer(gfx_, rankingTileBuffer);
    gfxDestroyBuffer(gfx_, scramblingTileBuffer);
}

void BlueNoiseSampler::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_SobolBuffer", sobolBuffer);
    gfxProgramSetParameter(gfx_, program, "g_RankingTile", rankingTileBuffer);
    gfxProgramSetParameter(gfx_, program, "g_ScramblingTile", scramblingTileBuffer);
}
} // namespace Capsaicin
