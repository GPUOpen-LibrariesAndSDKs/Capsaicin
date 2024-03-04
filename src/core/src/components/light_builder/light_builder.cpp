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

#include "light_builder.h"

#include "capsaicin_internal.h"
#include "hash_reduce.h"
#include "render_technique.h"

namespace Capsaicin
{
LightBuilder::LightBuilder() noexcept
    : Component(Name)
{}

LightBuilder::~LightBuilder() noexcept
{
    terminate();
}

RenderOptionList LightBuilder::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(delta_light_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(area_light_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(environment_light_enable, options));
    return newOptions;
}

LightBuilder::RenderOptions LightBuilder::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(delta_light_enable, newOptions, options)
    RENDER_OPTION_GET(area_light_enable, newOptions, options)
    RENDER_OPTION_GET(environment_light_enable, newOptions, options)
    return newOptions;
}

bool LightBuilder::init(CapsaicinInternal const &capsaicin) noexcept
{
    gatherAreaLightsProgram =
        gfxCreateProgram(gfx_, "components/light_builder/gather_area_lights", capsaicin.getShaderPath());
    countAreaLightsKernel   = gfxCreateGraphicsKernel(gfx_, gatherAreaLightsProgram, "CountAreaLights");
    scatterAreaLightsKernel = gfxCreateGraphicsKernel(gfx_, gatherAreaLightsProgram, "ScatterAreaLights");

    lightCountBuffer = gfxCreateBuffer<uint32_t>(gfx_, 1);
    lightCountBuffer.setName("LightCountBuffer");

    const uint32_t backBufferCount = gfxGetBackBufferCount(gfx_);
    lightCountBufferTemp.reserve(backBufferCount);
    for (uint32_t i = 0; i < backBufferCount; ++i)
    {
        GfxBuffer   buffer = gfxCreateBuffer<uint32_t>(gfx_, 1, nullptr, kGfxCpuAccess_Read);
        std::string name   = "AreaLightCountCopyBuffer";
        name += std::to_string(i);
        buffer.setName(name.c_str());
        lightCountBufferTemp.emplace_back(false, buffer);
    }
    lightHash = 0;

    return !!gatherAreaLightsProgram;
}

