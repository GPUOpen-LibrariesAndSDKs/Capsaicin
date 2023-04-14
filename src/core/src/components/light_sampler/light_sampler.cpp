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

#include "light_sampler.h"

#include "capsaicin_internal.h"
#include "hash_reduce.h"
#include "render_technique.h"

namespace Capsaicin
{
LightSampler::~LightSampler() noexcept
{
    gfxDestroyBuffer(gfx_, lightBuffer);
    gfxDestroyBuffer(gfx_, lightCountBuffer);
    for (auto &i : lightCountBufferTemp)
    {
        gfxDestroyBuffer(gfx_, i.second);
    }

    gfxDestroyKernel(gfx_, gatherAreaLightsKernel);
    gfxDestroyProgram(gfx_, gatherAreaLightsProgram);
}

RenderOptionList LightSampler::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(delta_light_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(area_light_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(environment_light_enable, options));
    return newOptions;
}

LightSampler::RenderOptions LightSampler::convertOptions(RenderSettings const &settings) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(delta_light_enable, newOptions, settings.options_)
    RENDER_OPTION_GET(area_light_enable, newOptions, settings.options_)
    RENDER_OPTION_GET(environment_light_enable, newOptions, settings.options_)
    return newOptions;
}

bool LightSampler::init(CapsaicinInternal const &capsaicin) noexcept
{
    gatherAreaLightsProgram =
        gfxCreateProgram(gfx_, "components/light_sampler/gather_area_lights", capsaicin.getShaderPath());
    gatherAreaLightsKernel = gfxCreateGraphicsKernel(gfx_, gatherAreaLightsProgram);

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
        lightCountBufferTemp.emplace_back(0, buffer);
    }

    return !!gatherAreaLightsProgram;
}

