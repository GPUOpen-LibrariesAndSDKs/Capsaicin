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

#include "capsaicin_internal_types.h"
#include "factory.h"
#include "render_technique.h"

#include <vector>

namespace Capsaicin
{
/** An abstract renderer class used to encapsulate all required operations to set up a rendering work flow. */
class Renderer
{
public:
    Renderer() noexcept = default;

    virtual ~Renderer() noexcept = default;

    Renderer(Renderer const &other)                = delete;
    Renderer(Renderer &&other) noexcept            = delete;
    Renderer &operator=(Renderer const &other)     = delete;
    Renderer &operator=(Renderer &&other) noexcept = delete;

    /**
     * Sets up the required render techniques.
     * @param renderOptions The current global render options.
     * @return A list of all required render techniques in the order that they are required. The calling
     * function takes all ownership of the returned list.
     */
    virtual std::vector<std::unique_ptr<RenderTechnique>> setupRenderTechniques(
        RenderOptionList const &renderOptions) noexcept = 0;

    /*
     * Gets any override render options for current renderer.
     * @note This is automatically called by the framework after setupRenderTechniques is called and should be
     * used to initialise any render options specific to the current renderer.
     * @return A list of all valid configuration options.
     */
    virtual RenderOptionList getRenderOptions() noexcept;
};

class RendererFactory : public Factory<Renderer>
{};
} // namespace Capsaicin
