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

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace Capsaicin
{
template<size_t Size>
class StaticString
{
private:
    template<size_t>
    friend class StaticString;

    using Data = std::array<char, Size + 1>; // Note: +1 to include '/0' at end of string
    Data dataArray;

public:
    /** Defaulted constructor. */
    constexpr StaticString() noexcept = default;

    /** Defaulted destructor. */
    constexpr ~StaticString() noexcept                    = default;
    constexpr StaticString(StaticString const &) noexcept = default;
    constexpr StaticString(StaticString &&) noexcept      = default;

    /**
     * Construct from member variable.
     * @param data The char array to construct from.
     */
    constexpr explicit StaticString(Data const &data) noexcept
        : dataArray(data)
    {}

    constexpr explicit StaticString(Data &&data) noexcept
        : dataArray(std::move(data))
    {}

    /**
     * Construct from char array.
     * @param other The char array to construct from.
     */
    constexpr explicit StaticString(char const (&other)[Size + 1]) noexcept
        : dataArray(std::to_array(other))
    {}

    constexpr explicit StaticString(char (&&other)[Size + 1]) noexcept
        : dataArray(std::to_array(std::move(data)))
    {}

    static constexpr size_t npos = std::numeric_limits<size_t>::max();

    /**
     * Convert to standard string.
     * @returns New string containing copy of current string contents.
     */
    explicit operator std::string() const { return std::string {dataArray.begin(), dataArray.end()}; }

    /**
     * Convert to standard string view.
     * @returns String view referencing current string contents.
     */
    explicit constexpr operator std::string_view() const noexcept
    {
        return std::string_view {dataArray.data(), Size};
    }

    /**
     * Swap the contents of 2 strings.
     * @param other The other string to swap with.
     */
    constexpr void swap(StaticString const &other) noexcept { dataArray.swap(other.data); }

    /**
     * Get pointer to internal string data.
     * @returns Memory pointer to string data.
     */
    constexpr char *data() noexcept { return dataArray.data(); }

    constexpr char const *data() const noexcept { return dataArray.data(); }

    /**
     * Get contents of string as a C-style string pointer.
     * @returns String pointer to internal data.
     */
    constexpr char const *c_str() const noexcept { return dataArray.data(); }

    /**
     * Return iterator to beginning of string.
     * @returns Iterator to beginning of string.
     */
    constexpr auto begin() noexcept { return dataArray.begin(); }

    constexpr auto begin() const noexcept { return dataArray.begin(); }

    /**
     * Return const iterator to beginning of string.
     * @returns Constant iterator to beginning of string.
     */
    constexpr auto cbegin() const noexcept { return dataArray.cbegin(); }

    /**
     * Return iterator to end of string.
     * @returns Iterator to end of string.
     */
    constexpr auto end() noexcept { return dataArray.end() - 1; }

    constexpr auto end() const noexcept { return dataArray.end() - 1; }

    /**
     * Return const iterator to end of string.
     * @returns Constant iterator to end of string.
     */
    constexpr auto cend() const noexcept { return dataArray.cend() - 1; }

    /**
     * Return reverse iterator to last element in string.
     * @returns Reverse iterator to last element in string.
     */
    constexpr auto rbegin() noexcept { return dataArray.rbegin(); }

    constexpr auto rbegin() const noexcept { return dataArray.rbegin(); }

    /**
     * Return const reverse  to last element in string.
     * @returns Constant reverse iterator to last element in string.
     */
    constexpr auto crbegin() const noexcept { return dataArray.crbegin(); }

    /**
     * Return reverse iterator to start of string.
     * @returns Reverse iterator to start of string.
     */
    constexpr auto rend() noexcept { return dataArray.rend() - 1; }

    constexpr auto rend() const noexcept { return dataArray.rend() - 1; }

    /**
     * Return const reverse iterator to start of string.
     * @returns Constant reverse iterator to start of string.
     */
    constexpr auto crend() const noexcept { return dataArray.crend() - 1; }

    /**
     * Get size of the string.
     * @returns String size in bytes.
     */
    constexpr size_t size() const noexcept { return Size; }

    /**
     * Get length of string.
     * @returns Number of characters in string.
     */
    constexpr size_t length() const noexcept { return Size; }

    /**
     * Check if string is empty.
     * @returns True if string is empty, False otherwise.
     */
    constexpr bool empty() const noexcept { return dataArray.empty(); }

    /**
     * Get maximum number of elements that can fit in string.
     * @returns String maximum size.
     */
    constexpr size_t max_size() noexcept { return Size; }

    /**
     * Get specified character from string with bounds checking.
     * @param i Zero-index position of character to get.
     * @returns Requested element (throws if position is out of bounds).
     */
    constexpr char &at(size_t i) { return dataArray.at(i); }

    constexpr char const &at(size_t i) const { return dataArray.at(i); }

    /**
     * Get specified character from string.
     * @param i Zero-index position of character to get.
     * @returns Requested element.
     */
    constexpr char &operator[](size_t i) noexcept { return dataArray[i]; }

    constexpr char const &operator[](size_t i) const noexcept { return dataArray[i]; }

    /**
     * Get element at start of string.
     * @returns First valid character in string.
     */
    constexpr char &front() noexcept { return dataArray.front(); }

    constexpr char const &front() const noexcept { return dataArray.front(); }

    /**
     * Get element at back of string.
     * @returns Last valid character in string.
     */
    constexpr char &back() noexcept { return dataArray[Size - 1]; }

    constexpr char const &back() const noexcept { return dataArray[Size - 1]; }

    /**
     * Convert string to lower case.
     * @returns New string containing copy of original but with all characters converted to lower case.
     */
    constexpr StaticString lower() const noexcept
    {
        StaticString str(*this);
        std::transform(str.begin(), str.end() - 1, str.begin(),
            [](char c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; });
        return str;
    }

    /**
     * Convert string to upper case.
     * @returns New string containing copy of original but with all characters converted to upper case.
     */
    constexpr StaticString upper() const noexcept
    {
        StaticString str(*this);
        std::transform(str.begin(), str.end() - 1, str.begin(),
            [](char c) { return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c; });
        return str;
    }

    /**
     * Search the string for a specified character.
     * @param ch The character to search for.
     * @param start (Optional) The position in the string to start searching at.
     * @returns The position of the found character (0-indexed) or 'npos' if not found.
     */
    constexpr size_t find(char ch, const size_t start = 0) const noexcept
    {
        if (start > Size - 1) return npos;
        for (size_t i = start; i < Size; ++i)
        {
            if (dataArray[i] == ch)
            {
                return i;
            }
        }
        return npos;
    }

    /**
     * Search the string for the first occurrence of another one.
     * @tparam Size2 Size of search string (must be less than current string)
     * @param subString String to search for.
     * @param start (Optional) The position in the string to start searching at.
     * @returns The position of the first found string (0-indexed) or 'npos' if not found.
     */
    template<size_t Size2>
    constexpr size_t find(StaticString<Size2> const &subString, const size_t start = 0) const noexcept
    {
        if (Size < Size2 || start > Size - Size2) return npos;
        for (size_t i = start; i < Size; ++i)
        {
            for (size_t j = 0; j < Size2; ++j)
            {
                if (dataArray[i + j] != subString.dataArray[j])
                {
                    break;
                }
            }
            return i;
        }
        return npos;
    }

    /**
     * Search the string for the first occurrence of another one.
     * @tparam Size2 Size of search string (must be less than current string)
     * @param subString String to search for.
     * @param start (Optional) The position in the string to start searching at.
     * @returns The position of the first found string (0-indexed) or 'npos' if not found.
     */
    template<size_t Size2>
    constexpr size_t find(char const (&subString)[Size2], const size_t start = 0) const noexcept
    {
        return find(StaticString<Size2 - 1>(subString), start);
    }

    /**
     * Searches backwards through an string to find the last occurrence of a character.
     * @param ch    The character to search for.
     * @param start (Optional) The position to start searching from.
     * @returns The position in the first string that the character is found at (npos if not found).
     */
    constexpr size_t rfind(char ch, size_t start = Size - 1) const noexcept
    {
        if (start > Size - 1) return npos;
        for (size_t i = start; ; --i)
        {
            if (dataArray[i] == ch)
            {
                return i;
            }
            if (i == 0)
            {
                break;
            }
        }
        return npos;
    }

    /**
     * Searches backwards through an string to find the last occurrence of a second string.
     * @tparam Size2 Size of the second string.
     * @param subString The string to search for.
     * @param start     (Optional) The position to start searching from.
     * @returns The position in the first string that the second is found at (npos if not found).
     */
    template<size_t Size2>
    constexpr size_t rfind(
        StaticString<Size2> const &subString, const size_t start = Size - Size2) const noexcept
    {
        if (Size < Size2 || start > Size - Size2) return npos;
        for (size_t i = start; ; --i)
        {
            for (size_t j = 0; j < Size2; ++j)
            {
                if (dataArray[i + j] != subString.dataArray[j])
                {
                    break;
                }
                if (j == Size2 - 1)
                {
                    return i;
                }
            }
            if (i == 0)
            {
                break;
            }
        }
        return npos;
    }

    /**
     * Searches backwards through an string to find the last occurrence of a second string.
     * @tparam Size2 Size of the second string.
     * @param subString The array of elements to search for.
     * @param start     (Optional) The position to start searching from.
     * @returns The position in the first string that the second is found at (npos if not found).
     */
    template<size_t Size2>
    constexpr size_t rfind(char const (&subString)[Size2], const size_t start = Size - Size2) const noexcept
    {
        return rfind(StaticString<Size2 - 1>(subString), start);
    }

    /**
     * Check if string contains specified character.
     * @param ch Character to search for.
     * @returns True if character found, False otherwise.
     */
    constexpr bool contains(char ch) const noexcept { return find(ch) != npos; }

    /**
     * Check if string contains another.
     * @tparam Size2 Size of the second string.
     * @param subString The string to search for.
     * @returns True if character found, False otherwise.
     */
    template<size_t Size2>
    constexpr bool contains(StaticString<Size2> const &subString) const noexcept
    {
        return find(subString) != npos;
    }

    /**
     * Check if string contains another.
     * @tparam Size2 Size of the second string.
     * @param subString The array of elements to search for.
     * @returns True if character found, False otherwise.
     */
    template<size_t Size2>
    constexpr bool contains(char const (&subString)[Size2]) const noexcept
    {
        return find(subString) != npos;
    }

    /**
     * Add 2 strings together.
     * @tparam Size2 Size of the second string.
     * @param other  First string.
     * @param other2 Second string to add to first.
     * @returns New string containing the combined input strings.
     */
    template<size_t Size2>
    friend constexpr StaticString<Size + Size2> operator+(
        StaticString const &other, StaticString<Size2> const &other2) noexcept
    {
        StaticString<Size + Size2> buffer;
        // Copy data from first array to buffer
        char       *dest   = buffer.data();
        auto        len    = Size;
        char const *source = other.data();
        while (len--)
        {
            *dest++ = *source++;
        }
        // Copy data from second array to buffer
        auto        len2    = Size2 + 1; //+1 for '\0'
        char const *source2 = other2.data();
        while (len2--)
        {
            *dest++ = *source2++;
        }
        return buffer;
    }

    /**
     * Add a character array to a string.
     * @tparam Size2 Size of the second string.
     * @param other  The first string as an character array.
     * @param other2 Second string to add to first.
     * @returns New string containing the combined input strings.
     */
    template<size_t Size2>
    friend constexpr StaticString<Size + Size2 - 1> operator+(
        char const (&other)[Size2], StaticString const &other2) noexcept
    {
        return toStaticString(other) + other2;
    }

    /**
     * Add a string to a character array.
     * @tparam Size2 Size of the second string.
     * @param other  The first string.
     * @param other2 Second string (as an character array) to add to first.
     * @returns New string containing the combined input strings.
     */
    template<size_t Size2>
    friend constexpr StaticString<Size + Size2 - 1> operator+(
        StaticString const &other, char const (&other2)[Size2]) noexcept
    {
        return other + toStaticString(other2);
    }

    /**
     * Add a string to a character.
     * @param other  The starting character.
     * @param other2 Second string to add to first.
     * @returns New string containing the combined input strings.
     */
    friend constexpr StaticString<Size + 1> operator+(char add, StaticString const &other) noexcept
    {
        StaticString<Size + 1> buffer;
        char                  *dest = buffer.data();
        // Copy data from character
        *dest++ = add;
        // Copy data from array to buffer
        auto        len    = Size;
        char const *source = other.data();
        while (len--)
        {
            *dest++ = *source++;
        }
        // Copy data from character
        *dest++ = add;
        *dest++ = '\0';
    }

    /**
     * Add a character to a string.
     * @param other  The first string.
     * @param other2 The character to add.
     * @returns New string containing the combined input strings.
     */
    friend constexpr StaticString<Size + 1> operator+(StaticString const &other, char add) noexcept
    {
        StaticString<Size + 1> buffer;
        // Copy data from first array to buffer
        char       *dest   = buffer.dataArray.data();
        auto        len    = Size;
        char const *source = other.dataArray.data();
        while (len--)
        {
            *dest++ = *source++;
        }
        // Copy data from character
        *dest++ = add;
    }
};

/**
 * Convert a constant char array to a StaticString.
 * @tparam Size Number of elements in char array.
 * @returns New static string.
 */
template<size_t Size>
consteval StaticString<Size - 1> toStaticString(char const (&other)[Size]) noexcept
{
    return StaticString<Size - 1>(other);
}

/**
 * Gets the readable name for a specified type.
 * @tparam T Generic type parameter.
 * @returns The type name string.
 */
template<typename T>
constexpr auto toStaticString() noexcept
{
    constexpr auto getFunctionName = []<typename T2>() noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return toStaticString(__PRETTY_FUNCTION__);
#elif defined(_MSC_VER)
        return toStaticString(__FUNCSIG__);
#else
#    error Unsupported compiler
#endif
    };
    // Sentinel information used to retrieve type name from output of getFunctionName
    // Uses a known type 'float' to interrogate function name string
    constexpr auto sentinelString = getFunctionName.template operator()<float>();
    constexpr auto                                           floatString = toStaticString("float");
    constexpr auto                                           startOffset = sentinelString.rfind(floatString);
    constexpr auto endOffset = sentinelString.size() - startOffset - floatString.size();

    // Split the type name out from the function name
    constexpr auto const function                                = getFunctionName.template operator()<T>();
    constexpr size_t                                         end = function.size() - endOffset;
    constexpr auto                                           it2 = function.rfind(':');
    constexpr size_t   startOffset2 = (it2 < function.size() - 2) ? it2 + 1 : startOffset;
    constexpr auto     size         = end - startOffset2;
    StaticString<size> buffer;
    char              *dest   = buffer.data();
    auto               len    = size;
    char const        *source = function.data() + startOffset2;
    while (len--)
    {
        *dest++ = *source++;
    }
    *buffer.end() = '\0';
    return buffer;
}
} // namespace Capsaicin
