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
#include "capsaicin_internal.h"

#include "common_functions.inl"
#include "components/light_builder/light_builder.h"
#include "render_technique.h"

#include <chrono>
#include <filesystem>
#include <gfx_imgui.h>
#include <imgui_stdlib.h>
#include <ppl.h>

namespace Capsaicin
{
CapsaicinInternal::~CapsaicinInternal()
{
    terminate();
}

GfxContext CapsaicinInternal::getGfx() const
{
    return gfx_;
}

GfxScene CapsaicinInternal::getScene() const
{
    return scene_;
}

std::vector<std::string> CapsaicinInternal::getShaderPaths() const
{
    return {shader_path_, third_party_shader_path_, third_party_shader_path_ + "FidelityFX/gpu/"};
}

uint2 CapsaicinInternal::getWindowDimensions() const noexcept
{
    return window_dimensions_;
}

uint2 CapsaicinInternal::getRenderDimensions() const noexcept
{
    return render_dimensions_;
}

float CapsaicinInternal::getRenderDimensionsScale() const noexcept
{
    return render_scale_;
}

void CapsaicinInternal::setRenderDimensionsScale(float const scale) noexcept
{
    render_scale_                  = scale;
    auto const newRenderDimensions = max(uint2(round(float2(window_dimensions_) * scale)), uint2(1));
    render_dimensions_updated_     = newRenderDimensions != render_dimensions_;
    render_dimensions_             = newRenderDimensions;
}

uint32_t CapsaicinInternal::getFrameIndex() const noexcept
{
    return frame_index_;
}

double CapsaicinInternal::getFrameTime() const noexcept
{
    return frame_time_;
}

double CapsaicinInternal::getAverageFrameTime() const noexcept
{
    return frameGraph.getAverageValue();
}

bool CapsaicinInternal::hasAnimation() const noexcept
{
    return gfxSceneGetAnimationCount(scene_) > 0;
}

void CapsaicinInternal::setPaused(bool const paused) noexcept
{
    play_paused_ = paused;
}

bool CapsaicinInternal::getPaused() const noexcept
{
    return play_paused_;
}

void CapsaicinInternal::setFixedFrameRate(bool const playMode) noexcept
{
    play_fixed_framerate_ = playMode;
}

void CapsaicinInternal::setFixedFrameTime(double const fixed_frame_time) noexcept
{
    play_fixed_frame_time_ = fixed_frame_time;
}

bool CapsaicinInternal::getFixedFrameRate() const noexcept
{
    return play_fixed_framerate_;
}

void CapsaicinInternal::restartPlayback() noexcept
{
    play_time_ = 0.0;
    // Also reset frame index so that rendering resumes from start as well
    frame_index_ = std::numeric_limits<uint32_t>::max();
}

void CapsaicinInternal::increasePlaybackSpeed() noexcept
{
    play_speed_ *= 2.0;
}

void CapsaicinInternal::decreasePlaybackSpeed() noexcept
{
    play_speed_ *= 0.5;
}

double CapsaicinInternal::getPlaybackSpeed() const noexcept
{
    return play_speed_;
}

void CapsaicinInternal::resetPlaybackSpeed() noexcept
{
    play_speed_ = 1.0;
}

void CapsaicinInternal::stepPlaybackForward(uint32_t const frames) noexcept
{
    play_time_ += static_cast<double>(frames) * play_fixed_frame_time_;
}

void CapsaicinInternal::stepPlaybackBackward(uint32_t const frames) noexcept
{
    play_time_ -= static_cast<double>(frames) * play_fixed_frame_time_;
}

void CapsaicinInternal::setPlayRewind(bool const rewind) noexcept
{
    play_rewind_ = rewind;
}

bool CapsaicinInternal::getPlayRewind() const noexcept
{
    return play_rewind_;
}

void CapsaicinInternal::setRenderPaused(bool const paused) noexcept
{
    render_paused_ = paused;
}

bool CapsaicinInternal::getRenderPaused() const noexcept
{
    return render_paused_;
}

bool CapsaicinInternal::getRenderDimensionsUpdated() const noexcept
{
    return render_dimensions_updated_;
}

bool CapsaicinInternal::getWindowDimensionsUpdated() const noexcept
{
    return window_dimensions_updated_;
}

bool CapsaicinInternal::getMeshesUpdated() const noexcept
{
    return mesh_updated_;
}

bool CapsaicinInternal::getTransformsUpdated() const noexcept
{
    return transform_updated_;
}

bool CapsaicinInternal::getInstancesUpdated() const noexcept
{
    return instances_updated_;
}

bool CapsaicinInternal::getSceneUpdated() const noexcept
{
    return scene_updated_;
}

bool CapsaicinInternal::getCameraChanged() const noexcept
{
    return camera_changed_;
}

bool CapsaicinInternal::getCameraUpdated() const noexcept
{
    return camera_updated_;
}

bool CapsaicinInternal::getAnimationUpdated() const noexcept
{
    return animation_updated_;
}

bool CapsaicinInternal::getEnvironmentMapUpdated() const noexcept
{
    return environment_map_updated_;
}

std::vector<std::string_view> CapsaicinInternal::getSharedTextures() const noexcept
{
    std::vector<std::string_view> textures;
    for (auto const &i : shared_textures_)
    {
        textures.emplace_back(i.first);
    }
    return textures;
}

bool CapsaicinInternal::hasSharedTexture(std::string_view const &texture) const noexcept
{
    return std::ranges::any_of(
        shared_textures_, [&texture](auto const &item) { return item.first == texture; });
}

bool CapsaicinInternal::checkSharedTexture(
    std::string_view const &texture, uint2 const dimensions, uint32_t const mips)
{
    if (auto const i = std::ranges::find_if(
            shared_textures_, [&texture](auto const &item) { return item.first == texture; });
        i != shared_textures_.end())
    {
        uint2      checkDim = dimensions;
        bool const autoSize = any(equal(dimensions, uint2(0)));
        if (autoSize)
        {
            checkDim = render_dimensions_;
        }
        if (i->second.getWidth() != checkDim.x || i->second.getHeight() != checkDim.y)
        {
            auto const        format = i->second.getFormat();
            auto const *const name   = i->second.getName();
            GfxTexture        newTexture;
            if (autoSize)
            {
                newTexture =
                    gfxCreateTexture2D(gfx_, render_dimensions_.x, render_dimensions_.y, format, mips);
            }
            else
            {
                newTexture = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips);
            }
            newTexture.setName(name);
            gfxDestroyTexture(gfx_, i->second);
            i->second = newTexture;
            return !!i->second;
        }
        return true;
    }
    return false;
}

GfxTexture const &CapsaicinInternal::getSharedTexture(std::string_view const &texture) const noexcept
{
    if (auto const i = std::ranges::find_if(
            shared_textures_, [&texture](auto const &item) { return item.first == texture; });
        i != shared_textures_.cend())
    {
        return i->second;
    }
    GFX_PRINTLN("Error: Unknown VAO requested: %s", texture.data());
    static GfxTexture const invalidReturn;
    return invalidReturn;
}

std::vector<std::string_view> CapsaicinInternal::getDebugViews() const noexcept
{
    std::vector<std::string_view> views;
    for (auto const &i : debug_views_)
    {
        views.emplace_back(i.first);
    }
    return views;
}

bool CapsaicinInternal::checkDebugViewSharedTexture(std::string_view const &view) const noexcept
{
    if (auto const i =
            std::ranges::find_if(debug_views_, [&view](auto const &item) { return item.first == view; });
        i != debug_views_.cend())
    {
        return !i->second;
    }
    GFX_PRINTLN("Error: Unknown debug view requested: %s", view.data());
    return false;
}

bool CapsaicinInternal::hasSharedBuffer(std::string_view const &buffer) const noexcept
{
    return std::ranges::any_of(shared_buffers_, [&buffer](auto const &item) { return item.first == buffer; });
}

bool CapsaicinInternal::checkSharedBuffer(
    std::string_view const &buffer, uint64_t const size, bool const exactSize, bool const copy)
{
    if (auto const i = std::ranges::find_if(
            shared_buffers_, [&buffer](auto const &item) { return item.first == buffer; });
        i != shared_buffers_.end())
    {
        if (exactSize ? i->second.getSize() == size : i->second.getSize() >= size)
        {
            return true;
        }
        auto const *const name      = i->second.getName();
        auto const        stride    = i->second.getStride();
        GfxBuffer         newBuffer = gfxCreateBuffer(gfx_, size);
        if (copy)
        {
            gfxCommandCopyBuffer(gfx_, newBuffer, 0, i->second, 0, i->second.getSize());
        }
        newBuffer.setName(name);
        newBuffer.setStride(stride);
        gfxDestroyBuffer(gfx_, i->second);
        i->second = newBuffer;
        return !!i->second;
    }
    return false;
}

GfxBuffer const &CapsaicinInternal::getSharedBuffer(std::string_view const &buffer) const noexcept
{
    if (auto const i = std::ranges::find_if(
            shared_buffers_, [&buffer](auto const &item) { return item.first == buffer; });
        i != shared_buffers_.cend())
    {
        return i->second;
    }
    GFX_PRINTLN("Error: Unknown buffer requested: %s", buffer.data());
    static GfxBuffer const invalidReturn;
    return invalidReturn;
}

