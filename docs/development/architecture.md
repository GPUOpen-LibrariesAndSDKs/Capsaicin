#### [Index](../index.md) | Development

-----------------------

# Architecture

Capsaicin is designed to be a easy-to-use and flexible framework for fast prototyping and development of real-time rendering research. Its core principles involve creating simple abstractions over complex low-level hardware implementation details to allow developers to focus on writing algorithms instead of dealing with complex API specifics. The framework makes efforts to ensure these abstractions are performant but the priority is rapid developer iteration and debugging and thus Capsaicin is not intended to be a high performance product development tool.

A key concept within Capsaicin is the ability to support multiple different research implementations and multiple concurrent developers independently working within the code base. To enable this the framework uses a modular design that allows for different components to be independently developed and then combined/reused in different ways.

The basic building blocks of Capsaicin can be viewed as follows:
- CapsaicinInternal
    - Renderer(s)
    - RenderTechnique(s)

## CapsaicinInternal

This is the internal framework entity responsible for managing and performing all engine operations.
CapsaicinInternal contains all the internal framework data and is responsible for:
- Loading scene data into internal data formats
    - Mesh data
    - Internal light lists
    - Material data
    - Ray tracing acceleration structures
    - Camera data
- Managing and setting up the Renderer(s) and RenderTechnique(s)
    - Creating and managing shared resources (shared textures, shared buffers etc.)
- Performing per-frame operations
    - Updating internal data
    - Running animations
    - Calling render operations

## Renderer

A *Renderer* is an internal abstract type to used to represent a sequence of *Render Techniques*. Multiple different renderer implementations can exist within the framework but only 1 renderer can be active at a time within CapsaicinInternal.
*Renderers* are modular and independent of each other allowing multiple renderers to co-exist within the code base without impacting each other. *Renderers* are registered using a [factory](https://en.wikipedia.org/wiki/Factory_(object-oriented_programming)) and are then runtime selectable.
*Renderers* themselves don't contain or create any resources, they do however allow for the framework to request a sequence of *Render Techniques* that make up that particular renderer implementation. The internal framework will then use this list to create all the requested render techniques.

See [Creating a Renderer](./renderer.md) on how to create a new *Renderer*.

## Render Techniques

*Render Techniques* represent an actual piece of rendering work to be performed. *Render Techniques* usually contain the memory allocations, shader invocations and other additional operations required to perform a particular rendering operation. Each *Render Technique* is usually meant to implement a self-contained (although often shared resources are expected) rendering operation such as a ToneMapping or TAA pass.

*Render Techniques* are created and stored within `CapsaicinInternal` based on the current *Renderer* and are then called each frame to perform their respective operations. Each *Render Technique* is then responsible for creating/destroying its own internal resources and calling any required host/device code.

As it is often required for multiple *Render Techniques* to access the same piece of shared data, each technique can register a shared resource with `CapsaicinInternal`. This shared resource differs in that it will be allocated/destroyed/managed by the core framework and not the techniques. These shared resources can then be requested by the technique at runtime allowing for multiple techniques to access/modify the shared resource during a single pass.

See [Creating a Renderer Technique](./render_technique.md) on how to create a new *Renderer*.

## Components

*Components* are similar to *Render Techniques* in that they perform some piece of rendering work except that they generally don't actually produce any direct visual output. Instead they perform some sub-section of a complete rendering work flow that can be reused or even shared between different *Render Techniques*. Each *Component* is usually meant to implement some shareable operation that generates data (e.g. memory buffers) that can be used by one or more *Render Techniques*. This means a *Component* can function like a shared resource but extends the concept beyond just shared memory to also allow for executing custom shader operations on initialisation/destruction or on demand. Examples are Light Samplers (where light lookups are used by multiple *Render Techniques*) or generating LUTs (where the component allows LUT buffers to be shared between techniques but simplifies responsibility of the LUT creation to a single location - the component).

*Components* are created and stored within `CapsaicinInternal` based on the current *Render Techniques* and are then called each frame to perform their respective operations. Each *Components* is then responsible for creating/destroying its own internal resources and calling any required host/device code. A *Component* is then responsible for providing *Render Techniques* whatever interfaces are required to retrieve the shared data or perform any requested operations.

See [Creating a Component](./component.md) on how to create a new *Component*.