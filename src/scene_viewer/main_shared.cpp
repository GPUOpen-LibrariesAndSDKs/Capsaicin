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

#include "main_shared.h"

#include <CLI/CLI.hpp>
#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <numbers>
#include <ranges>
#include <version.h>

using namespace std;
using namespace glm;

enum class KeyboardMappings : uint8_t
{
    QWERTY = 0,
    AZERTY,
};

struct KeyboardMapping
{
    uint32_t upZ;
    uint32_t downZ;
    uint32_t upX;
    uint32_t downX;
    uint32_t upY;
    uint32_t downY;
};

constexpr array keyboardMappings = {
    KeyboardMapping {.upZ = 0x57 /*W*/,
                     .downZ            = 0x53 /*S*/,
                     .upX              = 0x44 /*D*/,
                     .downX            = 0x41 /*A*/,
                     .upY              = 0x45 /*E*/,
                     .downY            = 0x51 /*Q*/        },
    KeyboardMapping {.upZ = 0x5A /*Z*/,
                     .downZ            = 0x53 /*S*/,
                     .upX              = 0x44 /*D*/,
                     .downX            = 0x51 /*Q*/,
                     .upY              = 0x21 /*Page Up*/,
                     .downY            = 0x22 /*Page Down*/}
};

vector<CapsaicinMain::SceneData> const CapsaicinMain::scenes = {
    {  .name                 = "Breakfast Room",
     .fileName          = {"assets/CapsaicinTestMedia/breakfast_room/BreakfastRoom.gltf"},
     .useEnvironmentMap = true},
    {                    .name = "Flying World",
     .fileName =
     {"assets/CapsaicinTestMedia/flying_world_battle_of_the_trash_god/FlyingWorld-BattleOfTheTrashGod.gltf"},
     .useEnvironmentMap = true},
    {     .name                 = "Gas Station",
     .fileName          = {"assets/CapsaicinTestMedia/gas_station/GasStation.gltf"},
     .useEnvironmentMap = true},
    {.name                 = "Tropical Bedroom",
     .fileName          = {"assets/CapsaicinTestMedia/tropical_bedroom/TropicalBedroom.gltf"},
     .useEnvironmentMap = true},
    {          .name                 = "Sponza",
     .fileName          = {"assets/CapsaicinTestMedia/sponza/Sponza.gltf"},
     .useEnvironmentMap = true},
};

vector<pair<string_view, filesystem::path>> const CapsaicinMain::sceneEnvironmentMaps = {
    {                    "None",                                                                     ""},
    {"Photo Studio London Hall", "assets/CapsaicinTestMedia/environment_maps/PhotoStudioLondonHall.hdr"},
    {              "Kiara Dawn",             "assets/CapsaicinTestMedia/environment_maps/KiaraDawn.hdr"},
    {        "Nagoya Wall Path",        "assets/CapsaicinTestMedia/environment_maps/NagoyaWallPath.hdr"},
    {        "Spaichingen Hill",       "assets/CapsaicinTestMedia/environment_maps/SpaichingenHill.hdr"},
    {            "Studio Small",           "assets/CapsaicinTestMedia/environment_maps/StudioSmall.hdr"},
    {                   "White",                 "assets/CapsaicinTestMedia/environment_maps/White.hdr"},
    {              "Atmosphere",                                                           "Atmosphere"},
};

/** List of executable relative file paths to search for scene files */
static array<filesystem::path, 4> const sceneDirectories = {"", "../../../", "../../", "../"};

CapsaicinMain::CapsaicinMain(string_view &&programNameIn) noexcept
    : programName(forward<string_view>(programNameIn))
{}

CapsaicinMain::~CapsaicinMain() noexcept
{
    // Destroy Capsaicin context
    gfxImGuiTerminate();
    Capsaicin::Terminate();

    gfxDestroyContext(contextGFX);
    gfxDestroyWindow(window);

    // Detach from console
    if (hasConsole)
    {
        FreeConsole();
    }
}

bool CapsaicinMain::run() noexcept
{
    // Initialise the required data
    if (!initialise())
    {
        return false;
    }

    // Render frames continuously
    while (true)
    {
        if (!renderFrame())
        {
            break;
        }
    }

    if (benchmarkMode && !benchmarkModeSuffix.empty() && Capsaicin::hasOption<bool>("image_metrics_enable")
        && Capsaicin::getOption<bool>("image_metrics_enable")
        && Capsaicin::getOption<bool>("image_metrics_save_to_file"))
    {
        try
        {
            // Flush remaining stats
            for (uint32_t i = 0; i <= gfxGetBackBufferCount(contextGFX); ++i)
            {
                Capsaicin::Render();
                gfxFrame(contextGFX);
            }
            // Force finalising metrics file
            Capsaicin::setOption<bool>("image_metrics_enable", false);
            Capsaicin::Render();
            // Rename metrics file to also contain suffix
            auto const       savePath       = getSaveName();
            filesystem::path newMetricsFile = savePath;
            newMetricsFile += "_";
            newMetricsFile += benchmarkModeSuffix;
            newMetricsFile.replace_extension("csv");
            if (exists(newMetricsFile))
            {
                filesystem::remove(newMetricsFile.c_str());
            }
            filesystem::path metricsFile = savePath;
            metricsFile.replace_extension("csv");
            error_code ec;
            if (filesystem::rename(metricsFile.c_str(), newMetricsFile.c_str(), ec); !ec)
            {
                printString(
                    "Failed to rename image metrics file: "s + metricsFile.string(), MessageLevel::Warning);
            }
        }
        catch (...)
        {
            return false;
        }
    }

    return true;
}

