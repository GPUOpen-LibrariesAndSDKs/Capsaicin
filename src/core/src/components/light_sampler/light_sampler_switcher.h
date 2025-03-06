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

#include "capsaicin_internal.h"
#include "components/light_sampler/light_sampler.h"

namespace Capsaicin
{
class LightSamplerSwitcher final
    : public Component
    , ComponentFactory::Registrar<LightSamplerSwitcher>
{
public:
    static constexpr std::string_view Name = "LightSamplerSwitcher";

    /** Constructor. */
    LightSamplerSwitcher() noexcept;

    /** Destructor. */
    ~LightSamplerSwitcher() noexcept override;

    LightSamplerSwitcher(LightSamplerSwitcher const &other)                = delete;
    LightSamplerSwitcher(LightSamplerSwitcher &&other) noexcept            = delete;
    LightSamplerSwitcher &operator=(LightSamplerSwitcher const &other)     = delete;
    LightSamplerSwitcher &operator=(LightSamplerSwitcher &&other) noexcept = delete;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        uint32_t light_sampler_type = 1; /**< The light sampler to use */
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @return The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    [[nodiscard]] ComponentList getComponents() const noexcept override;

    /**
     * Gets a list of any shared buffers used by the current component.
     * @return A list of all supported buffers.
     */
    [[nodiscard]] SharedBufferList getSharedBuffers() const noexcept override;

    /**
     * Gets the required list of shared textures needed for the current component.
     * @return A list of all required shared textures.
     */
    [[nodiscard]] SharedTextureList getSharedTextures() const noexcept override;

    /**
     * Gets a list of any debug views provided by the current render technique.
     * @return A list of all supported debug views.
     */
    [[nodiscard]] DebugViewList getDebugViews() const noexcept override;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(CapsaicinInternal const &capsaicin) noexcept override;

    /**
     * Run internal operations.
     * @param [in,out] capsaicin Current framework context.
     */
    void run(CapsaicinInternal &capsaicin) noexcept override;

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

    /**
     * Render GUI options.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void renderGUI(CapsaicinInternal &capsaicin) const noexcept override;

    /**
     * Check to determine if any kernels using light sampler code need to be (re)compiled.
     * @note Must be called before run().
     * @param capsaicin Current framework context.
     * @return True if an update occurred requiring internal updates to be performed.
     */
    [[nodiscard]] bool needsRecompile(CapsaicinInternal const &capsaicin) const noexcept;

    /**
     * Get the list of shader defines that should be passed to any kernel that uses this lightSampler.
     * @param capsaicin Current framework context.
     * @return A vector with each required define.
     */
    [[nodiscard]] std::vector<std::string> getShaderDefines(
        CapsaicinInternal const &capsaicin) const noexcept;

    /**
     * Add the required program parameters to a shader based on current settings.
     * @param capsaicin Current framework context.
     * @param program   The shader program to bind parameters to.
     */
    void addProgramParameters(CapsaicinInternal const &capsaicin, GfxProgram const &program) const noexcept;

    /**
     * Check if the light settings have changed (i.e. enabled/disabled lights).
     * @param capsaicin Current framework context.
     * @return True if light settings have changed.
     */
    [[nodiscard]] bool getLightSettingsUpdated(CapsaicinInternal const &capsaicin) const noexcept;

    /**
     * Gets number of timestamp queries.
     * @return The timestamp query count.
     */
    [[nodiscard]] uint32_t getTimestampQueryCount() const noexcept override;

    /**
     * Gets timestamp queries.
     * @return The timestamp queries.
     */
    [[nodiscard]] std::vector<TimestampQuery> const &getTimestampQueries() const noexcept override;

    /** Resets the timed section queries */
    void resetQueries() noexcept override;

    /**
     * Sets internal graphics context
     * @param gfx The gfx context.
     */
    void setGfxContext(GfxContext const &gfx) noexcept override;

private:
    RenderOptions                 options;
    std::unique_ptr<LightSampler> currentSampler = nullptr; /**< The currently active light sampler */
    bool samplerChanged = true; /**< Flag indicating if a sampler change has occurred */
};
} // namespace Capsaicin
