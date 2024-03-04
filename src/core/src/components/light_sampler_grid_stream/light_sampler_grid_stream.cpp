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

#include "light_sampler_grid_stream.h"

#include "../light_builder/light_builder.h"
#include "../light_sampler_grid_cdf/light_sampler_grid_shared.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
LightSamplerGridStream::LightSamplerGridStream() noexcept
    : LightSampler(Name)
{}

LightSamplerGridStream::~LightSamplerGridStream() noexcept
{
    terminate();
}

RenderOptionList LightSamplerGridStream::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_stream_num_cells, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_stream_lights_per_cell, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_stream_octahedron_sampling, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_stream_resample, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_stream_merge_type, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_stream_parallel_build, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_stream_centroid_build, options));
    return newOptions;
}

LightSamplerGridStream::RenderOptions LightSamplerGridStream::convertOptions(
    RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(light_grid_stream_num_cells, newOptions, options)
    RENDER_OPTION_GET(light_grid_stream_lights_per_cell, newOptions, options)
    RENDER_OPTION_GET(light_grid_stream_octahedron_sampling, newOptions, options)
    RENDER_OPTION_GET(light_grid_stream_resample, newOptions, options)
    RENDER_OPTION_GET(light_grid_stream_merge_type, newOptions, options)
    RENDER_OPTION_GET(light_grid_stream_parallel_build, newOptions, options)
    RENDER_OPTION_GET(light_grid_stream_centroid_build, newOptions, options)
    return newOptions;
}

ComponentList LightSamplerGridStream::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(LightBuilder));
    return components;
}

bool LightSamplerGridStream::init(CapsaicinInternal const &capsaicin) noexcept
{
    initKernels(capsaicin);

    boundsLengthBuffer = gfxCreateBuffer<uint>(gfx_, 1);
    boundsLengthBuffer.setName("Capsaicin_LightSamplerGridStream_BoundsCountBuffer");

    initBoundsBuffers();

    reducerMin.initialise(capsaicin, GPUReduce::Type::Float3, GPUReduce::Operation::Min);
    reducerMax.initialise(capsaicin, GPUReduce::Type::Float3, GPUReduce::Operation::Max);

    struct LightSamplingConfiguration
    {
        uint4  numCells;
        float3 cellSize;
        float  pack;
        float3 sceneMin;
        float  pack2;
        float3 sceneExtent;
    };

    configBuffer = gfxCreateBuffer<LightSamplingConfiguration>(gfx_, 1);
    configBuffer.setName("Capsaicin_LightSamplerGrid_ConfigBuffer");

    initLightIndexBuffer();

    dispatchCommandBuffer = gfxCreateBuffer<DispatchCommand>(gfx_, 1);
    dispatchCommandBuffer.setName("Capsaicin_LightSamplerGrid_DispatchCommandBuffer");

    return !!boundsProgram;
}

void LightSamplerGridStream::run(CapsaicinInternal &capsaicin) noexcept
{
    if (boundsReservations.size() > (boundsHostReservations.empty() ? 0 : 1))
    {
        // Nothing to do as requires explicit call to LightSamplerGridStream::update()
    }
    else
    {
        // When not being run using device side bounds values then we perform an update here
        // Update light sampling data structure
        if (capsaicin.getMeshesUpdated() || capsaicin.getTransformsUpdated()
            || boundsHostReservations.empty() /*i.e. un-initialised*/)
        {
            // Update the light sampler using scene bounds
            auto sceneBounds = capsaicin.getSceneBounds();
            setBounds(sceneBounds, this);
        }

        update(capsaicin, this);
    }
}

