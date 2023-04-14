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

#define _USE_MATH_DEFINES
#include "main_shared.h"

#include <CLI/CLI.hpp>
#include <chrono>
#include <cmath>
#include <gfx_imgui.h>
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
    uint32_t exit;
    uint32_t pause;
};

KeyboardMapping keyboardMappings[] = {
    {0x57 /*W*/, 0x53 /*S*/, 0x44 /*D*/, 0x41 /*A*/,       0x45 /*E*/,         0x51 /*Q*/, 0x1B /*Esc*/,0x20 /*Space*/    },
    {0x5A /*Z*/, 0x53 /*S*/, 0x44 /*D*/, 0x51 /*Q*/, 0x21 /*Page Up*/, 0x22 /*Page Down*/, 0x1B /*Esc*/,
     0x20 /*Space*/}
};

/** Data required to represent each supported scene file */
struct SceneData
{
    string_view name;
    string_view fileName;
    bool        useEnvironmentMap;
    float       renderExposure;
};

/** List of supported scene files and associated data */
static vector<SceneData> const scenes = {
    {      "Flying World",
     "assets/CapsaicinTestMedia/flying_world_battle_of_the_trash_god/FlyingWorld-BattleOfTheTrashGod.gltf",  true, 2.5f},
    {       "Gas Station",                         "assets/CapsaicinTestMedia/gas_station/GasStation.gltf",  true, 1.5f},
    {  "Tropical Bedroom",               "assets/CapsaicinTestMedia/tropical_bedroom/TropicalBedroom.gltf",  true, 2.0f},
};

/** List of supported environment maps */
static vector<pair<string_view, string_view>> const sceneEnvironmentMaps = {
    {                    "None",                                         ""                                },
    {"Photo Studio London Hall",
     "assets/CapsaicinTestMedia/environment_maps/photo_studio_london_hall_4k.hdr"                          },
    {              "Kiara Dawn",           "assets/CapsaicinTestMedia/environment_maps/kiara_1_dawn_4k.hdr"},
    {        "Nagoya Wall Path",       "assets/CapsaicinTestMedia/environment_maps/nagoya_wall_path_4k.hdr"},
    {        "Spaichingen Hill",       "assets/CapsaicinTestMedia/environment_maps/spaichingen_hill_4k.hdr"},
    {            "Studio Small",        "assets/CapsaicinTestMedia/environment_maps/studio_small_08_4k.hdr"},
    {                   "White",                     "assets/CapsaicinTestMedia/environment_maps/white.hdr"},
    {              "Atmosphere",                                                                         ""},
};

CapsaicinMain::CapsaicinMain(string_view &&programNameIn) noexcept
    : programName(forward<string_view>(programNameIn))
{}

CapsaicinMain::~CapsaicinMain() noexcept
{
    // Destroy Capsaicin context
    gfxImGuiTerminate();
    Capsaicin::Terminate();
    gfxDestroyScene(sceneData);

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
    while (renderFrame())
    {
        // Check for change
        if (updateRenderer)
        {
            setRenderer();
            updateRenderer = false;
        }
        if (updateScene)
        {
            if (!loadScene())
            {
                return false;
            }
            updateScene = false;
        }
        if (updateEnvironmentMap)
        {
            if (!setEnvironmentMap())
            {
                return false;
            }
            updateEnvironmentMap = false;
        }
        if (updateCamera)
        {
            setCamera();
            updateCamera = false;
        }

        // Check benchmark mode run
        if (benchmarkMode)
        {
            // If current frame has reached our benchmark value then dump frame
            if (Capsaicin::GetFrameIndex() == benchmarkModeFrameCount)
            {
                saveFrame();
            }
            else if (Capsaicin::GetFrameIndex() > benchmarkModeFrameCount)
            {
                // Need to wait a single render pass for the frame saving to complete before closing
                return true;
            }
        }
    }

    return true;
}

