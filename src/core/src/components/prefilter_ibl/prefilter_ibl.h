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

#include "components/component.h"

namespace Capsaicin
{
class PrefilterIBL
    : public Component
    , public ComponentFactory::Registrar<PrefilterIBL>
{
public:
    static constexpr std::string_view Name = "PrefilterIBL";

    PrefilterIBL(PrefilterIBL const &) noexcept = delete;

    PrefilterIBL(PrefilterIBL &&) noexcept = default;

    /** Constructor. */
    PrefilterIBL() noexcept;

    ~PrefilterIBL() noexcept;

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
     * Add the required program parameters to a shader based on current settings.
     * @param capsaicin Current framework context.
     * @param program   The shader program to bind parameters to.
     */

    void addProgramParameters(CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept;

private:
    void prefilterIBL(CapsaicinInternal const &capsaicin) noexcept;

    GfxProgram prefilter_ibl_program_;
    GfxTexture prefilter_ibl_buffer_;

    uint32_t prefilter_ibl_buffer_size_ = 1024;
    uint32_t prefilter_ibl_buffer_mips_ = 5;
    uint32_t prefilter_ibl_sample_size_ = 1024;
};
} // namespace Capsaicin