void LightSamplerGridStream::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, boundsLengthBuffer);
    boundsLengthBuffer = {};
    gfxDestroyBuffer(gfx_, boundsMinBuffer);
    boundsMinBuffer = {};
    gfxDestroyBuffer(gfx_, boundsMaxBuffer);
    boundsMaxBuffer = {};

    gfxDestroyBuffer(gfx_, configBuffer);
    configBuffer = {};
    gfxDestroyBuffer(gfx_, lightIndexBuffer);
    lightIndexBuffer = {};
    gfxDestroyBuffer(gfx_, lightReservoirBuffer);
    lightReservoirBuffer = {};

    gfxDestroyBuffer(gfx_, dispatchCommandBuffer);
    dispatchCommandBuffer = {};

    gfxDestroyKernel(gfx_, calculateBoundsKernel);
    calculateBoundsKernel = {};
    gfxDestroyKernel(gfx_, buildKernel);
    buildKernel = {};
    gfxDestroyProgram(gfx_, boundsProgram);
    boundsProgram = {};
}

void LightSamplerGridStream::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    if (ImGui::CollapsingHeader("Light Sampler Settings", ImGuiTreeNodeFlags_None))
    {
        ImGui::DragInt("Max Cells per Axis",
            (int32_t *)&capsaicin.getOption<uint32_t>("light_grid_stream_num_cells"), 1, 1, 128);
        auto lightBuilder = capsaicin.getComponent<LightBuilder>();
        ImGui::DragInt("Number Lights per Cell",
            (int32_t *)&capsaicin.getOption<uint32_t>("light_grid_stream_lights_per_cell"), 1, 1,
            lightBuilder->getLightCount());
        // ImGui::Checkbox("Octahedral Sampling",
        // &capsaicin.getOption<bool>("light_grid_stream_octahedron_sampling"));
        ImGui::Checkbox(
            "Fast Centroid Build", &capsaicin.getOption<bool>("light_grid_stream_centroid_build"));
        ImGui::Combo("Merge Algorithm",
            reinterpret_cast<int32_t *>(&capsaicin.getOption<uint32_t>("light_grid_stream_merge_type")),
            "Random Select\0Without Replacement\0With Replacement");
        ImGui::Checkbox("Intermediate Resample", &capsaicin.getOption<bool>("light_grid_stream_resample"));
        ImGui::Checkbox("Parallel Build", &capsaicin.getOption<bool>("light_grid_stream_parallel_build"));
    }
}

void LightSamplerGridStream::reserveBoundsValues(uint32_t reserve, std::type_info const &caller) noexcept
{
    boundsReservations.emplace(caller.hash_code(), reserve);
    // Determine if current buffer needs to be reallocated
    uint32_t elements = 0;
    for (auto &i : boundsReservations)
    {
        elements += i.second;
    }
    boundsMaxLength = elements / 32; // Currently assumes wavefront size of 32 and reduces due to use of
                                     // WaveActiveMin in shader code

    if (boundsMinBuffer.getCount() < boundsMaxLength && boundsMaxLength > 0)
    {
        gfxDestroyBuffer(gfx_, boundsMinBuffer);
        gfxDestroyBuffer(gfx_, boundsMaxBuffer);
        initBoundsBuffers();
    }
}

void LightSamplerGridStream::setBounds(
    std::pair<float3, float3> const &bounds, std::type_info const &caller) noexcept
{
    // Add to internal host reservations
    boundsHostReservations.emplace(caller.hash_code(), bounds);

    // Add an additional reserve spot in device buffer so that this can coexist with reserveBoundsValues
    reserveBoundsValues(1, this);
}

