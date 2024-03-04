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
#include "components/light_sampler_grid_cdf/light_sampler_grid_cdf.h"
#include "utilities/gpu_reduce.h"

namespace Capsaicin
{
class LightSamplerGridStream
    : public LightSampler
    , public ComponentFactory::Registrar<LightSamplerGridStream>
    , public LightSamplerFactory::Registrar<LightSamplerGridStream>
{
public:
    static constexpr std::string_view Name = "LightSamplerGridStream";

    LightSamplerGridStream(LightSamplerGridStream const &) noexcept = delete;

    LightSamplerGridStream(LightSamplerGridStream &&) noexcept = default;

    /** Constructor. */
    LightSamplerGridStream() noexcept;

    /** Destructor. */
    ~LightSamplerGridStream() noexcept;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        uint32_t light_grid_stream_num_cells = 16; /**< Maximum number of grid cells along any axis */
        uint32_t light_grid_stream_lights_per_cell =
            64; /**< Maximum number of lights to store per grid cell */
        bool light_grid_stream_octahedron_sampling =
            false; /**< Use octahedron sampling for each cell to also sample by direction */
        bool light_grid_stream_resample =
            true; /**< Use resampling based on local illumination when using reservoir sampling */
        uint32_t light_grid_stream_merge_type =
            1; /**< Select merge type, 0: Random reservoir selection, 1: Merge without replacement, 2: Merge
                  with replacement (allows high importance lights to be multiply sampled) */
        bool light_grid_stream_parallel_build =
            false; /**< Use faster reservoir parallel build on scene with large light count */
        bool light_grid_stream_centroid_build =
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
     * Reserve memory used for calculating bounds min/max values at runtime.
     * When using the hlsl `requestLightSampleLocation` function adequate storage must be reserved in order to
     * store the values by using this function. Alternatively setBounds() can be used to
     * directly set the bounds from the host.
     * @param reserve The number of position values to reserve space for.
     * @param caller  The technique or component that is reserving memory.
     */
    void reserveBoundsValues(uint32_t reserve, std::type_info const &caller) noexcept;

    template<typename T>
    void reserveBoundsValues(uint32_t reserve, [[maybe_unused]] T const *const caller) noexcept
    {
        reserveBoundsValues(reserve, typeid(T));
    }

    /**
     * Directly set the bounds that the sampler covers.
     * Alternatively if calculating bounds dynamically at runtime the reserveBoundsValues() function can be
     * used instead. This function can be skipped if just using whole scene bounds.
     * @param bounds The volume bounds.
     */
    void setBounds(std::pair<float3, float3> const &bounds, std::type_info const &caller) noexcept;

    template<typename T>
    void setBounds(std::pair<float3, float3> const &bounds, [[maybe_unused]] T const *const) noexcept
    {
        setBounds(bounds, typeid(T));
    }

    /**
     * Perform operations to update internal data structure.
     * @note Must be called after all hlsl `requestLightSampleLocation` calls have been made and before any
     * hlsl `sampleLightList` calls are made. If not using `requestLightSampleLocation` then this function
     * can be ignored.
     * @param [in,out] capsaicin Current framework context.
     * @param [in,out] parent    The parent render technique that is using the light sampler.
     */
    void update(CapsaicinInternal &capsaicin, Timeable *parent) noexcept;

    /**
     * Check to determine if any kernels using light sampler code need to be (re)compiled.
     * @note Must be called before run().
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
    bool initBoundsBuffers() noexcept;
    bool initLightIndexBuffer() noexcept;

    RenderOptions options;
    bool          recompileFlag =
        false; /**< Flag to indicate if option change requires a shader recompile this frame */
    bool lightsUpdatedFlag = false; /**< Flag to indicate if option change effects light samples */
    bool usingManyLights   = false; /**< Flag indicating if ,any lights parallel build is in use */

    std::map<size_t, uint32_t>
        boundsReservations; /**< List of any reservations made using reserveBoundsValues() */
    std::map<size_t, std::pair<float3, float3>>
        boundsHostReservations; /**< List of any reservations made using reserveBoundsValues() */
    std::pair<float3, float3> currentBounds = std::make_pair(float3(std::numeric_limits<float>::max()),
        float3(std::numeric_limits<float>::lowest())); /**< Current calculated bounds for all host bounds
                                                              in boundsHostReservations */
    uint32_t  boundsMaxLength = 1;                     /**< Allocated length of internal min/max buffers */
    GfxBuffer boundsLengthBuffer; /**< Buffer used to hold number of items in bounds(Min/Max)Buffer */
    GfxBuffer boundsMinBuffer;    /**< Buffer containing the min positions of reservoir locations */
    GfxBuffer boundsMaxBuffer;    /**< Buffer containing the max positions of reservoir locations */

    GPUReduce reducerMin; /**< Helper to perform GPU parallel reduceMin */
    GPUReduce reducerMax; /**< Helper to perform GPU parallel reduceMax */

    GfxBuffer configBuffer;         /**< Buffer used to hold LightSamplingConfiguration */
    GfxBuffer lightIndexBuffer;     /**< Buffer used to hold light indexes for all lights in each cell */
    GfxBuffer lightReservoirBuffer; /**< Buffer used to hold light reservoirs for each cell */

    GfxBuffer dispatchCommandBuffer;

    GfxProgram boundsProgram;
    GfxKernel  calculateBoundsKernel;
    GfxKernel  buildKernel;
};
} // namespace Capsaicin
