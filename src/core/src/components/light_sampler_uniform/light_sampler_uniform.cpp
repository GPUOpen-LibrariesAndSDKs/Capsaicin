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

#include "light_sampler_uniform.h"

#include "../light_builder/light_builder.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
LightSamplerUniform::LightSamplerUniform() noexcept
    : LightSampler(Name)
{}

LightSamplerUniform::~LightSamplerUniform() noexcept
{
    terminate();
}

ComponentList LightSamplerUniform::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(LightBuilder));
    return components;
}

bool LightSamplerUniform::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    return true;
}

void LightSamplerUniform::run([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept {}

bool LightSamplerUniform::needsRecompile(CapsaicinInternal const &capsaicin) const noexcept
{
    auto lightBuilder = capsaicin.getComponent<LightBuilder>();
    return lightBuilder->needsRecompile(capsaicin);
}

std::vector<std::string> LightSamplerUniform::getShaderDefines(
    CapsaicinInternal const &capsaicin) const noexcept
{
    auto                     lightBuilder = capsaicin.getComponent<LightBuilder>();
    std::vector<std::string> baseDefines(std::move(lightBuilder->getShaderDefines(capsaicin)));
    return baseDefines;
}

void LightSamplerUniform::addProgramParameters(
    CapsaicinInternal const &capsaicin, GfxProgram program) const noexcept
{
    auto lightBuilder = capsaicin.getComponent<LightBuilder>();
    lightBuilder->addProgramParameters(capsaicin, program);
}

bool LightSamplerUniform::getLightsUpdated(CapsaicinInternal const &capsaicin) const noexcept
{
    auto lightBuilder = capsaicin.getComponent<LightBuilder>();
    return lightBuilder->getLightsUpdated();
}

std::string_view LightSamplerUniform::getHeaderFile() const noexcept
{
    return std::string_view("\"../../components/light_sampler_uniform/light_sampler_uniform.hlsl\"");
}

void LightSamplerUniform::terminate() noexcept {}

} // namespace Capsaicin
