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
#include "capsaicin.h"

#include "capsaicin_internal.h"
#include "components/light_sampler/light_sampler.h"
#include "thread_pool.h"

namespace
{
Capsaicin::CapsaicinInternal *g_renderer = nullptr;
} // unnamed namespace

namespace Capsaicin
{
char const *g_play_modes[] = {"None", "Frame-by-frame"};

static_assert(
    ARRAYSIZE(g_play_modes) == Capsaicin::kPlayMode_Count, "An invalid number of play modes was supplied");

void Initialize(GfxContext gfx)
{
    if (g_renderer != nullptr) Terminate();
    ThreadPool::Create(std::thread::hardware_concurrency());
    g_renderer = new CapsaicinInternal();
    g_renderer->initialize(gfx);
}

std::vector<std::string_view> GetRenderers() noexcept
{
    return CapsaicinInternal::GetRenderers();
}

std::vector<std::string_view> GetAOVs() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getAOVs();
    return {};
}

std::vector<std::string_view> GetDebugViews() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getDebugViews();
    return {};
}

void Render(GfxScene scene, RenderSettings &render_settings)
{
    if (g_renderer != nullptr) g_renderer->render(scene, render_settings);
}

uint32_t GetFrameIndex()
{
    if (g_renderer != nullptr) return g_renderer->getFrameIndex();

    return 0;
}

double GetSequenceTime()
{
    if (g_renderer != nullptr) return g_renderer->getTime();

    return 0;
}

void SetSequenceTime(double time)
{
    if (g_renderer != nullptr) g_renderer->setTime(time);
}

bool GetAnimate()
{
    if (g_renderer != nullptr) return g_renderer->getAnimate();

    return false;
}

void SetAnimate(bool animation)
{
    if (g_renderer != nullptr) g_renderer->setAnimate(animation);
}

uint32_t GetDeltaLightCount()
{
    if (g_renderer != nullptr)
    {
        if (g_renderer->hasComponent("LightSampler"))
        {
            return g_renderer->getComponent<LightSampler>()->getDeltaLightCount();
        }
    }
    return 0;
}

uint32_t GetAreaLightCount()
{
    if (g_renderer != nullptr)
    {
        if (g_renderer->hasComponent("LightSampler"))
        {
            return g_renderer->getComponent<LightSampler>()->getAreaLightCount();
        }
    }
    return 0;
}

uint32_t GetEnvironmentLightCount()
{
    if (g_renderer != nullptr) return g_renderer->getEnvironmentLightCount();
    return 0;
}

std::pair<float, std::vector<NodeTimestamps>> GetProfiling() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getProfiling();
    return {};
}

void Terminate()
{
    delete g_renderer;
    g_renderer = nullptr;
    ThreadPool::Destroy();
}

void DumpAOVBuffer(char const *file_path, std::string_view const &aov)
{
    if (g_renderer != nullptr) g_renderer->dumpAOVBuffer(file_path, aov);
}
} // namespace Capsaicin
