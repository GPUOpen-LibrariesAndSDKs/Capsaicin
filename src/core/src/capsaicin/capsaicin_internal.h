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

#include "gpu_shared.h"
#include "graph.h"
#include "renderer.h"

#include <deque>
#include <gfx_imgui.h>
#include <gfx_scene.h>

namespace Capsaicin
{
class RenderTechnique;

class CapsaicinInternal
{
public:
    CapsaicinInternal();
    ~CapsaicinInternal();

    GfxContext  getGfx() const;
    GfxScene    getScene() const;
    uint32_t    getWidth() const;
    uint32_t    getHeight() const;
    char const *getShaderPath() const;

    /**
     * Get the current frame index (starts at zero)
     * @return The index of the current frame to/being rendered.
     */
    uint32_t getFrameIndex() const noexcept;

    /**
     * Get the elapsed time since the last render call.
     * @return The elapsed frame time (seconds)
     */
    double getFrameTime() const noexcept;

    /**
     * Get the average frame time.
     * @return The elapsed frame time (seconds)
     */
    double getAverageFrameTime() const noexcept;

    /**
     * Check if the current scene has any usable animations.
     * @return True if animations are present, False otherwise.
     */
    bool hasAnimation() const noexcept;

    /**
     * Set the current playback play/paused state.
     * @param paused True to pause animation, False to play.
     */
    void setPaused(bool paused) noexcept;

    /**
     * Get the current animation play/paused state.
     * @return True if playback is paused, False otherwise.
     */
    bool getPaused() const noexcept;

    /**
     * Set the current playback mode.
     * @param playMode The new playback mode (False to playback in real-time mode, True uses fixed frame
     * rate).
     */
    void setFixedFrameRate(bool playMode) noexcept;

    /**
     * Set the current fixed rate frame time.
     * @param fixed_frame_time A duration in seconds.
     */
    void setFixedFrameTime(double fixed_frame_time) noexcept;

    /**
     * Get current playback mode.
     * @return True if using fixed frame rate, False is using real-time.
     */
    bool getFixedFrameRate() const noexcept;

    /**
     * Restart playback to start of animation.
     */
    void restartPlayback() noexcept;

    /**
     * Increase current playback speed by double.
     */
    void increasePlaybackSpeed() noexcept;

    /**
     * Decrease current playback speed by half.
     */
    void decreasePlaybackSpeed() noexcept;

    /**
     * Get the current playback speed.
     * @return The current playback speed.
     */
    double getPlaybackSpeed() const noexcept;

    /**
     * Reset the playback speed to default.
     */
    void resetPlaybackSpeed() noexcept;

    /**
     * Step playback forward by specified number of frames.
     * @param frames The number of frames to step forward.
     */
    void stepPlaybackForward(uint32_t frames) noexcept;

    /**
     * Step playback backward by specified number of frames.
     * @param frames The number of frames to step backward.
     */
    void stepPlaybackBackward(uint32_t frames) noexcept;

    /**
     * Set the playback to forward/rewind.
     * @param rewind Set to True to rewind, False to playback forward.
     */
    void setPlayRewind(bool rewind) noexcept;

    /**
     * Get the current playback forward/rewind state.
     * @return True if in rewind, False if forward.
     */
    bool getPlayRewind() const noexcept;

    /**
     * Set the current render state. Pausing prevents any new frames from being rendered.
     * @param paused True to pause rendering.
     */
    void setRenderPaused(bool paused) noexcept;

    /**
     * Get the current render paused state.
     * @return True if rendering is paused, False otherwise.
     */
    bool getRenderPaused() const noexcept;

    /**
     */
    void stepJitterFrameIndex(uint32_t frames) noexcept;

    /**
     * Check if the scenes mesh data was changed this frame.
     * @return True if mesh data has changed.
     */
    bool getMeshesUpdated() const noexcept;

    /**
     * Check if the scenes instance transform data was changed this frame.
     * @return True if instance data has changed.
     */
    bool getTransformsUpdated() const noexcept;