bool CapsaicinInternal::hasComponent(std::string_view const &component) const noexcept
{
    return std::ranges::any_of(
        components_, [&component](auto const &item) { return item.first == component; });
}

std::shared_ptr<Component> const &CapsaicinInternal::getComponent(
    std::string_view const &component) const noexcept
{
    if (auto const i = std::ranges::find_if(
            components_, [&component](auto const &item) { return item.first == component; });
        i != components_.end())
    {
        return i->second;
    }
    GFX_PRINTLN("Error: Unknown buffer requested: %s", component.data());
    static std::shared_ptr<Component> const nullReturn;
    return nullReturn;
}

std::vector<std::string_view> CapsaicinInternal::GetRenderers() noexcept
{
    return RendererFactory::getNames();
}

std::string_view CapsaicinInternal::getCurrentRenderer() const noexcept
{
    return renderer_name_;
}

bool CapsaicinInternal::setRenderer(std::string_view const &name) noexcept
{
    auto const renderers = RendererFactory::getNames();
    auto const renderer  = std::ranges::find_if(renderers, [&name](auto val) { return name == val; });
    if (renderer == renderers.cend())
    {
        GFX_PRINTLN("Error: Requested invalid renderer: %s", name.data());
        return false;
    }
    if (renderer_ != nullptr)
    {
        renderer_      = nullptr;
        renderer_name_ = "";
    }
    frameGraph.reset();
    setupRenderTechniques(*renderer);
    return true;
}

std::string_view CapsaicinInternal::getCurrentDebugView() const noexcept
{
    return debug_view_;
}

bool CapsaicinInternal::setDebugView(std::string_view const &name) noexcept
{
    auto const debugView =
        std::ranges::find_if(std::as_const(debug_views_), [&name](auto val) { return name == val.first; });
    if (debugView == debug_views_.cend())
    {
        GFX_PRINTLN("Error: Requested invalid debug view: %s", name.data());
        return false;
    }
    debug_view_ = debugView->first;
    return true;
}

RenderOptionList const &CapsaicinInternal::getOptions() const noexcept
{
    return options_;
}

RenderOptionList &CapsaicinInternal::getOptions() noexcept
{
    return options_;
}

CameraMatrices const &CapsaicinInternal::getCameraMatrices(bool const jittered) const
{
    return camera_matrices_[jittered];
}

GfxBuffer CapsaicinInternal::getCameraMatricesBuffer(bool const jittered) const
{
    return camera_matrices_buffer_[jittered];
}

float2 CapsaicinInternal::getCameraJitter() const noexcept
{
    return camera_jitter_;
}

uint32_t CapsaicinInternal::getCameraJitterPhase() const noexcept
{
    return jitter_phase_count_;
}

void CapsaicinInternal::stepJitterFrameIndex(uint32_t const frames) noexcept
{
    if (uint32_t const remaining_frames = std::numeric_limits<uint32_t>::max() - jitter_frame_index_;
        frames < remaining_frames)
    {
        jitter_frame_index_ += frames;
    }
    else
    {
        jitter_frame_index_ = frames - remaining_frames;
    }
}

void CapsaicinInternal::setCameraJitterPhase(uint32_t const length) noexcept
{
    jitter_phase_count_ = length;
}

uint32_t CapsaicinInternal::getDeltaLightCount() const noexcept
{
    if (hasComponent("LightBuilder"))
    {
        return getComponent<LightBuilder>()->getDeltaLightCount();
    }
    return 0;
}

uint32_t CapsaicinInternal::getAreaLightCount() const noexcept
{
    if (hasComponent("LightBuilder"))
    {
        return getComponent<LightBuilder>()->getAreaLightCount();
    }
    return 0;
}

uint32_t CapsaicinInternal::getEnvironmentLightCount() const noexcept
{
    return !!environment_buffer_ ? 1 : 0;
}

uint32_t CapsaicinInternal::getTriangleCount() const noexcept
{
    return triangle_count_;
}

uint64_t CapsaicinInternal::getBvhDataSize() const noexcept
{
    uint64_t     bvh_data_size      = gfxAccelerationStructureGetDataSize(gfx_, acceleration_structure_);
    size_t const rt_primitive_count = raytracing_primitives_.size();

    for (size_t i = 0; i < rt_primitive_count; ++i)
    {
        auto const &rt_primitive = raytracing_primitives_[i];
        bvh_data_size += gfxRaytracingPrimitiveGetDataSize(gfx_, rt_primitive);
    }

    return bvh_data_size;
}

GfxBuffer CapsaicinInternal::getInstanceBuffer() const
{
    return instance_buffer_;
}

std::vector<Instance> const &CapsaicinInternal::getInstanceData() const
{
    return instance_data_;
}

GfxBuffer CapsaicinInternal::getInstanceIdBuffer() const
{
    return instance_id_buffer_;
}

std::vector<uint32_t> const &CapsaicinInternal::getInstanceIdData() const
{
    return instance_id_data_;
}

GfxBuffer CapsaicinInternal::getTransformBuffer() const
{
    return transform_buffer_;
}

GfxBuffer CapsaicinInternal::getPrevTransformBuffer() const
{
    return prev_transform_buffer_;
}

GfxBuffer CapsaicinInternal::getMaterialBuffer() const
{
    return material_buffer_;
}

std::vector<GfxTexture> const &CapsaicinInternal::getTextures() const
{
    return texture_atlas_;
}

GfxSamplerState CapsaicinInternal::getLinearSampler() const
{
    return linear_sampler_;
}

GfxSamplerState CapsaicinInternal::getLinearWrapSampler() const
{
    return linear_wrap_sampler_;
}

GfxSamplerState CapsaicinInternal::getNearestSampler() const
{
    return nearest_sampler_;
}

GfxSamplerState CapsaicinInternal::getAnisotropicSampler() const
{
    return anisotropic_sampler_;
}

GfxBuffer CapsaicinInternal::getIndexBuffer() const
{
    return index_buffer_;
}

GfxBuffer CapsaicinInternal::getVertexBuffer() const
{
    return vertex_buffer_;
}

GfxBuffer CapsaicinInternal::getVertexSourceBuffer() const
{
    return vertex_source_buffer_;
}

GfxBuffer CapsaicinInternal::getJointBuffer() const
{
    return joint_buffer_;
}

GfxBuffer CapsaicinInternal::getJointMatricesBuffer() const
{
    return joint_matrices_buffer_;
}

GfxBuffer CapsaicinInternal::getMorphWeightBuffer() const
{
    return morph_weight_buffer_;
}

uint32_t CapsaicinInternal::getVertexDataIndex() const
{
    return vertex_data_index_;
}

uint32_t CapsaicinInternal::getPrevVertexDataIndex() const
{
    return prev_vertex_data_index_;
}

uint32_t CapsaicinInternal::getRaytracingPrimitiveCount() const
{
    return static_cast<uint32_t>(raytracing_primitives_.size());
}

GfxAccelerationStructure CapsaicinInternal::getAccelerationStructure() const
{
    return acceleration_structure_;
}

uint32_t CapsaicinInternal::getSbtStrideInEntries(GfxShaderGroupType const type) const
{
    return sbt_stride_in_entries_[type];
}

std::pair<float3, float3> CapsaicinInternal::getSceneBounds() const
{
    // Calculate the scene bounds
    uint32_t const numInstance = gfxSceneGetObjectCount<GfxInstance>(scene_);
    float3         sceneMin(std::numeric_limits<float>::max());
    float3         sceneMax(std::numeric_limits<float>::lowest());
    for (uint i = 0; i < numInstance; ++i)
    {
        uint32_t const instanceIndex           = instance_id_data_[i];
        auto const &[instanceMin, instanceMax] = instance_bounds_[instanceIndex];
        float3 const minBounds                 = min(instanceMin, instanceMax);
        float3 const maxBounds                 = max(instanceMin, instanceMax);
        sceneMin                               = min(sceneMin, minBounds);
        sceneMax                               = max(sceneMax, maxBounds);
    }
    return std::make_pair(sceneMin, sceneMax);
}

GfxBuffer CapsaicinInternal::allocateConstantBuffer(uint64_t const size)
{
    GfxBuffer     &constant_buffer_pool        = constant_buffer_pools_[gfxGetBackBufferIndex(gfx_)];
    uint64_t const constant_buffer_pool_cursor = GFX_ALIGN(constant_buffer_pool_cursor_ + size, 256);

    if (constant_buffer_pool_cursor >= constant_buffer_pool.getSize())
    {
        gfxDestroyBuffer(gfx_, constant_buffer_pool);

        uint64_t constant_buffer_pool_size = constant_buffer_pool_cursor;
        constant_buffer_pool_size += (constant_buffer_pool_size + 2) >> 1;
        constant_buffer_pool_size = GFX_ALIGN(constant_buffer_pool_size, 65536);

        constant_buffer_pool = gfxCreateBuffer(gfx_, constant_buffer_pool_size, nullptr, kGfxCpuAccess_Write);

        char buffer[256];
        GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ConstantBufferPool%u", gfxGetBackBufferIndex(gfx_));

        constant_buffer_pool.setName(buffer);
    }

    GfxBuffer const constant_buffer =
        gfxCreateBufferRange(gfx_, constant_buffer_pool, constant_buffer_pool_cursor_, size);

    constant_buffer_pool_cursor_ = constant_buffer_pool_cursor;

    return constant_buffer;
}