void CapsaicinMain::printString(string const &text, MessageLevel const level) noexcept
{
    try
    {
        string outputText;
        switch (level)
        {
        case MessageLevel::Debug: [[fallthrough]];
        case MessageLevel::Info: break;
        case MessageLevel::Warning: outputText = "Warning: "; break;
        case MessageLevel::Error: outputText = "Error: "; [[fallthrough]];
        default: break;
        }
        outputText += text;

        // Check if a debugger is attached and use it instead of a console
        // If no debugger is attached then we need to attach to a console process in order to be able to
        // output text
        if (IsDebuggerPresent() != 0)
        {
            OutputDebugStringA(outputText.c_str());
        }
        else
        {
            if (!hasConsole)
            {
                // Look to see if there is a parent console (i.e. this was launched from terminal) and attach
                // to that
                if (AttachConsole(ATTACH_PARENT_PROCESS) != 0)
                {
                    // Set the screen buffer big enough to hold at least help text
                    CONSOLE_SCREEN_BUFFER_INFO scInfo;
                    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &scInfo);

                    // Force the console buffer to be resized no matter what as this forces the console to
                    // update the end line to match the number of printed lines from this app
                    constexpr int16_t minLength = 4096;
                    scInfo.dwSize.Y = glm::max(minLength, static_cast<int16_t>(scInfo.dwSize.Y + 100));
                    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), scInfo.dwSize);
                    hasConsole = true;
                }
            }
            if (hasConsole)
            {
                // The parent console has already printed a new user prompt before this program has even run
                // so need to insert any printed lines before the existing user prompt

                // Save current cursor position
                CONSOLE_SCREEN_BUFFER_INFO scInfo;
                GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &scInfo);
                auto const cursorPosY = scInfo.dwCursorPosition.Y;
                auto const cursorPosX = scInfo.dwCursorPosition.X;

                // Move to start of current line
                SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), {0, cursorPosY});

                // Insert new line into console buffer
                vector<CHAR_INFO> buffer;
                buffer.resize(static_cast<size_t>(cursorPosX));
                constexpr COORD coordinates = {};
                SMALL_RECT      textRegion  = {
                          .Left   = 0,
                          .Top    = cursorPosY,
                          .Right  = static_cast<short>(cursorPosX - 1),
                          .Bottom = cursorPosY,
                };
                const COORD bufferSize = {
                    .X = cursorPosX,
                    .Y = 1,
                };

                ReadConsoleOutputA(
                    GetStdHandle(STD_OUTPUT_HANDLE), buffer.data(), bufferSize, coordinates, &textRegion);
                DWORD dnc = 0;
                FillConsoleOutputCharacter(
                    GetStdHandle(STD_OUTPUT_HANDLE), ' ', cursorPosX, {0, cursorPosY}, &dnc);

                auto const lines = ranges::count(text, '\n') + ((text.back() == '\n') ? 0 : 1);
                WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), text.c_str(),
                    static_cast<DWORD>(text.length()), &dnc, nullptr);

                // Restore cursor position to previously saved state and increment by number of new lines
                textRegion = {
                    .Left   = 0,
                    .Top    = static_cast<short>(lines + cursorPosY),
                    .Right  = static_cast<short>(cursorPosX - 1),
                    .Bottom = static_cast<short>(lines + cursorPosY),
                };
                WriteConsoleOutputA(
                    GetStdHandle(STD_OUTPUT_HANDLE), buffer.data(), bufferSize, coordinates, &textRegion);
                SetConsoleCursorPosition(
                    GetStdHandle(STD_OUTPUT_HANDLE), {cursorPosX, static_cast<short>(lines + cursorPosY)});
            }
        }
        if (level == MessageLevel::Error)
        {
            MessageBoxA(nullptr, text.c_str(), "Error", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        }
    }
    catch (...)
    {}
}

