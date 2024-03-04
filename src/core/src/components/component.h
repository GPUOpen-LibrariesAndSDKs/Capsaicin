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
#pragma once

#include "capsaicin_internal_types.h"
#include "factory.h"
#include "timeable.h"

namespace Capsaicin
{
class CapsaicinInternal;

/** A abstract component class used to encapsulate shared operations between render techniques. */
class Component : public Timeable
{
    Component(Component const &)            = delete;
    Component &operator=(Component const &) = delete;
    Component()                             = delete;

public:
    Component(std::string_view const &name) noexcept;

    virtual ~Component() = default;

    /**
     * Gets the name of the component.
     * @returns The name string.
     */
    using Timeable::getName;

    /*
     * Gets configuration options for current component.
     * @return A list of all valid configuration options.
     */
    virtual RenderOptionList getRenderOptions() noexcept;

    /**
     * Gets a list of any shared components used by the current component.
     * @return A list of all supported components.
     */
    virtual ComponentList getComponents() const noexcept;

    /**
     * Gets a list of any shared buffers used by the current component.
     * @return A list of all supported buffers.
     */
    virtual BufferList getBuffers() const noexcept;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    virtual bool init(CapsaicinInternal const &capsaicin) noexcept = 0;

    /**
     * Run internal operations.
     * @param [in,out] capsaicin Current framework context.
     */
    virtual void run(CapsaicinInternal &capsaicin) noexcept = 0;

    /**
     * Destroy any used internal resources and shutdown.
     */
    virtual void terminate() noexcept = 0;

    /**
     * Render GUI options.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    virtual void renderGUI(CapsaicinInternal &capsaicin) const noexcept;

protected:
};

class ComponentFactory : public Factory<Component>
{};
} // namespace Capsaicin
