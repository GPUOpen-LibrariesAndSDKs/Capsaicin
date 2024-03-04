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

    enum class MessageLevel : uint32_t
    {
        Debug,
        Info,
        Warning,
        Error,
    };

    /**
     * Print a string to an output console or debugger window if one is available.
     * @note If a debugger is attached then the string will be output to the debug console, else if the
     * program was launched from a terminal window then it will use that. If neither of these then the text
     * will not be displayed. When message level is error then a message box will be displayed in addition
     * to attempting to print to debugger/console.
     * @param text The string to be printed.
     * @param level Logging level of current message.
     */
    void printString(std::string const &text, MessageLevel level = MessageLevel::Info) noexcept;

    /**
     * Initialise internal capsaicin data.
     * @return Boolean signaling if no error occurred.
     */
    [[nodiscard]] bool initialise() noexcept;

    /**
     * Load the scene file corresponding to the currently set scene.
     * @param scene The scene to load.
     * @return Boolean signaling if no error occurred.
     */
    [[nodiscard]] bool loadScene(Scene scene) noexcept;

    /**
     * Set the current camera data to the currently set cameraIndex.
     * @param camera The camera to load.
     */
    void setCamera(std::string_view camera) noexcept;

    /**
     * Update scene and render settings based on the currently requested environment map.
     * @param environmentMap The environment map to load.
     * @return Boolean signaling if no error occurred.
     */
    [[nodiscard]] bool setEnvironmentMap(EnvironmentMap environmentMap) noexcept;

    /**
     * Update render settings based on the currently set renderer.
     * @param renderer The renderer to load.
     */
    [[nodiscard]] bool setRenderer(std::string_view renderer) noexcept;

    /**
     * Update render settings based on the currently set renderer.
     * @return Boolean signaling if no error occurred.
     */
    [[nodiscard]] bool renderFrame() noexcept;

    /**
     * Perform operations to display the default GUI.
     * @return Boolean signaling if no error occurred.
     */
    bool renderGUI() noexcept;

    /**
     * Perform operations to display additional UI elements for camera control.
     */
    void renderCameraDetails() noexcept;

    /**
     * Perform operations to display additional GUI elements.
     * @note Called from within @renderGUI. This can be overridden to display
     * alternate UI information.
     * @return Boolean signaling if no error occurred.
     */
    bool renderGUIDetails() noexcept;

    /**
     * Save the currently displayed frame to disk.
     * @note Saves to default 'dump' subdirectory.
     */
    void saveFrame() noexcept;

    /**
     * Get the common base file name based on current capsaicin settings.
     * @return String containing base name.
     */
    std::string getSaveName() const noexcept;

    static constexpr auto defaultScene          = Scene::FlyingWorld;
    static constexpr auto defaultEnvironmentMap = EnvironmentMap::KiaraDawn;
    static constexpr auto defaultRenderer       = "GI-1.1";

    GfxWindow        window;                       /**< Gfx window class */
    GfxContext       contextGFX;                   /**< Gfx context */
    float            cameraSpeed           = 1.2f; /**< Camera speed (m/s) used for camera movement */
    glm::vec3        cameraTranslation     = glm::vec3(0.0f);       /**< Camera translation velocity (m/s) */
    glm::vec2        cameraRotation        = glm::vec2(0.0f);       /**< Camera rotation velocity (m/s) */
    Scene            currentScene          = defaultScene;          /**< Currently loaded scene */
    EnvironmentMap   currentEnvironmentMap = defaultEnvironmentMap; /**< Currently loaded environment map */
    std::string_view programName; /**< Stored name for the current program */
    bool benchmarkMode = false;   /**< If enabled this prevents user inputs and runs a predefined benchmark */
    uint32_t benchmarkModeFrameCount =
        512; /**< The number of frames to be rendered during benchmarking mode */
    uint32_t benchmarkModeStartFrame = uint32_t(-1); /**< The first frame to start saving images at in
                                                        benchmark mode (default is just the last frame) */
    std::string benchmarkModeSuffix;                 /**< String appended to any saved files */
    bool        saveAsJPEG = false;                  /**< File type selector for dump frame */
    bool reenableToneMap   = false; /**< Used to re-enable Tonemapping after a frame has been saved to disk */
    bool reDisableRender   = false; /**< Use to render only a single frame at a time */

    bool hasConsole = false; /**< Set if a console output terminal is attached */
};
