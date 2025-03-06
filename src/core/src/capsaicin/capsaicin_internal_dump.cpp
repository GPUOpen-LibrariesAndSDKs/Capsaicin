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
#include "capsaicin_internal.h"

#include <fstream>
#include <sstream>
#include <stb_image_write.h>
#include <tinyexr.h>

namespace Capsaicin
{

static uint32_t GetBitsPerPixel(const DXGI_FORMAT format) noexcept
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32G32B32A32_UINT: return 128;
    case DXGI_FORMAT_R32G32B32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32G32B32_UINT: return 96;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R16G16B16A16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16G16B16A16_UINT: [[fallthrough]];
    case DXGI_FORMAT_R32G32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32G32_UINT: return 64;
    case DXGI_FORMAT_R8G8B8A8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: [[fallthrough]];
    case DXGI_FORMAT_R8G8B8A8_UINT: [[fallthrough]];
    case DXGI_FORMAT_R16G16_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R16G16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16G16_UINT: [[fallthrough]];
    case DXGI_FORMAT_D32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32_UINT: return 32;
    case DXGI_FORMAT_R8G8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8G8_UINT: [[fallthrough]];
    case DXGI_FORMAT_R16_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_D16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16_UINT: return 16;
    case DXGI_FORMAT_R8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8_UINT: return 8;
    default: return 0;
    }
}

static uint32_t GetNumChannels(const DXGI_FORMAT format) noexcept
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32G32B32A32_UINT: [[fallthrough]];
    case DXGI_FORMAT_R16G16B16A16_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R16G16B16A16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16G16B16A16_UINT: [[fallthrough]];
    case DXGI_FORMAT_R8G8B8A8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: [[fallthrough]];
    case DXGI_FORMAT_R8G8B8A8_UINT: return 4;
    case DXGI_FORMAT_R32G32B32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32G32B32_UINT: return 3;
    case DXGI_FORMAT_R32G32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32G32_UINT: [[fallthrough]];
    case DXGI_FORMAT_R16G16_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R16G16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16G16_UINT: [[fallthrough]];
    case DXGI_FORMAT_R8G8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8G8_UINT: return 2;
    case DXGI_FORMAT_D32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32_UINT: [[fallthrough]];
    case DXGI_FORMAT_R16_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_D16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16_UINT: [[fallthrough]];
    case DXGI_FORMAT_R8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8_UINT: return 1;
    default: return 0;
    }
}

static bool IsFormatFloat(const DXGI_FORMAT format) noexcept
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R16G16B16A16_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32G32B32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32G32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R16G16_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_D32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R32_FLOAT: [[fallthrough]];
    case DXGI_FORMAT_R16_FLOAT: return true;
    case DXGI_FORMAT_R32G32B32A32_UINT: [[fallthrough]];
    case DXGI_FORMAT_R16G16B16A16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16G16B16A16_UINT: [[fallthrough]];
    case DXGI_FORMAT_R8G8B8A8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: [[fallthrough]];
    case DXGI_FORMAT_R8G8B8A8_UINT: [[fallthrough]];
    case DXGI_FORMAT_R32G32B32_UINT: [[fallthrough]];
    case DXGI_FORMAT_R32G32_UINT: [[fallthrough]];
    case DXGI_FORMAT_R16G16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16G16_UINT: [[fallthrough]];
    case DXGI_FORMAT_R8G8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8G8_UINT: [[fallthrough]];
    case DXGI_FORMAT_R32_UINT: [[fallthrough]];
    case DXGI_FORMAT_D16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R16_UINT: [[fallthrough]];
    case DXGI_FORMAT_R8_UNORM: [[fallthrough]];
    case DXGI_FORMAT_R8_UINT: [[fallthrough]];
    default: return false;
    }
}

struct Half
{
    int16_t val;
};

template<typename T, typename TFrom>
static T ConvertType(TFrom source) noexcept
{
    if constexpr (std::is_same_v<T, TFrom>)
    {
        return source;
    }
    else
    {
        float val;
        if constexpr (std::is_same_v<TFrom, float>)
        {
            val = source;
        }
        else if constexpr (std::is_same_v<TFrom, Half>)
        {
            val = glm::detail::toFloat32(source.val);
        }
        else
        {
            val = static_cast<float>(source) / static_cast<float>(std::numeric_limits<TFrom>::max());
        }
        val = glm::clamp(val, 0.F, 1.F);
        if constexpr (std::is_same_v<T, float>)
        {
            return val;
        }
        else
        {
            return static_cast<T>(glm::floor(val * std::numeric_limits<T>::max()));
        }
    }
}

