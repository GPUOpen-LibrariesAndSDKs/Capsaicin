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
#include "components/component.h"
#include "components/light_sampler/light_sampler.h"
#include "light_sampler_grid_shared.h"

namespace Capsaicin
{
class LightSamplerGridCDF
    : public LightSampler
    , public ComponentFactory::Registrar<LightSamplerGridCDF>
    , public LightSamplerFactory::Registrar<LightSamplerGridCDF>
{
public:
    static constexpr std::string_view Name = "LightSamplerGridCDF";

    LightSamplerGridCDF(LightSamplerGridCDF const &) noexcept = delete;

    LightSamplerGridCDF(LightSamplerGridCDF &&) noexcept = default;

    /** Constructor. */
    LightSamplerGridCDF() noexcept;

    /** Destructor. */
    ~LightSamplerGridCDF() noexcept;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        uint32_t light_grid_cdf_num_cells = 16; /**< Maximum number of grid cells along any axis */
        uint32_t light_grid_cdf_lights_per_cell =
            0; /**< Maximum number of lights to store per grid cell (0 causes all lights to be included)*/
        bool light_grid_cdf_threshold = false; /**< Use a cutoff threshold value for light samples */
        bool light_grid_cdf_octahedron_sampling =
            false; /**< Use octahedron sampling for each cell to also sample by direction */
        bool light_grid_cdf_centroid_build =
            false; /**< Use faster but simpler cell centroid sampling during build */
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @returns The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    ComponentList getComponents() const noexcept override;

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
     * @note Must be called before LightSamplerGridCDF::run().
     * @param capsaicin Current framework context.
     * @return True if an update occurred requiring internal updates to be performed.
     */
    bool needsRecompile(CapsaicinInternal const &capsaicin) const noexcept override;

    /**
     * Get the list of shader defines that should be passed to any kernel that uses this lightSampler.
     * @note Also includes values from the default lightBuilder.
     * @param capsaicin Current framework context.
     * @return A vector with each required define.
     */
    std::vector<std::string> getShaderDefines(CapsaicinInternal const &capsaicin) const noexcept override;

    /**
     * Add the required program parameters to a shader based on current settings.
     * @note Also includes values from the default lightBuilder.
     * @param capsaicin Current framework context.
     * @param program   The shader program to bind parameters to.
     */
    void addProgramParameters(CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept override;

    /**
     * Check if the scenes lighting data was changed this frame.
     * @param capsaicin Current framework context.
     * @returns True if light data has changed.
     */
    bool getLightsUpdated(CapsaicinInternal const &capsaicin) const noexcept override;

    /**
     * Get the name of the header file used in HLSL code to include necessary sampler functions.
     * @return String name of the HLSL header include.
     */
    std::string_view getHeaderFile() const noexcept override;

private:
    bool initKernels(CapsaicinInternal const &capsaicin) noexcept;

    RenderOptions options;
    bool          recompileFlag =
        false; /**< Flag to indicate if option change requires a shader recompile this frame */
    bool lightsUpdatedFlag = false; /**< Flag to indicate if option change effects light samples */

    LightSamplingConfiguration config = {uint4 {0}, float3 {0}, float3 {0}, float3 {0}};
    GfxBuffer                  configBuffer; /**< Buffer used to hold LightSamplingConfiguration */
    GfxBuffer lightIndexBuffer; /**< Buffer used to hold light indexes for all lights in each cell */
    GfxBuffer lightCDFBuffer;   /**< Buffer used to hold light CDF for all lights in each cell */

    GfxProgram boundsProgram;
    GfxKernel  buildKernel;
};
} // namespace Capsaicin