GfxTexture CapsaicinInternal::createRenderTexture(
    const DXGI_FORMAT format, std::string_view const &name, uint32_t mips, float const scale) const noexcept
{
    auto const dimensions = scale == 1.0F ? render_dimensions_ : uint2(float2(render_dimensions_) * scale);
    mips = mips == UINT_MAX ? std::max(gfxCalculateMipCount(dimensions.x, dimensions.y), 1U) : mips;
    constexpr float clear[] = {0.0F, 0.0F, 0.0F, 0.0F};
    auto            ret     = gfxCreateTexture2D(
        gfx_, dimensions.x, dimensions.y, format, mips, (format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear);
    ret.setName(name.data());
    return ret;
}

GfxTexture CapsaicinInternal::resizeRenderTexture(
    GfxTexture const &texture, bool const clear, uint32_t mips, float const scale) const noexcept
{
    auto const        format = texture.getFormat();
    auto const *const name   = texture.getName();
    auto const dimensions    = scale == 1.0F ? render_dimensions_ : uint2(float2(render_dimensions_) * scale);
    mips                     = (mips == UINT_MAX || (mips == 0 && texture.getMipLevels() > 1))
                                 ? gfxCalculateMipCount(dimensions.x, dimensions.y)
                                 : ((mips == 0) ? 1 : mips);
    auto ret = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips, texture.getClearValue());
    ret.setName(name);
    gfxDestroyTexture(gfx_, texture);
    if (clear)
    {
        gfxCommandClearTexture(gfx_, ret);
    }
    return ret;
}

GfxTexture CapsaicinInternal::createWindowTexture(
    const DXGI_FORMAT format, std::string_view const &name, uint32_t mips, float const scale) const noexcept
{
    auto const dimensions = scale == 1.0F ? window_dimensions_ : uint2(float2(window_dimensions_) * scale);
    mips = mips == UINT_MAX ? std::max(gfxCalculateMipCount(dimensions.x, dimensions.y), 1U) : mips;
    constexpr float clearValue[] = {0.0F, 0.0F, 0.0F, 0.0F};
    auto            ret          = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips,
        (format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clearValue);
    ret.setName(name.data());
    return ret;
}

GfxTexture CapsaicinInternal::resizeWindowTexture(
    GfxTexture const &texture, bool const clear, uint32_t mips, float const scale) const noexcept
{
    auto const        format = texture.getFormat();
    auto const *const name   = texture.getName();
    auto const dimensions    = scale == 1.0F ? window_dimensions_ : uint2(float2(window_dimensions_) * scale);
    mips                     = (mips == UINT_MAX || (mips == 0 && texture.getMipLevels() > 1))
                                 ? gfxCalculateMipCount(dimensions.x, dimensions.y)
                                 : ((mips == 0) ? 1 : mips);
    auto ret = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips, texture.getClearValue());
    ret.setName(name);
    gfxDestroyTexture(gfx_, texture);
    if (clear)
    {
        gfxCommandClearTexture(gfx_, ret);
    }
    return ret;
}

GfxProgram CapsaicinInternal::createProgram(char const *file_name) const noexcept
{
    auto const  shaderPaths      = getShaderPaths();
    char const *include_paths[3] = {shaderPaths[0].c_str(), shaderPaths[1].c_str(), shaderPaths[2].c_str()};
    return gfxCreateProgram(gfx_, file_name, shader_path_.c_str(), nullptr, include_paths, 3U);
}

void CapsaicinInternal::initialize(GfxContext const &gfx, ImGuiContext *imgui_context)
{
    if (!gfx)
    {
        return; // invalid graphics context
    }

    if (gfx_)
    {
        terminate();
    }

    {
        linear_sampler_      = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        linear_wrap_sampler_ = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
        nearest_sampler_     = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_POINT);
        anisotropic_sampler_ = gfxCreateSamplerState(
            gfx, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }
    shader_path_             = "src/core/src/";
    third_party_shader_path_ = "third_party/";
    // Check if shader source can be found
    std::error_code ec;
    bool            found = false;
    for (uint32_t i = 0; i < 4; ++i)
    {
        if (std::filesystem::exists(shader_path_ + "gpu_shared.h", ec))
        {
            found = true;
            break;
        }
        shader_path_.insert(0, "../");
        third_party_shader_path_.insert(0, "../");
    }
    if (!found)
    {
        GFX_PRINTLN("Could not find directory containing shader source files");
        return;
    }

    sbt_stride_in_entries_[kGfxShaderGroupType_Raygen]   = 1;
    sbt_stride_in_entries_[kGfxShaderGroupType_Miss]     = 2;
    sbt_stride_in_entries_[kGfxShaderGroupType_Hit]      = 2;
    sbt_stride_in_entries_[kGfxShaderGroupType_Callable] = 1;

    gfx_ = gfx;

    blit_program_ = createProgram("capsaicin/blit");
    blit_kernel_  = gfxCreateGraphicsKernel(gfx, blit_program_);

    generate_animated_vertices_program_ = createProgram("capsaicin/generate_animated_vertices");
    generate_animated_vertices_kernel_  = gfxCreateComputeKernel(gfx, generate_animated_vertices_program_);

    window_dimensions_ = uint2(gfxGetBackBufferWidth(gfx), gfxGetBackBufferHeight(gfx));
    setRenderDimensionsScale(render_scale_);

    ImGui::SetCurrentContext(imgui_context);
}

