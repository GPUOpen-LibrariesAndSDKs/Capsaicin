/**********************************************************************
Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.

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

#include <memory>
#include <string_view>
#include <unordered_map>

namespace Capsaicin
{
/**
 * A object factory class.
 * Used to create self-registering types that can be dynamically retrieved using the factory @make function.
 * @tparam Base Type of the base parent class.
 * @tparam Args Variadic parameters required for type constructor.
 * @example
 * //The base parent class for types to be added to the factory should inherit from Factory<Base>
 * class Shape : public Factory<Shape>
 * {
 * public:
 *   Shape(Key) {} //Required constructor
 * }
 * // The child classes then should inherit from Base::Registrar<T>
 * class Circle : public Shape::Registrar<Circle>
 * { }
 */
template<typename Base, typename... Args>
class Factory
{
    friend Base;

public:
    /**
     * Makes a new instance of a requested type.
     * @tparam Args2 Variadic parameters to pass to type constructor.
     * @param  name The name of the type of create.
     * @param  args The optional arguments to pass to the types constructor.
     * @return A pointer to the newly created type (nullptr if type does not exist).
     */
    template<typename... Args2>
    static std::unique_ptr<Base> make(std::string_view const &name, Args2 &&...args) noexcept
    {
        auto &list = GetList();
        if (auto i = list.find(name); i != list.cend()) return i->second(std::forward<Args2>(args)...);
        return nullptr;
    }

    /**
     * Gets a list of all valid names that can be used to create a type.
     * @return The list of names.
     */
    static std::vector<std::string_view> getNames() noexcept
    {
        std::vector<std::string_view> ret;
        for (auto &i : GetList())
        {
            ret.push_back(i.first);
        }
        return ret;
    }

    /**
     * Class used to encapsulate self-registration.
     * @tparam T Generic type parameter of the child type to register.
     */
    template<class T>
    class Registrar : public Base
    {
        template<typename T2, typename = void>
        static constexpr bool isDefined = false;

        template<typename T2>
        static constexpr bool isDefined<T2, std::void_t<decltype(sizeof(T2::Name))>> = true;

    public:
        friend T;

        /** The name that will be used to register the type with the factory. */
        template<typename T2 = T>
        static constexpr std::string_view registeredName =
            (isDefined<T2>) ? T2::Name : static_cast<std::string_view>(toStaticString<T>());

        /**
         * Registers the type to the factory.
         * @return True if it succeeds, false if it fails.
         */
        static bool registerType() noexcept
        {
            Factory::GetList()[registeredName<T>] = [](Args... args) noexcept -> std::unique_ptr<Base> {
                return std::make_unique<T>(std::forward<Args>(args)...);
            };
            return true;
        }

        static bool registered; /**< Internal boolean used to force @registerType to be called */

    private:
        Registrar() noexcept
            : Base(Key {})
        {
            (void)registered;
        }
    };

    /**
     * Class used to encapsulate self-registration using internal consteval name.
     * @tparam T Generic type parameter of the child type to register.
     */
    template<class T>
    class RegistrarName : public Base
    {
    public:
        friend T;

        /**
         * Registers the type to the factory.
         * @return True if it succeeds, false if it fails.
         */
        static bool registerType() noexcept
        {
            auto name                = T::Name;
            Factory::GetList()[name] = [](Args... args) noexcept -> std::unique_ptr<Base> {
                return std::make_unique<T>(std::forward<Args>(args)...);
            };
            return true;
        }

        static bool registered; /**< Internal boolean used to force @registerType to be called */

    private:
        RegistrarName() noexcept
            : Base(Key {}, T::Name)
        {
            (void)registered;
        }
    };

private:
    class Key
    {
        Key() noexcept {};
        template<class T>
        friend class Registrar;
        template<class T>
        friend class RegistrarName;
    };

    using FunctionType = std::unique_ptr<Base> (*)(Args...) noexcept;
    Factory() noexcept = default;

    /**
     * Gets the list of internal types and corresponding type names.
     * @return The list of names and type constructor functions.
     */
    static auto &GetList() noexcept
    {
        static std::unordered_map<std::string_view, FunctionType> list;
        return list;
    }
};

template<class Base, class... Args>
template<class T>
bool Factory<Base, Args...>::Registrar<T>::registered = Factory<Base, Args...>::Registrar<T>::registerType();

template<class Base, class... Args>
template<class T>
bool Factory<Base, Args...>::RegistrarName<T>::registered =
    Factory<Base, Args...>::RegistrarName<T>::registerType();
} // namespace Capsaicin