void LightSamplerGridStream::update(CapsaicinInternal &capsaicin, Timeable *parent) noexcept
{
    // Update internal options
    auto const optionsNew   = convertOptions(capsaicin.getOptions());
    auto       lightBuilder = capsaicin.getComponent<LightBuilder>();

    // Check if many lights kernel should be run. This requires greater than 128 lights per reservoir as
    // otherwise there will be empty reservoirs. Note: The 128 must match the LS_GRID_STREAM_THREADREDUCE
    // value which is currently set to the largest possible wave size. If targeting a specific platform then
    // matching this value with the wave size will give better results.
    bool manyLights = options.light_grid_stream_parallel_build
                   && (lightBuilder->getLightCount() > 128 * options.light_grid_stream_lights_per_cell);

    recompileFlag =
        optionsNew.light_grid_stream_octahedron_sampling != options.light_grid_stream_octahedron_sampling
        || optionsNew.light_grid_stream_resample != options.light_grid_stream_resample
        || optionsNew.light_grid_stream_merge_type != options.light_grid_stream_merge_type
        || optionsNew.light_grid_stream_centroid_build != options.light_grid_stream_centroid_build
        || usingManyLights != manyLights || lightBuilder->needsRecompile(capsaicin);
    lightsUpdatedFlag =
        optionsNew.light_grid_stream_octahedron_sampling != options.light_grid_stream_octahedron_sampling
        || optionsNew.light_grid_stream_lights_per_cell != options.light_grid_stream_lights_per_cell
        || optionsNew.light_grid_stream_num_cells != options.light_grid_stream_num_cells
        || optionsNew.light_grid_stream_centroid_build != options.light_grid_stream_centroid_build
        || usingManyLights != manyLights;
    options         = optionsNew;
    usingManyLights = manyLights;

    const uint32_t numCells = options.light_grid_stream_num_cells;
    uint lightDataLength    = numCells * numCells * numCells * options.light_grid_stream_lights_per_cell;
    if (options.light_grid_stream_octahedron_sampling)
    {
        lightDataLength *= 8;
    }
    if (lightIndexBuffer.getCount() < lightDataLength && lightDataLength > 0)
    {
        gfxDestroyBuffer(gfx_, lightIndexBuffer);
        gfxDestroyBuffer(gfx_, lightReservoirBuffer);
        initLightIndexBuffer();
    }

    if (recompileFlag)
    {
        gfxDestroyKernel(gfx_, calculateBoundsKernel);
        gfxDestroyKernel(gfx_, buildKernel);
        gfxDestroyProgram(gfx_, boundsProgram);

        initKernels(capsaicin);
    }

    // Calculate host side maximum bounds
    bool hostUpdated = false;
    if (!boundsHostReservations.empty())
    {
        // Sum up everything in boundsHostReservations
        std::pair<float3, float3> newBounds = boundsHostReservations[0];
        for (auto &i :
            std::ranges::subrange(++boundsHostReservations.cbegin(), boundsHostReservations.cend()))
        {
            newBounds.first  = glm::min(newBounds.first, i.second.first);
            newBounds.second = glm::max(newBounds.second, i.second.second);
        }

        // Check if the host side bounds needs to be uploaded
        if (newBounds != currentBounds)
        {
            currentBounds = newBounds;
            hostUpdated   = true;
            // Check if there are also any device side reservations, if so then upload the host value to the
            // last slot so that it will participate in reduceMinMax but wont be overwritten each frame
            if (boundsReservations.size() > 1)
            {
                // Copy to last element boundsMinBuffer and boundsMaxBuffer
                GfxBuffer uploadMinBuffer =
                    gfxCreateBuffer<float>(gfx_, 3, &newBounds.first, kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, boundsMinBuffer, ((size_t)boundsMaxLength - 1) * sizeof(float) * 3,
                    uploadMinBuffer, 0, sizeof(float) * 3);
                GfxBuffer uploadMaxBuffer =
                    gfxCreateBuffer<float>(gfx_, 3, &newBounds.second, kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, boundsMaxBuffer, ((size_t)boundsMaxLength - 1) * sizeof(float) * 3,
                    uploadMaxBuffer, 0, sizeof(float) * 3);
                gfxDestroyBuffer(gfx_, uploadMinBuffer);
                gfxDestroyBuffer(gfx_, uploadMaxBuffer);
            }
            else
            {
                GfxBuffer uploadMinBuffer =
                    gfxCreateBuffer<float>(gfx_, 3, &newBounds.first, kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, boundsMinBuffer, 0, uploadMinBuffer, 0, sizeof(float) * 3);
                GfxBuffer uploadMaxBuffer =
                    gfxCreateBuffer<float>(gfx_, 3, &newBounds.second, kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, boundsMaxBuffer, 0, uploadMaxBuffer, 0, sizeof(float) * 3);
                gfxDestroyBuffer(gfx_, uploadMinBuffer);
                gfxDestroyBuffer(gfx_, uploadMaxBuffer);
            }
        }
    }

    // Update constants buffer
    GfxBuffer              samplingConstants = capsaicin.allocateConstantBuffer<LightSamplingConstants>(1);
    LightSamplingConstants constantData      = {};
    constantData.maxCellsPerAxis             = numCells;
    constantData.maxNumLightsPerCell         = options.light_grid_stream_lights_per_cell;
    gfxBufferGetData<LightSamplingConstants>(gfx_, samplingConstants)[0] = constantData;

    // Add program parameters
    addProgramParameters(capsaicin, boundsProgram);
    gfxProgramSetParameter(gfx_, boundsProgram, "g_DispatchCommandBuffer", dispatchCommandBuffer);
    gfxProgramSetParameter(gfx_, boundsProgram, "g_LightSampler_Constants", samplingConstants);

    gfxProgramSetParameter(gfx_, boundsProgram, "g_EnvironmentBuffer", capsaicin.getEnvironmentBuffer());
    gfxProgramSetParameter(
        gfx_, boundsProgram, "g_TextureMaps", capsaicin.getTextures(), capsaicin.getTextureCount());
    gfxProgramSetParameter(gfx_, boundsProgram, "g_TextureSampler", capsaicin.getLinearSampler());

    gfxProgramSetParameter(gfx_, boundsProgram, "g_FrameIndex", capsaicin.getFrameIndex());

    // Create the light sampling structure bounds by reducing all values stored in the boundsMin|MaxBuffers
    if (boundsReservations.size() > (boundsHostReservations.empty() ? 0 : 1))
    {
        RenderTechnique::TimedSection const timedSection(*parent, "CalculateLightSamplerBounds");
        // Reduce Min/Max
        reducerMin.reduceIndirect(boundsMinBuffer, boundsLengthBuffer, boundsMaxLength);
        reducerMax.reduceIndirect(boundsMaxBuffer, boundsLengthBuffer, boundsMaxLength);
        // Clear boundsLengthBuffer
        gfxCommandClearBuffer(gfx_, boundsLengthBuffer, 0);
    }

    // Calculate the required configuration values
    if (boundsReservations.size() > (boundsHostReservations.empty() ? 0 : 1) || hostUpdated
        || lightsUpdatedFlag || recompileFlag)
    {
        RenderTechnique::TimedSection const timedSection(*parent, "CalculateLightSamplerConfiguration");
        // Calculate the required configuration values
        gfxCommandBindKernel(gfx_, calculateBoundsKernel);
        gfxCommandDispatch(gfx_, 1, 1, 1);
    }

    // Create the light sampling structure
    {
        RenderTechnique::TimedSection const timedSection(*parent, "BuildLightSampler");

        // Build the sampling structure
        gfxCommandBindKernel(gfx_, buildKernel);
        gfxCommandDispatchIndirect(gfx_, dispatchCommandBuffer);
    }

    // Release constant buffer
    gfxDestroyBuffer(gfx_, samplingConstants);
}

