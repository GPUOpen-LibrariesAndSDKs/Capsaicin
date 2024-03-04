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

#include "light_sampler_switcher.h"

#include "capsaicin_internal.h"
#include "light_sampler.h"

#include <memory>

namespace Capsaicin
{
LightSamplerSwitcher::LightSamplerSwitcher() noexcept
    : Component(Name)
{}

LightSamplerSwitcher::~LightSamplerSwitcher() noexcept
{
    terminate();
}

RenderOptionList LightSamplerSwitcher::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(light_sampler_type, options));
    for (auto &i : LightSamplerFactory::getNames())
    {
        for (auto &j : LightSamplerFactory::make(i)->getRenderOptions())
        {
            if (std::find(newOptions.cbegin(), newOptions.cend(), j) == newOptions.cend())
            {
                // Add the new component to requested list
                newOptions.emplace(std::move(j));
            }
        }
    }
    return newOptions;
}

LightSamplerSwitcher::RenderOptions LightSamplerSwitcher::convertOptions(
    RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(light_sampler_type, newOptions, options)
    return newOptions;
}

ComponentList LightSamplerSwitcher::getComponents() const noexcept
{
    ComponentList components;
    // Loop through all possible light samplers and get used components
    for (auto &i : LightSamplerFactory::getNames())
    {
        for (auto &j : LightSamplerFactory::make(i)->getComponents())
        {
            if (std::find(components.cbegin(), components.cend(), j) == components.cend())
            {
                // Add the new component to requested list
                components.emplace_back(std::move(j));
            }
        }
    }
    return components;
}

bool LightSamplerSwitcher::init(CapsaicinInternal const &capsaicin) noexcept
{
    options = convertOptions(capsaicin.getOptions());
    // Initialise the requested light sampler
    auto newSampler = LightSamplerFactory::make(LightSamplerFactory::getNames()[options.light_sampler_type]);
    std::swap(currentSampler, newSampler);
    currentSampler->setGfxContext(gfx_);
    currentSampler->init(capsaicin);
    return true;
}

void LightSamplerSwitcher::run(CapsaicinInternal &capsaicin) noexcept
{
    samplerChanged        = false;
    auto const optionsNew = convertOptions(capsaicin.getOptions());
    if (optionsNew.light_sampler_type != options.light_sampler_type)
    {
        samplerChanged = true;
        currentSampler->terminate();
        // Initialise the requested light sampler
        auto newSampler =
            LightSamplerFactory::make(LightSamplerFactory::getNames()[optionsNew.light_sampler_type]);
        std::swap(currentSampler, newSampler);
        currentSampler->setGfxContext(gfx_);
        currentSampler->init(capsaicin);
    }
    options = optionsNew;
    currentSampler->run(capsaicin);
}

void LightSamplerSwitcher::terminate() noexcept
{
    currentSampler->terminate();
}

void LightSamplerSwitcher::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    // Select which renderer to use
    std::string samplerString;
    auto        samplerList = LightSamplerFactory::getNames();
    for (auto &i : samplerList)
    {
        samplerString += i;
        samplerString += '\0';
    }
    ImGui::Combo("Light Sampler",
        reinterpret_cast<int32_t *>(&capsaicin.getOption<uint32_t>("light_sampler_type")),
        samplerString.c_str());
    return currentSampler->renderGUI(capsaicin);
}

bool LightSamplerSwitcher::needsRecompile(CapsaicinInternal const &capsaicin) const noexcept
{
    return samplerChanged || currentSampler->needsRecompile(capsaicin);
}

std::vector<std::string> LightSamplerSwitcher::getShaderDefines(
    CapsaicinInternal const &capsaicin) const noexcept
{
    auto        ret           = currentSampler->getShaderDefines(capsaicin);
    std::string samplerHeader = "LIGHT_SAMPLER_HEADER=";
    samplerHeader += currentSampler->getHeaderFile();
    ret.emplace_back(samplerHeader);
    return ret;
}

void LightSamplerSwitcher::addProgramParameters(
    CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    currentSampler->addProgramParameters(capsaicin, program);
}

bool LightSamplerSwitcher::getLightsUpdated(CapsaicinInternal const &capsaicin) const noexcept
{
    return samplerChanged || currentSampler->getLightsUpdated(capsaicin);
}

uint32_t LightSamplerSwitcher::getTimestampQueryCount() const noexcept
{
    return Timeable::getTimestampQueryCount() + currentSampler->getTimestampQueryCount();
}

std::vector<TimestampQuery> const &LightSamplerSwitcher::getTimestampQueries() const noexcept
{
    static std::vector<TimestampQuery> tempQueries;
    // Need to add child queries to the current list of queries
    tempQueries              = queries;
    auto const &childQueries = currentSampler->getTimestampQueries();
    tempQueries.insert(tempQueries.begin() + this->queryCount, childQueries.begin(), childQueries.end());
    return tempQueries;
}

void LightSamplerSwitcher::resetQueries() noexcept
{
    Timeable::resetQueries();
    currentSampler->resetQueries();
}

void LightSamplerSwitcher::setGfxContext(GfxContext const &gfx) noexcept
{
    gfx_ = gfx;
    if (currentSampler)
    {
        currentSampler->setGfxContext(gfx);
    }
}

} // namespace Capsaicin