void LightBuilder::run(CapsaicinInternal &capsaicin) noexcept
{
    auto optionsNew = convertOptions(capsaicin.getOptions());
    auto scene      = capsaicin.getScene();

    // Check if meshes were updated
    std::vector<uint32_t> lightInstancePrimitiveCount;
    if (capsaicin.getMeshesUpdated() || capsaicin.getFrameIndex() == 0)
    {
        areaLightTotal = 0;
        lightInstancePrimitiveCount.resize(gfxSceneGetObjectCount<GfxInstance>(scene));
        for (uint32_t i = 0; i < gfxSceneGetObjectCount<GfxInstance>(scene); ++i)
        {
            auto const &instance = gfxSceneGetObjects<GfxInstance>(scene)[i];
            if (instance.mesh && instance.material && gfxMaterialIsEmissive(*instance.material))
            {
                lightInstancePrimitiveCount[i] = areaLightTotal;
                areaLightTotal += (uint32_t)instance.mesh->indices.size() / 3;
            }
        }
    }

    // Check whether we need to update lighting structures
    size_t oldLightHash = lightHash;
    if (!capsaicin.getPaused() || capsaicin.getFrameIndex() == 0)
        lightHash = Capsaicin::HashReduce(
            gfxSceneGetObjects<GfxLight>(scene), gfxSceneGetObjectCount<GfxLight>(scene));

    // Get last valid area light count value
    const uint32_t bufferIndex = gfxGetBackBufferIndex(gfx_);
    if (lightCountBufferTemp[bufferIndex].first)
    {
        areaLightCount = *gfxBufferGetData<uint32_t>(gfx_, lightCountBufferTemp[bufferIndex].second);
        areaLightCount -= deltaLightCount + environmentMapCount;
        lightCountBufferTemp[bufferIndex].first = false;
    }

    auto     environmentMap     = capsaicin.getEnvironmentBuffer();
    uint32_t oldDeltaLightCount = deltaLightCount;
    deltaLightCount = (optionsNew.delta_light_enable) ? gfxSceneGetObjectCount<GfxLight>(scene) : 0;
    uint32_t oldAreaLightMaxCount = areaLightMaxCount;
    areaLightMaxCount             = (optionsNew.area_light_enable) ? areaLightTotal : 0;

    // Disable lights that are not found in scene
    /*optionsNew.delta_light_enable       = optionsNew.delta_light_enable && deltaLightCount > 0;
    optionsNew.area_light_enable        = optionsNew.area_light_enable && areaLightMaxCount > 0;
    optionsNew.environment_light_enable = optionsNew.environment_light_enable && !!environmentMap;*/

    lightsUpdated       = false;
    lightBufferIndex    = (1 - lightBufferIndex);
    lightSettingChanged = options.delta_light_enable != optionsNew.delta_light_enable
                       || options.area_light_enable != optionsNew.area_light_enable
                       || options.environment_light_enable != optionsNew.environment_light_enable;
    options = optionsNew;
    if (oldLightHash != lightHash
        || (capsaicin.getEnvironmentMapUpdated() && options.environment_light_enable)
        || (oldAreaLightMaxCount != areaLightMaxCount) || (oldDeltaLightCount != deltaLightCount)
        || lightSettingChanged
        || (areaLightMaxCount > 0 && (capsaicin.getMeshesUpdated() || capsaicin.getTransformsUpdated())))
    {
        lightsUpdated = true;

        // Update lights
        uint32_t lightCount;
        {
            TimedSection const timedSection(*this, "UpdateLights");

            std::vector<Light> allLightData;

            // Add the environment map to the light list
            // Note: other parts require that the environment map is always first in the list
            environmentMapCount = 0;
            if (!!environmentMap && options.environment_light_enable)
            {
                Light light = MakeEnvironmentLight(environmentMap.getWidth(), environmentMap.getHeight());
                allLightData.push_back(light);
                environmentMapCount = 1;
            }

            // Add delta lights to the list
            // Lights are added by type to improve gpu performance
            GfxLight const *lights = gfxSceneGetObjects<GfxLight>(scene);
            for (uint32_t i = 0; i < deltaLightCount; ++i)
            {
                if (lights[i].type == kGfxLightType_Point)
                {
                    // Create new point light
                    Light light = MakePointLight(
                        lights[i].color * lights[i].intensity, lights[i].position, lights[i].range);
                    allLightData.push_back(light);
                }
            }
            for (uint32_t i = 0; i < deltaLightCount; ++i)
            {
                if (lights[i].type == kGfxLightType_Spot)
                {
                    // Create new spot light
                    Light light = MakeSpotLight(lights[i].color * lights[i].intensity, lights[i].position,
                        lights[i].range, lights[i].direction, lights[i].outer_cone_angle,
                        lights[i].inner_cone_angle);
                    allLightData.push_back(light);
                }
            }
            for (uint32_t i = 0; i < deltaLightCount; ++i)
            {
                if (lights[i].type == kGfxLightType_Directional)
                {
                    // Create new directional light
                    Light light = MakeDirectionalLight(
                        lights[i].color * lights[i].intensity, lights[i].direction, lights[i].range);
                    allLightData.push_back(light);
                }
            }

            const uint32_t numLights = areaLightMaxCount + (uint32_t)allLightData.size();
            if (lightBuffers->getCount() < numLights)
                for (uint32_t i = 0; i < ARRAYSIZE(lightBuffers); ++i)
                {
                    char buffer[64];
                    GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_AllLightBuffer%u", i);

                    // Create light buffer
                    gfxDestroyBuffer(gfx_, lightBuffers[i]);

                    lightBuffers[i] = gfxCreateBuffer<Light>(gfx_, numLights);
                    lightBuffers[i].setName(buffer);
                }
            if (!allLightData.empty())
            {
                // Copy delta lights to start of buffer (after any environment maps)
                GfxBuffer const upload_buffer = gfxCreateBuffer<Light>(
                    gfx_, (uint32_t)allLightData.size(), allLightData.data(), kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(
                    gfx_, lightBuffers[lightBufferIndex], 0, upload_buffer, 0, allLightData.size() * sizeof(Light));
                gfxDestroyBuffer(gfx_, upload_buffer);
            }
            lightCount = (uint32_t)allLightData.size();
            gfxCommandClearBuffer(gfx_, lightCountBuffer, lightCount);
        }

        // Gather the area lights
        if (areaLightMaxCount > 0)
        {
            TimedSection const timedSection(*this, "GatherAreaLights");

            uint32_t const instanceCount    = gfxSceneGetObjectCount<GfxInstance>(scene);
            uint32_t       drawCommandCount = 0;

            GfxBuffer instanceIDBuffer = capsaicin.allocateConstantBuffer<uint32_t>(instanceCount);
            uint32_t *instanceIDData   = (uint32_t *)gfxBufferGetData(gfx_, instanceIDBuffer);

            GfxBuffer drawCommandBuffer =
                capsaicin.allocateConstantBuffer<D3D12_DRAW_INDEXED_ARGUMENTS>(instanceCount);
            D3D12_DRAW_INDEXED_ARGUMENTS *drawCommands =
                (D3D12_DRAW_INDEXED_ARGUMENTS *)gfxBufferGetData(gfx_, drawCommandBuffer);

            if (!lightInstancePrimitiveCount.empty())
            {
                // Create light mesh buffer
                gfxDestroyBuffer(gfx_, lightInstanceBuffer);
                lightInstanceBuffer =
                    gfxCreateBuffer<uint32_t>(gfx_, static_cast<uint32_t>(lightInstancePrimitiveCount.size()),
                        lightInstancePrimitiveCount.data());
                lightInstanceBuffer.setName("Capsaicin_LightInstanceBuffer");
            }
            if (lightInstancePrimitiveBuffer.getCount() < areaLightMaxCount)
            {
                // Create light mesh primitive buffer
                gfxDestroyBuffer(gfx_, lightInstancePrimitiveBuffer);
                lightInstancePrimitiveBuffer = gfxCreateBuffer<uint32_t>(gfx_, areaLightMaxCount);
                lightInstancePrimitiveBuffer.setName("Capsaicin_LightInstancePrimitiveBuffer");
            }

            for (uint32_t i = 0; i < instanceCount; ++i)
            {
                GfxConstRef<GfxInstance> instanceRef = gfxSceneGetObjectHandle<GfxInstance>(scene, i);

                if (!instanceRef->mesh || !instanceRef->material
                    || !gfxMaterialIsEmissive(*instanceRef->material))
                {
                    continue; // not an emissive primitive
                }

                uint32_t const  drawCommandIndex = drawCommandCount++;
                uint32_t const  instanceIndex    = (uint32_t)instanceRef;
                Instance const &instance         = capsaicin.getInstanceData()[instanceIndex];
                Mesh const     &mesh             = capsaicin.getMeshData()[instance.mesh_index];

                drawCommands[drawCommandIndex].IndexCountPerInstance = mesh.index_count;
                drawCommands[drawCommandIndex].InstanceCount         = 1;
                drawCommands[drawCommandIndex].StartIndexLocation    = mesh.index_offset_idx;
                drawCommands[drawCommandIndex].BaseVertexLocation    = mesh.vertex_offset_idx;
                drawCommands[drawCommandIndex].StartInstanceLocation = drawCommandIndex;

                instanceIDData[drawCommandIndex] = instanceIndex;
            }

            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_LightBuffer", lightBuffers[lightBufferIndex]);
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_LightBufferSize", lightCountBuffer);
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_LightInstanceBuffer", lightInstanceBuffer);
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_LightInstancePrimitiveBuffer",
                lightInstancePrimitiveBuffer);

            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_TransformBuffer", capsaicin.getTransformBuffer());

            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_InstanceIDBuffer", instanceIDBuffer);
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_TextureMaps", capsaicin.getTextures(),
                capsaicin.getTextureCount());
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_TextureSampler", capsaicin.getLinearSampler());
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_LightCount", lightCount);

            gfxCommandBindKernel(gfx_, countAreaLightsKernel);
            gfxCommandMultiDrawIndexedIndirect(gfx_, drawCommandBuffer, drawCommandCount);
            gfxCommandScanSum(
                gfx_, kGfxDataType_Uint, lightInstancePrimitiveBuffer, lightInstancePrimitiveBuffer);
            gfxCommandBindKernel(gfx_, scatterAreaLightsKernel);
            gfxCommandMultiDrawIndexedIndirect(gfx_, drawCommandBuffer, drawCommandCount);

            gfxDestroyBuffer(gfx_, instanceIDBuffer);
            gfxDestroyBuffer(gfx_, drawCommandBuffer);

            // If we actually have a change in the number of lights then we need to invalidate previous count
            // history. If all that happened is a change in transforms then we can ignore
            if (oldAreaLightMaxCount != areaLightMaxCount || capsaicin.getMeshesUpdated())
            {
                for (auto &i : lightCountBufferTemp)
                {
                    i.first = false;
                }
                areaLightCount = areaLightMaxCount;
            }

            // Begin copy of new value (will take 'bufferIndex' number of frames to become valid)
            gfxCommandCopyBuffer(gfx_, lightCountBufferTemp[bufferIndex].second, lightCountBuffer);
            lightCountBufferTemp[bufferIndex].first = true;
        }
        else
        {
            // Need to invalidate previous count history
            for (auto &i : lightCountBufferTemp)
            {
                i.first = 0;
            }
            areaLightCount = 0;
        }
    }
    else
    {
        // Lights haven't changed since last frame, so simply copy the previous light data across.
        gfxCommandCopyBuffer(gfx_, lightBuffers[lightBufferIndex], lightBuffers[1 - lightBufferIndex]);
    }
}

