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
#include "factory.h"

namespace Capsaicin
{
class CapsaicinInternal;

class LightSampler : public Component
{
    LightSampler(LightSampler const &)            = delete;
    LightSampler &operator=(LightSampler const &) = delete;

public:
    using Component::Component;
    using Component::getBuffers;
    using Component::getComponents;
    using Component::getRenderOptions;
    using Component::init;
    using Component::renderGUI;
    using Component::run;
    using Component::terminate;

    /**
     * Check to determine if any kernels using light sampler code need to be (re)compiled.
     * @param capsaicin Current framework context.
     * @return True if an update occurred requiring internal updates to be performed.
     */
    virtual bool needsRecompile(CapsaicinInternal const &capsaicin) const noexcept = 0;

    /**
     * Get the list of shader defines that should be passed to any kernel that uses this lightSampler.
     * @param capsaicin Current framework context.
     * @return A vector with each required define.
     */
    virtual std::vector<std::string> getShaderDefines(CapsaicinInternal const &capsaicin) const noexcept = 0;

    /**
     * Add the required program parameters to a shader based on current settings.
     * @param capsaicin Current framework context.
     * @param program   The shader program to bind parameters to.
     */
    virtual void addProgramParameters(
        CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept = 0;

    /**
     * Check if the scenes lighting data was changed this frame.
     * @param capsaicin Current framework context.
     * @returns True if light data has changed.
     */
    virtual bool getLightsUpdated(CapsaicinInternal const &capsaicin) const noexcept = 0;

    /**
     * Get the name of the header file used in HLSL code to include necessary sampler functions.
     * @return String name of the HLSL header include.
     */
    virtual std::string_view getHeaderFile() const noexcept = 0;
};

class LightSamplerFactory : public Factory<LightSampler>
{};
} // namespace Capsaicin
