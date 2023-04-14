![](docs/images/header.png)

# AMD Capsaicin Framework

Capsaicin is an experimental real-time rendering framework designed for graphics research and development. It is designed to aid in prototype development by providing simple and easy-to-use abstractions and frameworks to improve productivity of real-time research projects.

Features:
- Abstraction of common rendering operations
- Modular framework for easy development and collaboration
- Scene and model loading and animation
- Inbuilt user extensible UI
- Automated debugging and profiling information
- Pluggable and swappable common rendering techniques such as ToneMapping, TAA, AO etc.
- Unbiased reference path tracer

![Capsaicin](docs/images/scene_viewer.png)

## GI-1.0

We used Capsaicin to implement our GI-1.0 technique for estimating diffuse indirect illumination in real-time.

The technique uses two levels of radiance caching to allow for reduced sampling rate in order to improve performance while making the most of every ray through better sampling.

Please refer to our [publication](https://gpuopen.com/download/publications/GPUOpen2022_GI1_0.pdf) for more technical details.

#### Note on light support

GI-1.0 is primarily an indirect lighting solution and as such is expected to be combined with an existing direct lighting technique for integration into a rendering pipeline.

All common light types are supported when evaluating the indirect lighting component (e.g., point lights, spot lights, etc.) using our grid-based light sampler and (optional) reservoir-based resampling.

Furthermore the technique can estimate direct lighting through its probe system for a subset of lights; namely emissive meshes and skylights.

## Prerequisites

- Direct3D12 capable hardware and OS (Windows 10 20H2 or newer)
- [Windows 10 SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk/) or newer
- CMake 3.10 or newer (can use Visual Studio, VSCode or any other CMake supported IDE)
- DirectX Raytracing capable GPU and compatible drivers

## Building

Capsaicin uses submodules to include any required dependencies beyond those listed in **Prerequisites**.

When acquiring the code make sure to clone the repo with submodules included:

`git clone --recurse-submodules https://github.com/GPUOpen-LibrariesAndSDKs/Capsaicin`

If you have already cloned this repo without submodules, then use:

`git submodule update --init --recursive`

Capsaicin uses the [CMake](https://cmake.org/) build system. See the [Getting Started](./docs/development/getting_started.md) section for more information.

## Resources

- [Documentation](./docs/index.md)
    - [Getting Started](./docs/development/getting_started.md)
    - [Architecture](./docs/development/architecture.md)

## Citation

If Capsaicin is used any any published work, ensure to cite it using:

```bibtex
@Misc{Capsaicin23,
   author = {Guillaume Boissé, Matthew Oliver, Sylvain Meunier, Héloïse Dupont de Dinechin and Kenta Eto},
   title =  {The {AMD Capsaicin Framework}},
   year =   {2023},
   month =  {5},
   url =    {https://github.com/GPUOpen-LibrariesAndSDKs/Capsaicin},
   note =   {\url{https://github.com/GPUOpen-LibrariesAndSDKs/Capsaicin}}
}
```