bool LightSamplerGridStream::needsRecompile(
    [[maybe_unused]] CapsaicinInternal const &capsaicin) const noexcept
{
    return recompileFlag;
}

std::vector<std::string> LightSamplerGridStream::getShaderDefines(
    CapsaicinInternal const &capsaicin) const noexcept
{
    auto                     lightBuilder = capsaicin.getComponent<LightBuilder>();
    std::vector<std::string> baseDefines(std::move(lightBuilder->getShaderDefines(capsaicin)));
    if (options.light_grid_stream_octahedron_sampling)
    {
        baseDefines.push_back("LIGHTSAMPLERSTREAM_USE_OCTAHEDRON_SAMPLING");
    }
    if (options.light_grid_stream_resample)
    {
        baseDefines.push_back("LIGHTSAMPLERSTREAM_RES_USE_RESAMPLE");
    }
    if (options.light_grid_stream_merge_type == 1)
    {
        baseDefines.push_back("LIGHTSAMPLERSTREAM_RES_FAST_MERGE");
    }
    else if (options.light_grid_stream_merge_type == 0)
    {
        baseDefines.push_back("LIGHTSAMPLERSTREAM_RES_RANDOM_MERGE");
    }
    if (options.light_grid_stream_centroid_build)
    {
        baseDefines.push_back("LIGHT_SAMPLE_VOLUME_CENTROID");
    }
    if (usingManyLights)
    {
        baseDefines.push_back("LIGHTSAMPLERSTREAM_RES_MANYLIGHTS");
    }
    return baseDefines;
}

