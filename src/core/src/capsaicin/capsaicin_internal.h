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

#include "gpu_shared.h"
#include "renderer.h"

#include <deque>

namespace Capsaicin
{
class RenderTechnique;

class CapsaicinInternal
{
public:
    CapsaicinInternal();
    ~CapsaicinInternal();

    GfxContext  getGfx() const;
    GfxScene    getScene() const;
    uint32_t    getWidth() const;
    uint32_t    getHeight() const;
    uint32_t    getFrameIndex() const;
    char const *getShaderPath() const;

    double getTime() const;
    void   setTime(double time);
    bool   getAnimate() const;
    void   setAnimate(bool animate);

    /**
     * Check if the scenes mesh data was changed this frame.
     * @return True if mesh data has changed.
     */
    bool getMeshesUpdated() const;

    /**
     * Check if the scenes instance transform data was changed this frame.
     * @return True if instance data has changed.
     */
    bool getTransformsUpdated() const;

    /**
     * Check if the environment map was changed this frame.
     * @return True if environment map has changed.
     */
    bool getEnvironmentMapUpdated() const;

    /**
     * Gets the list of currently available AOVs.
     * @returns The AOV list.
     */
    std::vector<std::string_view> getAOVs() noexcept;

    /**
     * Query if a AOV buffer currently exists.
     * @param aov The AOV to search for.
     * @returns True if AOV exists, false if not.
     */
    bool hasAOVBuffer(std::string_view const &aov) const noexcept;

    /**
     * Gets an AOV buffer.
     * @param aov The AOV buffer to get.
     * @returns The requested texture or null texture if AOV not found.
     */
    GfxTexture getAOVBuffer(std::string_view const &aov) const noexcept;

    /**
     * Gets the list of currently available debug views.
     * @returns The debug view list.
     */
    std::vector<std::string_view> getDebugViews() noexcept;

    /**
     * Checks whether a debug view is of an AOV.
     * @param view The debug view to check.
     * @returns True if the debug view is directly reading an AOV, False otherwise or if debug view is
     * unknown.
     */
    bool checkDebugViewAOV(std::string_view const &view) const noexcept;

    /**
     * Query if a shared buffer currently exists.
     * @param buffer The buffer to search for.
     * @returns True if buffer exists, false if not.
     */
    bool hasBuffer(std::string_view const &buffer) const noexcept;

    /**
     * Gets an shared buffer.
     * @param buffer The buffer to get.
     * @returns The requested buffer or null buffer if not found.
     */
    GfxBuffer getBuffer(std::string_view const &buffer) const noexcept;

    /**
     * Query if a shared component currently exists.
     * @param component The Component to search for.
     * @returns True if component exists, false if not.
     */
    bool hasComponent(std::string_view const &component) const noexcept;

    /**
     * Gets an shared component.
     * @param component The component to get.
     * @returns The requested component or nullptr if not found.
     */
    std::shared_ptr<Component> const &getComponent(std::string_view const &component) const noexcept;

    /**
     * Gets an shared component and casts to requested type.
     * @tparam T The type of component cast.
     * @returns The requested component or nullptr if not found.
     */
    template<typename T>
    std::shared_ptr<T> const getComponent() const noexcept
    {
        return std::dynamic_pointer_cast<T>(getComponent(static_cast<std::string_view>(toStaticString<T>())));
    }

    glm::vec4  getInvDeviceZ() const;
    glm::vec3  getPreViewTranslation() const;
    GfxTexture getEnvironmentBuffer() const;

    /**
     * Gets the current camera data.
     * @returns The camera.
     */
    GfxCamera const &getCamera() const;

    /**
     * Gets camera matrices for current camera.
     * There are 2 versions of each camera matrices depending on whether jotter should be applied or not.
     * @param jittered (Optional) True if jittered camera matrices should be returned.
     * @returns The camera matrices.
     */
    CameraMatrices const &getCameraMatrices(bool jittered = false) const;

    /**
     * Gets GPU camera matrices buffer.
     * There are 2 versions of each camera matrices depending on whether jotter should be applied or not.
     * @param jittered (Optional) True if jittered camera matrices should be returned.
     * @returns The camera matrices buffer.
     */
    GfxBuffer getCameraMatricesBuffer(bool jittered = false) const;

    /**
     * Gets render settings currently in use.
     * @returns The render settings.
     */
    RenderSettings const &getRenderSettings() const;
    RenderSettings       &getRenderSettings();

