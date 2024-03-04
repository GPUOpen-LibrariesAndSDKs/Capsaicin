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
#include "capsaicin.h"

#include "capsaicin_internal.h"
#include "thread_pool.h"

namespace
{
Capsaicin::CapsaicinInternal *g_renderer = nullptr;
} // unnamed namespace

namespace Capsaicin
{
void Initialize(GfxContext gfx, ImGuiContext *imgui_context) noexcept
{
    if (g_renderer != nullptr) Terminate();
    ThreadPool::Create(std::thread::hardware_concurrency());
    g_renderer = new CapsaicinInternal();
    g_renderer->initialize(gfx, imgui_context);
}

std::vector<std::string_view> GetRenderers() noexcept
{
    return CapsaicinInternal::GetRenderers();
}

std::string_view GetCurrentRenderer() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getCurrentRenderer();
    return "";
}

bool SetRenderer(std::string_view const &name) noexcept
{
    if (g_renderer != nullptr) return g_renderer->setRenderer(name);
    return false;
}

std::vector<std::string> GetCurrentScenes() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getCurrentScenes();
    return {};
}

bool SetScenes(std::vector<std::string> const &names) noexcept
{
    if (g_renderer != nullptr) return g_renderer->setScenes(names);
    return false;
}

std::vector<std::string_view> GetSceneCameras() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getSceneCameras();
    return {};
}

std::string_view GetSceneCurrentCamera() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getSceneCurrentCamera();
    return "";
}

GfxRef<GfxCamera> GetSceneCamera() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getSceneCamera();
    return {};
}

bool SetSceneCamera(std::string_view const &name) noexcept
{
    if (g_renderer != nullptr) return g_renderer->setSceneCamera(name);
    return false;
}

std::string GetCurrentEnvironmentMap() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getCurrentEnvironmentMap();
    return "";
}

bool SetEnvironmentMap(std::string const &name) noexcept
{
    if (g_renderer != nullptr) return g_renderer->setEnvironmentMap(name);
    return false;
}

std::vector<std::string_view> GetDebugViews() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getDebugViews();
    return {};
}

std::string_view GetCurrentDebugView() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getCurrentDebugView();
    return "";
}

bool SetDebugView(std::string_view const &name) noexcept
{
    if (g_renderer != nullptr) return g_renderer->setDebugView(name);
    return false;
}

std::vector<std::string_view> GetAOVs() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getAOVs();
    return {};
}

void Render() noexcept
{
    if (g_renderer != nullptr) g_renderer->render();
}

void RenderGUI(bool readOnly) noexcept
{
    if (g_renderer != nullptr) g_renderer->renderGUI(readOnly);
}

uint32_t GetFrameIndex() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getFrameIndex();

    return 0;
}

double GetFrameTime() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getFrameTime();
    return 0.0;
}

double GetAverageFrameTime() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getAverageFrameTime();
    return 0.0;
}

bool HasAnimation() noexcept
{
    if (g_renderer != nullptr) return g_renderer->hasAnimation();
    return false;
}

void SetPaused(bool paused) noexcept
{
    if (g_renderer != nullptr) g_renderer->setPaused(paused);
}

bool GetPaused() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getPaused();
    return true;
}

void SetFixedFrameRate(bool playMode) noexcept
{
    if (g_renderer != nullptr) g_renderer->setFixedFrameRate(playMode);
}

void SetFixedFrameTime(double fixed_frame_time) noexcept
{
    if (g_renderer != nullptr) g_renderer->setFixedFrameTime(fixed_frame_time);
}

bool GetFixedFrameRate() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getFixedFrameRate();
    return false;
}

void RestartPlayback() noexcept
{
    if (g_renderer != nullptr) g_renderer->restartPlayback();
}

void IncreasePlaybackSpeed() noexcept
{
    if (g_renderer != nullptr) g_renderer->increasePlaybackSpeed();
}

void DecreasePlaybackSpeed() noexcept
{
    if (g_renderer != nullptr) g_renderer->decreasePlaybackSpeed();
}

double GetPlaybackSpeed() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getPlaybackSpeed();
    return 1.0;
}

void ResetPlaybackSpeed() noexcept
{
    if (g_renderer != nullptr) g_renderer->resetPlaybackSpeed();
}

void StepPlaybackForward(uint32_t frames) noexcept
{
    if (g_renderer != nullptr) g_renderer->stepPlaybackForward(frames);
}

void StepPlaybackBackward(uint32_t frames) noexcept
{
    if (g_renderer != nullptr) g_renderer->stepPlaybackBackward(frames);
}

void SetPlayRewind(bool rewind) noexcept
{
    if (g_renderer != nullptr) g_renderer->setPlayRewind(rewind);
}

bool GetPlayRewind() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getPlayRewind();
    return false;
}

void SetRenderPaused(bool paused) noexcept
{
    if (g_renderer != nullptr) g_renderer->setRenderPaused(paused);
}

bool GetRenderPaused() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getRenderPaused();
    return true;
}

void StepJitterFrameIndex(uint32_t frames)
{
    if (g_renderer != nullptr) g_renderer->stepJitterFrameIndex(frames);
}

uint32_t GetDeltaLightCount() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getDeltaLightCount();
    return 0;
}

uint32_t GetAreaLightCount() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getAreaLightCount();
    return 0;
}

uint32_t GetEnvironmentLightCount() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getEnvironmentLightCount();
    return 0;
}

uint32_t GetTriangleCount() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getTriangleCount();
    return 0;
}

uint64_t GetBvhDataSize() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getBvhDataSize();
    return 0;
}

RenderOptionList &GetOptions() noexcept
{
    if (g_renderer != nullptr) return g_renderer->getOptions();
    static RenderOptionList nullList;
    return nullList;
}

void Terminate() noexcept
{
    delete g_renderer;
    g_renderer = nullptr;
    ThreadPool::Destroy();
}

CAPSAICIN_EXPORT void ReloadShaders() noexcept
{
    if (g_renderer != nullptr) g_renderer->reloadShaders();
}

void DumpAOVBuffer(char const *file_path, std::string_view const &aov) noexcept
{
    if (g_renderer != nullptr) g_renderer->dumpAOVBuffer(file_path, aov);
}

void DumpCamera(char const *file_path, bool jittered) noexcept
{
    if (g_renderer != nullptr) g_renderer->dumpCamera(file_path, jittered);
}

} // namespace Capsaicin