    /**
     * Check if the scene was changed this frame.
     * @return True if scene has changed.
     */
    bool getSceneUpdated() const noexcept;

    /**
     * Check if the scene camera was changed this frame.
     * @note Only flags change in which camera is active, this does not track changes to any specific cameras
     * parameters.
     * @return True if camera has changed.
     */
    bool getCameraUpdated() const noexcept;

    /**
     * Check if the environment map was changed this frame.
     * @return True if environment map has changed.
     */
    bool getEnvironmentMapUpdated() const noexcept;

    /**
     * Gets the list of currently available AOVs.
     * @returns The AOV list.
     */
    std::vector<std::string_view> getAOVs() const noexcept;

    /**
     * Query if a AOV buffer currently exists.
     * @param aov The AOV to search for.
     * @returns True if AOV exists, false if not.
     */
    bool hasAOVBuffer(std::string_view const &aov) const noexcept;

    /**
     * Gets an AOV buffer.
     * @param aov The AOV buffer to get.
     * @returns The requested texture or null texture if AOV not found.
     */
    GfxTexture getAOVBuffer(std::string_view const &aov) const noexcept;

    /**
     * Checks whether a debug view is of an AOV.
     * @param view The debug view to check.
     * @returns True if the debug view is directly reading an AOV, False otherwise or if debug view is
     * unknown.
     */
    bool checkDebugViewAOV(std::string_view const &view) const noexcept;

    /**
     * Query if a shared buffer currently exists.
     * @param buffer The buffer to search for.
     * @returns True if buffer exists, false if not.
     */
    bool hasBuffer(std::string_view const &buffer) const noexcept;

    /**
     * Gets an shared buffer.
     * @param buffer The buffer to get.
     * @returns The requested buffer or null buffer if not found.
     */
    GfxBuffer getBuffer(std::string_view const &buffer) const noexcept;

    /**
     * Query if a shared component currently exists.
     * @param component The Component to search for.
     * @returns True if component exists, false if not.
     */
    bool hasComponent(std::string_view const &component) const noexcept;

    /**
     * Gets an shared component.
     * @param component The component to get.
     * @returns The requested component or nullptr if not found.
     */
    std::shared_ptr<Component> const &getComponent(std::string_view const &component) const noexcept;

    /**
     * Gets an shared component and casts to requested type.
     * @tparam T The type of component cast.
     * @returns The requested component or nullptr if not found.
     */
    template<typename T>
    std::shared_ptr<T> const getComponent() const noexcept
    {
        return std::dynamic_pointer_cast<T>(getComponent(static_cast<std::string_view>(toStaticString<T>())));
    }

    /**
     * Gets the list of supported renderers.
     * @returns The renderers list.
     */
    static std::vector<std::string_view> GetRenderers() noexcept;

    /**
     * Gets the name of the currently set renderer.
     * @returns The current renderer name.
     */
    std::string_view getCurrentRenderer() const noexcept;

    /**
     * Sets the current renderer.
     * @param name The name of the renderer to set (must be one of the options from GetRenderers()).
     * @returns True if successful, False otherwise.
     */
    bool setRenderer(std::string_view const &name) noexcept;

    /**
     * Gets the currently set scene.
     * @returns The current scene name.
     */
    std::vector<std::string> const &getCurrentScenes() const noexcept;

    /**
     * Sets the current scene.
     * @param name The name of the scene file.
     * @returns True if successful, False otherwise.
     */
    bool setScenes(std::vector<std::string> const &names) noexcept;

    /**
     * Gets the list of cameras available in the current scene.
     * @returns The cameras list.
     */
    std::vector<std::string_view> getSceneCameras() const noexcept;

    /**
     * Gets the name of the currently set scene camera.
     * @returns The current camera name.
     */
    std::string_view getSceneCurrentCamera() const noexcept;

    /**
     * Gets the current scenes camera.
     * @returns The requested camera object.
     */
    GfxRef<GfxCamera> getSceneCamera() const noexcept;

