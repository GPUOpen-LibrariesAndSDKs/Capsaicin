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
#include "capsaicin_internal.h"

#pragma warning(push)
#pragma warning(disable : 4018)
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>
#pragma warning(pop)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace Capsaicin
{
void CapsaicinInternal::dumpAOVBuffer(char const *file_path, std::string_view const &aov)
{
    dump_requests_.push_back({file_path, aov});
}

void CapsaicinInternal::dumpAnyBuffer(char const *file_path, GfxTexture dump_buffer)
{
    const GfxCommandEvent command_event(gfx_, "Dump '%s'", dump_buffer.getName());
    dumpBuffer(file_path, dump_buffer);
}

void CapsaicinInternal::dumpBuffer(char const *dump_file_path, GfxTexture dumped_buffer)
{
    uint32_t dump_buffer_width =
        dumped_buffer.getWidth() ? dumped_buffer.getWidth() : gfxGetBackBufferWidth(gfx_);
    uint32_t dump_buffer_height =
        dumped_buffer.getHeight() ? dumped_buffer.getHeight() : gfxGetBackBufferHeight(gfx_);
    uint64_t dump_buffer_size = dump_buffer_width * dump_buffer_height * 4 * sizeof(float);

    GfxBuffer dump_copy_buffer = gfxCreateBuffer(gfx_, dump_buffer_size, nullptr, kGfxCpuAccess_None);
    dump_copy_buffer.setName("Capsaicin_DumpCopyBuffer");

    GfxBuffer dump_buffer = gfxCreateBuffer(gfx_, dump_buffer_size, nullptr, kGfxCpuAccess_Read);
    dump_buffer.setName("Capsaicin_DumpBuffer");

    gfxProgramSetParameter(gfx_, dump_copy_to_buffer_program_, "g_BufferDimensions",
        glm::uvec2(dump_buffer_width, dump_buffer_height));
    gfxProgramSetParameter(gfx_, dump_copy_to_buffer_program_, "g_DumpedBuffer", dumped_buffer);
    gfxProgramSetParameter(gfx_, dump_copy_to_buffer_program_, "g_CopyBuffer", dump_copy_buffer);

    uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, dump_copy_to_buffer_kernel_);
    const uint32_t  num_groups_x = (dump_buffer_width + num_threads[0] - 1) / num_threads[0];
    const uint32_t  num_groups_y = (dump_buffer_height + num_threads[1] - 1) / num_threads[1];

    gfxCommandBindKernel(gfx_, dump_copy_to_buffer_kernel_);
    gfxCommandDispatch(gfx_, num_groups_x, num_groups_y, 1);

    gfxCommandCopyBuffer(gfx_, dump_buffer, dump_copy_buffer);

    gfxDestroyBuffer(gfx_, dump_copy_buffer);

    dump_in_flight_buffers_.push_back(
        {dump_buffer, dump_buffer_width, dump_buffer_height, dump_file_path, frame_index_});
}

void CapsaicinInternal::saveImage(
    GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height, char const *file_path)
{
    char const *extension = strrchr(file_path, '.');
    if (extension != nullptr
        && (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0
            || strcmp(extension, ".JPG") == 0 || strcmp(extension, ".JPEG") == 0))
    {
        saveJPG(dump_buffer, dump_buffer_width, dump_buffer_height, file_path);
    }
    else
    {
        saveEXR(dump_buffer, dump_buffer_width, dump_buffer_height, file_path);
    }
}

