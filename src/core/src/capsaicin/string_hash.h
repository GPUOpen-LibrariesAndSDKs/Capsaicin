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

#include "static_string.h"

namespace Capsaicin
{
/**
 * Type used to represent a string using a integer hash.
 */
class StringHash
{
    uint64_t hash;

public:
    /** Default constructor */
    constexpr StringHash() = delete;

    /**
     *  Construct from a string_view.
     *  @param string The string to construct from.
     */
    constexpr explicit StringHash(std::string_view const &string) noexcept
        : hash(0xcbf29ce484222325)
    {
        // FNV-1a hash
        for (auto const &c : string)
        {
            hash ^= static_cast<size_t>(c);
            hash *= 0x00000100000001B3;
        }
    }

    /**
     *  Construct from a char array.
     *  @param string The char array to construct from.
     */
    constexpr explicit StringHash(char const *string) noexcept
        : hash(0xcbf29ce484222325)
    {
        auto const *c = string;
        while (*c != '\0')
        {
            hash ^= static_cast<size_t>(*c);
            hash *= 0x00000100000001B3;
            ++c;
        }
    }

    /**
     *  Construct from a static string.
     *  @tparam Size   Static string size parameter.
     *  @param  string The string to construct from.
     */
    template<size_t Size>
    constexpr explicit StringHash(StaticString<Size> const &string) noexcept
        : hash(0xcbf29ce484222325)
    {
        for (auto const &c : string)
        {
            hash ^= static_cast<size_t>(c);
            hash *= 0x00000100000001B3;
        }
    }

    constexpr ~StringHash() noexcept                  = default;
    constexpr StringHash(StringHash const &) noexcept = default;
    constexpr StringHash(StringHash &&) noexcept      = default;

    constexpr bool operator==(StringHash const &other) const noexcept { return hash == other.hash; }

    constexpr bool operator<=(StringHash const &other) const noexcept { return hash <= other.hash; }

    constexpr bool operator>=(StringHash const &other) const noexcept { return hash >= other.hash; }

    constexpr bool operator!=(StringHash const &other) const noexcept { return hash != other.hash; }

    constexpr bool operator<(StringHash const &other) const noexcept { return hash < other.hash; }

    constexpr bool operator>(StringHash const &other) const noexcept { return hash > other.hash; }

    constexpr bool operator==(std::string_view const &other) const noexcept
    {
        return hash == StringHash(other).hash;
    }

    constexpr bool operator<=(std::string_view const &other) const noexcept
    {
        return hash == StringHash(other).hash;
    }

    constexpr bool operator>=(std::string_view const &other) const noexcept
    {
        return hash == StringHash(other).hash;
    }

    constexpr bool operator!=(std::string_view const &other) const noexcept
    {
        return hash == StringHash(other).hash;
    }

    constexpr bool operator<(std::string_view const &other) const noexcept
    {
        return hash == StringHash(other).hash;
    }

    constexpr bool operator>(std::string_view const &other) const noexcept
    {
        return hash == StringHash(other).hash;
    }
};

inline constexpr StringHash operator""_sid(char const *str, std::size_t const size) noexcept
{
    return StringHash(std::string_view {str, size});
}
} // namespace Capsaicin
