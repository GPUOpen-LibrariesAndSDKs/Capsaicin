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
#include "components/component.h"

namespace Capsaicin
{
class CapsaicinInternal;

class RenderTechnique : public Timeable
{
    RenderTechnique(RenderTechnique const &)            = delete;
    RenderTechnique &operator=(RenderTechnique const &) = delete;

public:
    RenderTechnique(std::string_view const &name) noexcept;
    virtual ~RenderTechnique() noexcept = default;

    /**
     * Gets the name of the technique.
     * @returns The name string.
     */
    using Timeable::getName;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    virtual RenderOptionList getRenderOptions() noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    virtual ComponentList getComponents() const noexcept;

    /**
     * Gets a list of any shared buffers used by the current render technique.
     * @return A list of all supported buffers.
     */
    virtual BufferList getBuffers() const noexcept;

    /**
     * Gets the required list of AOVs needed for the current render technique.
     * @return A list of all required AOV buffers.
     */
    virtual AOVList getAOVs() const noexcept;

    /**
     * Gets a list of any debug views provided by the current render technique.
     * @return A list of all supported debug views.
     */
    virtual DebugViewList getDebugViews() const noexcept;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    virtual bool init(CapsaicinInternal const &capsaicin) noexcept = 0;

    /**
     * Perform render operations.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    virtual void render(CapsaicinInternal &capsaicin) noexcept = 0;

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
} // namespace Capsaicin