bool CapsaicinMain::initialise() noexcept
{
    CLI::App app(programName);
    try
    {
        // Default application settings
        uint32_t windowWidth  = 1920;
        uint32_t windowHeight = 1080;
        float    renderScale  = 1.0F;
        bool     fullScreen   = false;

        // Command line settings
        app.set_version_flag("--version", SIG_VERSION_STR);
        app.add_option("--width", windowWidth, "Window width")->capture_default_str();
        app.add_option("--height", windowHeight, "Window height")->capture_default_str();
        app.add_option(
               "--render-scale", renderScale, "Render resolution scale with respect to window resolution")
            ->capture_default_str();
        app.add_option("--full-screen", fullScreen, "Open in Full-screen mode")->capture_default_str();
        uint32_t const defaultSceneSelect = static_cast<uint32_t>(
            ranges::find_if(scenes, [](SceneData const &sd) { return sd.name == defaultScene; })
            - scenes.cbegin());
        uint32_t sceneSelect = defaultSceneSelect;
        app.add_option("--start-scene-index", sceneSelect, "Start scene index")
            ->capture_default_str()
            ->check(CLI::Range(0U, static_cast<uint32_t>(scenes.size()) - 1));
        uint32_t const defaultEnvMapSelect =
            static_cast<uint32_t>(ranges::find_if(sceneEnvironmentMaps, [](EnvironmentData const &ed) {
                return ed.first == defaultEnvironmentMap;
            }) - sceneEnvironmentMaps.cbegin());
        uint32_t envMapSelect = numeric_limits<uint32_t>::max();
        app.add_option("--start-environment-map-index", envMapSelect, "Start environment map index")
            ->default_str(to_string(defaultEnvMapSelect))
            ->check(CLI::Range(0U, static_cast<uint32_t>(sceneEnvironmentMaps.size()) - 1));
        string externalScene;
        app.add_option("--load-external-scene", externalScene,
            "Override internal scene selection and explicitly load external file");
        auto     renderers        = Capsaicin::GetRenderers();
        auto     rendererSelectIt = ranges::find(renderers, Capsaicin::GetCurrentRenderer());
        uint32_t rendererSelect   = 0;
        if (rendererSelectIt != renderers.end())
        {
            rendererSelect = static_cast<uint32_t>(rendererSelectIt - renderers.begin());
        }
        else
        {
            rendererSelectIt = ranges::find(renderers, defaultRenderer);
            if (rendererSelectIt != renderers.end())
            {
                rendererSelect = static_cast<uint32_t>(rendererSelectIt - renderers.begin());
            }
        }
        app.add_option("--start-renderer-index", rendererSelect, "Start renderer index")
            ->capture_default_str()
            ->check(CLI::Range(0U, static_cast<uint32_t>(renderers.size()) - 1));
        uint32_t cameraSelect = numeric_limits<uint32_t>::max();
        app.add_option("--start-camera-index", cameraSelect, "Start camera index");
        vector<float> cameraPosition;
        app.add_option(
            "--user-camera-position", cameraPosition, "Set the initial position of the user camera");
        vector<float> cameraLookAt;
        app.add_option(
            "--user-camera-lookat", cameraLookAt, "Set the initial look at position of the user camera");
        bool startPlaying = false;
        app.add_flag("--start-playing", startPlaying, "Start with any animations running");
        auto *bench = app.add_flag("--benchmark-mode", benchmarkMode, "Enable benchmarking mode");
        app.add_option("--benchmark-frames", benchmarkModeFrameCount,
               "Number of frames to render during benchmark mode")
            ->needs(bench)
            ->check(CLI::Range(1U, numeric_limits<uint32_t>::max()))
            ->capture_default_str()
            ->take_last();
        app.add_option("--benchmark-first-frame", benchmarkModeStartFrame,
               "The first frame to start saving images from (Default just the last frame)")
            ->needs(bench)
            ->capture_default_str();
        app.add_option("--benchmark-suffix", benchmarkModeSuffix, "Suffix to add to any saved filenames")
            ->needs(bench)
            ->capture_default_str();

        vector<string> renderOptions;
        app.add_option("--render-options", renderOptions, "Additional render options");

        bool listScene = false;
        app.add_flag("--list-scenes", listScene, "List all available scenes and corresponding indexes");
        bool listEnv = false;
        app.add_flag(
            "--list-environments", listEnv, "List all available environment maps and corresponding indexes");
        bool listRenderer = false;
        app.add_flag(
            "--list-renderers", listRenderer, "List all available renderers and corresponding indexes");
        app.add_flag("--save-as-jpeg", saveAsJPEG, "Save as JPEG");

        // Parse command line and update any requested settings
        app.parse(GetCommandLine(), true);

        // Perform any help operations
        if (listScene)
        {
            for (uint32_t d = 0; auto const &i : scenes)
            {
                printString(to_string(d) + ": "s + string(i.name));
                ++d;
            }
            return false;
        }
        if (listEnv)
        {
            for (uint32_t d = 0; auto const &i : sceneEnvironmentMaps)
            {
                printString(to_string(d) + ": " + string(i.first));
                ++d;
            }
            return false;
        }
        if (listRenderer)
        {
            for (uint32_t d = 0; auto &i : renderers)
            {
                printString(to_string(d) + ": " + string(i));
                ++d;
            }
            return false;
        }

        // Create the internal gfx window and context
        window = gfxCreateWindow(windowWidth, windowHeight, programName.c_str(),
            (!benchmarkMode ? kGfxCreateWindowFlag_AcceptDrop : 0)
                | (fullScreen ? kGfxCreateWindowFlag_FullscreenWindow : 0));
        if (!window)
        {
            return false;
        }

        contextGFX = gfxCreateContext(
            window, kGfxCreateContextFlag_EnableShaderCache
#if _DEBUG || defined(SHADER_DEBUG)
                        | kGfxCreateContextFlag_EnableStablePowerState
                        | kGfxCreateContextFlag_EnableDebugLayer | kGfxCreateContextFlag_EnableShaderDebugging
#endif
        );
        if (!contextGFX)
        {
            return false;
        }

        // Create ImGui context using additional needed fonts
        array<char const *, 1> fonts = {R"(C:\Windows\Fonts\seguisym.ttf)"};
        array<ImFontConfig, 1> fontConfigs;
        fontConfigs[0].MergeMode                   = true;
        static array<ImWchar const, 5> glyphRanges = {
            0x2310,
            0x23FF, // Media player icons
            0x1F500,
            0x1F505, // Restart icon
            0,
        };
        fontConfigs[0].GlyphRanges = glyphRanges.data();
        fontConfigs[0].SizePixels  = 30.0F;
        fontConfigs[0].GlyphOffset.y += 5.0F; // Need to offset glyphs downward to properly center them
        if (auto err = gfxImGuiInitialize(contextGFX, fonts.data(), 1, fontConfigs.data());
            err != kGfxResult_NoError)
        {
            return false;
        }

        // Create Capsaicin render context
        Capsaicin::Initialize(contextGFX, ImGui::GetCurrentContext());

        // Set window scaling
        Capsaicin::SetRenderDimensionsScale(renderScale);

        // Initialise render settings
        if (!setRenderer(renderers[rendererSelect]))
        {
            return false;
        }

        // Pass any command line render options
        if (!renderOptions.empty())
        {
            auto &validOpts = Capsaicin::GetOptions();
            for (auto const &opt : renderOptions)
            {
                auto const splitLoc = opt.find('=');
                if (splitLoc == string::npos)
                {
                    printString("Invalid command line format of '--render-options'", MessageLevel::Error);
                    return false;
                }
                string const option = opt.substr(0, splitLoc);
                string const value  = opt.substr(splitLoc + 1);
                if (auto found = validOpts.find(option); found != validOpts.end())
                {
                    if (holds_alternative<bool>(found->second))
                    {
                        if (value == "true" || value == "1")
                        {
                            Capsaicin::setOption(option, true);
                        }
                        else if (value == "false" || value == "0")
                        {
                            Capsaicin::setOption(option, false);
                        }
                        else
                        {
                            printString("Invalid command line value passed for render option '" + option
                                            + "' expected bool",
                                MessageLevel::Error);
                            return false;
                        }
                    }
                    else if (holds_alternative<int32_t>(found->second))
                    {
                        try
                        {
                            int32_t const newValue = stoi(value);
                            Capsaicin::setOption(option, newValue);
                        }
                        catch (...)
                        {
                            printString("Invalid command line value passed for render option '" + option
                                            + "' expected integer",
                                MessageLevel::Error);
                            return false;
                        }
                    }
                    else if (holds_alternative<uint32_t>(found->second))
                    {
                        try
                        {
                            uint32_t const newValue = stoul(value);
                            Capsaicin::setOption(option, newValue);
                        }
                        catch (...)
                        {
                            printString("Invalid command line value passed for render option '" + option
                                            + "' expected unsigned integer",
                                MessageLevel::Error);
                            return false;
                        }
                    }
                    else if (holds_alternative<float>(found->second))
                    {
                        try
                        {
                            float const newValue = stof(value);
                            Capsaicin::setOption(option, newValue);
                        }
                        catch (...)
                        {
                            printString("Invalid command line value passed for render option '" + option
                                            + "' expected float",
                                MessageLevel::Error);
                            return false;
                        }
                    }
                }
                else
                {
                    printString("Invalid command line value passed for '--render-options': " + opt,
                        MessageLevel::Error);
                    return false;
                }
            }
        }

        // Load the requested start scene
        bool externalSceneLoaded = false;
        if (!externalScene.empty())
        {
            externalSceneLoaded = loadScene(externalScene);
        }
        if (!externalSceneLoaded
            && (scenes.size() <= sceneSelect || !loadScene(scenes[sceneSelect].fileName)))
        {
            return false;
        }

        // Check if environment map wasn't already loaded from a yaml file or overridden by user
        if (Capsaicin::GetCurrentEnvironmentMap().empty() || envMapSelect != numeric_limits<uint32_t>::max())
        {
            // Set environment map (must be done after scene load as environment maps are attached to scenes)
            if (auto const environmentMap =
                    scenes[sceneSelect].useEnvironmentMap
                        ? (envMapSelect != numeric_limits<uint32_t>::max() ? envMapSelect
                                                                           : defaultEnvMapSelect)
                        : 0;
                sceneEnvironmentMaps.size() <= environmentMap
                || !setEnvironmentMap(sceneEnvironmentMaps[environmentMap].second))
            {
                return false;
            }
        }

        // Check the passed in camera index
        if (cameraSelect != numeric_limits<uint32_t>::max())
        {
            auto const cameras = Capsaicin::GetSceneCameras();
            if (cameraSelect >= cameras.size())
            {
                printString(
                    "Invalid command line value passed in for '--start-camera-index'", MessageLevel::Error);
                return false;
            }
            if (cameraSelect == 0)
            {
                // Copy scene settings from any existing scene camera
                auto [oldPosition, oldForward, oldUp] = Capsaicin::GetSceneCameraView();
                auto const oldFovY                    = Capsaicin::GetSceneCameraFOV();
                auto const oldRange                   = Capsaicin::GetSceneCameraRange();
                setCamera(cameras[cameraSelect]);
                Capsaicin::SetSceneCameraView(oldPosition, oldForward, oldUp);
                Capsaicin::SetSceneCameraFOV(oldFovY);
                Capsaicin::SetSceneCameraRange(oldRange);
            }
            else
            {
                setCamera(cameras[cameraSelect]);
            }
        }

        // Check any initial user camera values
        if (!cameraPosition.empty() || !cameraLookAt.empty())
        {
            if (cameraSelect == 0)
            {
                // Get default camera values in case only some are set
                auto [position, forward, up] = Capsaicin::GetSceneCameraView();
                up                           = vec3(0.0F, 1.0F, 0.0F);
                if (cameraPosition.size() == 3)
                {
                    position = vec3(cameraPosition[0], cameraPosition[1], cameraPosition[2]);
                }
                else if (!cameraPosition.empty())
                {
                    printString(
                        "Invalid command line value passed in for '--user-camera-position' must be in the form '0.0 0.0 0.0'",
                        MessageLevel::Error);
                    return false;
                }
                if (cameraLookAt.size() == 3)
                {
                    forward = vec3(cameraLookAt[0], cameraLookAt[1], cameraLookAt[2]) - position;
                }
                else if (!cameraLookAt.empty())
                {
                    printString(
                        "Invalid command line value passed in for '--user-camera-lookat' must be in the form '0.0 0.0 0.0'",
                        MessageLevel::Error);
                    return false;
                }
                Capsaicin::SetSceneCameraView(position, forward, up);
            }
            else
            {
                printString(
                    "Command line values for '--user-camera-position' and '--user-camera-lookat' only take effect if start camera index is set to user camera '0'",
                    MessageLevel::Warning);
            }
        }

        if (benchmarkMode)
        {
            benchmarkModeStartFrame = glm::min(benchmarkModeStartFrame, benchmarkModeFrameCount - 1U);
            // Benchmark mode uses a fixed frame rate playback mode
            Capsaicin::SetFixedFrameRate(true);
        }
        else
        {
            // Register for drag+drop callback
            gfxWindowRegisterDropCallback(window, &fileDropCallback, this);
        }

        if (startPlaying)
        {
            Capsaicin::SetPaused(false);
        }
    }
    catch (const CLI::ParseError &e)
    {
        if (e.get_name() == "CallForHelp")
        {
            printString(app.help());
            return false;
        }
        else if (e.get_name() == "CallForAllHelp")
        {
            printString(app.help("", CLI::AppFormatMode::All));
            return false;
        }
        else if (e.get_name() == "CallForVersion")
        {
            printString(string(programName) + ": v"s + app.version());
            return false;
        }

        printString("Command Line Error: "s + ((exception)e).what(), MessageLevel::Error);
        return false;
    }
    catch (exception &e)
    {
        printString(e.what(), MessageLevel::Error);
        return false;
    }

    return true;
}