void LightBuilder::terminate() noexcept
{
    for (GfxBuffer &lightBuffer : lightBuffers)
    {
        gfxDestroyBuffer(gfx_, lightBuffer);
        lightBuffer = {};
    }
    gfxDestroyBuffer(gfx_, lightCountBuffer);
    lightCountBuffer = {};
    gfxDestroyBuffer(gfx_, lightInstanceBuffer);
    lightInstanceBuffer = {};
    gfxDestroyBuffer(gfx_, lightInstancePrimitiveBuffer);
    lightInstancePrimitiveBuffer = {};
    for (auto &i : lightCountBufferTemp)
    {
        gfxDestroyBuffer(gfx_, i.second);
        i.second = {};
    }
    lightCountBufferTemp.clear();

    gfxDestroyKernel(gfx_, countAreaLightsKernel);
    countAreaLightsKernel = {};
    gfxDestroyKernel(gfx_, scatterAreaLightsKernel);
    scatterAreaLightsKernel = {};
    gfxDestroyProgram(gfx_, gatherAreaLightsProgram);
    gatherAreaLightsProgram = {};
}

void LightBuilder::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    ImGui::Checkbox("Enable Delta Lights", &capsaicin.getOption<bool>("delta_light_enable"));
    ImGui::Checkbox("Enable Area Lights", &capsaicin.getOption<bool>("area_light_enable"));
    ImGui::Checkbox("Enable Environment Lights", &capsaicin.getOption<bool>("environment_light_enable"));
}