void CapsaicinMain::printString(std::string const &text) noexcept
{
    // Check if a debugger is attached and use it instead of a console
    // If no debugger is attached then we need to attach to a console process in order to be able to
    // output text
    if (IsDebuggerPresent())
    {
        OutputDebugStringA(text.c_str());
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
                {
                    // Force the console buffer to be resized no matter what as this forces the console to
                    // update the end line to match the number of printed lines from this app
                    constexpr int16_t minLength = 4096;
                    scInfo.dwSize.Y             = std::max(minLength, (short)(scInfo.dwSize.Y + 1));
                    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), scInfo.dwSize);
                }

                hasConsole = true;
            }
            else
            {
                return;
            }
        }
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
        buffer.resize(cursorPosX * sizeof(CHAR_INFO));
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
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', cursorPosX, {0, cursorPosY}, &dnc);
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), {0, cursorPosY});

        // Write out each new line from the input text
        uint32_t lines = 0;
        for (auto const i : std::views::split(text, '\n'))
        {
            DWORD dnc;
            WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), &*i.begin(), (DWORD)i.size(), &dnc, 0);
            WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), "\n", 1, &dnc, 0);
            ++lines;
        }

        // Restore cursor position to previously saved state and increment by number of new lines
        textRegion = {
            .Left   = 0,
            .Top    = (short)(cursorPosY + lines),
            .Right  = (short)(cursorPosX - 1),
            .Bottom = (short)(cursorPosY + lines),
        };
        WriteConsoleOutput(
            GetStdHandle(STD_OUTPUT_HANDLE), buffer.data(), bufferSize, coordinates, &textRegion);
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), {cursorPosX, short(cursorPosY + lines)});
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
    uint32_t sceneSelect = static_cast<uint32_t>(scene);
    app.add_option("--start-scene-index", sceneSelect, "Start scene index")
        ->capture_default_str()
        ->check(CLI::Range(0u, (uint32_t)scenes.size() - 1));
    uint32_t envMapSelect = static_cast<uint32_t>(environmentMap);
    app.add_option("--start-environment-map-index", envMapSelect, "Start environment map index")
        ->capture_default_str()
        ->check(CLI::Range(0u, (uint32_t)sceneEnvironmentMaps.size() - 1));
    auto     renderers        = Capsaicin::GetRenderers();
    auto     rendererSelectIt = find(renderers.begin(), renderers.end(), renderSettings.renderer_);
    uint32_t rendererSelect   = 0;
    if (rendererSelectIt != renderers.end())
    {
        rendererSelect = static_cast<uint32_t>(rendererSelectIt - renderers.begin());
    }
    app.add_option("--start-renderer-index", rendererSelect, "Start renderer index")
        ->capture_default_str()
        ->check(CLI::Range(0u, (uint32_t)renderers.size() - 1));
    uint32_t cameraSelect = cameraIndex;
    app.add_option("--start-camera-index", cameraSelect, "Start camera index");
    auto bench = app.add_flag("--benchmark-mode", benchmarkMode, "Enable benchmarking mode");
    app.add_option(
           "--benchmark-frames", benchmarkModeFrameCount, "Number of frames to render during benchmark mode")
        ->needs(bench)
        ->capture_default_str();

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

        printString("Command Line Error: "s + ((exception)e).what());
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

    // Setup passed in values
    scene                    = static_cast<Scene>(sceneSelect);
    environmentMap           = static_cast<EnvironmentMap>(envMapSelect);
    renderSettings.renderer_ = renderers[rendererSelect];

    // Create the internal gfx window and context
    window = gfxCreateWindow(windowWidth, windowHeight, programName.data());
    if (!window)
    {
        return false;
    }
    if (!reset())
    {
        return false;
    }

    // Initialise render settings
    setRenderer();

    // Load the requested start scene
    if (!loadScene())
    {
        return false;
    }

    // Check the passed in camera index
    if (cameraIndex != cameraSelect)
    {
        if (cameraSelect >= gfxSceneGetCameraCount(sceneData))
        {
            printString("Invalid value passed in for '--start-camera-index'");
            return false;
        }
        cameraIndex = cameraSelect;
        setCamera();
    }

    return true;
}

bool CapsaicinMain::reset() noexcept
{
    // Restart capsaicin to prevent resource issues on change
    //  - this prevents freezes due to capsaicin not releasing resources properly
    if (contextGFX)
    {
        gfxImGuiTerminate();
        Capsaicin::Terminate();
        gfxDestroyContext(contextGFX);
    }

    contextGFX = gfxCreateContext(
        window, kGfxCreateContextFlag_EnableStablePowerState
#if _DEBUG
                    | kGfxCreateContextFlag_EnableDebugLayer | kGfxCreateContextFlag_EnableShaderDebugging
#endif
    );
    if (!contextGFX)
    {
        return false;
    }

    // Create Capsaicin render context
    Capsaicin::Initialize(contextGFX);
    if (auto err = gfxImGuiInitialize(contextGFX); err != kGfxResult_NoError)
    {
        return false;
    }

    // Reset render settings animation state
    Capsaicin::SetSequenceTime(0.0);
    restartAnimation();
    setAnimation(false);

    // Reset frame graph
    frameGraph.reset();

    // Reset time
    auto wallClock =
        chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch());
    previousTime = wallClock.count() / 1000000.0;
    currentTime  = previousTime;
    frameTime    = 0.0f;

    // Reset camera movement
    cameraTranslation = vec3(0.0f);
    cameraRotation    = vec2(0.0f);

    return true;
}

