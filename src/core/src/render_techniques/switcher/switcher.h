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
#include "render_technique.h"

#include <array>
#include <iterator>

namespace Capsaicin
{
template<typename T1, typename T2>

requires std::is_base_of_v<RenderTechnique, T1> && std::is_base_of_v<RenderTechnique, T2>

class Switcher : public RenderTechnique
{
protected:
    struct RenderOptions
    {
        bool mixer_use_second_technique = false; /**< Switch between first and second technique */
    };

    T1            technique1;
    T2            technique2;
    RenderOptions options;

private:
    static constexpr auto name     = toStaticString("Mixer") + toStaticString<T1>() + toStaticString<T2>();
    static constexpr auto variable = toStaticString("mixer_") + toStaticString<T1>().lower()
                                   + toStaticString("_") + toStaticString<T2>().lower();

public:
    /** Default constructor */
    Switcher()
        : RenderTechnique(static_cast<std::string_view>(name))
    {}

    /** Defaulted destructor */
    ~Switcher() { terminate(); }

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override
    {
        auto ret = technique1.getRenderOptions();
        std::ranges::move(technique2.getRenderOptions(), std::inserter(ret, ret.end()));
        ret.emplace(static_cast<std::string_view>(variable), oldOptions.mixer_use_second_technique);
        return ret;
    }

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @returns The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept
    {
        RenderOptions newOptions;
        newOptions.mixer_use_second_technique = *std::get_if<decltype(options.mixer_use_second_technique)>(
            &settings.options_.at(static_cast<std::string_view>(variable)));
        return newOptions;
    }

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    ComponentList getComponents() const noexcept override
    {
        auto ret = technique1.getComponents();
        std::ranges::move(technique2.getComponents(), std::back_inserter(ret));
        return ret;
    }

    /**
     * Gets a list of any shared buffers provided by the current render technique.
     * @return A list of all supported buffers.
     */
    BufferList getBuffers() const noexcept override
    {
        auto ret = technique1.getBuffers();
        std::ranges::move(technique2.getBuffers(), std::back_inserter(ret));
        return ret;
    }

    /**
     * Gets the required list of AOVs needed for the current render technique.
     * @return A list of all required AOV buffers.
     */
    AOVList getAOVs() const noexcept override
    {
        auto ret = technique1.getAOVs();
        std::ranges::move(technique2.getAOVs(), std::back_inserter(ret));
        return ret;
    }

    /**
     * Gets a list of any debug views provided by the current render technique.
     * @return A list of all supported debug views.
     */
    DebugViewList getDebugViews() const noexcept override
    {
        auto ret = technique1.getDebugViews();
        std::ranges::move(technique2.getDebugViews(), std::back_inserter(ret));
        return ret;
    }

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(CapsaicinInternal const &capsaicin) noexcept override
    {
        options = convertOptions(capsaicin.getOptions());
        if (!options.mixer_use_second_technique)
        {
            return technique1.init(capsaicin);
        }
        else
        {
            return technique2.init(capsaicin);
        }
    }

    /**
     * Perform render operations.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void render(CapsaicinInternal &capsaicin) noexcept override
    {
        auto const optionsNew = convertOptions(capsaicin.getOptions());
        if (optionsNew.light_sampler_type != options.light_sampler_type)
        {
            if (!options.mixer_use_second_technique)
            {
                technique1.terminate();
                technique2.init(capsaicin);
            }
            else
            {
                technique2.terminate();
                technique1.init(capsaicin);
            }
        }
        options = optionsNew;
        if (!options.mixer_use_second_technique)
        {
            technique1.render(capsaicin);
        }
        else
        {
            technique2.render(capsaicin);
        }
    }

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override
    {
        if (!options.mixer_use_second_technique)
        {
            return technique1.terminate();
        }
        else
        {
            return technique2.terminate();
        }
    }

    /**
     * Gets number of timestamp queries.
     * @returns The timestamp query count.
     */
    uint32_t getTimestampQueryCount() const noexcept override
    {
        if (!oldOptions.mixer_use_second_technique)
        {
            return technique1.getTimestampQueryCount();
        }
        else
        {
            return technique2.getTimestampQueryCount();
        }
    }

    /**
     * Gets timestamp queries.
     * @returns The timestamp queries.
     */
    TimestampQuery const *getTimestampQueries() const noexcept override
    {
        if (!oldOptions.mixer_use_second_technique)
        {
            return technique1.getTimestampQueries();
        }
        else
        {
            return technique2.getTimestampQueries();
        }
    }

    /** Resets the timed section queries */
    void resetQueries() noexcept override
    {
        technique1.resetQueries();
        technique2.resetQueries();
    }

    /**
     * Sets internal graphics context
     * @param gfx The gfx context.
     */
    void setGfxContext(GfxContext const &gfx) noexcept override
    {
        technique1.setGfxContext(gfx);
        technique2.setGfxContext(gfx);
    }

protected:
};
} // namespace Capsaicin
