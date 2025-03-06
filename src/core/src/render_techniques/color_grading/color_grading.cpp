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
#include "color_grading.h"

#include "capsaicin_internal.h"

#include <fstream>

namespace
{

class Tokenizer
{
public:
    explicit Tokenizer(std::string const &fileName)
    {
        memset(token_.data(), 0, token_.size());
        std::filesystem::path const filePath = fileName;
        data_.resize(std::filesystem::file_size(filePath));
        std::ifstream readFile(filePath);
        if (readFile.read(data_.data(), static_cast<std::streamsize>(data_.size())); readFile)
        {
            advanceToNextToken();
        }
        else
        {
            GFX_ASSERTMSG(0, "Failed to read color grading file");
            data_.clear();
        }
    }

    [[nodiscard]] char const *getToken() const { return token_.data(); }

    bool advanceToNextToken()
    {
        char c         = 0;
        bool has_token = false;
        memset(token_.data(), 0, token_.size());
        auto token_cursor = data_cursor_;

        while (readNextCharacter(c))
        {
            switch (c)
            {
            case ' ':
            case '\r':
            case '\n':
                if (!has_token)
                {
                    token_cursor = data_cursor_;
                }
                else
                {
                    size_t const token_size = std::min(data_cursor_ - token_cursor - 1, token_.size() - 1);
                    memcpy(token_.data(), &data_[token_cursor], token_size);
                    return true;
                }
                break;
            default: has_token = true; break;
            }
        }

        return false;
    }

private:
    bool readNextCharacter(char &c)
    {
        if (data_cursor_ >= data_.size())
        {
            return false; // EOF
        }
        c = data_[data_cursor_++];
        return true;
    }

    std::vector<char>     data_;
    std::array<char, 256> token_ {};
    size_t                data_cursor_ = 0;
};

} // unnamed namespace

namespace Capsaicin
{

ColorGrading::ColorGrading()
    : RenderTechnique("Color grading")
{}

ColorGrading::~ColorGrading()
{
    terminate();
}

RenderOptionList ColorGrading::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(color_grading_enable, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(color_grading_file, options_));
    return newOptions;
}

ColorGrading::RenderOptions ColorGrading::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(color_grading_enable, newOptions, options)
    RENDER_OPTION_GET(color_grading_file, newOptions, options)
    return newOptions;
}

SharedTextureList ColorGrading::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::ReadWrite});
    textures.push_back({"ColorScaled", SharedTexture::Access::ReadWrite, SharedTexture::Flags::Optional});
    return textures;
}

bool ColorGrading::init(CapsaicinInternal const &capsaicin) noexcept
{
    if (options_.color_grading_enable)
    {
        if (!lut_buffer_ && (!options_.color_grading_file.empty() && lut_buffer_user_selected))
        {
            if (!openLUTFile(options_.color_grading_file, capsaicin))
            {
                options_.color_grading_file = "";
                lut_buffer_user_selected    = false;
            }
        }
        if (!!lut_buffer_)
        {
            color_grading_program_ = capsaicin.createProgram("render_techniques/color_grading/color_grading");
            apply_kernel_          = gfxCreateComputeKernel(gfx_, color_grading_program_, "Apply");
        }

        return !!apply_kernel_;
    }
    lut_buffer_user_selected = false;
    return true;
}