void CapsaicinInternal::dumpDebugView(std::filesystem::path const &filePath, std::string_view const &texture)
{
    if (filePath.has_extension())
    {
        if (hasSharedTexture(texture))
        {
            GfxTexture dump_buffer = getSharedTexture(texture);
            if (texture != "Color" && texture != "ColorScaled")
            {
                // Check extension, with non HDR image writes we want to output the tone-mapped image
                // instead of the raw AOV data
                auto extension = filePath.extension().string();
                std::ranges::transform(extension, extension.begin(), tolower);
                if (extension == ".jpg" || extension == ".jpeg")
                {
                    dump_buffer = currentView;
                }
            }
            dumpTexture(filePath, dump_buffer);
        }
        else
        {
            // Any dump request that is not an AOV should be a debug view. Debug views write to the debug
            // target so dump that
            dumpTexture(filePath, getSharedTexture("Debug"));
        }
    }
}

void CapsaicinInternal::dumpCamera(std::filesystem::path const &filePath, bool const jittered) const
{
    dumpCamera(camera_matrices_[jittered], jittered ? camera_jitter_.x : 0.F,
        jittered ? camera_jitter_.y : 0.F, filePath);
}

void CapsaicinInternal::dumpTexture(std::filesystem::path const &filePath, GfxTexture const &texture)
{
    uint32_t const dumpBufferWidth =
        texture.getWidth() > 0 ? texture.getWidth() : gfxGetBackBufferWidth(gfx_);
    uint32_t const dumpBufferHeight =
        texture.getHeight() > 0 ? texture.getHeight() : gfxGetBackBufferHeight(gfx_);
    uint32_t       dump_buffer_size = dumpBufferWidth * dumpBufferHeight;
    uint32_t const bytesPerPixel    = GetBitsPerPixel(texture.getFormat()) / 8;
    GFX_ASSERT(bytesPerPixel != 0);
    dump_buffer_size *= bytesPerPixel;

    GfxBuffer dumpBuffer = gfxCreateBuffer(gfx_, dump_buffer_size, nullptr, kGfxCpuAccess_Read);
    dumpBuffer.setStride(bytesPerPixel);
    dumpBuffer.setName("Capsaicin_DumpBuffer");
    gfxCommandCopyTextureToBuffer(gfx_, dumpBuffer, texture);

    dump_in_flight_buffers_.emplace_back(dumpBuffer, texture.getFormat(), dumpBufferWidth, dumpBufferHeight,
        filePath, gfxGetBackBufferCount(gfx_));
}

void CapsaicinInternal::saveImage(GfxBuffer const &dumpBuffer, const DXGI_FORMAT bufferFormat,
    uint32_t const dumpBufferWidth, uint32_t const dumpBufferHeight, std::filesystem::path const &filePath)
{
    if (filePath.has_extension())
    {
        auto extension = filePath.extension().string();
        std::ranges::transform(extension, extension.begin(), tolower);
        if (extension == ".jpg" || extension == ".jpeg")
        {
            saveJPG(dumpBuffer, bufferFormat, dumpBufferWidth, dumpBufferHeight, filePath);
        }
        else if (extension == ".exr")
        {
            saveEXR(dumpBuffer, bufferFormat, dumpBufferWidth, dumpBufferHeight, filePath);
        }
        else
        {
            GFX_PRINT_ERROR(kGfxResult_InvalidParameter, "Can't save '%s': Unknown file extension",
                filePath.string().c_str());
        }
    }
}

