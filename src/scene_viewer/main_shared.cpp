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

#define _USE_MATH_DEFINES
#include "main_shared.h"

#include <CLI/CLI.hpp>
#include <chrono>
#include <cmath>
#include <gfx_imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <ranges>
#include <version.h>

using namespace std;
using namespace glm;

extern "C"
{
__declspec(dllexport) extern const UINT D3D12SDKVersion = 606;
}
extern "C"
{
__declspec(dllexport) extern char8_t const *D3D12SDKPath = u8".\\";
}

enum class KeyboardMappings : uint32_t
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

KeyboardMapping keyboardMappings[] = {
    {0x57 /*W*/, 0x53 /*S*/, 0x44 /*D*/, 0x41 /*A*/,       0x45 /*E*/,         0x51 /*Q*/},
    {0x5A /*Z*/, 0x53 /*S*/, 0x44 /*D*/, 0x51 /*Q*/, 0x21 /*Page Up*/, 0x22 /*Page Down*/}
};

/** Data required to represent each supported scene file */
struct SceneData
{
    std::string              name;
    std::vector<std::string> fileNames;
    bool                     useEnvironmentMap;
    float                    renderExposure;
};

/** List of supported scene files and associated data */
static vector<SceneData> const scenes = {
    {"Flying World",    {"assets/CapsaicinTestMedia/flying_world_battle_of_the_trash_god/FlyingWorld-BattleOfTheTrashGod.gltf"},  true, 2.5f                                                                                                          },
    {"Gas Station",                                                   {"assets/CapsaicinTestMedia/gas_station/GasStation.gltf"},  true, 1.0f},
    {"Tropical Bedroom",                                    {"assets/CapsaicinTestMedia/tropical_bedroom/TropicalBedroom.gltf"},  true, 1.0f},
    {"Sponza",                                                                 {"assets/CapsaicinTestMedia/sponza/Sponza.gltf"},  true, 5.0f},
    {"Breakfast Room",                                          {"assets/CapsaicinTestMedia/breakfast_room/BreakfastRoom.gltf"},  true, 3.0f},
};

/** List of supported environment maps */
static vector<pair<string_view, string_view>> const sceneEnvironmentMaps = {
    {                    "None",                                                                     ""},
    {"Photo Studio London Hall", "assets/CapsaicinTestMedia/environment_maps/PhotoStudioLondonHall.hdr"},
    {              "Kiara Dawn",             "assets/CapsaicinTestMedia/environment_maps/KiaraDawn.hdr"},
    {        "Nagoya Wall Path",        "assets/CapsaicinTestMedia/environment_maps/NagoyaWallPath.hdr"},
    {        "Spaichingen Hill",       "assets/CapsaicinTestMedia/environment_maps/SpaichingenHill.hdr"},
    {            "Studio Small",           "assets/CapsaicinTestMedia/environment_maps/StudioSmall.hdr"},
    {                   "White",                 "assets/CapsaicinTestMedia/environment_maps/White.hdr"},
    {              "Atmosphere",                                                                     ""},
};

/** List of executable relative file paths to search for scene files */
static vector<string> const sceneDirectories = {"", "../../../", "../../", "../"};

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
        // Check benchmark mode run
        if (benchmarkMode)
        {
            // If current frame has reached our benchmark value then dump frame
            if (Capsaicin::GetFrameIndex() >= benchmarkModeFrameCount)
            {
                // Needed to wait a single render pass for the frame saving to complete before closing
                break;
            }
            else if (Capsaicin::GetFrameIndex() >= benchmarkModeStartFrame)
            {
                saveFrame();
            }
        }
        if (!renderFrame())
        {
            return true;
        }
    }

    if (benchmarkMode && !benchmarkModeSuffix.empty() && Capsaicin::hasOption<bool>("image_metrics_enable")
        && Capsaicin::getOption<bool>("image_metrics_enable")
        && Capsaicin::getOption<bool>("image_metrics_save_to_file"))
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
        auto        savePath       = getSaveName();
        std::string newMetricsFile = savePath + '_' + benchmarkModeSuffix + ".csv";
        std::remove(newMetricsFile.c_str());
        std::string metricsFile = savePath + ".csv";
        if (std::rename(metricsFile.c_str(), newMetricsFile.c_str()) != 0)
        {
            printString("Failed to rename image metrics file: "s + metricsFile, MessageLevel::Warning);
        }
    }

    return true;
}

