#### [Index](../index.md) | Development

-----------------------

# Creating a Component

Creating a new *Component* just requires adding a new source file into the `src/core/components` folder.
Each new *Component* should be added in its own sub-folder, so for example a new component `my_component` would result in new files created at the following locations:\
`src/core/components/my_component/my_component.h`\
`src/core/components/my_component/my_component.cpp`\
Note: It is not required that the source file has the same name as the sub-folder it is contained within.

All new *Components* must inherit from the abstract base class `Component`. To make the new component searchable by the rest of the system then it should also be added to the component factory by also inheriting from `ComponentFactory::Registrar<T>`. Doing so registers the new *Component* with the component factory. To ensure this registration works correctly the new *Component* must implement an empty default constructor (cannot use `=default`) as well as a static constant string containing a unique name for the *Component*.

The member functions that need overriding are:
- `Constructor()`:\
 A default constructor that initialises the `Component` base class with a unique name for the current *Component*.
- `~Destructor()`:\
 Each *Component* must provide a destructor that properly frees all internally created resources.
- `bool init(CapsaicinInternal const &capsaicin)`:\
 This function is called automatically by the framework after the *Component* and any requested *Render Options*, *Components*, *AOVs* (see below), or other requested items have been created and initialised. It is the responsibility of the *Component* to perform all required initialisation operations within this function, such as creating any used CPU|GPU resources that are required to persist over the lifetime of the *Component*. The return value for this function can be used to signal to the framework if resource allocation or other initialisation operations have failed and the *Component* would not be able to operate as a result. Returning `false` indicates an error state while `true` signifies correct initialisation.
- `void run(CapsaicinInternal &capsaicin)`:\
 This function is called automatically every frame and is responsible for performing all the required main operations of the component. Current render settings and other internal framework state can be retrieved from the passed in `capsaicin` object. This object can be used to retrieve internal *Render Options* or other settings. Unlike *Render Techniques* not all of the rendering operations must be performed within this function. *Components* can also provide additional member functions that can be explicitly called by *Render Techniques* to perform additional work or parameter passing. This function is always run by the engine before running the per-frame functions of any *Render Techniques*.
- `void terminate()`:\
 This function is automatically called when a *Component* is being destroyed or when a reset has occurred. It is the responsibility of the *Component* to perform all required destruction operations within this function, such as releasing all used CPU|GPU resources. It is not always guaranteed that this function will be called when destroying a *Component* so a components destructor should also call this function to destroy any created resources.

The member functions that can be optionally overridden if needed are:
- `RenderOptionList getRenderOptions()`:\
 This function is called on *Component* creation and is used to pass any configuration *Render Options* that the *Component* may provide. If none are provided this list can be empty. A configuration option is passed using a unique string to identify the option and additional data to represent the type of data the option represents (e.g. float, int etc.). The list of configuration options are held by the internal framework and are used to allow a user to change parameters at runtime. Any *Component* that provides *Render Options* must check for changes during frame rendering and handle them as required. This is identical to `getRenderOptions` on *Render Techniques* as all render options are stored together by capsaicin internally.
- `ComponentList getComponents() const`:\
 This function is called on *Component* creation and is responsible for returning a list of all additionally required *Components* required by the current *Component*. If no *Components* are required overriding this function is not necessary or the returned list can be empty. The internal framework uses this list to create *Components* in addition to the ones requested by *Render Techniques*. Each *Component* can gain access to other *Components* by using `Capsaicin.getComponent("Name")` or `Capsaicin.getComponent<Type>()`.
- `BufferList getBuffers() const`:\
 This function is called on *Component* creation and is responsible for returning a list of all required shared memory buffer objects. If no buffers are required overriding this function is not necessary or the returned list can be empty. Each requested buffer is identified by a unique name string as well as additional information such as the buffers requested size etc. The internal framework uses this list to create buffers for all *Components* in addition to the ones requested by *Renderer Techniques*. Each *Component* can then gain access to each created buffer using `Capsaicin.getBuffer("Name")`.
