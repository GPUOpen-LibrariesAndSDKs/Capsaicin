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

#include <dxgiformat.h>
#include <map>
#include <string_view>
#include <variant>
#include <vector>

namespace Capsaicin
{
/**
 * Type used to pass information about requested AOVs between capsaicin and render techniques.
 */
struct AOV
{
    const std::string_view name; /**< The name to identify the AOV */

    enum Access
    {
        Read,
        Write,
        ReadWrite,
    } access = Read; /**< The type of access pattern a render technique requires */

    enum Flags
    {
        None       = 0,      /**< Use default values */
        Clear      = 1 << 1, /**< True to clear buffer every frame */
        Accumulate = 1 << 2, /**< True to allow the buffer to accumulate over frames (this is used for
                                error checking to prevent the frame being cleared) */
        Optional = 1 << 3,   /**< True if AOV should only be used if another non-optional request is made */
    } flags = None;

    const DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN; /**< The internal buffer format (If using read then
                                                       format can be set to unknown to use auto setup) */

    const std::string_view backup_name =
        std::string_view(); /**< The name to identify the AOV backup (blank if no backup required) */
};

using AOVList = std::vector<AOV>;

using DebugViewList = std::vector<std::string_view>;

/**
 * Type used to pass information about requested Bufferss between capsaicin and render techniques.
 */
struct Buffer
{
    const std::string_view name; /**< The name to identify the buffer */

    enum Access
    {
        Read,
        Write,
        ReadWrite,
    } access = Read; /**< The type of access pattern a render technique requires */

    const size_t size = 0; /**< The size of the buffer */
};

using BufferList = std::vector<Buffer>;

using ComponentList = std::vector<std::string_view>;

/**
 * A macro for easy creation of render options from a struct.
 * @param  variable The member variable name.
 * @param  default  The instance of the struct containing default values.
 */
#define RENDER_OPTION_MAKE(variable, default) #variable, default.variable

/**
 * A macro for easy conversion of render options back to a struct.
 * @param  variable The member variable name.
 * @param  ret      The instance of the struct to return values in.
 * @param  options  The instance of render options to read value from.
 */
#define RENDER_OPTION_GET(variable, ret, options) \
    ret.variable = *std::get_if<decltype(ret.variable)>(&options.at(#variable));

#define COMPONENT_MAKE(type) ComponentFactory::Registrar<type>::registeredName<>

using option           = std::variant<bool, uint32_t, int32_t, float>;
using RenderOptionList = std::map<std::string_view, option>;
} // namespace Capsaicin