    /**
     * Sets the current scenes camera.
     * @param name The name of the camera to set (must be one of the options from getSceneCameras()).
     * @returns True if successful, False otherwise.
     */
    bool setSceneCamera(std::string_view const &name) noexcept;

    /**
     * Gets the currently set environment map.
     * @returns The current environment map name.
     */
    std::string getCurrentEnvironmentMap() const noexcept;

    /**
     * Sets the current scene environment map.
     * @param name The name of the image file (blank to disable environment map).
     * @returns True if successful, False otherwise.
     */
    bool setEnvironmentMap(std::string const &name) noexcept;

    /**
     * Gets the list of currently available debug views.
     * @returns The debug view list.
     */
    std::vector<std::string_view> getDebugViews() const noexcept;

    /**
     * Gets the currently set debug view.
     * @returns The debug view string (empty string if none selected).
     */
    std::string_view getCurrentDebugView() const noexcept;

    /**
     * Sets the current debug view.
     * @param name The name of the debug view to set (must be one of the options from GetDebugViews()).
     * @returns True if successful, False otherwise.
     */
    bool setDebugView(std::string_view const &name) noexcept;

    /**
     * Gets render options currently in use.
     * @returns The render options.
     */
    RenderOptionList const &getOptions() const noexcept;
    RenderOptionList       &getOptions() noexcept;

    /**
     * Checks if an options exists with the specified type.
     * @tparam T Generic type parameter of the requested option.
     * @param name The name of the option to get.
     * @returns True if options is found and has correct type, False otherwise.
     */
    template<typename T>
    bool hasOption(std::string_view const &name) noexcept
    {
        auto &options = getOptions();
        if (auto i = options.find(name); i != options.end())
        {
            return std::holds_alternative<T>(i->second);
        }
        return false;
    }

    /**
     * Gets an option from internal options list.
     * @tparam T Generic type parameter of the requested option.
     * @param name The name of the option to get.
     * @returns The options value (nullptr if option does not exists or typename does not match).
     */
    template<typename T>
    T const &getOption(std::string_view const &name) const noexcept
    {
        if (auto i = options_.find(name); i != options_.end())
        {
            if (std::holds_alternative<T>(i->second))
            {
                return *std::get_if<T>(&(i->second));
            }
        }
        GFX_PRINTLN("Error: Unknown settings options requested: %s", name.data());
        static T unknown;
        return unknown;
    }

    /**
     * Gets a reference to an option from internal options list.
     * @tparam T Generic type parameter of the requested option.
     * @param name The name of the option to get.
     * @returns The options value (nullptr if option does not exists or typename does not match).
     */
    template<typename T>
    T &getOption(std::string_view const &name) noexcept
    {
        auto &options = getOptions();
        if (auto i = options.find(name); i != options.end())
        {
            if (std::holds_alternative<T>(i->second))
            {
                return *std::get_if<T>(&(i->second));
            }
        }
        GFX_PRINTLN("Error: Unknown settings options requested: %s", name.data());
        static T unknown;
        return unknown;
    }

    /**
     * Sets an options value in the internal options list.
     * If the option does not exists it is created.
     * @tparam T Generic type parameter of the requested option.
     * @param name  The name of the option to set.
     * @param value The new value of the option.
     */
    template<typename T>
    void setOption(std::string_view const &name, const T value) noexcept
    {
        auto &options = getOptions();
        if (auto i = options.find(name); i != options.end())
        {
            if (std::holds_alternative<T>(i->second))
            {
                *std::get_if<T>(&(i->second)) = value;
            }
        }
        else
        {
            options.emplace(name, value);
        }
    }

    glm::vec4  getInvDeviceZ() const;
    glm::vec3  getPreViewTranslation() const;
    GfxTexture getEnvironmentBuffer() const;

    /**
     * Gets the current camera data.
     * @returns The camera.
     */
    GfxCamera const &getCamera() const;

    /**
     * Gets camera matrices for current camera.
     * There are 2 versions of each camera matrices depending on whether jotter should be applied or not.
     * @param jittered (Optional) True if jittered camera matrices should be returned.
     * @returns The camera matrices.
     */
    CameraMatrices const &getCameraMatrices(bool jittered = false) const;

