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

#include <ppl.h>

namespace Capsaicin
{
template<typename TYPE>
size_t HashCombine(size_t seed, TYPE const &value)
{
    return seed ^ (std::hash<TYPE> {}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

template<typename TYPE>
size_t HashReduce(TYPE const *values, uint32_t count)
{
    size_t const result = concurrency::parallel_reduce(
        values, values + count, static_cast<size_t>(0x12345678U),
        [](TYPE const *start, TYPE const *end, size_t hash) -> size_t {
            for (auto j = start; j < end; ++j)
            {
                hash = HashCombine(hash, *j);
            }
            return hash;
        },
        [](size_t const hash1, size_t const hash2) -> size_t { return HashCombine(hash1, hash2); });
    return result;
}
} // namespace Capsaicin

namespace std
{
template<>
struct hash<GfxMesh>
{
    size_t operator()(GfxMesh const &value) const noexcept
    {
        size_t hash = 0x12345678U;

        hash = Capsaicin::HashCombine(hash, value.bounds_min);
        hash = Capsaicin::HashCombine(hash, value.bounds_max);
        hash = Capsaicin::HashCombine(hash, value.vertices.size());
        hash = Capsaicin::HashCombine(hash, value.indices.size());

        return hash;
    }
};

template<>
struct hash<GfxInstance>
{
    size_t operator()(GfxInstance const &value) const noexcept
    {
        size_t hash = 0x12345678U;

        hash = Capsaicin::HashCombine(hash, static_cast<uint64_t>(value.mesh));
        hash = Capsaicin::HashCombine(hash, static_cast<uint64_t>(value.material));
        hash = Capsaicin::HashCombine(hash, value.transform);

        return hash;
    }
};

template<>
struct hash<GfxLight>
{
    size_t operator()(GfxLight const &value) const noexcept
    {
        size_t hash = 0x12345678U;

        hash = Capsaicin::HashCombine(hash, value.color);
        hash = Capsaicin::HashCombine(hash, value.intensity);
        hash = Capsaicin::HashCombine(hash, value.position);
        hash = Capsaicin::HashCombine(hash, value.direction);
        hash = Capsaicin::HashCombine(hash, value.range);
        hash = Capsaicin::HashCombine(hash, value.inner_cone_angle);
        hash = Capsaicin::HashCombine(hash, value.outer_cone_angle);

        return hash;
    }
};

template<>
struct hash<GfxMaterial>
{
    size_t operator()(GfxMaterial const &value) const noexcept
    {
        size_t hash = 0x12345678U;

        hash = Capsaicin::HashCombine(hash, value.albedo);
        hash = Capsaicin::HashCombine(hash, static_cast<uint32_t>(value.albedo_map));
        hash = Capsaicin::HashCombine(hash, value.emissivity);
        hash = Capsaicin::HashCombine(hash, static_cast<uint32_t>(value.emissivity_map));
        hash = Capsaicin::HashCombine(hash, value.metallicity);
        hash = Capsaicin::HashCombine(hash, static_cast<uint32_t>(value.metallicity_map));
        hash = Capsaicin::HashCombine(hash, value.roughness);
        hash = Capsaicin::HashCombine(hash, static_cast<uint32_t>(value.roughness_map));
        hash = Capsaicin::HashCombine(hash, static_cast<uint32_t>(value.normal_map));

        return hash;
    }
};

template<>
struct hash<glm::mat4>
{
    size_t operator()(glm::mat4 const &value) const noexcept
    {
        size_t hash = 0x12345678U;

        for (uint32_t i = 0; i < 16; ++i)
        {
            hash = Capsaicin::HashCombine(hash, value[i >> 2U][i & 0x3U]);
        }

        return hash;
    }
};

template<>
struct hash<glm::vec4>
{
    size_t operator()(glm::vec4 const &value) const noexcept
    {
        size_t hash = 0x12345678U;

        for (uint32_t i = 0; i < 4; ++i)
        {
            hash = Capsaicin::HashCombine(hash, value[i]);
        }

        return hash;
    }
};

template<>
struct hash<glm::vec3>
{
    size_t operator()(glm::vec3 const &value) const noexcept
    {
        size_t hash = 0x12345678U;

        for (uint32_t i = 0; i < 3; ++i)
        {
            hash = Capsaicin::HashCombine(hash, value[i]);
        }

        return hash;
    }
};
} // namespace std