void CapsaicinMain::printString(std::string const &text, MessageLevel level) noexcept
{
    std::string outputText;
    switch (level)
    {
    case MessageLevel::Debug: break;
    case MessageLevel::Info: break;
    case MessageLevel::Warning: outputText = "Warning: "; break;
    case MessageLevel::Error: outputText = "Error: "; [[fallthrough]];
    default: break;
    }
    outputText += text;

    // Check if a debugger is attached and use it instead of a console
    // If no debugger is attached then we need to attach to a console process in order to be able to
    // output text
    if (IsDebuggerPresent())
    {
        OutputDebugStringA(outputText.c_str());
    }
    else
    {
        if (!hasConsole)
        {
            // Look to see if there is a parent console (i.e. this was launched from terminal) and attach
            // to that
            if (AttachConsole(ATTACH_PARENT_PROCESS))
            {
                // Set the screen buffer big enough to hold at least help text
                CONSOLE_SCREEN_BUFFER_INFO scInfo;
                GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &scInfo);

                // Force the console buffer to be resized no matter what as this forces the console to
                // update the end line to match the number of printed lines from this app
                constexpr int16_t minLength = 4096;
                scInfo.dwSize.Y             = std::max(minLength, (short)(scInfo.dwSize.Y + 100));
                SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), scInfo.dwSize);
                hasConsole = true;
            }
        }
        if (hasConsole)
        {
            // The parent console has already printed a new user prompt before this program has even run so
            // need to insert any printed lines before the existing user prompt

            // Save current cursor position
            CONSOLE_SCREEN_BUFFER_INFO scInfo;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &scInfo);
            auto cursorPosY = scInfo.dwCursorPosition.Y;
            auto cursorPosX = scInfo.dwCursorPosition.X;

            // Move to start of current line
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), {0, cursorPosY});

            // Insert new line into console buffer
            std::vector<CHAR_INFO> buffer;
            buffer.resize(cursorPosX);
            COORD      coordinates = {0};
            SMALL_RECT textRegion  = {
                 .Left   = 0,
                 .Top    = cursorPosY,
                 .Right  = (short)(cursorPosX - 1),
                 .Bottom = cursorPosY,
            };
            COORD bufferSize = {
                .X = cursorPosX,
                .Y = 1,
            };

            ReadConsoleOutputA(
                GetStdHandle(STD_OUTPUT_HANDLE), buffer.data(), bufferSize, coordinates, &textRegion);
            DWORD dnc;
            FillConsoleOutputCharacter(
                GetStdHandle(STD_OUTPUT_HANDLE), ' ', cursorPosX, {0, cursorPosY}, &dnc);

            // Set the screen buffer big enough to hold new lines
            std::vector<std::string_view> textLines;
            for (auto const i : std::views::split(outputText, '\n'))
            {
                textLines.emplace_back(i.begin(), i.end());
            }

            // Write out each new line from the input text
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), {0, cursorPosY});
            uint32_t lines = 0;
            for (auto const &i : textLines)
            {
                auto lineWidth = i.size();
                if (lineWidth > 0)
                {
                    WriteConsoleOutputCharacterA(GetStdHandle(STD_OUTPUT_HANDLE), &*i.begin(),
                        (DWORD)lineWidth, {0, short(cursorPosY + lines)}, &dnc);
                    ++lines;
                    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), {0, short(cursorPosY + lines)});
                }
            }

            // Restore cursor position to previously saved state and increment by number of new lines
            textRegion = {
                .Left   = 0,
                .Top    = (short)(cursorPosY + lines),
                .Right  = (short)(cursorPosX - 1),
                .Bottom = (short)(cursorPosY + lines),
            };
            WriteConsoleOutputA(
                GetStdHandle(STD_OUTPUT_HANDLE), buffer.data(), bufferSize, coordinates, &textRegion);
            SetConsoleCursorPosition(
                GetStdHandle(STD_OUTPUT_HANDLE), {cursorPosX, short(cursorPosY + lines)});
        }
    }
    if (level == MessageLevel::Error)
    {
        MessageBoxA(nullptr, text.c_str(), "Error", MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
    }
}