void ColorGrading::render(CapsaicinInternal &capsaicin) noexcept
{
    auto const options = convertOptions(capsaicin.getOptions());

    if (!options.color_grading_enable)
    {
        if (options_.color_grading_enable)
        {
            // Destroy resources when not being used
            terminate();
            options_.color_grading_enable = false;
            if (!lut_buffer_user_selected)
            {
                options_.color_grading_file = "";
                capsaicin.setOption("color_grading_file", options_.color_grading_file);
            }
        }
        return;
    }

    bool const newFile = options.color_grading_file != options_.color_grading_file;
    bool       reInit  = !options_.color_grading_enable && options.color_grading_enable;

    if (newFile)
    {
        if (options.color_grading_file.empty())
        {
            if (lut_buffer_user_selected)
            {
                // Reset current file to auto selected one
                lut_buffer_user_selected = false;
                if (auto const sceneLUTString = getSceneLUTFile(capsaicin);
                    std::filesystem::exists(sceneLUTString) && openLUTFile(sceneLUTString, capsaicin))
                {
                    capsaicin.setOption("color_grading_file", options_.color_grading_file);
                }
            }
            else
            {
                // Clear LUT buffer
                terminate();
                options_.color_grading_file = "";
            }
        }
        else
        {
            lut_buffer_user_selected = true;
            if (!openLUTFile(options.color_grading_file, capsaicin))
            {
                // Reset filename if failed to open
                options_.color_grading_file = "";
                capsaicin.setOption("color_grading_file", options_.color_grading_file);
                terminate();
            }
            else
            {
                reInit = !apply_kernel_; // Ensure kernels are loaded
            }
        }
    }
    else if (reInit)
    {
        options_.color_grading_enable = true; // Prevent spamming of file checks
        if (!lut_buffer_user_selected)
        {
            if (auto const sceneLUTString = getSceneLUTFile(capsaicin);
                std::filesystem::exists(sceneLUTString) && openLUTFile(sceneLUTString, capsaicin))
            {
                capsaicin.setOption("color_grading_file", options_.color_grading_file);
            }
        }
        else if (!options.color_grading_file.empty() && !openLUTFile(options.color_grading_file, capsaicin))
        {
            // Reset filename if failed to open
            options_.color_grading_file = "";
            capsaicin.setOption("color_grading_file", options_.color_grading_file);
        }
    }
    else if (capsaicin.getSceneUpdated() && !lut_buffer_user_selected)
    {
        // Check for a new scene specific LUT file
        if (auto const sceneLUTString = getSceneLUTFile(capsaicin);
            std::filesystem::exists(sceneLUTString) && openLUTFile(sceneLUTString, capsaicin))
        {
            capsaicin.setOption("color_grading_file", options_.color_grading_file);
            reInit = !apply_kernel_; // Ensure kernels are loaded
        }
        else if (!options_.color_grading_file.empty())
        {
            options_.color_grading_file = "";
            capsaicin.setOption("color_grading_file", options_.color_grading_file);
            terminate();
        }
    }

    if (!lut_buffer_)
    {
        return;
    }

    if (reInit)
    {
        options_.color_grading_enable = true;
        if (!init(capsaicin))
        {
            lut_buffer_user_selected = false;
            terminate();
            return;
        }
    }

    bool const usesScaling = capsaicin.hasSharedTexture("ColorScaled")
                          && capsaicin.hasOption<bool>("taa_enable")
                          && capsaicin.getOption<bool>("taa_enable");

    GfxTexture const &color_buffer =
        !usesScaling ? capsaicin.getSharedTexture("Color") : capsaicin.getSharedTexture("ColorScaled");
    auto const bufferDimensions =
        !usesScaling ? capsaicin.getRenderDimensions() : capsaicin.getWindowDimensions();

    gfxProgramSetParameter(gfx_, color_grading_program_, "g_ColorBuffer", color_buffer);

    gfxProgramSetParameter(gfx_, color_grading_program_, "g_LutBuffer", lut_buffer_);
    gfxProgramSetParameter(gfx_, color_grading_program_, "g_LutSampler", capsaicin.getLinearSampler());

    uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, apply_kernel_);
    uint32_t const  num_groups_x = (bufferDimensions.x + num_threads[0] - 1) / num_threads[0];
    uint32_t const  num_groups_y = (bufferDimensions.y + num_threads[1] - 1) / num_threads[1];

    gfxCommandBindKernel(gfx_, apply_kernel_);
    gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);
}

void ColorGrading::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    bool &enabled = capsaicin.getOption<bool>("color_grading_enable");

    ImGui::Checkbox("Enable Color Grading", &enabled);
}

void ColorGrading::terminate() noexcept
{
    gfxDestroyTexture(gfx_, lut_buffer_);

    lut_buffer_ = {};

    gfxDestroyProgram(gfx_, color_grading_program_);
    gfxDestroyKernel(gfx_, apply_kernel_);

    color_grading_program_ = {};
    apply_kernel_          = {};
}