void CapsaicinInternal::saveEXR(GfxBuffer const &dumpBuffer, DXGI_FORMAT bufferFormat,
    uint32_t dumpBufferWidth, uint32_t dumpBufferHeight, std::filesystem::path const &filePath)
{
    uint32_t const inputChannelCount = GetNumChannels(bufferFormat);
    GFX_ASSERT(inputChannelCount > 0 && inputChannelCount <= 4);
    // We use 16bit float buffers for most color buffers, as such the 4th component is unused and can be
    // ignored This may cause issues if we ever use all 4 components of a R16G16B16A16 float buffer, but it
    // works for now
    uint32_t const    channelCount = bufferFormat == DXGI_FORMAT_R16G16B16A16_FLOAT ? 3 : inputChannelCount;
    std::vector<char> channelNames(channelCount);
    switch (channelCount)
    {
    case 1: channelNames = {'R'}; break;
    case 2: channelNames = {'G', 'R'}; break;
    case 3:
        channelNames = {'B', 'G', 'R'}; // EXR readers often expect a specific order for RGB channels
        break;
    default:
        channelNames = {'A', 'B', 'G', 'R'}; // EXR readers expect Alpha first
        // EXR readers default assume pre-multiplied alpha, we do not convert RGB to pre-multiplied to keep
        // values identical to their internal format. This may cause visual differences when viewing the files
        break;
    }

    // Image
    void const    *bufferData         = gfxBufferGetData(gfx_, dumpBuffer);
    uint32_t const imageWidth         = dumpBufferWidth;
    uint32_t const imageHeight        = dumpBufferHeight;
    uint32_t const imagePixelCount    = dumpBufferWidth * dumpBufferHeight;
    uint32_t const bitsPerChannel     = GetBitsPerPixel(bufferFormat) / inputChannelCount;
    int            pixelType          = TINYEXR_PIXELTYPE_FLOAT;
    bool           requiresConversion = false;
    if (IsFormatFloat(bufferFormat))
    {
        pixelType = bitsPerChannel == 16 ? TINYEXR_PIXELTYPE_HALF : TINYEXR_PIXELTYPE_FLOAT;
    }
    else if (bitsPerChannel == 32)
    {
        pixelType = TINYEXR_PIXELTYPE_UINT;
    }
    else
    {
        // We convert to float if not directly supported
        pixelType          = TINYEXR_PIXELTYPE_FLOAT;
        requiresConversion = true;
    }

    // Header
    std::vector<EXRChannelInfo> channelInfos;
    std::vector<int>            pixelTypes;
    std::vector<int>            requestedPixelTypes;
    for (char const &channel_name : channelNames)
    {
        auto &channelInfo   = channelInfos.emplace_back();
        channelInfo.name[0] = channel_name;
        channelInfo.name[1] = '\0';
        pixelTypes.push_back(pixelType);
        requestedPixelTypes.push_back(pixelType);
    }

    EXRHeader exrHeader;
    InitEXRHeader(&exrHeader);
    exrHeader.compression_type      = TINYEXR_COMPRESSIONTYPE_PIZ;
    exrHeader.num_channels          = static_cast<int>(channelCount);
    exrHeader.channels              = channelInfos.data();
    exrHeader.pixel_types           = pixelTypes.data();
    exrHeader.requested_pixel_types = requestedPixelTypes.data();

    std::vector<unsigned char *>      images(channelNames.size());
    std::vector<std::vector<uint8_t>> image_channels(channelNames.size());
    auto fillImages = [&]<typename T, typename TFrom>(TFrom const *dumpBufferData) {
        uint32_t channel = 0;
        for (char const &channel_name : channelNames)
        {
            int channelOffset = 0;
            switch (channel_name)
            {
            case 'R': channelOffset = 0; break;
            case 'G': channelOffset = 1; break;
            case 'B': channelOffset = 2; break;
            case 'A': channelOffset = 3; break;
            default: assert(false);
            }
            auto &imageChannel = image_channels[channel];
            imageChannel.resize(imagePixelCount * sizeof(T));
            images[channel] = imageChannel.data();
            for (size_t pixel_index = 0; pixel_index < imagePixelCount; ++pixel_index)
            {
                auto val =
                    dumpBufferData[inputChannelCount * pixel_index + static_cast<size_t>(channelOffset)];
                reinterpret_cast<T *>(imageChannel.data())[pixel_index] = ConvertType<T, TFrom>(val);
            }
            ++channel;
        }
    };
    if (requiresConversion)
    {
        if (bitsPerChannel == 16)
        {
            fillImages.operator()<float, uint16_t>(static_cast<uint16_t const *>(bufferData));
        }
        else if (bitsPerChannel == 8)
        {
            fillImages.operator()<float, uint8_t>(static_cast<uint8_t const *>(bufferData));
        }
    }
    else if (pixelType == TINYEXR_PIXELTYPE_FLOAT || pixelType == TINYEXR_PIXELTYPE_UINT)
    {
        fillImages.operator()<float, float>(static_cast<float const *>(bufferData));
    }
    else if (pixelType == TINYEXR_PIXELTYPE_HALF)
    {
        fillImages.operator()<uint16_t, uint16_t>(static_cast<uint16_t const *>(bufferData));
    }

    EXRImage exrImage;
    InitEXRImage(&exrImage);
    exrImage.num_channels = static_cast<int32_t>(channelCount);
    exrImage.images       = images.data();
    exrImage.width        = static_cast<int32_t>(imageWidth);
    exrImage.height       = static_cast<int32_t>(imageHeight);

    char const *err = nullptr;
    if (int const ret = SaveEXRImageToFile(&exrImage, &exrHeader, filePath.string().c_str(), &err);
        ret != TINYEXR_SUCCESS)
    {
        if (err != nullptr)
        {
            GFX_PRINT_ERROR(kGfxResult_InternalError, "Can't save '%s': %s", filePath.string().c_str(), err);
            FreeEXRErrorMessage(err);
        }
        else
        {
            GFX_PRINT_ERROR(kGfxResult_InternalError, "Can't save '%s'", filePath.string().c_str());
        }
    }
}