void LightSampler::run(CapsaicinInternal &capsaicin) noexcept
{
    auto const optionsNew = convertOptions(capsaicin.getRenderSettings());
    auto       scene      = capsaicin.getScene();

    // Check if meshes were updated
    uint32_t oldAreaLightTotal = areaLightTotal;
    if (capsaicin.getMeshesUpdated())
    {
        areaLightTotal           = 0;
        GfxMesh const *meshes    = gfxSceneGetObjects<GfxMesh>(scene);
        uint32_t const meshCount = gfxSceneGetObjectCount<GfxMesh>(scene);

        for (uint32_t i = 0; i < meshCount; ++i)
        {
            if (meshes[i].material && gfxMaterialIsEmissive(*meshes[i].material))
            {
                areaLightTotal += (uint32_t)meshes[i].indices.size() / 3;
            }
        }
    }

    // Check whether we need to update lighting structures
    size_t oldLightHash = lightHash;
    if (capsaicin.getFrameIndex() == 0 || capsaicin.getAnimate())
        lightHash = Capsaicin::HashReduce(
            gfxSceneGetObjects<GfxLight>(scene), gfxSceneGetObjectCount<GfxLight>(scene));

    // Get last valid area light count value
    const uint32_t bufferIndex = gfxGetBackBufferIndex(gfx_);
    if (lightCountBufferTemp[bufferIndex].first != 0)
    {
        areaLightCount = *gfxBufferGetData<uint32_t>(gfx_, lightCountBufferTemp[bufferIndex].second);
    }

    auto environmentMap         = capsaicin.getEnvironmentBuffer();
    lightsUpdated               = false;
    uint32_t oldDeltaLightCount = deltaLightCount;
    deltaLightCount = (optionsNew.delta_light_enable) ? gfxSceneGetObjectCount<GfxLight>(scene) : 0;
    uint32_t oldAreaLightMaxCount = areaLightMaxCount;
    areaLightMaxCount             = (optionsNew.area_light_enable) ? areaLightTotal : 0;
    lightSettingChanged =
        ((options.delta_light_enable != optionsNew.delta_light_enable)
            && (deltaLightCount != oldDeltaLightCount))
        || (options.area_light_enable != optionsNew.area_light_enable
            && areaLightMaxCount != oldAreaLightMaxCount)
        || (options.environment_light_enable != optionsNew.environment_light_enable && !!environmentMap);
    options.delta_light_enable       = optionsNew.delta_light_enable && deltaLightCount > 0;
    options.area_light_enable        = optionsNew.area_light_enable && areaLightMaxCount > 0;
    options.environment_light_enable = optionsNew.environment_light_enable && !!environmentMap;
    if (oldLightHash != lightHash
        || (capsaicin.getEnvironmentMapUpdated() && options.environment_light_enable)
        || (oldAreaLightMaxCount != areaLightMaxCount) || lightSettingChanged
        || (areaLightMaxCount > 0 && (capsaicin.getMeshesUpdated() || capsaicin.getTransformsUpdated())))
    {
        lightsUpdated = true;

        // Update lights
        {
            TimedSection const timedSection(*this, "UpdateLights");

            std::vector<Light> allLightData;

            // Add the environment map to the light list
            // Note: other parts require that the environment map is always first in the list
            if (!!environmentMap && options.environment_light_enable)
            {
                Light light = MakeEnvironmentLight(environmentMap.getWidth(), environmentMap.getHeight());
                allLightData.push_back(light);
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
            if (lightBuffer.getCount() < numLights)
            {
                // Create light buffer
                gfxDestroyBuffer(gfx_, lightBuffer);
                lightBuffer = gfxCreateBuffer<Light>(gfx_, numLights);
                lightBuffer.setName("Capsaicin_AllLightBuffer");
            }
            if (!allLightData.empty())
            {
                // Copy delta lights to start of buffer (after any environment maps)
                GfxBuffer const upload_buffer = gfxCreateBuffer<Light>(
                    gfx_, (uint32_t)allLightData.size(), allLightData.data(), kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(
                    gfx_, lightBuffer, 0, upload_buffer, 0, allLightData.size() * sizeof(Light));
                gfxDestroyBuffer(gfx_, upload_buffer);
            }
            uint32_t lightCount = (uint32_t)allLightData.size();
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

            for (uint32_t i = 0; i < instanceCount; ++i)
            {
                GfxConstRef<GfxInstance> instanceRef = gfxSceneGetObjectHandle<GfxInstance>(scene, i);

                if (!instanceRef->mesh || !instanceRef->mesh->material
                    || !gfxMaterialIsEmissive(*instanceRef->mesh->material))
                {
                    continue; // not an emissive primitive
                }

                uint32_t const  drawCommandIndex = drawCommandCount++;
                uint32_t const  instanceIndex    = (uint32_t)instanceRef;
                Instance const &instance         = capsaicin.getInstanceData()[instanceIndex];
                Mesh const     &mesh             = capsaicin.getMeshData()[instance.mesh_index];

                drawCommands[drawCommandIndex].IndexCountPerInstance = mesh.index_count;
                drawCommands[drawCommandIndex].InstanceCount         = 1;
                drawCommands[drawCommandIndex].StartIndexLocation    = mesh.index_offset / mesh.index_stride;
                drawCommands[drawCommandIndex].BaseVertexLocation = mesh.vertex_offset / mesh.vertex_stride;
                drawCommands[drawCommandIndex].StartInstanceLocation = drawCommandIndex;

                instanceIDData[drawCommandIndex] = instanceIndex;
            }

            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_LightBuffer", lightBuffer);
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_LightBufferSize", lightCountBuffer);

            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_MeshBuffer", capsaicin.getMeshBuffer());
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
                gfx_, gatherAreaLightsProgram, "g_TextureSampler", capsaicin.getNearestSampler());

            gfxCommandBindKernel(gfx_, gatherAreaLightsKernel);
            gfxCommandMultiDrawIndexedIndirect(gfx_, drawCommandBuffer, drawCommandCount);

            gfxDestroyBuffer(gfx_, instanceIDBuffer);
            gfxDestroyBuffer(gfx_, drawCommandBuffer);

            // If we actually have a change in the number of lights then we need to invalidate previous count
            // history If all that happened is a change in transforms then we can ignore
            if (oldAreaLightMaxCount != areaLightMaxCount || capsaicin.getMeshesUpdated())
            {
                for (auto &i : lightCountBufferTemp)
                {
                    i.first = 0;
                }
                areaLightCount = areaLightMaxCount;
            }

            // Begin copy of new value (will take 'bufferIndex' number of frames to become valid)
            gfxCommandCopyBuffer(gfx_, lightCountBufferTemp[bufferIndex].second, lightCountBuffer);
            lightCountBufferTemp[bufferIndex].first = areaLightMaxCount;
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
}

bool LightSampler::needsRecompile(CapsaicinInternal const &capsaicin) const noexcept
{
    return getLightSettingsUpdated();
}

std::vector<std::string> LightSampler::getShaderDefines(CapsaicinInternal const &capsaicin) const noexcept
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

void LightSampler::addProgramParameters(CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_LightBufferSize", lightCountBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightBuffer", lightBuffer);
}

uint32_t LightSampler::getAreaLightCount() const
{
    return areaLightCount;
}

uint32_t LightSampler::getDeltaLightCount() const
{
    return deltaLightCount;
}

uint32_t LightSampler::getLightCount() const
{
    return areaLightCount + deltaLightCount + environmentMapCount;
}

bool LightSampler::getLightsUpdated() const
{
    return lightsUpdated;
}

bool LightSampler::getLightSettingsUpdated() const
{
    return lightSettingChanged;
}
} // namespace Capsaicin