bool CapsaicinMain::initialise() noexcept
{
    // Default application settings
    uint32_t windowWidth  = 1920;
    uint32_t windowHeight = 1080;

    // Command line settings
    CLI::App app(programName.data());
    app.set_version_flag("--version", SIG_VERSION_STR);
    app.allow_config_extras(CLI::config_extras_mode::error);
    app.add_option("--width", windowWidth, "Window width")->capture_default_str();
    app.add_option("--height", windowHeight, "Window height")->capture_default_str();
    uint32_t sceneSelect = static_cast<uint32_t>(defaultScene);
    app.add_option("--start-scene-index", sceneSelect, "Start scene index")
        ->capture_default_str()
        ->check(CLI::Range(0u, (uint32_t)scenes.size() - 1));
    uint32_t envMapSelect = static_cast<uint32_t>(defaultEnvironmentMap);
    app.add_option("--start-environment-map-index", envMapSelect, "Start environment map index")
        ->capture_default_str()
        ->check(CLI::Range(0u, (uint32_t)sceneEnvironmentMaps.size() - 1));
    auto     renderers        = Capsaicin::GetRenderers();
    auto     rendererSelectIt = find(renderers.begin(), renderers.end(), Capsaicin::GetCurrentRenderer());
    uint32_t rendererSelect   = 0;
    if (rendererSelectIt != renderers.end())
    {
        rendererSelect = static_cast<uint32_t>(rendererSelectIt - renderers.begin());
    }
    else
    {
        rendererSelectIt = find(renderers.begin(), renderers.end(), defaultRenderer);
        if (rendererSelectIt != renderers.end())
        {
            rendererSelect = static_cast<uint32_t>(rendererSelectIt - renderers.begin());
        }
    }
    app.add_option("--start-renderer-index", rendererSelect, "Start renderer index")
        ->capture_default_str()
        ->check(CLI::Range(0u, (uint32_t)renderers.size() - 1));
    uint32_t cameraSelect = uint32_t(-1);
    app.add_option("--start-camera-index", cameraSelect, "Start camera index");
    std::vector<float> cameraPosition;
    app.add_option("--user-camera-position", cameraPosition, "Set the initial position of the user camera");
    std::vector<float> cameraLookAt;
    app.add_option(
        "--user-camera-lookat", cameraLookAt, "Set the initial look at position of the user camera");
    bool startPlaying = false;
    app.add_flag("--start-playing", startPlaying, "Start with any animations running");
    auto bench = app.add_flag("--benchmark-mode", benchmarkMode, "Enable benchmarking mode");
    app.add_option(
           "--benchmark-frames", benchmarkModeFrameCount, "Number of frames to render during benchmark mode")
        ->needs(bench)
        ->capture_default_str();
    app.add_option("--benchmark-first-frame", benchmarkModeStartFrame,
           "The first frame to start saving images from (Default just the last frame)")
        ->needs(bench)
        ->capture_default_str();
    app.add_option("--benchmark-suffix", benchmarkModeSuffix, "Suffix to add to any saved filenames")
        ->needs(bench)
        ->capture_default_str();

    std::vector<std::string> renderOptions;
    app.add_option("--render-options", renderOptions, "Additional render options");

    bool listScene = false;
    app.add_flag("--list-scenes", listScene, "List all available scenes and corresponding indexes");
    bool listEnv = false;
    app.add_flag(
        "--list-environments", listEnv, "List all available environment maps and corresponding indexes");
    bool listRenderer = false;
    app.add_flag("--list-renderers", listRenderer, "List all available renderers and corresponding indexes");

    // Parse command line and update any requested settings
    try
    {
        app.parse(GetCommandLine(), true);
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
            printString(std::string(programName) + ": v"s + app.version());
            return false;
        }

        printString("Command Line Error: "s + ((exception)e).what(), MessageLevel::Error);
        return false;
    }

    // Perform any help operations
    if (listScene)
    {
        for (uint32_t d = 0; auto &i : scenes)
        {
            printString(std::to_string(d) + ": "s + std::string(i.name));
            ++d;
        }
        return false;
    }
    if (listEnv)
    {
        for (uint32_t d = 0; auto &i : sceneEnvironmentMaps)
        {
            printString(std::to_string(d) + ": " + std::string(i.first));
            ++d;
        }
        return false;
    }
    if (listRenderer)
    {
        for (uint32_t d = 0; auto &i : renderers)
        {
            printString(std::to_string(d) + ": " + std::string(i));
            ++d;
        }
        return false;
    }

    // Create the internal gfx window and context
    window = gfxCreateWindow(windowWidth, windowHeight, programName.data());
    if (!window)
    {
        return false;
    }

    contextGFX = gfxCreateContext(
        window, 0
#if _DEBUG || defined(SHADER_DEBUG)
                    | kGfxCreateContextFlag_EnableStablePowerState | kGfxCreateContextFlag_EnableDebugLayer
                    | kGfxCreateContextFlag_EnableShaderDebugging
#endif
    );
    if (!contextGFX)
    {
        return false;
    }

    // Create ImGui context using additional needed fonts
    char const  *fonts[] = {"C:\\Windows\\Fonts\\seguisym.ttf"};
    ImFontConfig fontConfigs[1];
    fontConfigs[0].MergeMode           = true;
    static const ImWchar glyphRanges[] = {
        0x2310,
        0x23FF, // Media player icons
        0x1F500,
        0x1F505, // Restart icon
        0,
    };
    fontConfigs[0].GlyphRanges = &glyphRanges[0];
    fontConfigs[0].SizePixels  = 30.0f;
    fontConfigs[0].GlyphOffset.y += 5.0f; // Need to offset glyphs downward to properly center them
    if (auto err = gfxImGuiInitialize(contextGFX, fonts, 1, fontConfigs); err != kGfxResult_NoError)
    {
        return false;
    }

    // Create Capsaicin render context
    Capsaicin::Initialize(contextGFX, ImGui::GetCurrentContext());

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
            if (splitLoc == std::string::npos)
            {
                printString("Invalid command line format of '--render-options'", MessageLevel::Error);
                return false;
            }
            std::string option = opt.substr(0, splitLoc);
            std::string value  = opt.substr(splitLoc + 1);
            if (auto found = validOpts.find(option); found != validOpts.end())
            {
                if (std::holds_alternative<bool>(found->second))
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
                else if (std::holds_alternative<int32_t>(found->second))
                {
                    try
                    {
                        const int32_t newValue = std::stoi(value);
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
                else if (std::holds_alternative<uint32_t>(found->second))
                {
                    try
                    {
                        const uint32_t newValue = std::stoul(value);
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
                else if (std::holds_alternative<float>(found->second))
                {
                    try
                    {
                        float const newValue = std::stof(value);
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
                printString(
                    "Invalid command line value passed for '--render-options': " + opt, MessageLevel::Error);
                return false;
            }
        }
    }

    // Load the requested start scene
    if (!loadScene(static_cast<Scene>(sceneSelect)))
    {
        return false;
    }

    // Set environment map (must be done after scene load as environment maps are attached to scenes)
    auto const environmentMap = scenes[static_cast<uint32_t>(sceneSelect)].useEnvironmentMap
                                  ? static_cast<EnvironmentMap>(envMapSelect)
                                  : EnvironmentMap::None;
    if (!setEnvironmentMap(environmentMap))
    {
        return false;
    }

    // Check the passed in camera index
    if (cameraSelect != -1)
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
            auto oldCamera = Capsaicin::GetSceneCamera();
            setCamera(cameras[cameraSelect]);
            auto camera = Capsaicin::GetSceneCamera();
            *camera     = *oldCamera;
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
            auto camera = &*Capsaicin::GetSceneCamera();
            camera->up  = glm::vec3(0.0f, 1.0f, 0.0f);
            if (cameraPosition.size() == 3)
            {
                camera->eye = glm::vec3(cameraPosition[0], cameraPosition[1], cameraPosition[2]);
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
                camera->center = glm::vec3(cameraLookAt[0], cameraLookAt[1], cameraLookAt[2]);
            }
            else if (!cameraLookAt.empty())
            {
                printString(
                    "Invalid command line value passed in for '--user-camera-lookat' must be in the form '0.0 0.0 0.0'",
                    MessageLevel::Error);
                return false;
            }
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
        benchmarkModeStartFrame = std::min(benchmarkModeStartFrame, benchmarkModeFrameCount - 1);
        // Benchmark mode uses a fixed frame rate playback mode
        Capsaicin::SetFixedFrameRate(true);
    }

    if (startPlaying)
    {
        Capsaicin::SetPaused(false);
    }

    return true;
}

bool CapsaicinMain::loadScene(Scene scene) noexcept
{
    // Check that scene file is locatable
    error_code ec;

    auto const              &sceneData  = scenes[static_cast<uint32_t>(scene)];
    auto const              &sceneNames = sceneData.fileNames;
    std::vector<std::string> scenePaths;

    for (auto const &sceneDirectory : sceneDirectories)
    {
        scenePaths.clear();
        for (auto const &sceneName : sceneNames)
        {
            string scenePath = sceneDirectory + sceneName;
            if (!std::filesystem::exists(scenePath, ec))
            {
                break;
            }

            scenePaths.push_back(scenePath);
        }

        // All scenes exist, load them
        if (scenePaths.size() == sceneNames.size()) break;
    }

    if (scenePaths.size() != sceneNames.size())
    {
        printString("Failed to find all requested files for scene: "s + sceneData.name, MessageLevel::Error);
        return false;
    }
    else if (!Capsaicin::SetScenes(scenePaths))
    {
        return false;
    }

    // Set render settings based on current scene
    Capsaicin::setOption("tonemap_exposure", sceneData.renderExposure);
    currentScene = scene;
    return true;
}

void CapsaicinMain::setCamera(std::string_view camera) noexcept
{
    // Set the camera to the currently requested camera index
    Capsaicin::SetSceneCamera(camera);

    // Reset camera movement data
    cameraTranslation = glm::vec3(0.0f);
    cameraRotation    = glm::vec2(0.0f);
}

bool CapsaicinMain::setEnvironmentMap(EnvironmentMap environmentMap) noexcept
{
    if (environmentMap == EnvironmentMap::None)
    {
        // Load a null image
        currentEnvironmentMap = environmentMap;
        return Capsaicin::SetEnvironmentMap("");
    }
    else if (sceneEnvironmentMaps[static_cast<uint32_t>(environmentMap)].first == "Atmosphere")
    {
        // The atmosphere technique overrides current environment map
        Capsaicin::setOption<bool>("atmosphere_enable", true);
        currentEnvironmentMap = environmentMap;
        return true;
    }
    else if (Capsaicin::hasOption<bool>("atmosphere_enable"))
    {
        Capsaicin::setOption<bool>("atmosphere_enable", false);
    }

    error_code ec;
    for (auto &i : sceneDirectories)
    {
        string evFile = i;
        evFile += sceneEnvironmentMaps[static_cast<uint32_t>(environmentMap)].second.data();
        if (std::filesystem::exists(evFile, ec))
        {
            currentEnvironmentMap = environmentMap;
            return Capsaicin::SetEnvironmentMap(evFile);
        }
    }
    printString("Failed to find requested environment map file: "s
                    + string(sceneEnvironmentMaps[static_cast<uint32_t>(environmentMap)].second),
        MessageLevel::Error);
    return false;
}

bool CapsaicinMain::setRenderer(std::string_view renderer) noexcept
{
    // Change render settings based on currently selected renderer
    if (!Capsaicin::SetRenderer(renderer))
    {
        return false;
    }

    // Set render settings based on current scene
    auto const currentScenes = Capsaicin::GetCurrentScenes();
    auto const selectedScene = std::find_if(scenes.cbegin(), scenes.cend(),
        [&currentScenes](auto const &value) { return value.fileNames == currentScenes; });

    if (selectedScene != scenes.cend())
    {
        Capsaicin::setOption("tonemap_exposure", selectedScene->renderExposure);
    }

    // Reset camera movement
    cameraTranslation = vec3(0.0f);
    cameraRotation    = vec2(0.0f);
    return true;
}

bool CapsaicinMain::renderFrame() noexcept
{
    // Get keyboard layout mapping
    KeyboardMappings kbMap = KeyboardMappings::QWERTY;
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
            auto        camera       = Capsaicin::GetSceneCamera();
            vec3 const  forward      = normalize(camera->center - camera->eye);
            vec3 const  right        = cross(forward, camera->up);
            vec3 const  up           = cross(right, forward);
            vec3        acceleration = cameraTranslation * -30.0f;
            float const force        = cameraSpeed * 10000.0f;

            // Clamp frametime to prevent errors at low frame rates
            auto frameTime = glm::min(static_cast<float>(Capsaicin::GetFrameTime()), 0.05f);

            // Get keyboard input
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
            cameraTranslation += acceleration * 0.5f * frameTime;
            cameraTranslation = glm::clamp(cameraTranslation, -cameraSpeed, cameraSpeed);
            // Clamp tiny values to zero to improve convergence to resting state
            auto const clampMin = glm::lessThan(glm::abs(cameraTranslation), vec3(0.0000001f));
            if (glm::any(clampMin))
            {
                if (clampMin.x)
                {
                    cameraTranslation.x = 0.0f;
                }
                if (clampMin.y)
                {
                    cameraTranslation.y = 0.0f;
                }
                if (clampMin.z)
                {
                    cameraTranslation.z = 0.0f;
                }
            }

            // Get mouse input
            vec2        acceleration2 = cameraRotation * -45.0f;
            float const force2        = 0.15f;
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                acceleration2.x -= force2 * ImGui::GetMouseDragDelta(0, 0.0f).x;
                acceleration2.y += force2 * ImGui::GetMouseDragDelta(0, 0.0f).y;
            }
            cameraRotation += acceleration2 * 0.5f * frameTime;
            cameraRotation = glm::clamp(cameraRotation, -4e-2f, 4e-2f);
            // Clamp tiny values to zero to improve convergence to resting state
            auto const clampRotationMin = glm::lessThan(glm::abs(cameraRotation), vec2(0.00000001f));
            if (glm::any(clampRotationMin))
            {
                if (clampRotationMin.x)
                {
                    cameraRotation.x = 0.0f;
                }
                if (clampRotationMin.y)
                {
                    cameraRotation.y = 0.0f;
                }
            }
            ImGui::ResetMouseDragDelta(0);

            if (!glm::all(glm::equal(cameraTranslation, vec3(0.0f)))
                || !glm::all(glm::equal(cameraRotation, vec2(0.0f))))
            {
                if (Capsaicin::GetSceneCurrentCamera() != "User")
                {
                    // Change to the user camera
                    auto oldCamera = camera;
                    Capsaicin::SetSceneCamera("User");
                    camera  = Capsaicin::GetSceneCamera();
                    *camera = *oldCamera;
                }

                // Update translation
                vec3 translation = cameraTranslation * frameTime;
                camera->eye += translation.x * right + translation.y * up + translation.z * forward;
                camera->center += translation.x * right + translation.y * up + translation.z * forward;

                // Rotate camera
                quat rotationX = angleAxis(-cameraRotation.x, up);
                quat rotationY = angleAxis(cameraRotation.y, right);

                const vec3 newForward = normalize(forward * rotationX * rotationY);
                if (abs(dot(newForward, vec3(0.0f, 1.0f, 0.0f))) < 0.9f)
                {
                    // Prevent view and up direction becoming parallel (this uses a FPS style camera)
                    camera->center      = camera->eye + newForward;
                    const vec3 newRight = normalize(cross(newForward, vec3(0.0f, 1.0f, 0.0f)));
                    camera->up          = normalize(cross(newRight, newForward));
                }
            }

            // Update camera for potential window resize
            camera->aspect = static_cast<float>(gfxGetBackBufferWidth(contextGFX))
                           / static_cast<float>(gfxGetBackBufferHeight(contextGFX));

            // Update camera speed
            float const mouseWheel =
                (!ImGui::GetIO().WantCaptureMouse ? 0.1f * ImGui::GetIO().MouseWheel : 0.0f);
            if (mouseWheel < 0.0f)
            {
                cameraSpeed /= 1.0f - mouseWheel;
            }
            if (mouseWheel > 0.0f)
            {
                cameraSpeed *= 1.0f + mouseWheel;
            }
            cameraSpeed = glm::clamp(cameraSpeed, 1e-3f, 1e3f);

            // Update camera FOV
            float const mouseWheelH =
                (!ImGui::GetIO().WantCaptureMouse ? 0.01f * ImGui::GetIO().MouseWheelH : 0.0f);
            camera->fovY -= mouseWheelH;
            camera->fovY =
                glm::clamp(camera->fovY, 10.0f * (float)M_PI / 180.0f, 140.0f * (float)M_PI / 180.0f);

            // Handle playback animation keys
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
                saveFrame();
            }
        }
    }

    // Render the scene
    Capsaicin::Render();

    if (!benchmarkMode)
    {
        // Re-enable Tonemap after save to disk
        if (reenableToneMap)
        {
            Capsaicin::setOption("tonemap_enable", true);
            reenableToneMap = false;
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

    return true;
}

bool CapsaicinMain::renderGUI() noexcept
{
    // Show the GUI
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin(
        programName.data(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    {
        if (!benchmarkMode)
        {
            // Select which scene to display
            int32_t selectedScene = static_cast<int32_t>(currentScene);
            string  sceneList;
            for (auto &i : scenes)
            {
                sceneList += i.name;
                sceneList += '\0';
            }
            if (ImGui::Combo("Scene", &selectedScene, sceneList.c_str(), static_cast<int32_t>(scenes.size())))
            {
                if (currentScene != static_cast<Scene>(selectedScene))
                {
                    // Change the selected scene
                    if (!loadScene(static_cast<Scene>(selectedScene)))
                    {
                        ImGui::End();
                        return false;
                    }
                    // Reset environment map
                    auto const environmentMap =
                        scenes[static_cast<uint32_t>(selectedScene)].useEnvironmentMap
                            ? (currentEnvironmentMap != EnvironmentMap::None ? currentEnvironmentMap
                                                                             : defaultEnvironmentMap)
                            : EnvironmentMap::None;
                    if (!setEnvironmentMap(environmentMap))
                    {
                        ImGui::End();
                        return false;
                    }
                }
            }

            // Optionally select which environment map is used
            if (scenes[static_cast<uint32_t>(selectedScene)].useEnvironmentMap)
            {
                string  emList;
                int32_t selectedEM = static_cast<int32_t>(currentEnvironmentMap);
                for (auto &i : sceneEnvironmentMaps)
                {
                    if (i.first == "Atmosphere" && !Capsaicin::hasOption<bool>("atmosphere_enable")) continue;
                    emList += i.first;
                    emList += '\0';
                }
                if (ImGui::Combo(
                        "Environment Map", &selectedEM, emList.c_str(), static_cast<int32_t>(emList.size())))
                {
                    if (currentEnvironmentMap != static_cast<EnvironmentMap>(selectedEM))
                    {
                        // Change the selected environment map
                        if (!setEnvironmentMap(static_cast<EnvironmentMap>(selectedEM)))
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

    return true;
}

void CapsaicinMain::renderCameraDetails() noexcept
{
    if (ImGui::CollapsingHeader("Camera settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Select which preset camera to use
        string     cameraList;
        auto const cameras        = Capsaicin::GetSceneCameras();
        int32_t    selectedCamera = static_cast<int32_t>(
            std::find(cameras.begin(), cameras.end(), Capsaicin::GetSceneCurrentCamera()) - cameras.begin());
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
                setCamera(cameras[selectedCamera]);
            }
        }

        auto const camera    = Capsaicin::GetSceneCamera();
        float      fovf      = glm::degrees(camera->fovY);
        int32_t    fov       = static_cast<int32_t>(fovf);
        float      remainder = fovf - static_cast<float>(fov);
        ImGui::DragInt("FOV", &fov, 1, 10, 140);
        camera->fovY = glm::radians(static_cast<float>(fov) + remainder);
        ImGui::DragFloat("Speed", &cameraSpeed, 0.01f);

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
                    "Look At", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoReorder);
                ImGui::TableSetupScrollFreeze(3, 3);
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                auto &eye      = camera->eye;
                auto &center   = camera->center;
                auto  rotation = glm::degrees(glm::eulerAngles(
                    glm::quatLookAt(glm::normalize(center - camera->eye), vec3(0.0f, 1.0f, 0.0f))));
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", eye.x);
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", rotation.x);
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", center.x);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", eye.y);
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", rotation.y);
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", center.y);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", eye.z);
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", rotation.z);
                ImGui::TableNextColumn();
                ImGui::Text("%.5f", center.z);
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
}

bool CapsaicinMain::renderGUIDetails() noexcept
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
        constexpr char const *playModes[2] = {"Real-time", "Fixed Frame Rate"};

        int32_t playMode = static_cast<int32_t>(Capsaicin::GetFixedFrameRate());
        if (ImGui::Combo("Play mode", &playMode, playModes, 2))
        {
            Capsaicin::SetFixedFrameRate(playMode > 0);
        }
        if (!Capsaicin::HasAnimation())
        {
            ImGui::EndDisabled();
        }
        ImVec2     buttonHeight(0.0f, 30.0f);
        char const restartGlyph[] = {static_cast<char>(0xF0), static_cast<char>(0x9F),
            static_cast<char>(0x94), static_cast<char>(0x83),
            static_cast<char>(0x0)}; // Workaround compiler not handling u8"\u1F503" properly
        if (ImGui::Button(restartGlyph, buttonHeight)) // Restart
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
        if (ImGui::Button(reinterpret_cast<char const *>(u8"\u23E9"), buttonHeight)) // Fast forward
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

    if (ImGui::CollapsingHeader("Render settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Select which renderer to use
        string  rendererString;
        auto    rendererList     = Capsaicin::GetRenderers();
        int32_t selectedRenderer = static_cast<int32_t>(
            find(rendererList.cbegin(), rendererList.cend(), Capsaicin::GetCurrentRenderer())
            - rendererList.cbegin());
        int32_t currentRenderer = selectedRenderer;
        for (auto &i : rendererList)
        {
            rendererString += i;
            rendererString += '\0';
        }
        if (ImGui::Combo("Renderer", &selectedRenderer, rendererString.c_str(), 8))
        {
            if (currentRenderer != selectedRenderer)
            {
                // Change the selected renderer
                if (!setRenderer(rendererList[selectedRenderer]))
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
        string  debugString;
        auto    debugList = Capsaicin::GetDebugViews();
        int32_t selectedDebug =
            static_cast<int32_t>(find(debugList.cbegin(), debugList.cend(), Capsaicin::GetCurrentDebugView())
                                 - debugList.cbegin());
        selectedDebug        = std::max(selectedDebug, 0); // Reset to 0 if unfound
        int32_t currentDebug = selectedDebug;
        for (auto &i : debugList)
        {
            debugString += i;
            debugString += '\0';
        }
        if (ImGui::Combo("Debug View", &selectedDebug, debugString.c_str(), 8))
        {
            if (currentDebug != selectedDebug)
            {
                // Change the selected view
                Capsaicin::SetDebugView(debugList[selectedDebug]);
            }
        }
        if (ImGui::Button("Reload Shaders (F5)"))
        {
            Capsaicin::ReloadShaders();
        }
        if (ImGui::Button("Dump Frame (F6)"))
        {
            saveFrame();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Save as JPEG", &saveAsJPEG);
    }
    return true;
}

void CapsaicinMain::saveFrame() noexcept
{
    // Ensure output directory exists
    std::string savePath = "./dump/"s;
    {
        std::filesystem::path outPath = savePath;
        std::error_code       ec;
        if (!exists(outPath, ec))
        {
            create_directory(outPath, ec);
        }
    }
    savePath = getSaveName();
    if (!benchmarkModeSuffix.empty())
    {
        savePath += '_';
        savePath += benchmarkModeSuffix;
    }
    savePath += '_';
    uint32_t frameIndex = Capsaicin::GetFrameIndex() + 1; //+1 to correct for 0 indexed
    savePath += to_string(frameIndex);
    savePath += '_';
    savePath += to_string(Capsaicin::GetAverageFrameTime());
    if (saveAsJPEG)
    {
        savePath += ".jpeg"sv;
    }
    else
    {
        savePath += ".exr"sv;
    }
    // Save the current frame buffer to disk
    Capsaicin::DumpAOVBuffer(savePath.c_str(), "Color");

    // Disable performing tone mapping as we output in HDR
    if (!saveAsJPEG && Capsaicin::hasOption<bool>("tonemap_enable"))
    {
        reenableToneMap = Capsaicin::getOption<bool>("tonemap_enable");
        Capsaicin::setOption("tonemap_enable", false);
    }
}

std::string CapsaicinMain::getSaveName() const noexcept
{
    std::string savePath = "./dump/"s;

    auto currentScenes = Capsaicin::GetCurrentScenes();
    GFX_ASSERT(!currentScenes.empty());
    auto currentSceneName = currentScenes[0];
    currentSceneName.erase(currentSceneName.length() - 5); // Remove the '.gltf' extension
    auto const sceneFolders = currentSceneName.find_last_of("/\\");
    if (sceneFolders != std::string::npos)
    {
        currentSceneName.erase(0, sceneFolders + 1);
    }
    auto currentEM = Capsaicin::GetCurrentEnvironmentMap();
    if (!currentEM.empty())
    {
        currentEM.erase(currentEM.length() - 4); // Remove the '.hdr' extension
        auto const emFolders = currentEM.find_last_of("/\\");
        if (emFolders != std::string::npos)
        {
            currentEM.erase(0, emFolders + 1);
        }
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
    savePath.erase(std::remove_if(savePath.begin(), savePath.end(),
                       [](unsigned char const c) { return std::isspace(c); }),
        savePath.end());
    return savePath;
}
