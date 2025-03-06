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

#include "combine.h"

#include "capsaicin_internal.h"

namespace Capsaicin
{
Combine::Combine()
    : RenderTechnique("Combine")
{}

Combine::~Combine()
{
    terminate();
}

SharedTextureList Combine::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::Write});

    textures.push_back({.name = "DirectLighting", .flags = SharedTexture::Flags::Optional});
    textures.push_back({.name = "GlobalIllumination", .flags = SharedTexture::Flags::Optional});
    textures.push_back({.name = "Emission", .flags = SharedTexture::Flags::Optional});
    textures.push_back({
        .name   = "PrevCombinedIllumination",
        .flags  = SharedTexture::Flags::Optional,
        .format = DXGI_FORMAT_R16G16B16A16_FLOAT,
    });
    return textures;
}

bool Combine::init(CapsaicinInternal const &capsaicin) noexcept
{
    std::vector<char const *> defines;
    if (capsaicin.hasSharedTexture("DirectLighting"))
    {
        defines.push_back("HAS_DIRECT_LIGHTING_BUFFER");
    }
    if (capsaicin.hasSharedTexture("GlobalIllumination"))
    {
        defines.push_back("HAS_GLOBAL_ILLUMINATION_BUFFER");
    }
    if (capsaicin.hasSharedTexture("Emission"))
    {
        defines.push_back("HAS_EMISSION_BUFFER");
    }
    if (capsaicin.hasSharedTexture("PrevCombinedIllumination"))
    {
        defines.push_back("HAS_BACKUP_BUFFER");
    }
    combineProgram = capsaicin.createProgram("render_techniques/combine/combine");
    combineKernel  = gfxCreateComputeKernel(
        gfx_, combineProgram, "main", defines.data(), static_cast<uint32_t>(defines.size()));

    return !!combineKernel;
}

void Combine::render(CapsaicinInternal &capsaicin) noexcept
{
    bool const direct   = capsaicin.hasSharedTexture("DirectLighting");
    bool const global   = capsaicin.hasSharedTexture("GlobalIllumination");
    bool const emissive = capsaicin.hasSharedTexture("Emission");
    bool const backup   = capsaicin.hasSharedTexture("PrevCombinedIllumination");

    if (!direct && !global && !emissive && !backup)
    {
        // Nothing to do
        return;
    }

    gfxProgramSetParameter(gfx_, combineProgram, "g_ColorBuffer", capsaicin.getSharedTexture("Color"));
    if (direct)
    {
        gfxProgramSetParameter(
            gfx_, combineProgram, "g_DirectLightingBuffer", capsaicin.getSharedTexture("DirectLighting"));
    }
    if (global)
    {
        gfxProgramSetParameter(gfx_, combineProgram, "g_GlobalIlluminationBuffer",
            capsaicin.getSharedTexture("GlobalIllumination"));
    }
    if (emissive)
    {
        gfxProgramSetParameter(
            gfx_, combineProgram, "g_EmissionBuffer", capsaicin.getSharedTexture("Emission"));
    }
    if (backup)
    {
        // Backup combined colour in cases where its needed next frame but must not contain any other
        // modifications that may occur later in the pipeline (taa, tonemap etc.)
        gfxProgramSetParameter(gfx_, combineProgram, "g_PrevCombinedIllumination",
            capsaicin.getSharedTexture("PrevCombinedIllumination"));
    }

    auto const      renderDimensions = capsaicin.getRenderDimensions();
    uint32_t const *numThreads       = gfxKernelGetNumThreads(gfx_, combineKernel);
    uint32_t const  numGroupsX       = (renderDimensions.x + numThreads[0] - 1) / numThreads[0];
    uint32_t const  numGroupsY       = (renderDimensions.y + numThreads[1] - 1) / numThreads[1];

    gfxCommandBindKernel(gfx_, combineKernel);
    gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
}

void Combine::terminate() noexcept
{
    gfxDestroyProgram(gfx_, combineProgram);
    gfxDestroyKernel(gfx_, combineKernel);
}
} // namespace Capsaicin