void CapsaicinInternal::saveJPG(GfxBuffer const &dumpBuffer, const DXGI_FORMAT bufferFormat,
    uint32_t const dumpBufferWidth, uint32_t const dumpBufferHeight,
    std::filesystem::path const &filePath) const
{
    // Image
    void const    *bufferData      = gfxBufferGetData(gfx_, dumpBuffer);
    uint32_t const imageWidth      = dumpBufferWidth;
    uint32_t const imageHeight     = dumpBufferHeight;
    uint32_t const imagePixelCount = dumpBufferWidth * dumpBufferHeight;
    uint32_t const channelCount    = GetNumChannels(bufferFormat);
    uint32_t const bitsPerChannel  = GetBitsPerPixel(bufferFormat) / channelCount;

    if (bitsPerChannel == 8 && channelCount == 3)
    {
        // Write data directly
        auto const ret = stbi_write_jpg(filePath.string().c_str(), static_cast<int32_t>(imageWidth),
            static_cast<int32_t>(imageHeight), 3, bufferData, 90);
        if (ret == 0)
        {
            GFX_PRINT_ERROR(kGfxResult_InternalError, "Can't save '%s'", filePath.string().c_str());
        }
    }
    else
    {
        std::vector<unsigned char> imageData(static_cast<size_t>(imageWidth) * imageHeight * 3);
        auto                       quantize = [&]<typename T>(T const *dumpBufferData) {
            for (size_t pixelIndex = 0; pixelIndex < imagePixelCount; ++pixelIndex)
            {
                imageData[3 * pixelIndex + 0] =
                    ConvertType<uint8_t, T>(dumpBufferData[channelCount * pixelIndex + 0]);
                imageData[3 * pixelIndex + 1] =
                    channelCount > 1 ? ConvertType<uint8_t, T>(dumpBufferData[channelCount * pixelIndex + 1])
                                                           : 0;
                imageData[3 * pixelIndex + 2] =
                    channelCount > 2 ? ConvertType<uint8_t, T>(dumpBufferData[channelCount * pixelIndex + 2])
                                                           : 0;
            }
        };

        if (bool const isFloatFormat = IsFormatFloat(bufferFormat); bitsPerChannel == 32 && isFloatFormat)
        {
            quantize(static_cast<float const *>(bufferData));
        }
        else if (bitsPerChannel == 32)
        {
            quantize(static_cast<uint32_t const *>(bufferData));
        }
        else if (bitsPerChannel == 16 && isFloatFormat)
        {
            quantize(static_cast<Half const *>(bufferData));
        }
        else if (bitsPerChannel == 16)
        {
            quantize(static_cast<uint16_t const *>(bufferData));
        }
        else if (bitsPerChannel == 8)
        {
            quantize(static_cast<uint8_t const *>(bufferData));
        }

        int const ret = stbi_write_jpg(filePath.string().c_str(), static_cast<int32_t>(imageWidth),
            static_cast<int32_t>(imageHeight), 3, imageData.data(), 90);
        if (ret == 0)
        {
            GFX_PRINT_ERROR(kGfxResult_InternalError, "Can't save '%s'", filePath.string().c_str());
        }
    }
}

