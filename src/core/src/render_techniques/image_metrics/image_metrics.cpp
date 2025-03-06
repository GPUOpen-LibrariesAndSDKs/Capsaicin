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

#include "image_metrics.h"

#include "capsaicin_internal.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace Capsaicin
{
ImageMetrics::ImageMetrics()
    : RenderTechnique("Image Metrics")
{}

ImageMetrics::~ImageMetrics()
{
    ImageMetrics::terminate();
}

RenderOptionList ImageMetrics::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(image_metrics_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(image_metrics_save_to_file, options));
    return newOptions;
}

ImageMetrics::RenderOptions ImageMetrics::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(image_metrics_enable, newOptions, options)
    RENDER_OPTION_GET(image_metrics_save_to_file, newOptions, options)
    return newOptions;
}

SharedTextureList ImageMetrics::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::Read});
    return textures;
}

bool ImageMetrics::init(CapsaicinInternal const &capsaicin) noexcept
{
    options = convertOptions(capsaicin.getOptions());
    if (options.image_metrics_enable)
    {
        // Initialise image comparison helpers
        if (!metricMSE.initialise(capsaicin, GPUImageMetrics::Type::HDR_RGB, GPUImageMetrics::Operation::MSE))
        {
            return false;
        }
        if (!metricRMAE.initialise(
                capsaicin, GPUImageMetrics::Type::HDR_RGB, GPUImageMetrics::Operation::RMAE))
        {
            return false;
        }
        if (!metricSMAPE.initialise(
                capsaicin, GPUImageMetrics::Type::HDR_RGB, GPUImageMetrics::Operation::SMAPE))
        {
            return false;
        }
        if (!metricSSIM.initialise(
                capsaicin, GPUImageMetrics::Type::HDR_RGB, GPUImageMetrics::Operation::SSIM))
        {
            return false;
        }
        // Delay loading of image and opening file until we know for sure that the scene has finished loading
        // (this prevents recreation on environment map set)
        needsInit = true;
    }
    return true;
}

void ImageMetrics::render(CapsaicinInternal &capsaicin) noexcept
{
    RenderOptions const newOptions = convertOptions(capsaicin.getOptions());
    if (options.image_metrics_enable != newOptions.image_metrics_enable)
    {
        if (newOptions.image_metrics_enable)
        {
            options = newOptions;
            init(capsaicin);
        }
        else
        {
            terminate();
        }
    }
    if (!newOptions.image_metrics_enable)
    {
        options = newOptions;
        return;
    }
    if (needsInit || capsaicin.getEnvironmentMapUpdated() || capsaicin.getSceneUpdated()
        || capsaicin.getCameraChanged())
    {
        needsInit = false;
        if (options.image_metrics_save_to_file)
        {
            closeFile();
        }
        // Reload reference image
        if (loadReferenceImage(capsaicin) && !!referenceImage && options.image_metrics_save_to_file)
        {
            openFile(capsaicin);
        }
    }
    if (!referenceImage)
    {
        // Nothing to do as there is no image to compare
        return;
    }
    if (options.image_metrics_save_to_file != newOptions.image_metrics_save_to_file)
    {
        if (newOptions.image_metrics_save_to_file)
        {
            openFile(capsaicin);
        }
        else
        {
            closeFile();
        }
    }
    options = newOptions;

    auto const &colourBuffer = capsaicin.getSharedTexture("Color");
    metricMSE.compareAsync(colourBuffer, referenceImage);
    metricRMAE.compareAsync(colourBuffer, referenceImage);
    metricSMAPE.compareAsync(colourBuffer, referenceImage);
    metricSSIM.compareAsync(colourBuffer, referenceImage);

    if (options.image_metrics_save_to_file && capsaicin.getFrameIndex() > metricMSE.getAsyncDelay())
    {
        auto const mse = static_cast<double>(metricMSE.getMetricValue());
        // RMSE = sqrt(MSE)
        double const rmse = sqrt(mse);
        // PSNR = 20log10(MaxValue) - 10log10(MSE)
        double const psnr  = -10.0 * log10(mse);
        auto const   rmae  = static_cast<double>(metricRMAE.getMetricValue());
        auto const   smape = static_cast<double>(metricSMAPE.getMetricValue());
        auto const   ssim  = static_cast<double>(metricSSIM.getMetricValue());

        // Write values to file
        outputFile << std::setprecision(10) << mse << ',' << rmse << ',' << psnr << ',' << rmae << ','
                   << smape << ',' << ssim << '\n';
    }
}

void ImageMetrics::terminate() noexcept
{
    gfxDestroyTexture(gfx_, referenceImage);
    referenceImage = {};
    closeFile();
}