void CapsaicinInternal::render()
{
    // Update current frame time
    auto const previousTime = current_time_;
    auto const wallTime     = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
    current_time_ = static_cast<double>(wallTime.count()) / 1000000.0;
    frame_time_   = current_time_ - previousTime;

    // Check if manual frame increment/decrement has been applied

    if (bool const manual_play = play_time_ != play_time_old_;
        !render_paused_ || manual_play || frame_index_ == std::numeric_limits<uint32_t>::max())
    {
        // Start a new frame
        ++frame_index_;

        frameGraph.addValue(frame_time_);

        constant_buffer_pool_cursor_ = 0;
        auto const currentWindow     = uint2(gfxGetBackBufferWidth(gfx_), gfxGetBackBufferHeight(gfx_));
        window_dimensions_updated_   = window_dimensions_ != currentWindow;
        window_dimensions_           = currentWindow;
        if (window_dimensions_updated_)
        {
            // Update render dimensions
            setRenderDimensionsScale(render_scale_);
        }

        // Update the shared texture history
        if (!render_dimensions_updated_)
        {
            GfxCommandEvent const command_event(gfx_, "UpdatePreviousSharedTextures");

            for (auto const &i : backup_shared_textures_)
            {
                gfxCommandCopyTexture(
                    gfx_, shared_textures_[i.second].second, shared_textures_[i.first].second);
            }
        }

        // Clear our shared textures/buffers
        {
            GfxCommandEvent const command_event(gfx_, "ClearGBuffers");

            if (!render_dimensions_updated_)
            {
                for (auto const &i : clear_shared_buffers_)
                {
                    gfxCommandClearBuffer(gfx_, shared_buffers_[i].second);
                }

                for (auto const &i : clear_shared_textures_)
                {
                    gfxCommandClearTexture(gfx_, shared_textures_[i].second);
                }

                if (!debug_view_.empty() && debug_view_ != "None")
                {
                    gfxCommandClearTexture(gfx_, getSharedTexture("Debug"));
                }
            }
            else
            {
                for (auto &i : shared_textures_)
                {
                    if (i.first != "ColorScaled")
                    {
                        i.second = resizeRenderTexture(i.second);
                    }
                    else
                    {
                        i.second = resizeWindowTexture(i.second);
                    }
                }
            }
        }

        // Update the scene state
        updateScene();

        // Update the components
        for (auto const &component : components_)
        {
            component.second->setGfxContext(gfx_);
            component.second->resetQueries();
            {
                Component::TimedSection const timed_section(*component.second, component.second->getName());
                component.second->run(*this);
            }
        }

        // Execute our render techniques
        for (auto const &render_technique : render_techniques_)
        {
            render_technique->setGfxContext(gfx_);
            render_technique->resetQueries();
            {
                RenderTechnique::TimedSection const timed_section(
                    *render_technique, render_technique->getName());
                render_technique->render(*this);
            }
        }

        // Reset all update flags
        render_dimensions_updated_ = false;
        window_dimensions_updated_ = false;
        mesh_updated_              = false;
        transform_updated_         = false;
        environment_map_updated_   = false;
        scene_updated_             = false;
        camera_changed_            = false;
        camera_updated_            = false;
        animation_updated_         = false;
    }

    // Show debug visualizations if requested or blit Color AOV
    currentView =
        hasSharedTexture("ColorScaled") && hasOption<bool>("taa_enable") && getOption<bool>("taa_enable")
            ? getSharedTexture("ColorScaled")
            : getSharedTexture("Color");
    if (!debug_view_.empty() && debug_view_ != "None")
    {
        if (auto const debugView = std::ranges::find_if(
                std::as_const(debug_views_), [this](auto val) { return debug_view_ == val.first; });
            debugView == debug_views_.cend())
        {
            GFX_PRINTLN("Error: Invalid debug view requested: %s", debug_view_.data());
            GfxCommandEvent const command_event(gfx_, "DrawInvalidDebugView");
            gfxCommandClearBackBuffer(gfx_);
        }
        else if (!debugView->second || debug_view_ == "Depth")
        {
            // Output shared texture
            if (auto const &texture = getSharedTexture(debugView->first);
                texture.getFormat() == DXGI_FORMAT_D32_FLOAT
                || (texture.getFormat() == DXGI_FORMAT_R32_FLOAT
                    && (strstr(texture.getName(), "Depth") != nullptr
                        || strstr(texture.getName(), "depth") != nullptr)))
            {
                auto const &debug_texture = getSharedTexture("Debug");
                if (!debug_depth_kernel_)
                {
                    debug_depth_program_    = createProgram("capsaicin/debug_depth");
                    GfxDrawState const draw = {};
                    gfxDrawStateSetColorTarget(draw, 0, debug_texture.getFormat());
                    debug_depth_kernel_ = gfxCreateGraphicsKernel(gfx_, debug_depth_program_, draw);
                }
                {
                    GfxCommandEvent const command_event(gfx_, "DrawDepthDebugView");
                    gfxProgramSetParameter(gfx_, debug_depth_program_, "DepthBuffer", texture);
                    auto const  &camera = getCamera();
                    float2 const nearFar(camera.nearZ, camera.farZ);
                    gfxProgramSetParameter(gfx_, debug_depth_program_, "g_NearFar", nearFar);
                    gfxCommandBindColorTarget(gfx_, 0, debug_texture);
                    gfxCommandBindKernel(gfx_, debug_depth_kernel_);
                    gfxCommandDraw(gfx_, 3);
                }
                currentView = debug_texture;
            }
            else
            {
                // If tone-mapping is enabled then we allow it to tonemap the shared texture into the Debug
                // buffer and then output from there
                if (auto const format = texture.getFormat();
                    hasOption<bool>("tonemap_enable") && getOption<bool>("tonemap_enable")
                    && (format == DXGI_FORMAT_R32G32B32A32_FLOAT || format == DXGI_FORMAT_R32G32B32_FLOAT
                        || format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_R11G11B10_FLOAT))
                {
                    currentView = getSharedTexture("Debug");
                }
                else
                {
                    currentView = texture;
                }
            }
        }
        else
        {
            // Output debug AOV
            currentView = getSharedTexture("Debug");
        }
    }
    {
        // Blit the current view to back buffer
        GfxCommandEvent const command_event(gfx_, "Blit");
        gfxProgramSetParameter(gfx_, blit_program_, "ColorBuffer", currentView);
        gfxCommandBindKernel(gfx_, blit_kernel_);
        gfxCommandDraw(gfx_, 3);
    }

    // Dump buffers for past dump requests (takes X frames to be become available)
    uint32_t dump_available_buffer_count = 0;
    for (auto &dump_in_flight_buffer : dump_in_flight_buffers_)
    {
        if (uint32_t &dump_frame_index = std::get<5>(dump_in_flight_buffer); dump_frame_index == 0)
        {
            dump_available_buffer_count++;
        }
        else
        {
            --dump_frame_index;
        }
    }

    // Write out each available buffer in parallel
    concurrency::parallel_for(0U, dump_available_buffer_count, 1U, [&](uint32_t const buffer_index) {
        auto const &buffer = dump_in_flight_buffers_[buffer_index];
        saveImage(std::get<0>(buffer), std::get<1>(buffer), std::get<2>(buffer), std::get<3>(buffer),
            std::get<4>(buffer));
    });

    for (uint32_t available_buffer_index = 0; available_buffer_index < dump_available_buffer_count;
         available_buffer_index++)
    {
        gfxDestroyBuffer(gfx_, std::get<0>(dump_in_flight_buffers_.front()));
        dump_in_flight_buffers_.pop_front();
    }
}

