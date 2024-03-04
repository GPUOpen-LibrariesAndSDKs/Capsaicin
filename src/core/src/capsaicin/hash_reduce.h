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
#include "thread_pool.h"

namespace Capsaicin
{
template<typename TYPE>
void HashCombine(size_t &seed, TYPE const &value)
{
    seed ^= std::hash<TYPE> {}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template<typename TYPE>
size_t HashReduce(TYPE const *values, uint32_t count)
{
    uint32_t index = 0;

    constexpr uint32_t const block_size = 16;

    uint32_t block_count = (count + block_size - 1) / block_size;

    if (!block_count)
    {
        return (size_t)0x12345678u;
    }

    size_t *partial_hashes[] = {
        (size_t *)alloca(block_count * sizeof(size_t)),
        (size_t *)alloca(((block_count + block_size - 1) / block_size) * sizeof(size_t)),
    };

    Capsaicin::ThreadPool().Dispatch(
        [&](uint32_t i) {
            size_t hash = 0x12345678u;

            for (uint32_t j = i * block_size; j < (i + 1) * block_size; ++j)
            {
                if (j >= count)
                {
                    break;
                }

                HashCombine(hash, values[j]);
            }

            partial_hashes[index][i] = hash;
        },
        block_count, 1);

    while (block_count > 1)
    {
        count = block_count;

        block_count = (block_count + block_size - 1) / block_size;

        Capsaicin::ThreadPool().Dispatch(
            [&](uint32_t i) {
                size_t hash = partial_hashes[index][i * block_size];

                for (uint32_t j = i * block_size + 1; j < (i + 1) * block_size; ++j)
                {
                    if (j >= count)
                    {
                        break;
                    }

                    HashCombine(hash, partial_hashes[index][j]);
                }

                partial_hashes[1 - index][i] = hash;
            },
            block_count, 1);

        index = 1 - index;
    }

    return *partial_hashes[index];
}
} // namespace Capsaicin

namespace std
{
template<>
struct hash<GfxMesh>
{
    inline size_t operator()(GfxMesh const &value) const
    {
        size_t hash = 0x12345678u;

        Capsaicin::HashCombine(hash, value.bounds_min);
        Capsaicin::HashCombine(hash, value.bounds_max);
        Capsaicin::HashCombine(hash, value.vertices.size());
        Capsaicin::HashCombine(hash, value.indices.size());

        return hash;
    }
};

template<>
struct hash<GfxInstance>
{
    inline size_t operator()(GfxInstance const &value) const
    {
        size_t hash = 0x12345678u;

        Capsaicin::HashCombine(hash, (uint64_t)value.mesh);
        Capsaicin::HashCombine(hash, (uint64_t)value.material);
        Capsaicin::HashCombine(hash, value.transform);

        return hash;
    }
};

template<>
struct hash<GfxLight>
{
    inline size_t operator()(GfxLight const &value) const
    {
        size_t hash = 0x12345678u;

        Capsaicin::HashCombine(hash, value.color);
        Capsaicin::HashCombine(hash, value.intensity);
        Capsaicin::HashCombine(hash, value.position);
        Capsaicin::HashCombine(hash, value.direction);
        Capsaicin::HashCombine(hash, value.range);
        Capsaicin::HashCombine(hash, value.inner_cone_angle);
        Capsaicin::HashCombine(hash, value.outer_cone_angle);

        return hash;
    }
};

template<>
struct hash<glm::mat4>
{
    inline size_t operator()(glm::mat4 const &value) const
    {
        size_t hash = 0x12345678u;

        for (uint32_t i = 0; i < 16; ++i)
        {
            Capsaicin::HashCombine(hash, value[i >> 2u][i & 0x3u]);
        }

        return hash;
    }
};

template<>
struct hash<glm::vec3>
{
    inline size_t operator()(glm::vec3 const &value) const
    {
        size_t hash = 0x12345678u;

        for (uint32_t i = 0; i < 3; ++i)
        {
            Capsaicin::HashCombine(hash, value[i]);
        }

        return hash;
    }
};
} // namespace std