bool LightBuilder::needsRecompile([[maybe_unused]] CapsaicinInternal const &capsaicin) const noexcept
{
    return getLightSettingsUpdated();
}

std::vector<std::string> LightBuilder::getShaderDefines(
    [[maybe_unused]] CapsaicinInternal const &capsaicin) const noexcept
{
    std::vector<std::string> baseDefines;
    if (!options.delta_light_enable)
    {
        baseDefines.push_back("DISABLE_DELTA_LIGHTS");
    }
    if (!options.area_light_enable)
    {
        baseDefines.push_back("DISABLE_AREA_LIGHTS");
    }
    if (!options.environment_light_enable)
    {
        baseDefines.push_back("DISABLE_ENVIRONMENT_LIGHTS");
    }
    return baseDefines;
}

void LightBuilder::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_LightBufferSize", lightCountBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightBuffer", lightBuffers[lightBufferIndex]);
    gfxProgramSetParameter(gfx_, program, "g_PrevLightBuffer", lightBuffers[1 - lightBufferIndex]);
    gfxProgramSetParameter(gfx_, program, "g_LightInstanceBuffer", lightInstanceBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightInstancePrimitiveBuffer", lightInstancePrimitiveBuffer);
}

uint32_t LightBuilder::getAreaLightCount() const
{
    return areaLightCount;
}

uint32_t LightBuilder::getDeltaLightCount() const
{
    return deltaLightCount;
}

uint32_t LightBuilder::getLightCount() const
{
    return areaLightCount + deltaLightCount + environmentMapCount;
}

bool LightBuilder::getLightsUpdated() const
{
    return lightsUpdated;
}

bool LightBuilder::getLightSettingsUpdated() const
{
    return lightSettingChanged;
}
} // namespace Capsaicin