- `void renderGUI(CapsaicinInternal &capsaicin) const`:\
 This function can be used to draw *Component* specific UI elements to aid in visualisation and/or debugging. This function will be called by the parent *Capsaicin* `renderGUI` call which will execute all *Components* `renderGUI` functions followed by all *Render Techniques* `renderGUI` functions in the order that were added to the system. Any UI elements output by this function will be displayed in the 'Render Settings' UI section. Any *Component* that outputs a large number of UI parameters should wrap them in a collapsible tree node so as not to pollute the UI.

An example blank implementation of `my_component` would look like:
```
#include "component.h"

namespace Capsaicin
{
class MyComponent : public Component
	, public ComponentFactory::Registrar<MyComponent>
{
public:
	/***** Must define unique name to represent new type *****/
    static constexpr std::string_view Name = "My Component";

	/***** Must have constructor to initialise base 'Component' *****/
    MyComponent() noexcept : Component(Name) {}

    ~MyComponent()
    {
        /***** Must clean-up any created member variables/data               *****/
		terminate();
    }

    RenderOptionList getRenderOptions() noexcept override
    {
        RenderOptionList newOptions;
        /***** Push any desired options to the returned list here (else just 'return {}')  *****/
        /***** Example (using provided helper RENDER_OPTION_MAKE):                         *****/
        /*****  newOptions.emplace(RENDER_OPTION_MAKE(my_component_enable, options));      *****/
        return newOptions;
    }

    struct RenderOptions
    {
        /***** Any member variable options can be added here.         *****/
        /***** This struct can be entirely omitted if not being used. *****/
        /***** This represent the internal format of options where as 'RenderOptionList' has these stored as strings and variants *****/
		/***** Example: bool my_component_enable;                     *****/
    };

    static RenderOptions convertOptions(RenderOptionList const &options) noexcept
    {
        /***** Optional function only required if actually providing RenderOptions *****/
        RenderOptions newOptions;
        /***** Used to convert options between external string/variant and internal data type 'RenderOptions' *****/
        /***** Example: (using provided helper RENDER_OPTION_GET):                                            *****/
        /*****  RENDER_OPTION_GET(my_component_enable, newOptions, options);                                  *****/
        return newOptions;
    }
	
	ComponentList getComponents() const noexcept override
	{
        ComponentList components;
        /***** Push any desired Components to the returned list here (else just 'return {}' or dont override)  *****/
        /***** Example: if corresponding header is already included (using provided helper COMPONENT_MAKE):    *****/
        /*****  components.emplace_back(COMPONENT_MAKE(TypeOfComponent));                                      *****/
        return components;
	}

    BufferList getBuffers() const noexcept override
    {
        BufferList buffers;
        /***** Push any desired Buffers to the returned list here (else just 'return {}' or dont override)  *****/
        return buffers;
    }
	
	bool init(CapsaicinInternal const &capsaicin) noexcept override
	{
        /***** Perform any required initialisation operations here          *****/
		return true;
	}

    void run(CapsaicinInternal &capsaicin) noexcept override
    {
		/***** If any options are provided they should be checked for changes here *****/
		/***** Example:                                                            *****/
		/*****  RenderOptions newOptions = convertOptions(capsaicin.getOptions()); *****/
        /*****  Check for changes and handle accordingly                           *****/
        /*****  options = newOptions;                                              *****/
        /***** Perform any required rendering operations here                      *****/
    }
	
	void terminate() noexcept override
	{
        /***** Cleanup any created CPU or GPU resources                     *****/
	}

	void renderGUI(CapsaicinInternal &capsaicin) const noexcept override
	{
        /***** Add any UI drawing commands here                             *****/
	}
	
    /***** Additional member functions can also be provided                 *****/

protected:
    /***** Internal member data can be added here *****/
    /***** Example:                               *****/
    /*****  RenderOptions options;                *****/
};
} // namespace Capsaicin
```