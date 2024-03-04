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

#include "capsaicin_internal.h"
#include "components/component.h"
#include "components/light_sampler/light_sampler.h"

namespace Capsaicin
{
class LightSamplerUniform
    : public LightSampler
    , public ComponentFactory::Registrar<LightSamplerUniform>
    , public LightSamplerFactory::Registrar<LightSamplerUniform>
{
public:
    static constexpr std::string_view Name = "LightSamplerUniform";

    LightSamplerUniform(LightSamplerUniform const &) noexcept = delete;

    LightSamplerUniform(LightSamplerUniform &&) noexcept = default;

    /** Constructor. */
    LightSamplerUniform() noexcept;

    /** Destructor. */
    ~LightSamplerUniform() noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    ComponentList getComponents() const noexcept override;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(CapsaicinInternal const &capsaicin) noexcept override;

    /**
     * Run internal operations.
     * @param [in,out] capsaicin Current framework context.
     */
    void run(CapsaicinInternal &capsaicin) noexcept override;

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

    /**
     * Check to determine if any kernels using light sampler code need to be (re)compiled.
     * @param capsaicin Current framework context.
     * @return True if an update occurred requiring internal updates to be performed.
     */
    bool needsRecompile(CapsaicinInternal const &capsaicin) const noexcept override;

    /**
     * Get the list of shader defines that should be passed to any kernel that uses this lightSampler.
     * @note Also includes values from the default lightBuilder.
     * @param capsaicin Current framework context.
     * @return A vector with each required define.
     */
    std::vector<std::string> getShaderDefines(CapsaicinInternal const &capsaicin) const noexcept override;

    /**
     * Add the required program parameters to a shader based on current settings.
     * @note Also includes values from the default lightBuilder.
     * @param capsaicin Current framework context.
     * @param program   The shader program to bind parameters to.
     */
    void addProgramParameters(CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept override;

    /**
     * Check if the scenes lighting data was changed this frame.
     * @param capsaicin Current framework context.
     * @returns True if light data has changed.
     */
    bool getLightsUpdated(CapsaicinInternal const &capsaicin) const noexcept override;

    /**
     * Get the name of the header file used in HLSL code to include necessary sampler functions.
     * @return String name of the HLSL header include.
     */
    virtual std::string_view getHeaderFile() const noexcept override;

private:
};
} // namespace Capsaicin