    /**
     * Gets GPU camera matrices buffer.
     * There are 2 versions of each camera matrices depending on whether jotter should be applied or not.
     * @param jittered (Optional) True if jittered camera matrices should be returned.
     * @returns The camera matrices buffer.
     */
    GfxBuffer getCameraMatricesBuffer(bool jittered = false) const;

    /**
     * Gets count of enabled delta lights (point,spot,direction) in current scene.
     * @returns The delta light count.
     */
    uint32_t getDeltaLightCount() const noexcept;

    /**
     * Gets count of enabled area lights in current scene.
     * @returns The area light count.
     */
    uint32_t getAreaLightCount() const noexcept;

    /**
     * Gets count of enabled environment lights in current scene.
     * @returns The environment light count.
     */
    uint32_t getEnvironmentLightCount() const noexcept;

    /**
     * Gets count of number of triangles present in current scene.
     * @returns The triangle count.
     */
    uint32_t getTriangleCount() const noexcept;

    /**
     * Gets size of the acceleration structure (in bytes).
     * @returns The acceleration structure size.
     */
    uint64_t getBvhDataSize() const noexcept;

    GfxBuffer        getInstanceBuffer() const;
    Instance const  *getInstanceData() const;
    Instance        *getInstanceData();
    glm::vec3 const *getInstanceMinBounds() const;
    glm::vec3 const *getInstanceMaxBounds() const;
    GfxBuffer        getInstanceIdBuffer() const;
    uint32_t const  *getInstanceIdData() const;

    glm::mat4x3 const *getTransformData() const;
    GfxBuffer          getTransformBuffer() const;
    GfxBuffer          getPrevTransformBuffer() const;
    glm::mat4x3 const *getPrevTransformData() const;

    GfxBuffer       getMaterialBuffer() const;
    Material const *getMaterialData() const;

    GfxTexture const *getTextures() const;
    uint32_t          getTextureCount() const;
    GfxSamplerState   getLinearSampler() const;
    GfxSamplerState   getLinearWrapSampler() const;
    GfxSamplerState   getNearestSampler() const;
    GfxSamplerState   getAnisotropicSampler() const;

    GfxBuffer       getMeshBuffer() const;
    Mesh const     *getMeshData() const;
    GfxBuffer       getIndexBuffer() const;
    uint32_t const *getIndexData() const;
    GfxBuffer       getVertexBuffer() const;
    Vertex const   *getVertexData() const;

    GfxBuffer const *getIndexBuffers() const;
    uint32_t         getIndexBufferCount() const;
    GfxBuffer const *getVertexBuffers() const;
    uint32_t         getVertexBufferCount() const;

    GfxAccelerationStructure getAccelerationStructure() const;

    inline uint32_t getSbtStrideInEntries(GfxShaderGroupType type) const { return sbt_stride_in_entries_[type]; }

    /**
     * Calculate and return the AABB surrounding current scene contents.
     * @returns The scene bounds (min, max).
     */
    std::pair<float3, float3> getSceneBounds() const;

    template<typename TYPE>
    GfxBuffer allocateConstantBuffer(uint32_t element_count)
    {
        GfxBuffer constant_buffer = allocateConstantBuffer(element_count * sizeof(TYPE));
        constant_buffer.setStride((uint32_t)sizeof(TYPE));
        return constant_buffer;
    }

    GfxBuffer allocateConstantBuffer(uint64_t size);

    /**
     * Initializes Capsaicin. Must be called before any other functions.
     * @param gfx The gfx context to use inside Capsaicin.
     * @param imgui_context (Optional) The ImGui context.
     */
    void initialize(GfxContext gfx, ImGuiContext *imgui_context);

    /**
     * Render the current frame.
     */
    void render();

    /**
     * Render UI elements related to current internal state
     * Must be called between ImGui::Begin() and ImGui::End().
     * @param readOnly (Optional) True to only display read only data, False to display controls accepting
     * user input.
     */
    void renderGUI(bool readOnly = false);

