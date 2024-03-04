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

#include "light_sampler_grid_cdf.h"

#include "../light_builder/light_builder.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
LightSamplerGridCDF::LightSamplerGridCDF() noexcept
    : LightSampler(Name)
{}

LightSamplerGridCDF::~LightSamplerGridCDF() noexcept
{
    terminate();
}

RenderOptionList LightSamplerGridCDF::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_cdf_num_cells, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_cdf_lights_per_cell, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_cdf_threshold, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_cdf_octahedron_sampling, options));
    newOptions.emplace(RENDER_OPTION_MAKE(light_grid_cdf_centroid_build, options));
    return newOptions;
}

LightSamplerGridCDF::RenderOptions LightSamplerGridCDF::convertOptions(
    RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(light_grid_cdf_num_cells, newOptions, options)
    RENDER_OPTION_GET(light_grid_cdf_lights_per_cell, newOptions, options)
    RENDER_OPTION_GET(light_grid_cdf_threshold, newOptions, options)
    RENDER_OPTION_GET(light_grid_cdf_octahedron_sampling, newOptions, options)
    RENDER_OPTION_GET(light_grid_cdf_centroid_build, newOptions, options)
    return newOptions;
}

ComponentList LightSamplerGridCDF::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(LightBuilder));
    return components;
}

bool LightSamplerGridCDF::init(CapsaicinInternal const &capsaicin) noexcept
{
    initKernels(capsaicin);

    configBuffer = gfxCreateBuffer<LightSamplingConfiguration>(gfx_, 1);
    configBuffer.setName("Capsaicin_LightSamplerGridCDF_ConfigBuffer");

    return !!boundsProgram;
}