    /**
     * Gets count of enabled environment lights in current scene.
     * @returns The environment light count.
     */
    uint32_t getEnvironmentLightCount() const;

    GfxBuffer        getInstanceBuffer() const;
    Instance const  *getInstanceData() const;
    Instance        *getInstanceData();
    glm::vec3 const *getInstanceMinBounds() const;
    glm::vec3 const *getInstanceMaxBounds() const;
    GfxBuffer        getInstanceIdBuffer() const;
    uint32_t const  *getInstanceIdData() const;

    glm::mat4 const *getTransformData() const;
    GfxBuffer        getTransformBuffer() const;
    GfxBuffer        getPrevTransformBuffer() const;
    glm::mat4 const *getPrevTransformData() const;

    GfxBuffer       getMaterialBuffer() const;
    Material const *getMaterialData() const;

    GfxTexture const *getTextures() const;
    uint32_t          getTextureCount() const;
    GfxSamplerState   getLinearSampler() const;
    GfxSamplerState   getNearestSampler() const;
    GfxSamplerState   getAnisotropicSampler() const;

    GfxBuffer       getMeshBuffer() const;
    Mesh const     *getMeshData() const;
    GfxBuffer       getIndexBuffer() const;
    uint32_t const *getIndexData() const;
    GfxBuffer       getVertexBuffer() const;
    Vertex const   *getVertexData() const;

    GfxBuffer const *getIndexBuffers() const;
    uint32_t         getIndexBufferCount() const;
    GfxBuffer const *getVertexBuffers() const;
    uint32_t         getVertexBufferCount() const;

    GfxAccelerationStructure getAccelerationStructure() const;

    /**
     * Calculate and return the AABB surrounding current scene contents.
     * @returns The scene bounds (min, max).
     */
    std::pair<float3, float3> getSceneBounds() const;

    template<typename TYPE>
    GfxBuffer allocateConstantBuffer(uint32_t element_count)
    {
        GfxBuffer constant_buffer = allocateConstantBuffer(element_count * sizeof(TYPE));
        constant_buffer.setStride((uint32_t)sizeof(TYPE));
        return constant_buffer;
    }

    GfxBuffer allocateConstantBuffer(uint64_t size);

    /**
     * Initializes Capsaicin. Must be called before any other functions.
     * @param gfx The gfx context to use inside Capsaicin.
     */
    void initialize(GfxContext gfx);

    /**
     * Render the current frame.
     * @param          scene           The scene to render.
     * @param [in,out] render_settings The render settings to use during rendering.
     */
    void render(GfxScene scene, RenderSettings &render_settings);

    /** Terminates this object */
    void terminate();

    /**
     * Gets the profiling information for each timed section from the current frame.
     * @returns The total frame time as well as timestamps for each sub-section (see NodeTimestamps for
     * details).
     */
    std::pair<float, std::vector<NodeTimestamps>> getProfiling() noexcept;

    /**
     * Saves an AOV buffer to disk.
     * @param file_path Full pathname to the file to save as.
     * @param aov       The buffer to save (get available from @GetAOVs()).
     */
    void dumpAOVBuffer(char const *file_path, std::string_view const &aov);

    /**
     * Saves an texture to disk.
     * @param file_path   Full pathname to the file to save as.
     * @param dump_buffer Texture to save.
     */
    void dumpAnyBuffer(char const *file_path, GfxTexture dump_buffer);

    /**
     * Gets the list of supported renderers that can be set inside RenderSettings.
     * @returns The renderers list.
     */
    static std::vector<std::string_view> GetRenderers() noexcept;

private:
    /**
     * Sets up the render techniques for the currently set renderer.
     * This will setup any required AOVs, views or buffers required for all specified render techniques.
     * @param [in,out] render_settings The render settings used to setup.
     */
    void setupRenderTechniques(RenderSettings &render_settings) noexcept;

    void renderNextFrame(GfxScene scene);

    void dumpBuffer(char const *file_path, GfxTexture dump_buffer);
    void saveImage(GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height,
        char const *file_path);
    void saveEXR(GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height,
        char const *exr_file_path);
    void saveJPG(GfxBuffer dump_buffer, uint32_t dump_buffer_width, uint32_t dump_buffer_height,
        char const *jpg_file_path);

    size_t mesh_hash_               = 0;
    size_t transform_hash_          = 0;
    bool   was_resized_             = false;
    bool   mesh_updated_            = true;
    bool   transform_updated_       = true;
    bool   environment_map_updated_ = true;