bool ColorGrading::openLUTFile(std::string const &fileName, CapsaicinInternal const &capsaicin) noexcept
{
    // Check if file can actually be opened
    if (!std::filesystem::exists(fileName))
    {
        GFX_PRINT_ERROR(
            kGfxResult_InvalidOperation, "Failed to open Color Grading file `%s'", fileName.c_str());
        return false;
    }
    else
    {
        // Use tokenizer to split the input LUT file into each entry
        Tokenizer tokenizer(fileName);

        uint32_t            token_index = 0;
        uint32_t            lut_size    = 0;
        std::vector<float4> lut_data;
        bool                failed = true;

        do
        {
            switch (token_index)
            {
            case 0:
                // The first token of a .cube file should specify the LUT type, we only support 3D LUT files
                if (strcmp(tokenizer.getToken(), "LUT_3D_SIZE") != 0)
                {
                    GFX_PRINT_ERROR(kGfxResult_InvalidParameter,
                        "Invalid Color Grading file, only 3D LUT files accepted");
                }
                break;
            case 1:
                // The second element specifies the LUT size in each dimension. i.e. a size of x results in a
                // table with x*x*x entries
                lut_size = static_cast<uint32_t>(std::stoi(tokenizer.getToken()));
                if ((lut_size == 0U) || lut_size > 100)
                {
                    GFX_PRINT_ERROR(kGfxResult_InvalidParameter,
                        "Invalid Color Grading file, invalid or un-found LUT size");
                }
                else
                {
                    lut_data.resize(static_cast<size_t>(lut_size) * lut_size * lut_size);
                }
                break;
            default:
            {
                // All entries after the first 2 are the actually LUT data elements
                uint32_t const lut_index = (token_index - 2);
                // Check if number of elements exceeds expected LUT size
                if (lut_index >= 3 * lut_data.size())
                {
                    GFX_PRINT_ERROR(kGfxResult_InvalidParameter,
                        "Invalid Color Grading file, found elements exceeds specified LUT size");
                }
                else
                {
                    // Add element to LUT data
                    if ((lut_index % 3) == 0U)
                    {
                        lut_data[lut_index / 3].w = 1.0F;
                    }
                    lut_data[lut_index / 3][lut_index % 3] = std::stof(tokenizer.getToken());
                }
            }
            break;
            }
            ++token_index;
        }
        while (tokenizer.advanceToNextToken());

        // Check if number of found elements exactly matches specified LUT size
        if (token_index - 2 != 3 * lut_data.size())
        {
            GFX_PRINT_ERROR(kGfxResult_InvalidParameter,
                "Invalid Color Grading file, found elements does not match specified LUT size");
        }
        else if (!lut_data.empty())
        {
            // Upload the LUT data into a 3D texture
            gfxDestroyTexture(gfx_, lut_buffer_);
            lut_buffer_ = gfxCreateTexture3D(gfx_, lut_size, lut_size, lut_size, DXGI_FORMAT_R8G8B8A8_UNORM);
            // We lazily load the upload kernel so that it is only loaded when actually needed (assumed LUT
            // updates occur infrequently)
            GfxProgram const upload_program =
                capsaicin.createProgram("render_techniques/color_grading/color_grading_upload");
            GfxKernel const upload_kernel = gfxCreateComputeKernel(gfx_, upload_program, "Upload");
            GfxBuffer const upload_buffer =
                gfxCreateBuffer<float4>(gfx_, static_cast<uint32_t>(lut_data.size()), lut_data.data());

            gfxProgramSetParameter(gfx_, upload_program, "g_RWLutBuffer", lut_buffer_);
            gfxProgramSetParameter(gfx_, upload_program, "g_UploadBuffer", upload_buffer);
            gfxProgramSetParameter(gfx_, upload_program, "g_LutSize", lut_size);

            uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, upload_kernel);
            uint32_t const  num_groups_x =
                (static_cast<uint32_t>(lut_data.size()) + num_threads[0] - 1) / num_threads[0];

            gfxCommandBindKernel(gfx_, upload_kernel);
            gfxCommandDispatch(gfx_, num_groups_x, 1, 1);
            gfxDestroyBuffer(gfx_, upload_buffer);
            gfxDestroyKernel(gfx_, upload_kernel);
            gfxDestroyProgram(gfx_, upload_program);
            failed = false;
        }

        if (!failed)
        {
            // Only set the internal file if it was successfully loaded
            options_.color_grading_file = fileName;
            return true;
        }
    }
    return false;
}

std::string ColorGrading::getSceneLUTFile(CapsaicinInternal const &capsaicin) noexcept
{
    // Check if a LUT buffer was found with the current scene
    if (auto const &scenes = capsaicin.getCurrentScenes(); !scenes.empty())
    {
        auto sceneLUT = scenes[0];
        sceneLUT.replace_extension("cube");
        if (exists(sceneLUT))
        {
            return sceneLUT.string();
        }
    }
    return "";
}

} // namespace Capsaicin
