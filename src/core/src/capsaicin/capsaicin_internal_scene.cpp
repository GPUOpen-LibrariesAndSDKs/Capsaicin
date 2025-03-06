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
#include "hash_reduce.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <meshoptimizer.h>
#include <numbers>
#include <yaml-cpp/yaml.h>

namespace Capsaicin
{
std::vector<std::filesystem::path> const &CapsaicinInternal::getCurrentScenes() const noexcept
{
    return scene_files_;
}

bool CapsaicinInternal::setScene(std::filesystem::path const &fileName) noexcept
{
    if (scene_files_.size() == 1 && scene_files_.front() == fileName)
    {
        // Already loaded
        return true;
    }

    // Early check if supported file type (to avoid clearing scene unnecessarily)
    if (fileName.extension() != ".gltf" && fileName.extension() != ".glb" && fileName.extension() != ".obj"
        && fileName.extension() != ".yaml")
    {
        GFX_PRINT_ERROR(kGfxResult_InternalError, "Scene '%s' can't be loaded, unknown file format.",
            fileName.string().c_str());
        return false;
    }

    // Clear any pre-existing scene data
    bool const initRequired = !!scene_;
    if (initRequired)
    {
        // Reset internal state
        gfxFinish(gfx_); // flush & sync
        setDebugView("None");
        resetPlaybackState();
        setPaused(true);
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

    bool const loaded = loadSceneFile(fileName, false);

    // Re-initialise the components/techniques. Also handle delayed loading of renderer when a scene
    // previously hadn't been set.
    if (initRequired || !renderer_name_.empty())
    {
        // Reset flags as everything is about to get reset anyway
        resetEvents();
        scene_updated_ = true;

        // Initialise all components
        for (auto const &[name, component] : components_)
        {
            component->setGfxContext(gfx_);
            if (!component->init(*this))
            {
                GFX_PRINTLN("Error: Failed to initialise component: %s", name.data());
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

    return loaded;
}

bool CapsaicinInternal::appendScene(std::filesystem::path const &fileName) noexcept
{
    // Early check if supported file type (to avoid clearing scene unnecessarily)
    if (fileName.extension() != ".gltf" && fileName.extension() != ".glb" && fileName.extension() != ".obj"
        && fileName.extension() != ".yaml")
    {
        GFX_PRINT_ERROR(kGfxResult_InternalError, "Scene '%s' can't be loaded, unknown file format.",
            fileName.string().c_str());
        return false;
    }

    if (frame_index_ > 0)
    {
        // Reset internal state
        gfxFinish(gfx_); // flush & sync
        resetPlaybackState();
        resetRenderState();
    }

    if (!loadSceneFile(fileName, true))
    {
        return false;
    }

    // Update scene stats
    triangle_count_ = 0;
    for (uint32_t i = 0; i < gfxSceneGetObjectCount<GfxInstance>(scene_); ++i)
    {
        if (gfxSceneGetObjects<GfxInstance>(scene_)[i].mesh)
        {
            GfxMesh const &mesh = *gfxSceneGetObjects<GfxInstance>(scene_)[i].mesh;
            triangle_count_ += static_cast<uint32_t>(mesh.indices.size() / 3);
        }
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
    auto const *const ret =
        gfxSceneGetCameraMetadata(scene_, gfxSceneGetActiveCamera(scene_)).getObjectName();
    return ret;
}

CameraView CapsaicinInternal::getSceneCameraView() const noexcept
{
    auto const &camera = getCamera();
    return {camera.eye, normalize(camera.center - camera.eye), camera.up};
}

float CapsaicinInternal::getSceneCameraFOV() const noexcept
{
    return getCamera().fovY;
}

glm::vec2 CapsaicinInternal::getSceneCameraRange() const noexcept
{
    auto const &camera = getCamera();
    return {camera.nearZ, camera.farZ};
}

bool CapsaicinInternal::setSceneCamera(std::string_view const &name) noexcept
{
    // Convert camera name to an index
    auto const cameras     = getSceneCameras();
    auto const cameraIndex = std::ranges::find(cameras, name);
    if (cameraIndex == cameras.end())
    {
        GFX_PRINTLN("Error: Invalid camera requested: %s", name.data());
        return false;
    }
    auto const camera = gfxSceneGetCameraHandle(scene_, static_cast<uint32_t>(cameraIndex - cameras.begin()));
    camera->aspect =
        static_cast<float>(gfxGetBackBufferWidth(gfx_)) / static_cast<float>(gfxGetBackBufferHeight(gfx_));
    if (gfxSceneSetActiveCamera(scene_, camera) != kGfxResult_NoError)
    {
        return false;
    }
    camera_changed_ = true;
    camera_updated_ = true;
    resetRenderState();
    return true;
}

void CapsaicinInternal::setSceneCameraView(
    glm::vec3 const &position, glm::vec3 const &forward, glm::vec3 const &up) noexcept
{
    GfxCamera &camera = *gfxSceneGetActiveCamera(scene_);
    camera.eye        = position;
    camera.center     = position + forward;
    camera.up         = up;
}

void CapsaicinInternal::setSceneCameraFOV(float const FOVY) noexcept
{
    GfxRef const camera_ref = gfxSceneGetActiveCamera(scene_);
    camera_ref->fovY        = FOVY;
}

void CapsaicinInternal::setSceneCameraRange(glm::vec2 const &nearFar) noexcept
{
    GfxCamera &camera = *gfxSceneGetActiveCamera(scene_);
    camera.nearZ      = nearFar.x;
    camera.farZ       = nearFar.y;
}

std::filesystem::path CapsaicinInternal::getCurrentEnvironmentMap() const noexcept
{
    return environment_map_file_;
}

bool CapsaicinInternal::setEnvironmentMap(std::filesystem::path const &fileName) noexcept
{
    // Normalise file name and standardise path separators
    std::filesystem::path const normFileName = fileName.lexically_normal().generic_string();

    if (environment_map_file_ == normFileName)
    {
        // Already loaded
        return true;
    }

    // Check if supported file type
    if ((normFileName.extension() != ".hdr" && normFileName.extension() != ".exr") && !normFileName.empty())
    {
        GFX_PRINT_ERROR(kGfxResult_InternalError,
            "Environment Map '%s' can't be loaded, unknown file format.", normFileName.string().c_str());
        return false;
    }

    if (!generateEnvironmentMap(normFileName))
    {
        return false;
    }

    if (environment_map_updated_)
    {
        resetRenderState();
    }
    return true;
}

GfxTexture CapsaicinInternal::getEnvironmentBuffer() const
{
    return environment_buffer_;
}

GfxCamera const &CapsaicinInternal::getCamera() const
{
    // Get hold of the active camera (can be animated)
    GfxConstRef const camera_ref = gfxSceneGetActiveCamera(scene_);
    return *camera_ref;
}

bool CapsaicinInternal::loadSceneFile(std::filesystem::path const &fileName, bool const append) noexcept
{
    // Normalise file name and standardise path separators
    std::filesystem::path const normFileName = fileName.lexically_normal().generic_string();

    // Check if supported file type
    if (normFileName.extension() != ".gltf" && normFileName.extension() != ".glb"
        && normFileName.extension() != ".obj" && normFileName.extension() != ".yaml")
    {
        GFX_PRINT_ERROR(kGfxResult_InternalError, "Scene '%s' can't be loaded, unknown file format.",
            normFileName.string().c_str());
        return false;
    }

    if (!append)
    {
        if (scene_files_.size() == 1 && scene_files_.front() == normFileName)
        {
            // Already loaded
            return true;
        }

        if (!createBlankScene())
        {
            return false;
        }
    }

    // Load in scene from requested file
    bool const loaded =
        (normFileName.extension() == ".yaml") ? loadSceneYAML(normFileName) : loadSceneGLTF(normFileName);

    if (!loaded)
    {
        if (!createBlankScene())
        {
            return false;
        }
    }

    scene_updated_ = true;

    if (!append)
    {
        // Set up camera based on internal scene data
        uint32_t cameraIndex = 0;
        if (uint32_t const cameraCount = gfxSceneGetCameraCount(scene_); cameraCount > 1)
        {
            cameraIndex = 1; // Use first scene camera
            // Try and find 'Main' camera
            for (uint32_t i = 1; i < cameraCount; ++i)
            {
                auto        cameraHandle = gfxSceneGetCameraHandle(scene_, i);
                GfxMetadata metaData     = gfxSceneGetCameraMetadata(scene_, cameraHandle);
                std::string cameraName   = metaData.getObjectName();
                if (cameraName.starts_with("Camera") && cameraName.length() > 6)
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
            auto const defaultCamera = gfxSceneGetCameraHandle(scene_, cameraIndex);
            auto const userCamera    = gfxSceneGetCameraHandle(scene_, 0);
            userCamera->eye          = defaultCamera->eye;
            userCamera->center       = defaultCamera->center;
            userCamera->up           = defaultCamera->up;
        }
        auto const camera = gfxSceneGetCameraHandle(scene_, cameraIndex);
        camera->aspect    = static_cast<float>(gfxGetBackBufferWidth(gfx_))
                       / static_cast<float>(gfxGetBackBufferHeight(gfx_));
        if (gfxSceneSetActiveCamera(scene_, camera) != kGfxResult_NoError)
        {
            return false;
        }
    }

    return loaded;
}

bool CapsaicinInternal::loadSceneYAML(std::filesystem::path const &fileName) noexcept
{
    try
    {
        std::ifstream file(fileName);

        if (!file.is_open())
        {
            GFX_PRINT_ERROR(kGfxResult_InternalError, "Failed to open file '%s'", fileName.string().c_str());
            return false;
        }

        YAML::Node data            = YAML::Load(file);
        auto       parentDirectory = fileName.parent_path();

        bool loaded = false;
        if (auto sceneList = data["scene_paths"])
        {
            for (auto scene : sceneList)
            {
                if (auto scenePath = parentDirectory / scene.as<std::string>(); !loadSceneGLTF(scenePath))
                {
                    return false;
                }
                loaded = true;
            }
        }
        if (!loaded)
        {
            GFX_PRINT_ERROR(
                kGfxResult_InternalError, "Invalid YAML scene file '%s'", fileName.string().c_str());
            return false;
        }

        if (auto environmentMap = data["ibl_path"])
        {
            if (auto emString = environmentMap.as<std::string>(); emString == "Disabled")
            {
                setEnvironmentMap("");
            }
            else
            {
                if (auto emPath = parentDirectory / environmentMap.as<std::string>();
                    !setEnvironmentMap(emPath))
                {
                    setEnvironmentMap("");
                    return false;
                }
            }
        }

        if (hasOption<float>("auto_exposure_value"))
        {
            if (auto exposure = data["tonemap_exposure"])
            {
                setOption<float>("auto_exposure_value", exposure.as<float>());
            }
        }
        return true;
    }
    catch (std::exception const &e)
    {
        GFX_PRINTLN(e.what());
        return false;
    }
}

bool CapsaicinInternal::loadSceneGLTF(std::filesystem::path const &fileName) noexcept
{
    auto const fileNameString = fileName.string();
    if (gfxSceneImport(scene_, fileNameString.c_str()) != kGfxResult_NoError)
    {
        GFX_PRINT_ERROR(kGfxResult_InternalError, "Failed to import scene '%s'", fileNameString.c_str());
        gfxSceneClear(scene_);
        return false;
    }
    scene_files_.emplace_back(fileName);
    return true;
}

bool CapsaicinInternal::createBlankScene() noexcept
{
    if (!!scene_)
    {
        scene_updated_ = true;
        // Remove environment map as it's tied to scene
        setEnvironmentMap("");
        // Clear existing scene
        gfxDestroyScene(scene_);
        scene_       = {};
        scene_files_ = {};
    }

    // Create new blank scene
    scene_ = gfxCreateScene();
    if (!scene_)
    {
        return false;
    }

    // Create default user camera
    auto const userCamera = gfxSceneCreateCamera(scene_);
    userCamera->type      = kGfxCameraType_Perspective;
    userCamera->eye       = {0.0F, 0.0F, -1.0F};
    userCamera->center    = {0.0F, 0.0F, 0.0F};
    userCamera->up        = {0.0F, 1.0F, 0.0F};
    userCamera->aspect = static_cast<float>(render_dimensions_.x) / static_cast<float>(render_dimensions_.y);
    userCamera->fovY   = DegreesToRadians(90.0F);
    userCamera->nearZ  = 0.1F;
    userCamera->farZ   = 1e4F;
    GfxMetadata userCameraMeta;
    userCameraMeta.object_name = "User";
    gfxSceneSetCameraMetadata(scene_, gfxSceneGetCameraHandle(scene_, 0), userCameraMeta);

    return true;
}

bool CapsaicinInternal::generateEnvironmentMap(std::filesystem::path const &fileName) noexcept
{
    if (fileName.empty())
    {
        // If empty file requested then just use blank environment map
        environment_map_file_ = "";

        // Remove the old environment map
        if (!!environment_buffer_)
        {
            gfxDestroyTexture(gfx_, environment_buffer_);
            environment_buffer_      = {};
            environment_map_updated_ = true;
        }
        return true;
    }

    // Scale environment buffer to screen resolution
    uint32_t const maxWidth        = std::max(render_dimensions_.x, render_dimensions_.y);
    uint32_t const floorWidth      = std::bit_floor(maxWidth);
    uint32_t const ceilWidth       = std::bit_ceil(maxWidth);
    uint32_t       environmentSize = (maxWidth - floorWidth >= ceilWidth - maxWidth) ? ceilWidth : floorWidth;

    if (environment_map_file_ == fileName)
    {
        environmentSize = std::min((maxWidth == render_dimensions_.x ? environment_map_source_dimensions_.x
                                                                     : environment_map_source_dimensions_.y)
                                       / 2,
            environmentSize);

        // Need to check if we actually need to resize based on render dimensions
        if (environment_buffer_.getWidth() == environmentSize)
        {
            // Nothing needs doing
            return true;
        }
    }

    // Load in the environment map
    std::string const fileNameString = fileName.string();
    if (gfxSceneImport(scene_, fileNameString.c_str()) != kGfxResult_NoError)
    {
        return false;
    }
    auto const environmentMap = gfxSceneFindObjectByAssetFile<GfxImage>(scene_, fileNameString.c_str());
    if (!environmentMap)
    {
        GFX_PRINTLN("Failed to find valid environment map source file: %s", fileNameString.c_str());
        return false;
    }

    // Remove the old environment cube map
    if (!!environment_buffer_)
    {
        gfxDestroyTexture(gfx_, environment_buffer_);
        environment_buffer_ = {};
    }
    environment_map_updated_ = true;
    environment_map_file_    = fileName;

    // Get source dimensions
    uint32_t const environment_map_width  = environmentMap->width;
    uint32_t const environment_map_height = environmentMap->height;
    environment_map_source_dimensions_    = uint2(environment_map_width, environment_map_height);
    uint32_t const environment_map_mip_count =
        gfxCalculateMipCount(environment_map_width, environment_map_height);
    uint32_t const environment_map_channel_count     = environmentMap->channel_count;
    uint32_t const environment_map_bytes_per_channel = environmentMap->bytes_per_channel;

    // Scale environment buffer to screen resolution without exceeding the resolution of the source
    environmentSize =
        std::min((maxWidth == render_dimensions_.x ? environmentMap->width : environmentMap->height) / 2,
            environmentSize);

    // Create environment cube map texture
    uint32_t const environment_buffer_mips = gfxCalculateMipCount(environmentSize);
    environment_buffer_ =
        gfxCreateTextureCube(gfx_, environmentSize, DXGI_FORMAT_R16G16B16A16_FLOAT, environment_buffer_mips);
    environment_buffer_.setName("Capsaicin_EnvironmentBuffer");

    GfxTexture const environment_map = gfxCreateTexture2D(gfx_, environment_map_width, environment_map_height,
        environmentMap->format, environment_map_mip_count);
    {
        GfxBuffer const upload_buffer = gfxCreateBuffer(gfx_,
            static_cast<size_t>(environment_map_width) * environment_map_height
                * environment_map_channel_count * environment_map_bytes_per_channel,
            environmentMap->data.data(), kGfxCpuAccess_Write);
        gfxCommandCopyBufferToTexture(gfx_, environment_map, upload_buffer);
        gfxCommandGenerateMips(gfx_, environment_map);
        gfxDestroyBuffer(gfx_, upload_buffer);
    }

    constexpr std::array forward_vectors = {glm::dvec3(-1.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0), glm::dvec3(0.0, 0.0, -1.0),
        glm::dvec3(0.0, 0.0, 1.0)};

    constexpr std::array up_vectors = {glm::dvec3(0.0, -1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0),
        glm::dvec3(0.0, 0.0, -1.0), glm::dvec3(0.0, 0.0, 1.0), glm::dvec3(0.0, -1.0, 0.0),

        glm::dvec3(0.0, -1.0, 0.0)};
    {
        GfxCommandEvent const command_event(gfx_, "GenerateEnvironmentMap");
        GfxProgram const      convolve_ibl_program_ = createProgram("capsaicin/convolve_ibl");
        for (uint32_t cubemap_face = 0; cubemap_face < 6; ++cubemap_face)
        {
            GfxDrawState const draw_sky_state = {};
            gfxDrawStateSetColorTarget(draw_sky_state, 0, environment_buffer_.getFormat());

            GfxKernel const draw_sky_kernel =
                gfxCreateGraphicsKernel(gfx_, convolve_ibl_program_, draw_sky_state, "DrawSky");

            uint2 const buffer_dimensions = {environment_buffer_.getWidth(), environment_buffer_.getHeight()};

            glm::dmat4 const view =
                lookAt(glm::dvec3(0.0), forward_vectors[cubemap_face], up_vectors[cubemap_face]);
            glm::dmat4 const proj = glm::perspective(std::numbers::pi_v<double> / 2.0, 1.0, 0.1, 1e4);
            auto const       view_proj_inv = glm::mat4(inverse(proj * view));

            gfxProgramSetParameter(gfx_, convolve_ibl_program_, "g_BufferDimensions", buffer_dimensions);
            gfxProgramSetParameter(gfx_, convolve_ibl_program_, "g_ViewProjectionInverse", view_proj_inv);

            gfxProgramSetParameter(gfx_, convolve_ibl_program_, "g_EnvironmentMap", environment_map);

            gfxProgramSetParameter(gfx_, convolve_ibl_program_, "g_LinearSampler", linear_sampler_);

            gfxCommandBindColorTarget(gfx_, 0, environment_buffer_, 0, cubemap_face);
            gfxCommandBindKernel(gfx_, draw_sky_kernel);
            gfxCommandDraw(gfx_, 3);

            gfxDestroyKernel(gfx_, draw_sky_kernel);
        }

        GfxKernel const blur_sky_kernel = gfxCreateComputeKernel(gfx_, convolve_ibl_program_, "BlurSky");

        for (uint32_t mip_level = 1; mip_level < environment_buffer_mips; ++mip_level)
        {
            gfxProgramSetParameter(
                gfx_, convolve_ibl_program_, "g_InEnvironmentBuffer", environment_buffer_, mip_level - 1);
            gfxProgramSetParameter(
                gfx_, convolve_ibl_program_, "g_OutEnvironmentBuffer", environment_buffer_, mip_level);

            uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, blur_sky_kernel);
            uint32_t const  num_groups_x =
                (GFX_MAX(environmentSize >> mip_level, 1U) + num_threads[0] - 1) / num_threads[0];
            uint32_t const num_groups_y =
                (GFX_MAX(environmentSize >> mip_level, 1U) + num_threads[1] - 1) / num_threads[1];
            constexpr uint32_t num_groups_z = 6; // blur all faces

            gfxCommandBindKernel(gfx_, blur_sky_kernel);
            gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, num_groups_z);
        }
        gfxDestroyKernel(gfx_, blur_sky_kernel);
        gfxDestroyProgram(gfx_, convolve_ibl_program_);
    }

    auto const handle = gfxSceneGetImageHandle(scene_, environmentMap.getIndex());
    gfxSceneDestroyImage(scene_, handle);
    gfxDestroyTexture(gfx_, environment_map);
    return true;
}

void CapsaicinInternal::updateScene() noexcept
{
    // Run the animations
    if (!play_paused_ || (play_time_ != play_time_old_))
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

        play_time_old_ = play_time_;
        updateSceneAnimations();
    }

    // Calculate the camera matrices for this frame
    updateSceneCameraMatrices();

    // Resize environment map if needed
    if (render_dimensions_updated_ && !environment_map_updated_)
    {
        generateEnvironmentMap(environment_map_file_);
        if (environment_map_updated_)
        {
            resetRenderState();
        }
    }

    // Update the scene history
    {
        prev_vertex_data_index_ = vertex_data_index_;
        if (animation_updated_)
        {
            vertex_data_index_ = vertex_data_index_ ^ 1;

            if (frame_index_ == 0)
            {
                // Fall back to the current frame's skin cache for the first frame.
                prev_vertex_data_index_ = vertex_data_index_;
            }
        }
    }

    // Update vertex and index buffers
    updateSceneMeshes();

    // Update instance buffer
    updateSceneInstances();

    // Update transform buffer
    updateSceneTransforms();

    // Update materials and textures
    updateSceneMaterials();

    // Run any skinning or morph based vertex animation
    bool const animationGPUUpdated = updateSceneAnimatedGeometry();

    // Update Ray Tracing acceleration structure
    updateSceneBVH(animationGPUUpdated);
}

void CapsaicinInternal::updateSceneAnimations() noexcept
{
    uint32_t const animation_count = gfxSceneGetAnimationCount(scene_);
    for (uint32_t animation_index = 0; animation_index < animation_count; ++animation_index)
    {
        GfxConstRef const animation_ref    = gfxSceneGetAnimationHandle(scene_, animation_index);
        float const       animation_length = gfxSceneGetAnimationLength(scene_, animation_ref);
        auto time_in_seconds = static_cast<float>(fmod(play_time_, static_cast<double>(animation_length)));
        // Handle negative playback times
        time_in_seconds = (time_in_seconds >= 0.0F) ? time_in_seconds : animation_length + time_in_seconds;
        gfxSceneApplyAnimation(scene_, animation_ref, time_in_seconds);
    }
    animation_updated_ = animation_count > 0;
}

void CapsaicinInternal::updateSceneCameraMatrices() noexcept
{
    uint32_t const jitter_index = jitter_frame_index_ != ~0U ? jitter_frame_index_ : frame_index_;
    if (render_dimensions_updated_)
    {
        auto const currentWindow = uint2(gfxGetBackBufferWidth(gfx_), gfxGetBackBufferHeight(gfx_));
        gfxSceneGetActiveCamera(scene_)->aspect =
            static_cast<float>(currentWindow.x) / static_cast<float>(currentWindow.y);
    }
    auto const      &camera = getCamera();
    glm::dmat4 const view = lookAt(glm::dvec3(camera.eye), glm::dvec3(camera.center), glm::dvec3(camera.up));
    glm::dmat4       projection =
        glm::perspective(static_cast<double>(camera.fovY), static_cast<double>(camera.aspect),
            static_cast<double>(camera.farZ), static_cast<double>(camera.nearZ));
    if (camera.eye != camera_prev_.eye || camera.center != camera_prev_.center
        || camera.up != camera_prev_.up)
    {
        camera_updated_ = true;
        // Detect relatively large changes such as jump cuts and flag them as a camera change
        constexpr float changeTolerance = 0.9F; // Maximum allowed overlap before considered a camera change
        auto const      lookAt          = normalize(camera.center - camera.eye);
        auto const      oldLookAt       = normalize(camera_prev_.center - camera_prev_.eye);
        // Detect large changes in view direction greater than current view frustum FOV
        if (acosf(dot(lookAt, oldLookAt)) >= camera.fovY * camera.aspect * changeTolerance)
        {
            camera_changed_ = true;
        }
        else
        {
            // Use Separating Axis Theorem to check if the new view frustum does not overlap the old one
            // Get the frustum plane normals
            auto const viewProjection     = glm::mat4x3(transpose(glm::mat4(projection) * glm::mat4(view)));
            auto const viewProjectionPrev = glm::mat4x3(transpose(camera_matrices_[0].view_projection_prev));
            std::array const axes         = {normalize(viewProjection[3] + viewProjection[0]), // left
                        normalize(viewProjection[3] - viewProjection[0]),                      // right
                        normalize(viewProjection[3] + viewProjection[1]),                      // bottom
                        normalize(viewProjection[3] - viewProjection[1]),                      // top
                        normalize(viewProjection[3] + viewProjection[2]),                      // near
                        normalize(viewProjectionPrev[3] + viewProjectionPrev[0]),
                        normalize(viewProjectionPrev[3] - viewProjectionPrev[0]),
                        normalize(viewProjectionPrev[3] + viewProjectionPrev[1]),
                        normalize(viewProjectionPrev[3] - viewProjectionPrev[1]),
                        normalize(viewProjectionPrev[3] + viewProjectionPrev[2])};
            // Get the points at each corner of the frustum
            auto getCorners = [](GfxCamera const  &cameraIn,
                                  glm::vec3 const &direction) -> std::array<glm::vec3, 8> {
                float const  height     = tanf(cameraIn.fovY * 0.5F);
                float const  nearHeight = height * cameraIn.nearZ;
                float const  farHeight  = height * cameraIn.farZ;
                float const  nearWidth  = nearHeight * cameraIn.aspect * 0.5F;
                float const  farWidth   = farHeight * cameraIn.aspect * 0.5F;
                float3 const right      = cross(direction, cameraIn.up);

                float3 const fc       = cameraIn.eye + (direction * cameraIn.farZ);
                float3 const upFar    = cameraIn.up * farHeight;
                float3 const rightFar = right * farWidth;
                float3 const ftl      = fc + upFar - rightFar;
                float3 const ftr      = fc + upFar + rightFar;
                float3 const fbl      = fc - upFar - rightFar;
                float3 const fbr      = fc - upFar + rightFar;

                float3 const nc        = cameraIn.eye + (direction * cameraIn.nearZ);
                float3 const upNear    = cameraIn.up * nearHeight;
                float3 const rightNear = right * nearWidth;
                float3 const ntl       = nc + upNear - rightNear;
                float3 const ntr       = nc + upNear + rightNear;
                float3 const nbl       = nc - upNear - rightNear;
                float3 const nbr       = nc - upNear + rightNear;
                return {ftl, ftr, fbl, fbr, ntl, ntr, nbl, nbr};
            };
            std::array const frustumCorners     = getCorners(camera, lookAt);
            std::array const frustumCornersPrev = getCorners(camera_prev_, oldLookAt);
            float            foundOverlap       = std::numeric_limits<float>::max();
            for (auto const &axis : axes)
            {
                // Project each frustum corner onto the current axis
                auto project = [](std::array<glm::vec3, 8> const &corners,
                                   glm::vec3 const               &axisIn) -> float2 {
                    auto ret = float2(std::numeric_limits<float>::max(), std::numeric_limits<float>::min());
                    for (auto const &corner : corners)
                    {
                        auto const p = dot(axisIn, corner);
                        ret.x        = glm::min(p, ret.x);
                        ret.y        = glm::max(p, ret.y);
                    }
                    return ret;
                };
                auto const projection1 = project(frustumCorners, axis);
                auto const projection2 = project(frustumCornersPrev, axis);
                // Check if the projections overlap
                if (projection2.y < projection1.x || projection1.y < projection2.x)
                {
                    // The frustums do not overlap
                    camera_changed_ = true;
                    break;
                }
                else
                {
                    // Get the amount of overlap
                    auto const largestMin  = glm::max(projection1.x, projection2.x);
                    auto const smallestMax = glm::min(projection1.y, projection2.y);
                    auto       overlap     = smallestMax - largestMin;
                    // Check if one frustum projection is entirely contained in the second
                    if ((projection2.x > projection1.x && projection2.y < projection1.y)
                        || (projection1.x > projection2.x && projection1.y < projection2.y))
                    {
                        // Get the overlap and the distance from the minimum frustum corner
                        auto const diff = glm::abs(projection1 - projection2);
                        overlap += glm::min(diff.x, diff.y);
                    }
                    foundOverlap = glm::min(overlap, foundOverlap);
                }
            }
            // Get middle size of frustum
            // Check if overlap is so small we should consider camera to have completely moved
            if (float const midHeight = tanf(camera.fovY * 0.5F) * (camera.farZ - camera.nearZ);
                foundOverlap < (midHeight * (1.0F - changeTolerance)))
            {
                camera_changed_ = true;
            }
        }
    }
    if (frame_index_ == 0 || render_dimensions_updated_)
    {
        // Reset previous camera
        camera_prev_ = camera;
    }
    camera_jitter_ = float2(CalculateHaltonNumber((jitter_index % jitter_phase_count_) + 1, 2),
        CalculateHaltonNumber((jitter_index % jitter_phase_count_) + 1, 3));
    camera_jitter_ =
        ((camera_jitter_ - 0.5F) * float2(2.0F, -2.0F)) / static_cast<float2>(render_dimensions_);
    for (uint32_t i = 0; i < 2; ++i)
    {
        if (i > 0)
        {
            // Recalculate previous camera matrices using the current jitter. This causes cancellation
            // of jitter between current and previous matrices.
            // The view and projection matrices are recalculated to prevent precision differences
            // between double precision and the stored single precision values.
            glm::dmat4 const view_prev = lookAt(
                glm::dvec3(camera_prev_.eye), glm::dvec3(camera_prev_.center), glm::dvec3(camera_prev_.up));
            glm::dmat4 projection_prev              = glm::perspective(static_cast<double>(camera_prev_.fovY),
                             static_cast<double>(camera_prev_.aspect), static_cast<double>(camera_prev_.farZ),
                             static_cast<double>(camera_prev_.nearZ));
            projection_prev[2][0]                   = static_cast<double>(camera_jitter_.x);
            projection_prev[2][1]                   = static_cast<double>(camera_jitter_.y);
            glm::dmat4 const view_projection_prev   = projection_prev * view_prev;
            camera_matrices_[i].view_projection     = glm::mat4(view_projection_prev);
            camera_matrices_[i].inv_view_projection = inverse(view_projection_prev);
        }
        camera_matrices_[i].view_prev                = camera_matrices_[i].view;
        camera_matrices_[i].projection_prev          = camera_matrices_[i].projection;
        camera_matrices_[i].view_projection_prev     = camera_matrices_[i].view_projection;
        camera_matrices_[i].inv_view_projection_prev = camera_matrices_[i].inv_view_projection;
        camera_matrices_[i].view                     = glm::mat4(view);
        if (i > 0)
        {
            projection[2][0] = static_cast<double>(camera_jitter_.x);
            projection[2][1] = static_cast<double>(camera_jitter_.y);
        }
        camera_matrices_[i].projection          = glm::mat4(projection);
        glm::dmat4 const view_projection        = projection * view;
        glm::dmat4 const inv_view_projection    = inverse(view_projection);
        camera_matrices_[i].view_projection     = glm::mat4(view_projection);
        camera_matrices_[i].inv_view_projection = glm::mat4(inv_view_projection);
        camera_matrices_[i].inv_projection      = glm::mat4(inverse(projection));
        camera_matrices_[i].inv_view            = glm::mat4(inverse(view));
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
    camera_prev_ = camera;
}

void CapsaicinInternal::updateSceneMeshes() noexcept
{
    // Check whether we need to re-build our mesh data
    auto const mesh_hash = mesh_hash_;
    if (frame_index_ == 0 || animation_updated_)
    {
        mesh_hash_ = HashReduce(gfxSceneGetObjects<GfxMesh>(scene_), gfxSceneGetObjectCount<GfxMesh>(scene_));
    }
    mesh_updated_ = mesh_hash != mesh_hash_;

    // Currently the transform data check is the same as checking for instance change
    instances_updated_ = false;

    // Check for a change in optional meshlet buffers
    if ((hasSharedBuffer("Meshlets") && getSharedBuffer("Meshlets").getSize() == 0)
        || (hasSharedBuffer("MeshletCull") && getSharedBuffer("MeshletCull").getSize() == 0))
    {
        // We must rebuild meshlet data
        mesh_updated_ = true;
    }

    // Check for change in render options
    auto const old_options = render_options;
    render_options         = convertOptions(getOptions());
    if ((old_options.capsaicin_lod_mode != render_options.capsaicin_lod_mode
            && (old_options.capsaicin_lod_mode != 0 || render_options.capsaicin_lod_offset != 0
                || render_options.capsaicin_lod_mode != 1)
            && (render_options.capsaicin_lod_mode != 0 || old_options.capsaicin_lod_offset != 0
                || old_options.capsaicin_lod_mode != 1))
        || (render_options.capsaicin_lod_mode > 0
            && (old_options.capsaicin_lod_offset != render_options.capsaicin_lod_offset
                || old_options.capsaicin_lod_aggressive != render_options.capsaicin_lod_aggressive)))
    {
        mesh_updated_      = true;
        instances_updated_ = true;
    }

    if (mesh_updated_)
    {
        // Reload and build the required buffers (vertex/index etc.) specific for each mesh
        GfxCommandEvent const command_event(gfx_, "BuildMeshes");

        GfxMesh const *meshes     = gfxSceneGetObjects<GfxMesh>(scene_);
        uint32_t const mesh_count = gfxSceneGetObjectCount<GfxMesh>(scene_);

        bool hasMeshlets    = hasSharedBuffer("Meshlets");
        bool hasMeshletCull = hasSharedBuffer("MeshletCull");
        GFX_ASSERTMSG(hasMeshlets == hasSharedBuffer("MeshletPack") && (!hasMeshletCull || hasMeshlets),
            "Cannot have Meshlets without also having MeshletPack shared buffer");

        mesh_infos_.clear();
        mesh_infos_.reserve(mesh_count);

        std::vector<Meshlet> meshlet_data; /**< The buffer storing meshlets. */
        std::vector<uint32_t>
            meshlet_pack_data; /**< The buffer storing packed meshlet vertex/index offsets. */
        std::vector<MeshletCull> meshlet_cull_data; /**< The buffer storing per meshlet culling data. */
        std::vector<uint32_t>    index_data;
        std::vector<Vertex>      vertex_data;
        std::vector<Vertex>      vertex_source_data;
        std::vector<Joint>       joint_data;

        // Prepare mesh data for loading to GPU. Perform copy for indices and skinning data when
        // needed; copy vertex data to vertex buffer for static meshes or to vertex source buffer for
        // animated ones.
        for (uint32_t i = 0; i < mesh_count; ++i)
        {
            auto const generateLOD = [&](uint32_t const offsetLOD, std::vector<GfxVertex> const &vertexBuffer,
                                         std::vector<uint32_t> const &indexBuffer,
                                         std::vector<uint32_t>       &indexBufferOut,
                                         size_t                       indexBufferOffset = 0) {
                size_t const vertexCount = vertexBuffer.size();
                size_t       indexCount  = indexBuffer.size();

                // Generate LOD. Note: mesh optimizer creates LODs by removing indices from the
                // index buffer and doesn't attempt to move vertices
                float const threshold = std::powf(0.5F, static_cast<float>(offsetLOD));
                auto const  targetIndexCount =
                    static_cast<size_t>(fmax(static_cast<float>(indexCount) * threshold, 6.0F));
                constexpr float    baseTargetError = 0.1F;
                float              targetError     = baseTargetError * static_cast<float>(offsetLOD);
                constexpr uint32_t options         = meshopt_SimplifyLockBorder;

                float lodError = 0.0F;
                indexBufferOut.resize(indexBufferOffset + indexBuffer.size());
                indexCount = meshopt_simplify(indexBufferOut.data() + indexBufferOffset, indexBuffer.data(),
                    indexCount, &vertexBuffer[0].position.x, vertexCount, sizeof(GfxVertex), targetIndexCount,
                    targetError, options, &lodError);

                uint32_t retries = 1;
                while (indexCount == 0 && retries <= offsetLOD)
                {
                    // Simplify has gone way overboard, try and back off until it works
                    targetError = baseTargetError * static_cast<float>(offsetLOD - retries);
                    indexCount  = meshopt_simplify(indexBufferOut.data() + indexBufferOffset,
                         indexBuffer.data(), indexBuffer.size(), &vertexBuffer[0].position.x, vertexCount,
                         sizeof(GfxVertex), targetIndexCount, targetError, options, &lodError);
                    ++retries;
                }
                indexBufferOut.resize(indexBufferOffset + indexCount);

                if (render_options.capsaicin_lod_aggressive && indexCount > 100
                    && static_cast<float>(indexCount) / static_cast<float>(targetIndexCount) > 2.0F)
                {
                    // If simplify doest reduce by as many indices as we want then fall back to a
                    // less accurate but cruder simplification technique
                    auto indexCount2 = meshopt_simplifySloppy(indexBufferOut.data() + indexBufferOffset,
                        indexBuffer.data(), indexCount, &vertexBuffer[0].position.x, vertexCount,
                        sizeof(GfxVertex), targetIndexCount, targetError, &lodError);

                    retries = 1;
                    while (indexCount2 == 0 && retries <= offsetLOD)
                    {
                        // Sloppy simplification can at time completely remove all indices in this
                        // case we back off until we get a value that works much like the back off
                        // for regular simplify
                        targetError = baseTargetError * static_cast<float>(offsetLOD - retries);
                        indexCount2 = meshopt_simplifySloppy(indexBufferOut.data() + indexBufferOffset,
                            indexBuffer.data(), indexCount, &vertexBuffer[0].position.x, vertexCount,
                            sizeof(GfxVertex), targetIndexCount, targetError, &lodError);
                        ++retries;
                    }
                    if (indexCount2 != 0)
                    {
                        // We only use the output of sloppy simplification if it is actually valid.
                        // If the fall-back still couldn't find anything then we ignore the output
                        // of sloppy entirely
                        indexBufferOut.resize(indexBufferOffset + indexCount2);
                        indexCount = indexCount2;
                    }
                }
                // mesh optimizer outputs the LOD error as a relative metric, to convert it to an absolute
                // value as it needs to be scaled
                lodError *=
                    meshopt_simplifyScale(&vertexBuffer[0].position.x, vertexCount, sizeof(GfxVertex));
                return std::make_tuple(indexBufferOffset, indexCount, lodError);
            };
            auto const loadMesh = [&](std::vector<GfxVertex> const &meshVertices,
                                      std::vector<uint32_t> const  &meshIndices,
                                      std::vector<GfxVertex> const &morphVertices,
                                      std::vector<GfxJoint> const  &joints) {
                // Get mesh values
                uint32_t const mesh_index     = gfxSceneGetObjectHandle<GfxMesh>(scene_, i);
                auto const     indexCount     = meshIndices.size();
                MeshInfo       mesh           = {};
                mesh.index_offset_idx         = static_cast<uint32_t>(index_data.size());
                mesh.index_count              = static_cast<uint32_t>(indexCount);
                mesh.vertex_source_offset_idx = static_cast<uint32_t>(vertex_source_data.size());
                mesh.joints_offset            = static_cast<uint32_t>(joint_data.size());
                mesh.targets_count = static_cast<uint32_t>(morphVertices.size() / meshVertices.size());
                mesh.vertex_count  = static_cast<uint32_t>(meshVertices.size());
                mesh.is_animated   = !joints.empty() || !morphVertices.empty();

                // Add mesh vertices. If the mesh has skinning/morphs then it is added to a secondary vertex
                // list used specifically for animation.
                if (!mesh.is_animated)
                {
                    mesh.vertex_offset_idx[0] = static_cast<uint32_t>(vertex_data.size());
                    mesh.vertex_offset_idx[1] = mesh.vertex_offset_idx[0];
                    vertex_data.reserve(vertex_data.size() + mesh.vertex_count);
                    for (auto const &[vertPosition, vertNormal, vertUV] : meshVertices)
                    {
                        Vertex vertex       = {};
                        vertex.position_uvx = float4(vertPosition, vertUV.x);
                        vertex.normal_uvy   = float4(vertNormal, vertUV.y);
                        vertex_data.push_back(vertex);
                    }
                }
                else
                {
                    // For every animated instance, allocate two slots
                    // for animated vertex data generated from vertex source data.
                    vertex_data.reserve(vertex_data.size() + 2ULL * mesh.vertex_count);
                    mesh.vertex_offset_idx[0] = static_cast<uint32_t>(vertex_data.size());
                    vertex_data.resize(vertex_data.size() + mesh.vertex_count);
                    mesh.vertex_offset_idx[1] = static_cast<uint32_t>(vertex_data.size());
                    vertex_data.resize(vertex_data.size() + mesh.vertex_count);

                    vertex_source_data.reserve(
                        vertex_source_data.size()
                        + (static_cast<size_t>(mesh.vertex_count) * mesh.targets_count));
                    for (size_t j = 0; j < mesh.vertex_count; ++j)
                    {
                        Vertex vertex       = {};
                        vertex.position_uvx = float4(meshVertices[j].position, meshVertices[j].uv.x);
                        vertex.normal_uvy   = float4(meshVertices[j].normal, meshVertices[j].uv.y);
                        vertex_source_data.push_back(vertex);
                        for (uint32_t k = 0; k < mesh.targets_count; ++k)
                        {
                            Vertex target_vertex = {};
                            target_vertex.position_uvx =
                                float4(morphVertices[j * mesh.targets_count + k].position,
                                    morphVertices[j * mesh.targets_count + k].uv.x);
                            target_vertex.normal_uvy =
                                float4(morphVertices[j * mesh.targets_count + k].normal,
                                    morphVertices[j * mesh.targets_count + k].uv.y);
                            vertex_source_data.push_back(target_vertex);
                        }
                    }
                }

                if (mesh_index >= mesh_infos_.size())
                {
                    mesh_infos_.resize(static_cast<size_t>(mesh_index) + 1);
                }

                if (hasMeshlets)
                {
                    // Create meshlets
                    {
                        constexpr size_t max_vertices  = 64;
                        constexpr size_t max_triangles = 64;
                        constexpr float  cone_weight   = 1.0F;

                        // Build meshlets
                        size_t const                 indexCountLOD = meshIndices.size();
                        std::vector<meshopt_Meshlet> meshlets(
                            meshopt_buildMeshletsBound(indexCountLOD, max_vertices, max_triangles));
                        std::vector<uint32_t> meshletVertices(meshlets.size() * max_vertices);
                        std::vector<uint8_t>  meshletTriangles(meshlets.size() * max_triangles * 3);
                        meshlets.resize(meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(),
                            meshletTriangles.data(), meshIndices.data(), indexCountLOD,
                            &meshVertices[0].position.x, mesh.vertex_count, sizeof(GfxVertex), max_vertices,
                            max_triangles, cone_weight));

                        // Collapse used memory from worst case usage
                        meshopt_Meshlet const &lastMeshlet = meshlets.back();
                        meshletVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
                        meshletTriangles.resize(
                            lastMeshlet.triangle_offset + ((lastMeshlet.triangle_count * 3 + 3) & ~3U));

                        // Optimise meshlet layout
                        for (auto &[vertexOffset, triangleOffset, vertexCount, triangleCount] : meshlets)
                        {
                            meshopt_optimizeMeshlet(&meshletVertices[vertexOffset],
                                &meshletTriangles[triangleOffset], triangleCount, vertexCount);
                        }

                        mesh.meshlet_count      = static_cast<uint32_t>(meshlets.size());
                        mesh.meshlet_offset_idx = static_cast<uint32_t>(meshlet_data.size());

                        std::vector<uint32_t> indices;
                        for (auto &[meshlet_vertex_offset, meshlet_triangle_offset, meshlet_vertex_count,
                                 meshlet_triangle_count] : meshlets)
                        {
                            // Add packed meshlet data. Each meshlet contains limited number of
                            // vertices/triangles, so we store them using a packed lower bit representation.
                            // These packed vertex indices act as offsets to the base mesh which is itself
                            // stored as a vertex offset in the global vertex buffer
                            // (instance.vertex_offset_idx)
                            auto const dataOffset = static_cast<uint32_t>(meshlet_pack_data.size());
                            for (uint32_t j = 0; j < meshlet_vertex_count; ++j)
                            {
                                meshlet_pack_data.push_back(
                                    meshletVertices[static_cast<size_t>(meshlet_vertex_offset) + j]);
                            }

                            // Meshlet indices are also stored packed in lower bit representation. These are
                            // used to order the meshlet vertices into triangles.
                            auto const indexMeshletOffset = static_cast<uint32_t>(indices.size());
                            for (size_t j = 0; j < meshlet_triangle_count; ++j)
                            {
                                // Indices are packed into same data buffer as vertices. Since they are only 8
                                // bit we can pack them into a 32bit uint inorder to avoid issues with reading
                                // buffers in HLSL
                                size_t const offset = static_cast<size_t>(meshlet_triangle_offset) + (j * 3);
                                meshlet_pack_data.push_back(
                                    static_cast<uint32_t>(meshletTriangles[offset])
                                    | (static_cast<uint32_t>(meshletTriangles[offset + 1]) << 10)
                                    | (static_cast<uint32_t>(meshletTriangles[offset + 2]) << 20));

                                // Remap index buffer to meshlet indices so that primitiveIDs match
                                indices.push_back(
                                    meshletVertices[meshletTriangles[offset]
                                                    + static_cast<size_t>(meshlet_vertex_offset)]);
                                indices.push_back(
                                    meshletVertices[meshletTriangles[offset + 1]
                                                    + static_cast<size_t>(meshlet_vertex_offset)]);
                                indices.push_back(
                                    meshletVertices[meshletTriangles[offset + 2]
                                                    + static_cast<size_t>(meshlet_vertex_offset)]);
                            }

                            // Add the new meshlet
                            Meshlet m              = {};
                            m.vertex_count         = static_cast<uint16_t>(meshlet_vertex_count);
                            m.triangle_count       = static_cast<uint16_t>(meshlet_triangle_count);
                            m.data_offset_idx      = dataOffset;
                            m.mesh_prim_offset_idx = indexMeshletOffset / 3;
                            meshlet_data.push_back(m);

                            if (hasMeshletCull)
                            {
                                meshopt_Bounds const bounds =
                                    meshopt_computeMeshletBounds(&meshletVertices[meshlet_vertex_offset],
                                        &meshletTriangles[meshlet_triangle_offset], meshlet_triangle_count,
                                        &meshVertices[0].position.x, mesh.vertex_count, sizeof(GfxVertex));

                                MeshletCull m2 = {};
                                m2.sphere      = float4(
                                    bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius);
                                m2.cone = float4(bounds.cone_axis[0], bounds.cone_axis[1],
                                    bounds.cone_axis[2], bounds.cone_cutoff);
                                meshlet_cull_data.push_back(m2);
                            }
                        }
                        index_data.insert(index_data.end(), indices.begin(), indices.end());
                    }
                }
                else
                {
                    // Must add indices in normally
                    for (auto const &index : meshIndices)
                    {
                        index_data.push_back(index);
                    }
                }
                mesh_infos_[mesh_index] = mesh;

                for (auto const &[jointJoints, jointWeights] : joints)
                {
                    joint_data.emplace_back(jointJoints, jointWeights);
                }
            };

            // Check current LOD mode and load meshes accordingly
            if (render_options.capsaicin_lod_mode == 0)
            {
                // Default mode just loads meshes unaltered
                loadMesh(meshes[i].vertices, meshes[i].indices, meshes[i].morph_targets, meshes[i].joints);
            }
            else if (render_options.capsaicin_lod_mode >= 1)
            {
                if (constexpr uint32_t minIndicesCap = 20;
                    render_options.capsaicin_lod_mode == 2
                    || (render_options.capsaicin_lod_offset != 0 && meshes[i].indices.size() > minIndicesCap))
                {
                    // Reindex index buffer to remove duplicated vertices
                    size_t const indexCount           = meshes[i].indices.size();
                    size_t const unindexedVertexCount = meshes[i].vertices.size();
                    size_t const morphCount = meshes[i].morph_targets.size() / meshes[i].vertices.size();
                    std::vector<meshopt_Stream> streams;
                    streams.reserve(1 + morphCount);
                    streams.emplace_back(meshes[i].vertices.data(), sizeof(GfxVertex), sizeof(GfxVertex));
                    if (!meshes[i].joints.empty())
                    {
                        streams.emplace_back(meshes[i].joints.data(), sizeof(GfxJoint), sizeof(GfxJoint));
                    }
                    for (size_t j = 0; j < morphCount; ++j)
                    {
                        streams.emplace_back(meshes[i].morph_targets.data() + (j * meshes[i].vertices.size()),
                            sizeof(GfxVertex), sizeof(GfxVertex));
                    }
                    std::vector<uint32_t> remap(indexCount);
                    size_t                vertexCount =
                        meshopt_generateVertexRemapMulti(remap.data(), meshes[i].indices.data(), indexCount,
                            unindexedVertexCount, streams.data(), streams.size());
                    std::vector<uint32_t> indexBuffer(indexCount);
                    meshopt_remapIndexBuffer(
                        indexBuffer.data(), meshes[i].indices.data(), indexCount, remap.data());
                    std::vector<GfxVertex> vertexBuffer(vertexCount);
                    meshopt_remapVertexBuffer(vertexBuffer.data(), meshes[i].vertices.data(),
                        unindexedVertexCount, sizeof(GfxVertex), remap.data());
                    std::vector<GfxVertex> morphVertices(morphCount * vertexCount);
                    for (size_t morph = 0; morph < morphCount; ++morph)
                    {
                        meshopt_remapVertexBuffer(morphVertices.data() + (morph * vertexCount),
                            meshes[i].morph_targets.data() + (morph * unindexedVertexCount),
                            unindexedVertexCount, sizeof(GfxVertex), remap.data());
                    }

                    // Get mesh LOD data
                    if (render_options.capsaicin_lod_mode == 1)
                    {
                        // If using Manual mode we can just generate a single LOD for the requested LOD level
                        generateLOD(
                            render_options.capsaicin_lod_offset, vertexBuffer, indexBuffer, indexBuffer);

                        // Compact the vertex buffer by removing unused vertices
                        auto const vertexCountOriginal = vertexCount;
                        vertexCount = meshopt_optimizeVertexFetch(vertexBuffer.data(), indexBuffer.data(),
                            indexBuffer.size(), vertexBuffer.data(), vertexCount, sizeof(GfxVertex));
                        vertexBuffer.resize(vertexCount);
                        for (size_t morph = 0; morph < morphCount; ++morph)
                        {
                            meshopt_optimizeVertexFetch(morphVertices.data() + (morph * vertexCount),
                                indexBuffer.data(), indexBuffer.size(),
                                morphVertices.data() + (morph * vertexCountOriginal), vertexCountOriginal,
                                sizeof(GfxVertex));
                        }
                        morphVertices.resize(morphCount * vertexCount);
                    }

                    loadMesh(vertexBuffer, indexBuffer, morphVertices, meshes[i].joints);
                }
                else
                {
                    // Just use default
                    loadMesh(
                        meshes[i].vertices, meshes[i].indices, meshes[i].morph_targets, meshes[i].joints);
                }
            }
        }

        // Add any skinning hierarchies
        uint32_t const skin_count         = gfxSceneGetObjectCount<GfxSkin>(scene_);
        uint32_t       joint_matrix_count = 0;
        joint_matrices_offsets_.clear();
        for (uint32_t i = 0; i < skin_count; ++i)
        {
            joint_matrices_offsets_.push_back(joint_matrix_count);
            GfxConstRef const skin_ref = gfxSceneGetObjectHandle<GfxSkin>(scene_, i);
            joint_matrix_count += static_cast<uint32_t>(skin_ref->joint_matrices.size());
        }

        // Copy data into GPU buffers
        gfxDestroyBuffer(gfx_, index_buffer_);
        index_buffer_ =
            gfxCreateBuffer<uint32_t>(gfx_, static_cast<uint32_t>(index_data.size()), index_data.data());
        index_buffer_.setName("IndexBuffer");
        gfxDestroyBuffer(gfx_, vertex_buffer_);
        vertex_buffer_ =
            gfxCreateBuffer<Vertex>(gfx_, static_cast<uint32_t>(vertex_data.size()), vertex_data.data());
        vertex_buffer_.setName("VertexBuffer");
        gfxDestroyBuffer(gfx_, vertex_source_buffer_);
        vertex_source_buffer_ = gfxCreateBuffer<Vertex>(
            gfx_, static_cast<uint32_t>(vertex_source_data.size()), vertex_source_data.data());
        vertex_source_buffer_.setName("VertexSourceBuffer");
        gfxDestroyBuffer(gfx_, joint_buffer_);
        joint_buffer_ =
            gfxCreateBuffer<Joint>(gfx_, static_cast<uint32_t>(joint_data.size()), joint_data.data());
        joint_buffer_.setName("JointBuffer");
        gfxDestroyBuffer(gfx_, joint_matrices_buffer_);
        joint_matrices_buffer_ = gfxCreateBuffer<glm::mat4>(gfx_, joint_matrix_count);
        if (hasMeshlets)
        {
            // Resizing must be exact as otherwise the copy buffer command will fail
            checkSharedBuffer("Meshlets", meshlet_data.size() * sizeof(Meshlet), true);
            GfxBuffer upload_buffer = gfxCreateBuffer<Meshlet>(
                gfx_, static_cast<uint32_t>(meshlet_data.size()), meshlet_data.data(), kGfxCpuAccess_Write);
            gfxCommandCopyBuffer(gfx_, getSharedBuffer("Meshlets"), upload_buffer);
            gfxDestroyBuffer(gfx_, upload_buffer);

            checkSharedBuffer("MeshletPack", meshlet_pack_data.size() * sizeof(uint32_t), true);
            upload_buffer = gfxCreateBuffer<uint32_t>(gfx_, static_cast<uint32_t>(meshlet_pack_data.size()),
                meshlet_pack_data.data(), kGfxCpuAccess_Write);
            gfxCommandCopyBuffer(gfx_, getSharedBuffer("MeshletPack"), upload_buffer);
            gfxDestroyBuffer(gfx_, upload_buffer);

            if (hasMeshletCull)
            {
                checkSharedBuffer("MeshletCull", meshlet_cull_data.size() * sizeof(MeshletCull), true);
                upload_buffer =
                    gfxCreateBuffer<MeshletCull>(gfx_, static_cast<uint32_t>(meshlet_cull_data.size()),
                        meshlet_cull_data.data(), kGfxCpuAccess_Write);
                gfxCommandCopyBuffer(gfx_, getSharedBuffer("MeshletCull"), upload_buffer);
                gfxDestroyBuffer(gfx_, upload_buffer);
            }
        }

        // NVIDIA-specific fix
        if (gfx_.getVendorId() == 0x10DEU) // NVIDIA
        {
            vertex_buffer_.setStride(4);
        }
    }
}

void CapsaicinInternal::updateSceneInstances() noexcept
{
    // Update the instance information
    if (instances_updated_ || mesh_updated_)
    {
        GfxCommandEvent const command_event(gfx_, "BuildInstances");

        bool const         hasMeshlets    = hasSharedBuffer("Meshlets");
        GfxInstance const *instances      = gfxSceneGetObjects<GfxInstance>(scene_);
        uint32_t const     instance_count = gfxSceneGetObjectCount<GfxInstance>(scene_);

        // Populate instance-related data.
        triangle_count_           = 0;
        size_t morph_weight_count = 0;
        instance_data_.clear();
        instance_source_info_data_.clear();
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            Instance           instance    = {};
            InstanceSourceInfo source_info = {};

            GfxConstRef<GfxMesh> const     mesh_ref     = instances[i].mesh;
            GfxConstRef<GfxMaterial> const material_ref = instances[i].material;
            MeshInfo const                &mesh_info    = mesh_infos_[static_cast<uint32_t>(mesh_ref)];

            uint32_t const instance_index = gfxSceneGetObjectHandle<GfxInstance>(scene_, i);

            instance.material_index   = static_cast<uint32_t>(material_ref);
            instance.transform_index  = instance_index;
            instance.index_offset_idx = mesh_info.index_offset_idx;
            instance.index_count      = mesh_info.index_count;
            if (hasMeshlets)
            {
                instance.meshlet_count      = mesh_info.meshlet_count;
                instance.meshlet_offset_idx = mesh_info.meshlet_offset_idx;
            }

            // Update scene statistics
            triangle_count_ += instance.index_count / 3;

            instance.vertex_offset_idx[0] = mesh_info.vertex_offset_idx[0];
            instance.vertex_offset_idx[1] = mesh_info.vertex_offset_idx[1];
            if (mesh_info.is_animated)
            {
                source_info.vertex_source_offset_idx = mesh_info.vertex_source_offset_idx;
                source_info.joints_offset            = mesh_info.joints_offset;
                source_info.weights_offset           = static_cast<uint32_t>(morph_weight_count);
                source_info.targets_count            = mesh_info.targets_count;

                morph_weight_count += instances[i].weights.size();
            }

            if (instance_index >= instance_data_.size())
            {
                instance_data_.resize(instance_index + 1);
                instance_bounds_.resize(instance_index + 1);
            }
            instance_data_[instance_index] = instance;
            instance_source_info_data_.push_back(source_info);
        }

        // Update GPU instance buffer
        gfxDestroyBuffer(gfx_, instance_buffer_);
        instance_buffer_ = gfxCreateBuffer<Instance>(
            gfx_, static_cast<uint32_t>(instance_data_.size()), instance_data_.data());
        instance_buffer_.setName("InstanceBuffer");

        // Set up our instance indirection table
        instance_id_data_.resize(gfxSceneGetObjectCount<GfxInstance>(scene_));

        for (size_t i = 0; i < instance_id_data_.size(); ++i)
        {
            instance_id_data_[i] = gfxSceneGetObjectHandle<GfxInstance>(scene_, static_cast<uint32_t>(i));
        }

        if (!instance_id_buffer_ || instance_id_data_.size() != instance_id_buffer_.getSize())
        {
            gfxDestroyBuffer(gfx_, instance_id_buffer_);
            instance_id_buffer_ =
                gfxCreateBuffer<uint32_t>(gfx_, static_cast<uint32_t>(instance_id_data_.size()));
            instance_id_buffer_.setName("InstanceIDBuffer");
        }
        else
        {
            // Update our instance ID table
            GfxBuffer const instance_id_buffer =
                allocateConstantBuffer<uint32_t>(static_cast<uint32_t>(instance_id_data_.size()));
            memcpy(gfxBufferGetData(gfx_, instance_id_buffer), instance_id_data_.data(),
                instance_id_data_.size() * sizeof(uint32_t));
            gfxCommandCopyBuffer(gfx_, instance_id_buffer_, instance_id_buffer);
            gfxDestroyBuffer(gfx_, instance_id_buffer);
        }

        // Update the morph weight buffer (as morphs are applied per instance)
        if (morph_weight_buffer_.getCount() != static_cast<uint32_t>(morph_weight_count))
        {
            gfxDestroyBuffer(gfx_, morph_weight_buffer_);
            morph_weight_buffer_ = gfxCreateBuffer<float>(gfx_, static_cast<uint32_t>(morph_weight_count));
            morph_weight_buffer_.setName("MorphWeightBuffer");
        }
    }
}

void CapsaicinInternal::updateSceneTransforms() noexcept
{
    GfxInstance const *instances      = gfxSceneGetObjects<GfxInstance>(scene_);
    uint32_t const     instance_count = gfxSceneGetObjectCount<GfxInstance>(scene_);
    // Check whether we need to re-build our transform data
    size_t const transform_hash = transform_hash_;
    if (frame_index_ == 0 || animation_updated_)
    {
        transform_hash_ = HashReduce(instances, instance_count);
    }
    transform_updated_ = transform_hash != transform_hash_;

    // Update per-instance transform data
    if (transform_updated_ || mesh_updated_)
    {
        GfxCommandEvent const command_event(gfx_, "BuildTransforms");

        // Update our transforms
        std::vector<glm::mat4x3> transform_data;
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            uint32_t const instance_index = gfxSceneGetObjectHandle<GfxInstance>(scene_, i);

            if (instance_index >= instance_data_.size())
            {
                continue;
            }

            GFX_ASSERT(instance_index < instance_bounds_.size());

            Instance const &instance = instance_data_[instance_index];

            if (instance.transform_index >= transform_data.size())
            {
                transform_data.resize(instance.transform_index + 1);
            }
            transform_data[instance.transform_index] = instances[i].transform;

            if (instances[i].mesh)
            {
                GfxMesh const &mesh = *instances[i].mesh;

                auto &instanceBounds = instance_bounds_[instance_index];
                CalculateTransformedBounds(mesh.bounds_min, mesh.bounds_max, instances[i].transform,
                    instanceBounds.first, instanceBounds.second);
            }
        }

        if (prev_transform_buffer_.getCount() == transform_data.size())
        {
            // Backup previous transforms
            gfxCommandCopyBuffer(gfx_, prev_transform_buffer_, transform_buffer_);
        }

        // Update the transform buffer
        gfxDestroyBuffer(gfx_, transform_buffer_);
        transform_buffer_ = gfxCreateBuffer<glm::mat4x3>(
            gfx_, static_cast<uint32_t>(transform_data.size()), transform_data.data());
        transform_buffer_.setName("TransformBuffer");
        if (prev_transform_buffer_.getCount() != static_cast<uint32_t>(transform_data.size()))
        {
            // Previous transform buffer should match current due to rebuild
            gfxDestroyBuffer(gfx_, prev_transform_buffer_);
            prev_transform_buffer_ = gfxCreateBuffer<glm::mat4x3>(gfx_, transform_buffer_.getCount());
            prev_transform_buffer_.setName("PrevTransformBuffer");
            gfxCommandCopyBuffer(gfx_, prev_transform_buffer_, transform_buffer_);
        }
        transform_updated_last_frame = true;
    }
    else if (transform_updated_last_frame)
    {
        GfxCommandEvent const command_event(gfx_, "UpdatePreviousTransforms");
        gfxCommandCopyBuffer(gfx_, prev_transform_buffer_, transform_buffer_);
        transform_updated_last_frame = false;
    }
}

void CapsaicinInternal::updateSceneMaterials() noexcept
{
    if (mesh_updated_) // Currently we don't support changing materials without also changing meshes
    {
        size_t const material_hash = material_hash_;
        material_hash_ =
            HashReduce(gfxSceneGetObjects<GfxMaterial>(scene_), gfxSceneGetObjectCount<GfxMaterial>(scene_));
        materials_updated_ = material_hash != material_hash_;

        if (materials_updated_)
        {
            // Rebuild materials buffer
            gfxDestroyBuffer(gfx_, material_buffer_);

            GfxMaterial const    *materials      = gfxSceneGetObjects<GfxMaterial>(scene_);
            uint32_t const        material_count = gfxSceneGetObjectCount<GfxMaterial>(scene_);
            std::vector<Material> material_data;
            material_data.reserve(material_count);

            for (uint32_t i = 0; i < material_count; ++i)
            {
                bool const     noAlpha  = materials[i].albedo.w >= 1.0F && !materials[i].albedo_map;
                Material const material = {.albedo = float4(float3(materials[i].albedo),
                                               glm::uintBitsToFloat(materials[i].albedo_map)),
                    .emissivity =
                        float4(materials[i].emissivity, glm::uintBitsToFloat(materials[i].emissivity_map)),
                    .metallicity_roughness =
                        float4(materials[i].metallicity, glm::uintBitsToFloat(materials[i].metallicity_map),
                            materials[i].roughness, glm::uintBitsToFloat(materials[i].roughness_map)),
                    .normal_alpha_side = float4(glm::uintBitsToFloat(materials[i].normal_map),
                        materials[i].albedo.w,
                        glm::uintBitsToFloat(
                            static_cast<uint32_t>((materials[i].flags & kGfxMaterialFlag_DoubleSided) != 0)),
                        glm::uintBitsToFloat(
                            materials[i].alpha_mode == GfxMaterialAlphaMode_Blend && !noAlpha  ? 2
                            : materials[i].alpha_mode == GfxMaterialAlphaMode_Mask && !noAlpha ? 1
                                                                                               : 0))};

                uint32_t const material_index = gfxSceneGetObjectHandle<GfxMaterial>(scene_, i);

                if (material_index >= material_data.size())
                {
                    material_data.resize(static_cast<size_t>(material_index) + 1);
                }

                material_data[material_index] = material;
            }

            material_buffer_ = gfxCreateBuffer<Material>(
                gfx_, static_cast<uint32_t>(material_data.size()), material_data.data());
            material_buffer_.setName("Capsaicin_MaterialBuffer");

            // Rebuild texture atlas for all used materials
            for (GfxTexture const &texture : texture_atlas_)
            {
                gfxDestroyTexture(gfx_, texture);
            }
            texture_atlas_.clear();

            uint32_t const image_count = gfxSceneGetObjectCount<GfxImage>(scene_);

            for (uint32_t i = 0; i < image_count; ++i)
            {
                GfxConstRef const image_ref = gfxSceneGetObjectHandle<GfxImage>(scene_, i);

                uint32_t const image_index = image_ref;

                if (image_index >= texture_atlas_.size())
                {
                    texture_atlas_.resize(static_cast<size_t>(image_index) + 1);
                }

                GfxTexture &texture = texture_atlas_[image_index];

                const DXGI_FORMAT format         = image_ref->format;
                uint32_t const    image_width    = image_ref->width;
                uint32_t const    image_height   = image_ref->height;
                uint32_t const    image_mips     = gfxCalculateMipCount(image_width, image_height);
                uint32_t const    image_channels = image_ref->channel_count;

                texture = gfxCreateTexture2D(gfx_, image_width, image_height, format, image_mips);
                texture.setName(gfxSceneGetObjectMetadata<GfxImage>(scene_, image_ref).getObjectName());

                if ((image_ref->width == 0) || (image_ref->height == 0))
                {
                    gfxCommandClearTexture(gfx_, texture);
                }
                else
                {
                    uint8_t const *image_data = image_ref->data.data();

                    uint64_t const uncompressed_size = static_cast<uint64_t>(image_width) * image_height
                                                     * image_channels * image_ref->bytes_per_channel;
                    uint64_t texture_size =
                        !gfxImageIsFormatCompressed(*image_ref) ? uncompressed_size : image_ref->data.size();
                    bool const mips = (image_ref->flags & kGfxImageFlag_HasMipLevels) != 0;
                    if (mips && !gfxImageIsFormatCompressed(*image_ref))
                    {
                        texture_size += texture_size / 3;
                    }
                    texture_size = GFX_MIN(texture_size, image_ref->data.size());
                    GfxBuffer const texture_data =
                        gfxCreateBuffer(gfx_, texture_size, image_data, kGfxCpuAccess_Write);

                    gfxCommandCopyBufferToTexture(gfx_, texture, texture_data);
                    if (!mips && !gfxImageIsFormatCompressed(*image_ref))
                    {
                        gfxCommandGenerateMips(gfx_, texture);
                    }
                    gfxDestroyBuffer(gfx_, texture_data);
                }
            }
        }
    }
}

bool CapsaicinInternal::updateSceneAnimatedGeometry() noexcept
{
    bool ret = false;
    if ((joint_matrices_buffer_.getCount() > 0 || morph_weight_buffer_.getCount() > 0)
        && (frame_index_ == 0 || animation_updated_ || mesh_updated_ || instances_updated_))
    {
        GfxInstance const *instances      = gfxSceneGetObjects<GfxInstance>(scene_);
        uint32_t const     instance_count = gfxSceneGetObjectCount<GfxInstance>(scene_);

        // Update skinning joint matrices
        uint32_t const         skin_count = gfxSceneGetObjectCount<GfxSkin>(scene_);
        std::vector<glm::mat4> joint_matrices_data(joint_matrices_buffer_.getCount());
        for (uint32_t i = 0; i < skin_count; ++i)
        {
            GfxConstRef const skin_ref = gfxSceneGetObjectHandle<GfxSkin>(scene_, i);
            memcpy(joint_matrices_data.data() + joint_matrices_offsets_[i], skin_ref->joint_matrices.data(),
                skin_ref->joint_matrices.size() * sizeof(glm::mat4));
        }

        if (!joint_matrices_data.empty())
        {
            // Load joint matrices to GPU buffer
            GfxCommandEvent const command_event(gfx_, "UpdateJointMatrices");
            gfxDestroyBuffer(gfx_, joint_matrices_buffer_);
            joint_matrices_buffer_ = gfxCreateBuffer<glm::mat4>(
                gfx_, static_cast<uint32_t>(joint_matrices_data.size()), joint_matrices_data.data());
        }

        // Update morph weights
        std::vector<float> morph_weight_data(morph_weight_buffer_.getCount());
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            memcpy(morph_weight_data.data() + instance_source_info_data_[i].weights_offset,
                instances[i].weights.data(), instance_source_info_data_[i].targets_count * sizeof(float));
        }

        if (!morph_weight_data.empty())
        {
            // Load morph weights to GPU buffer
            GfxCommandEvent const command_event(gfx_, "UpdateMorphWeights");
            gfxDestroyBuffer(gfx_, morph_weight_buffer_);
            morph_weight_buffer_ = gfxCreateBuffer<float>(
                gfx_, static_cast<uint32_t>(morph_weight_data.size()), morph_weight_data.data());
        }

        mesh_updated_ = true;
        {
            // Updated GPU vertex buffer with new vertex positions after animation.
            GfxCommandEvent const command_event(gfx_, "GenerateAnimatedVertices");

            for (uint32_t i = 0; i < instance_count; ++i)
            {
                uint32_t const instance_index = gfxSceneGetObjectHandle<GfxInstance>(scene_, i);
                if (instance_index >= instance_data_.size())
                {
                    continue;
                }

                Instance const            &instance         = instance_data_[instance_index];
                InstanceSourceInfo const  &source_info      = instance_source_info_data_[i];
                GfxConstRef<GfxMesh> const mesh_ref         = instances[i].mesh;
                MeshInfo const            &mesh_info        = mesh_infos_[static_cast<uint32_t>(mesh_ref)];
                GfxConstRef<GfxSkin> const skin_ref         = instances[i].skin;
                glm::mat4 const &inverse_instance_transform = inverse(glm::mat4(instances[i].transform));

                if (instance.vertex_offset_idx[0] == instance.vertex_offset_idx[1])
                {
                    continue;
                }

                ret                         = true;
                uint32_t const vertex_count = mesh_info.vertex_count;

                // Bind the shader parameters
                gfxProgramSetParameter(
                    gfx_, generate_animated_vertices_program_, "g_VertexCount", vertex_count);
                gfxProgramSetParameter(gfx_, generate_animated_vertices_program_, "g_VertexOffset",
                    instance.vertex_offset_idx[vertex_data_index_]);
                gfxProgramSetParameter(gfx_, generate_animated_vertices_program_, "g_VertexSourceOffset",
                    source_info.vertex_source_offset_idx);
                gfxProgramSetParameter(
                    gfx_, generate_animated_vertices_program_, "g_JointOffset", source_info.joints_offset);
                gfxProgramSetParameter(
                    gfx_, generate_animated_vertices_program_, "g_WeightsOffset", source_info.weights_offset);
                gfxProgramSetParameter(
                    gfx_, generate_animated_vertices_program_, "g_TargetsCount", source_info.targets_count);
                gfxProgramSetParameter(gfx_, generate_animated_vertices_program_, "g_JointMatrixOffset",
                    skin_ref ? joint_matrices_offsets_[skin_ref.getIndex()] : ~0U);
                gfxProgramSetParameter(gfx_, generate_animated_vertices_program_,
                    "g_InstanceInverseTransform", inverse_instance_transform);
                gfxProgramSetParameter(
                    gfx_, generate_animated_vertices_program_, "g_VertexBuffer", getVertexBuffer());
                gfxProgramSetParameter(gfx_, generate_animated_vertices_program_, "g_VertexSourceBuffer",
                    getVertexSourceBuffer());
                gfxProgramSetParameter(gfx_, generate_animated_vertices_program_, "g_JointMatricesBuffer",
                    getJointMatricesBuffer());
                gfxProgramSetParameter(
                    gfx_, generate_animated_vertices_program_, "g_JointBuffer", getJointBuffer());
                gfxProgramSetParameter(
                    gfx_, generate_animated_vertices_program_, "g_MorphWeightBuffer", getMorphWeightBuffer());

                uint32_t const *num_threads =
                    gfxKernelGetNumThreads(gfx_, generate_animated_vertices_kernel_);
                uint32_t const num_groups_x = (vertex_count + num_threads[0] - 1) / num_threads[0];

                gfxCommandBindKernel(gfx_, generate_animated_vertices_kernel_);
                gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
            }
        }
    }
    return ret;
}

void CapsaicinInternal::updateSceneBVH(bool const animationGPUUpdated) noexcept
{
    if (animationGPUUpdated || mesh_updated_ || transform_updated_ || instances_updated_)
    {
        GfxCommandEvent const command_event(gfx_, "BuildBVH");
        GfxInstance const    *instances      = gfxSceneGetObjects<GfxInstance>(scene_);
        uint32_t const        instance_count = gfxSceneGetObjectCount<GfxInstance>(scene_);

        // Check if performing an update or complete rebuild
        bool const freshBuild = mesh_updated_ || !acceleration_structure_ || instances_updated_;
        if (freshBuild)
        {
            destroyAccelerationStructure();
            acceleration_structure_ = gfxCreateAccelerationStructure(gfx_);
            acceleration_structure_.setName("AccelerationStructure");
        }

        std::unordered_map<uint32_t, uint32_t> mesh_data; /**< Cache of used meshes. Allows us not to
                                                             duplicate meshes and create instances instead.*/
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            uint32_t const instance_index = gfxSceneGetObjectHandle<GfxInstance>(scene_, i);
            if (instance_index >= instance_data_.size())
            {
                continue;
            }

            Instance const            &instance  = instance_data_[instance_index];
            GfxConstRef<GfxMesh> const mesh_ref  = instances[i].mesh;
            MeshInfo const            &mesh_info = mesh_infos_[static_cast<uint32_t>(mesh_ref)];
            if (freshBuild)
            {
                if (instance_index >= raytracing_primitives_.size())
                {
                    raytracing_primitives_.resize(static_cast<size_t>(instance_index) + 1);
                }

                // Cache which meshes already have a RTPrimitive. For any new instance that references an
                // already used mesh we will create an actual instance in the acceleration structure using the
                // meshes existing corresponding primitive. However, for animated meshes we cannot reuse
                // mesh primitives as the animations may be applied to each of them differently. The same goes
                // for per object LODs as the LOD may differ for each instance
                GfxRaytracingPrimitive &rt_mesh = raytracing_primitives_[instance_index];
                auto       it = !mesh_info.is_animated ? mesh_data.find(static_cast<uint32_t>(mesh_ref))
                                                       : mesh_data.end();
                bool const isInstanced = it != mesh_data.end();
                if (isInstanced)
                {
                    // Create an instance from an existing mesh
                    uint32_t const                existing_instance_index = it->second;
                    GfxRaytracingPrimitive const &existing_rt_mesh =
                        raytracing_primitives_[existing_instance_index];
                    rt_mesh = gfxCreateRaytracingPrimitiveInstance(gfx_, existing_rt_mesh);
                }
                else
                {
                    // Create a new mesh primitive
                    mesh_data.emplace(static_cast<uint32_t>(mesh_ref), instance_index);
                    rt_mesh = gfxCreateRaytracingPrimitive(gfx_, acceleration_structure_);
                }

                // Set instance data
                glm::mat4 const row_major_transform = transpose(instances[i].transform);
                gfxRaytracingPrimitiveSetTransform(gfx_, rt_mesh, &row_major_transform[0][0]);
                gfxRaytracingPrimitiveSetInstanceID(gfx_, rt_mesh, instance_index);
                gfxRaytracingPrimitiveSetInstanceContributionToHitGroupIndex(
                    gfx_, rt_mesh, instance_index * sbt_stride_in_entries_[kGfxShaderGroupType_Hit]);

                // Instanced RT primitives do not need building
                if (isInstanced)
                {
                    continue;
                }

                // Build the mesh into acceleration structure
                GfxBuffer const index_buffer = gfxCreateBufferRange<uint32_t>(
                    gfx_, index_buffer_, instance.index_offset_idx, instance.index_count);
                GfxBuffer const vertex_buffer = gfxCreateBufferRange<Vertex>(gfx_, vertex_buffer_,
                    instance.vertex_offset_idx[vertex_data_index_], mesh_info.vertex_count);

                GfxConstRef<GfxMaterial> const material_ref = instances[i].material;
                // The mesh is set as opaque based on the alpha mode flag, we also check if it actually has
                // any valid alpha sources and set to opaque if not as an optimisation for incorrect input
                // files
                bool const noAlpha =
                    (material_ref ? (material_ref->albedo.w >= 1.0F && !material_ref->albedo_map) : false);
                uint32_t const opaqueFlag =
                    !material_ref || noAlpha || material_ref->alpha_mode == GfxMaterialAlphaMode_Opaque
                        ? kGfxBuildRaytracingPrimitiveFlag_Opaque
                        : 0;

                gfxRaytracingPrimitiveBuild(gfx_, rt_mesh, index_buffer, vertex_buffer, 0, opaqueFlag);

                gfxDestroyBuffer(gfx_, index_buffer);
                gfxDestroyBuffer(gfx_, vertex_buffer);
            }
            else
            {
                // Update the transform matrix accordingly
                glm::mat4 const row_major_transform = transpose(instances[i].transform);
                gfxRaytracingPrimitiveSetTransform(
                    gfx_, raytracing_primitives_[instance_index], &row_major_transform[0][0]);

                // Just perform an update on existing RT primitives
                if (mesh_info.is_animated)
                {
                    // Need to update the acceleration structure with the animated vertex changes
                    GfxRaytracingPrimitive const &rt_mesh = raytracing_primitives_[instance_index];

                    uint32_t const index_count  = instance.index_count;
                    uint32_t const index_offset = instance.index_offset_idx;

                    GfxBuffer const index_buffer =
                        gfxCreateBufferRange<uint32_t>(gfx_, index_buffer_, index_offset, index_count);
                    GfxBuffer const vertex_buffer = gfxCreateBufferRange<Vertex>(gfx_, vertex_buffer_,
                        instance.vertex_offset_idx[vertex_data_index_], mesh_info.vertex_count);

                    gfxRaytracingPrimitiveUpdate(gfx_, rt_mesh, index_buffer, vertex_buffer, sizeof(Vertex));

                    gfxDestroyBuffer(gfx_, index_buffer);
                    gfxDestroyBuffer(gfx_, vertex_buffer);
                }
            }
        }

        gfxAccelerationStructureUpdate(gfx_, acceleration_structure_);
    }
}
} // namespace Capsaicin
