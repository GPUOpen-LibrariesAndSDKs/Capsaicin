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
#include "hash_reduce.h"
#include "render_technique.h"
#include "thread_pool.h"

#define _USE_MATH_DEFINES
#include <chrono>
#include <filesystem>
#include <gfx_imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <math.h>

namespace Capsaicin
{
CapsaicinInternal::CapsaicinInternal() {}

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

uint32_t CapsaicinInternal::getWidth() const
{
    return gfxGetBackBufferWidth(gfx_);
}

uint32_t CapsaicinInternal::getHeight() const
{
    return gfxGetBackBufferHeight(gfx_);
}

char const *CapsaicinInternal::getShaderPath() const
{
    return shader_path_.c_str();
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

void CapsaicinInternal::setPaused(bool paused) noexcept
{
    play_paused_ = paused;
}

bool CapsaicinInternal::getPaused() const noexcept
{
    return play_paused_;
}

void CapsaicinInternal::setFixedFrameRate(bool playMode) noexcept
{
    play_fixed_framerate_ = playMode;
}

void CapsaicinInternal::setFixedFrameTime(double fixed_frame_time) noexcept
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
    frame_index_ = 0;
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

void CapsaicinInternal::stepPlaybackForward(uint32_t frames) noexcept
{
    play_time_ += (double)frames * play_fixed_frame_time_;
}

void CapsaicinInternal::stepPlaybackBackward(uint32_t frames) noexcept
{
    play_time_ -= (double)frames * play_fixed_frame_time_;
}

void CapsaicinInternal::setPlayRewind(bool rewind) noexcept
{
    play_rewind_ = rewind;
}

bool CapsaicinInternal::getPlayRewind() const noexcept
{
    return play_rewind_;
}

void CapsaicinInternal::setRenderPaused(bool paused) noexcept
{
    render_paused_ = paused;
}

bool CapsaicinInternal::getRenderPaused() const noexcept
{
    return render_paused_;
}

void CapsaicinInternal::stepJitterFrameIndex(uint32_t frames) noexcept
{
    uint32_t remaining_frames = std::numeric_limits<uint32_t>::max() - jitter_frame_index_;

    if (frames < remaining_frames)
    {
        jitter_frame_index_ += frames;
    }
    else
    {
        jitter_frame_index_ = frames - remaining_frames;
    }
}

bool CapsaicinInternal::getMeshesUpdated() const noexcept
{
    return mesh_updated_;
}

bool CapsaicinInternal::getTransformsUpdated() const noexcept
{
    return transform_updated_;
}

bool CapsaicinInternal::getSceneUpdated() const noexcept
{
    return scene_updated_;
}

bool CapsaicinInternal::getCameraUpdated() const noexcept
{
    return camera_updated_;
}

bool CapsaicinInternal::getEnvironmentMapUpdated() const noexcept
{
    return environment_map_updated_;
}

std::vector<std::string_view> CapsaicinInternal::getAOVs() const noexcept
{
    std::vector<std::string_view> aovs;
    for (auto const &i : aov_buffers_)
    {
        aovs.emplace_back(i.first);
    }
    return aovs;
}

bool CapsaicinInternal::hasAOVBuffer(std::string_view const &aov) const noexcept
{
    return std::any_of(
        aov_buffers_.cbegin(), aov_buffers_.cend(), [aov](auto const &item) { return item.first == aov; });
}

GfxTexture CapsaicinInternal::getAOVBuffer(std::string_view const &aov) const noexcept
{
    if (auto i = std::find_if(aov_buffers_.cbegin(), aov_buffers_.cend(),
            [aov](auto const &item) { return item.first == aov; });
        i != aov_buffers_.end())
        return i->second;
    GFX_PRINTLN("Error: Unknown VAO requested: %s", aov.data());
    return {};
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

bool CapsaicinInternal::checkDebugViewAOV(std::string_view const &view) const noexcept
{
    if (auto i = std::find_if(debug_views_.cbegin(), debug_views_.cend(),
            [view](auto const &item) { return item.first == view; });
        i != debug_views_.end())
        return !i->second;
    GFX_PRINTLN("Error: Unknown debug view requested: %s", view.data());
    return false;
}

bool CapsaicinInternal::hasBuffer(std::string_view const &buffer) const noexcept
{
    return std::any_of(shared_buffers_.cbegin(), shared_buffers_.cend(),
        [buffer](auto const &item) { return item.first == buffer; });
}

GfxBuffer CapsaicinInternal::getBuffer(std::string_view const &buffer) const noexcept
{
    if (auto i = std::find_if(shared_buffers_.cbegin(), shared_buffers_.cend(),
            [buffer](auto const &item) { return item.first == buffer; });
        i != shared_buffers_.end())
        return i->second;
    GFX_PRINTLN("Error: Unknown buffer requested: %s", buffer.data());
    return {};
}

bool CapsaicinInternal::hasComponent(std::string_view const &component) const noexcept
{
    return std::any_of(components_.cbegin(), components_.cend(),
        [component](auto const &item) { return item.first == component; });
}

std::shared_ptr<Component> const &CapsaicinInternal::getComponent(
    std::string_view const &component) const noexcept
{
    if (auto i = std::find_if(components_.cbegin(), components_.cend(),
            [component](auto const &item) { return item.first == component; });
        i != components_.end())
        return i->second;
    GFX_PRINTLN("Error: Unknown buffer requested: %s", component.data());
    static std::shared_ptr<Component> nullReturn;
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
    auto       renderer =
        std::find_if(renderers.cbegin(), renderers.cend(), [name](auto val) { return name == val; });
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
    setupRenderTechniques(name);
    return true;
}

std::vector<std::string> const &CapsaicinInternal::getCurrentScenes() const noexcept
{
    return scene_files_;
}

bool CapsaicinInternal::setScenes(std::vector<std::string> const &names) noexcept
{
    if (scene_files_ == names)
    {
        // Already loaded
        return true;
    }

    // Clear any pre-existing scene data
    bool initRequired = !!scene_;
    if (initRequired)
    {
        // Reset internal state
        gfxFinish(gfx_); // flush & sync
        // Remove environment map as its tied to scene
        setEnvironmentMap("");
        setDebugView("None");
        gfxDestroyScene(scene_);
        scene_       = {};
        scene_files_ = {};
        resetPlaybackState();
        resetRenderState();
        // Also need to reset the component/techniques
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
    }
    scene_updated_ = true;
    // Create new blank scene
    scene_ = gfxCreateScene();
    if (!scene_)
    {
        return false;
    }

    // Create default user camera
    auto userCamera    = gfxSceneCreateCamera(scene_);
    userCamera->type   = kGfxCameraType_Perspective;
    userCamera->eye    = {0.0f, 0.0f, -1.0f};
    userCamera->center = {0.0f, 0.0f, 0.0f};
    userCamera->up     = {0.0f, 1.0f, 0.0f};
    userCamera->aspect = getWidth() / (float)getHeight();
    userCamera->fovY   = DegreesToRadians(90.0f);
    userCamera->nearZ  = 0.1f;
    userCamera->farZ   = 1e4f;
    GfxMetadata userCameraMeta;
    userCameraMeta.object_name = "User";
    gfxSceneSetCameraMetadata(scene_, gfxSceneGetCameraHandle(scene_, 0), userCameraMeta);

    // Load in scene based on current requested scene index
    for (auto const &name : names)
    {
        if (gfxSceneImport(scene_, name.c_str()) != kGfxResult_NoError)
        {
            GFX_PRINT_ERROR(kGfxResult_InternalError,
                "Scene '%s' can't be loaded, clear the scene and abort.", name.c_str());
            gfxSceneClear(scene_);
            return false;
        }
    }

    scene_files_ = names;

    // Set up camera based on internal scene data
    uint32_t       cameraIndex = 0;
    const uint32_t cameraCount = gfxSceneGetCameraCount(scene_);
    if (cameraCount > 1)
    {
        cameraIndex = 1; // Use first scene camera
        // Try and find 'Main' camera
        for (uint32_t i = 1; i < cameraCount; ++i)
        {
            auto        cameraHandle = gfxSceneGetCameraHandle(scene_, i);
            auto        metaData     = gfxSceneGetCameraMetadata(scene_, cameraHandle);
            std::string cameraName   = metaData.getObjectName();
            if (cameraName.find("Camera") == 0 && cameraName.length() > 6)
            {
                cameraName           = cameraName.substr(6);
                metaData.object_name = cameraName;
                gfxSceneSetCameraMetadata(scene_, cameraHandle, metaData);
            }
            if (cameraName.find("Main") != std::string_view::npos)
            {
                cameraIndex = i;
            }
        }
        // Set user camera equal to first camera
        auto defaultCamera = gfxSceneGetCameraHandle(scene_, cameraIndex);
        userCamera->eye    = defaultCamera->eye;
        userCamera->center = defaultCamera->center;
        userCamera->up     = defaultCamera->up;
    }
    auto camera = gfxSceneGetCameraHandle(scene_, cameraIndex);
    camera->aspect =
        static_cast<float>(gfxGetBackBufferWidth(gfx_)) / static_cast<float>(gfxGetBackBufferHeight(gfx_));
    if (!gfxSceneSetActiveCamera(scene_, camera) == kGfxResult_NoError)
    {
        return false;
    }

    // Calculate some scene stats
    triangle_count_ = 0;
    for (uint32_t i = 0; i < gfxSceneGetObjectCount<GfxInstance>(scene_); ++i)
    {
        if (gfxSceneGetObjects<GfxInstance>(scene_)[i].mesh)
        {
            GfxMesh const &mesh = *gfxSceneGetObjects<GfxInstance>(scene_)[i].mesh;
            triangle_count_ += (uint32_t)(mesh.indices.size() / 3);
        }
    }

    // Re-initialise the components/techniques. Also handle delayed loading of renderer when a scene
    // previously hadnt been set.
    if (initRequired || !renderer_name_.empty())
    {
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

        // Reset flags as everything just got forced reset anyway
        mesh_updated_            = false;
        transform_updated_       = false;
        environment_map_updated_ = false;
        scene_updated_           = false;
        camera_updated_          = false;
    }

    return true;
}

std::vector<std::string_view> CapsaicinInternal::getSceneCameras() const noexcept
{
    std::vector<std::string_view> ret;
    for (uint32_t i = 0; i < gfxSceneGetCameraCount(scene_); ++i)
    {
        auto cameraHandle = gfxSceneGetCameraHandle(scene_, i);
        ret.emplace_back(gfxSceneGetCameraMetadata(scene_, cameraHandle).getObjectName());
    }
    return ret;
}

std::string_view CapsaicinInternal::getSceneCurrentCamera() const noexcept
{
    auto const ret = gfxSceneGetCameraMetadata(scene_, gfxSceneGetActiveCamera(scene_)).getObjectName();
    return ret;
}

GfxRef<GfxCamera> CapsaicinInternal::getSceneCamera() const noexcept
{
    return gfxSceneGetActiveCamera(scene_);
}

bool CapsaicinInternal::setSceneCamera(std::string_view const &name) noexcept
{
    // Convert camera name to an index
    auto const cameras     = getSceneCameras();
    auto const cameraIndex = std::find(cameras.begin(), cameras.end(), name);
    if (cameraIndex == cameras.end())
    {
        GFX_PRINTLN("Error: Invalid camera requested: %s", name.data());
        return false;
    }
    auto camera = gfxSceneGetCameraHandle(scene_, static_cast<uint32_t>(cameraIndex - cameras.begin()));
    camera->aspect =
        static_cast<float>(gfxGetBackBufferWidth(gfx_)) / static_cast<float>(gfxGetBackBufferHeight(gfx_));
    if (!gfxSceneSetActiveCamera(scene_, camera) == kGfxResult_NoError)
    {
        return false;
    }
    camera_updated_ = true;
    resetRenderState();
    return true;
}

std::string CapsaicinInternal::getCurrentEnvironmentMap() const noexcept
{
    return environment_map_file_;
}

bool CapsaicinInternal::setEnvironmentMap(std::string const &name) noexcept
{
    if (environment_map_file_ == name)
    {
        // Already loaded
        return true;
    }

    // Remove the old environment map
    if (!!environment_buffer_)
    {
        gfxDestroyTexture(gfx_, environment_buffer_);
        environment_buffer_ = {};
    }
    environment_map_updated_ = true;

    resetRenderState();

    if (name.empty())
    {
        // If empty file requested then just use blank environment map
        environment_map_file_ = "";
        return true;
    }
    // Load in the new environment map
    if (gfxSceneImport(scene_, name.c_str()) != kGfxResult_NoError)
    {
        return false;
    }

    // Update render settings
    auto environmentMap = gfxSceneFindObjectByAssetFile<GfxImage>(scene_, name.c_str());

    if (!environmentMap)
    {
        GFX_PRINTLN("Failed to find valid environment map source file: %s", name.data());
        return false;
    }
    environment_map_file_ = name;

    // Create environment map texture
    uint32_t const environment_buffer_size = 1024;
    uint32_t const environment_buffer_mips = gfxCalculateMipCount(environment_buffer_size);

    environment_buffer_ = gfxCreateTextureCube(
        gfx_, environment_buffer_size, DXGI_FORMAT_R16G16B16A16_FLOAT, environment_buffer_mips);
    environment_buffer_.setName("Capsaicin_EnvironmentBuffer");

    uint32_t const environment_map_width  = environmentMap->width;
    uint32_t const environment_map_height = environmentMap->height;
    uint32_t const environment_map_mip_count =
        gfxCalculateMipCount(environment_map_width, environment_map_height);
    uint32_t const environment_map_channel_count     = environmentMap->channel_count;
    uint32_t const environment_map_bytes_per_channel = environmentMap->bytes_per_channel;

    GfxTexture environment_map = gfxCreateTexture2D(gfx_, environment_map_width, environment_map_height,
        environmentMap->format, environment_map_mip_count);
    {
        GfxBuffer upload_buffer = gfxCreateBuffer(gfx_,
            (size_t)environment_map_width * environment_map_height * environment_map_channel_count
                * environment_map_bytes_per_channel,
            environmentMap->data.data(), kGfxCpuAccess_Write);
        gfxCommandCopyBufferToTexture(gfx_, environment_map, upload_buffer);
        gfxCommandGenerateMips(gfx_, environment_map);
        gfxDestroyBuffer(gfx_, upload_buffer);
    }

    glm::dvec3 const forward_vectors[] = {glm::dvec3(-1.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0), glm::dvec3(0.0, 0.0, -1.0),
        glm::dvec3(0.0, 0.0, 1.0)};

    glm::dvec3 const up_vectors[] = {glm::dvec3(0.0, -1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0),
        glm::dvec3(0.0, 0.0, -1.0), glm::dvec3(0.0, 0.0, 1.0), glm::dvec3(0.0, -1.0, 0.0),
        glm::dvec3(0.0, -1.0, 0.0)};

    for (uint32_t cubemap_face = 0; cubemap_face < 6; ++cubemap_face)
    {
        GfxDrawState draw_sky_state = {};
        gfxDrawStateSetColorTarget(draw_sky_state, 0, environment_buffer_, 0, cubemap_face);

        GfxKernel draw_sky_kernel =
            gfxCreateGraphicsKernel(gfx_, convolve_ibl_program_, draw_sky_state, "DrawSky");

        uint32_t const buffer_dimensions[] = {
            environment_buffer_.getWidth(), environment_buffer_.getHeight()};

        glm::dmat4 const view =
            glm::lookAt(glm::dvec3(0.0), forward_vectors[cubemap_face], up_vectors[cubemap_face]);
        glm::dmat4 const proj          = glm::perspective(M_PI / 2.0, 1.0, 0.1, 1e4);
        glm::mat4 const  view_proj_inv = glm::mat4(glm::inverse(proj * view));

        gfxProgramSetParameter(gfx_, convolve_ibl_program_, "g_BufferDimensions", buffer_dimensions);
        gfxProgramSetParameter(gfx_, convolve_ibl_program_, "g_ViewProjectionInverse", view_proj_inv);

        gfxProgramSetParameter(gfx_, convolve_ibl_program_, "g_EnvironmentMap", environment_map);

        gfxProgramSetParameter(gfx_, convolve_ibl_program_, "g_LinearSampler", linear_sampler_);

        gfxCommandBindKernel(gfx_, draw_sky_kernel);
        gfxCommandDraw(gfx_, 3);

        gfxDestroyKernel(gfx_, draw_sky_kernel);
    }

    GfxKernel blur_sky_kernel = gfxCreateComputeKernel(gfx_, convolve_ibl_program_, "BlurSky");

    for (uint32_t mip_level = 1; mip_level < environment_buffer_mips; ++mip_level)
    {
        gfxProgramSetParameter(
            gfx_, convolve_ibl_program_, "g_InEnvironmentBuffer", environment_buffer_, mip_level - 1);
        gfxProgramSetParameter(
            gfx_, convolve_ibl_program_, "g_OutEnvironmentBuffer", environment_buffer_, mip_level);

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, blur_sky_kernel);
        uint32_t const  num_groups_x =
            (GFX_MAX(environment_buffer_size >> mip_level, 1u) + num_threads[0] - 1) / num_threads[0];
        uint32_t const num_groups_y =
            (GFX_MAX(environment_buffer_size >> mip_level, 1u) + num_threads[1] - 1) / num_threads[1];
        uint32_t const num_groups_z = 6; // blur all faces

        gfxCommandBindKernel(gfx_, blur_sky_kernel);
        gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, num_groups_z);
    }

    gfxDestroyKernel(gfx_, blur_sky_kernel);
    auto handle = gfxSceneGetImageHandle(scene_, environmentMap.getIndex());
    gfxSceneDestroyImage(scene_, handle);
    gfxDestroyTexture(gfx_, environment_map);
    return true;
}

std::string_view CapsaicinInternal::getCurrentDebugView() const noexcept
{
    return debug_view_;
}

bool CapsaicinInternal::setDebugView(std::string_view const &name) noexcept
{
    auto debugView = std::find_if(
        debug_views_.cbegin(), debug_views_.cend(), [this](auto val) { return debug_view_ == val.first; });
    if (debugView == debug_views_.cend())
    {
        GFX_PRINTLN("Error: Requested invalid debug view: %s", name.data());
        return false;
    }
    debug_view_ = name;
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

glm::vec4 CapsaicinInternal::getInvDeviceZ() const
{
    return glm::vec4(0.0f); // this is only here for compatibility with UE5
}

glm::vec3 CapsaicinInternal::getPreViewTranslation() const
{
    return glm::vec3(0.0f); // not needed when running through Capsaicin
}

GfxTexture CapsaicinInternal::getEnvironmentBuffer() const
{
    return environment_buffer_;
}

GfxCamera const &CapsaicinInternal::getCamera() const
{
    // Get hold of the active camera (can be animated)
    GfxConstRef<GfxCamera> camera_ref = gfxSceneGetActiveCamera(scene_);
    return *camera_ref;
}

CameraMatrices const &CapsaicinInternal::getCameraMatrices(bool jittered) const
{
    return camera_matrices_[jittered];
}

GfxBuffer CapsaicinInternal::getCameraMatricesBuffer(bool jittered) const
{
    return camera_matrices_buffer_[jittered];
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
    uint64_t       bvh_data_size = gfxAccelerationStructureGetDataSize(gfx_, acceleration_structure_);
    uint32_t const rt_primitive_count =
        gfxAccelerationStructureGetRaytracingPrimitiveCount(gfx_, acceleration_structure_);

    for (uint32_t i = 0; i < rt_primitive_count; ++i)
    {
        GfxRaytracingPrimitive const &rt_primitive =
            gfxAccelerationStructureGetRaytracingPrimitives(gfx_, acceleration_structure_)[i];
        bvh_data_size += gfxRaytracingPrimitiveGetDataSize(gfx_, rt_primitive);
    }

    return bvh_data_size;
}

GfxBuffer CapsaicinInternal::getInstanceBuffer() const
{
    return instance_buffer_;
}

Instance const *CapsaicinInternal::getInstanceData() const
{
    return instance_data_.data();
}

Instance *CapsaicinInternal::getInstanceData()
{
    return instance_data_.data();
}

glm::vec3 const *CapsaicinInternal::getInstanceMinBounds() const
{
    return instance_min_bounds_.data();
}

glm::vec3 const *CapsaicinInternal::getInstanceMaxBounds() const
{
    return instance_max_bounds_.data();
}

GfxBuffer CapsaicinInternal::getInstanceIdBuffer() const
{
    return instance_id_buffer_;
}

uint32_t const *CapsaicinInternal::getInstanceIdData() const
{
    return instance_id_data_.data();
}

GfxBuffer CapsaicinInternal::getTransformBuffer() const
{
    return transform_buffer_;
}

glm::mat4x3 const *CapsaicinInternal::getTransformData() const
{
    return transform_data_.data();
}

GfxBuffer CapsaicinInternal::getPrevTransformBuffer() const
{
    return prev_transform_buffer_;
}

glm::mat4x3 const *CapsaicinInternal::getPrevTransformData() const
{
    return prev_transform_data_.data();
}

GfxBuffer CapsaicinInternal::getMaterialBuffer() const
{
    return material_buffer_;
}

Material const *CapsaicinInternal::getMaterialData() const
{
    return material_data_.data();
}

GfxTexture const *CapsaicinInternal::getTextures() const
{
    return texture_atlas_.data();
}

uint32_t CapsaicinInternal::getTextureCount() const
{
    return (uint32_t)texture_atlas_.size();
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

GfxBuffer CapsaicinInternal::getMeshBuffer() const
{
    return mesh_buffer_;
}

Mesh const *CapsaicinInternal::getMeshData() const
{
    return mesh_data_.data();
}

GfxBuffer CapsaicinInternal::getIndexBuffer() const
{
    return index_buffer_;
}

uint32_t const *CapsaicinInternal::getIndexData() const
{
    return index_data_.data();
}

GfxBuffer CapsaicinInternal::getVertexBuffer() const
{
    return vertex_buffer_;
}

Vertex const *CapsaicinInternal::getVertexData() const
{
    return vertex_data_.data();
}

GfxBuffer const *CapsaicinInternal::getIndexBuffers() const
{
    return &index_buffer_;
}

uint32_t CapsaicinInternal::getIndexBufferCount() const
{
    return 1;
}

GfxBuffer const *CapsaicinInternal::getVertexBuffers() const
{
    return &vertex_buffer_;
}

uint32_t CapsaicinInternal::getVertexBufferCount() const
{
    return 1;
}

GfxAccelerationStructure CapsaicinInternal::getAccelerationStructure() const
{
    return acceleration_structure_;
}

std::pair<float3, float3> CapsaicinInternal::getSceneBounds() const
{
    // Calculate the scene bounds
    const uint32_t numInstance = gfxSceneGetObjectCount<GfxInstance>(getScene());
    float3         sceneMin(std::numeric_limits<float>::max());
    float3         sceneMax(std::numeric_limits<float>::lowest());
    for (uint i = 0; i < numInstance; ++i)
    {
        const uint32_t instanceIndex     = getInstanceIdData()[i];
        auto const    &instanceMinBounds = getInstanceMinBounds()[instanceIndex];
        auto const    &instanceMaxBounds = getInstanceMaxBounds()[instanceIndex];
        const float3   minBounds         = glm::min(instanceMinBounds, instanceMaxBounds);
        const float3   maxBounds         = glm::max(instanceMinBounds, instanceMaxBounds);
        sceneMin                         = glm::min(sceneMin, minBounds);
        sceneMax                         = glm::max(sceneMax, maxBounds);
    }
    return std::make_pair(sceneMin, sceneMax);
}

GfxBuffer CapsaicinInternal::allocateConstantBuffer(uint64_t size)
{
    GfxBuffer     &constant_buffer_pool        = constant_buffer_pools_[gfxGetBackBufferIndex(gfx_)];
    uint64_t const constant_buffer_pool_cursor = GFX_ALIGN(constant_buffer_pool_cursor_ + size, 256);

    if (constant_buffer_pool_cursor >= constant_buffer_pool.getSize())
    {
        gfxDestroyBuffer(gfx_, constant_buffer_pool);

        uint64_t constant_buffer_pool_size = constant_buffer_pool_cursor;
        constant_buffer_pool_size += ((constant_buffer_pool_size + 2) >> 1);
        constant_buffer_pool_size = GFX_ALIGN(constant_buffer_pool_size, 65536);

        constant_buffer_pool = gfxCreateBuffer(gfx_, constant_buffer_pool_size, nullptr, kGfxCpuAccess_Write);

        char buffer[256];
        GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ConstantBufferPool%u", gfxGetBackBufferIndex(gfx_));

        constant_buffer_pool.setName(buffer);
    }

    GfxBuffer constant_buffer =
        gfxCreateBufferRange(gfx_, constant_buffer_pool, constant_buffer_pool_cursor_, size);

    constant_buffer_pool_cursor_ = constant_buffer_pool_cursor;

    return constant_buffer;
}

void CapsaicinInternal::initialize(GfxContext gfx, ImGuiContext *imgui_context)
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
    shader_path_ = "src/core/src/";
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

    char const *screen_triangle_vs =
        "struct VS_OUTPUT { float4 pos : SV_POSITION; float2 texcoord : TEXCOORD; };"
        "VS_OUTPUT main(in uint idx : SV_VertexID) { VS_OUTPUT output; output.texcoord = float2(1.0f - 2.0f * (idx & 1), 2.0f * (idx >> 1));"
        "output.pos = 1.0f - float4(4.0f * (idx & 1), 4.0f * (idx >> 1), 1.0f, 0.0f); return output; }";

    GfxProgramDesc blit_program_desc = {};
    blit_program_desc.vs             = screen_triangle_vs;
    blit_program_desc.ps =
        "Texture2D ColorBuffer; float4 main(in float4 pos : SV_POSITION, float2 texcoord : TEXCOORD) : SV_Target {"
        "int2 dims; ColorBuffer.GetDimensions(dims.x, dims.y);"
        "return ColorBuffer.Load(int3(texcoord * dims, 0)); }";
    blit_program_ = gfxCreateProgram(gfx, blit_program_desc, "Capsaicin_BlitProgram");
    blit_kernel_  = gfxCreateGraphicsKernel(gfx, blit_program_);

    GfxProgramDesc debug_depth_program_desc = {};
    debug_depth_program_desc.vs             = screen_triangle_vs;
    debug_depth_program_desc.ps =
        "Texture2D DepthBuffer; float4x4 ViewProjectionInverse; float4 main(in float4 pos : SV_Position) : SV_Target {"
        "float depth = DepthBuffer.Load(int3(pos.xy, 0)).x;"
        "int3 dims; DepthBuffer.GetDimensions(0, dims.x, dims.y, dims.z);"
        "float4 world = mul(ViewProjectionInverse, float4((2.0f * pos.xy / dims.xy - 1.0f) * float2(1.0f, -1.0f), depth, 1.0f)); world /= world.w;"
        "float3 color = abs(float3(uint3(abs(world.xyz)) & 1) - frac(abs(world.xyz)));"
        "return float4(color * (depth < 1.0f ? 1.0f : 0.0f), 1.0f); }";
    debug_depth_program_ = gfxCreateProgram(gfx, debug_depth_program_desc, "Capsaicin_DebugDepthProgram");
    debug_depth_kernel_  = gfxCreateGraphicsKernel(gfx, debug_depth_program_);

    convolve_ibl_program_ = gfxCreateProgram(gfx, "capsaicin/convolve_ibl", shader_path_.c_str());

    dump_copy_to_buffer_program_ =
        gfxCreateProgram(gfx, "capsaicin/dump_copy_aov_to_buffer", shader_path_.c_str());
    dump_copy_to_buffer_kernel_ =
        gfxCreateComputeKernel(gfx, dump_copy_to_buffer_program_, "CopyAOVToBuffer");

    buffer_width_  = gfxGetBackBufferWidth(gfx);
    buffer_height_ = gfxGetBackBufferHeight(gfx);

    ImGui::SetCurrentContext(imgui_context);

    gfx_ = gfx;
}

void CapsaicinInternal::render()
{
    // Update current frame time
    auto const previousTime = current_time_;
    auto       wallTime     = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
    current_time_ = static_cast<double>(wallTime.count()) / 1000000.0;
    frame_time_   = current_time_ - previousTime;

    // Check if manual frame increment/decrement has been applied
    bool manual_play = play_time_ != play_time_old_;

    if (!render_paused_ || manual_play || frame_index_ == 0)
    {
        // Reset update flags
        mesh_updated_      = false;
        transform_updated_ = false;

        frameGraph.addValue(static_cast<float>(frame_time_));

        constant_buffer_pool_cursor_ = 0;
        was_resized_ =
            (buffer_width_ != gfxGetBackBufferWidth(gfx_) || buffer_height_ != gfxGetBackBufferHeight(gfx_));
        buffer_width_  = gfxGetBackBufferWidth(gfx_);
        buffer_height_ = gfxGetBackBufferHeight(gfx_);

        // Run the animations
        bool animation = false;
        if (!play_paused_ || manual_play)
        {
            if (!play_paused_)
            {
                if (play_fixed_framerate_)
                {
                    play_time_ += play_fixed_frame_time_ * play_speed_ * (!play_rewind_ ? 1.0 : -1.0);
                }
                else
                {
                    play_time_ += frame_time_ * play_speed_ * (!play_rewind_ ? 1.0 : -1.0);
                }
            }
            play_time_old_                 = play_time_;
            uint32_t const animation_count = gfxSceneGetAnimationCount(scene_);
            animation                      = animation_count > 0;
            for (uint32_t animation_index = 0; animation_index < animation_count; ++animation_index)
            {
                GfxConstRef<GfxAnimation> animation_ref = gfxSceneGetAnimationHandle(scene_, animation_index);
                float const animation_length            = gfxSceneGetAnimationLength(scene_, animation_ref);
                float       time_in_seconds             = (float)fmod(play_time_, (double)animation_length);
                // Handle negative playback times
                time_in_seconds =
                    (time_in_seconds >= 0.0f) ? time_in_seconds : animation_length + time_in_seconds;
                gfxSceneApplyAnimation(scene_, animation_ref, time_in_seconds);
            }
        }

        // Calculate the camera matrices for this frame
        {
            uint32_t const jitter_index = jitter_frame_index_ != ~0 ? jitter_frame_index_ : frame_index_;

            auto const &camera = getCamera();
            for (uint32_t i = 0; i < 2; ++i)
            {
                camera_jitter_x_ =
                    (i > 0 ? (2.0f * CalculateHaltonNumber(jitter_index + 1, 2) - 1.0f) / getWidth() : 0.0f);
                camera_jitter_y_ =
                    (i > 0 ? (2.0f * CalculateHaltonNumber(jitter_index + 1, 3) - 1.0f) / getHeight() : 0.0f);
                camera_matrices_[i].projection[2][0] = camera_jitter_x_;
                camera_matrices_[i].projection[2][1] = camera_jitter_y_;
                camera_matrices_[i].view_projection =
                    glm::dmat4(camera_matrices_[i].projection) * glm::dmat4(camera_matrices_[i].view);
                camera_matrices_[i].view_prev                = camera_matrices_[i].view;
                camera_matrices_[i].projection_prev          = camera_matrices_[i].projection;
                camera_matrices_[i].view_projection_prev     = camera_matrices_[i].view_projection;
                camera_matrices_[i].inv_view_projection_prev = camera_matrices_[i].inv_view_projection;
                glm::dmat4 const view =
                    glm::lookAt(glm::dvec3(camera.eye), glm::dvec3(camera.center), glm::dvec3(camera.up));
                camera_matrices_[i].view = glm::mat4(view);
                glm::dmat4 projection    = glm::perspective(
                    (double)camera.fovY, (double)camera.aspect, (double)camera.nearZ, (double)camera.farZ);
                projection[2][0]                        = camera_jitter_x_;
                projection[2][1]                        = camera_jitter_y_;
                camera_matrices_[i].projection          = glm::mat4(projection);
                glm::dmat4 const view_projection        = projection * view;
                camera_matrices_[i].view_projection     = glm::mat4(view_projection);
                glm::dmat4 const inv_view_projection    = glm::inverse(view_projection);
                camera_matrices_[i].inv_view_projection = glm::mat4(inv_view_projection);
                camera_matrices_[i].inv_projection      = glm::mat4(glm::inverse(projection));
                camera_matrices_[i].inv_view            = glm::mat4(glm::inverse(view));
                camera_matrices_[i].reprojection =
                    glm::mat4(glm::dmat4(camera_matrices_[i].view_projection_prev) * inv_view_projection);

                // Update camera matrices
                {
                    gfxDestroyBuffer(gfx_, camera_matrices_buffer_[i]);
                    camera_matrices_buffer_[i] = allocateConstantBuffer<CameraMatrices>(1);
                    memcpy(gfxBufferGetData(gfx_, camera_matrices_buffer_[i]), &camera_matrices_[i],
                        sizeof(camera_matrices_[i]));
                }
            }
        }

        // Update the scene history
        {
            for (size_t i = 0; i < prev_transform_data_.size(); ++i)
            {
                prev_transform_data_[i] = transform_data_[i];
            }

            if (!prev_transform_data_.empty())
            {
                GfxCommandEvent const command_event(gfx_, "UpdatePreviousTranforms");
                GfxBuffer             prev_transform_buffer =
                    allocateConstantBuffer<glm::mat4x3>((uint32_t)prev_transform_data_.size());
                memcpy(gfxBufferGetData(gfx_, prev_transform_buffer), prev_transform_data_.data(),
                    prev_transform_data_.size() * sizeof(glm::mat4x3));
                gfxCommandCopyBuffer(gfx_, prev_transform_buffer_, prev_transform_buffer);
                gfxDestroyBuffer(gfx_, prev_transform_buffer);
            }
        }

        // Update the AOV history
        {
            GfxCommandEvent const command_event(gfx_, "UpdatePreviousGBuffers");

            for (auto &i : aov_backup_buffers_)
            {
                gfxCommandCopyTexture(gfx_, i.second, i.first);
            }
        }

        // Clear our AOVs
        {
            const GfxCommandEvent command_event(gfx_, "ClearGBuffers");

            if (!was_resized_)
            {
                for (auto &i : aov_clear_buffers_)
                {
                    gfxCommandClearTexture(gfx_, i);
                }

                if (!debug_view_.empty() && debug_view_ != "None")
                {
                    gfxCommandClearTexture(gfx_, getAOVBuffer("Debug"));
                }
            }
            else
            {
                for (auto &i : aov_buffers_)
                {
                    gfxCommandClearTexture(gfx_, i.second);
                }
            }
        }

        // Check whether we need to re-build our acceleration structure
        size_t mesh_hash = mesh_hash_;
        if (frame_index_ == 0 || animation)
            mesh_hash =
                HashReduce(gfxSceneGetObjects<GfxMesh>(scene_), gfxSceneGetObjectCount<GfxMesh>(scene_));

        if (mesh_hash != mesh_hash_)
        {
            GfxCommandEvent const command_event(gfx_, "BuildScene");
            mesh_updated_ = true;

            mesh_data_.clear();
            index_data_.clear();
            vertex_data_.clear();
            instance_data_.clear();
            material_data_.clear();
            transform_data_.clear();

            gfxDestroyBuffer(gfx_, mesh_buffer_);
            gfxDestroyBuffer(gfx_, index_buffer_);
            gfxDestroyBuffer(gfx_, vertex_buffer_);
            gfxDestroyBuffer(gfx_, instance_buffer_);
            gfxDestroyBuffer(gfx_, material_buffer_);
            gfxDestroyBuffer(gfx_, transform_buffer_);

            for (GfxTexture const &texture : texture_atlas_)
            {
                gfxDestroyTexture(gfx_, texture);
            }

            texture_atlas_.clear();
            raytracing_primitives_.clear();

            gfxDestroyAccelerationStructure(gfx_, acceleration_structure_);

            GfxMaterial const *materials      = gfxSceneGetObjects<GfxMaterial>(scene_);
            uint32_t const     material_count = gfxSceneGetObjectCount<GfxMaterial>(scene_);

            for (uint32_t i = 0; i < material_count; ++i)
            {
                Material material = {};

                material.albedo = float4(
                    float3(materials[i].albedo), glm::uintBitsToFloat((uint32_t)materials[i].albedo_map));
                material.emissivity = float4(
                    materials[i].emissivity, glm::uintBitsToFloat((uint32_t)materials[i].emissivity_map));
                material.metallicity_roughness = float4(materials[i].metallicity,
                    glm::uintBitsToFloat((uint32_t)materials[i].metallicity_map), materials[i].roughness,
                    glm::uintBitsToFloat((uint32_t)materials[i].roughness_map));
                material.normal_alpha_side =
                    float4(glm::uintBitsToFloat((uint32_t)materials[i].normal_map), materials[i].albedo.w,
                        glm::uintBitsToFloat(
                            (uint32_t)((materials[i].flags & kGfxMaterialFlag_DoubleSided) != 0)),
                        0.0f);

                uint32_t const material_index = gfxSceneGetObjectHandle<GfxMaterial>(scene_, i);

                if (material_index >= material_data_.size())
                {
                    material_data_.resize((size_t)material_index + 1);
                }

                material_data_[material_index] = material;
            }

            material_buffer_ =
                gfxCreateBuffer<Material>(gfx_, (uint32_t)material_data_.size(), material_data_.data());
            material_buffer_.setName("Capsaicin_MaterialBuffer");

            uint32_t const image_count = gfxSceneGetObjectCount<GfxImage>(scene_);

            for (uint32_t i = 0; i < image_count; ++i)
            {
                GfxConstRef<GfxImage> image_ref = gfxSceneGetObjectHandle<GfxImage>(scene_, i);

                uint32_t const image_index = (uint32_t)image_ref;

                if (image_index >= texture_atlas_.size())
                {
                    texture_atlas_.resize((size_t)image_index + 1);
                }

                GfxTexture &texture = texture_atlas_[image_index];

                DXGI_FORMAT    format         = image_ref->format;
                uint32_t       image_width    = image_ref->width;
                uint32_t       image_height   = image_ref->height;
                uint32_t const image_mips     = gfxCalculateMipCount(image_width, image_height);
                uint32_t const image_channels = image_ref->channel_count;

                texture = gfxCreateTexture2D(gfx_, image_width, image_height, format, image_mips);
                texture.setName(gfxSceneGetObjectMetadata<GfxImage>(scene_, image_ref).getObjectName());

                if (!image_ref->width || !image_ref->height)
                {
                    gfxCommandClearTexture(gfx_, texture);
                }
                else
                {
                    uint8_t const *image_data = image_ref->data.data();

                    const uint64_t uncompressed_size =
                        (uint64_t)image_width * image_height * image_channels * image_ref->bytes_per_channel;
                    uint64_t texture_size =
                        !gfxImageIsFormatCompressed(*image_ref) ? uncompressed_size : image_ref->data.size();
                    bool const mips = image_ref->flags & kGfxImageFlag_HasMipLevels;
                    if (mips && !gfxImageIsFormatCompressed(*image_ref))
                    {
                        texture_size += texture_size / 3;
                    }
                    texture_size = GFX_MIN(texture_size, image_ref->data.size());
                    GfxBuffer texture_data =
                        gfxCreateBuffer(gfx_, texture_size, image_data, kGfxCpuAccess_Write);

                    gfxCommandCopyBufferToTexture(gfx_, texture, texture_data);
                    if (!mips && !gfxImageIsFormatCompressed(*image_ref))
                        gfxCommandGenerateMips(gfx_, texture);
                    gfxDestroyBuffer(gfx_, texture_data);
                }
            }
            GfxMesh const *meshes     = gfxSceneGetObjects<GfxMesh>(scene_);
            uint32_t const mesh_count = gfxSceneGetObjectCount<GfxMesh>(scene_);

            for (uint32_t i = 0; i < mesh_count; ++i)
            {
                Mesh mesh = {};

                mesh.vertex_offset_idx = (uint32_t)vertex_data_.size();
                mesh.index_offset_idx  = (uint32_t)index_data_.size();
                mesh.index_count       = (uint32_t)meshes[i].indices.size();

                uint32_t const mesh_index = gfxSceneGetObjectHandle<GfxMesh>(scene_, i);

                if (mesh_index >= mesh_data_.size())
                {
                    mesh_data_.resize((size_t)mesh_index + 1);
                }

                mesh_data_[mesh_index] = mesh;
                for (size_t j = 0; j < meshes[i].indices.size(); ++j)
                    index_data_.push_back(meshes[i].indices[j]);

                for (size_t j = 0; j < meshes[i].vertices.size(); ++j)
                {
                    Vertex vertex = {};

                    vertex.position = float4(meshes[i].vertices[j].position, 1.0f);
                    vertex.normal   = float4(meshes[i].vertices[j].normal, 0.0f);
                    vertex.uv       = float2(meshes[i].vertices[j].uv);

                    vertex_data_.push_back(vertex);
                }
            }

            mesh_buffer_  = gfxCreateBuffer<Mesh>(gfx_, (uint32_t)mesh_data_.size(), mesh_data_.data());
            index_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, (uint32_t)index_data_.size(), index_data_.data());
            vertex_buffer_ =
                gfxCreateBuffer<Vertex>(gfx_, (uint32_t)vertex_data_.size(), vertex_data_.data());

            mesh_buffer_.setName("Capsaicin_MeshBuffer");
            index_buffer_.setName("Capsaicin_IndexBuffer");
            vertex_buffer_.setName("Capsaicin_VertexBuffer");

            // NVIDIA-specific fix
            if (gfx_.getVendorId() == 0x10DEu) // NVIDIA
            {
                vertex_buffer_.setStride(4);
            }

            acceleration_structure_ = gfxCreateAccelerationStructure(gfx_);
            acceleration_structure_.setName("Capsaicin_AccelerationStructure");

            GfxInstance const *instances      = gfxSceneGetObjects<GfxInstance>(scene_);
            uint32_t const     instance_count = gfxSceneGetObjectCount<GfxInstance>(scene_);

            for (uint32_t i = 0; i < instance_count; ++i)
            {
                Instance instance = {};

                GfxConstRef<GfxMesh>     mesh_ref     = instances[i].mesh;
                GfxConstRef<GfxMaterial> material_ref = instances[i].material;

                uint32_t const instance_index = gfxSceneGetObjectHandle<GfxInstance>(scene_, i);

                instance.mesh_index      = (uint32_t)mesh_ref;
                instance.material_index  = (uint32_t)material_ref;
                instance.transform_index = instance_index;

                if (instance_index >= instance_data_.size())
                {
                    instance_data_.resize((size_t)instance_index + 1);

                    instance_min_bounds_.resize((size_t)instance_index + 1);
                    instance_max_bounds_.resize((size_t)instance_index + 1);
                }

                instance_data_[instance_index] = instance;

                if (instance.transform_index >= transform_data_.size())
                {
                    transform_data_.resize((size_t)instance.transform_index + 1);
                }

                transform_data_[instance.transform_index] = instances[i].transform;

                Mesh const &mesh = mesh_data_[(uint32_t)mesh_ref];

                uint32_t const index_count  = (uint32_t)mesh_ref->indices.size();
                uint32_t const vertex_count = (uint32_t)mesh_ref->vertices.size();

                if (instance_index >= raytracing_primitives_.size())
                {
                    raytracing_primitives_.resize((size_t)instance_index + 1);
                }

                GfxRaytracingPrimitive &rt_mesh = raytracing_primitives_[instance_index];

                rt_mesh = gfxCreateRaytracingPrimitive(gfx_, acceleration_structure_);

                GfxBuffer index_buffer =
                    gfxCreateBufferRange<uint32_t>(gfx_, index_buffer_, mesh.index_offset_idx, index_count);
                GfxBuffer vertex_buffer =
                    gfxCreateBufferRange<Vertex>(gfx_, vertex_buffer_, mesh.vertex_offset_idx, vertex_count);

                uint32_t non_opaque =
                    !material_ref
                            || (material_ref->albedo.w >= 1.0f
                                && (!material_ref->albedo_map
                                    || (material_ref->albedo_map->flags & kGfxImageFlag_HasAlphaChannel)
                                           == 0))
                        ? kGfxBuildRaytracingPrimitiveFlag_Opaque
                        : 0;

                gfxRaytracingPrimitiveBuild(gfx_, rt_mesh, index_buffer, vertex_buffer, 0, non_opaque);

                glm::mat4 const row_major_transform = glm::transpose(instances[i].transform);

                gfxRaytracingPrimitiveSetTransform(gfx_, rt_mesh, &row_major_transform[0][0]);
                gfxRaytracingPrimitiveSetInstanceID(gfx_, rt_mesh, instance_index);
                gfxRaytracingPrimitiveSetInstanceContributionToHitGroupIndex(
                    gfx_, rt_mesh, instance_index * sbt_stride_in_entries_[kGfxShaderGroupType_Hit]);

                gfxDestroyBuffer(gfx_, index_buffer);
                gfxDestroyBuffer(gfx_, vertex_buffer);
            }

            instance_buffer_ =
                gfxCreateBuffer<Instance>(gfx_, (uint32_t)instance_data_.size(), instance_data_.data());
            instance_buffer_.setName("Capsaicin_InstanceBuffer");

            transform_buffer_ =
                gfxCreateBuffer<glm::mat4x3>(gfx_, (uint32_t)transform_data_.size(), transform_data_.data());
            transform_buffer_.setName("Capsaicin_TransformBuffer");

            prev_transform_data_.resize(transform_data_.size());

            for (size_t i = 0; i < prev_transform_data_.size(); ++i)
            {
                prev_transform_data_[i] = transform_data_[i];
            }

            prev_transform_buffer_ = gfxCreateBuffer<glm::mat4x3>(
                gfx_, (uint32_t)prev_transform_data_.size(), prev_transform_data_.data());
            prev_transform_buffer_.setName("Capsaicin_PrevTransformBuffer");

            gfxAccelerationStructureUpdate(gfx_, acceleration_structure_);

            mesh_hash_ = mesh_hash;
        }

        GfxInstance const *instances      = gfxSceneGetObjects<GfxInstance>(scene_);
        uint32_t const     instance_count = gfxSceneGetObjectCount<GfxInstance>(scene_);

        // Check whether we need to re-build our transform data
        size_t transform_hash = transform_hash_;
        if (frame_index_ == 0 || animation) transform_hash = HashReduce(instances, instance_count);

        if (transform_hash != transform_hash_ || mesh_updated_)
        {
            transform_updated_ = true;
            transform_hash_    = transform_hash;

            // Update our transforms
            GfxBuffer transform_buffer =
                allocateConstantBuffer<glm::mat4x3>((uint32_t)transform_data_.size());
            glm::mat4x3 *transform_data = (glm::mat4x3 *)gfxBufferGetData(gfx_, transform_buffer);

            for (uint32_t i = 0; i < instance_count; ++i)
            {
                uint32_t const instance_index = gfxSceneGetObjectHandle<GfxInstance>(scene_, i);

                if (instance_index >= instance_data_.size())
                {
                    continue;
                }

                GFX_ASSERT(instance_index < instance_min_bounds_.size());
                GFX_ASSERT(instance_index < instance_max_bounds_.size());

                Instance const &instance = instance_data_[instance_index];

                transform_data[instance.transform_index]  = instances[i].transform;
                transform_data_[instance.transform_index] = instances[i].transform;

                if (instances[i].mesh)
                {
                    GfxMesh const &mesh = *instances[i].mesh;

                    CalculateTransformedBounds(mesh.bounds_min, mesh.bounds_max, instances[i].transform,
                        instance_min_bounds_[instance_index], instance_max_bounds_[instance_index]);
                }

                glm::mat4 const row_major_transform = glm::transpose(instances[i].transform);

                gfxRaytracingPrimitiveSetTransform(
                    gfx_, raytracing_primitives_[instance_index], &row_major_transform[0][0]);
            }

            // Update our acceleration structure
            {
                GfxCommandEvent const command_event(gfx_, "UpdateTLAS");

                gfxCommandCopyBuffer(gfx_, transform_buffer_, transform_buffer);
                gfxAccelerationStructureUpdate(gfx_, acceleration_structure_);
                gfxDestroyBuffer(gfx_, transform_buffer);
            }

            // Set up our instance indirection table
            instance_id_data_.resize(gfxSceneGetObjectCount<GfxInstance>(scene_));

            for (size_t i = 0; i < instance_id_data_.size(); ++i)
            {
                instance_id_data_[i] = gfxSceneGetObjectHandle<GfxInstance>(scene_, (uint32_t)i);
            }

            GfxBuffer instance_id_buffer =
                allocateConstantBuffer<uint32_t>((uint32_t)instance_id_data_.size());
            memcpy(gfxBufferGetData(gfx_, instance_id_buffer), instance_id_data_.data(),
                instance_id_data_.size() * sizeof(uint32_t));

            if (!instance_id_buffer_ || instance_id_buffer.getSize() != instance_id_buffer_.getSize())
            {
                gfxDestroyBuffer(gfx_, instance_id_buffer_);
                instance_id_buffer_ = gfxCreateBuffer<uint32_t>(gfx_, (uint32_t)instance_id_data_.size());
                instance_id_buffer_.setName("Capsaicin_InstanceIDBuffer");
            }

            // Update our instance table
            {
                GfxCommandEvent const command_event(gfx_, "UpdateInstanceTable");
                gfxCommandCopyBuffer(gfx_, instance_id_buffer_, instance_id_buffer);
                gfxDestroyBuffer(gfx_, instance_id_buffer);
            }
        }

        // Bind our global index and vertex data
        gfxCommandBindIndexBuffer(gfx_, index_buffer_);
        gfxCommandBindVertexBuffer(gfx_, vertex_buffer_);

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

        // We've completed a new frame
        ++frame_index_;

        // Reset remaining update flags
        environment_map_updated_ = false;
        scene_updated_           = false;
        camera_updated_          = false;
    }

    // Show debug visualizations if requested or blit kAOV_Color
    if (debug_view_.empty() || debug_view_ == "None")
    {
        const GfxCommandEvent command_event(gfx_, "Blit");
        gfxProgramSetParameter(gfx_, blit_program_, "ColorBuffer", getAOVBuffer("Color"));
        gfxCommandBindKernel(gfx_, blit_kernel_);
        gfxCommandDraw(gfx_, 3);
    }
    else
    {
        auto debugView = std::find_if(debug_views_.cbegin(), debug_views_.cend(),
            [this](auto val) { return debug_view_ == val.first; });
        if (debugView == debug_views_.cend())
        {
            GFX_PRINTLN("Error: Invalid debug view requested: %s", debug_view_.data());
            const GfxCommandEvent command_event(gfx_, "DrawInvalidDebugView");
            gfxCommandClearBackBuffer(gfx_);
        }
        else if (!debugView->second)
        {
            // Output AOV
            auto aov = getAOVBuffer(debugView->first);
            if (aov.getFormat() == DXGI_FORMAT_D32_FLOAT
                || (aov.getFormat() == DXGI_FORMAT_R32_FLOAT
                    && (strstr(aov.getName(), "Depth") != nullptr
                        || strstr(aov.getName(), "depth") != nullptr)))
            {
                const GfxCommandEvent command_event(gfx_, "DrawDepthDebugView");
                gfxProgramSetParameter(gfx_, debug_depth_program_, "DepthBuffer", getAOVBuffer("Depth"));
                gfxProgramSetParameter(gfx_, debug_depth_program_, "ViewProjectionInverse",
                    glm::mat4(glm::inverse(glm::dmat4(camera_matrices_[0].view_projection))));
                gfxCommandBindKernel(gfx_, debug_depth_kernel_);
                gfxCommandDraw(gfx_, 3);
            }
            else
            {
                // If tonemapping is enabled then we allow it to tonemap the AOV into the Debug buffer and
                // then output from there
                auto const format = aov.getFormat();
                if (hasOption<bool>("tonemap_enable") && getOption<bool>("tonemap_enable")
                    && (format == DXGI_FORMAT_R32G32B32A32_FLOAT || format == DXGI_FORMAT_R32G32B32_FLOAT
                        || format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_R11G11B10_FLOAT))
                {
                    aov = getAOVBuffer("Debug");
                }
                const GfxCommandEvent command_event(gfx_, "DrawAOVDebugView");
                gfxProgramSetParameter(gfx_, blit_program_, "ColorBuffer", aov);
                gfxCommandBindKernel(gfx_, blit_kernel_);
                gfxCommandDraw(gfx_, 3);
            }
        }
        else
        {
            // Output debug AOV
            const GfxCommandEvent command_event(gfx_, "DrawDebugView");
            gfxProgramSetParameter(gfx_, blit_program_, "ColorBuffer", getAOVBuffer("Debug"));
            gfxCommandBindKernel(gfx_, blit_kernel_);
            gfxCommandDraw(gfx_, 3);
        }
    }

    // Dump buffers
    while (dump_requests_.size() > 0)
    {
        auto const &[dump_file_path, dump_aov] = dump_requests_.front();
        if (hasAOVBuffer(dump_aov))
        {
            const GfxCommandEvent command_event(gfx_, "Dump AOV '%s'", dump_aov.c_str());
            dumpBuffer(dump_file_path.c_str(), getAOVBuffer(dump_aov));
        }
        dump_requests_.pop_front();
    }

    uint32_t dump_available_buffer_count = 0;
    for (auto &dump_in_flight_buffer : dump_in_flight_buffers_)
    {
        uint32_t dump_frame_index = std::get<4>(dump_in_flight_buffer);
        if (frame_index_ > dump_frame_index + kGfxConstant_BackBufferCount)
        {
            dump_available_buffer_count++;
        }
        else
        {
            // BE CAREFUL: dump_frame_index is monotonically increasing
            break;
        }
    }

    ThreadPool().Dispatch(
        [&](uint32_t buffer_index) {
            auto const &[dump_buffer, dump_buffer_width, dump_buffer_height, dump_file_path,
                dump_frame_index] = dump_in_flight_buffers_[buffer_index];
            saveImage(dump_buffer, dump_buffer_width, dump_buffer_height, dump_file_path.c_str());
        },
        dump_available_buffer_count, 1);

    for (uint32_t available_buffer_index = 0; available_buffer_index < dump_available_buffer_count;
         available_buffer_index++)
    {
        gfxDestroyBuffer(gfx_, std::get<0>(dump_in_flight_buffers_.front()));
        dump_in_flight_buffers_.pop_front();
    }

    // Dump cameras
    ThreadPool().Dispatch(
        [&](uint32_t camera_index) {
            auto const &[dump_file_path, dump_jittered] = dump_camera_requests_[camera_index];
            dumpCamera(dump_file_path.c_str(), camera_matrices_[dump_jittered],
                dump_jittered ? camera_jitter_x_ : 0.f, dump_jittered ? camera_jitter_y_ : 0.f);
        },
        (uint32_t)dump_camera_requests_.size(), 1);

    dump_camera_requests_.clear();
}

void CapsaicinInternal::renderGUI(bool readOnly)
{
    if (!ImGui::GetCurrentContext())
    {
        static bool warned;
        if (!warned)
            GFX_PRINT_ERROR(kGfxResult_InvalidOperation,
                "No ImGui context was supplied on initialization; cannot call `Capsaicin::RenderGUI()'");
        warned = true;
        return; // no ImGui context was supplied on initialization
    }

    ImGui::Text("Selected device :  %s", gfx_.getName());
    ImGui::Separator();
    const uint32_t deltaLightCount = getDeltaLightCount();
    const uint32_t areaLightCount  = getAreaLightCount();
    const uint32_t envLightCount   = getEnvironmentLightCount();
    const uint32_t triangleCount   = getTriangleCount();
    const uint64_t bvhDataSize     = getBvhDataSize();
    ImGui::Text("Triangle Count            :  %u", triangleCount);
    ImGui::Text("Light Count               :  %u", areaLightCount + deltaLightCount + envLightCount);
    ImGui::Text("  Area Light Count        :  %u", areaLightCount);
    ImGui::Text("  Delta Light Count       :  %u", deltaLightCount);
    ImGui::Text("  Environment Light Count :  %u", envLightCount);
    ImGui::Text("BVH Data Size             :  %.1f MiB", bvhDataSize / (1024.0 * 1024.0));
    ImGui::Text("Render Resolution         :  %ux%u", getWidth(), getHeight());

    if (!readOnly)
    {
        // Display renderer specific options
        if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
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

    if (ImGui::CollapsingHeader("Profiling", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool   children           = false;
        size_t maxStringSize      = 0;
        float  totalTimestampTime = 0.0f;
        auto   getTimestamps      = [&](Timeable *timeable) -> void {
            const uint32_t timestamp_query_count = timeable->getTimestampQueryCount();

            if (!timestamp_query_count)
            {
                return; // no profiling info available
            }

            bool hasChildren = timestamp_query_count > 1;
            children         = children || hasChildren;
            const ImGuiTreeNodeFlags flags =
                (hasChildren ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf);

            const auto &timestamp_queries = timeable->getTimestampQueries();
            auto        total_query_duration = gfxTimestampQueryGetDuration(gfx_, timestamp_queries[0].query);
            totalTimestampTime += total_query_duration;

            if (ImGui::TreeNodeEx(timeable->getName().data(), flags, "%-20s: %.3f ms",
                           timeable->getName().data(), total_query_duration))
            {
                maxStringSize = std::max(maxStringSize, timeable->getName().length());
                if (hasChildren)
                {
                    for (uint32_t i = 1; i < timestamp_query_count; ++i)
                    {
                        ImGui::TreeNodeEx(std::to_string(i).c_str(),
                                   ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%-17s: %.3f ms",
                                   timestamp_queries[i].name.data(),
                                   gfxTimestampQueryGetDuration(gfx_, timestamp_queries[i].query));
                    }
                }
                ImGui::TreePop();
            }
        };
        for (auto const &component : components_)
        {
            getTimestamps(&*component.second);
        }
        for (auto const &render_technique : render_techniques_)
        {
            getTimestamps(&*render_technique);
        }
        if (ImGui::TreeNodeEx(
                "Total", ImGuiTreeNodeFlags_Leaf, "%-20s: %.3f ms", "Total", totalTimestampTime))
        {
            ImGui::TreePop();
        }

        ImGui::Separator();

        const std::string graphName = std::format("{:.2f}", frame_time_ * 1000.0) + " ms ("
                                    + std::format("{:.2f}", 1.0f / frame_time_) + " fps)";

        ImGui::PushID("Total frame time");
        std::string text            = "Total frame time";
        size_t      additionalSpace = maxStringSize > text.size() ? maxStringSize - text.size() : 0;
        if (children)
        {
            text.insert(0, "   ");
        }
        for (size_t i = 0; i < additionalSpace + 1; ++i)
        {
            text.append(" ");
        }
        text.append(":");
        ImGui::Text(text.data());
        ImGui::SameLine();
        ImGui::PlotLines("", Graph::GetValueAtIndex, &frameGraph, frameGraph.getValueCount(), 0,
            graphName.c_str(), 0.0f, FLT_MAX, ImVec2(150, 20));
        ImGui::PopID();

        ImGui::PushID("Frame");
        text            = "Frame";
        additionalSpace = maxStringSize > text.size() ? maxStringSize - text.size() : 0;
        if (children)
        {
            text.insert(0, "   ");
        }
        for (size_t i = 0; i < additionalSpace + 1; ++i)
        {
            text.append(" ");
        }
        text.append(":");
        ImGui::Text(text.data());
        ImGui::SameLine();
        ImGui::Text(std::to_string(frame_index_).c_str());
        ImGui::PopID();
    }

    if (!readOnly)
    {
        if (ImGui::CollapsingHeader("Render Options", ImGuiTreeNodeFlags_None))
        {
            for (auto &i : options_)
            {
                if (std::holds_alternative<bool>(i.second))
                {
                    ImGui::Checkbox(i.first.data(), std::get_if<bool>(&(i.second)));
                }
                else if (std::holds_alternative<uint32_t>(i.second))
                {
                    uint32_t *option = std::get_if<uint32_t>(&(i.second));
                    ImGui::DragInt(i.first.data(), reinterpret_cast<int32_t *>(option), 1, 0);
                }
                else if (std::holds_alternative<int32_t>(i.second))
                {
                    ImGui::DragInt(i.first.data(), std::get_if<int32_t>(&(i.second)), 1);
                }
                else if (std::holds_alternative<float>(i.second))
                {
                    ImGui::DragFloat(i.first.data(), std::get_if<float>(&(i.second)), 5e-3f);
                }
            }
        }
    }
}

void CapsaicinInternal::terminate()
{
    gfxFinish(gfx_); // flush & sync

    // Dump remaining buffers, they are all available after gfxFinish
    ThreadPool().Dispatch(
        [&](uint32_t buffer_index) {
            auto &[dump_buffer, dump_buffer_width, dump_buffer_height, dump_file_path, dump_frame_index] =
                dump_in_flight_buffers_[buffer_index];
            saveImage(dump_buffer, dump_buffer_width, dump_buffer_height, dump_file_path.c_str());
        },
        (uint32_t)dump_in_flight_buffers_.size(), 1);

    while (dump_in_flight_buffers_.size() > 0)
    {
        gfxDestroyBuffer(gfx_, std::get<0>(dump_in_flight_buffers_.front()));
        dump_in_flight_buffers_.pop_front();
    }

    gfxDestroyKernel(gfx_, blit_kernel_);
    gfxDestroyProgram(gfx_, blit_program_);
    gfxDestroyKernel(gfx_, debug_depth_kernel_);
    gfxDestroyProgram(gfx_, debug_depth_program_);
    gfxDestroyProgram(gfx_, convolve_ibl_program_);
    gfxDestroyKernel(gfx_, dump_copy_to_buffer_kernel_);
    gfxDestroyProgram(gfx_, dump_copy_to_buffer_program_);

    gfxDestroyBuffer(gfx_, camera_matrices_buffer_[0]);
    gfxDestroyBuffer(gfx_, camera_matrices_buffer_[1]);
    gfxDestroyBuffer(gfx_, mesh_buffer_);
    gfxDestroyBuffer(gfx_, index_buffer_);
    gfxDestroyBuffer(gfx_, vertex_buffer_);
    gfxDestroyBuffer(gfx_, instance_buffer_);
    gfxDestroyBuffer(gfx_, material_buffer_);
    gfxDestroyBuffer(gfx_, transform_buffer_);
    gfxDestroyBuffer(gfx_, instance_id_buffer_);
    gfxDestroyBuffer(gfx_, prev_transform_buffer_);

    gfxDestroyTexture(gfx_, environment_buffer_);

    gfxDestroySamplerState(gfx_, linear_sampler_);
    gfxDestroySamplerState(gfx_, linear_wrap_sampler_);
    gfxDestroySamplerState(gfx_, nearest_sampler_);
    gfxDestroySamplerState(gfx_, anisotropic_sampler_);

    gfxDestroyAccelerationStructure(gfx_, acceleration_structure_);

    for (auto &i : aov_buffers_)
    {
        gfxDestroyTexture(gfx_, i.second);
    }
    aov_buffers_.clear();
    aov_backup_buffers_.clear();
    aov_clear_buffers_.clear();

    debug_views_.clear();

    for (auto &i : shared_buffers_)
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

    render_techniques_.clear();
    components_.clear();
    renderer_ = nullptr;

    gfxDestroyScene(scene_);
    scene_ = {};
}

void CapsaicinInternal::reloadShaders() noexcept
{
    // Instead of just recompiling kernels we re-initialise all component/techniques. This has the side effect
    // of not only recompiling kernels but also re-initialising old data that may no longer contain correct
    // values
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

void CapsaicinInternal::setupRenderTechniques(std::string_view const &name) noexcept
{
    // Clear any existing AOVs
    for (auto &i : aov_buffers_)
    {
        gfxCommandClearTexture(gfx_, i.second);
    }

    // Delete any existing render techniques
    render_techniques_.clear();

    gfxFinish(gfx_); // flush & sync

    // Delete old AOVS, debug views and buffers
    options_.clear();
    components_.clear();
    for (auto &i : shared_buffers_)
    {
        gfxDestroyBuffer(gfx_, i.second);
    }
    shared_buffers_.clear();
    for (auto &i : aov_buffers_)
    {
        gfxDestroyTexture(gfx_, i.second);
    }
    aov_buffers_.clear();
    aov_backup_buffers_.clear();
    aov_clear_buffers_.clear();
    debug_views_.clear();
    debug_views_.emplace_back("None", nullptr);
    debug_view_    = "None";
    renderer_name_ = "";
    renderer_      = nullptr;
    resetPlaybackState();

    // Create the new renderer
    renderer_ = RendererFactory::make(name);
    if (renderer_)
    {
        render_techniques_ = std::move(renderer_->setupRenderTechniques(options_));
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

        // Get requested components
        std::vector<std::string_view> requestedComponents;
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getComponents())
            {
                if (std::find(requestedComponents.cbegin(), requestedComponents.cend(), j)
                    == requestedComponents.cend())
                {
                    // Add the new component to requested list
                    requestedComponents.emplace_back(std::move(j));
                }
            }
        }

        // Create all requested components
        for (auto &i : requestedComponents)
        {
            // Create the new component
            auto component = ComponentFactory::make(i);
            if (component)
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
                        && std::find(newRequestedComponents.cbegin(), newRequestedComponents.cend(), j)
                               == newRequestedComponents.cend())
                    {
                        // Add the new component to requested list
                        newRequestedComponents.emplace_back(std::move(j));
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
                auto component = ComponentFactory::make(i);
                if (component)
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

        // Update render options based on passed in settings
        // Loop through and update to any values stored in passed in render settings
        for (auto &i : options_)
        {
            if (auto j = options_.find(i.first); j != options_.end())
            {
                if (j->second.index() == i.second.index())
                {
                    i.second = j->second;
                }
            }
            else
            {
                // Update user version with required changed options
                options_.emplace(i.first, i.second);
            }
        }
    }

    {
        // Get requested buffers
        struct BufferParams
        {
            size_t size;
        };

        using bufferList = std::unordered_map<std::string_view, BufferParams>;
        bufferList requestedBuffers;
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getBuffers())
            {
                if (auto const found = requestedBuffers.find(j.name); found == requestedBuffers.end())
                {
                    BufferParams newParams = BufferParams {j.size};
                    // Add the new buffer to requested list
                    requestedBuffers.try_emplace(j.name, std::move(newParams));
                }
            }
        }
        for (auto const &i : components_)
        {
            for (auto &j : i.second->getBuffers())
            {
                if (auto const found = requestedBuffers.find(j.name); found == requestedBuffers.end())
                {
                    BufferParams newParams = BufferParams {j.size};
                    // Add the new buffer to requested list
                    requestedBuffers.try_emplace(j.name, std::move(newParams));
                }
            }
        }

        // Create all requested Buffers
        for (auto &i : requestedBuffers)
        {
            // Create new texture
            GfxBuffer   buffer     = gfxCreateBuffer(gfx_, i.second.size);
            std::string bufferName = "Capsaicin_";
            bufferName += i.first;
            bufferName += "Buffer";
            buffer.setName(bufferName.c_str());
            shared_buffers_.emplace_back(i.first, buffer);
        }

        // Initialise the Buffers
        for (auto &i : shared_buffers_)
        {
            gfxCommandClearBuffer(gfx_, i.second);
        }
    }

    {
        // Get requested AOVs
        struct AOVParams
        {
            DXGI_FORMAT      format;
            AOV::Flags       flags;
            std::string_view backup = std::string_view();
        };

        // We use 3 main default AOVs that are always available
        using aovList             = std::unordered_map<std::string_view, AOVParams>;
        const aovList defaultAOVs = {
            {"Color", {DXGI_FORMAT_R16G16B16A16_FLOAT, AOV::Accumulate}},
        };
        const aovList defaultOptionalAOVs = {
            {"Depth",         {DXGI_FORMAT_D32_FLOAT, AOV::Clear}},
            {"Debug", {DXGI_FORMAT_R16G16B16A16_FLOAT, AOV::None}},
        };
        aovList                                                requestedAOVs = defaultAOVs;
        std::unordered_map<std::string_view, std::string_view> backupAOVs;
        aovList                                                optionalAOVs;
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getAOVs())
            {
                if (auto const found = requestedAOVs.find(j.name); found == requestedAOVs.end())
                {
                    // Check if backup AOV
                    if (auto pos = backupAOVs.find(j.backup_name); pos != backupAOVs.cend())
                    {
                        if (j.access != AOV::Read)
                        {
                            GFX_PRINTLN(
                                "Error: Cannot request write access to backup AOV: %s", j.name.data());
                        }
                        if (j.flags != AOV::None)
                        {
                            GFX_PRINTLN("Error: Cannot set flags on a backup AOV: %s", j.name.data());
                        }
                        if (j.format != DXGI_FORMAT_UNKNOWN)
                        {
                            GFX_PRINTLN("Error: Cannot set format on a backup AOV: %s", j.name.data());
                        }
                        if (!j.backup_name.empty())
                        {
                            GFX_PRINTLN("Error: Cannot create backup of a backup AOV: %s, %s", j.name.data(),
                                j.backup_name.data());
                        }
                        continue;
                    }
                    // Check if the AOV is being read despite never having been written to
                    if ((j.access == AOV::Read) && ((j.flags & AOV::Clear) == AOV::Clear))
                    {
                        GFX_PRINTLN("Error: Requested read access to AOV that has not been written to: %s",
                            j.name.data());
                    }
                    // Check if AOV is one of the optional default ones and add it using default values if not
                    // supplied
                    AOVParams newParams = AOVParams {j.format, j.flags, j.backup_name};
                    if (auto const k = defaultOptionalAOVs.find(j.name); k != defaultOptionalAOVs.end())
                    {
                        if (newParams.format == DXGI_FORMAT_UNKNOWN)
                        {
                            newParams.format = k->second.format;
                        }
                        if (newParams.flags == AOV::None)
                        {
                            newParams.flags = k->second.flags;
                        }
                    }
                    if (j.flags & AOV::Optional)
                    {
                        optionalAOVs.try_emplace(j.name, std::move(newParams));
                        if (!j.backup_name.empty())
                        {
                            GFX_PRINTLN("Error: Requested backup of optional AOV: %s", j.name.data());
                        }
                    }
                    else
                    {
                        // Add the new AOV to requested list
                        requestedAOVs.try_emplace(j.name, std::move(newParams));
                        // Check if also a backup AOV
                        if (!j.backup_name.empty())
                        {
                            if (auto pos = backupAOVs.find(j.backup_name);
                                pos != backupAOVs.end() && pos->second != j.name)
                            {
                                GFX_PRINTLN(
                                    "Error: Cannot create multiple different backups with same name: %s, %s",
                                    j.name.data(), j.backup_name.data());
                            }
                            else
                            {
                                backupAOVs.emplace(j.backup_name, j.name);
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
                        GFX_PRINTLN("Error: Requested AOV with different formats: %s", j.name.data());
                    }
                    if (((j.flags & AOV::Clear) && (found->second.flags & AOV::Accumulate))
                        || ((j.flags & AOV::Accumulate) && (found->second.flags & AOV::Clear)))
                    {
                        GFX_PRINTLN("Error: Requested AOV with different clear settings: %s", j.name.data());
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
                            GFX_PRINTLN("Error: Requested AOV with different backup names: %s, %2",
                                j.name.data(), j.backup_name.data());
                        }
                    }
                    // Add clear/accumulate flag if requested
                    if (j.flags & AOV::Clear)
                    {
                        found->second.flags = AOV::Flags(found->second.flags | AOV::Clear);
                    }
                    else if (j.flags & AOV::Accumulate)
                    {
                        found->second.flags = AOV::Flags(found->second.flags | AOV::Accumulate);
                    }
                }
            }
        }

        // Merge optional AOVS
        for (auto &i : optionalAOVs)
        {
            if (auto j = requestedAOVs.find(i.first); j != requestedAOVs.end())
            {
                // Update existing format if it doesn't have one
                if (j->second.format == DXGI_FORMAT_UNKNOWN)
                {
                    j->second.format = i.second.format;
                }
                // Validate that requested values match the existing ones
                else if (i.second.format != j->second.format && i.second.format != DXGI_FORMAT_UNKNOWN)
                {
                    GFX_PRINTLN("Error: Requested AOV with different formats: %s", i.first.data());
                }
                if (((i.second.flags & AOV::Clear) && (j->second.flags & AOV::Accumulate))
                    || ((i.second.flags & AOV::Accumulate) && (j->second.flags & AOV::Clear)))
                {
                    GFX_PRINTLN("Error: Requested AOV with different clear settings: %s", i.first.data());
                }

                // Add backup name if requested
                if (!i.second.backup.empty())
                {
                    if (j->second.backup.empty())
                    {
                        j->second.backup = i.second.backup;
                    }
                    else if (j->second.backup != i.second.backup)
                    {
                        GFX_PRINTLN("Error: Requested AOV with different backup names: %s, %2",
                            i.first.data(), j->second.backup.data());
                    }
                }
                // Add clear/accumulate flag if requested
                if (i.second.flags & AOV::Clear)
                {
                    j->second.flags = AOV::Flags(j->second.flags | AOV::Clear);
                }
                else if (i.second.flags & AOV::Accumulate)
                {
                    j->second.flags = AOV::Flags(j->second.flags | AOV::Accumulate);
                }
            }
        }

        // Create all requested AOVs
        for (auto &i : requestedAOVs)
        {
            // Create new texture
            GfxTexture  texture    = gfxCreateTexture2D(gfx_, i.second.format);
            std::string bufferName = "Capsaicin_";
            bufferName += i.first;
            bufferName += "AOV";
            texture.setName(bufferName.c_str());
            aov_buffers_.emplace_back(i.first, texture);

            // Add to backup list
            if (!i.second.backup.empty())
            {
                // Create new backup texture
                GfxTexture texture2 = gfxCreateTexture2D(gfx_, i.second.format);
                texture2.setName(i.second.backup.data());
                aov_backup_buffers_.emplace_back(std::make_pair(texture, texture2));

                aov_buffers_.emplace_back(i.second.backup, texture2);
            }

            // Add to clear list
            if (i.second.flags & AOV::Clear)
            {
                aov_clear_buffers_.emplace_back(texture);
            }

            // Add the AOV as a debug view (Using false to differentiate as AOV)
            if (i.first != "Color" && i.first != "Debug") debug_views_.emplace_back(i.first, false);
        }

        // Initialise the AOVs
        for (auto &i : aov_buffers_)
        {
            gfxCommandClearTexture(gfx_, i.second);
        }
    }

