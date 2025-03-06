#### [Index](../index.md) | Development

-----------------------

# Getting Started

When acquiring the code make sure to clone the repo with submodules included:

`git clone --recurse-submodules https://github.com/Radeon-Pro/Capsaicin`

If you have already cloned this repo without submodules, then use:

`git submodule update --init --recursive`

## Build

Capsaicin uses the [CMake](https://cmake.org/) build system.

There are several ways to get started compiling the source code:
- Visual Studio project
    - Use CMake to generate a Visual Studio project
    - `cmake CMakeLists.txt -B ./build -G "Visual Studio 17 2022"` (note other Visual Studio versions can be used)
        - or use CMakeGUI as a graphical front end to create the project
    - Open the newly created project in Visual Studio and build as normal
- Open directly in VS Code or Visual Studio
    - Both VS Code and newer versions of Visual Studio enable loading CMake projects directly (see corresponding documentation for your IDE)
- Build on command line
    - `cmake -S ./ -B ./build -A x64`
    - `cmake --build ./build --config RelWithDebInfo`

When running CMake for the first time it will attempt to gain access to any additional third party dependencies required by Capsaicin. For each of these dependencies an existing installed package will be searched for and in cases were one cannot be found then a local copy will be downloaded into the projects "third_party" subfolder.

If a centralised package management system should be used for resolving dependencies then ensure that CMake has been setup to use this package system before running CMake for the first time over the project (refer to instructions supplied with the package managers for details).

The used third party dependencies and there expected names are:
- CLI11: CLI11 is a command line parser for C++11 and beyond
- yaml-cpp: yaml-cpp is a YAML parser and emitter in C++
- nlohmann-json: JSON for Modern C++
- onnxruntime-gpu: Cross-platform, high performance ML inferencing and training accelerator 
- directml: High-performance, hardware-accelerated DirectX 12 library for machine learning
- gtest: Google Testing and Mocking Framework (only used when testing enabled)
- gfx third party dependencies:
    - d3d12-memory-allocator: Easy to integrate D3d12 memory allocation library from GPUOpen
    - DirectX12-Agility: DirectX 12 Agility SDK
    - directx-dxc: DirectX Shader Compiler (LLVM/Clang)
    - WinPixEventRuntime: D3D12 instrumentation using PIX events
    - imgui: Immediate Mode Graphical User interface for C++
    - Stb: Public domain header-only libraries
    - cgltf: Single-file glTF 2.0 loader and writer written in C99
    - glm: OpenGL Mathematics (GLM)
    - tinyobjloader: Tiny but powerful single file wavefront obj loader
    - tinyexr: Library to load and save OpenEXR(.exr) images
    - ktx: The Khronos KTX library and tools
    - vulkan-headers: Vulkan header files and API registry

## Code Layout

Code is separated by functionality with the expectation that files (i.e. headers/source/shaders) will be grouped together in the same folder.

All source code is written using C++ and uses the `.cpp` file extension. Header files use the `.h` extension. This extension is also used for any files shared between host and device code. All device code uses the `.hlsl` extension.

- `assets` : Contains bundled test scenes and associated data
- `docs` : Contains the documentation
- `dump` : The default location for saved screenshots
- `shader_pdb` : The default location for saved shader debugging information
- `src` : Contains the frameworks source code
    - `core` : The location of the framework code
        - `include` : Contains the single `capsaicin.h` header file used to interface with the framework
        - `src`
            - `capsaicin` : Contains the main internal framework code
            - `components` : The location of all available components (each within its own sub-folder)
            - `geometry` : Common HLSL headers for evaluating/interrogating geometric data such as meshes and intersections
            - `lights` : Common HLSL headers for lighting evaluation/sampling
            - `materials` : Common HLSL headers for material evaluation/sampling
            - `math` : Common mathematical helper functions
            - `render_techniques` : The location of all available render techniques (each within its own sub-folder)
            - `renderers` : All available renderers (each within its own sub-folder)
            - `utilities` : Reusable host side utility helpers (sort, reduce etc.)
    - `scene_viewer` : The default application
- `third_party` : Contains the submodules for any needed third party dependencies as well as any dependencies fetched via CMake where an existing installed package could not be found

See [Architecture](./architecture.md) for details on how the framework is designed and how this design corresponds to the above folder layout.
