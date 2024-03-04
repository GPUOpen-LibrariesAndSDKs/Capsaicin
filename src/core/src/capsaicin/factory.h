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

#include <memory>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace Capsaicin
{
/**
 * A object factory class.
 * Used to create self-registering types that can be dynamically retrieved using the factory @make function.
 * @tparam Base Type of the base parent class.
 * @example
 * // The base parent class for types to be added to the factory should create a factor handler by inheriting
 * from Factory<Base>
 * class ShapeFactory : public Factory<Shape>
 * {
 * }
 * // The child classes then should inherit from Base::Registrar<T>
 * class Circle : public Shape, public ShapeFactory::Registrar<Circle>
 * { }
 */
template<typename Base>
class Factory
{
    friend Base;

public:
    using FunctionType = std::unique_ptr<Base> (*)() noexcept;
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

    /**
     * Makes a new instance of a requested type.
     * @param name The name of the type of create.
     * @param args The optional arguments to pass to the types constructor.
     * @return A pointer to the newly created type (nullptr if type does not exist).
     */
    static std::unique_ptr<Base> make(std::string_view const &name) noexcept
    {
        auto &list = GetList();
        if (auto i = list.find(name); i != list.cend())
        {
            return i->second();
        }
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
    class Registrar
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
            Factory::GetList()[registeredName<T>] = []() noexcept -> std::unique_ptr<Base> {
                if constexpr (std::is_constructible_v<T, std::string_view>)
                {
                    return std::make_unique<T>(registeredName<T>);
                }
                else
                {
                    return std::make_unique<T>();
                }
            };
            return true;
        }

        static bool registered; /**< Internal boolean used to force @registerType to be called */

        Registrar() noexcept { (void)registered; }
    };
};

template<class Base>
template<class T>
bool Factory<Base>::Registrar<T>::registered = Factory<Base>::Registrar<T>::registerType();

#define MANUALLY_REGISTER_TO_FACTORY(FactoryName, TypeName)            \
    class TypeName##Register : public FactoryName::Registrar<TypeName> \
    {};                                                                \
    static TypeName##Register register##TypeName;
} // namespace Capsaicin
