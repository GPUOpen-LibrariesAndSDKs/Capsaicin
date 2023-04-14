/**********************************************************************
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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

#include "light_sampler_bounds.h"

#include "capsaicin_internal.h"
#include "components/stratified_sampler/stratified_sampler.h"
#include "light_sampler_bounds_shared.h"

namespace Capsaicin
{
LightSamplerBounds::~LightSamplerBounds() noexcept
{
    terminate();
}

RenderOptionList LightSamplerBounds::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(light_bounds_num_cells, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_bounds_lights_per_cell, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_bounds_threshold, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_bounds_cdf, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_bounds_uniform_sample, options));
    return newOptions;
}

LightSamplerBounds::RenderOptions LightSamplerBounds::convertOptions(RenderSettings const &settings) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(light_bounds_num_cells, newOptions, settings.options_)
    RENDER_OPTION_GET(light_bounds_lights_per_cell, newOptions, settings.options_)
    RENDER_OPTION_GET(light_bounds_threshold, newOptions, settings.options_)
    RENDER_OPTION_GET(light_bounds_cdf, newOptions, settings.options_)
    RENDER_OPTION_GET(light_bounds_uniform_sample, newOptions, settings.options_)
    return newOptions;
}

ComponentList LightSamplerBounds::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(LightSampler));
    components.emplace_back(COMPONENT_MAKE(StratifiedSampler));
    return components;
}

bool LightSamplerBounds::init(CapsaicinInternal const &capsaicin) noexcept
{
    initKernels(capsaicin);

    boundsLengthBuffer = gfxCreateBuffer<uint>(gfx_, 1);
    boundsLengthBuffer.setName("Capsaicin_LightSamplerBounds_BoundsCountBuffer");

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
    configBuffer.setName("Capsaicin_LightSamplerBounds_ConfigBuffer");

    initLightIndexBuffer();

    dispatchCommandBuffer = gfxCreateBuffer<DispatchCommand>(gfx_, 1);
    dispatchCommandBuffer.setName("Capsaicin_LightSamplerBounds_DispatchCommandBuffer");

    return !!boundsProgram;
}

void LightSamplerBounds::run(CapsaicinInternal &capsaicin) noexcept
{
    // Nothing to do as requires explicit call to LightSamplerBounds::update()
}

void LightSamplerBounds::reserveBoundsValues(uint32_t reserve, std::type_info const &caller) noexcept
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

void LightSamplerBounds::setBounds(
    std::pair<float3, float3> const &bounds, std::type_info const &caller) noexcept
{
    // Add to internal host reservations
    boundsHostReservations.emplace(caller.hash_code(), bounds);

    // Add an additional reserve spot in device buffer so that this can coexist with reserveBoundsValues
    reserveBoundsValues(1, this);
}

void LightSamplerBounds::update(CapsaicinInternal &capsaicin, RenderTechnique &parent) noexcept
{
    // Update internal options
    auto const &renderSettings     = capsaicin.getRenderSettings();
    auto const  optionsNew         = convertOptions(renderSettings);
    auto        lightSampler       = capsaicin.getComponent<LightSampler>();
    auto        stratified_sampler = capsaicin.getComponent<StratifiedSampler>();

    recompileFlag = optionsNew.light_bounds_cdf != options.light_bounds_cdf
                 || optionsNew.light_bounds_uniform_sample != options.light_bounds_uniform_sample
                 || optionsNew.light_bounds_threshold != options.light_bounds_threshold
                 || lightSampler->needsRecompile(capsaicin);
    lightingChanged = lightSampler->getLightsUpdated();
    options         = optionsNew;

    if (options.light_bounds_uniform_sample)
    {
        // If using uniform sampling skip building structure
        return;
    }

    const uint32_t numCells        = options.light_bounds_num_cells;
    const uint32_t lightsPerCell   = options.light_bounds_lights_per_cell - 1;
    const uint     lightDataLength = numCells * numCells * numCells * lightsPerCell;
    if (lightIndexBuffer.getCount() < lightDataLength && lightDataLength > 0)
    {
        gfxDestroyBuffer(gfx_, lightIndexBuffer);
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
                    gfxCreateBuffer(gfx_, sizeof(float) * 3, &newBounds.first, kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, boundsMinBuffer, ((size_t)boundsMaxLength - 1) * sizeof(float) * 3,
                    uploadMinBuffer, 0, sizeof(float) * 3);
                GfxBuffer uploadMaxBuffer =
                    gfxCreateBuffer(gfx_, sizeof(float) * 3, &newBounds.second, kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, boundsMaxBuffer, ((size_t)boundsMaxLength - 1) * sizeof(float) * 3,
                    uploadMaxBuffer, 0, sizeof(float) * 3);
            }
            else
            {
                GfxBuffer uploadMinBuffer =
                    gfxCreateBuffer(gfx_, sizeof(float) * 3, &newBounds.first, kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, boundsMinBuffer, 0, uploadMinBuffer, 0, sizeof(float) * 3);
                GfxBuffer uploadMaxBuffer =
                    gfxCreateBuffer(gfx_, sizeof(float) * 3, &newBounds.second, kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, boundsMaxBuffer, 0, uploadMaxBuffer, 0, sizeof(float) * 3);
            }
        }
    }

    // Update constants buffer
    GfxBuffer              samplingConstants = capsaicin.allocateConstantBuffer<LightSamplingConstants>(1);
    LightSamplingConstants constantData      = {};
    constantData.maxCellsPerAxis             = options.light_bounds_num_cells;
    constantData.maxNumLightsPerCell =
        options.light_bounds_lights_per_cell - 1; //-1 as need space for table header
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

    stratified_sampler->addProgramParameters(capsaicin, boundsProgram);

    // Create the light sampling structure bounds by reducing all values stored in the boundsMin|MaxBuffers
    {
        RenderTechnique::TimedSection const timedSection(parent, "CalculateLightSamplerBounds");

        if (boundsReservations.size() > (boundsHostReservations.empty() ? 0 : 1))
        {
            // Reduce Min/Max
            reducerMin.reduceIndirect(boundsMinBuffer, boundsLengthBuffer, boundsMaxLength);
            reducerMax.reduceIndirect(boundsMaxBuffer, boundsLengthBuffer, boundsMaxLength);
        }
    }

    // Calculate the required configuration values
    {
        RenderTechnique::TimedSection const timedSection(parent, "CalculateLightSamplerConfiguration");

        if (boundsReservations.size() > (boundsHostReservations.empty() ? 0 : 1) || hostUpdated)
        {
            // Calculate the required configuration values
            gfxCommandBindKernel(gfx_, calculateBoundsKernel);
            gfxCommandDispatch(gfx_, 1, 1, 1);
        }
    }

    // Create the light sampling structure
    {
        RenderTechnique::TimedSection const timedSection(parent, "BuildLightSampler");

        // Build the sampling structure
        gfxCommandBindKernel(gfx_, buildKernel);
        gfxCommandDispatchIndirect(gfx_, dispatchCommandBuffer);
    }

    // Release constant buffer
    gfxDestroyBuffer(gfx_, samplingConstants);

    // Clear boundsLengthBuffer
    gfxCommandClearBuffer(gfx_, boundsLengthBuffer, 0);
}

bool LightSamplerBounds::needsRecompile(CapsaicinInternal const &capsaicin) const noexcept
{
    return recompileFlag;
}

std::vector<std::string> LightSamplerBounds::getShaderDefines(
    CapsaicinInternal const &capsaicin) const noexcept
{
    auto                     lightSampler = capsaicin.getComponent<LightSampler>();
    std::vector<std::string> baseDefines(std::move(lightSampler->getShaderDefines(capsaicin)));
    if (options.light_bounds_threshold)
    {
        baseDefines.push_back("LIGHTSAMPLERBOUNDS_USE_THRESHOLD");
    }
    if (options.light_bounds_cdf)
    {
        baseDefines.push_back("LIGHTSAMPLERBOUNDS_USE_CDF");
    }
    if (options.light_bounds_uniform_sample)
    {
        baseDefines.push_back("LIGHTSAMPLERBOUNDS_USE_UNIFORM_SAMPLING");
    }
    return baseDefines;
}

void LightSamplerBounds::addProgramParameters(
    CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    auto lightSampler = capsaicin.getComponent<LightSampler>();
    lightSampler->addProgramParameters(capsaicin, program);

    // Bind the light sampling shader parameters
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_BoundsLength", boundsLengthBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_MinBounds", boundsMinBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_MaxBounds", boundsMaxBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_Configuration", configBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_CellsIndex", lightIndexBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_CellsCDF", lightCDFBuffer);
}

bool LightSamplerBounds::getLightsUpdated() const
{
    return lightingChanged;
}

void LightSamplerBounds::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, boundsLengthBuffer);
    gfxDestroyBuffer(gfx_, boundsMinBuffer);
    gfxDestroyBuffer(gfx_, boundsMaxBuffer);

    gfxDestroyBuffer(gfx_, configBuffer);
    gfxDestroyBuffer(gfx_, lightIndexBuffer);
    gfxDestroyBuffer(gfx_, lightCDFBuffer);

    gfxDestroyBuffer(gfx_, dispatchCommandBuffer);

    gfxDestroyKernel(gfx_, calculateBoundsKernel);
    gfxDestroyKernel(gfx_, buildKernel);
    gfxDestroyProgram(gfx_, boundsProgram);
}

bool LightSamplerBounds::initKernels(CapsaicinInternal const &capsaicin) noexcept
{
    boundsProgram = gfxCreateProgram(
        gfx_, "components/light_sampler_bounds/light_sampler_bounds", capsaicin.getShaderPath());
    auto                      baseDefines(std::move(getShaderDefines(capsaicin)));
    std::vector<char const *> defines;
    for (auto &i : baseDefines)
    {
        defines.push_back(i.c_str());
    }
    calculateBoundsKernel = gfxCreateComputeKernel(gfx_, boundsProgram, "CalculateBounds");
    buildKernel           = gfxCreateComputeKernel(
        gfx_, boundsProgram, "Build", defines.data(), static_cast<uint32_t>(defines.size()));

    return !!buildKernel;
}

bool LightSamplerBounds::initBoundsBuffers() noexcept
{
    boundsMinBuffer = gfxCreateBuffer<glm::vec3>(gfx_, boundsMaxLength);
    boundsMinBuffer.setName("Capsaicin_LightSamplerBounds_BoundsMinBuffer");
    boundsMaxBuffer = gfxCreateBuffer<glm::vec3>(gfx_, boundsMaxLength);
    boundsMaxBuffer.setName("Capsaicin_LightSamplerBounds_BoundsMaxBuffer");
    return !!boundsMaxBuffer;
}

bool LightSamplerBounds::initLightIndexBuffer() noexcept
{
    const uint32_t numCells        = options.light_bounds_num_cells;
    const uint32_t lightsPerCell   = options.light_bounds_lights_per_cell;
    const uint     lightDataLength = numCells * numCells * numCells * lightsPerCell;

    lightIndexBuffer = gfxCreateBuffer<uint>(gfx_, lightDataLength);
    lightIndexBuffer.setName("Capsaicin_LightSamplerBounds_IndexBuffer");
    lightCDFBuffer = gfxCreateBuffer<uint>(gfx_, lightDataLength);
    lightCDFBuffer.setName("Capsaicin_LightSamplerBounds_CDFBuffer");
    return !!lightCDFBuffer;
}
} // namespace Capsaicin
