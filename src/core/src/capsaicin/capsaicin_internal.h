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

#include "capsaicin.h"
#include "gpu_shared.h"
#include "graph.h"
#include "renderer.h"

#include <deque>
#include <filesystem>
#include <gfx_imgui.h>
#include <gfx_scene.h>

namespace Capsaicin
{
class RenderTechnique;

class CapsaicinInternal
{
public:
    CapsaicinInternal() = default;

    ~CapsaicinInternal();

    CapsaicinInternal(CapsaicinInternal const &other)                = delete;
    CapsaicinInternal(CapsaicinInternal &&other) noexcept            = delete;
    CapsaicinInternal &operator=(CapsaicinInternal const &other)     = delete;
    CapsaicinInternal &operator=(CapsaicinInternal &&other) noexcept = delete;

    [[nodiscard]] GfxContext               getGfx() const;
    [[nodiscard]] GfxScene                 getScene() const;
    [[nodiscard]] std::vector<std::string> getShaderPaths() const;

    /**
     * Get the current window width and height.
     * @return Window width and height.
     */
    [[nodiscard]] uint2 getWindowDimensions() const noexcept;

    /**
     * Get the current render width and height.
     * This may differ from window resolution when using render scaling or other non 1:1 render resolutions.
     * @return Render width and height.
     */
    [[nodiscard]] uint2 getRenderDimensions() const noexcept;

    /**
     * Get the current scaling factor that differentiates between render resolution and window resolution.
     * @return The scaling factor to apply to window resolution to get render resolution.
     */
    [[nodiscard]] float getRenderDimensionsScale() const noexcept;

    /**
     * Set a scaling factor to differentiate between render resolution and window resolution.
     * A scaling factor of 2.0 would result in the render resolution being set to twice the window resolution
     * along each 2D axis (e.g. 1920x1080 will become 3840x2160). While a value of 0.5 will result in half the
     * resolution along each axis. In cases where the scaling factor does not evenly divide into the window
     * resolution then the render resolution will be rounded to the nearest pixel.
     * @param scale The scaling factor to apply to window resolution to get render resolution.
     */
    void setRenderDimensionsScale(float scale) noexcept;

    /**
     * Get the index of the most recent frame (starts at zero).
     * @return The index of the current/last frame rendered.
     */
    [[nodiscard]] uint32_t getFrameIndex() const noexcept;

    /**
     * Get the elapsed time since the last render call.
     * @return The elapsed frame time (seconds)
     */
    [[nodiscard]] double getFrameTime() const noexcept;

    /**
     * Get the average frame time.
     * @return The elapsed frame time (seconds)
     */
    [[nodiscard]] double getAverageFrameTime() const noexcept;

    /**
     * Check if the current scene has any usable animations.
     * @return True if animations are present, False otherwise.
     */
    [[nodiscard]] bool hasAnimation() const noexcept;

    /**
     * Set the current playback play/paused state.
     * @param paused True to pause animation, False to play.
     */
    void setPaused(bool paused) noexcept;

    /**
     * Get the current animation play/paused state.
     * @return True if playback is paused, False otherwise.
     */
    [[nodiscard]] bool getPaused() const noexcept;

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
    [[nodiscard]] bool getFixedFrameRate() const noexcept;

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
    [[nodiscard]] double getPlaybackSpeed() const noexcept;

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
    [[nodiscard]] bool getPlayRewind() const noexcept;

    /**
     * Set the current render state. Pausing prevents any new frames from being rendered.
     * @param paused True to pause rendering.
     */
    void setRenderPaused(bool paused) noexcept;

    /**
     * Get the current render paused state.
     * @return True if rendering is paused, False otherwise.
     */
    [[nodiscard]] bool getRenderPaused() const noexcept;

    /**
     * Check if the render dimensions changed this frame.
     * @return True if render dimensions has changed.
     */
    [[nodiscard]] bool getRenderDimensionsUpdated() const noexcept;

    /**
     * Check if the window dimensions changed this frame.
     * @return True if window dimensions has changed.
     */
    [[nodiscard]] bool getWindowDimensionsUpdated() const noexcept;

    /**
     * Check if the scenes mesh data was changed this frame.
     * @return True if mesh data has changed.
     */
    [[nodiscard]] bool getMeshesUpdated() const noexcept;

    /**
     * Check if the scenes instance transform data was changed this frame.
     * @return True if instance data has changed.
     */
    [[nodiscard]] bool getTransformsUpdated() const noexcept;

    /**
     * Check if the scenes instance data was changed this frame (not including transforms).
     * @return True if instance data has changed.
     */
    [[nodiscard]] bool getInstancesUpdated() const noexcept;