bool CapsaicinMain::loadScene(filesystem::path const &sceneFile, bool const append) noexcept
{
    try
    {
        // Check that scene file is locatable
        error_code ec;
        for (filesystem::path const &sceneDirectory : sceneDirectories)
        {
            filesystem::path searchPath = sceneDirectory;
            searchPath                  = searchPath / sceneFile;
            if (exists(searchPath, ec))
            {
                if (!append)
                {
                    return Capsaicin::SetScene(searchPath);
                }
                else
                {
                    return Capsaicin::AppendScene(searchPath);
                }
            }
        }
    }
    catch (exception const &e)
    {
        printString(e.what(), MessageLevel::Error);
    }
    printString("Failed to find requested scene file: "s + sceneFile.string(), MessageLevel::Error);

    return false;
}

void CapsaicinMain::setCamera(string_view const camera) noexcept
{
    // Set the camera to the currently requested camera
    Capsaicin::SetSceneCamera(camera);

    // Reset camera movement data
    cameraTranslation = vec3(0.0F);
    cameraRotation    = vec2(0.0F);
}

bool CapsaicinMain::setEnvironmentMap(filesystem::path const &environmentMap) noexcept
{
    try
    {
        if (environmentMap == "Atmosphere")
        {
            if (!Capsaicin::hasOption<bool>("atmosphere_enable"))
            {
                return false;
            }
            // The atmosphere technique overrides current environment map which requires an environment map to
            // exists
            if (Capsaicin::GetCurrentEnvironmentMap().empty())
            {
                Capsaicin::SetEnvironmentMap(sceneEnvironmentMaps[1].second);
            }
            Capsaicin::setOption<bool>("atmosphere_enable", true);
            return true;
        }
        else if (Capsaicin::hasOption<bool>("atmosphere_enable"))
        {
            if (Capsaicin::getOption<bool>("atmosphere_enable"))
            {
                // If currently enabled then we need to reset the internal environment map
                Capsaicin::SetEnvironmentMap("");
            }
            Capsaicin::setOption<bool>("atmosphere_enable", false);
        }

        if (environmentMap.empty())
        {
            // Load a null image
            return Capsaicin::SetEnvironmentMap("");
        }

        error_code ec;
        for (filesystem::path const &sceneDirectory : sceneDirectories)
        {
            filesystem::path searchPath = sceneDirectory;
            searchPath                  = searchPath / environmentMap;
            if (exists(searchPath, ec))
            {
                return Capsaicin::SetEnvironmentMap(searchPath);
            }
        }
    }
    catch (exception const &e)
    {
        printString(e.what(), MessageLevel::Error);
    }
    printString(
        "Failed to find requested environment map file: "s + environmentMap.string(), MessageLevel::Error);
    return false;
}

bool CapsaicinMain::setColorGradingLUT(std::filesystem::path const &colorGradingFile) noexcept
{
    if (Capsaicin::hasOption<string>("color_grading_file"))
    {
        Capsaicin::setOption("color_grading_enable", true);
        Capsaicin::setOption("color_grading_file", colorGradingFile.string());
        return true;
    }
    return false;
}

bool CapsaicinMain::setRenderer(string_view const renderer) noexcept
{
    // Change render settings based on currently selected renderer
    if (!Capsaicin::SetRenderer(renderer))
    {
        return false;
    }

    // Reset camera movement
    cameraTranslation = vec3(0.0F);
    cameraRotation    = vec2(0.0F);
    return true;
}