void ImageMetrics::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    if (!referenceImage)
    {
        // Nothing to do as there is no image to compare
        return;
    }
    double const mse = (capsaicin.getFrameIndex() > metricMSE.getAsyncDelay())
                         ? static_cast<double>(metricMSE.getMetricValue())
                         : 0.0;
    ImGui::Text("PQ-MSE  :  %f", mse);
    // RMSE = sqrt(MSE)
    double const rmse = sqrt(mse);
    ImGui::Text("PQ-RMSE :  %f", rmse);
    // PSNR = 20log10(MaxValue) - 10log10(MSE)
    double const psnr = -10.0 * log10(mse);
    ImGui::Text("PQ-PSNR :  %f", psnr);
    double const rmae = (capsaicin.getFrameIndex() > metricRMAE.getAsyncDelay())
                          ? static_cast<double>(metricRMAE.getMetricValue())
                          : 0.0;
    ImGui::Text("PQ-RMAE :  %f", rmae);
    double const smape = (capsaicin.getFrameIndex() > metricSMAPE.getAsyncDelay())
                           ? static_cast<double>(metricSMAPE.getMetricValue())
                           : 0.0;
    ImGui::Text("PQ-SMAPE :  %f", smape);
    double const ssim = (capsaicin.getFrameIndex() > metricSSIM.getAsyncDelay())
                          ? static_cast<double>(metricSSIM.getMetricValue())
                          : 0.0;
    ImGui::Text("PQ-SSIM :  %f", ssim);
}

bool ImageMetrics::loadReferenceImage(CapsaicinInternal const &capsaicin) noexcept
{
    // Ensure old texture is removed
    gfxDestroyTexture(gfx_, referenceImage);
    referenceImage = {};

    // Open reference image location
    std::string const referenceFileName =
        "assets/CapsaicinReferenceImages/" + getFileName(capsaicin) + ".exr";
    if (std::filesystem::exists(referenceFileName))
    {
        GfxScene const tempScene = gfxCreateScene();
        if (gfxSceneImport(tempScene, referenceFileName.c_str()) != kGfxResult_NoError)
        {
            gfxDestroyScene(tempScene);
            return false;
        }
        GfxConstRef const imageRef = gfxSceneGetObjectHandle<GfxImage>(tempScene, 0);
        referenceImage = gfxCreateTexture2D(gfx_, imageRef->width, imageRef->height, imageRef->format, 1);
        referenceImage.setName(gfxSceneGetObjectMetadata<GfxImage>(tempScene, imageRef).getObjectName());

        GfxBuffer const uploadBuffer =
            gfxCreateBuffer(gfx_, imageRef->data.size(), imageRef->data.data(), kGfxCpuAccess_Write);
        gfxCommandCopyBufferToTexture(gfx_, referenceImage, uploadBuffer);
        gfxDestroyBuffer(gfx_, uploadBuffer);
        gfxDestroyScene(tempScene);
        return !!referenceImage;
    }
    return false;
}

std::string ImageMetrics::getFileName(CapsaicinInternal const &capsaicin, bool const withFolder) noexcept
{
    auto const           &currentScenes = capsaicin.getCurrentScenes();
    std::filesystem::path currentScene  = currentScenes.empty() ? "" : currentScenes[0];
    currentScene                        = currentScene.replace_extension(""); // Remove the '.gltf' extension
    currentScene                        = currentScene.filename();
    auto currentEM                      = capsaicin.getCurrentEnvironmentMap();
    if (!currentEM.empty())
    {
        currentEM = currentEM.replace_extension(""); // Remove the '.hdr' extension
        currentEM = currentEM.filename();
    }
    else
    {
        currentEM = "None";
    }

    std::string       fileName;
    std::string const currentSceneString = currentScene.string();
    if (withFolder)
    {
        fileName = currentSceneString + '/';
    }
    auto rendererName = capsaicin.getCurrentRenderer();
    if (rendererName.find("Path Tracer") != std::string::npos)
    {
        // Treat all path tracer variants identically
        rendererName = "Path Tracer";
    }
    fileName += currentSceneString + '_' + currentEM.string() + '_';
    fileName.append(capsaicin.getSceneCurrentCamera());
    fileName += '_';
    fileName.append(rendererName);

    std::erase_if(fileName, [](unsigned char const c) { return std::isspace(c); });
    return fileName;
}

void ImageMetrics::openFile(CapsaicinInternal const &capsaicin) noexcept
{
    auto const fileName = "./dump/" + getFileName(capsaicin, false) + ".csv";
    outputFile.open(fileName, std::ios::out | std::ios::trunc);
    // Write header
    outputFile << "MSE,RMSE,PSNR,RMAE,SMAPE,SSIM\n";
}

void ImageMetrics::closeFile() noexcept
{
    if (outputFile.is_open())
    {
        outputFile.close();
    }
}
} // namespace Capsaicin