    /**
     * Check if the scene was changed this frame.
     * @return True if scene has changed.
     */
    [[nodiscard]] bool getSceneUpdated() const noexcept;

    /**
     * Check if the scene camera was changed this frame.
     * @note Only flags changes to which camera is active, this does not track changes to any specific cameras
     * parameters.
     * @return True if camera has changed.
     */
    [[nodiscard]] bool getCameraChanged() const noexcept;

    /**
     * Check if the scene camera was moved this frame.
     * @return True if camera has been updated.
     */
    [[nodiscard]] bool getCameraUpdated() const noexcept;

    /**
     * Check if the animation was updated this frame.
     * @return True if animation has changed.
     */
    [[nodiscard]] bool getAnimationUpdated() const noexcept;

    /**
     * Check if the environment map was changed this frame.
     * @return True if environment map has changed.
     */
    [[nodiscard]] bool getEnvironmentMapUpdated() const noexcept;

    /**
     * Gets the list of currently available shared textures.
     * @return The shared texture list.
     */
    [[nodiscard]] std::vector<std::string_view> getSharedTextures() const noexcept;

    /**
     * Query if a shared texture currently exists.
     * @param texture The name of the shared texture to search for.
     * @return True if shared texture exists, false if not.
     */
    [[nodiscard]] bool hasSharedTexture(std::string_view const &texture) const noexcept;

    /**
     * Check if a shared texture exists and has the requested dimensions. The texture is resized if required.
     * @param texture    The name of the shared texture to search for.
     * @param dimensions The requested dimensions.
     * @param mips       (Optional) Number of mip levels.
     * @return True if shared texture exists and matches required size, false if not.
     */
    bool checkSharedTexture(std::string_view const &texture, uint2 dimensions, uint32_t mips = 1);

    /**
     * Gets a shared texture.
     * @param texture The name of the shared texture to get.
     * @return The requested texture or null texture if not found.
     */
    [[nodiscard]] GfxTexture const &getSharedTexture(std::string_view const &texture) const noexcept;

    /**
     * Checks whether a debug view is of a shared texture.
     * @param view The name of the debug view to check.
     * @return True if the debug view is directly reading a shared texture, False otherwise or if debug view
     * is unknown.
     */
    [[nodiscard]] bool checkDebugViewSharedTexture(std::string_view const &view) const noexcept;

    /**
     * Query if a shared buffer currently exists.
     * @param buffer The name of the buffer to search for.
     * @return True if buffer exists, false if not.
     */
    [[nodiscard]] bool hasSharedBuffer(std::string_view const &buffer) const noexcept;

    /**
     * Check if a shared buffer exists and has the requested size. The buffer is resized if required.
     * @param buffer    The name of the buffer to search for.
     * @param size      The requested size in Bytes.
     * @param exactSize (Optional) True if buffer must exactly match requested size, False if buffer size must
     * be greater or equal to requested size (Default: false).
     * @param copy      (Optional) True if any existing data should be copied on resize (Default: false).
     * @return True if buffer exists and matches required size, false if not.
     */
    bool checkSharedBuffer(
        std::string_view const &buffer, uint64_t size, bool exactSize = false, bool copy = false);

    /**
     * Gets a shared buffer.
     * @param buffer The name of the buffer to get.
     * @return The requested buffer or null buffer if not found.
     */
    [[nodiscard]] GfxBuffer const &getSharedBuffer(std::string_view const &buffer) const noexcept;

    /**
     * Query if a shared component currently exists.
     * @param component The Component to search for.
     * @return True if component exists, false if not.
     */
    [[nodiscard]] bool hasComponent(std::string_view const &component) const noexcept;

    /**
     * Gets a shared component.
     * @param component The component to get.
     * @return The requested component or nullptr if not found.
     */
    [[nodiscard]] std::shared_ptr<Component> const &getComponent(
        std::string_view const &component) const noexcept;

    /**
     * Gets a shared component and casts to requested type.
     * @tparam T The type of component cast.
     * @return The requested component or nullptr if not found.
     */
    template<typename T>
    [[nodiscard]] std::shared_ptr<T> const getComponent() const noexcept
    {
        return std::dynamic_pointer_cast<T>(getComponent(static_cast<std::string_view>(toStaticString<T>())));
    }

    /**
     * Gets the list of supported renderers.
     * @return The renderers list.
     */
    static std::vector<std::string_view> GetRenderers() noexcept;

    /**
     * Gets the name of the currently set renderer.
     * @return The current renderer name.
     */
    [[nodiscard]] std::string_view getCurrentRenderer() const noexcept;

    /**
     * Sets the current renderer.
     * @param name The name of the renderer to set (must be one of the options from GetRenderers()).
     * @return True if successful, False otherwise.
     */
    bool setRenderer(std::string_view const &name) noexcept;