void CapsaicinInternal::renderGUI(bool const readOnly)
{
    // Check if we have a functional UI context
    if (ImGui::GetCurrentContext() == nullptr)
    {
        static bool warned;
        if (!warned)
        {
            GFX_PRINT_ERROR(kGfxResult_InvalidOperation,
                "No ImGui context was supplied on initialization; cannot call `Capsaicin::RenderGUI()'");
        }
        warned = true;
        return; // no ImGui context was supplied on initialization
    }

    // Display scene specific statistics
    ImGui::Text("Selected device :  %s", gfx_.getName());
    ImGui::Separator();
    uint32_t const deltaLightCount = getDeltaLightCount();
    uint32_t const areaLightCount  = getAreaLightCount();
    uint32_t const envLightCount   = getEnvironmentLightCount();
    uint32_t const triangleCount   = getTriangleCount();
    uint64_t const bvhDataSize     = getBvhDataSize();
    ImGui::Text("Triangle Count            :  %u", triangleCount);
    ImGui::Text("Light Count               :  %u", areaLightCount + deltaLightCount + envLightCount);
    ImGui::Text("  Area Light Count        :  %u", areaLightCount);
    ImGui::Text("  Delta Light Count       :  %u", deltaLightCount);
    ImGui::Text("  Environment Light Count :  %u", envLightCount);
    ImGui::Text(
        "BVH Data Size             :  %.1f MiB", static_cast<double>(bvhDataSize) / (1024.0 * 1024.0));
    ImGui::Text("Render Resolution         :  %ux%u", render_dimensions_.x, render_dimensions_.y);
    ImGui::Text("Window Resolution         :  %ux%u", window_dimensions_.x, window_dimensions_.y);

    if (!readOnly)
    {
        // Display renderer specific options, this is where any UI elements created by a render technique or
        // component will be displayed
        if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            renderStockGUI();
            for (auto const &component : components_)
            {
                component.second->renderGUI(*this);
            }
            for (auto const &render_technique : render_techniques_)
            {
                render_technique->renderGUI(*this);
            }
        }
        ImGui::Separator();
    }

    // Display the profiling information
    if (ImGui::CollapsingHeader("Profiling", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float totalTimestampTime = 0.0F;

        auto getTimestamps = [&](Timeable *timeable) -> void {
            // Check the current input for any timeable information
            uint32_t const timestamp_query_count = timeable->getTimestampQueryCount();
            if (timestamp_query_count == 0)
            {
                return; // skip if no profiling info available
            }

            bool const               hasChildren = timestamp_query_count > 1;
            ImGuiTreeNodeFlags const flags = hasChildren ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf;
            auto const              &timestamp_queries = timeable->getTimestampQueries();
            auto const total_query_duration = gfxTimestampQueryGetDuration(gfx_, timestamp_queries[0].query);

            // Add the current query duration to the total running count for later use
            totalTimestampTime += total_query_duration;

            // Display tree of parent with any child timeable. We use a left padding of 25 chars as
            // this should fit any timeable name we currently use
            if (ImGui::TreeNodeEx(timeable->getName().data(), flags, "%-25s: %.3f ms",
                    timeable->getName().data(), static_cast<double>(total_query_duration)))
            {
                if (hasChildren)
                {
                    for (uint32_t i = 1; i < timestamp_query_count; ++i)
                    {
                        // Display child element. Children are inset 3 spaces to the left padding is
                        // reduced as a result
                        ImGui::TreeNodeEx(std::to_string(i).c_str(),
                            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%-22s: %.3f ms",
                            timestamp_queries[i].name.data(),
                            static_cast<double>(
                                gfxTimestampQueryGetDuration(gfx_, timestamp_queries[i].query)));
                    }
                }

                ImGui::TreePop();
            }
        };
        // Loop through all components and then all techniques in order and check for timeable information
        for (auto const &component : components_)
        {
            getTimestamps(&*component.second);
        }
        for (auto const &render_technique : render_techniques_)
        {
            getTimestamps(&*render_technique);
        }

        // Add final tree combined total
        if (ImGui::TreeNodeEx("Total", ImGuiTreeNodeFlags_Leaf, "%-25s: %.3f ms", "Total",
                static_cast<double>(totalTimestampTime)))
        {
            ImGui::TreePop();
        }
        ImGui::Separator();

        // Output total frame time, left padding is 3 more than was used for tree nodes due to tree
        // indentation
        ImGui::PushID("Total frame time");
        ImGui::Text("%-28s:", "Total frame time");

        // Add frame time graph
        ImGui::SameLine();
        std::string const graphName = std::format("{:.2f}", frame_time_ * 1000.0) + " ms ("
                                    + std::format("{:.2f}", 1.0 / frame_time_) + " fps)";
        ImGui::PlotLines("", Graph::GetValueAtIndex, &frameGraph,
            static_cast<int>(frameGraph.getValueCount()), 0, graphName.c_str(), 0.0F, FLT_MAX,
            ImVec2(150, 20));
        ImGui::PopID();

        // Out put current frame number
        ImGui::PushID("Frame");
        ImGui::Text("%-28s:", "Frame");
        ImGui::SameLine();
        ImGui::Text("%s", std::to_string(frame_index_).c_str());
        ImGui::PopID();
    }

    if (!readOnly)
    {
        // As not all techniques/components will expose there settings in a visible way through their own
        // renderGUI functions we expose everything through a developer specific render options tree. This can
        // be used to modify any render option even if it isn't exposed in a more user-friendly way through
        // techniques/components renderGUI. This obviously then doesn't have nice naming or checks for invalid
        // inputs so it is assumed that this is for developer debugging purposes only
        if (ImGui::CollapsingHeader("Render Options", ImGuiTreeNodeFlags_None))
        {
            for (auto const &i : options_)
            {
                if (std::holds_alternative<bool>(i.second))
                {
                    auto value = *std::get_if<bool>(&i.second);
                    if (ImGui::Checkbox(i.first.data(), &value))
                    {
                        setOption(i.first, value);
                    }
                }
                else if (std::holds_alternative<uint32_t>(i.second))
                {
                    auto value = *std::get_if<uint32_t>(&i.second);
                    if (ImGui::DragInt(i.first.data(), reinterpret_cast<int32_t *>(&value), 1, 0))
                    {
                        setOption(i.first, value);
                    }
                }
                else if (std::holds_alternative<int32_t>(i.second))
                {
                    auto value = *std::get_if<int32_t>(&i.second);
                    if (ImGui::DragInt(i.first.data(), &value, 1))
                    {
                        setOption(i.first, value);
                    }
                }
                else if (std::holds_alternative<float>(i.second))
                {
                    auto value = *std::get_if<float>(&i.second);
                    if (ImGui::DragFloat(i.first.data(), &value, 5e-3F))
                    {
                        setOption(i.first, value);
                    }
                }
                else if (std::holds_alternative<std::string>(i.second))
                {
                    // ImGui needs a constant string buffer so that it can write data into it while a user is
                    // typing We only accept data once the user hits enter at which point we update our
                    // internal value. This requires a separate static string buffer to temporarily hold
                    // string storage
                    static std::map<std::string_view, std::array<char, 2048>> staticImguiStrings;
                    if (!staticImguiStrings.contains(i.first))
                    {
                        std::array<char, 2048> buffer {};
                        auto                   value = *std::get_if<std::string>(&i.second);
                        strncpy_s(buffer.data(), buffer.size(), value.c_str(), value.size() + 1);
                        staticImguiStrings[i.first] = buffer;
                    }
                    else
                    {
                        // Check if string needs updating
                        if (auto value = *std::get_if<std::string>(&i.second);
                            std::string(staticImguiStrings[i.first].data()) != value)
                        {
                            strncpy_s(staticImguiStrings[i.first].data(), staticImguiStrings[i.first].size(),
                                value.c_str(), value.size() + 1);
                        }
                    }
                    if (ImGui::InputText(i.first.data(), staticImguiStrings[i.first].data(),
                            staticImguiStrings[i.first].size(),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                    {
                        setOption(i.first, std::string(staticImguiStrings[i.first].data()));
                    }
                }
            }
        }
    }
}

void CapsaicinInternal::terminate() noexcept
{
    gfxFinish(gfx_); // flush & sync

    // Dump remaining buffers, they are all available after gfxFinish
    concurrency::parallel_for(
        0U, static_cast<uint32_t>(dump_in_flight_buffers_.size()), 1U, [&](uint32_t const buffer_index) {
            auto const &buffer = dump_in_flight_buffers_[buffer_index];
            saveImage(std::get<0>(buffer), std::get<1>(buffer), std::get<2>(buffer), std::get<3>(buffer),
                std::get<4>(buffer).c_str());
        });

    while (!dump_in_flight_buffers_.empty())
    {
        gfxDestroyBuffer(gfx_, std::get<0>(dump_in_flight_buffers_.front()));
        dump_in_flight_buffers_.pop_front();
    }

    render_techniques_.clear();
    components_.clear();
    renderer_ = nullptr;

    gfxDestroyKernel(gfx_, blit_kernel_);
    gfxDestroyProgram(gfx_, blit_program_);
    gfxDestroyKernel(gfx_, debug_depth_kernel_);
    gfxDestroyProgram(gfx_, debug_depth_program_);
    gfxDestroyKernel(gfx_, generate_animated_vertices_kernel_);
    gfxDestroyProgram(gfx_, generate_animated_vertices_program_);

    gfxDestroyBuffer(gfx_, camera_matrices_buffer_[0]);
    gfxDestroyBuffer(gfx_, camera_matrices_buffer_[1]);
    gfxDestroyBuffer(gfx_, index_buffer_);
    gfxDestroyBuffer(gfx_, vertex_buffer_);
    gfxDestroyBuffer(gfx_, vertex_source_buffer_);
    gfxDestroyBuffer(gfx_, instance_buffer_);
    gfxDestroyBuffer(gfx_, material_buffer_);
    gfxDestroyBuffer(gfx_, transform_buffer_);
    gfxDestroyBuffer(gfx_, instance_id_buffer_);
    gfxDestroyBuffer(gfx_, prev_transform_buffer_);
    gfxDestroyBuffer(gfx_, morph_weight_buffer_);
    gfxDestroyBuffer(gfx_, joint_buffer_);
    gfxDestroyBuffer(gfx_, joint_matrices_buffer_);

    gfxDestroyTexture(gfx_, environment_buffer_);

    gfxDestroySamplerState(gfx_, linear_sampler_);
    gfxDestroySamplerState(gfx_, linear_wrap_sampler_);
    gfxDestroySamplerState(gfx_, nearest_sampler_);
    gfxDestroySamplerState(gfx_, anisotropic_sampler_);

    destroyAccelerationStructure();

    for (auto const &i : shared_textures_)
    {
        gfxDestroyTexture(gfx_, i.second);
    }
    shared_textures_.clear();
    backup_shared_textures_.clear();
    clear_shared_textures_.clear();

    debug_views_.clear();

    for (auto const &i : shared_buffers_)
    {
        gfxDestroyBuffer(gfx_, i.second);
    }
    shared_buffers_.clear();

    for (GfxTexture const &texture : texture_atlas_)
    {
        gfxDestroyTexture(gfx_, texture);
    }
    texture_atlas_.clear();

    for (GfxBuffer const &constant_buffer_pool : constant_buffer_pools_)
    {
        gfxDestroyBuffer(gfx_, constant_buffer_pool);
    }
    memset(constant_buffer_pools_, 0, sizeof(constant_buffer_pools_));

    gfxDestroyScene(scene_);
    scene_ = {};
}

void CapsaicinInternal::reloadShaders() noexcept
{
    // Instead of just recompiling kernels we re-initialise all component/techniques. This has the side
    // effect of not only recompiling kernels but also re-initialising old data that may no longer contain
    // correct values
    gfxFinish(gfx_); // flush & sync

    // Reset the component/techniques
    for (auto const &i : components_)
    {
        i.second->setGfxContext(gfx_);
        i.second->terminate();
    }
    for (auto const &i : render_techniques_)
    {
        i->setGfxContext(gfx_);
        i->terminate();
    }

    resetPlaybackState();
    resetRenderState();

    // Re-initialise the components/techniques
    for (auto const &i : components_)
    {
        if (!i.second->init(*this))
        {
            GFX_PRINTLN("Error: Failed to initialise component: %s", i.first.data());
        }
    }
    for (auto const &i : render_techniques_)
    {
        if (!i->init(*this))
        {
            GFX_PRINTLN("Error: Failed to initialise render technique: %s", i->getName().data());
        }
    }
}

RenderOptionList CapsaicinInternal::getStockRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(capsaicin_lod_mode, render_options));
    newOptions.emplace(RENDER_OPTION_MAKE(capsaicin_lod_offset, render_options));
    newOptions.emplace(RENDER_OPTION_MAKE(capsaicin_lod_aggressive, render_options));
    newOptions.emplace(RENDER_OPTION_MAKE(capsaicin_mirror_roughness_threshold, render_options));
    return newOptions;
}

CapsaicinInternal::RenderOptions CapsaicinInternal::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(capsaicin_lod_mode, newOptions, options)
    RENDER_OPTION_GET(capsaicin_lod_offset, newOptions, options)
    RENDER_OPTION_GET(capsaicin_lod_aggressive, newOptions, options)
    RENDER_OPTION_GET(capsaicin_mirror_roughness_threshold, newOptions, options)
    return newOptions;
}

ComponentList CapsaicinInternal::getStockComponents() const noexcept
{
    // Nothing to do here, available for future use
    return {};
}

SharedBufferList CapsaicinInternal::getStockSharedBuffers() const noexcept
{
    SharedBufferList ret;
    ret.push_back({"Meshlets", SharedBuffer::Access::Write,
        (SharedBuffer::Flags::Allocate | SharedBuffer::Flags::Optional), 0, sizeof(Meshlet)});
    ret.push_back({"MeshletPack", SharedBuffer::Access::Write,
        (SharedBuffer::Flags::Allocate | SharedBuffer::Flags::Optional), 0, sizeof(uint32_t)});
    ret.push_back({"MeshletCull", SharedBuffer::Access::Write,
        (SharedBuffer::Flags::Allocate | SharedBuffer::Flags::Optional), 0, sizeof(MeshletCull)});
    return ret;
}

SharedTextureList CapsaicinInternal::getStockSharedTextures() const noexcept
{
    SharedTextureList ret;
    ret.push_back({.name = "Color",
        .flags           = SharedTexture::Flags::Accumulate,
        .format          = DXGI_FORMAT_R16G16B16A16_FLOAT});
    return ret;
}

