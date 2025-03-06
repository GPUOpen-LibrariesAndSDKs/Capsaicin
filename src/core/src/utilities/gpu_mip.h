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
#pragma once

#include "gpu_shared.h"

#include <gfx.h>

namespace Capsaicin
{
class CapsaicinInternal;

/** A helper utility class to perform GPU accelerated mip-map generation. */
class GPUMip
{
public:
    /** Defaulted constructor. */
    GPUMip() noexcept = default;

    /** Destructor. */
    ~GPUMip() noexcept;

    /** Type of data in 2D texture to mip. */
    enum class Type : uint8_t
    {
        R = 0,
        RG,
        RGB,
        RGBA,
        sRGB,
        sRGBA,
        DepthMin,
        DepthMax,
    };

    /**
     * Initialise the internal data based on current configuration.
     * @param gfxIn         Active gfx context.
     * @param shaderPaths Path to shader files based on current working directory.
     * @param type        The input texture data type.
     * @return True, if any initialisation/changes succeeded.
     */
    bool initialise(GfxContext const &gfxIn, std::vector<std::string> const &shaderPaths, Type type) noexcept;

    /**
     * Initialise the internal data based on current configuration.
     * @param capsaicin Current framework context.
     * @param type       The input texture data type.
     * @return True, if any initialisation/changes succeeded.
     */
    bool initialise(CapsaicinInternal const &capsaicin, Type type) noexcept;

    /**
     * Create a complete chain of mip maps for an input texture.
     * @param inputTexture The input texture to create mips for (must have enough allocated space for mip
     * levels).
     * @return True, if operation succeeded.
     */
    bool mip(GfxTexture const &inputTexture) noexcept;

    /**
     * Create a complete chain of mip maps for an input texture and save them in a second texture.
     * @param inputTexture  The input texture to create mips for.
     * @param outputTexture The output texture that will hold the generated mips (must have enough
     * allocated space for mip levels).
     * @return True, if operation succeeded.
     */
    bool mip(GfxTexture const &inputTexture, GfxTexture const &outputTexture) noexcept;

private:
    /** Terminates and cleans up this object. */
    void terminate() noexcept;

    bool mipInternal(GfxTexture const &inputTexture, GfxTexture const &outputTexture) noexcept;

    GfxContext gfx;

    Type       currentType = Type::sRGB;
    GfxProgram mipProgramPower2;
    GfxKernel  mipKernelPower2;
    GfxProgram mipProgramNonPower2;
    GfxKernel  mipKernelNonPower2;
    GfxBuffer  mipTempBuffer;
    GfxBuffer  mipSPDCountBuffer;
};
} // namespace Capsaicin