    /** Terminates this object */
    void terminate();

    /**
     * Reload all shader code currently in use
     */
    void reloadShaders() noexcept;

    /**
     * Saves an AOV buffer to disk.
     * @param file_path Full pathname to the file to save as.
     * @param aov       The buffer to save (get available from @GetAOVs()).
     */
    void dumpAOVBuffer(char const *file_path, std::string_view const &aov);

    /**
     * Saves an texture to disk.
     * @param file_path   Full pathname to the file to save as.
     * @param dump_buffer Texture to save.
     */
    void dumpAnyBuffer(char const *file_path, GfxTexture dump_buffer);

    /**
     * Saves current camera attributes to disk.
     * @param file_path   Full pathname to the file to save as.
     * @param jittered    Jittered camera or not.
     */
    void dumpCamera(char const *file_path, bool jittered);

private:
    /**
     * Sets up the render techniques for the currently set renderer.
     * This will setup any required AOVs, views or buffers required for all specified render techniques.
     * @param name Name of the renderer to setup.
     */
    void setupRenderTechniques(std::string_view const &name) noexcept;

    /**
     * Reset current frame index and duration state.
     * This should be called whenever and renderer or scene changes are made.
     */
    void resetPlaybackState() noexcept;

    /**
     * Reset internal data such as AOVs and history to initial state.
     */
    void resetRenderState() noexcept;

    void dumpBuffer(char const *file_path, GfxTexture dump_buffer);
    void saveImage(GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height,
        char const *file_path);
    void saveEXR(GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height,
        char const *exr_file_path);
    void saveJPG(GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height,
        char const *jpg_file_path);
    void dumpCamera(char const *file_path, CameraMatrices const &camera_matrices, float camera_jitter_x,
        float camera_jitter_y);

    size_t mesh_hash_               = 0;
    size_t transform_hash_          = 0;
    bool   was_resized_             = false;
    bool   mesh_updated_            = true;
    bool   transform_updated_       = true;
    bool   environment_map_updated_ = true;
    bool   scene_updated_           = true;
    bool   camera_updated_          = true;

    GfxContext  gfx_; /**< The graphics context to be used. */
    std::string shader_path_;
    uint32_t    buffer_width_  = 0;
    uint32_t    buffer_height_ = 0;

    GfxScene    scene_; /**< The scene to be rendered. */
    GfxTexture  environment_buffer_;
    std::vector<std::string> scene_files_;
    std::string environment_map_file_;

    uint32_t frame_index_        = 0;   /**< Current frame number (incremented each render call) */
    uint32_t jitter_frame_index_ = ~0u; /**< Current jitter frame number */
    double   current_time_       = 0.0; /**< Current wall clock time used for timing (seconds) */
    double   frame_time_         = 0.0; /**< Elapsed frame time for most recent frame (seconds) */

    bool   play_paused_           = true;  /**< Current animation play/paused state (True if paused) */
    bool   play_fixed_framerate_  = false; /**< Current animation playback mode (True if fixed frame rate) */
    double play_time_             = 0.0f;  /**< Current animation absolute playback position (s) */
    double play_time_old_         = -1.0f; /**< Previous animation absolute playback position (s) */
    double play_fixed_frame_time_ = 1.0f / 30.0f; /**< Frame time used with fixed frame rate mode */
    double play_speed_            = 1.0f;         /**< Current playback speed */
    bool   play_rewind_           = false;        /**< Current rewind state (True if rewinding) */
    bool   render_paused_ = false; /**< Current render paused state (True to pause rendering of new frames) */

    CameraMatrices camera_matrices_[2]; /**< Unjittered and jittered matrices */
    float          camera_jitter_x_;
    float          camera_jitter_y_;

    RenderOptionList options_; /**< Options for controlling the operation of each render technique */

