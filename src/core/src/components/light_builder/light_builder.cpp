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
#include "light_builder_shared.h"
#include "render_technique.h"

namespace Capsaicin
{
// Local luminance function used for culling low emissive lights
static float luminance(float3 const rgb)
{
    return dot(rgb, float3(0.2126F, 0.7152F, 0.0722F));
}

LightBuilder::LightBuilder() noexcept
    : Component(Name)
{}

LightBuilder::~LightBuilder() noexcept
{
    LightBuilder::terminate();
}

RenderOptionList LightBuilder::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(delta_light_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(area_light_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(environment_light_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(environment_light_cosine_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(low_emission_area_lights_disable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(low_emission_threshold, options));
    return newOptions;
}

LightBuilder::RenderOptions LightBuilder::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(delta_light_enable, newOptions, options)
    RENDER_OPTION_GET(area_light_enable, newOptions, options)
    RENDER_OPTION_GET(environment_light_enable, newOptions, options)
    RENDER_OPTION_GET(environment_light_cosine_enable, newOptions, options)
    RENDER_OPTION_GET(low_emission_area_lights_disable, newOptions, options)
    RENDER_OPTION_GET(low_emission_threshold, newOptions, options)
    return newOptions;
}

SharedBufferList LightBuilder::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    buffers.push_back({"Meshlets", SharedBuffer::Access::Read});
    buffers.push_back({"MeshletPack", SharedBuffer::Access::Read});
    buffers.push_back({"PrevLightBuffer", SharedBuffer::Access::Write,
        (SharedBuffer::Flags::Allocate | SharedBuffer::Flags::Optional), 0, sizeof(Light)});
    return buffers;
}

bool LightBuilder::init(CapsaicinInternal const &capsaicin) noexcept
{
    gatherAreaLightsProgram = capsaicin.createProgram("components/light_builder/gather_area_lights");
    gatherAreaLightsKernel  = gfxCreateComputeKernel(gfx_, gatherAreaLightsProgram, "main");

    lightCountBuffer = gfxCreateBuffer<uint32_t>(gfx_, 1);
    lightCountBuffer.setName("LightCountBuffer");

    options = convertOptions(capsaicin.getOptions());

    // Setup initial light counts for current scene
    auto const scene = capsaicin.getScene();
    lightHash = HashReduce(gfxSceneGetObjects<GfxLight>(scene), gfxSceneGetObjectCount<GfxLight>(scene));
    deltaLightCount = (options.delta_light_enable) ? gfxSceneGetObjectCount<GfxLight>(scene) : 0;
    areaLightTotal  = 0;
    for (uint32_t i = 0; i < gfxSceneGetObjectCount<GfxInstance>(scene); ++i)
    {
        auto const &instance = gfxSceneGetObjects<GfxInstance>(scene)[i];
        if (instance.mesh && instance.material && gfxMaterialIsEmissive(*instance.material))
        {
            auto const primitives = static_cast<uint32_t>(instance.mesh->indices.size()) / 3;
            areaLightTotal += primitives;
        }
    }
    areaLightCount      = (options.area_light_enable) ? areaLightTotal : 0;
    environmentMapCount = (options.environment_light_enable && !!capsaicin.getEnvironmentBuffer()) ? 1 : 0;
    // Environment map may not always be loaded at init time so assume that one will be loaded if no other
    // lights exist
    environmentMapCount = areaLightCount == 0 && deltaLightCount == 0 ? 1 : environmentMapCount;

    return !!gatherAreaLightsProgram;
}

void LightBuilder::run(CapsaicinInternal &capsaicin) noexcept
{
    auto optionsNew = convertOptions(capsaicin.getOptions());
    auto scene      = capsaicin.getScene();

    // Check whether we need to update lighting structures
    size_t const oldLightHash = lightHash;
    if (options.delta_light_enable && !capsaicin.getPaused())
    {
        lightHash = HashReduce(gfxSceneGetObjects<GfxLight>(scene), gfxSceneGetObjectCount<GfxLight>(scene));
    }

    if (!options.area_light_enable
        && (capsaicin.getMeshesUpdated() || (areaLightTotal > 0 && capsaicin.getTransformsUpdated())))
    {
        // Need to reset area light count as it won't get counted while area lights are disabled
        areaLightTotal = std::numeric_limits<uint32_t>::max();
    }

    auto const hasPreviousLightBuffer = capsaicin.hasSharedBuffer("PrevLightBuffer");
    auto const environmentMap         = capsaicin.getEnvironmentBuffer();
    lightsUpdated                     = false;
    auto const oldDeltaLightCount     = deltaLightCount;
    auto const oldAreaLightCount      = areaLightCount;
    auto const oldEnvironmentMapCount = environmentMapCount;
    deltaLightCount     = (optionsNew.delta_light_enable) ? gfxSceneGetObjectCount<GfxLight>(scene) : 0;
    areaLightCount      = (optionsNew.area_light_enable) ? areaLightTotal : 0;
    environmentMapCount = (optionsNew.environment_light_enable && !!environmentMap) ? 1 : 0;

    auto const cullLowChanged = optionsNew.low_emission_area_lights_disable
                             && (options.low_emission_threshold != optionsNew.low_emission_threshold);
    lightIndexesChanged = (oldEnvironmentMapCount != environmentMapCount)
                       || (oldAreaLightCount != areaLightCount) || (oldDeltaLightCount != deltaLightCount)
                       || cullLowChanged || capsaicin.getFrameIndex() == 0;
    bool const areaLightUpdated =
        optionsNew.area_light_enable
        && (capsaicin.getMeshesUpdated() || capsaicin.getInstancesUpdated() || capsaicin.getFrameIndex() == 0
            || areaLightTotal == std::numeric_limits<uint32_t>::max()
            || (areaLightCount > 0 && capsaicin.getTransformsUpdated())
            || options.low_emission_area_lights_disable != optionsNew.low_emission_area_lights_disable
            || cullLowChanged);
    bool const deltaLightUpdated =
        optionsNew.delta_light_enable && (oldLightHash != lightHash || capsaicin.getFrameIndex() == 0);
    bool const envMapUpdated = optionsNew.environment_light_enable
                            && (capsaicin.getEnvironmentMapUpdated() || capsaicin.getFrameIndex() == 0);
    if (deltaLightUpdated || envMapUpdated || areaLightUpdated || lightIndexesChanged)
    {
        lightsUpdated = true;

        // Update lights
        {
            TimedSection const timedSection(*this, "UpdateLights");

            // We create a single light list for all known lights in the current scene. We use `Light` to
            // represent any type of supported light (area, point, directional etc.) by re-interpreting
            // the bits stored in each light struct based on the type of light stored. All delta lights
            // (point/spot/direction) are added to the list directly on the CPU at the beginning of the
            // list.
            std::vector<Light> allLightData;

            // Add the environment map to the light list
            // Note: other parts require that the environment map is always first in the list
            if (environmentMapCount != 0)
            {
                Light const light =
                    MakeEnvironmentLight(environmentMap.getWidth(), environmentMap.getHeight());
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
                    Light const light = MakePointLight(
                        lights[i].color * lights[i].intensity, lights[i].position, lights[i].range);
                    allLightData.push_back(light);
                }
            }
            for (uint32_t i = 0; i < deltaLightCount; ++i)
            {
                if (lights[i].type == kGfxLightType_Spot)
                {
                    // Create new spotlight
                    Light const light = MakeSpotLight(lights[i].color * lights[i].intensity,
                        lights[i].position, lights[i].range, normalize(lights[i].direction),
                        lights[i].outer_cone_angle, lights[i].inner_cone_angle);
                    allLightData.push_back(light);
                }
            }
            for (uint32_t i = 0; i < deltaLightCount; ++i)
            {
                if (lights[i].type == kGfxLightType_Directional)
                {
                    // Create new directional light
                    Light const light = MakeDirectionalLight(lights[i].color * lights[i].intensity,
                        normalize(lights[i].direction), lights[i].range);
                    allLightData.push_back(light);
                }
            }

            // Check if meshes were updated and add any area lights to the list
            if (areaLightCount > 0)
            {
                // Create a mapping table that maps each emissive instance into the final light buffer
                // list. Surfaces are mapped by their instanceID and primitiveID (zero index incrementing
                // value per triangle in mesh). Since not all instances have emissive meshes and that each
                // emissive mesh has different primitive counts we need to map (instanceID|primitiveID)
                // pairs to an ID into the light buffer. The `lightInstanceBuffer` contains a lookup by
                // instanceID and returns the start offset into the light buffer for that instance. Those
                // values can then be offset by the primitiveID to get the exact light location. As many
                // instances are going to contain zero valid emissive meshes the buffer is sparsely
                // populated.
                std::vector<uint32_t> lightInstancePrimitiveOffset;
                areaLightTotal              = 0;
                areaLightCount              = 0;
                auto const areaLightStartID = static_cast<uint32_t>(allLightData.size());
                lightInstancePrimitiveOffset.resize(gfxSceneGetObjectCount<GfxInstance>(scene));
                for (uint32_t i = 0; i < gfxSceneGetObjectCount<GfxInstance>(scene); ++i)
                {
                    auto const &instance = gfxSceneGetObjects<GfxInstance>(scene)[i];
                    if (instance.mesh && instance.material && gfxMaterialIsEmissive(*instance.material))
                    {
                        auto primitives = static_cast<uint32_t>(instance.mesh->indices.size()) / 3;

                        areaLightTotal += primitives;
                        if (optionsNew.low_emission_area_lights_disable)
                        {
                            // Check base luminance of emissive material
                            if (luminance(instance.material->emissivity) < optionsNew.low_emission_threshold)
                            {
                                continue;
                            }
                        }
                        lightInstancePrimitiveOffset[i] = areaLightCount + areaLightStartID;
                        areaLightCount += primitives;
                    }
                }

                if (!lightInstancePrimitiveOffset.empty())
                {
                    // Create light mesh buffer
                    gfxDestroyBuffer(gfx_, lightInstanceBuffer);
                    lightInstanceBuffer = gfxCreateBuffer<uint32_t>(gfx_,
                        static_cast<uint32_t>(lightInstancePrimitiveOffset.size()),
                        lightInstancePrimitiveOffset.data());
                    lightInstanceBuffer.setName("LightInstanceBuffer");
                }
            }

            uint32_t const lightCount = areaLightCount + static_cast<uint32_t>(allLightData.size());
            uint32_t const numLights =
                glm::max(lightCount, 1U); // Always allocate buffers even when no lights
            if (lightBuffer.getCount() < numLights)
            {
                gfxDestroyBuffer(gfx_, lightBuffer);
                lightBuffer = gfxCreateBuffer<Light>(gfx_, numLights);
                lightBuffer.setName("AllLightBuffer");
                if (hasPreviousLightBuffer)
                {
                    capsaicin.checkSharedBuffer("PrevLightBuffer", numLights * sizeof(Light), true);
                }
            }
            else if (hasPreviousLightBuffer && !lightIndexesChanged)
            {
                // Swap current buffer with previous buffer only if it makes sense to. In the case of light
                // IDs being invalidated the old buffer contains useless info anyway.
                // Swapping is faster so just don't look at the constant cast
                std::swap(lightBuffer, const_cast<GfxBuffer &>(capsaicin.getSharedBuffer("PrevLightBuffer")));
            }
            if (!allLightData.empty())
            {
                // Copy delta lights to start of buffer (after any environment maps)
                GfxBuffer const upload_buffer = gfxCreateBuffer<Light>(gfx_,
                    static_cast<uint32_t>(allLightData.size()), allLightData.data(), kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(
                    gfx_, lightBuffer, 0, upload_buffer, 0, allLightData.size() * sizeof(Light));
                gfxDestroyBuffer(gfx_, upload_buffer);
            }
            gfxCommandClearBuffer(gfx_, lightCountBuffer, lightCount);
        }

        // Gather the area lights
        if (areaLightCount > 0)
        {
            // The initial delta lights were added top the light list using the CPU but for area lights we
            // use a GPU shader to write all lights in parallel.
            TimedSection const timedSection(*this, "GatherAreaLights");

            // Create a list of valid instance|meshlet pairs that contain emissive meshlets.
            std::vector<DrawData> drawData;
            uint32_t const        instanceCount = gfxSceneGetObjectCount<GfxInstance>(capsaicin.getScene());
            for (uint32_t i = 0; i < instanceCount; ++i)
            {
                if (GfxConstRef const instanceRef = gfxSceneGetObjectHandle<GfxInstance>(scene, i);
                    instanceRef->mesh && instanceRef->material
                    && gfxMaterialIsEmissive(*instanceRef->material))
                {
                    if (optionsNew.low_emission_area_lights_disable)
                    {
                        if (luminance(instanceRef->material->emissivity) < optionsNew.low_emission_threshold)
                        {
                            continue;
                        }
                    }
                    uint32_t const  instanceIndex = capsaicin.getInstanceIdData()[i];
                    Instance const &instance      = capsaicin.getInstanceData()[instanceIndex];
                    for (uint32_t j = 0; j < instance.meshlet_count; ++j)
                    {
                        drawData.emplace_back(instance.meshlet_offset_idx + j, instanceIndex);
                    }
                }
            }
            auto            drawCount      = static_cast<uint32_t>(drawData.size());
            GfxBuffer const drawDataBuffer = gfxCreateBuffer<DrawData>(gfx_, drawCount, drawData.data());

            // The shader is actually a compute kernel, but it functions identically to a mesh shader. We run
            // a mesh shader group for each entry in the draw call list. Each shader group is then responsible
            // for collecting and writing primitives into the light list. A downside of this approach is that
            // the number of primitives per meshlet may not fully fill our group size which can lead to unused
            // threads. Attempting to merge meshlets to improve occupancy is outside the scope of what's
            // required here, and we leave it as an optimisation for the asset writer/processor.
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_DrawDataBuffer", drawDataBuffer);
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_DrawCount", drawCount);
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_LightBuffer", lightBuffer);
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_LightInstanceBuffer", lightInstanceBuffer);

            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_MaterialBuffer", capsaicin.getMaterialBuffer());
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_VertexBuffer", capsaicin.getVertexBuffer());
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_VertexDataIndex", capsaicin.getVertexDataIndex());
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_MeshletBuffer", capsaicin.getSharedBuffer("Meshlets"));
            gfxProgramSetParameter(gfx_, gatherAreaLightsProgram, "g_MeshletPackBuffer",
                capsaicin.getSharedBuffer("MeshletPack"));
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
            gfxProgramSetParameter(
                gfx_, gatherAreaLightsProgram, "g_TransformBuffer", capsaicin.getTransformBuffer());

            // Draw meshlets
            gfxCommandBindKernel(gfx_, gatherAreaLightsKernel);
            gfxCommandDispatch(gfx_, drawCount, 1, 1);

            gfxDestroyBuffer(gfx_, drawDataBuffer);
        }

        if (hasPreviousLightBuffer && lightIndexesChanged)
        {
            // The previous light buffer is unusable, to avoid errors we reset it to match the newly created
            // one
            gfxCommandCopyBuffer(gfx_, capsaicin.getSharedBuffer("PrevLightBuffer"), lightBuffer);
        }
    }
    else if (hasPreviousLightBuffer && lightsUpdatedBack)
    {
        // Lights haven't changed since last frame, so simply copy the previous light data across.
        gfxCommandCopyBuffer(gfx_, capsaicin.getSharedBuffer("PrevLightBuffer"), lightBuffer);
    }
    lightsUpdatedBack = lightsUpdated;
    // Check change in settings last so that areaLightCount has a chance to be correctly calculated
    lightSettingsChanged =
        oldEnvironmentMapCount != environmentMapCount || (oldAreaLightCount > 0) != (areaLightCount > 0)
        || (oldDeltaLightCount > 0) != (deltaLightCount > 0)
        || options.environment_light_cosine_enable != optionsNew.environment_light_cosine_enable;
    options = optionsNew;
}

void LightBuilder::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, lightBuffer);
    lightBuffer = {};
    gfxDestroyBuffer(gfx_, lightCountBuffer);
    lightCountBuffer = {};
    gfxDestroyBuffer(gfx_, lightInstanceBuffer);
    lightInstanceBuffer = {};