bool CapsaicinMain::loadScene() noexcept
{
    // Clear any pre-existing scene data
    if (sceneData)
    {
        gfxDestroyScene(sceneData);
        if (!reset())
        {
            return false;
        }
    }
    renderSettings.environment_map_ = {};
    // Create new blank scene
    sceneData = gfxCreateScene();
    if (!sceneData)
    {
        return false;
    }

    // Create default user camera
    auto userCamera    = gfxSceneCreateCamera(sceneData);
    userCamera->eye    = {0.0f, 0.0f, -1.0f};
    userCamera->center = {0.0f, 0.0f, 0.0f};
    userCamera->up     = {0.0f, 1.0f, 0.0f};

    // Load in environment map based on current settings
    if (scenes[static_cast<uint32_t>(scene)].useEnvironmentMap)
    {
        if (!setEnvironmentMap())
        {
            return false;
        }
    }
    // Load in scene based on current requested scene index
    if (gfxSceneImport(sceneData, scenes[static_cast<uint32_t>(scene)].fileName.data()) != kGfxResult_NoError)
    {
        return false;
    }

    if (!scenes[static_cast<uint32_t>(scene)].useEnvironmentMap)
    {
        // Load a null image
        renderSettings.environment_map_ = GfxConstRef<GfxImage>();
    }

    setSceneRenderOptions(true);

    // Set up camera based on internal scene data
    cameraIndex = 0;
    if (gfxSceneGetCameraCount(sceneData) > 1)
    {
        cameraIndex = 1; // Use first scene camera
        // Try and find 'Main' camera
        for (uint32_t i = 1; i < gfxSceneGetCameraCount(sceneData); ++i)
        {
            auto        cameraHandle = gfxSceneGetCameraHandle(sceneData, i);
            string_view cameraName   = gfxSceneGetCameraMetadata(sceneData, cameraHandle).getObjectName();
            if (cameraName.find("Main"sv) != string::npos)
            {
                cameraIndex = i;
            }
        }
        // Set user camera equal to first camera
        auto defaultCamera = gfxSceneGetCameraHandle(sceneData, cameraIndex);
        userCamera->eye    = defaultCamera->eye;
        userCamera->center = defaultCamera->center;
        userCamera->up     = defaultCamera->up;
    }
    setCamera();

    // Calculate some scene stats
    triangleCount = 0;
    for (uint32_t i = 0; i < gfxSceneGetObjectCount<GfxInstance>(sceneData); ++i)
    {
        if (gfxSceneGetObjects<GfxInstance>(sceneData)[i].mesh)
        {
            GfxMesh const &mesh = *gfxSceneGetObjects<GfxInstance>(sceneData)[i].mesh;
            triangleCount += (uint32_t)(mesh.indices.size() / 3);
        }
    }

    return true;
}

void CapsaicinMain::setCamera() noexcept
{
    // Set the camera to the currently requested camera index
    GFX_ASSERT(cameraIndex < gfxSceneGetCameraCount(sceneData));
    camera         = gfxSceneGetCameraHandle(sceneData, cameraIndex);
    camera->aspect = static_cast<float>(gfxGetBackBufferWidth(contextGFX))
                   / static_cast<float>(gfxGetBackBufferHeight(contextGFX));
    gfxSceneSetActiveCamera(sceneData, camera);

    // Reset camera movement data
    cameraTranslation = glm::vec3(0.0f);
    cameraRotation    = glm::vec2(0.0f);
}

