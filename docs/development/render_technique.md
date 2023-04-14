#### [Index](../index.md) | Development

-----------------------

# Creating a Renderer Technique

Creating a new *Renderer Technique* just requires adding a new source file into the `src/core/renderer_techniques` folder.
Each new *Renderer Technique* should be added in its own sub-folder, so for example a new technique `my_technique` would result in new files created at the following locations:\
`src/core/renderer_techniques/my_technique/my_technique.h`\
`src/core/renderer_techniques/my_technique/my_technique.cpp`\
Note: It is not required that the source file has the same name as the sub-folder it is contained within.

All new *Renderer Techniques* must inherit from the base class `RenderTechnique` and override all required member functions.

The member functions that need overriding are:
- `bool init(CapsaicinInternal const &capsaicin)`:\
 This function is called automatically by the framework after the *Renderer Technique* and any requested *Render Options*, *Components*, *AOVs* (see below), or other requested items have been created and initialised. It is the responsibility of the *Render Technique* to perform all required initialisation operations within this function, such as creating any used CPU|GPU resources that are required to persist over the lifetime of the *Render Technique*. The return value for this function can be used to signal to the framework if resource allocation or other initialisation operations have failed and the *Render Technique* would not be able to operate as a result. Returning `false` indicates an error state while `true` signifies correct initialisation.
- `void render(CapsaicinInternal &capsaicin)`:\
 This function is called every frame and is responsible for performing all the required operations of the *Renderer Technique*. Current render settings, debug views and other internal framework state can be retrieved from the passed in `capsaicin` object. This object can be used to retrieve internal *AOV*s using `capsaicin.getAOVBuffer("Name")` as well as current *Debug Views* and *Render Options*. It is the responsibility of the *Render Technique* to perform all required per-frame operations within this function.
- `Constructor()`:\
 A default constructor that initialises the `RenderTechnique` base class with a unique name for the current technique.
- `~Destructor()`:\
 Each technique must provide a destructor that properly frees all internally created resources.

The member functions that can be optionally overridden if needed are:
- `RenderOptionList getRenderOptions()`:\
 This function is called on technique creation and is used to pass any configuration *Render Options* that the technique may provide. If none are provided this list can be empty. A configuration option is passed using a unique string to identify the option and additional data to represent the type of data the option represents (e.g. float, int etc.). The list of configuration options are held by the internal framework and are used to allow a user to change parameters at runtime. Any technique that provides *Render Options* must check for changes during frame rendering and handle them as required (see `render(...)` above).
- `ComponentList getComponents() const`:\
 This function is called on technique creation and is responsible for returning a list of all required *Components* required by the current technique. If no *Components* are required overriding this function is not necessary or the returned list can be empty. Each requested *Component* is identified by its unique string name (See [Architecture](./architecture.md) for description of *Component*). The internal framework uses this list to create *Components* for all *Render Techniques*. Each technique can then gain access to each created *Component* using `Capsaicin.getComponent("Name")`.
- `BufferList getBuffers() const`:\
 This function is called on technique creation and is responsible for returning a list of all required shared memory buffer objects. If no buffers are required overriding this function is not necessary or the returned list can be empty. Each requested buffer is identified by a unique name string as well as additional information such as the buffers requested size etc. The internal framework uses this list to create buffers for all *Renderer Techniques*. Each technique can then gain access to each created buffer using `Capsaicin.getBuffer("Name")`.
- `AOVList getAOVs() const`:\
 This function is called on technique creation and is responsible for returning a list of all required *AOV* buffers (shared render buffers e.g. GBuffers) required by the current technique. If no *AOV*s are required overriding this function is not necessary or the returned list can be empty. The `AOVList` type is used to describe each *AOV* using a unique string name as well as the types of operations that will be performed on the *AOV* (e.g. Read, Write, Accumulate). It also holds additional optional values that can be used to define the *AOVs* format, set it to be automatically cleared/backed-up each frame and other options. The internal framework uses this list to create *AOV*s for all *Renderer Techniques*. Each technique can then gain access to each created *AOV* using `Capsaicin.getAOVBuffer("Name")`. It should be noted that some *AOVs* are automatically created by the framework such as the output buffer "Color", other inbuilt buffers such as depth "Depth" and debug output buffers "Debug" are also automatically created but only if a *Renderer Technique* requests to use them.