    std::vector<std::unique_ptr<RenderTechnique>>
        render_techniques_; /**< The list of render techniques to be applied. */
    std::map<std::string_view /*name*/, std::shared_ptr<Component>>
                              components_;         /**< The list of render techniques to be applied. */
    std::string_view          renderer_name_;      /**< Currently used renderer string name */
    std::unique_ptr<Renderer> renderer_ = nullptr; /**< Currently used renderer */
    using debug_views                   = std::vector<std::pair<std::string_view, bool>>;
    debug_views      debug_views_; /**< List of available debug views */
    std::string_view debug_view_;  /**< The debug view to use (get available from GetDebugViews() -
                                               "None" or empty for default behaviour) */

    GfxKernel  blit_kernel_;  /**< The kernel to blit the color buffer to the back buffer. */
    GfxProgram blit_program_; /**< The program to blit the color buffer to the back buffer. */
    GfxKernel  debug_depth_kernel_;
    GfxProgram debug_depth_program_;
    GfxProgram convolve_ibl_program_;

    using aov_buffer = std::vector<std::pair<std::string_view /*name*/, GfxTexture>>;
    using aov_backup = std::vector<std::pair<GfxTexture /*Source*/, GfxTexture /*Destination*/>>;
    using aov_clear  = std::vector<GfxTexture>;
    aov_buffer aov_buffers_;        /**< The list of AOVs populated by the render techniques. */
    aov_backup aov_backup_buffers_; /**< The list of AOVS to backup each frame */
    aov_clear  aov_clear_buffers_;  /**< List of buffers to clear each frame */
    using shared_buffer = std::vector<std::pair<std::string_view, GfxBuffer>>;
    shared_buffer shared_buffers_; /**< The list of buffers populated by the render techniques. */
    GfxBuffer     constant_buffer_pools_[kGfxConstant_BackBufferCount];
    uint64_t      constant_buffer_pool_cursor_ = 0;

    GfxBuffer                camera_matrices_buffer_[2]; /**< Unjittered and jittered camera matrices */
    std::vector<Instance>    instance_data_;
    GfxBuffer                instance_buffer_;
    std::vector<glm::vec3>   instance_min_bounds_;
    std::vector<glm::vec3>   instance_max_bounds_;
    std::vector<uint32_t>    instance_id_data_;
    GfxBuffer                instance_id_buffer_;
    std::vector<glm::mat4x3> transform_data_;
    GfxBuffer                transform_buffer_;
    std::vector<glm::mat4x3> prev_transform_data_;
    GfxBuffer                prev_transform_buffer_;
    std::vector<Material>    material_data_;
    GfxBuffer                material_buffer_;
    std::vector<GfxTexture>  texture_atlas_;
    GfxSamplerState          linear_sampler_;
    GfxSamplerState          linear_wrap_sampler_;
    GfxSamplerState          nearest_sampler_;
    GfxSamplerState          anisotropic_sampler_;
    std::vector<Mesh>        mesh_data_;
    GfxBuffer                mesh_buffer_;
    std::vector<uint32_t>    index_data_;
    GfxBuffer                index_buffer_; /**< The buffer storing all indices so it can be access via RT. */
    std::vector<Vertex>      vertex_data_;
    GfxBuffer vertex_buffer_; /**< The buffer storing all vertices so it can be access via RT. */
    GfxAccelerationStructure            acceleration_structure_;
    std::vector<GfxRaytracingPrimitive> raytracing_primitives_;
    uint32_t                            sbt_stride_in_entries_[kGfxShaderGroupType_Count] = {};

    // Scene statistics for currently loaded scene
    uint32_t triangle_count_ = 0;

    Graph frameGraph; /**< The stored frame history graph */

    std::deque<std::tuple<std::string /*fileName*/, std::string /*AOV*/>>        dump_requests_;
    std::deque<std::tuple<std::string /*fileName*/, bool /*jitterred*/>>         dump_camera_requests_;
    std::deque<std::tuple<GfxBuffer, uint32_t, uint32_t, std::string, uint32_t>> dump_in_flight_buffers_;
    GfxKernel                                                                    dump_copy_to_buffer_kernel_;
    GfxProgram                                                                   dump_copy_to_buffer_program_;
};
} // namespace Capsaicin