bool CapsaicinMain::setEnvironmentMap() noexcept
{
    if (sceneEnvironmentMaps[static_cast<uint32_t>(environmentMap)].first == "Atmosphere")
    {
        // The atmosphere technique overrides current environment map
        renderSettings.setOption<bool>("atmosphere_enable", true);
        return true;
    }
    else if (renderSettings.hasOption<bool>("atmosphere_enable"))
    {
        renderSettings.setOption<bool>("atmosphere_enable", false);
    }

    // Remove the old environment map
    if (renderSettings.environment_map_)
    {
        auto handle = gfxSceneGetImageHandle(sceneData, renderSettings.environment_map_.getIndex());
        gfxSceneDestroyImage(sceneData, handle);
    }

    if (sceneEnvironmentMaps[static_cast<uint32_t>(environmentMap)].first == "None")
    {
        // Don't load new map
        renderSettings.environment_map_ = GfxConstRef<GfxImage>();
        return true;
    }

    // Load in the new environment map
    if (gfxSceneImport(sceneData, sceneEnvironmentMaps[static_cast<uint32_t>(environmentMap)].second.data())
        != kGfxResult_NoError)
    {
        return false;
    }

    // Update render settings
    renderSettings.environment_map_ = gfxSceneFindObjectByAssetFile<GfxImage>(
        sceneData, sceneEnvironmentMaps[static_cast<uint32_t>(environmentMap)].second.data());
    return true;
}

void CapsaicinMain::setRenderer() noexcept
{
    // Change render settings based on currently selected renderer
    renderSettings.debug_view_ = "None";

    if (sceneData)
    {
        // If we are already loaded and the renderer is changed then destroy capsaicin and reload
        reset();
    }

    setSceneRenderOptions();
}

void CapsaicinMain::setSceneRenderOptions(bool force) noexcept
{
    if (!force && renderSettings.hasOption<float>("tonemap_exposure"))
    {
        return;
    }
    // Set render settings based on current scene
    renderSettings.setOption("tonemap_exposure", scenes[static_cast<uint32_t>(scene)].renderExposure);
}

void CapsaicinMain::setPlayMode(Capsaicin::PlayMode playMode) noexcept
{
    if (renderSettings.play_mode_ != playMode)
    {
        renderSettings.play_mode_ = playMode;
        if (renderSettings.play_mode_ == Capsaicin::kPlayMode_FrameByFrame)
        {
            renderSettings.play_to_frame_index_ = Capsaicin::GetFrameIndex();
            setAnimation(true);
        }
        else if (renderSettings.play_mode_ == Capsaicin::kPlayMode_None)
        {
            // Check if state was previously paused before it was changed to frame-by-frame
            if (renderSettings.delta_time_ > 0.0f)
            {
                setAnimation(false);
            }
        }
    }
}

void CapsaicinMain::setAnimation(bool animate) noexcept
{
    renderSettings.play_from_start_ = !animate;
    if (renderSettings.play_mode_ == Capsaicin::kPlayMode_None)
    {
        if (animate)
        {
            Capsaicin::SetSequenceTime(renderSettings.delta_time_);
            renderSettings.delta_time_ = 0.0f;
        }
        else
        {
            renderSettings.delta_time_ = (float)Capsaicin::GetSequenceTime() + FLT_EPSILON;
        }
    }
    Capsaicin::SetAnimate(animate);
}

void CapsaicinMain::toggleAnimation() noexcept
{
    bool const paused = !getAnimation();
    setAnimation(paused);
}

void CapsaicinMain::restartAnimation() noexcept
{
    // Reset animations to start
    renderSettings.play_from_start_     = true;
    renderSettings.delta_time_          = renderSettings.delta_time_ == 0.0f ? 0.0f : FLT_EPSILON;
    renderSettings.play_to_frame_index_ = 1;
    Capsaicin::SetSequenceTime(0.0);
}

bool CapsaicinMain::getAnimation() noexcept
{
    return Capsaicin::GetAnimate();
}

void CapsaicinMain::tickAnimation() noexcept
{
    if (renderSettings.play_mode_ == Capsaicin::kPlayMode_FrameByFrame)
    {
        renderSettings.play_from_start_ = false;
        return;
    }
    bool const paused               = !getAnimation();
    renderSettings.play_from_start_ = paused;
}