DebugViewList CapsaicinInternal::getStockDebugViews() const noexcept
{
    // We provide a custom shader for all depth based textures which we will use as default on the Depth
    // target
    return {"Depth"};
}

void CapsaicinInternal::renderStockGUI() noexcept
{
    // Nothing to do (yet)
}

void CapsaicinInternal::negotiateRenderTechniques() noexcept
{
    // Delete old shared textures and buffers
    for (auto const &i : shared_buffers_)
    {
        gfxDestroyBuffer(gfx_, i.second);
    }
    shared_buffers_.clear();
    clear_shared_buffers_.clear();
    for (auto const &i : shared_textures_)
    {
        gfxDestroyTexture(gfx_, i.second);
    }
    shared_textures_.clear();
    backup_shared_textures_.clear();
    clear_shared_textures_.clear();
    // Debug views must also be cleared as shared texture views may change after re-negotiation
    debug_views_.clear();
    debug_views_.emplace_back("None", nullptr);
    debug_view_ = "None";

    {
        // Get requested buffers
        struct BufferParams
        {
            SharedBuffer::Flags flags  = SharedBuffer::Flags::None;
            size_t              size   = 0;
            uint32_t            stride = 0;
        };

        using BufferList = std::unordered_map<std::string_view, BufferParams>;
        BufferList requestedBuffers;
        BufferList optionalBuffers;
        auto       bufferFunc = [&](SharedBuffer const &j) {
            if (auto const found = requestedBuffers.find(j.name); found == requestedBuffers.end())
            {
                // Check if the shared buffer is being read despite never having been written to
                if (j.access == SharedBuffer::Access::Read && !(j.flags & SharedBuffer::Flags::Optional)
                    && !optionalBuffers.contains(j.name) && (j.flags & SharedBuffer::Flags::Clear))
                {
                    GFX_PRINTLN(
                        "Error: Requested read access to shared buffer that has not been written to: %s",
                        j.name.data());
                }
                auto newParams = BufferParams {j.flags, j.size, j.stride};
                bool addBuffer = false;
                if (j.flags & SharedBuffer::Flags::Optional)
                {
                    if (j.access != SharedBuffer::Access::Read)
                    {
                        if (optionalBuffers.contains(j.name))
                        {
                            GFX_PRINTLN(
                                "Error: Found multiple writes to same optional buffer: %s", j.name.data());
                        }
                        // Add to list of optional buffer
                        optionalBuffers.try_emplace(j.name, newParams);
                    }
                    else
                    {
                        // Check if buffer is an optional write
                        if (auto const k = optionalBuffers.find(j.name); k != optionalBuffers.end())
                        {
                            addBuffer = true;
                        }
                    }
                }
                else
                {
                    addBuffer = true;
                }
                if (addBuffer)
                {
                    // Add the new shared buffer to requested list
                    requestedBuffers.try_emplace(j.name, newParams);
                }
            }
            else
            {
                // Update existing size if it doesn't have one
                if (found->second.size == 0)
                {
                    found->second.size = j.size;
                }
                // Validate that requested values match the existing ones
                else if (found->second.size != j.size && j.size != 0)
                {
                    GFX_PRINTLN("Error: Requested shared buffer with different sizes: %s", j.name.data());
                }
                // Now check the same for stride
                if (found->second.stride == 0)
                {
                    found->second.stride = j.stride;
                }
                else if (found->second.stride != j.stride && j.stride != 0)
                {
                    GFX_PRINTLN("Error: Requested shared buffer with different strides: %s", j.name.data());
                }
                if (((j.flags & SharedBuffer::Flags::Clear)
                        && (found->second.flags & SharedBuffer::Flags::Accumulate))
                    || ((j.flags & SharedBuffer::Flags::Accumulate)
                        && (found->second.flags & SharedBuffer::Flags::Clear)))
                {
                    GFX_PRINTLN(
                        "Error: Requested shared buffer with different clear settings: %s", j.name.data());
                }

                // Add clear/accumulate flag if requested
                if (j.flags & SharedBuffer::Flags::Clear)
                {
                    found->second.flags = (found->second.flags | SharedBuffer::Flags::Clear);
                }
                else if (j.flags & SharedBuffer::Flags::Accumulate)
                {
                    found->second.flags = (found->second.flags | SharedBuffer::Flags::Accumulate);
                }
                else if (j.flags & SharedBuffer::Flags::Allocate)
                {
                    found->second.flags = (found->second.flags | SharedBuffer::Flags::Allocate);
                }
            }
        };
        // Check any internal shared buffers first
        for (auto &j : getStockSharedBuffers())
        {
            bufferFunc(j);
        }

        // Loop through all render techniques and components and check their requested shared buffers
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getSharedBuffers())
            {
                bufferFunc(j);
            }
        }
        for (auto const &i : components_)
        {
            for (auto &j : i.second->getSharedBuffers())
            {
                bufferFunc(j);
            }
        }

        // Merge optional shared buffers
        for (auto &i : optionalBuffers)
        {
            if (auto j = requestedBuffers.find(i.first); j != requestedBuffers.end())
            {
                // Update existing size if it doesn't have one
                if (j->second.size == 0)
                {
                    j->second.size = i.second.size;
                }
                // Validate that requested values match the existing ones
                else if (i.second.size != j->second.size && i.second.size != 0)
                {
                    GFX_PRINTLN("Error: Requested shared buffer with different sizes: %s", i.first.data());
                }
                // Do the same for stride
                if (j->second.stride == 0)
                {
                    j->second.stride = i.second.stride;
                }
                else if (i.second.stride != j->second.stride && i.second.stride != 0)
                {
                    GFX_PRINTLN("Error: Requested shared buffer with different strides: %s", i.first.data());
                }
                if (((i.second.flags & SharedBuffer::Flags::Clear)
                        && (j->second.flags & SharedBuffer::Flags::Accumulate))
                    || ((i.second.flags & SharedBuffer::Flags::Accumulate)
                        && (j->second.flags & SharedBuffer::Flags::Clear)))
                {
                    GFX_PRINTLN(
                        "Error: Requested shared buffer with different clear settings: %s", i.first.data());
                }

                // Add clear/accumulate flag if requested
                if (i.second.flags & SharedBuffer::Flags::Clear)
                {
                    j->second.flags = (j->second.flags | SharedBuffer::Flags::Clear);
                }
                else if (i.second.flags & SharedBuffer::Flags::Accumulate)
                {
                    j->second.flags = (j->second.flags | SharedBuffer::Flags::Accumulate);
                }
                else if (i.second.flags & SharedBuffer::Flags::Allocate)
                {
                    j->second.flags = (j->second.flags | SharedBuffer::Flags::Allocate);
                }
            }
        }

        // Create all requested shared buffers
        for (auto &i : requestedBuffers)
        {
            if (i.second.size == 0 && !(i.second.flags & SharedBuffer::Flags::Allocate))
            {
                GFX_PRINTLN("Error: Requested shared buffer does not have valid size: %s", i.first.data());
                continue;
            }

            // Create new buffer
            GfxBuffer buffer     = gfxCreateBuffer(gfx_, i.second.size);
            auto      bufferName = std::string(i.first);
            bufferName += "SharedBuffer";
            buffer.setName(bufferName.c_str());

            // Set stride if available
            if (i.second.stride != 0)
            {
                buffer.setStride(i.second.stride);
            }

            // Add to clear list
            if (i.second.flags & SharedBuffer::Flags::Clear)
            {
                clear_shared_buffers_.emplace_back(static_cast<uint32_t>(shared_buffers_.size()));
            }

            // Add to buffer list
            shared_buffers_.emplace_back(i.first, buffer);
        }

        // Initialise the buffers
        for (auto const &i : shared_buffers_)
        {
            gfxCommandClearBuffer(gfx_, i.second);
        }
    }

    {
        // Get requested shared textures
        struct TextureParams
        {
            DXGI_FORMAT          format     = DXGI_FORMAT_R16G16B16A16_FLOAT;
            SharedTexture::Flags flags      = SharedTexture::Flags::None;
            uint2                dimensions = uint2(0, 0);
            bool                 mips       = false;
            std::string_view     backup     = "";
        };

        // We use 3 main default shared textures that are always available
        using TextureList                     = std::unordered_map<std::string_view, TextureParams>;
        TextureList const defaultOptionalAOVs = {
            {      "Depth",         {DXGI_FORMAT_D32_FLOAT, SharedTexture::Flags::Clear}},
            {      "Debug", {DXGI_FORMAT_R16G16B16A16_FLOAT, SharedTexture::Flags::None}},
            {"ColorScaled",   {DXGI_FORMAT_R16G16B16A16_FLOAT,
   SharedTexture::Flags::None}                               }, //   Optional AOV used when up-scaling
        };
        TextureList                                            requestedTextures;
        std::unordered_map<std::string_view, std::string_view> backupTextures;
        TextureList                                            optionalTextures;
        auto                                                   textureFunc = [&](SharedTexture const &j) {
            if (auto const found = requestedTextures.find(j.name); found == requestedTextures.end())
            {
                // Check if backup shared texture
                if (backupTextures.contains(j.backup_name))
                {
                    if (j.access != SharedTexture::Access::Read)
                    {
                        GFX_PRINTLN(
                            "Error: Cannot request write access to backup shared texture: %s", j.name.data());
                    }
                    if (j.flags != SharedTexture::Flags::None)
                    {
                        GFX_PRINTLN("Error: Cannot set flags on a backup shared texture: %s", j.name.data());
                    }
                    if (j.format != DXGI_FORMAT_UNKNOWN)
                    {
                        GFX_PRINTLN("Error: Cannot set format on a backup shared texture: %s", j.name.data());
                    }
                    if (!j.backup_name.empty())
                    {
                        GFX_PRINTLN("Error: Cannot create backup of a backup shared texture: %s, %s",
                                                                              j.name.data(), j.backup_name.data());
                    }
                    return;
                }
                // Check if the shared texture is being read despite never having been written to
                if ((j.access == SharedTexture::Access::Read) && !(j.flags & SharedTexture::Flags::Optional)
                    && !optionalTextures.contains(j.name) && (j.flags & SharedTexture::Flags::Clear))
                {
                    GFX_PRINTLN(
                        "Error: Requested read access to shared texture that has not been written to: %s",
                        j.name.data());
                }
                // Check if shared texture is one of the optional default ones and add it using default
                // values
                auto newParams = TextureParams {j.format, j.flags, j.dimensions, j.mips, j.backup_name};
                if (auto const k = defaultOptionalAOVs.find(j.name); k != defaultOptionalAOVs.end())
                {
                    newParams.format = k->second.format;
                    newParams.flags  = k->second.flags;
                    newParams.dimensions = k->second.dimensions;
                }
                bool addTexture = false;
                if (j.flags & SharedTexture::Flags::Optional)
                {
                    if (j.access == SharedTexture::Access::Write)
                    {
                        // Check if texture already contained an optional write
                        if (optionalTextures.contains(j.name))
                        {
                            GFX_PRINTLN(
                                "Error: Found multiple writes to same optional texture: %s", j.name.data());
                        }
                        // Add to list of optional textures
                        optionalTextures.try_emplace(j.name, newParams);
                        if (!j.backup_name.empty())
                        {
                            GFX_PRINTLN(
                                "Error: Requested backup of optional shared texture: %s", j.name.data());
                        }
                    }
                    else
                    {
                        // Check if texture is already an optional write
                        if (auto const k = optionalTextures.find(j.name); k != optionalTextures.end())
                        {
                            addTexture = true;
                        }
                    }
                }
                else
                {
                    addTexture = true;
                }
                if (addTexture)
                {
                    // Add the new shared texture to requested list
                    requestedTextures.try_emplace(j.name, newParams);
                    // Check if also a backup shared texture
                    if (!j.backup_name.empty())
                    {
                        if (auto const pos = backupTextures.find(j.backup_name);
                            pos != backupTextures.end() && pos->second != j.name)
                        {
                            GFX_PRINTLN(
                                "Error: Cannot create multiple different backups with same name: %s, %s",
                                j.name.data(), j.backup_name.data());
                        }
                        else
                        {
                            backupTextures.emplace(j.backup_name, j.name);
                        }
                    }
                }
            }
            else
            {
                // Update existing format if it doesn't have one
                if (found->second.format == DXGI_FORMAT_UNKNOWN)
                {
                    found->second.format = j.format;
                }
                // Validate that requested values match the existing ones
                else if (found->second.format != j.format && j.format != DXGI_FORMAT_UNKNOWN)
                {
                    GFX_PRINTLN("Error: Requested shared texture with different formats: %s", j.name.data());
                }
                if (((j.flags & SharedTexture::Flags::Clear)
                        && (found->second.flags & SharedTexture::Flags::Accumulate))
                    || ((j.flags & SharedTexture::Flags::Accumulate)
                        && (found->second.flags & SharedTexture::Flags::Clear)))
                {
                    GFX_PRINTLN(
                        "Error: Requested shared texture with different clear settings: %s", j.name.data());
                }

                // Update texture size and mips
                if (any(greaterThan(j.dimensions, uint2(0))))
                {
                    if (any(equal(found->second.dimensions, uint2(0))))
                    {
                        found->second.dimensions = j.dimensions;
                    }
                }
                if (any(notEqual(found->second.dimensions, j.dimensions)))
                {
                    GFX_PRINTLN(
                        "Error: Cannot create shared texture with different resolutions: %s", j.name.data());
                }
                if (j.mips)
                {
                    found->second.mips = j.mips;
                }

                // Add backup name if requested
                if (!j.backup_name.empty())
                {
                    if (found->second.backup.empty())
                    {
                        found->second.backup = j.backup_name;
                    }
                    else if (j.backup_name != found->second.backup)
                    {
                        GFX_PRINTLN("Error: Requested shared texture with different backup names: %s, %2",
                                                                              j.name.data(), j.backup_name.data());
                    }
                }
                // Add clear/accumulate flag if requested
                if (j.flags & SharedTexture::Flags::Clear)
                {
                    found->second.flags = found->second.flags | SharedTexture::Flags::Clear;
                }
                else if (j.flags & SharedTexture::Flags::Accumulate)
                {
                    found->second.flags = found->second.flags | SharedTexture::Flags::Accumulate;
                }
            }
        };
        // Check any internal shared buffers first
        for (auto &j : getStockSharedTextures())
        {
            textureFunc(j);
        }

        // Loop through all render techniques and components and check their requested shared textures
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getSharedTextures())
            {
                textureFunc(j);
            }
        }
        for (auto const &i : components_)
        {
            for (auto &j : i.second->getSharedTextures())
            {
                textureFunc(j);
            }
        }

        // Check for up-scaling request
        if (auto j = requestedTextures.find("ColorScaled"); j != requestedTextures.end())
        {
            // Note: Changing the render scale after texture negotiation will not create the display
            // resolution output AOV. Any technique should check for the presence of "ColorScaled" when
            // checking if scaling is enabled
            if (render_scale_ == 1.0F)
            {
                requestedTextures.erase(j);
            }
        }

        // Merge optional shared textures
        for (auto &[textureName, textureParams] : optionalTextures)
        {
            if (auto j = requestedTextures.find(textureName); j != requestedTextures.end())
            {
                // Update existing format if it doesn't have one
                if (j->second.format == DXGI_FORMAT_UNKNOWN)
                {
                    j->second.format = textureParams.format;
                }
                // Validate that requested values match the existing ones
                else if (textureParams.format != j->second.format
                         && textureParams.format != DXGI_FORMAT_UNKNOWN)
                {
                    GFX_PRINTLN(
                        "Error: Requested shared texture with different formats: %s", textureName.data());
                }
                if (((textureParams.flags & SharedTexture::Flags::Clear)
                        && (j->second.flags & SharedTexture::Flags::Accumulate))
                    || ((textureParams.flags & SharedTexture::Flags::Accumulate)
                        && (j->second.flags & SharedTexture::Flags::Clear)))
                {
                    GFX_PRINTLN("Error: Requested shared texture with different clear settings: %s",
                        textureName.data());
                }

                // Update texture size and mips
                if (any(equal(j->second.dimensions, uint2(0))))
                {
                    j->second.dimensions = textureParams.dimensions;
                }
                else if (any(notEqual(j->second.dimensions, textureParams.dimensions)))
                {
                    GFX_PRINTLN("Error: Cannot create shared texture with different resolutions: %s",
                        textureName.data());
                }
                if (!j->second.mips)
                {
                    j->second.mips = textureParams.mips;
                }

                // Add backup name if requested
                if (!textureParams.backup.empty())
                {
                    if (j->second.backup.empty())
                    {
                        j->second.backup = textureParams.backup;
                    }
                    else if (j->second.backup != textureParams.backup)
                    {
                        GFX_PRINTLN("Error: Requested shared texture with different backup names: %s, %2",
                            textureName.data(), j->second.backup.data());
                    }
                }
                // Add clear/accumulate flag if requested
                if ((textureParams.flags & SharedTexture::Flags::Clear))
                {
                    j->second.flags = j->second.flags | SharedTexture::Flags::Clear;
                }
                else if ((textureParams.flags & SharedTexture::Flags::Accumulate))
                {
                    j->second.flags = j->second.flags | SharedTexture::Flags::Accumulate;
                }
            }
        }

        // Create all requested shared textures
        for (auto &[textureName, textureParams] : requestedTextures)
        {
            if (textureParams.format == DXGI_FORMAT_UNKNOWN)
            {
                GFX_PRINTLN(
                    "Error: Requested shared texture does not have valid format: %s", textureName.data());
                continue;
            }

            // Create new texture
            constexpr std::array clear = {0.0F, 0.0F, 0.0F, 0.0F};
            GfxTexture           texture;
            if (all(greaterThan(textureParams.dimensions, uint2(0))))
            {
                texture = gfxCreateTexture2D(gfx_, textureParams.dimensions.x, textureParams.dimensions.y,
                    textureParams.format,
                    textureParams.mips
                        ? gfxCalculateMipCount(textureParams.dimensions.x, textureParams.dimensions.y)
                        : 1,
                    (textureParams.format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear.data());
            }
            else
            {
                // The ColorScaled AOV is a special case that is used for up-scaling. It is set to match the
                // display resolution.
                auto textureDimensions =
                    (textureName != "ColorScaled") ? render_dimensions_ : window_dimensions_;
                texture =
                    gfxCreateTexture2D(gfx_, textureDimensions.x, textureDimensions.y, textureParams.format,
                        textureParams.mips
                            ? gfxCalculateMipCount(textureParams.dimensions.x, textureParams.dimensions.y)
                            : 1,
                        (textureParams.format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear.data());
            }
            auto bufferName = std::string(textureName);
            bufferName += "SharedTexture";
            texture.setName(bufferName.c_str());

            // Add to the backup list
            if (!textureParams.backup.empty())
            {
                // Create new backup texture
                GfxTexture texture2;
                if (all(greaterThan(textureParams.dimensions, uint2(0))))
                {
                    texture2 = gfxCreateTexture2D(gfx_, textureParams.dimensions.x,
                        textureParams.dimensions.y, textureParams.format,
                        textureParams.mips
                            ? gfxCalculateMipCount(textureParams.dimensions.x, textureParams.dimensions.y)
                            : 1,
                        (textureParams.format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear.data());
                }
                else
                {
                    auto textureDimensions =
                        (textureName != "ColorScaled") ? render_dimensions_ : window_dimensions_;
                    texture2 = gfxCreateTexture2D(gfx_, textureDimensions.x, textureDimensions.y,
                        textureParams.format,
                        textureParams.mips
                            ? gfxCalculateMipCount(textureParams.dimensions.x, textureParams.dimensions.y)
                            : 1,
                        (textureParams.format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear.data());
                }
                bufferName = std::string(textureParams.backup);
                bufferName += "SharedTexture";
                texture2.setName(bufferName.c_str());
                auto const location = static_cast<uint32_t>(shared_textures_.size());
                shared_textures_.emplace_back(textureParams.backup, texture2);
                backup_shared_textures_.emplace_back(std::make_pair(location + 1, location));

                // Add the shared texture as a debug view (Using false to differentiate as shared texture)
                if (textureName != "Color" && textureName != "Debug" && textureName != "ColorScaled")
                {
                    debug_views_.emplace_back(textureParams.backup, false);
                }
            }

            // Add to clear list
            if (textureParams.flags & SharedTexture::Flags::Clear)
            {
                clear_shared_textures_.emplace_back(static_cast<uint32_t>(shared_textures_.size()));
            }

            // Add to texture list
            shared_textures_.emplace_back(textureName, texture);

            // Add the shared texture as a debug view (Using false to differentiate as shared texture)
            if (textureName != "Color" && textureName != "Debug" && textureName != "ColorScaled")
            {
                debug_views_.emplace_back(textureName, false);
            }
        }

        // Initialise the shared textures
        for (auto const &i : shared_textures_)
        {
            gfxCommandClearTexture(gfx_, i.second);
        }
    }

    {
        // Get debug views
        auto debugViewFunc = [&](std::string_view &name) {
            auto const k =
                std::ranges::find_if(debug_views_, [&name](auto val) { return val.first == name; });
            if (k == debug_views_.end())
            {
                debug_views_.emplace_back(name, true);
            }
            else if (!k->second)
            {
                // We allow components to override the default debug view if requested
                k->second = true;
            }
            else
            {
                GFX_PRINTLN("Error: Duplicate debug views detected: %s", name.data());
            }
        };

        // Get any stock debug views
        for (auto &j : this->getStockDebugViews())
        {
            debugViewFunc(j);
        }

        // Check components and render techniques for debug views and add them to the internal list
        for (auto const &i : components_)
        {
            for (auto &j : i.second->getDebugViews())
            {
                debugViewFunc(j);
            }
        }
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getDebugViews())
            {
                debugViewFunc(j);
            }
        }
    }
}