void CapsaicinInternal::saveEXR(
    GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height, char const *exr_file_path)
{
    char const channel_names[] = {'A', 'B', 'G', 'R'};
    int const  channel_count   = ARRAYSIZE(channel_names);
    static_assert(channel_count > 0 && channel_count <= 4);

    // Header
    std::vector<EXRChannelInfo> channel_infos;
    std::vector<int>            pixel_types;
    std::vector<int>            requested_pixel_types;
    for (char channel_name : channel_names)
    {
        auto &channel_info   = channel_infos.emplace_back();
        channel_info.name[0] = channel_name;
        channel_info.name[1] = '\0';
        pixel_types.push_back(TINYEXR_PIXELTYPE_FLOAT);
        requested_pixel_types.push_back(TINYEXR_PIXELTYPE_FLOAT);
    }

    EXRHeader exr_header;
    InitEXRHeader(&exr_header);
    exr_header.compression_type      = TINYEXR_COMPRESSIONTYPE_NONE;
    exr_header.num_channels          = channel_count;
    exr_header.channels              = &channel_infos[0];
    exr_header.pixel_types           = &pixel_types[0];
    exr_header.requested_pixel_types = &requested_pixel_types[0];

    // Image
    float const   *dump_buffer_data  = (float *)gfxBufferGetData(gfx_, dump_buffer);
    const uint32_t image_width       = dump_buffer_width;
    const uint32_t image_height      = dump_buffer_height;
    const uint32_t image_pixel_count = dump_buffer_width * dump_buffer_height;

    std::vector<std::vector<float>> image_channels;
    std::vector<unsigned char *>    images;
    for (char channel_name : channel_names)
    {
        int dump_channel_offset;
        switch (channel_name)
        {
        case 'R': dump_channel_offset = 0; break;
        case 'G': dump_channel_offset = 1; break;
        case 'B': dump_channel_offset = 2; break;
        case 'A': dump_channel_offset = 3; break;
        default: assert(false);
        }

        auto &image_channel = image_channels.emplace_back();
        image_channel.resize(image_pixel_count * sizeof(float));
        images.push_back((unsigned char *)&image_channel[0]);
        for (size_t pixel_index = 0; pixel_index < image_pixel_count; ++pixel_index)
        {
            image_channel[pixel_index] = dump_buffer_data[4 * pixel_index + dump_channel_offset];
        }
    }

    EXRImage exr_image;
    InitEXRImage(&exr_image);
    exr_image.num_channels = channel_count;
    exr_image.images       = &images[0];
    exr_image.width        = image_width;
    exr_image.height       = image_height;

    char const *exr_err = nullptr;
    int         ret     = SaveEXRImageToFile(&exr_image, &exr_header, exr_file_path, &exr_err);

    if (ret != TINYEXR_SUCCESS)
    {
        if (exr_err != nullptr)
        {
            GFX_PRINT_ERROR(kGfxResult_InternalError, "Can't save '%s': %s", exr_file_path, exr_err);
            FreeEXRErrorMessage(exr_err);
        }
        else
        {
            GFX_PRINT_ERROR(kGfxResult_InternalError, "Can't save '%s'", exr_file_path);
        }
    }
}

void CapsaicinInternal::saveJPG(
    GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height, char const *jpg_file_path)
{
    // Image
    float const   *dump_buffer_data  = (float *)gfxBufferGetData(gfx_, dump_buffer);
    const uint32_t image_width       = dump_buffer_width;
    const uint32_t image_height      = dump_buffer_height;
    const uint32_t image_pixel_count = dump_buffer_width * dump_buffer_height;

    std::vector<unsigned char> image_data;
    image_data.resize(image_width * image_height * 4);

    for (size_t pixel_index = 0; pixel_index < image_pixel_count; ++pixel_index)
    {
        auto quantize = [dump_buffer_data, pixel_index](uint32_t channel_offset) {
            return (unsigned char)glm::floor(
                glm::clamp(dump_buffer_data[4 * pixel_index + channel_offset], 0.f, 1.f) * 255.f);
        };

        image_data[4 * pixel_index + 0] = quantize(0);
        image_data[4 * pixel_index + 1] = quantize(1);
        image_data[4 * pixel_index + 2] = quantize(2);
        image_data[4 * pixel_index + 3] = quantize(3);
    }

    int ret = stbi_write_jpg(jpg_file_path, image_width, image_height, 4, image_data.data(), 90);
    if (ret == 0)
    {
        GFX_PRINT_ERROR(kGfxResult_InternalError, "Can't save '%s'", jpg_file_path);
    }
}
} // namespace Capsaicin
