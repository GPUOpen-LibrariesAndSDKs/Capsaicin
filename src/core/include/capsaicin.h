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
#pragma once

#include "capsaicin_export.h"

#define NOMINMAX
#include "gfx_scene.h"

#include <map>
#include <string_view>
#include <variant>

namespace Capsaicin
{
enum PlayMode
{
    kPlayMode_None = 0,
    kPlayMode_FrameByFrame,
    kPlayMode_Count
};

CAPSAICIN_EXPORT extern char const *g_play_modes[];

struct RenderSettings
{
    PlayMode play_mode_           = kPlayMode_None;
    uint32_t play_to_frame_index_ = 1;
    bool     play_from_start_     = false;

    float delta_time_                = 0.0f; // Use system time by default
    float frame_by_frame_delta_time_ = 1.0f / 30.0f;

    GfxConstRef<GfxImage> environment_map_;

    std::string_view renderer_;   /**< The requested renderer to use (get available from @GetRenderers()) */
    std::string_view debug_view_; /**< The debug view to use (get available from GetDebugViews() -
                                              "None" or empty for default behaviour) */

    using option = std::variant<bool, uint32_t, int32_t, float>;
    std::map<std::string_view, option>
        options_; /**< Options for controlling the operation of each render technique */

    /**
     * Checks if an options exists with the specified type.
     * @tparam T Generic type parameter of the requested option.
     * @param name The name of the option to get.
     * @returns True if options is found and has correct type, False otherwise.
     */
    template<typename T>
    bool hasOption(std::string_view const &name) const noexcept
    {
        if (auto i = options_.find(name); i != options_.end())
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
     * Sets an options value in the internal options list.
     * If the option does not exists it is created.
     * @tparam T Generic type parameter of the requested option.
     * @param name  The name of the option to set.
     * @param value The new value of the option.
     */
    template<typename T>
    void setOption(std::string_view const &name, const T value) noexcept
    {
        if (auto i = options_.find(name); i != options_.end())
        {
            if (std::holds_alternative<T>(i->second))
            {
                *std::get_if<T>(&(i->second)) = value;
            }
        }
        else
        {
            options_.emplace(name, value);
        }
    }
};

/**
 * Initializes Capsaicin. Must be called before any other functions.
 * @param gfx The gfx context to use inside Capsaicin.
 */
CAPSAICIN_EXPORT void Initialize(GfxContext gfx);

/**
 * Gets the list of supported renderers that can be set inside RenderSettings.
 * @returns The renderers list.
 */
CAPSAICIN_EXPORT std::vector<std::string_view> GetRenderers() noexcept;

/**
 * Gets the list of currently available AOVs.
 * @returns The AOV list.
 */
CAPSAICIN_EXPORT std::vector<std::string_view> GetAOVs() noexcept;

/**
 * Gets the list of currently available debug views.
 * @returns The debug view list.
 */
CAPSAICIN_EXPORT std::vector<std::string_view> GetDebugViews() noexcept;

/**
 * Render the current frame.
 * @param          scene           The scene to render.
 * @param [in,out] render_settings The render settings to use during rendering.
 */
CAPSAICIN_EXPORT void     Render(GfxScene scene, RenderSettings &render_settings);
CAPSAICIN_EXPORT uint32_t GetFrameIndex();
CAPSAICIN_EXPORT double   GetSequenceTime();
CAPSAICIN_EXPORT void     SetSequenceTime(double time);
CAPSAICIN_EXPORT bool     GetAnimate();
CAPSAICIN_EXPORT void     SetAnimate(bool animation);

/**
 * Gets count of enabled delta lights (point,spot,direction) in current scene.
 * @returns The delta light count.
 */
CAPSAICIN_EXPORT uint32_t GetDeltaLightCount();

/**
 * Gets count of enabled area lights in current scene.
 * @returns The area light count.
 */
CAPSAICIN_EXPORT uint32_t GetAreaLightCount();

/**
 * Gets count of enabled environment lights in current scene.
 * @returns The environment light count.
 */
CAPSAICIN_EXPORT uint32_t GetEnvironmentLightCount();

struct TimeStamp
{
    std::string_view name_; /**< The name of the timestamp */
    float            time_; /**< The time in seconds */
};

struct NodeTimestamps
{
    std::string_view       name_;     /**< The name of current timestamp node */
    std::vector<TimeStamp> children_; /**< The list of timestamps for all child timestamps (The first entry is
                                         the timestamp for the whole node) */
};

/**
 * Gets the profiling information for each timed section from the current frame.
 * @returns The total frame time as well as timestamps for each sub-section (see NodeTimestamps for details).
 */
CAPSAICIN_EXPORT std::pair<float /*total frame time*/, std::vector<NodeTimestamps>> GetProfiling() noexcept;

/** Terminates this object. Should be called after all other operations. */
CAPSAICIN_EXPORT void Terminate();

/**
 * Saves an AOV buffer to disk.
 * @param file_path Full pathname to the file to save as.
 * @param aov       The buffer to save (get available from @GetAOVs()).
 */
CAPSAICIN_EXPORT void DumpAOVBuffer(char const *file_path, std::string_view const &aov);
} // namespace Capsaicin