void CapsaicinInternal::setupRenderTechniques(std::string_view const &name) noexcept
{
    // Clear any existing shared textures
    for (auto const &i : shared_textures_)
    {
        gfxCommandClearTexture(gfx_, i.second);
    }

    // Delete any existing render techniques
    render_techniques_.clear();

    gfxFinish(gfx_); // flush & sync

    // Delete old options, debug views and other state
    options_.clear();
    components_.clear();
    renderer_name_ = "";
    renderer_      = nullptr;
    resetPlaybackState();

    // Get default internal options
    options_ = getStockRenderOptions();

    // Create the new renderer
    renderer_ = RendererFactory::make(name);
    if (renderer_)
    {
        render_techniques_ = renderer_->setupRenderTechniques(options_);
        renderer_name_     = name;
    }
    else
    {
        GFX_PRINTLN("Error: Unknown renderer requested: %s", name.data());
        return;
    }

    {
        // Get render technique options
        for (auto const &i : render_techniques_)
        {
            options_.merge(i->getRenderOptions());
        }

        // Get stock components
        auto requestedComponents = getStockComponents();

        // Get any components requested by active render techniques
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getComponents())
            {
                if (std::ranges::find(std::as_const(requestedComponents), j) == requestedComponents.cend())
                {
                    // Add the new component to requested list
                    requestedComponents.emplace_back(j);
                }
            }
        }

        // Create all requested components
        for (auto &i : requestedComponents)
        {
            // Create the new component
            if (auto component = ComponentFactory::make(i))
            {
                components_.try_emplace(i, std::move(component));
            }
            else
            {
                GFX_PRINTLN("Error: Unknown component requested: %s", i.data());
            }
        }

        // Create any additional components requested by current components
        while (true)
        {
            std::vector<std::string_view> newRequestedComponents;
            for (auto const &i : requestedComponents)
            {
                for (auto &j : components_[i]->getComponents())
                {
                    if (!components_.contains(j)
                        && std::ranges::find(std::as_const(newRequestedComponents), j)
                               == newRequestedComponents.cend())
                    {
                        // Add the new component to requested list
                        newRequestedComponents.emplace_back(j);
                    }
                }
            }

            if (newRequestedComponents.empty())
            {
                break;
            }

            // Create all requested components
            for (auto &i : newRequestedComponents)
            {
                // Create the new component
                if (auto component = ComponentFactory::make(i))
                {
                    components_.try_emplace(i, std::move(component));
                }
                else
                {
                    GFX_PRINTLN("Error: Unknown component requested: %s", i.data());
                }
            }
            std::swap(newRequestedComponents, requestedComponents);
        }

        // Get component options
        for (auto const &i : components_)
        {
            options_.merge(i.second->getRenderOptions());
        }

        // Check with renderer and set any renderer specific default options
        for (auto const overrides = renderer_->getRenderOptions(); auto const &i : overrides)
        {
            if (auto j = options_.find(i.first); j != options_.end())
            {
                if (j->second.index() == i.second.index())
                {
                    j->second = i.second;
                }
                else
                {
                    GFX_PRINTLN("Error: Attempted to override option using incorrect type: %s",
                        std::string(i.first).c_str());
                }
            }
            else
            {
                GFX_PRINTLN("Error: Unknown override option requested: %s", std::string(i.first).c_str());
            }
        }
    }

    negotiateRenderTechniques();

    // If no scene currently loaded then delay initialisation till scene load
    if (!!scene_)
    {
        // Reset flags as everything is about to get reset anyway
        resetEvents();

        // Initialise all components
        for (auto const &i : components_)
        {
            i.second->setGfxContext(gfx_);
            if (!i.second->init(*this))
            {
                GFX_PRINTLN("Error: Failed to initialise component: %s", i.first.data());
            }
        }

        // Initialise all render techniques
        for (auto const &i : render_techniques_)
        {
            i->setGfxContext(gfx_);
            if (!i->init(*this))
            {
                GFX_PRINTLN("Error: Failed to initialise render technique: %s", i->getName().data());
            }
        }
    }
}