void CapsaicinInternal::dumpCamera(CameraMatrices const &cameraMatrices, float const cameraJitterX,
    float const cameraJitterY, std::filesystem::path const &filePath) const
{
    auto formatMatrix = [](glm::mat4x4 const &matrix) -> std::string {
        std::ostringstream json_matrix;
        json_matrix << matrix[0][0] << ", " << matrix[0][1] << ", " << matrix[0][2] << ", " << matrix[0][3]
                    << ", " << matrix[1][0] << ", " << matrix[1][1] << ", " << matrix[1][2] << ", "
                    << matrix[1][3] << ", " << matrix[2][0] << ", " << matrix[2][1] << ", " << matrix[2][2]
                    << ", " << matrix[2][3] << ", " << matrix[3][0] << ", " << matrix[3][1] << ", "
                    << matrix[3][2] << ", " << matrix[3][3];
        return json_matrix.str();
    };

    auto formatVector = [](glm::vec3 const &vector) -> std::string {
        std::ostringstream json_vector;
        json_vector << vector[0] << ", " << vector[1] << ", " << vector[2];
        return json_vector.str();
    };

    if (std::ofstream jsonFile(filePath); jsonFile.is_open())
    {
        auto const &[type, eye, center, up, aspect, fovY, nearZ, farZ] = getCamera();
        constexpr bool exportPrevious                                  = false;
        jsonFile << "{" << '\n'
                 << R"(    "type": "perpective",)" << '\n'
                 << "    \"eye\": [" << '\n'
                 << "        " << formatVector(eye) << '\n'
                 << "    ]," << '\n'
                 << "    \"center\": [" << '\n'
                 << "        " << formatVector(center) << '\n'
                 << "    ]," << '\n'
                 << "    \"up\": [" << '\n'
                 << "        " << formatVector(up) << '\n'
                 << "    ]," << '\n'
                 << "    \"aspect\": " << aspect << ", " << '\n'
                 << "    \"fovY\": " << fovY << ", " << '\n'
                 << "    \"nearZ\": " << nearZ << ", " << '\n'
                 << "    \"farZ\": " << farZ << ", " << '\n'
                 << "    \"jitterX\": " << cameraJitterX << ", " << '\n'
                 << "    \"jitterY\": " << cameraJitterY << ", " << '\n'
                 << "    \"view\": [" << '\n'
                 << "        " << formatMatrix(cameraMatrices.view) << '\n'
                 << "    ]," << '\n';
        if constexpr (exportPrevious)
        {
            // BE CAREFUL: previous frame matrices are tweaked for computing motion vectors
            jsonFile << "    \"view_prev\": [" << '\n'
                     << "        " << formatMatrix(cameraMatrices.view_prev) << '\n'
                     << "    ]," << '\n';
        }
        jsonFile << "    \"inv_view\": [" << '\n'
                 << "        " << formatMatrix(cameraMatrices.inv_view) << '\n'
                 << "    ]," << '\n'
                 << "    \"projection\": [" << '\n'
                 << "        " << formatMatrix(cameraMatrices.projection) << '\n'
                 << "    ]," << '\n';
        if constexpr (exportPrevious)
        {
            // BE CAREFUL: previous frame matrices are tweaked for computing motion vectors
            jsonFile << "    \"projection_prev\": [" << '\n'
                     << "        " << formatMatrix(cameraMatrices.projection_prev) << '\n'
                     << "    ]," << '\n';
        }
        jsonFile << "    \"inv_projection\": [" << '\n'
                 << "        " << formatMatrix(cameraMatrices.inv_projection) << '\n'
                 << "    ]," << '\n'
                 << "    \"view_projection\": [" << '\n'
                 << "        " << formatMatrix(cameraMatrices.view_projection) << '\n'
                 << "    ]," << '\n';
        if constexpr (exportPrevious)
        {
            // BE CAREFUL: previous frame matrices are tweaked for computing motion vectors
            jsonFile << "    \"view_projection_prev\": [" << '\n'
                     << "        " << formatMatrix(cameraMatrices.view_projection_prev) << '\n'
                     << "    ]," << '\n';
        }
        jsonFile << "    \"inv_view_projection\": [" << '\n'
                 << "        " << formatMatrix(cameraMatrices.inv_view_projection) << '\n'
                 << "    ]" << '\n'
                 << "}" << '\n';

        jsonFile.close();
    }
    else
    {
        GFX_PRINT_ERROR(kGfxResult_InternalError, "Can't write to '%s'", filePath.string().c_str());
    }
}

} // namespace Capsaicin