    gfxDestroyKernel(gfx_, gatherAreaLightsKernel);
    gatherAreaLightsKernel = {};
    gfxDestroyProgram(gfx_, gatherAreaLightsProgram);
    gatherAreaLightsProgram = {};
}

void LightBuilder::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    if (areaLightTotal == 0)
    {
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox("Enable Area Lights", &capsaicin.getOption<bool>("area_light_enable"));
    ImGui::Checkbox(
        "Cull Low Emission Area Lights", &capsaicin.getOption<bool>("low_emission_area_lights_disable"));
    if (capsaicin.getOption<bool>("low_emission_area_lights_disable"))
    {
        ImGui::SliderFloat(
            "Low Emission Threshold", &capsaicin.getOption<float>("low_emission_threshold"), 0.0F, 100.0F);
    }
    if (areaLightTotal == 0)
    {
        ImGui::EndDisabled();
    }
    auto const deltaLightTotal = gfxSceneGetObjectCount<GfxLight>(capsaicin.getScene());
    if (deltaLightTotal == 0)
    {
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox("Enable Delta Lights", &capsaicin.getOption<bool>("delta_light_enable"));
    if (deltaLightTotal == 0)
    {
        ImGui::EndDisabled();
    }
    bool const environmentMapTotal = !!capsaicin.getEnvironmentBuffer();
    if (!environmentMapTotal)
    {
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox("Enable Environment Lights", &capsaicin.getOption<bool>("environment_light_enable"));
    if (environmentMapTotal)
    {
        ImGui::Checkbox("Enable Cosine Sampling Environment Lights",
            &capsaicin.getOption<bool>("environment_light_cosine_enable"));
    }
    if (!environmentMapTotal)
    {
        ImGui::EndDisabled();
    }
}

bool LightBuilder::needsRecompile([[maybe_unused]] CapsaicinInternal const &capsaicin) const noexcept
{
    return getLightSettingsUpdated();
}

std::vector<std::string> LightBuilder::getShaderDefines(
    [[maybe_unused]] CapsaicinInternal const &capsaicin) const noexcept
{
    std::vector<std::string> baseDefines;
    if (deltaLightCount == 0)
    {
        baseDefines.emplace_back("DISABLE_DELTA_LIGHTS");
    }
    if (areaLightCount == 0)
    {
        baseDefines.emplace_back("DISABLE_AREA_LIGHTS");
    }
    if (environmentMapCount == 0)
    {
        baseDefines.emplace_back("DISABLE_ENVIRONMENT_LIGHTS");
    }
    if (options.environment_light_cosine_enable)
    {
        baseDefines.emplace_back("ENABLE_COSINE_ENVIRONMENT_SAMPLING");
    }
    if (capsaicin.hasSharedBuffer("PrevLightBuffer"))
    {
        baseDefines.emplace_back("ENABLE_PREVIOUS_LIGHTS");
    }
    return baseDefines;
}

void LightBuilder::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin, GfxProgram const &program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_LightBufferSize", lightCountBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightBuffer", lightBuffer);
    if (capsaicin.hasSharedBuffer("PrevLightBuffer"))
    {
        gfxProgramSetParameter(
            gfx_, program, "g_PrevLightBuffer", capsaicin.getSharedBuffer("PrevLightBuffer"));
    }
    gfxProgramSetParameter(gfx_, program, "g_LightInstanceBuffer", lightInstanceBuffer);
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
    return lightSettingsChanged;
}

bool LightBuilder::getLightIndexesChanged() const
{
    return lightIndexesChanged;
}
} // namespace Capsaicin