bool CapsaicinMain::renderFrame() noexcept
{
    // Get keyboard layout mapping
    auto kbMap = KeyboardMappings::QWERTY;
    if (PRIMARYLANGID(HIWORD(GetKeyboardLayout(0))) == LANG_FRENCH)
    {
        kbMap = KeyboardMappings::AZERTY;
    }

    // Check if window should close
    if (gfxWindowIsCloseRequested(window) || gfxWindowIsKeyReleased(window, VK_ESCAPE))
    {
        return false;
    }

    // Get events
    gfxWindowPumpEvents(window);

    if (!benchmarkMode)
    {
        // Update the camera
        if (!Capsaicin::GetFixedFrameRate())
        {
            {
                auto [position, forward, up] = Capsaicin::GetSceneCameraView();
                vec3 const right             = cross(forward, up);
                up                           = cross(right, forward);
                vec3        acceleration     = cameraTranslation * -30.0F;
                float const force            = cameraSpeed * 10000.0F;

                // Clamp frame-time to prevent errors at low frame rates
                auto const frameTime = glm::min(static_cast<float>(Capsaicin::GetFrameTime()), 0.05F);

                // Get camera keyboard input
                if (!ImGui::GetIO().WantCaptureKeyboard)
                {
                    if (gfxWindowIsKeyDown(window, keyboardMappings[static_cast<uint32_t>(kbMap)].upZ))
                    {
                        acceleration.z += force;
                    }
                    if (gfxWindowIsKeyDown(window, keyboardMappings[static_cast<uint32_t>(kbMap)].downZ))
                    {
                        acceleration.z -= force;
                    }
                    if (gfxWindowIsKeyDown(window, keyboardMappings[static_cast<uint32_t>(kbMap)].upX))
                    {
                        acceleration.x += force;
                    }
                    if (gfxWindowIsKeyDown(window, keyboardMappings[static_cast<uint32_t>(kbMap)].downX))
                    {
                        acceleration.x -= force;
                    }
                    if (gfxWindowIsKeyDown(window, keyboardMappings[static_cast<uint32_t>(kbMap)].upY))
                    {
                        acceleration.y += force;
                    }
                    if (gfxWindowIsKeyDown(window, keyboardMappings[static_cast<uint32_t>(kbMap)].downY))
                    {
                        acceleration.y -= force;
                    }
                }
                cameraTranslation += acceleration * 0.5F * frameTime;
                cameraTranslation = glm::clamp(cameraTranslation, -cameraSpeed, cameraSpeed);
                // Clamp tiny values to zero to improve convergence to resting state
                if (auto const clampMin = lessThan(glm::abs(cameraTranslation), vec3(0.0000001F));
                    any(clampMin))
                {
                    if (clampMin.x)
                    {
                        cameraTranslation.x = 0.0F;
                    }
                    if (clampMin.y)
                    {
                        cameraTranslation.y = 0.0F;
                    }
                    if (clampMin.z)
                    {
                        cameraTranslation.z = 0.0F;
                    }
                }

                // Get mouse input
                vec2 acceleration2 = cameraRotation * -45.0F;
                if (!ImGui::GetIO().WantCaptureMouse)
                {
                    constexpr float force2 = 0.15F;
                    acceleration2.x -= force2 * ImGui::GetMouseDragDelta(0, 0.0F).x;
                    acceleration2.y += force2 * ImGui::GetMouseDragDelta(0, 0.0F).y;
                }
                cameraRotation += acceleration2 * 0.5F * frameTime;
                cameraRotation = glm::clamp(cameraRotation, -4e-2F, 4e-2F);
                // Clamp tiny values to zero to improve convergence to resting state
                if (auto const clampRotationMin = lessThan(glm::abs(cameraRotation), vec2(0.00000001F));
                    any(clampRotationMin))
                {
                    if (clampRotationMin.x)
                    {
                        cameraRotation.x = 0.0F;
                    }
                    if (clampRotationMin.y)
                    {
                        cameraRotation.y = 0.0F;
                    }
                }
                ImGui::ResetMouseDragDelta(0);

                if (!all(glm::equal(cameraTranslation, vec3(0.0F)))
                    || !all(glm::equal(cameraRotation, vec2(0.0F))))
                {
                    if (Capsaicin::GetSceneCurrentCamera() != "User")
                    {
                        // Change to the user camera
                        auto [oldPosition, oldForward, oldUp] = Capsaicin::GetSceneCameraView();
                        auto const oldFovY                    = Capsaicin::GetSceneCameraFOV();
                        auto const oldRange                   = Capsaicin::GetSceneCameraRange();
                        Capsaicin::SetSceneCamera("User");
                        Capsaicin::SetSceneCameraView(oldPosition, oldForward, oldUp);
                        Capsaicin::SetSceneCameraFOV(oldFovY);
                        Capsaicin::SetSceneCameraRange(oldRange);
                    }

                    // Update translation
                    vec3 const translation = cameraTranslation * frameTime;
                    position += translation.x * right + translation.y * up + translation.z * forward;

                    // Rotate camera
                    quat const rotationX = angleAxis(-cameraRotation.x, up);
                    quat const rotationY = angleAxis(cameraRotation.y, right);

                    forward = normalize(forward * rotationX * rotationY);
                    if (abs(dot(forward, vec3(0.0F, 1.0F, 0.0F))) < 0.9F)
                    {
                        // Prevent view and up direction becoming parallel (this uses an FPS style camera)
                        vec3 const newRight = normalize(cross(forward, vec3(0.0F, 1.0F, 0.0F)));
                        up                  = normalize(cross(newRight, forward));
                    }

                    Capsaicin::SetSceneCameraView(position, forward, up);
                }

                // Update camera speed
                float const mouseWheel =
                    (!ImGui::GetIO().WantCaptureMouse ? 0.1F * ImGui::GetIO().MouseWheel : 0.0F);
                if (mouseWheel < 0.0F)
                {
                    cameraSpeed /= 1.0F - mouseWheel;
                }
                if (mouseWheel > 0.0F)
                {
                    cameraSpeed *= 1.0F + mouseWheel;
                }
                cameraSpeed = glm::clamp(cameraSpeed, 1e-3F, 1e3F);

                // Update camera FOV
                float const mouseWheelH =
                    (!ImGui::GetIO().WantCaptureMouse ? 0.01F * ImGui::GetIO().MouseWheelH : 0.0F);
                if (mouseWheelH != 0.0F)
                {
                    auto FOV = Capsaicin::GetSceneCameraFOV();
                    FOV -= mouseWheelH;
                    FOV = glm::clamp(FOV, 10.0F * static_cast<float>(numbers::pi_v<float>) / 180.0F,
                        140.0F * numbers::pi_v<float> / 180.0F);
                    Capsaicin::SetSceneCameraFOV(FOV);
                }
            }

            // Handle playback animation keys
            if (!ImGui::GetIO().WantCaptureKeyboard)
            {
                if (Capsaicin::HasAnimation())
                {
                    if (gfxWindowIsKeyReleased(window, VK_UP))
                    {
                        if (!Capsaicin::GetPaused())
                        {
                            Capsaicin::IncreasePlaybackSpeed();
                        }
                    }
                    if (gfxWindowIsKeyReleased(window, VK_DOWN))
                    {
                        if (!Capsaicin::GetPaused())
                        {
                            Capsaicin::DecreasePlaybackSpeed();
                        }
                    }
                    if (gfxWindowIsKeyReleased(window, VK_LEFT))
                    {
                        Capsaicin::StepPlaybackBackward(1);
                    }
                    if (gfxWindowIsKeyReleased(window, VK_RIGHT))
                    {
                        Capsaicin::StepPlaybackForward(1);
                    }
                }

                if (gfxWindowIsKeyReleased(window, VK_CONTROL))
                {
                    Capsaicin::SetRenderPaused(!Capsaicin::GetRenderPaused());
                }

                if (Capsaicin::HasAnimation() || Capsaicin::GetRenderPaused())
                {
                    // Pause/Resume animations if requested
                    if (gfxWindowIsKeyReleased(window, VK_SPACE))
                    {
                        if (Capsaicin::GetPaused())
                        {
                            if (!Capsaicin::GetRenderPaused())
                            {
                                Capsaicin::ResetPlaybackSpeed();
                                Capsaicin::SetPaused(false);
                            }
                            else
                            {
                                // Render 1 more frame
                                Capsaicin::SetRenderPaused(false);
                                reDisableRender = true;
                            }
                        }
                        else
                        {
                            Capsaicin::SetPaused(true);
                        }
                    }
                }

                // Hot-reload the shaders if requested
                if (gfxWindowIsKeyReleased(window, VK_F5))
                {
                    Capsaicin::ReloadShaders();
                }

                // Save image to disk if requested
                if (gfxWindowIsKeyReleased(window, VK_F6))
                {
                    saveImage = true;
                }
            }
        }
    }

    if (benchmarkMode)
    {
        // If current frame has reached our benchmark value then dump frame
        if ((Capsaicin::GetFrameIndex() + 1) >= benchmarkModeStartFrame)
        {
            saveImage = true;
        }
    }

    bool reenableToneMap = false;
    if (saveImage)
    {
        // Disable performing tone mapping as we output in HDR
        if (!saveAsJPEG && Capsaicin::hasOption<bool>("tonemap_enable"))
        {
            reenableToneMap = Capsaicin::getOption<bool>("tonemap_enable");
            Capsaicin::setOption("tonemap_enable", false);
        }
    }

    // Render the scene
    Capsaicin::Render();

    if (saveImage)
    {
        saveFrame();
        saveImage = false;

        // Re-enable Tonemap after save to disk
        if (reenableToneMap)
        {
            Capsaicin::setOption("tonemap_enable", true);
        }
    }

    if (reDisableRender)
    {
        Capsaicin::SetRenderPaused(true);
        reDisableRender = false;
    }

    // Render the UI
    renderGUI();

    // Complete the frame
#if _DEBUG || defined(SHADER_DEBUG)
    gfxFrame(contextGFX);
#else
    gfxFrame(contextGFX, false);
#endif

    if (benchmarkMode)
    {
        // If current frame has reached our benchmark value then terminate loop
        if (Capsaicin::GetFrameIndex() >= (benchmarkModeFrameCount - 1U))
        {
            return false;
        }
    }

    return true;
}

