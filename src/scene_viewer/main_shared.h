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

#include <array>
#include <capsaicin.h>
#include <cinttypes>
#include <gfx_window.h>
#include <glm/glm.hpp>
#include <string_view>

class CapsaicinMain
{
public:
    /**
     * Default constructor.
     * @param programName Name of the current program (used for window decoration and identification).
     */
    CapsaicinMain(std::string_view &&programName) noexcept;

    CapsaicinMain() = delete;

    CapsaicinMain(CapsaicinMain &other) = delete;

    /** Default destructor. */
    ~CapsaicinMain() noexcept;

    /**
     * Run the internal program loop.
     * @note This initialises required settings and then runs the frame loop
     * until such time as a user input requests a shutdown
     * @return Boolean signaling if no error occurred.
     */
    bool run() noexcept;

protected:
    /**
     * Print a string to an output console or debugger window if one is available.
     * @note If a debugger is attached then the string will be output to the debug console, else if the
     * program was launched from a terminal window then it will use that. If neither of these then the text
     * will not be displayed.
     * @param text The string to be printed.
     */
    void printString(std::string const &text) noexcept;

    /**
     * Initialise internal capsaicin data.
     * @return Boolean signaling if no error occurred.
     */
    bool initialise() noexcept;

    /**
     * Resets internal Capsaicin state.
     * @return Boolean signaling if no error occurred.
     */
    bool reset() noexcept;

    /**
     * Load the scene file corresponding to the currently set scene.
     * @return Boolean signaling if no error occurred.
     */
    bool loadScene() noexcept;

    /**
     * Set the current camera data to the currently set cameraIndex.
     */
    void setCamera() noexcept;

    /**
     * Update scene and render settings based on the currently requested environment map.
     * @return Boolean signaling if no error occurred.
     */
    bool setEnvironmentMap() noexcept;

    /**
     * Update render settings based on the currently set renderer.
     */
    void setRenderer() noexcept;

    /**
     * Set scene specific render options.
     * @param force (Optional) True to force overwrite any existing values.
     */
    void setSceneRenderOptions(bool force = false) noexcept;

    /**
     * Set the current animation play mode.
     * @param playMode The type of animation mode to use.
     */
    void setPlayMode(Capsaicin::PlayMode playMode) noexcept;

    /**
     * Set current animation state.
     * @param animate True to enable animations, False otherwise.
     */
    void setAnimation(bool animate) noexcept;

    /**
     * Toggle the current animation state.
     */
    void toggleAnimation() noexcept;

    /**
     * Reset current animation state to beginning.
     */
    void restartAnimation() noexcept;

    /**
     * Get the current animation state.
     * @return Boolean signaling if animation is currently enabled.
     */
    bool getAnimation() noexcept;

    /**
     * Update animation state for current frame.
     */
    void tickAnimation() noexcept;

    /**
     * Get the current frame within the current animation sequence.
     * @return The current animation frame index.
     */
    uint32_t getCurrentAnimationFrame() noexcept;

    /**
     * Update render settings based on the currently set renderer.
     * @return Boolean signaling if no error occurred.
     */
    bool renderFrame() noexcept;

    /**
     * Perform operations to display the default GUI.
     * @return Boolean signaling if no error occurred.
     */
    bool renderGUI() noexcept;

    /**
     * Perform operations to display additional UI elements for camera control.
     * @return Boolean signaling if no error occurred.
     */
    bool renderCameraDetails() noexcept;

    /**
     * Perform operations to display additional GUI elements.
     * @note Called from within @renderGUI. This can be overridden to display
     * alternate UI information.
     */
    void renderGUIDetails() noexcept;

    /**
     * Perform operations to display profiling GUI elements.
     * @note Called from within CapsaicinMain::renderGUI.
     */
    void renderProfiling() noexcept;

    /**
     * Perform operations to display all internal render options.
     * @note Called from within CapsaicinMain::renderGUIDetails.
     */
    void renderOptions() noexcept;

    /**
     * Save the currently displayed frame to disk.
     * @note Saves to default 'dump' subdirectory.
     */
    void saveFrame() noexcept;

    enum class Scene : uint32_t
    {
        FlyingWorld = 0,
        GasStation,
        TropicalBedroom,
    };

    enum class EnvironmentMap : uint32_t
    {
        None = 0,
        PhotoStudioLondonHall,
        KiaraDawn,
        NagoyaWallPath,
        SpaichingenHill,
        StudioSmall,
        White,
        Atmosphere,
    };

    GfxWindow                 window;         /**< Gfx window class */
    GfxContext                contextGFX;     /**< Gfx context */
    Capsaicin::RenderSettings renderSettings; /**< Render settings used to control rendering */
    GfxScene                  sceneData;      /**< Current scene data */
    Scene scene = Scene::TropicalBedroom; /**< Index of currently selected scene (indexes into internal list) */
    EnvironmentMap environmentMap = EnvironmentMap::KiaraDawn; /**< Currently selected environment map */
    uint32_t       cameraIndex = 1; /**< Index of currently used camera (indexes into scenes camera list) */
    GfxRef<GfxCamera> camera;       /**< Handle to internal gfx camera for currently used camera */
    float             cameraSpeed       = 1.2f;            /**< Camera speed (m/s) used for camera movement */
    glm::vec3         cameraTranslation = glm::vec3(0.0f); /**< Camera translation velocity (m/s) */
    glm::vec2         cameraRotation    = glm::vec2(0.0f); /**< Camera rotation velocity (m/s) */
    double            previousTime      = 0.0; /**< Previous wall clock time used for timing (seconds) */
    double            currentTime       = 0.0; /**< Current wall clock time used for timing (seconds) */
    float             frameTime         = 0.0; /**< Elapsed frame time for most recent frame (seconds) */
    std::string_view  programName;             /**< Stored name for the current program */
    bool benchmarkMode = false; /**< If enabled this prevents user inputs and runs a predefined benchmark */
    uint32_t benchmarkModeFrameCount =
        512; /**< The number of frames to be rendered during benchmarking mode */
    bool updateScene          = false;
    bool updateEnvironmentMap = false;
    bool updateCamera         = false;
    bool updateRenderer       = false;
    bool reenableToneMap = false; /**< Used to re-enable Tonemapping after a frame has been saved to disk */

    // Scene statistics for currently loaded scene
    uint32_t triangleCount = 0;

    class Graph
    {
    public:
        Graph() noexcept = default;

        uint32_t getValueCount() const noexcept;
        void     addValue(float value) noexcept;
        float    getLastAddedValue() const noexcept;
        float    getValueAtIndex(uint32_t index) const noexcept;
        float    getAverageValue() noexcept;
        void     reset() noexcept;

        static float GetValueAtIndex(void *object, int32_t index) noexcept;

    private:
        uint32_t               current = 0;      /**< The current cursor into values circular buffer */
        std::array<float, 256> values  = {0.0f}; /**< The stored list of values */
    };

    Graph frameGraph; /**< The stored frame history graph */
    std::pair<std::string_view, std::string_view>
        selectedProfile; /**< Currently selected technique used in renderProfiling */

    bool hasConsole = false; /**< Set if a console output terminal is attached */
};