uint32_t CapsaicinMain::getCurrentAnimationFrame() noexcept
{
    return (uint32_t)((float)Capsaicin::GetSequenceTime() / renderSettings.frame_by_frame_delta_time_);
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
    if (gfxWindowIsCloseRequested(window)
        || gfxWindowIsKeyReleased(window, keyboardMappings[static_cast<uint32_t>(kbMap)].exit))
    {
        return false;
    }

    // Get events
    gfxWindowPumpEvents(window);

    if (!benchmarkMode)
    {
        // Update the camera
        if (renderSettings.play_mode_ != Capsaicin::kPlayMode_FrameByFrame)
        {
            vec3 const  forward      = normalize(camera->center - camera->eye);
            vec3 const  right        = cross(forward, camera->up);
            vec3 const  up           = cross(right, forward);
            vec3        acceleration = cameraTranslation * -30.0f;
            float const force        = cameraSpeed * 10000.0f;

            // Clamp frametime to prevent errors at low frame rates
            frameTime = glm::min(frameTime, 0.05f);

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
            ImGui::ResetMouseDragDelta(0);

            if (!glm::all(glm::equal(cameraTranslation, vec3(0.0f)))
                || !glm::all(glm::equal(cameraRotation, vec2(0.0f))))
            {
                if (cameraIndex != 0)
                {
                    // Change to the user camera
                    auto userCamera = gfxSceneGetCameraHandle(sceneData, 0);
                    *userCamera     = *camera;
                    cameraIndex     = 0;
                    setCamera();
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
        }

        // Hot-reload the shaders if requested
        if (gfxWindowIsKeyReleased(window, VK_F5))
        {
            gfxKernelReloadAll(contextGFX);
        }

        // Save image to disk if requested
        if (gfxWindowIsKeyReleased(window, VK_F6))
        {
            saveFrame();
        }

        // Pause/Resume animations if requested
        if (gfxWindowIsKeyReleased(window, keyboardMappings[static_cast<uint32_t>(kbMap)].pause))
        {
            toggleAnimation();
        }
    }

    // Render the scene
    Capsaicin::Render(sceneData, renderSettings);

    if (!benchmarkMode)
    {
        // Re-enable Tonemap after save to disk
        if (reenableToneMap)
        {
            renderSettings.setOption("tonemap_enable", true);
            reenableToneMap = false;
        }
    }

    // Progress any animation state
    tickAnimation();

    // Render the UI
    renderGUI();

    // Complete the frame
    gfxFrame(contextGFX);

    // Update frame time
    auto wallTime =
        chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch());
    currentTime  = wallTime.count() / 1000000.0;
    frameTime    = static_cast<float>(currentTime - previousTime);
    previousTime = currentTime;

    return true;
}

bool CapsaicinMain::renderGUI() noexcept
{
    // Show the GUI
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin(
        programName.data(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    {
        ImGui::Text("Selected device :  %s", contextGFX.getName());
        ImGui::Separator();

        if (!benchmarkMode)
        {
            // Select which scene to display
            int32_t selectedScene = static_cast<uint32_t>(scene);
            string  sceneList;
            for (auto &i : scenes)
            {
                sceneList += i.name;
                sceneList += '\0';
            }
            if (ImGui::Combo("Scene", &selectedScene, sceneList.c_str(), static_cast<int32_t>(scenes.size())))
            {
                if (static_cast<uint32_t>(scene) != selectedScene)
                {
                    // Change the selected scene
                    scene       = static_cast<Scene>(selectedScene);
                    updateScene = true;
                }
            }

            // Optionally select which environment map is used
            if (scenes[static_cast<uint32_t>(scene)].useEnvironmentMap)
            {
                int32_t selectedEM = static_cast<uint32_t>(environmentMap);
                string  emList;
                for (auto &i : sceneEnvironmentMaps)
                {
                    if (i.first == "Atmosphere" && !renderSettings.hasOption<bool>("atmosphere_enable"))
                        continue;
                    emList += i.first;
                    emList += '\0';
                }
                if (ImGui::Combo(
                        "Environment Map", &selectedEM, emList.c_str(), static_cast<int32_t>(emList.size())))
                {
                    if (static_cast<uint32_t>(environmentMap) != selectedEM)
                    {
                        // Change the selected environment map
                        environmentMap       = static_cast<EnvironmentMap>(selectedEM);
                        updateEnvironmentMap = true;
                    }
                }
            }

            ImGui::Text("Triangle Count            :  %u", triangleCount);
            const uint32 deltaLightCount = Capsaicin::GetDeltaLightCount();
            const uint32 areaLightCount  = Capsaicin::GetAreaLightCount();
            const uint32 envLightCount   = Capsaicin::GetEnvironmentLightCount();
            ImGui::Text("Light Count               :  %u", areaLightCount + deltaLightCount + envLightCount);
            ImGui::Text("  Area Light Count        :  %u", areaLightCount);
            ImGui::Text("  Delta Light Count       :  %u", deltaLightCount);
            ImGui::Text("  Environment Light Count :  %u", envLightCount);
            ImGui::Text("Render Resolution         :  %ux%u", gfxGetBackBufferWidth(contextGFX),
                gfxGetBackBufferHeight(contextGFX));

            // Call the class specific GUI function
            renderGUIDetails();
        }
        else
        {
            // Display profiling options
            renderProfiling();
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

bool CapsaicinMain::renderCameraDetails() noexcept
{
    if (ImGui::CollapsingHeader("Camera settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Select which preset camera to use
        int32_t selectedCamera = cameraIndex;
        string  cameraList;
        for (uint32_t i = 0; i < gfxSceneGetCameraCount(sceneData); ++i)
        {
            if (i > 0)
            {
                auto        cameraHandle = gfxSceneGetCameraHandle(sceneData, i);
                string_view cameraName   = gfxSceneGetCameraMetadata(sceneData, cameraHandle).getObjectName();
                if (cameraName.find("Camera"sv) == 0 && cameraName.length() > 6)
                {
                    cameraName = cameraName.substr(6);
                }
                cameraList += cameraName;
            }
            else
            {
                cameraList += "User"sv;
            }
            cameraList += '\0';
        }
        if (ImGui::Combo(
                "Camera", &selectedCamera, cameraList.c_str(), static_cast<int32_t>(cameraList.size())))
        {
            if (cameraIndex != selectedCamera)
            {
                // Change the selected camera
                cameraIndex  = selectedCamera;
                updateCamera = true;
            }
        }

        float   fovf      = glm::degrees(camera->fovY);
        int32_t fov       = static_cast<int32_t>(fovf);
        float   remainder = fovf - static_cast<float>(fov);
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
    return true;
}

void CapsaicinMain::renderGUIDetails() noexcept
{
    // Display camera options
    renderCameraDetails();

    if (ImGui::CollapsingHeader("Render settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Select which renderer to use
        string  rendererString;
        auto    rendererList = Capsaicin::GetRenderers();
        int32_t selectedRenderer =
            static_cast<int32_t>(find(rendererList.cbegin(), rendererList.cend(), renderSettings.renderer_)
                                 - rendererList.cbegin());
        int32_t currentRenderer = selectedRenderer;
        for (auto &i : rendererList)
        {
            rendererString += i;
            rendererString += '\0';
        }
        auto renderer = renderSettings.renderer_;
        if (ImGui::Combo("Renderer", &selectedRenderer, rendererString.c_str(), 8))
        {
            if (currentRenderer != selectedRenderer)
            {
                // Change the selected renderer
                renderSettings.renderer_ = rendererList[selectedRenderer];
                updateRenderer           = true;
            }
        }
        // Light sampling settings
        if (renderSettings.hasOption<bool>("delta_light_enable") &&
            ImGui::CollapsingHeader("Light Sampler Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enable Delta Lights", &renderSettings.getOption<bool>("delta_light_enable"));
            ImGui::Checkbox("Enable Area Lights", &renderSettings.getOption<bool>("area_light_enable"));
            ImGui::Checkbox(
                "Enable Environment Lights", &renderSettings.getOption<bool>("environment_light_enable"));
        }
        // Display renderer specific options
        if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (renderSettings.hasOption<bool>("tonemap_enable"))
            {
                // Tone mapping settings
                bool &enabled = renderSettings.getOption<bool>("tonemap_enable");
                if (!enabled) ImGui::BeginDisabled(true);
                ImGui::DragFloat("Exposure", &renderSettings.getOption<float>("tonemap_exposure"), 5e-3f);
                if (!enabled) ImGui::EndDisabled();
                ImGui::Checkbox("Enable Tone Mapping", &enabled);
            }
            if (renderer == "Path Tracer")
            {
                ImGui::DragInt("Samples Per Pixel",
                    (int32_t *)&renderSettings.getOption<uint32_t>("reference_pt_sample_count"), 1, 0, 30);
                auto &bounces = renderSettings.getOption<uint32_t>("reference_pt_bounce_count");
                ImGui::DragInt("Bounces", (int32_t *)&bounces, 1, 0, 30);
                auto &minBounces = renderSettings.getOption<uint32_t>("reference_pt_min_rr_bounces");
                ImGui::DragInt("Min Bounces", (int32_t *)&minBounces, 1, 0, bounces);
                minBounces = glm::min(minBounces, bounces);
                ImGui::Checkbox("Disable Albedo Textures",
                    &renderSettings.getOption<bool>("reference_pt_disable_albedo_materials"));
                ImGui::Checkbox("Disable Direct Lighting",
                    &renderSettings.getOption<bool>("reference_pt_disable_direct_lighting"));
                ImGui::Checkbox("Disable Specular Lighting",
                    &renderSettings.getOption<bool>("reference_pt_disable_specular_lighting"));
            }
            else if (renderer == "GI-1.0")
            {
                ImGui::Checkbox("Use TAA", &renderSettings.getOption<bool>("taa_enable"));
                ImGui::Checkbox("Use Resampling", &renderSettings.getOption<bool>("gi10_use_resampling"));
                ImGui::Checkbox(
                    "Use Direct Lighting", &renderSettings.getOption<bool>("gi10_use_direct_lighting"));
                ImGui::Checkbox("Disable Albedo Textures",
                    &renderSettings.getOption<bool>("gi10_disable_albedo_textures"));
            }
        }
        ImGui::Separator();
    }

    // Display animation options
    if (gfxSceneGetAnimationCount(sceneData) > 0)
    {
        if (ImGui::CollapsingHeader("Animation Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int32_t playMode = (int32_t)renderSettings.play_mode_;
            if (ImGui::Combo(
                    "Play mode", &playMode, Capsaicin::g_play_modes, (int32_t)Capsaicin::kPlayMode_Count))
            {
                setPlayMode((Capsaicin::PlayMode)playMode);
            }
            else if (ImGui::Button("Restart"))
            {
                restartAnimation();
            }
            else if (renderSettings.play_mode_ == Capsaicin::kPlayMode_None)
            {
                string_view buttonLabel = getAnimation() ? "Pause"sv : "Play"sv;
                if (ImGui::Button(buttonLabel.data()))
                {
                    toggleAnimation();
                }
            }
            else if (renderSettings.play_mode_ == Capsaicin::kPlayMode_FrameByFrame)
            {
                uint32_t playToFrame    = getCurrentAnimationFrame();
                string   playFrameLabel = "Play Next Frame ("s + to_string(playToFrame) + ')';
                if (ImGui::Button(playFrameLabel.data()))
                {
                    renderSettings.play_to_frame_index_ = Capsaicin::GetFrameIndex() + 1;
                }
            }
        }
    }

    // Display profiling options
    renderProfiling();

    // Display debugging options
    if (ImGui::CollapsingHeader("Debugging", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Select which debug view to use
        string  debugString;
        auto    debugList     = Capsaicin::GetDebugViews();
        int32_t selectedDebug = static_cast<int32_t>(
            find(debugList.cbegin(), debugList.cend(), renderSettings.debug_view_) - debugList.cbegin());
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
                renderSettings.debug_view_ = debugList[selectedDebug];
            }
        }
        if (ImGui::Button("Reload Shaders (F5)"))
        {
            gfxKernelReloadAll(contextGFX);
        }
        if (ImGui::Button("Dump Frame (F6)"))
        {
            saveFrame();
        }
        renderOptions();
    }
}

void CapsaicinMain::renderProfiling() noexcept
{
    if (ImGui::CollapsingHeader("Profiling", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto [totalFrameTime, timestamps] = Capsaicin::GetProfiling();

        bool   children      = false;
        size_t maxStringSize = 0;
        for (auto &i : timestamps)
        {
            bool                     hasChildren = i.children_.size() > 1;
            const ImGuiTreeNodeFlags flags =
                (hasChildren ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf);

            children = children || hasChildren;
            if (ImGui::TreeNodeEx(i.name_.data(), flags, "%-20s: %.3f ms", i.children_[0].name_.data(),
                    i.children_[0].time_))
            {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
                maxStringSize = std::max(maxStringSize, i.children_[0].name_.length());
                for (uint32_t j = 1; j < i.children_.size(); ++j)
                {
                    const ImGuiTreeNodeFlags selectedFlag =
                        (selectedProfile.first == i.name_ && selectedProfile.second == i.children_[j].name_
                                ? ImGuiTreeNodeFlags_Selected
                                : ImGuiTreeNodeFlags_None);
                    ImGui::TreeNodeEx(std::to_string(j).c_str(),
                        ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | selectedFlag,
                        "%-17s:  %.3f ms", i.children_[j].name_.data(), i.children_[j].time_);

                    maxStringSize = std::max(maxStringSize, i.children_[j].name_.length());
                    if (ImGui::IsItemClicked())
                    {
                        selectedProfile =
                            std::make_pair(!selectedFlag ? i.name_ : nullptr, i.children_[j].name_);
                    }
                }

                ImGui::PopStyleColor();
                ImGui::TreePop();
            }
        }

        ImGui::Separator();

        frameGraph.addValue(totalFrameTime);
        const std::string graphName = std::format("{:.2f}", totalFrameTime) + " ms ("
                                    + std::format("{:.2f}", 1000.0f / totalFrameTime) + " fps)";

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
        ImGui::PlotLines("", CapsaicinMain::Graph::GetValueAtIndex, &frameGraph, frameGraph.getValueCount(),
            0, graphName.c_str(), 0.0f, FLT_MAX, ImVec2(150, 20));
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
        ImGui::Text(to_string(Capsaicin::GetFrameIndex()).c_str());
        ImGui::PopID();
    }
}

void CapsaicinMain::renderOptions() noexcept
{
    if (ImGui::CollapsingHeader("Render Options", ImGuiTreeNodeFlags_OpenOnArrow))
    {
        for (auto &i : renderSettings.options_)
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

void CapsaicinMain::saveFrame() noexcept
{
    // Save the current frame buffer to disk
    uint32_t frameIndex = Capsaicin::GetFrameIndex();
    string   savePath   = "./dump/"s;
    // Ensure output directory exists
    {
        std::filesystem::path outPath = savePath;
        std::error_code       ec;
        if (!exists(outPath, ec))
        {
            create_directory(outPath, ec);
        }
    }
    savePath += scenes[static_cast<uint32_t>(scene)].name;
    savePath += "_C";
    if (cameraIndex > 0)
    {
        auto        cameraHandle = gfxSceneGetCameraHandle(sceneData, cameraIndex);
        string_view cameraName   = gfxSceneGetCameraMetadata(sceneData, cameraHandle).getObjectName();
        if (cameraName.find("Camera"sv) == 0 && cameraName.length() > 6)
        {
            cameraName = cameraName.substr(6);
        }
        savePath += cameraName;
    }
    else
    {
        savePath += "User"sv;
    }
    savePath += "_R"sv;
    savePath += renderSettings.renderer_;
    savePath += "_F"sv;
    savePath += to_string(frameIndex);
    savePath += "_T"sv;
    double frameTime = frameGraph.getAverageValue();
    savePath += to_string(frameTime);
    // savePath += "AdditionalDescription"sv;
    savePath += ".exr"sv;
    savePath.erase(std::remove_if(savePath.begin(), savePath.end(),
                       [](unsigned char const c) { return std::isspace(c); }),
        savePath.end());
    Capsaicin::DumpAOVBuffer(savePath.c_str(), "Color");

    // Disable performing tone mapping as we output in HDR
    if (renderSettings.hasOption<bool>("tonemap_enable"))
    {
        reenableToneMap = renderSettings.getOption<bool>("tonemap_enable");
        renderSettings.setOption("tonemap_enable", false);
    }
}

uint32_t CapsaicinMain::Graph::getValueCount() const noexcept
{
    return static_cast<uint32_t>(values.size());
}

void CapsaicinMain::Graph::addValue(float value) noexcept
{
    values[current] = value;
    current         = (current + 1) % values.size();
}

float CapsaicinMain::Graph::getLastAddedValue() const noexcept
{
    if (current == 0) return getValueAtIndex(static_cast<uint32_t>(values.size() - 1));
    return getValueAtIndex(current - 1);
}

float CapsaicinMain::Graph::getValueAtIndex(uint32_t index) const noexcept
{
    return values[index];
}

float CapsaicinMain::Graph::getAverageValue() noexcept
{
    double   runningCount = 0.0;
    uint32_t validFrames  = 0;
    for (uint32_t i = 0; i < getValueCount(); ++i)
    {
        runningCount += (double)getValueAtIndex(i);
        if (getValueAtIndex(i) != 0.0f)
        {
            ++validFrames;
        }
    }
    return static_cast<float>(runningCount / (double)validFrames);
}

void CapsaicinMain::Graph::reset() noexcept
{
    current = 0;
    values.fill(0.0f);
}

float CapsaicinMain::Graph::GetValueAtIndex(void *object, int32_t index) noexcept
{
    Graph const  &graph      = *static_cast<Graph const *>(object);
    const int32_t offset     = (int32_t)(graph.values.size()) - index;
    const int32_t newIndex   = (int32_t)(graph.current) - offset;
    const int32_t fixedIndex = (newIndex < 0 ? (int32_t)(graph.values.size()) + newIndex : newIndex);
    return graph.values[fixedIndex];
}