bool CapsaicinMain::renderGUI() noexcept
{
    try
    {
        // Show the GUI
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::Begin(programName.c_str(), nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        {
            if (!benchmarkMode)
            {
                // Select which scene to display
                auto const currentScenes    = Capsaicin::GetCurrentScenes();
                auto       currentSceneBase = currentScenes[0];
                currentSceneBase.replace_extension("");
                uint32_t const currentScene =
                    !currentScenes.empty()
                        ? static_cast<uint32_t>(ranges::find_if(scenes,
                                                    [&currentSceneBase](SceneData const &sd) {
                                                        auto findBase = sd.fileName;
                                                        findBase.replace_extension("");
                                                        // YAML files show up with GLTF extension so ignore
                                                        // extension when checking
                                                        return currentSceneBase.generic_string().rfind(
                                                                   findBase.generic_string())
                                                            != string::npos;
                                                    })
                                                - scenes.cbegin())
                        : static_cast<uint32_t>(scenes.size());
                auto     selectedScene = static_cast<int32_t>(currentScene);
                string   sceneList;
                uint32_t sceneIndex = 0;
                for (auto const &i : scenes)
                {
                    sceneList += i.name;
                    if (sceneIndex == currentScene && currentScenes.size() > 1)
                    {
                        sceneList += '+';
                    }
                    sceneList += '\0';
                    ++sceneIndex;
                }
                if (selectedScene >= static_cast<int32_t>(scenes.size()))
                {
                    if (!currentScenes.empty())
                    {
                        sceneList += "External";
                    }
                    else
                    {
                        sceneList += "None";
                    }
                    sceneList += '\0';
                }
                if (ImGui::Combo(
                        "Scene", &selectedScene, sceneList.c_str(), static_cast<int32_t>(scenes.size())))
                {
                    if (currentScene != static_cast<uint32_t>(selectedScene))
                    {
                        // Backup current environment map
                        auto const currentEnvironmentMap = Capsaicin::GetCurrentEnvironmentMap();

                        // Change the selected scene
                        if (!loadScene(scenes[static_cast<uint32_t>(selectedScene)].fileName))
                        {
                            ImGui::End();
                            return false;
                        }

                        // Check if scene file requested an environment map
                        if (auto const sceneEnvironmentMap = Capsaicin::GetCurrentEnvironmentMap();
                            sceneEnvironmentMap.empty())
                        {
                            // Reset environment map
                            auto const environmentMap =
                                selectedScene >= static_cast<int32_t>(scenes.size())
                                        || scenes[static_cast<uint32_t>(selectedScene)].useEnvironmentMap
                                    ? (currentEnvironmentMap.empty()
                                              ? ranges::find_if(sceneEnvironmentMaps,
                                                    [](EnvironmentData const &ed) {
                                                        return ed.first == defaultEnvironmentMap;
                                                    })
                                                    ->second
                                              : currentEnvironmentMap)
                                    : "";
                            if (!setEnvironmentMap(environmentMap))
                            {
                                ImGui::End();
                                return false;
                            }
                        }
                    }
                }

                // Optionally select which environment map is used
                if (selectedScene >= static_cast<int32_t>(scenes.size())
                    || scenes[static_cast<uint32_t>(selectedScene)].useEnvironmentMap)
                {
                    string emList;
                    auto   currentEnvironmentMap2 = Capsaicin::GetCurrentEnvironmentMap();
                    if (Capsaicin::hasOption<bool>("atmosphere_enable")
                        && Capsaicin::getOption<bool>("atmosphere_enable"))
                    {
                        currentEnvironmentMap2 = "Atmosphere";
                    }
                    uint32_t const currentEnvironmentMap = static_cast<uint32_t>(
                        ranges::find_if(sceneEnvironmentMaps,
                            [&currentEnvironmentMap2](EnvironmentData const &ed) {
                                return !ed.second.string().empty()
                                         ? currentEnvironmentMap2.string().rfind(ed.second.string())
                                               != string::npos
                                         : currentEnvironmentMap2 == "";
                            })
                        - sceneEnvironmentMaps.cbegin());
                    auto selectedEM = static_cast<int32_t>(currentEnvironmentMap);
                    for (auto const &i : sceneEnvironmentMaps)
                    {
                        if (i.first == "Atmosphere" && !Capsaicin::hasOption<bool>("atmosphere_enable"))
                        {
                            continue;
                        }
                        emList += i.first;
                        emList += '\0';
                    }
                    if (selectedEM >= static_cast<int32_t>(sceneEnvironmentMaps.size()))
                    {
                        emList += "External";
                        emList += '\0';
                    }
                    if (ImGui::Combo("Environment Map", &selectedEM, emList.c_str(),
                            static_cast<int32_t>(emList.size())))
                    {
                        if (currentEnvironmentMap != static_cast<uint32_t>(selectedEM))
                        {
                            // Change the selected environment map
                            if (!setEnvironmentMap(
                                    sceneEnvironmentMaps[static_cast<uint32_t>(selectedEM)].second))
                            {
                                ImGui::End();
                                return false;
                            }
                        }
                    }
                }

                // Call the class specific GUI function
                renderGUIDetails();
            }
            else
            {
                // Display profiling options
                Capsaicin::RenderGUI(true);
            }
        }
        ImGui::End();

        // Submit the frame
        {
            GfxCommandEvent const command_event(contextGFX, "DrawImGui");
            gfxImGuiRender();
        }
    }
    catch (exception const &e)
    {
        printString(e.what(), MessageLevel::Error);
        return false;
    }

    return true;
}

void CapsaicinMain::renderCameraDetails()
{
    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Select which preset camera to use
        string     cameraList;
        auto const cameras = Capsaicin::GetSceneCameras();
        int32_t    selectedCamera =
            static_cast<int32_t>(ranges::find(cameras, Capsaicin::GetSceneCurrentCamera()) - cameras.begin());
        auto const cameraIndex = selectedCamera;
        for (auto const &i : cameras)
        {
            cameraList += i;
            cameraList += '\0';
        }
        if (ImGui::Combo(
                "Camera", &selectedCamera, cameraList.c_str(), static_cast<int32_t>(cameraList.size())))
        {
            if (cameraIndex != selectedCamera)
            {
                // Change the selected camera
                setCamera(cameras[static_cast<uint32_t>(selectedCamera)]);
            }
        }

        auto fov              = Capsaicin::GetSceneCameraFOV();
        fov                   = degrees(fov);
        auto        fovInt    = static_cast<int32_t>(fov);
        float const remainder = fov - static_cast<float>(fovInt);
        ImGui::DragInt("FOV", &fovInt, 1, 10, 140);
        fov = radians(static_cast<float>(fovInt) + remainder);
        Capsaicin::SetSceneCameraFOV(fov);
        ImGui::DragFloat("Speed", &cameraSpeed, 0.01F);

        if (ImGui::TreeNode("Camera Data", "Camera Data"))
        {
            ImGui::SetCursorPosX(20);
            if (ImGui::BeginTable(
                    "Camera Location", 3, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoHostExtendX))
            {
                ImGui::TableSetupColumn(
                    "Position", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoReorder);
                ImGui::TableSetupColumn(
                    "Rotation", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoReorder);
                ImGui::TableSetupColumn(
                    "Forward", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoReorder);
                ImGui::TableSetupScrollFreeze(3, 3);
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                auto const [position, forward, up] = Capsaicin::GetSceneCameraView();
                auto const rotation                = degrees(eulerAngles(quatLookAt(forward, up)));
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(position.x));
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(rotation.x));
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(forward.x));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(position.y));
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(rotation.y));
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(forward.y));

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(position.z));
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(rotation.z));
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", static_cast<double>(forward.z));
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
}

