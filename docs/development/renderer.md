#### [Index](../index.md) | Development

-----------------------

# Creating a Renderer

Creating a new *Renderer* just requires adding a new C++ source file into the `src/core/renderers` folder.
Each new *Renderer* should be added in its own sub-folder, so for example a new renderer `my_renderer` would result in a new file created at the following location:\
`src/core/renderers/my_renderer/my_renderer.cpp`\
Note: It is not required that the source file has the same name as the sub-folder it is contained within.

All new *Renderers* must inherit from the abstract base class `Renderer` using its inbuilt factory registration helper `Renderer::Registrar<T>`. Doing so registers the new *Renderer* with the renderer factory. To ensure this registration works correctly the new *Renderer* must implement an empty default constructor (cannot use `=default`) as well as a static constant string containing a unique name for the *Renderer*.

The new *Renderer* should then override all base class member functions as required.

The member functions that need overriding are:
- `std::vector<std::unique_ptr<RenderTechnique>> setupRenderTechniques(...)`:\
 This function is responsible for returning a list of all required *Render Techniques* in the order that they are required to operate during rendering. The return from this function transfers ownership of the *Render Techniques* to the internal framework which will then manage their lifetime after that.
- `Constructor()`:\
 A default blank constructor.

An example blank implementation of `my_renderer` would look like:
```
#include "renderer.h"
/***** Include and headers for used render techniques here *****/

namespace Capsaicin
{
class MyRenderer : public Renderer::Registrar<MyRenderer>
{
public:
	/***** Must define unique name to represent new type *****/
    static constexpr std::string_view Name = "My Renderer";

	/***** Must have empty constructor *****/
    MyRenderer() noexcept {}

    std::vector<std::unique_ptr<RenderTechnique>> setupRenderTechniques(
        RenderSettings const &render_settings) noexcept override
    {
        std::vector<std::unique_ptr<RenderTechnique>> render_techniques;
        /***** Emplace any desired render techniques to the returned list here *****/
        return render_techniques;
    }

private:
};
} // namespace Capsaicin

```