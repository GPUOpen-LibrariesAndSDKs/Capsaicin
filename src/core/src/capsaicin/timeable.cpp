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

#include "timeable.h"

namespace Capsaicin
{
Timeable::TimedSection::TimedSection(Timeable &parentTimeable, std::string_view const &name) noexcept
    : parent(parentTimeable)
    , queryIndex(parent.queryCount++)
{
    if (queryIndex >= (uint32_t)parent.queries.size())
    {
        parent.queries.resize(static_cast<size_t>(queryIndex) + 1);
        parent.queries[queryIndex].query = gfxCreateTimestampQuery(parent.gfx_);
    }
    parent.queries[queryIndex].name = (!name.empty() ? name : "<unnamed>");
    gfxCommandBeginEvent(parent.gfx_, parent.queries[queryIndex].name.data());
    gfxCommandBeginTimestampQuery(parent.gfx_, parent.queries[queryIndex].query);
}

Timeable::TimedSection::~TimedSection() noexcept
{
    gfxCommandEndTimestampQuery(parent.gfx_, parent.queries[queryIndex].query);
    gfxCommandEndEvent(parent.gfx_);
}

Timeable::Timeable(std::string_view const &name) noexcept
    : name_(name)
{}

Timeable::~Timeable() noexcept
{
    for (TimestampQuery const &query : queries)
    {
        gfxDestroyTimestampQuery(gfx_, query.query);
    }
}

uint32_t Timeable::getTimestampQueryCount() const noexcept
{
    return queryCount;
}

std::vector<TimestampQuery> const &Timeable::getTimestampQueries() const noexcept
{
    return queries;
}

void Timeable::resetQueries() noexcept
{
    queryCount = 0;
}

void Timeable::setGfxContext(GfxContext const &gfx) noexcept
{
    gfx_ = gfx;
}

std::string_view Timeable::getName() const noexcept
{
    return name_;
}
} // namespace Capsaicin