bool CapsaicinMain::renderGUIDetails()
{
    // Display camera options
    renderCameraDetails();

    // Display animation options
    if (ImGui::CollapsingHeader("Animation Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!Capsaicin::HasAnimation())
        {
            ImGui::BeginDisabled();
        }
        constexpr array<char const *, 2> playModes = {"Real-time", "Fixed Frame Rate"};

        auto playMode = static_cast<int32_t>(Capsaicin::GetFixedFrameRate());
        if (ImGui::Combo("Play mode", &playMode, playModes.data(), 2))
        {
            Capsaicin::SetFixedFrameRate(playMode > 0);
        }
        if (!Capsaicin::HasAnimation())
        {
            ImGui::EndDisabled();
        }
        constexpr ImVec2 buttonHeight(0.0F, 30.0F);
        constexpr array  restartGlyph = {static_cast<char>(0xF0), static_cast<char>(0x9F),
             static_cast<char>(0x94), static_cast<char>(0x83),
             static_cast<char>(0x0)}; // Workaround compiler not handling u8"\u1F503" properly
        if (ImGui::Button(restartGlyph.data(), buttonHeight)) // Restart
        {
            Capsaicin::RestartPlayback();
        }
        if (!Capsaicin::HasAnimation())
        {
            ImGui::BeginDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button(reinterpret_cast<char const *>(u8"\u23EE"), buttonHeight)) // Step backward
        {
            Capsaicin::StepPlaybackBackward(30);
        }
        ImGui::SameLine();
        if (ImGui::Button(reinterpret_cast<char const *>(u8"\u23EA"), buttonHeight)) // Rewind
        {
            // If paused then just step back 1 frame, otherwise rewind
            if (Capsaicin::GetPaused())
            {
                Capsaicin::StepPlaybackBackward(1);
            }
            else if (!Capsaicin::GetPlayRewind())
            {
                if (Capsaicin::GetPlaybackSpeed() > 1.5)
                {
                    // If currently fast forwarding then slow down speed
                    Capsaicin::DecreasePlaybackSpeed();
                }
                else
                {
                    Capsaicin::ResetPlaybackSpeed();
                    Capsaicin::SetPlayRewind(true);
                }
            }
            else
            {
                // If already rewinding then increase rewind speed
                Capsaicin::IncreasePlaybackSpeed();
            }
        }
        ImGui::SameLine();
        if (!Capsaicin::HasAnimation() && Capsaicin::GetRenderPaused())
        {
            ImGui::EndDisabled();
        }
        if (Capsaicin::GetPaused())
        {
            // Display play button
            if (ImGui::Button(reinterpret_cast<char const *>(u8"\u23F5"), buttonHeight))
            {
                if (!Capsaicin::GetRenderPaused())
                {
                    Capsaicin::ResetPlaybackSpeed();
                    Capsaicin::SetPaused(false);
                }
                else
                {
                    // Render 1 more frame
                    Capsaicin::SetRenderPaused(false);
                    reDisableRender = true;
                }
            }
        }
        else
        {
            // Display pause button
            if (ImGui::Button(reinterpret_cast<char const *>(u8"\u23F8"), buttonHeight))
            {
                Capsaicin::SetPaused(true);
            }
        }
        if (!Capsaicin::HasAnimation() && (Capsaicin::GetRenderPaused() || reDisableRender))
        {
            ImGui::BeginDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button(reinterpret_cast<char const *>(u8"\u23E9"), buttonHeight)) // Fast-forward
        {
            // If paused then just step forward 1 frame, otherwise fast-forward
            if (Capsaicin::GetPaused())
            {
                Capsaicin::StepPlaybackForward(1);
            }
            else if (Capsaicin::GetPlayRewind())
            {
                if (Capsaicin::GetPlaybackSpeed() > 1.5)
                {
                    // If currently fast rewinding then slow down speed
                    Capsaicin::DecreasePlaybackSpeed();
                }
                else
                {
                    Capsaicin::ResetPlaybackSpeed();
                    Capsaicin::SetPlayRewind(false);
                }
            }
            else
            {
                // If already fast-forwarding then increase speed
                Capsaicin::IncreasePlaybackSpeed();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(reinterpret_cast<char const *>(u8"\u23ED"), buttonHeight)) // Step forward
        {
            Capsaicin::StepPlaybackForward(30);
        }
        ImGui::SameLine();
        if (!Capsaicin::HasAnimation())
        {
            ImGui::EndDisabled();
        }
        if (!Capsaicin::GetRenderPaused())
        {
            if (ImGui::Button(reinterpret_cast<char const *>(u8"\u23F3"), buttonHeight)) // Pause renderer
            {
                // Ensure animation is also paused
                Capsaicin::SetPaused(true);
                Capsaicin::SetRenderPaused(true);
                reDisableRender = false;
            }
        }
        else
        {
            if (ImGui::Button(reinterpret_cast<char const *>(u8"\u231B"), buttonHeight)) // Unpause renderer
            {
                Capsaicin::SetRenderPaused(false);
                reDisableRender = false;
            }
        }
    }

    if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Select which renderer to use
        string     rendererString;
        auto const rendererList     = Capsaicin::GetRenderers();
        int32_t    selectedRenderer = static_cast<int32_t>(
            ranges::find(rendererList, Capsaicin::GetCurrentRenderer()) - rendererList.cbegin());
        int32_t const currentRenderer = selectedRenderer;
        for (auto const &i : rendererList)
        {
            rendererString += i;
            rendererString += '\0';
        }
        if (ImGui::Combo("Renderer", &selectedRenderer, rendererString.c_str(), 8))
        {
            if (currentRenderer != selectedRenderer)
            {
                // Change the selected renderer
                if (!setRenderer(rendererList[static_cast<uint32_t>(selectedRenderer)]))
                {
                    return false;
                }
            }
        }
        Capsaicin::RenderGUI(false);
        ImGui::Separator();
    }

    // Display debugging options
    if (ImGui::CollapsingHeader("Debugging", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Select which debug view to use
        string     debugString;
        auto const debugList     = Capsaicin::GetDebugViews();
        int32_t    selectedDebug = static_cast<int32_t>(
            ranges::find(debugList, Capsaicin::GetCurrentDebugView()) - debugList.cbegin());
        selectedDebug              = glm::max(selectedDebug, 0); // Reset to 0 if not found
        int32_t const currentDebug = selectedDebug;
        for (auto const &i : debugList)
        {
            debugString += i;
            debugString += '\0';
        }
        if (ImGui::Combo("Debug View", &selectedDebug, debugString.c_str(), 8))
        {
            if (currentDebug != selectedDebug)
            {
                // Change the selected view
                Capsaicin::SetDebugView(debugList[static_cast<uint32_t>(selectedDebug)]);
            }
        }
        if (ImGui::Button("Reload Shaders (F5)"))
        {
            Capsaicin::ReloadShaders();
        }
        if (ImGui::Button("Dump Frame (F6)"))
        {
            saveImage = true;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Save as JPEG", &saveAsJPEG);
    }
    return true;
}

void CapsaicinMain::saveFrame() noexcept
{
    try
    {
        // Ensure output directory exists
        filesystem::path savePath = "./dump/"s;
        {
            if (error_code ec; !exists(savePath, ec))
            {
                create_directory(savePath, ec);
            }
        }
        savePath = getSaveName();
        if (!benchmarkModeSuffix.empty())
        {
            savePath += '_';
            savePath += benchmarkModeSuffix;
        }
        savePath += '_';
        uint32_t const frameIndex = Capsaicin::GetFrameIndex() + 1; //+1 to correct for 0 indexed
        savePath += to_string(frameIndex);
        if (!benchmarkMode || benchmarkModeStartFrame == benchmarkModeFrameCount - 1U)
        {
            // Only add the frame time when not batch saving images
            savePath += '_';
            savePath += to_string(Capsaicin::GetAverageFrameTime());
        }

        auto const        view     = Capsaicin::GetCurrentDebugView();
        string_view const dumpView = view != "None" ? view : "Color";
        if (view != "None")
        {
            savePath += view;
        }
        if (saveAsJPEG)
        {
            savePath += ".jpeg";
        }
        else
        {
            savePath += ".exr";
        }

        // Save the requested buffer to disk
        Capsaicin::DumpDebugView(savePath, dumpView);
    }
    catch (exception const &e)
    {
        printString(e.what(), MessageLevel::Error);
        return;
    }
}

filesystem::path CapsaicinMain::getSaveName()
{
    filesystem::path savePath = "./dump/"s;

    auto const currentScenes = Capsaicin::GetCurrentScenes();
    GFX_ASSERT(!currentScenes.empty());
    filesystem::path currentSceneName = currentScenes[0];
    currentSceneName           = currentSceneName.replace_extension(""); // Remove the '.gltf' extension
    currentSceneName           = currentSceneName.filename();
    filesystem::path currentEM = Capsaicin::GetCurrentEnvironmentMap();
    if (!currentEM.empty())
    {
        currentEM = currentEM.replace_extension(""); // Remove the extension
        currentEM = currentEM.filename();
    }
    else
    {
        currentEM = "None";
    }

    savePath += currentSceneName;
    savePath += '_';
    savePath += currentEM;
    savePath += '_';
    savePath += Capsaicin::GetSceneCurrentCamera();
    savePath += '_';
    savePath += Capsaicin::GetCurrentRenderer();
    auto filename = savePath.filename().string();
    erase_if(filename, [](unsigned char const c) { return isspace(c); });
    savePath.replace_filename(filename);
    return savePath;
}

void CapsaicinMain::fileDropCallback(char const *filePath, uint32_t const index, void *data) noexcept
{
    auto *thisPtr = static_cast<CapsaicinMain *>(data);
    try
    {
        error_code ec;
        auto       newPath       = filesystem::proximate(filePath, ec);
        string     convertString = newPath.string();
        // Must convert path separate to portable form, so it will match our pre-existing list.
        ranges::replace(convertString, '\\', '/');
        newPath = convertString;
        if (newPath.extension() == ".gltf" || newPath.extension() == ".glb" || newPath.extension() == ".obj"
            || newPath.extension() == ".yaml")
        {
            // We use the shift key as a modifier to toggle between replacing current scene and just appending
            // to it
            bool const mod = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (bool const append = index > 0 || mod; !thisPtr->loadScene(newPath, append))
            {
                MessageBoxA(nullptr, "Failed to open dragged file (invalid or corrupted)", filePath,
                    MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
            }
        }
        else if (newPath.extension() == ".hdr")
        {
            // Check if scene supports environment map
            auto const currentScenes = Capsaicin::GetCurrentScenes();
            if (uint32_t const currentScene = static_cast<uint32_t>(
                    ranges::find_if(scenes,
                        [&currentScenes](SceneData const &sd) {
                            return currentScenes[0].string().rfind(sd.fileName.string()) != string::npos;
                        })
                    - scenes.cbegin());
                currentScene >= scenes.size()
                || scenes[static_cast<uint32_t>(currentScene)].useEnvironmentMap)
            {
                if (!thisPtr->setEnvironmentMap(newPath))
                {
                    MessageBoxA(nullptr, "Failed to open dragged file (invalid or corrupted)", filePath,
                        MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
                }
            }
            else
            {
                MessageBoxA(nullptr, "Current Scene does not support adding environment maps", filePath,
                    MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
            }
        }
        else if (newPath.extension() == ".cube")
        {
            if (!thisPtr->setColorGradingLUT(newPath))
            {
                MessageBoxA(nullptr, "Current renderer does not support color grading LUTS", filePath,
                    MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
            }
        }
        else
        {
            MessageBoxA(nullptr, "Failed to open dragged file (unknown or unsupported)", filePath,
                MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
        }
    }
    catch (exception const &e)
    {
        thisPtr->printString(e.what(), MessageLevel::Error);
    }
}
