#### [Index](../index.md) | Development

-----------------------

# Creating a Renderer

Creating a new *Renderer* just requires adding a new C++ source file into the `src/core/renderers` folder.
Each new *Renderer* should be added in its own sub-folder, so for example a new renderer `my_renderer` would result in a new file created at the following location:\
`src/core/renderers/my_renderer/my_renderer.cpp`\
Note: It is not required that the source file has the same name as the sub-folder it is contained within.

All new *Renderers* must inherit from the abstract base class `Renderer`. To make the new *Renderer* searchable by the rest of the system then it should also be added to the renderer factory by also inheriting from `RendererFactory::Registrar<T>`. Doing so registers the new *Renderer* with the renderer factory. To ensure this registration works correctly the new *Renderer* must implement an empty default constructor (cannot use `=default`) as well as a static constant string containing a unique name for the *Renderer*.

The new *Renderer* should then override all base class member functions as required.

The member functions that need overriding are:
- `Constructor()`:\
 A blank constructor.
- `std::vector<std::unique_ptr<RenderTechnique>> setupRenderTechniques(...)`:\
 This function is responsible for returning a list of all required *Render Techniques* in the order that they are required to operate during rendering. The return from this function transfers ownership of the *Render Techniques* to the internal framework which will then manage their lifetime after that.

The member functions that can be optionally overridden if needed are:
- `RenderOptionList getRenderOptions() noexcept`:\
 This function is called automatically by the framework after any requested *Render Techniques* have been created but before they are initialised. This way the *Renderer* can use this function to set any *Render Options* to values other than there default if required. See `getRenderOptions` in *Render Technique* for details about how render options are used. Any returned *Render Option* must match an existing option created by one of the *Renderers* *Render Techniques*.

An example blank implementation of `MyRenderer` would look like:
```
#include "renderer.h"
/***** Include and headers for used render techniques here *****/

namespace Capsaicin
{
class MyRenderer
    : public Renderer
    , public RendererFactory::Registrar<MyRenderer>
{
public:
    /***** Must define unique name to represent new type *****/
    static constexpr std::string_view Name = "My Renderer";

    /***** Must have empty constructor *****/
    MyRenderer() noexcept {}

    std::vector<std::unique_ptr<RenderTechnique>> setupRenderTechniques(
        RenderOptionList const &renderOptions) noexcept override
    {
        std::vector<std::unique_ptr<RenderTechnique>> render_techniques;
        /***** Emplace any desired render techniques to the returned list here *****/
        return render_techniques;
    }
    
    RenderOptionList getRenderOptions() noexcept override
    {
        RenderOptionList newOptions;
        /***** Configure any default options here                                 *****/
        /***** Example:                                                           *****/
        /*****  newOptions.emplace("matching name of existing option", newValue); *****/
        return newOptions;
    }

private:
};
} // namespace Capsaicin

```