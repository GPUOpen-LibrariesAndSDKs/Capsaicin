#### [Index](../index.md) | Development

-----------------------

# Getting Started

When acquiring the code make sure to clone the repo with submodules included:

`git clone --recurse-submodules https://github.com/GPUOpen-LibrariesAndSDKs/Capsaicin`

If you have already cloned this repo without submodules, then use:

`git submodule update --init --recursive`

## Build

Capsaicin uses the [CMake](https://cmake.org/) build system.

There are several ways to get started compiling the source code:
- Visual Studio project
	- Use CMake to generate a Visual Studio project
	- `cmake CMakeLists.txt -B ./build -G "Visual Studio 17 2022"` (note other Visual Studio versions can be used)
	- Open the newly created project in Visual Studio and build as normal
- Build on command line
	- `cmake -S ./ -B ./build -A x64`
	- `cmake --build ./build --config RelWithDebInfo`

## Code Layout

Code is separated by functionality with the expectation that files (i.e. headers/source/shaders) will be grouped together in the same folder.

All source code is written using C++ and uses the `.cpp` file extension. Header files use the `.h` extension. This extension is also used for any files shared between host and device code. All device code uses the `.hlsl` extension.

- `assets` : Contains bundled test scenes
- `docs` : Contains the documentation
- `dump` : The default location for saved screenshots
- `shader_pdb` : The default location for saved shader debugging information
- `src` : Contains the frameworks source code
	- `core` : The location of the framework code
		- `include` : Contains the single `capsaicin.h` header file used to interface with the framework
		- `src`
			- `capsaicin` : Contains the main internal framework code
			- `lights` : Common HLSL headers for lighting evaluation/sampling
			- `materials` : Common HLSL headers for material evaluation/sampling
			- `math` : Common mathematical helper functions
			- `random` : Random number generators
			- `render_technique` : The location of all available render techniques (each within its own sub-folder)
			- `renderers` : All available renderers (each within its own sub-folder)
			- `utilities` : Reusable host side utility helpers (sort, reduce etc.)
	- `scene_viewer` : The default application
- `third_party` : Contains the submodules for any needed third party dependencies

See [Architecture](./architecture.md) for details on how the framework is designed and how this design corresponds to the above folder layout.

## Default Application (Scene_Viewer)

The default application has a variety of different functionality.

Available controls:

`Esc` - Quit

`F5` - Reload Shaders\
`F6` - Take Screenshot

`W` - Move the camera forward\
`A` - Move the camera left\
`S` - Move the camera backward\
`D` - Move the camera right\
`Q` - Move the camera up\
`E` - Move the camera down\
`Left Mouse (Hold) + Drag` - Rotate camera\
`Mouse Wheel` - Change movement speed\
`Mouse Wheel (Horizontal)` - Change Field of View

`Space` - Pause/Resume animations
