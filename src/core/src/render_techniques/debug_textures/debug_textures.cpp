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

#include "debug_textures.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
DebugTextures::DebugTextures()
    : RenderTechnique("Debug Textures")
{}

DebugTextures::~DebugTextures()
{
    terminate();
}

RenderOptionList DebugTextures::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(debug_textures_id, options));
    newOptions.emplace(RENDER_OPTION_MAKE(debug_textures_mip, options));
    newOptions.emplace(RENDER_OPTION_MAKE(debug_textures_alpha, options));
    return newOptions;
}

DebugTextures::RenderOptions DebugTextures::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(debug_textures_id, newOptions, options)
    RENDER_OPTION_GET(debug_textures_mip, newOptions, options)
    RENDER_OPTION_GET(debug_textures_alpha, newOptions, options)
    return newOptions;
}

SharedTextureList DebugTextures::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Debug", SharedTexture::Access::Write});
    return textures;
}

DebugViewList DebugTextures::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("DebugTexture");
    return views;
}

bool DebugTextures::init(CapsaicinInternal const &capsaicin) noexcept
{
    program = capsaicin.createProgram("render_techniques/debug_textures/debug_textures");

    GfxDrawState const drawState = {};
    gfxDrawStateSetColorTarget(drawState, 0, capsaicin.getSharedTexture("Debug").getFormat());
    kernel = gfxCreateGraphicsKernel(gfx_, program, drawState);

    return !!kernel;
}

void DebugTextures::render(CapsaicinInternal &capsaicin) noexcept
{
    options = convertOptions(capsaicin.getOptions());
    if (auto const debugView = capsaicin.getCurrentDebugView(); debugView == "DebugTexture")
    {
        // Sample the requested texture into the debug view
        auto const &textures = capsaicin.getTextures();
        gfxProgramSetParameter(
            gfx_, program, "g_TextureMaps", textures.data(), static_cast<uint32_t>(textures.size()));
        options.debug_textures_id =
            glm::clamp(options.debug_textures_id, uint32_t {0}, static_cast<uint32_t>(textures.size()));
        options.debug_textures_mip = glm::clamp(
            options.debug_textures_mip, uint32_t {0}, textures[options.debug_textures_id].getMipLevels());
        gfxProgramSetParameter(gfx_, program, "g_Sampler", capsaicin.getNearestSampler());
        gfxProgramSetParameter(gfx_, program, "g_TextureID", options.debug_textures_id);
        gfxProgramSetParameter(gfx_, program, "g_mipLevel", options.debug_textures_mip);
        gfxProgramSetParameter(gfx_, program, "g_Alpha", options.debug_textures_alpha);
        gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("Debug"));
        gfxCommandBindKernel(gfx_, kernel);
        gfxCommandDraw(gfx_, 3);
    }
}

void DebugTextures::terminate() noexcept
{
    gfxDestroyProgram(gfx_, program);
    program = {};
    gfxDestroyKernel(gfx_, kernel);
    kernel = {};
}

void DebugTextures::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept
{
    if (auto const debugView = capsaicin.getCurrentDebugView(); debugView == "DebugTexture")
    {
        auto const &textures = capsaicin.getTextures();
        std::string textureList;
        for (auto const &i : textures)
        {
            textureList += i.getName();
            textureList += '\0';
        }
        auto selectedTexture = static_cast<int32_t>(capsaicin.getOption<uint32_t>("debug_textures_id"));
        if (ImGui::Combo(
                "Texture", &selectedTexture, textureList.c_str(), static_cast<int32_t>(textures.size())))
        {
            capsaicin.setOption("debug_textures_id", static_cast<uint32_t>(selectedTexture));
        }
        int32_t selectedMip = static_cast<int32_t>(capsaicin.getOption<uint32_t>("debug_textures_mip"));
        if (ImGui::DragInt("Mip Level", &selectedMip, 1, 0,
                static_cast<int32_t>(textures[static_cast<uint32_t>(selectedTexture)].getMipLevels())))
        {
            capsaicin.setOption("debug_textures_mip", static_cast<uint32_t>(selectedMip));
        }
        bool useAlpha = options.debug_textures_alpha;
        if (ImGui::Checkbox("View Alpha Channel", &useAlpha))
        {
            capsaicin.setOption("debug_textures_alpha", useAlpha);
        }
    }
}
} // namespace Capsaicin