void LightSamplerGridStream::addProgramParameters(
    CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    auto lightBuilder = capsaicin.getComponent<LightBuilder>();
    lightBuilder->addProgramParameters(capsaicin, program);

    // Bind the light sampling shader parameters
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_BoundsLength", boundsLengthBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_MinBounds", boundsMinBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_MaxBounds", boundsMaxBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_Configuration", configBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_CellsIndex", lightIndexBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_CellsReservoirs", lightReservoirBuffer);
}

bool LightSamplerGridStream::getLightsUpdated(CapsaicinInternal const &capsaicin) const noexcept
{
    auto lightBuilder = capsaicin.getComponent<LightBuilder>();
    return lightsUpdatedFlag || lightBuilder->getLightsUpdated();
}

std::string_view LightSamplerGridStream::getHeaderFile() const noexcept
{
    return std::string_view("\"../../components/light_sampler_grid_stream/light_sampler_grid_stream.hlsl\"");
}

bool LightSamplerGridStream::initKernels(CapsaicinInternal const &capsaicin) noexcept
{
    boundsProgram = gfxCreateProgram(
        gfx_, "components/light_sampler_grid_stream/light_sampler_grid_stream", capsaicin.getShaderPath());
    auto                      baseDefines(std::move(getShaderDefines(capsaicin)));
    std::vector<char const *> defines;
    for (auto &i : baseDefines)
    {
        defines.push_back(i.c_str());
    }
    calculateBoundsKernel = gfxCreateComputeKernel(
        gfx_, boundsProgram, "CalculateBounds", defines.data(), static_cast<uint32_t>(defines.size()));
    buildKernel = gfxCreateComputeKernel(
        gfx_, boundsProgram, "Build", defines.data(), static_cast<uint32_t>(defines.size()));
    return !!buildKernel;
}

bool LightSamplerGridStream::initBoundsBuffers() noexcept
{
    boundsMinBuffer = gfxCreateBuffer<glm::vec3>(gfx_, boundsMaxLength);
    boundsMinBuffer.setName("Capsaicin_LightSamplerGrid_BoundsMinBuffer");
    boundsMaxBuffer = gfxCreateBuffer<glm::vec3>(gfx_, boundsMaxLength);
    boundsMaxBuffer.setName("Capsaicin_LightSamplerGrid_BoundsMaxBuffer");
    gfxCommandClearBuffer(gfx_, boundsLengthBuffer, 0);
    return !!boundsMaxBuffer;
}

bool LightSamplerGridStream::initLightIndexBuffer() noexcept
{
    const uint32_t numCells        = options.light_grid_stream_num_cells;
    const uint32_t lightsPerCell   = options.light_grid_stream_lights_per_cell;
    uint           lightDataLength = numCells * numCells * numCells * lightsPerCell;
    if (options.light_grid_stream_octahedron_sampling)
    {
        lightDataLength *= 8;
    }

    lightIndexBuffer = gfxCreateBuffer<uint>(gfx_, lightDataLength);
    lightIndexBuffer.setName("Capsaicin_LightSamplerGrid_IndexBuffer");
    lightReservoirBuffer =
        gfxCreateBuffer<float2>(gfx_, lightDataLength); // Only need half as much if use resampling
    lightReservoirBuffer.setName("Capsaicin_LightSamplerGrid_ReservoirBuffer");
    return !!lightReservoirBuffer;
}
} // namespace Capsaicin