    {
        // Get debug views
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getDebugViews())
            {
                auto k = std::find_if(
                    debug_views_.begin(), debug_views_.end(), [j](auto val) { return val.first == j; });
                if (k == debug_views_.end())
                {
                    debug_views_.emplace_back(std::move(j), true);
                }
                else if (!k->second)
                {
                    // We allow render techniques to override the default AOV debug view if requested
                    k->second = true;
                }
                else
                {
                    GFX_PRINTLN("Error: Duplicate debug views detected: %s", j.data());
                }
            }
        }
    }

    // If no scene currently loaded then delay initialisation till scene load
    if (!!scene_)
    {
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

        // Reset flags as everything just got forced reset anyway
        mesh_updated_            = false;
        transform_updated_       = false;
        environment_map_updated_ = false;
        scene_updated_           = false;
        camera_updated_          = false;
    }
}

void CapsaicinInternal::resetPlaybackState() noexcept
{
    // Reset frame index
    frame_index_ = 0;
    // Reset frame time
    auto wallTime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
    current_time_ = static_cast<double>(wallTime.count()) / 1000000.0;
    frame_time_   = 0.0f;
}

void CapsaicinInternal::resetRenderState() noexcept
{
    // Reset frame index to signal component/techniques an initialisation is required/occurred
    frame_index_ = 0;

    // Reset the AOV history
    {
        GfxCommandEvent const command_event(gfx_, "ResetPreviousGBuffers");

        for (auto &i : aov_backup_buffers_)
        {
            gfxCommandClearTexture(gfx_, i.second);
        }
    }
}

} // namespace Capsaicin
