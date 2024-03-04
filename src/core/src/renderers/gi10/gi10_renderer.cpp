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

#include "atmosphere/atmosphere.h"
#include "gi10/gi10.h"
#include "renderer.h"
#include "skybox/skybox.h"
#include "ssgi/ssgi.h"
#include "taa/taa.h"
#include "taa/update_history.h"
#include "tone_mapping/tone_mapping.h"
#include "visibility_buffer/visibility_buffer.h"

namespace Capsaicin
{
/** The GI-1.0 renderer. */
class GI10Renderer
    : public Renderer
    , public RendererFactory::Registrar<GI10Renderer>
{
public:
    static constexpr std::string_view Name = "GI-1.1";

    /** Default constructor. */
    GI10Renderer() noexcept {};

    /**
     * Sets up the required render techniques.
     * @param renderOptions The current global render options.
     * @return A list of all required render techniques in the order that they are required. The calling
     * function takes all ownership of the returned list.
     */
    std::vector<std::unique_ptr<RenderTechnique>> setupRenderTechniques(
        [[maybe_unused]] RenderOptionList const &renderOptions) noexcept override
    {
        std::vector<std::unique_ptr<RenderTechnique>> render_techniques;
        render_techniques.emplace_back(std::make_unique<VisibilityBuffer>());
        render_techniques.emplace_back(std::make_unique<SSGI>());
        render_techniques.emplace_back(std::make_unique<GI10>());
        render_techniques.emplace_back(std::make_unique<Atmosphere>());
        render_techniques.emplace_back(std::make_unique<Skybox>());
        render_techniques.emplace_back(std::make_unique<UpdateHistory>());
        render_techniques.emplace_back(std::make_unique<TAA>());
        render_techniques.emplace_back(std::make_unique<ToneMapping>());
        return render_techniques;
    }
};
} // namespace Capsaicin