void LightSamplerGridCDF::run(CapsaicinInternal &capsaicin) noexcept
{
    // Update internal options
    auto const optionsNew   = convertOptions(capsaicin.getOptions());
    auto       lightBuilder = capsaicin.getComponent<LightBuilder>();

    recompileFlag =
        optionsNew.light_grid_cdf_threshold != options.light_grid_cdf_threshold
        || optionsNew.light_grid_cdf_octahedron_sampling != options.light_grid_cdf_octahedron_sampling
        || optionsNew.light_grid_cdf_centroid_build != options.light_grid_cdf_centroid_build
        || (optionsNew.light_grid_cdf_lights_per_cell != options.light_grid_cdf_lights_per_cell
            && (optionsNew.light_grid_cdf_lights_per_cell == 0
                || options.light_grid_cdf_lights_per_cell == 0))
        || lightBuilder->needsRecompile(capsaicin);
    lightsUpdatedFlag =
        optionsNew.light_grid_cdf_octahedron_sampling != options.light_grid_cdf_octahedron_sampling
        || optionsNew.light_grid_cdf_lights_per_cell != options.light_grid_cdf_lights_per_cell
        || optionsNew.light_grid_cdf_num_cells != options.light_grid_cdf_num_cells
        || optionsNew.light_grid_cdf_centroid_build != options.light_grid_cdf_centroid_build;
    options = optionsNew;

    if (recompileFlag)
    {
        gfxDestroyKernel(gfx_, buildKernel);
        gfxDestroyProgram(gfx_, boundsProgram);

        initKernels(capsaicin);
    }

    if (capsaicin.getMeshesUpdated() || capsaicin.getTransformsUpdated() || lightsUpdatedFlag
        || lightBuilder->getLightsUpdated() || recompileFlag || capsaicin.getFrameIndex() == 0
        || config.numCells.x == 0 /*i.e. uninitialised*/)
    {
        // Update the light sampler using scene bounds
        auto [sceneMin, sceneMax] = capsaicin.getSceneBounds();

        // Ensure each cell is square
        const float3 sceneExtent = sceneMax - sceneMin;
        float const  largestAxis = glm::max(sceneExtent.x, glm::max(sceneExtent.y, sceneExtent.z));
        float const  cellScale   = largestAxis / options.light_grid_cdf_num_cells;
        const float3 cellNum     = ceil(sceneExtent / cellScale);

        // Clamp max number of lights to those actually available
        uint lightsPerCell =
            (options.light_grid_cdf_lights_per_cell == 0)
                ? lightBuilder->getLightCount()
                : glm::min(options.light_grid_cdf_lights_per_cell, lightBuilder->getLightCount());
        lightsPerCell += 1; // There is 1 extra slot used for cell table header

        // Create updated configuration
        config.numCells    = uint4((uint3)cellNum, lightsPerCell);
        config.cellSize    = sceneExtent / cellNum;
        config.sceneMin    = sceneMin;
        config.sceneExtent = sceneExtent;

        GfxBuffer const uploadBuffer =
            gfxCreateBuffer<LightSamplingConfiguration>(gfx_, 1, &config, kGfxCpuAccess_Write);
        gfxCommandCopyBuffer(gfx_, configBuffer, uploadBuffer);
        gfxDestroyBuffer(gfx_, uploadBuffer);
    }

    uint lightDataLength = config.numCells.x * config.numCells.y * config.numCells.z * config.numCells.w;
    if (options.light_grid_cdf_octahedron_sampling)
    {
        lightDataLength *= 8;
    }
    if (lightIndexBuffer.getCount() < lightDataLength && lightDataLength > 0)
    {
        gfxDestroyBuffer(gfx_, lightIndexBuffer);
        gfxDestroyBuffer(gfx_, lightCDFBuffer);

        lightIndexBuffer = gfxCreateBuffer<uint>(gfx_, lightDataLength);
        lightIndexBuffer.setName("Capsaicin_LightSamplerGridCDF_IndexBuffer");
        lightCDFBuffer = gfxCreateBuffer<uint>(gfx_, lightDataLength);
        lightCDFBuffer.setName("Capsaicin_LightSamplerGridCDF_CDFBuffer");

        lightsUpdatedFlag = true;
    }

    // Create the light sampling structure
    if (lightBuilder->getLightsUpdated() || lightsUpdatedFlag || recompileFlag)
    {
        RenderTechnique::TimedSection const timedSection(*this, "BuildLightSampler");

        // Add program parameters
        addProgramParameters(capsaicin, boundsProgram);

        gfxProgramSetParameter(gfx_, boundsProgram, "g_EnvironmentBuffer", capsaicin.getEnvironmentBuffer());
        gfxProgramSetParameter(
            gfx_, boundsProgram, "g_TextureMaps", capsaicin.getTextures(), capsaicin.getTextureCount());
        gfxProgramSetParameter(gfx_, boundsProgram, "g_TextureSampler", capsaicin.getLinearSampler());

        gfxProgramSetParameter(gfx_, boundsProgram, "g_FrameIndex", capsaicin.getFrameIndex());

        // Get total number of grid cells
        uint3 groups = (uint3)config.numCells;
        if (options.light_grid_cdf_octahedron_sampling)
        {
            groups.x *= 8;
        }

        // Build the sampling structure
        uint32_t const *numThreads = gfxKernelGetNumThreads(gfx_, buildKernel);
        uint32_t const  numGroupsX = (groups.x + numThreads[0] - 1) / numThreads[0];
        uint32_t const  numGroupsY = (groups.y + numThreads[1] - 1) / numThreads[1];
        uint32_t const  numGroupsZ = (groups.z + numThreads[2] - 1) / numThreads[2];
        gfxCommandBindKernel(gfx_, buildKernel);
        gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, numGroupsZ);
    }
}

void LightSamplerGridCDF::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, configBuffer);
    configBuffer = {};
    gfxDestroyBuffer(gfx_, lightIndexBuffer);
    lightIndexBuffer = {};
    gfxDestroyBuffer(gfx_, lightCDFBuffer);
    lightCDFBuffer = {};

    gfxDestroyKernel(gfx_, buildKernel);
    buildKernel = {};
    gfxDestroyProgram(gfx_, boundsProgram);
    boundsProgram = {};
}