void CapsaicinInternal::resetPlaybackState() noexcept
{
    // Reset frame index
    frame_index_ = std::numeric_limits<uint32_t>::max();
    // Reset frame time
    auto const wallTime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
    current_time_  = static_cast<double>(wallTime.count()) / 1000000.0;
    frame_time_    = 0.0;
    play_time_     = 0.0;
    play_time_old_ = -1.0;
}

void CapsaicinInternal::resetRenderState() const noexcept
{
    // Reset the shared texture history
    {
        GfxCommandEvent const command_event(gfx_, "ResetPreviousGBuffers");

        for (auto const &i : backup_shared_textures_)
        {
            gfxCommandClearTexture(gfx_, shared_textures_[i.second].second);
        }
    }
}

void CapsaicinInternal::destroyAccelerationStructure()
{
    for (auto const &raytracing_primitive : raytracing_primitives_)
    {
        gfxDestroyRaytracingPrimitive(gfx_, raytracing_primitive);
    }

    raytracing_primitives_.clear();

    gfxDestroyAccelerationStructure(gfx_, acceleration_structure_);
}

void CapsaicinInternal::resetEvents() noexcept
{
    render_dimensions_updated_ = false;
    window_dimensions_updated_ = false;
    mesh_updated_              = false;
    transform_updated_         = false;
    environment_map_updated_   = false;
    scene_updated_             = false;
    camera_changed_            = false;
    camera_updated_            = false;
    animation_updated_         = false;
}

} // namespace Capsaicin