    /**
     * Gets the currently set scene.
     * @return The current scene name.
     */
    [[nodiscard]] std::vector<std::filesystem::path> const &getCurrentScenes() const noexcept;

    /**
     * Sets the current scene and replaces any existing scene(s).
     * @param fileName The name of the scene file.
     * @return True if successful, False otherwise.
     */
    bool setScene(std::filesystem::path const &fileName) noexcept;

    /**
     * Append the contents of a scene file to the existing scene(s).
     * @param fileName The name of the scene file.
     * @return True if successful, False otherwise.
     */
    bool appendScene(std::filesystem::path const &fileName) noexcept;

    /**
     * Gets the list of cameras available in the current scene.
     * @return The cameras list.
     */
    [[nodiscard]] std::vector<std::string_view> getSceneCameras() const noexcept;

    /**
     * Gets the name of the currently set scene camera.
     * @return The current camera name.
     */
    [[nodiscard]] std::string_view getSceneCurrentCamera() const noexcept;

    /**
     * Gets the current scenes active cameras view data.
     * @return The requested camera data [position, forward, up].
     */
    [[nodiscard]] CameraView getSceneCameraView() const noexcept;

    /**
     * Gets the current scenes active camera vertical Field Of View.
     * @return The requested camera data FOV.
     */
    [[nodiscard]] float getSceneCameraFOV() const noexcept;

    /**
     * Gets the current scenes active camera near and far Z range.
     * @return The requested camera range.
     */
    [[nodiscard]] glm::vec2 getSceneCameraRange() const noexcept;

    /**
     * Sets the current scenes camera.
     * @param name The name of the camera to set (must be one of the options from getSceneCameras()).
     * @return True if successful, False otherwise.
     */
    bool setSceneCamera(std::string_view const &name) noexcept;

    /**
     * Sets the currently active cameras view data.
     * @param position The new camera position.
     * @param forward  The new camera forward direction vector.
     * @param up       The new camera up direction vector.
     */
    void setSceneCameraView(
        glm::vec3 const &position, glm::vec3 const &forward, glm::vec3 const &up) noexcept;

    /**
     * Sets the currently active cameras vertical Field Of View.
     * @param FOVY The vertical filed of view angle.
     */
    void setSceneCameraFOV(float FOVY) noexcept;

    /**
     * Sets the currently active cameras near and far Z range.
     * @param nearFar The near and far Z distances.
     */
    void setSceneCameraRange(glm::vec2 const &nearFar) noexcept;

    /**
     * Gets the currently set environment map.
     * @return The current environment map name.
     */
    [[nodiscard]] std::filesystem::path getCurrentEnvironmentMap() const noexcept;

    /**
     * Sets the current scene environment map.
     * @param fileName The name of the image file (blank to disable environment map).
     * @return True if successful, False otherwise.
     */
    bool setEnvironmentMap(std::filesystem::path const &fileName) noexcept;

    /**
     * Gets the list of currently available debug views.
     * @return The debug view list.
     */
    [[nodiscard]] std::vector<std::string_view> getDebugViews() const noexcept;

    /**
     * Gets the currently set debug view.
     * @return The debug view string (empty string if none selected).
     */
    [[nodiscard]] std::string_view getCurrentDebugView() const noexcept;

    /**
     * Sets the current debug view.
     * @param name The name of the debug view to set (must be one of the options from GetDebugViews()).
     * @return True if successful, False otherwise.
     */
    bool setDebugView(std::string_view const &name) noexcept;

    /**
     * Gets render options currently in use.
     * @return The render options.
     */
    [[nodiscard]] RenderOptionList const &getOptions() const noexcept;
    [[nodiscard]] RenderOptionList       &getOptions() noexcept;

    /**
     * Checks if an options exists with the specified type.
     * @tparam T Generic type parameter of the requested option.
     * @param name The name of the option to get.
     * @return True if options is found and has correct type, False otherwise.
     */
    template<typename T>
    [[nodiscard]] bool hasOption(std::string_view const &name) const noexcept
    {
        auto &options = getOptions();
        if (auto const i = options.find(name); i != options.end())
        {
            return std::holds_alternative<T>(i->second);
        }
        return false;
    }

