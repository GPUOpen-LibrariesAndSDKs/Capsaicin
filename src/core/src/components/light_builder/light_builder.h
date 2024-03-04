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

#include "components/component.h"

namespace Capsaicin
{
class LightBuilder
    : public Component
    , public ComponentFactory::Registrar<LightBuilder>
{
public:
    static constexpr std::string_view Name = "LightBuilder";

    /** Constructor. */
    LightBuilder() noexcept;

    LightBuilder(LightBuilder const &) noexcept = delete;

    LightBuilder(LightBuilder &&) noexcept = default;

    /** Destructor. */
    virtual ~LightBuilder() noexcept;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept;

    struct RenderOptions
    {
        bool delta_light_enable       = true; /**< True to enable delta light in light sampling */
        bool area_light_enable        = true; /**< True to enable area lights in light sampling */
        bool environment_light_enable = true; /**< True to enable environment lights in light sampling */
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @returns The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @returns True if initialisation succeeded, False otherwise.
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
     * @param capsaicin Current framework context.
     * @returns True if an update occurred requiring internal updates to be performed.
     */
    virtual bool needsRecompile(CapsaicinInternal const &capsaicin) const noexcept;

    /**
     * Get the list of shader defines that should be passed to any kernel that uses the lightSampler.
     * @param capsaicin Current framework context.
     * @returns A vector with each required define.
     */
    virtual std::vector<std::string> getShaderDefines(CapsaicinInternal const &capsaicin) const noexcept;

    /**
     * Add the required program parameters to a shader based on current settings.
     * @param capsaicin Current framework context.
     * @param program   The shader program to bind parameters to.
     */
    virtual void addProgramParameters(CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept;

    /**
     * Gets count of enabled area lights in current scene.
     * @returns The area light count.
     */
    uint32_t getAreaLightCount() const;

    /**
     * Gets count of enabled delta lights (point,spot,direction) in current scene.
     * @returns The delta light count.
     */
    uint32_t getDeltaLightCount() const;

    /**
     * Gets approximate light count within the light buffer.
     * The light count is a maximum upper bound of possible lights in the light list. Since lights are culled
     * on the GPU it takes several frames for the exact value to be read back.
     * This should not be used in ant shader operations as @getLightCountBuffer() should be used instead.
     * @returns The light count.
     */
    uint32_t getLightCount() const;

    /**
     * Check if the scenes lighting data was changed this frame.
     * @returns True if light data has changed.
     */
    bool getLightsUpdated() const;

    /**
     * Check if the light settings have changed (i.e. enabled/disabled lights).
     * @returns True if light settings have changed.
     */
    bool getLightSettingsUpdated() const;

private:
    RenderOptions options;

    uint32_t areaLightTotal      = 0; /**< Number of area lights in meshes (may not be all enabled) */
    size_t   lightHash           = 0;
    uint32_t areaLightMaxCount   = 0; /**< Max number of area lights in light buffer */
    uint32_t areaLightCount      = 0; /**< Approximate number of area lights in light buffer */
    uint32_t deltaLightCount     = 0; /**< Number of delta lights in light buffer */
    uint32_t environmentMapCount = 0; /**< Number of environment map lights in buffer */
    uint32_t lightBufferIndex    = 0; /**< Index of currently active light buffer */

    bool lightsUpdated       = true;
    bool lightSettingChanged = true;

    GfxBuffer lightBuffers[2];        /**< Buffers used to hold all light list */
    GfxBuffer lightCountBuffer;       /**< Buffer used to hold number of lights in light buffer */
    GfxBuffer lightInstanceBuffer;    /**< Buffer used to hold the offset of the instance primitives */
    GfxBuffer
        lightInstancePrimitiveBuffer; /**< Buffer used to hold the light identifier per emissive primitive */
    std::vector<std::pair<bool, GfxBuffer>>
        lightCountBufferTemp; /**< Buffer used to copy light count into cpu memory */

    GfxKernel  countAreaLightsKernel;
    GfxKernel  scatterAreaLightsKernel;
    GfxProgram gatherAreaLightsProgram;
};
} // namespace Capsaicin