void LightSamplerGridCDF::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    if (ImGui::CollapsingHeader("Light Sampler Settings", ImGuiTreeNodeFlags_None))
    {
        auto lightBuilder = capsaicin.getComponent<LightBuilder>();
        ImGui::DragInt("Max Cells per Axis",
            (int32_t *)&capsaicin.getOption<uint32_t>("light_grid_cdf_num_cells"), 1, 1, 128);
        bool autoLights    = capsaicin.getOption<uint32_t>("light_grid_cdf_lights_per_cell") == 0;
        auto currentLights = lightBuilder->getLightCount();
        if (autoLights)
        {
            ImGui::BeginDisabled();
            ImGui::DragInt("Number Lights per Cell", (int32_t *)&currentLights, 1, 1, currentLights);
            ImGui::EndDisabled();
        }
        else
        {
            ImGui::DragInt("Number Lights per Cell",
                (int32_t *)&capsaicin.getOption<uint32_t>("light_grid_cdf_lights_per_cell"), 1, 1,
                currentLights);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Auto", &autoLights))
        {
            capsaicin.setOption<uint32_t>("light_grid_cdf_lights_per_cell", autoLights ? 0 : currentLights);
        }
        ImGui::Checkbox(
            "Cull Low Contributing Lights", &capsaicin.getOption<bool>("light_grid_cdf_threshold"));
        // ImGui::Checkbox("Octahedral Sampling",
        // &capsaicin.getOption<bool>("light_grid_cdf_octahedron_sampling"));
        ImGui::Checkbox("Fast Centroid Build", &capsaicin.getOption<bool>("light_grid_cdf_centroid_build"));
    }
}

bool LightSamplerGridCDF::needsRecompile([[maybe_unused]] CapsaicinInternal const &capsaicin) const noexcept
{
    return recompileFlag;
}

std::vector<std::string> LightSamplerGridCDF::getShaderDefines(
    CapsaicinInternal const &capsaicin) const noexcept
{
    auto                     lightBuilder = capsaicin.getComponent<LightBuilder>();
    std::vector<std::string> baseDefines(std::move(lightBuilder->getShaderDefines(capsaicin)));
    if (options.light_grid_cdf_threshold)
    {
        baseDefines.push_back("LIGHTSAMPLERCDF_USE_THRESHOLD");
    }
    if (options.light_grid_cdf_octahedron_sampling)
    {
        baseDefines.push_back("LIGHTSAMPLERCDF_USE_OCTAHEDRON_SAMPLING");
    }
    if (options.light_grid_cdf_centroid_build)
    {
        baseDefines.push_back("LIGHT_SAMPLE_VOLUME_CENTROID");
    }
    if (options.light_grid_cdf_lights_per_cell == 0)
    {
        baseDefines.push_back("LIGHTSAMPLERCDF_HAS_ALL_LIGHTS");
    }
    return baseDefines;
}

void LightSamplerGridCDF::addProgramParameters(
    CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    auto lightBuilder = capsaicin.getComponent<LightBuilder>();
    lightBuilder->addProgramParameters(capsaicin, program);

    // Bind the light sampling shader parameters
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_Configuration", configBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_CellsIndex", lightIndexBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightSampler_CellsCDF", lightCDFBuffer);
}

bool LightSamplerGridCDF::getLightsUpdated(CapsaicinInternal const &capsaicin) const noexcept
{
    auto lightBuilder = capsaicin.getComponent<LightBuilder>();
    return lightsUpdatedFlag || lightBuilder->getLightsUpdated();
}

std::string_view LightSamplerGridCDF::getHeaderFile() const noexcept
{
    return std::string_view("\"../../components/light_sampler_grid_cdf/light_sampler_grid_cdf.hlsl\"");
}

bool LightSamplerGridCDF::initKernels(CapsaicinInternal const &capsaicin) noexcept
{
    boundsProgram = gfxCreateProgram(
        gfx_, "components/light_sampler_grid_cdf/light_sampler_grid_cdf", capsaicin.getShaderPath());
    auto                      baseDefines(std::move(getShaderDefines(capsaicin)));
    std::vector<char const *> defines;
    for (auto &i : baseDefines)
    {
        defines.push_back(i.c_str());
    }
    buildKernel = gfxCreateComputeKernel(
        gfx_, boundsProgram, "Build", defines.data(), static_cast<uint32_t>(defines.size()));
    return !!buildKernel;
}
} // namespace Capsaicin