    /**
     * Gets an option from internal options list.
     * @tparam T Generic type parameter of the requested option.
     * @param name The name of the option to get.
     * @return The options value (uninitialised if option does not exist or typename does not match).
     */
    template<typename T>
    [[nodiscard]] T const &getOption(std::string_view const &name) const noexcept
    {
        if (auto const i = options_.find(name); i != options_.end())
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
     * @return The options value (uninitialised if option does not exist or typename does not match).
     */
    template<typename T>
    [[nodiscard]] T &getOption(std::string_view const &name) noexcept
    {
        auto &options = getOptions();
        if (auto const i = options.find(name); i != options.end())
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
     * If the option does not exist it is created.
     * @tparam T Generic type parameter of the requested option.
     * @param name  The name of the option to set.
     * @param value The new value of the option.
     */
    template<typename T>
    void setOption(std::string_view const &name, T const value) noexcept
    {
        auto &options = getOptions();
        if (auto const i = options.find(name); i != options.end())
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

    [[nodiscard]] GfxTexture getEnvironmentBuffer() const;

    /**
     * Gets the current camera data.
     * @return The camera.
     */
    [[nodiscard]] GfxCamera const &getCamera() const;

    /**
     * Gets camera matrices for current camera.
     * There are 2 versions of each camera matrices depending on whether jotter should be applied or not.
     * @param jittered (Optional) True if jittered camera matrices should be returned.
     * @return The camera matrices.
     */
    [[nodiscard]] CameraMatrices const &getCameraMatrices(bool jittered = false) const;

    /**
     * Gets GPU camera matrices buffer.
     * There are 2 versions of each camera matrices depending on whether jotter should be applied or not.
     * @param jittered (Optional) True if jittered camera matrices should be returned.
     * @return The camera matrices buffer.
     */
    [[nodiscard]] GfxBuffer getCameraMatricesBuffer(bool jittered = false) const;

    /**
     * Get the jitter applied to jittered camera matrices for the most recent frame.
     * @return The jittered values (x, y) respectively.
     */
    [[nodiscard]] float2 getCameraJitter() const noexcept;

    /**
     * Get the phase used to modulate the jitter applied to jittered camera matrices for the most recent
     * frame.
     * @return The jitter phase value.
     */
    [[nodiscard]] uint32_t getCameraJitterPhase() const noexcept;

    /**
     * Set camera jitter to manual and step jitter frame index by a specified number of frames.
     * @param frames The number of frames to step.
     */
    void stepJitterFrameIndex(uint32_t frames) noexcept;

    /**
     * Set camera jitter sequence length.
     * @param length The length of the Halton sequence used to calculate jitter.
     */
    void setCameraJitterPhase(uint32_t length) noexcept;

    /**
     * Gets count of enabled delta lights (point,spot,direction) in current scene.
     * @return The delta light count.
     */
    [[nodiscard]] uint32_t getDeltaLightCount() const noexcept;

    /**
     * Gets count of enabled area lights in current scene.
     * @return The area light count.
     */
    [[nodiscard]] uint32_t getAreaLightCount() const noexcept;

    /**
     * Gets count of enabled environment lights in current scene.
     * @return The environment light count.
     */
    [[nodiscard]] uint32_t getEnvironmentLightCount() const noexcept;

    /**
     * Gets count of number of triangles present in current scene.
     * @return The triangle count.
     */
    [[nodiscard]] uint32_t getTriangleCount() const noexcept;

    /**
     * Gets size of the acceleration structure (in bytes).
     * @return The acceleration structure size.
     */
    [[nodiscard]] uint64_t getBvhDataSize() const noexcept;

    [[nodiscard]] GfxBuffer                    getInstanceBuffer() const;
    [[nodiscard]] std::vector<Instance> const &getInstanceData() const;
    [[nodiscard]] GfxBuffer                    getInstanceIdBuffer() const;
    [[nodiscard]] std::vector<uint32_t> const &getInstanceIdData() const;

    [[nodiscard]] GfxBuffer getTransformBuffer() const;
    [[nodiscard]] GfxBuffer getPrevTransformBuffer() const;

    [[nodiscard]] GfxBuffer getMaterialBuffer() const;

    [[nodiscard]] std::vector<GfxTexture> const &getTextures() const;
    [[nodiscard]] GfxSamplerState                getLinearSampler() const;
    [[nodiscard]] GfxSamplerState                getLinearWrapSampler() const;
    [[nodiscard]] GfxSamplerState                getNearestSampler() const;
    [[nodiscard]] GfxSamplerState                getAnisotropicSampler() const;

    [[nodiscard]] GfxBuffer getIndexBuffer() const;
    [[nodiscard]] GfxBuffer getVertexBuffer() const;
    [[nodiscard]] GfxBuffer getVertexSourceBuffer() const;
    [[nodiscard]] GfxBuffer getJointBuffer() const;
    [[nodiscard]] GfxBuffer getJointMatricesBuffer() const;
    [[nodiscard]] GfxBuffer getMorphWeightBuffer() const;

    [[nodiscard]] uint32_t getVertexDataIndex() const;
    [[nodiscard]] uint32_t getPrevVertexDataIndex() const;

    [[nodiscard]] uint32_t                 getRaytracingPrimitiveCount() const;
    [[nodiscard]] GfxAccelerationStructure getAccelerationStructure() const;

    [[nodiscard]] uint32_t getSbtStrideInEntries(GfxShaderGroupType type) const;
    void                   destroyAccelerationStructure();

    /**
     * Calculate and return the AABB surrounding current scene contents.
     * @return The scene bounds (min, max).
     */
    [[nodiscard]] std::pair<float3, float3> getSceneBounds() const;

    template<typename TYPE>
    [[nodiscard]] GfxBuffer allocateConstantBuffer(uint32_t const element_count)
    {
        GfxBuffer constant_buffer = allocateConstantBuffer(element_count * sizeof(TYPE));
        constant_buffer.setStride(static_cast<uint32_t>(sizeof(TYPE)));
        return constant_buffer;
    }

    [[nodiscard]] GfxBuffer allocateConstantBuffer(uint64_t size);

    /**
     * Create a new texture sized according to the render resolution.
     * @param format The internal format for the new texture.
     * @param name   The unique name for the texture.
     * @param mips   (Optional) The number of mip levels in the new texture. Setting this to UINT_MAX results
     * in automatic calculation of mip levels based on current dimensions.
     * @param scale  (Optional) Scale to apply to render resolution (i.e. value of 0.5 results in half
     * resolution).
     * @return New texture with requested attributes.
     */
    [[nodiscard]] GfxTexture createRenderTexture(DXGI_FORMAT format, std::string_view const &name,
        uint32_t mips = 1, float scale = 1.0F) const noexcept;

    /**
     * Resize an existing texture to match current render dimensions.
     * @param texture Existing texture to resize.
     * @param clear   (Optional) True to clear texture after creation.
     * @param mips    (Optional) The number of mip levels in the new texture. Setting this to 0 will result in
     * mips being created if they existed in the original texture (a full mip chain will be created). Setting
     * this to UINT_MAX results in automatic calculation of mip levels based on current dimensions.
     * @param scale   (Optional) Scale to apply to render resolution (i.e. value of 0.5 results in half
     * resolution).
     * @return New texture with requested dimensions.
     */
    [[nodiscard]] GfxTexture resizeRenderTexture(
        GfxTexture const &texture, bool clear = true, uint32_t mips = 0, float scale = 1.0F) const noexcept;

    /**
     * Create a new texture sized according to the window display resolution.
     * @param format The internal format for the new texture.
     * @param name   The unique name for the texture.
     * @param mips   (Optional) The number of mip levels in the new texture. Setting this to UINT_MAX results
     * in automatic calculation of mip levels based on current dimensions.
     * @param scale  (Optional) Scale to apply to render resolution (i.e. value of 0.5 results in half
     * resolution).
     * @return New texture with requested attributes.
     */
    [[nodiscard]] GfxTexture createWindowTexture(DXGI_FORMAT format, std::string_view const &name,
        uint32_t mips = 1, float scale = 1.0F) const noexcept;

    /**
     * Resize an existing texture to match current window display resolution.
     * @param texture Existing texture to resize.
     * @param clear   (Optional) True to clear texture after creation.
     * @param mips    (Optional) The number of mip levels in the new texture. Setting this to 0 will result in
     * mips being created if they existed in the original texture (a full mip chain will be created). Setting
     * this to UINT_MAX results in automatic calculation of mip levels based on current dimensions.
     * @param scale   (Optional) Scale to apply to render resolution (i.e. value of 0.5 results in half
     * resolution).
     * @return New texture with requested dimensions.
     */
    [[nodiscard]] GfxTexture resizeWindowTexture(
        GfxTexture const &texture, bool clear = true, uint32_t mips = 0, float scale = 1.0F) const noexcept;

    /**
     * Create a new program using provided file name.
     * @param file_name     Name of the program.
     * @return New program.
     */
    [[nodiscard]] GfxProgram createProgram(char const *file_name) const noexcept;

    /**
     * Initializes Capsaicin. Must be called before any other functions.
     * @param gfx The gfx context to use inside Capsaicin.
     * @param imgui_context (Optional) The ImGui context.
     */
    void initialize(GfxContext const &gfx, ImGuiContext *imgui_context);

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
    void terminate() noexcept;

    /**
     * Reload all shader code currently in use
     */
    void reloadShaders() noexcept;

    /**
     * Saves an debug view to disk.
     * @param filePath Full pathname to the file to save as.
     * @param texture   The buffer to save (get available from @getSharedTextures()).
     */
    void dumpDebugView(std::filesystem::path const &filePath, std::string_view const &texture);

    /**
     * Saves current camera attributes to disk.
     * @param filePath   Full pathname to the file to save as.
     * @param jittered    Jittered camera or not.
     */
    void dumpCamera(std::filesystem::path const &filePath, bool jittered) const;

private:
    /*
     * Gets configuration options specific to capsaicin itself.
     * @return A list of all valid configuration options.
     */
    [[nodiscard]] RenderOptionList getStockRenderOptions() noexcept;

    struct RenderOptions
    {
        uint32_t capsaicin_lod_mode   = 0; /**< LOD mode in use (0=none, 1=manual, 2=ObjectCoverage) */
        uint32_t capsaicin_lod_offset = 0; /**< Manual LOD offset (only applicable when LODs are in use)  */
        bool capsaicin_lod_aggressive = false; /**< Enable aggressive mesh LOD simplification (better reduces
                                                  mesh size but with potential to destroy mesh topology) */
        float capsaicin_mirror_roughness_threshold =
            0.1f; /**< The threshold below which to force mirror reflections */
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @return The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Gets a list of any shared components specific to capsaicin itself.
     * @return A list of all supported components.
     */
    [[nodiscard]] ComponentList getStockComponents() const noexcept;

    /**
     * Gets a list of any shared buffers specific to capsaicin itself.
     * @return A list of all supported buffers.
     */
    [[nodiscard]] SharedBufferList getStockSharedBuffers() const noexcept;

    /**
     * Gets the required list of shared textures specific to capsaicin itself.
     * @return A list of all required shared textures.
     */
    [[nodiscard]] SharedTextureList getStockSharedTextures() const noexcept;

    /**
     * Gets a list of any debug views specific to capsaicin itself.
     * @return A list of all supported debug views.
     */
    [[nodiscard]] DebugViewList getStockDebugViews() const noexcept;

    /**
     * Render GUI options specific to capsaicin itself.
     */
    void renderStockGUI() noexcept;

    /**
     * Sets up the shared textures/buffers and debug views used by the current render techniques and
     * components.
     */
    void negotiateRenderTechniques() noexcept;

    /**
     * Sets up the render techniques for the currently set renderer.
     * This will set up any required shared textures, views or buffers required for all specified render
     * techniques.
     * @param name Name of the renderer to set up.
     */
    void setupRenderTechniques(std::string_view const &name) noexcept;

    /**
     * Reset current frame index and duration state.
     * This should be called whenever any renderer or scene changes are made.
     */
    void resetPlaybackState() noexcept;

    /**
     * Reset internal data such as shared textures and history to initial state.
     */
    void resetRenderState() const noexcept;

    /**
     * Reset any internal events to their default state.
     */
    void resetEvents() noexcept;

    /**
     * Load a scene file.
     * @param fileName Name of the scene file to load.
     * @param append   (Optional) True to append the scene contents to the existing scene, false to overwrite.
     * @return True if operation completed successfully.
     */
    [[nodiscard]] bool loadSceneFile(std::filesystem::path const &fileName, bool append = false) noexcept;

    /**
     * Load a YAML based scene file.
     * @param fileName Name of the scene file to load.
     * @return True if operation completed successfully.
     */
    [[nodiscard]] bool loadSceneYAML(std::filesystem::path const &fileName) noexcept;

    /**
     * Load a GLTF based scene file.
     * @param fileName Name of the scene file to load.
     * @return True if operation completed successfully.
     */
    [[nodiscard]] bool loadSceneGLTF(std::filesystem::path const &fileName) noexcept;

    /**
     * Create a default initialised scene.
     * @return True if successful, False otherwise.
     */
    [[nodiscard]] bool createBlankScene() noexcept;

    /**
     * Generate a filtered cube map based on an input panoramic image texture
     * @param fileName Name of the panchromatic environment image to load.
     * @return True if succeeded, false otherwise.
     */
    bool generateEnvironmentMap(std::filesystem::path const &fileName) noexcept;

    /**
     * Update scene state for any changes.
     */
    void updateScene() noexcept;

    /**
     * Update any existing animations.
     */
    void updateSceneAnimations() noexcept;

    /**
     * Generate camera matrices based on currently active scene camera.
     */
    void updateSceneCameraMatrices() noexcept;

    /**
     * Update geometry buffers based on current scene settings.
     */
    void updateSceneMeshes() noexcept;

    /**
     * Update instance buffer based on current scene settings.
     */
    void updateSceneInstances() noexcept;

    /**
     * Update transform buffer based on current scene settings.
     */
    void updateSceneTransforms() noexcept;

    /**
     * Update material buffer and texture atlas.
     */
    void updateSceneMaterials() noexcept;

    /**
     * Updates vertex buffers with the results of any skinned or morph target animation.
     * @return True if animation caused buffers to be modified, False otherwise.
     */
    [[nodiscard]] bool updateSceneAnimatedGeometry() noexcept;

    /**
     * Update acceleration structures based on current scene settings.
     */
    void updateSceneBVH(bool animationGPUUpdated) noexcept;

    void dumpTexture(std::filesystem::path const &filePath, GfxTexture const &texture);
    void saveImage(GfxBuffer const &dumpBuffer, DXGI_FORMAT bufferFormat, uint32_t dumpBufferWidth,
        uint32_t dumpBufferHeight, std::filesystem::path const &filePath);
    void saveEXR(GfxBuffer const &dumpBuffer, DXGI_FORMAT bufferFormat, uint32_t dumpBufferWidth,
        uint32_t dumpBufferHeight, std::filesystem::path const &filePath);
    void saveJPG(GfxBuffer const &dumpBuffer, DXGI_FORMAT bufferFormat, uint32_t dumpBufferWidth,
        uint32_t dumpBufferHeight, std::filesystem::path const &filePath) const;
    void dumpCamera(CameraMatrices const &cameraMatrices, float cameraJitterX, float cameraJitterY,
        std::filesystem::path const &filePath) const;

    struct InstanceSourceInfo
    {
        uint32_t vertex_source_offset_idx;
        uint32_t joints_offset;
        uint32_t weights_offset;
        uint32_t targets_count;
    };

    size_t mesh_hash_                 = 0;
    size_t transform_hash_            = 0;
    size_t material_hash_             = 0;
    bool   render_dimensions_updated_ = false;
    bool   window_dimensions_updated_ = false;
    bool   mesh_updated_              = true;
    bool   transform_updated_         = true;
    bool   environment_map_updated_   = true;
    bool   scene_updated_             = true;
    bool   camera_changed_            = true;
    bool   camera_updated_            = true;
    bool   animation_updated_         = true;
    bool   materials_updated_         = true;
    bool   instances_updated_         = true;

    GfxContext  gfx_; /**< The graphics context to be used. */
    std::string shader_path_;
    std::string third_party_shader_path_;
    float render_scale_      = 1.0F; /**< The ratio between render resolution and display/window resolution */
    uint2 render_dimensions_ = uint2(0); /**< The normal rendering resolution */
    uint2 window_dimensions_ =
        uint2(0); /**< The resolution of the display window (may not exist if running headless) */
    RenderOptions render_options;

    GfxScene                           scene_; /**< The scene to be rendered. */
    GfxTexture                         environment_buffer_;
    std::vector<std::filesystem::path> scene_files_;
    std::filesystem::path              environment_map_file_;
    uint2 environment_map_source_dimensions_ {}; /** Original size of source envMap */

    uint32_t frame_index_ =
        std::numeric_limits<uint32_t>::max(); /**< Current frame number (incremented each render call) */
    double current_time_ = 0.0;               /**< Current wall clock time used for timing (seconds) */
    double frame_time_   = 0.0;               /**< Elapsed frame time for most recent frame (seconds) */

    bool   play_paused_           = true;  /**< Current animation play/paused state (True if paused) */
    bool   play_fixed_framerate_  = false; /**< Current animation playback mode (True if fixed frame rate) */
    double play_time_             = 0.0F;  /**< Current animation absolute playback position (s) */
    double play_time_old_         = -1.0F; /**< Previous animation absolute playback position (s) */
    double play_fixed_frame_time_ = 1.0F / 30.0F; /**< Frame time used with fixed frame rate mode */
    double play_speed_            = 1.0F;         /**< Current playback speed */
    bool   play_rewind_           = false;        /**< Current rewind state (True if rewinding) */
    bool   render_paused_ = false; /**< Current render paused state (True to pause rendering of new frames) */

    CameraMatrices camera_matrices_[2] {}; /**< Un-jittered and jittered matrices */
    uint32_t       jitter_frame_index_ =
        ~0U; /**< Current jitter frame number (only used for overriding normal frame index) */
    uint32_t  jitter_phase_count_ = 16; /**< Current jitter phase used to modulo jitter index */
    float2    camera_jitter_ {};        /**< Jitter applied to camera matrices (x, y) respectively */
    GfxCamera camera_prev_;             /**< Camera used in the previous frame */

    RenderOptionList options_; /**< Options for controlling the operation of each render technique */

    std::vector<std::unique_ptr<RenderTechnique>>
        render_techniques_; /**< The list of render techniques to be applied. */
    std::map<std::string_view /*name*/, std::shared_ptr<Component>>
                              components_;         /**< The list of render techniques to be applied. */
    std::string_view          renderer_name_;      /**< Currently used renderer string name */
    std::unique_ptr<Renderer> renderer_ = nullptr; /**< Currently used renderer */
    using DebugViews                    = std::vector<std::pair<std::string_view, bool>>;
    DebugViews       debug_views_; /**< List of available debug views */
    std::string_view debug_view_;  /**< The debug view to use (get available from GetDebugViews() -
                                               "None" or empty for default behaviour) */
    GfxTexture currentView;        /**< Current view being displayed */

    GfxKernel  blit_kernel_;  /**< The kernel to blit the color buffer to the back buffer. */
    GfxProgram blit_program_; /**< The program to blit the color buffer to the back buffer. */
    GfxKernel  debug_depth_kernel_;
    GfxProgram debug_depth_program_;

    using SharedTexturesList = std::vector<std::pair<std::string_view /*name*/, GfxTexture>>;
    using TextureBackupList  = std::vector<std::pair<uint32_t /*Source*/, uint32_t /*Destination*/>>;
    using TextureClearList   = std::vector<uint32_t>;
    SharedTexturesList
        shared_textures_; /**< The list of shared textures populated by the render techniques. */
    TextureBackupList backup_shared_textures_; /**< The list of shared textures to back up each frame */
    TextureClearList  clear_shared_textures_;  /**< List of shared textures to clear each frame */
    using SharedBuffersList = std::vector<std::pair<std::string_view, GfxBuffer>>;
    SharedBuffersList shared_buffers_;       /**< The list of buffers populated by the render techniques. */
    TextureClearList  clear_shared_buffers_; /**< List of shared buffers to clear each frame */
    GfxBuffer         constant_buffer_pools_[kGfxConstant_BackBufferCount];
    uint64_t          constant_buffer_pool_cursor_ = 0;

    GfxBuffer             camera_matrices_buffer_[2]; /**< Un-jittered and jittered camera matrices */
    std::vector<Instance> instance_data_;
    GfxBuffer             instance_buffer_;
    std::vector<std::pair<glm::vec3, glm::vec3>> instance_bounds_;
    std::vector<uint32_t>                        instance_id_data_;
    GfxBuffer                                    instance_id_buffer_;
    GfxBuffer                                    transform_buffer_;
    GfxBuffer                                    prev_transform_buffer_;
    bool                                         transform_updated_last_frame = false;
    GfxBuffer                                    material_buffer_;
    std::vector<GfxTexture>                      texture_atlas_;
    GfxSamplerState                              linear_sampler_;
    GfxSamplerState                              linear_wrap_sampler_;
    GfxSamplerState                              nearest_sampler_;
    GfxSamplerState                              anisotropic_sampler_;
    GfxBuffer index_buffer_;  /**< The buffer storing all indices so it can be accessed via RT. */
    GfxBuffer vertex_buffer_; /**< The buffer storing all vertices so it can be accessed via RT. */
    uint32_t  vertex_data_index_      = 0; /**< Animated vertices data frame index for the current frame. */
    uint32_t  prev_vertex_data_index_ = 0; /**< Animated vertices data frame index for the previous frame. */
    GfxBuffer vertex_source_buffer_;       /**< The buffer storing vertices source data for animation. */
    GfxBuffer morph_weight_buffer_;        /**< The buffer storing weights for morph targets animation. */
    GfxBuffer joint_buffer_;               /**< The buffer storing per vertex joint indices and weights. */
    std::vector<uint32_t>           joint_matrices_offsets_;
    GfxBuffer                       joint_matrices_buffer_; /**< The buffer storing joint matrices. */
    std::vector<InstanceSourceInfo> instance_source_info_data_;

    struct MeshInfo
    {
        uint vertex_offset_idx[2];
        uint index_offset_idx;
        uint index_count;
        uint vertex_source_offset_idx;
        uint joints_offset;
        uint targets_count;
        uint vertex_count;
        uint meshlet_count;      /**< Number of meshlets in mesh */
        uint meshlet_offset_idx; /**< Absolute offset into Meshlet buffer for first meshlet */
        bool is_animated;
    };

    std::vector<MeshInfo>               mesh_infos_;
    GfxAccelerationStructure            acceleration_structure_;
    std::vector<GfxRaytracingPrimitive> raytracing_primitives_;
    uint32_t                            sbt_stride_in_entries_[kGfxShaderGroupType_Count] = {};

    // Scene statistics for currently loaded scene
    uint32_t triangle_count_ = 0;

    Graph frameGraph; /**< The stored frame history graph */

    std::deque<std::tuple<GfxBuffer, DXGI_FORMAT, uint32_t /*width*/, uint32_t /*height*/,
        std::filesystem::path, uint32_t /*remainingDelay*/>>
               dump_in_flight_buffers_; /**< In flight dumpDebugView requests */
    GfxKernel  generate_animated_vertices_kernel_;
    GfxProgram generate_animated_vertices_program_;
};
} // namespace Capsaicin