- `DebugViewList getDebugViews() const`:\
 This function is called on technique creation and returns a list of any *Debug Views* provided by the technique. If none are provided overriding this function is not necessary or the returned list can be empty. By default the internal framework will provide default *Debug Views* for any known *AOV*s using default rendering shaders based on the format of the *AOV* (e.g. depth etc.). These *Debug Views* will have the same name as the *AOV* its displaying. If a *Render Technique* wishes to add its own additional *Debug View*(s) it can do so by returning a list of provided views using a unique string name to identify them. In cases where the internal frameworks default *Debug View* of an *AOV* is undesirable it is also possible for a technique to override it by providing its own *Debug View* and giving it the same name string as the *AOV*. For any created custom *Debug View* it is the responsibility of the *Render Technique* to check the render settings (using `RenderSettings.::debug_view_`) each frame and output the debug view to the "Debug" *AOV* when requested (see `render(...)` above).

An example blank implementation of `my_technique` would look like:
```
#include "render_technique.h"

namespace Capsaicin
{
class MyTechnique : public RenderTechnique
{
public:
    /***** Must call base class giving unique string name for this technique *****/
    MyTechnique() : RenderTechnique("My Technique") {}

    ~MyTechnique()
    {
        /***** Must clean-up any created member variables/data               *****/
    }

    RenderOptionList getRenderOptions() noexcept override
    {
        RenderOptionList newOptions;
        /***** Push any desired options to the returned list here (else just 'return {}')  *****/
        /***** Example (using provided helper RENDER_OPTION_MAKE):                         *****/
        /***** newOptions.emplace(RENDER_OPTION_MAKE(my_technique_enable, options));          *****/
        return newOptions;
    }

    struct RenderOptions
    {
        /***** Any member variable options can be added here. *****/
        /***** This represent the internal format of options where as 'RenderOptionList' has these stored as strings and variants *****/
    };

    static RenderOptions convertOptions(RenderSettings const &settings) noexcept
    {
        /***** Optional function only required if actually providing RenderOptions *****/
        RenderOptions newOptions;
        /***** Used to convert options between external string/variant and internal data type 'RenderOptions' *****/
        /***** Example: (using provided helper RENDER_OPTION_GET):                                            *****/
        /***** RENDER_OPTION_GET(my_technique_enable, newOptions, settings.options_);                         *****/
        return newOptions;
    }
	
	ComponentList RenderTechnique::getComponents() const noexcept
	{
        ComponentList components;
        /***** Push any desired Components to the returned list here (else just 'return {}')  *****/
        return components;
	}

    BufferList getBuffers() const noexcept override
    {
        BufferList buffers;
        /***** Push any desired Buffers to the returned list here (else just 'return {}')  *****/
        return buffers;
    }

    AOVList getAOVs() const noexcept override
    {
        AOVList aovs;
        /***** Push any desired AOVs to the returned list here (else just 'return {}')     *****/
        return aovs;
    }

    DebugViewList getDebugViews() const noexcept override
    {
        DebugViewList views;
        /***** Push any desired Debug Views to the returned list here (else just 'return {}') *****/
        return views;
    }
	
	bool init(CapsaicinInternal const &capsaicin) noexcept override
	{
        /***** Perform any required initialisation operations here          *****/
		return true;
	}

    void render(CapsaicinInternal &capsaicin) noexcept override
    {
        auto &renderSettings = capsaicin.getRenderSettings();
        RenderOptions newOptions = convertOptions(renderSettings);
        /***** Perform all required rendering operations here               *****/
        /***** Debug Views can be checked with 'renderSettings.debug_view_' *****/
        options = newOptions;
    }

protected:
    /***** Internal member data can be added here *****/
    /***** Example: *****/
    /***** RenderOptions options; *****/
};
} // namespace Capsaicin
```