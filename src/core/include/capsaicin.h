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

#include "capsaicin_export.h"

#include <filesystem>
#include <gfx_imgui.h>
#include <glm/glm.hpp>
#include <map>
#include <string_view>
#include <variant>

namespace Capsaicin
{
/**
 * Initializes Capsaicin. Must be called before any other functions.
 * @param gfx The gfx context to use inside Capsaicin.
 * @param imgui_context (Optional) The ImGui context.
 */
CAPSAICIN_EXPORT void Initialize(GfxContext const &gfx, ImGuiContext *imgui_context = nullptr) noexcept;

/**
 * Gets the list of supported renderers.
 * @return The renderers list.
 */
CAPSAICIN_EXPORT std::vector<std::string_view> GetRenderers() noexcept;

/**
 * Gets the currently set renderer.
 * @return The current renderer name.
 */
CAPSAICIN_EXPORT std::string_view GetCurrentRenderer() noexcept;

/**
 * Sets the current renderer.
 * @param name The name of the renderer to set (must be one of the options from GetRenderers()).
 * @return True if successful, False otherwise.
 */
CAPSAICIN_EXPORT bool SetRenderer(std::string_view const &name) noexcept;

/**
 * Gets the currently set scenes.
 * @return The current scene name.
 */
CAPSAICIN_EXPORT std::vector<std::filesystem::path> GetCurrentScenes() noexcept;

/**
 * Sets the current scene and replaces any existing scene(s).
 * @param fileName The name of the scene file.
 * @return True if successful, False otherwise.
 */
CAPSAICIN_EXPORT bool SetScene(std::filesystem::path const &fileName) noexcept;

/**
 * Append the contents of a scene file to the existing scene(s).
 * @param fileName The name of the scene file.
 * @return True if successful, False otherwise.
 */
CAPSAICIN_EXPORT bool AppendScene(std::filesystem::path const &fileName) noexcept;

/**
 * Gets the list of cameras available in the current scene.
 * @return The cameras list.
 */
CAPSAICIN_EXPORT std::vector<std::string_view> GetSceneCameras() noexcept;

/**
 * Gets the name of the currently set scene camera.
 * @return The current camera name.
 */
CAPSAICIN_EXPORT std::string_view GetSceneCurrentCamera() noexcept;

struct CameraView
{
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 up;
};

/**
 * Gets the current scenes active cameras view data.
 * @return The requested camera data [position, forward, up].
 */
CAPSAICIN_EXPORT CameraView GetSceneCameraView() noexcept;

/**
 * Gets the current scenes active camera vertical Field Of View.
 * @return The requested camera FOVY.
 */
CAPSAICIN_EXPORT float GetSceneCameraFOV() noexcept;

/**
 * Gets the current scenes active camera near and far Z range.
 * @return The requested camera range.
 */
CAPSAICIN_EXPORT glm::vec2 GetSceneCameraRange() noexcept;

/**
 * Sets the current scenes camera.
 * @param name The name of the camera to set (must be one of the options from GetSceneCameras()).
 * @return True if successful, False otherwise.
 */
CAPSAICIN_EXPORT bool SetSceneCamera(std::string_view const &name) noexcept;

/**
 * Sets the currently active cameras view data.
 * @param position The new camera position.
 * @param forward  The new camera forward direction vector.
 * @param up       The new camera up direction vector.
 */
CAPSAICIN_EXPORT void SetSceneCameraView(
    glm::vec3 const &position, glm::vec3 const &forward, glm::vec3 const &up) noexcept;

/**
 * Sets the currently active cameras vertical Field Of View.
 * @param FOVY The vertical filed of view angle.
 */
CAPSAICIN_EXPORT void SetSceneCameraFOV(float FOVY) noexcept;

/**
 * Sets the currently active cameras near and far Z range.
 * @param nearFar The near and far Z distances.
 */
CAPSAICIN_EXPORT void SetSceneCameraRange(glm::vec2 const &nearFar) noexcept;

/**
 * Gets the currently set environment map.
 * @return The current environment map name.
 */
CAPSAICIN_EXPORT std::filesystem::path GetCurrentEnvironmentMap() noexcept;

/**
 * Sets the current scene environment map.
 * @param fileName The name of the image file (blank to disable environment map).
 * @return True if successful, False otherwise.
 */
CAPSAICIN_EXPORT bool SetEnvironmentMap(std::filesystem::path const &fileName) noexcept;

/**
 * Gets the list of currently available debug views.
 * @return The debug view list.
 */
CAPSAICIN_EXPORT std::vector<std::string_view> GetDebugViews() noexcept;

/**
 * Gets the currently set debug view.
 * @return The current debug view name.
 */
CAPSAICIN_EXPORT std::string_view GetCurrentDebugView() noexcept;

/**
 * Sets the current debug view.
 * @param name The name of the debug view to set (must be one of the options from GetDebugViews()).
 * @return True if successful, False otherwise.
 */
CAPSAICIN_EXPORT bool SetDebugView(std::string_view const &name) noexcept;

/**
 * Gets the list of currently available AOVs.
 * @return The AOV list.
 */
CAPSAICIN_EXPORT std::vector<std::string_view> GetAOVs() noexcept;

/**
 * Render the current frame.
 */
CAPSAICIN_EXPORT void Render() noexcept;

/**
 * Render UI elements related to current internal state
 * Must be called between ImGui::Begin() and ImGui::End().
 * @param readOnly (Optional) True to only display read only data, False to display controls accepting user
 * input.
 */
CAPSAICIN_EXPORT void RenderGUI(bool readOnly = false) noexcept;

/**
 * Get the index of the most recent frame (starts at zero).
 * @return The index of the current/last frame rendered.
 */
CAPSAICIN_EXPORT uint32_t GetFrameIndex() noexcept;

/**
 * Get the elapsed time since the last render call.
 * @return The elapsed frame time (seconds)
 */
CAPSAICIN_EXPORT double GetFrameTime() noexcept;

/**
 * Get the average frame time.
 * @return The elapsed frame time (seconds)
 */
CAPSAICIN_EXPORT double GetAverageFrameTime() noexcept;

/**
 * Check if the current scene has any usable animations.
 * @return True if animations are present, False otherwise.
 */
CAPSAICIN_EXPORT bool HasAnimation() noexcept;

/**
 * Set the current playback play/paused state
 * @param paused True to pause animation, False to play.
 */
CAPSAICIN_EXPORT void SetPaused(bool paused) noexcept;

/**
 * Get the current animation play/paused state.
 * @return True if playback is paused, False otherwise.
 */
CAPSAICIN_EXPORT bool GetPaused() noexcept;

/**
 * Set the current playback mode.
 * @param playMode The new playback mode (False to playback in real-time mode, True uses fixed frame
 * rate).
 */
CAPSAICIN_EXPORT void SetFixedFrameRate(bool playMode) noexcept;

/**
 * Set the current fixed rate frame time.
 * @param fixed_frame_time A duration in seconds.
 */
CAPSAICIN_EXPORT void SetFixedFrameTime(double fixed_frame_time) noexcept;

/**
 * Get current playback mode.
 * @return True if using fixed frame rate, False is using real-time.
 */
CAPSAICIN_EXPORT bool GetFixedFrameRate() noexcept;

/**
 * Restart playback to start of animation.
 */
CAPSAICIN_EXPORT void RestartPlayback() noexcept;

/**
 * Increase current playback speed by double.
 */
CAPSAICIN_EXPORT void IncreasePlaybackSpeed() noexcept;

/**
 * Decrease current playback speed by half.
 */
CAPSAICIN_EXPORT void DecreasePlaybackSpeed() noexcept;

/**
 * Get the current playback speed.
 * @return The current playback speed.
 */
CAPSAICIN_EXPORT double GetPlaybackSpeed() noexcept;

/**
 * Reset the playback speed to default.
 */
CAPSAICIN_EXPORT void ResetPlaybackSpeed() noexcept;

/**
 * Step playback forward by specified number of frames.
 * @param frames The number of frames to step forward.
 */
CAPSAICIN_EXPORT void StepPlaybackForward(uint32_t frames) noexcept;

/**
 * Step playback backward by specified number of frames.
 * @param frames The number of frames to step backward.
 */
CAPSAICIN_EXPORT void StepPlaybackBackward(uint32_t frames) noexcept;

/**
 * Set the playback to forward/rewind.
 * @param rewind Set to True to rewind, False to playback forward.
 */
CAPSAICIN_EXPORT void SetPlayRewind(bool rewind) noexcept;

/**
 * Get the current playback forward/rewind state
 * @return True if in rewind, False if forward.
 */
CAPSAICIN_EXPORT bool GetPlayRewind() noexcept;

/**
 * Set the current render state. Pausing prevents any new frames from being rendered.
 * @param paused True to pause rendering.
 */
CAPSAICIN_EXPORT void SetRenderPaused(bool paused) noexcept;

/**
 * Get the current render paused state.
 * @return True if rendering is paused, False otherwise.
 */
CAPSAICIN_EXPORT bool GetRenderPaused() noexcept;

/**
 * Set camera jitter to manual and step jitter frame index by a specified number of frames.
 * @param frames The number of frames to step.
 */
CAPSAICIN_EXPORT void StepJitterFrameIndex(uint32_t frames);

/**
 * Set camera jitter sequence length.
 * @param length The length of the Halton sequence used to calculate jitter.
 */
CAPSAICIN_EXPORT void SetCameraJitterPhase(uint32_t length);

/**
 * Gets count of enabled delta lights (point,spot,direction) in current scene.
 * @return The delta light count.
 */
CAPSAICIN_EXPORT uint32_t GetDeltaLightCount() noexcept;

/**
 * Gets count of enabled area lights in current scene.
 * @return The area light count.
 */
CAPSAICIN_EXPORT uint32_t GetAreaLightCount() noexcept;

/**
 * Gets count of enabled environment lights in current scene.
 * @return The environment light count.
 */
CAPSAICIN_EXPORT uint32_t GetEnvironmentLightCount() noexcept;

/**
 * Gets count of number of triangles present in current scene.
 * @return The triangle count.
 */
CAPSAICIN_EXPORT uint32_t GetTriangleCount() noexcept;

/**
 * Gets size of the acceleration structure (in bytes).
 * @return The acceleration structure size.
 */
CAPSAICIN_EXPORT uint64_t GetBvhDataSize() noexcept;

/**
 * Gets the dimensions/resolution of the currently active window.
 * @return The window width and height.
 */
CAPSAICIN_EXPORT std::pair<uint32_t, uint32_t> GetWindowDimensions() noexcept;

/**
 * Gets the current render dimensions/resolution.
 * This may differ from window resolution when using render scaling or other non 1:1 render resolutions.
 * @return The render width and height.
 */
CAPSAICIN_EXPORT std::pair<uint32_t, uint32_t> GetRenderDimensions() noexcept;

/**
 * Set a scaling factor to differentiate between render resolution and window resolution.
 * A scaling factor of 2.0 would result in the render resolution being set to twice the window resolution
 * along each 2D axis (e.g. 1920x1080 will become 3840x2160). While a value of 0.5 will result in half the
 * resolution along each axis. In cases where the scaling factor does not evenly divide into the window
 * resolution then the render resolution will be rounded to the nearest pixel.
 * @param scale The scaling factor to apply to window resolution to get render resolution.
 */
CAPSAICIN_EXPORT void SetRenderDimensionsScale(float scale) noexcept;

/**
 * Gets the internal configuration options.
 * @return The list of available options.
 */
CAPSAICIN_EXPORT
std::map<std::string_view, std::variant<bool, uint32_t, int32_t, float, std::string>> &GetOptions() noexcept;

/**
 * Checks if an options exists with the specified type.
 * @tparam T Generic type parameter of the requested option.
 * @param name The name of the option to get.
 * @return True if options is found and has correct type, False otherwise.
 */
template<typename T>
bool hasOption(std::string_view const &name) noexcept
{
    auto &options = GetOptions();
    if (auto const i = options.find(name); i != options.end())
    {
        return std::holds_alternative<T>(i->second);
    }
    return false;
}

/**
 * Gets a reference to an option from internal options list.
 * @tparam T Generic type parameter of the requested option.
 * @param name The name of the option to get.
 * @return The options value (uninitialised if option does not exist or typename does not match).
 */
template<typename T>
T &getOption(std::string_view const &name) noexcept
{
    auto &options = GetOptions();
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
    auto &options = GetOptions();
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

/** Terminates this object. Should be called after all other operations. */
CAPSAICIN_EXPORT void Terminate() noexcept;

/**
 * Reload all shader code currently in use
 */
CAPSAICIN_EXPORT void ReloadShaders() noexcept;

/**
 * Saves an debug view to disk.
 * @param file_path Full pathname to the file to save as.
 * @param view      The view to save (get available from GetDebugViews()).
 */
CAPSAICIN_EXPORT void DumpDebugView(
    std::filesystem::path const &file_path, std::string_view const &view) noexcept;

/**
 * Saves current camera attributes to disk.
 * @param file_path   Full pathname to the file to save as.
 * @param jittered    Jittered camera or not.
 */
CAPSAICIN_EXPORT void DumpCamera(std::filesystem::path const &file_path, bool jittered) noexcept;

} // namespace Capsaicin