    GfxContext  gfx_;                /**< The graphics context to be used. */
    GfxScene    scene_;              /**< The scene to be rendered. */
    double      time_        = 0.0f; /**< The elapsed time (in secs). */
    bool        animate_     = true; /**< Whether to animate the scene. */
    uint32_t    frame_index_ = 0;
    std::string shader_path_;
    uint32_t    buffer_width_  = 0;
    uint32_t    buffer_height_ = 0;

    GfxCamera      camera_;             /**< The camera to be used for drawing. */
    CameraMatrices camera_matrices_[2]; /**< Unjittered and jittered matrices */
    RenderSettings render_settings_;    /**< The settings to be used for rendering. */
    std::vector<std::unique_ptr<RenderTechnique>>
        render_techniques_; /**< The list of render techniques to be applied. */
    std::map<std::string_view /*name*/, std::shared_ptr<Component>>
                              components_;         /**< The list of render techniques to be applied. */
    std::unique_ptr<Renderer> renderer_ = nullptr; /**< Currently used renderer */
    using debug_views                   = std::vector<std::pair<std::string_view, bool>>;
    debug_views debug_views_; /**< List of available debug views */

    GfxKernel  blit_kernel_;  /**< The kernel to blit the color buffer to the back buffer. */
    GfxProgram blit_program_; /**< The program to blit the color buffer to the back buffer. */
    GfxKernel  debug_depth_kernel_;
    GfxProgram debug_depth_program_;
    GfxProgram convolve_ibl_program_;

    using aov_buffer = std::vector<std::pair<std::string_view /*name*/, GfxTexture>>;
    using aov_backup = std::vector<std::pair<GfxTexture /*Source*/, GfxTexture /*Destination*/>>;
    using aov_clear  = std::vector<GfxTexture>;
    aov_buffer aov_buffers_;        /**< The list of AOVs populated by the render techniques. */
    aov_backup aov_backup_buffers_; /**< The list of AOVS to backup each frame */
    aov_clear  aov_clear_buffers_;  /**< List of buffers to clear each frame */
    using shared_buffer = std::vector<std::pair<std::string_view, GfxBuffer>>;
    shared_buffer shared_buffers_; /**< The list of buffers populated by the render techniques. */
    GfxBuffer     constant_buffer_pools_[kGfxConstant_BackBufferCount];
    uint64_t      constant_buffer_pool_cursor_ = 0;

    GfxConstRef<GfxImage> environment_map_;
    GfxTexture            environment_buffer_;

    GfxBuffer               camera_matrices_buffer_[2]; /**< Unjittered and jittered camera matrices */
    std::vector<Instance>   instance_data_;
    GfxBuffer               instance_buffer_;
    std::vector<glm::vec3>  instance_min_bounds_;
    std::vector<glm::vec3>  instance_max_bounds_;
    std::vector<uint32_t>   instance_id_data_;
    GfxBuffer               instance_id_buffer_;
    std::vector<glm::mat4>  transform_data_;
    GfxBuffer               transform_buffer_;
    std::vector<glm::mat4>  prev_transform_data_;
    GfxBuffer               prev_transform_buffer_;
    std::vector<Material>   material_data_;
    GfxBuffer               material_buffer_;
    std::vector<GfxTexture> texture_atlas_;
    GfxSamplerState         linear_sampler_;
    GfxSamplerState         nearest_sampler_;
    GfxSamplerState         anisotropic_sampler_;
    std::vector<Mesh>       mesh_data_;
    GfxBuffer               mesh_buffer_;
    std::vector<uint32_t>   index_data_;
    GfxBuffer               index_buffer_; /**< The buffer storing all indices so it can be access via RT. */
    std::vector<Vertex>     vertex_data_;
    GfxBuffer vertex_buffer_; /**< The buffer storing all vertices so it can be access via RT. */
    GfxAccelerationStructure            acceleration_structure_;
    std::vector<GfxRaytracingPrimitive> raytracing_primitives_;

    std::deque<std::tuple<std::string /*fileName*/, std::string_view /*AOV*/>>   dump_requests_;
    std::deque<std::tuple<GfxBuffer, uint32_t, uint32_t, std::string, uint32_t>> dump_in_flight_buffers_;
    GfxKernel                                                                    dump_copy_to_buffer_kernel_;
    GfxProgram                                                                   dump_copy_to_buffer_program_;
};
} // namespace Capsaicin
