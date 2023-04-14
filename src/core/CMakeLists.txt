file(GLOB_RECURSE HEADER_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

file(GLOB_RECURSE INLINE_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl)

file(GLOB_RECURSE SHADER_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.geom
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hlsl)

file(GLOB_RECURSE SOURCE_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

set_source_files_properties(${SHADER_FILES}
    PROPERTIES
    VS_TOOL_OVERRIDE
    "None")

add_library(core SHARED ${SOURCE_FILES})

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
include(GenerateExportHeader)
generate_export_header(core BASE_NAME capsaicin)

configure_file(include/version.h.in version.h)

target_sources(core PRIVATE ${HEADER_FILES} ${INLINE_FILES} ${SHADER_FILES}
	${PROJECT_BINARY_DIR}/src/core/capsaicin_export.h)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/src/core)

target_include_directories(core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/capsaicin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_techniques
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ffx-bx
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ffx-parallelsort
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/miniz
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/samplerCPP
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/tinyexr)

target_compile_features(core PUBLIC cxx_std_20)
target_compile_options(core PRIVATE
    /W3 /WX
    -D_CRT_SECURE_NO_WARNINGS
    -D_HAS_EXCEPTIONS=0
    -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    -D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
    -DGLM_FORCE_CTOR_INIT
    -DGLM_FORCE_XYZW_ONLY
    -DGLM_FORCE_DEPTH_ZERO_TO_ONE
)

function(assign_source_group arg1)
    foreach(_source IN ITEMS ${ARGN})
        get_filename_component(PARENT_DIR "${_source}" DIRECTORY)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "" GROUP "${PARENT_DIR}")
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/src" "" GROUP "${GROUP}")
        string(REPLACE "../gi10/src" "" GROUP "${GROUP}")
        string(REPLACE "../gi10/include" "" GROUP "${GROUP}")
        string(REPLACE "../gi10/shaders" "" GROUP "${GROUP}")
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${GROUP}")
        string(REPLACE "/" "\\" GROUP "${GROUP}")
        source_group("${arg1}\\${GROUP}" FILES "${_source}")
    endforeach()
endfunction(assign_source_group)

assign_source_group("Header Files" ${HEADER_FILES})
assign_source_group("Inline Headers" ${INLINE_FILES})
assign_source_group("Source Files" ${SOURCE_FILES})
assign_source_group("Shader Files" ${SHADER_FILES})

assign_source_group("Header Files\\gi10" ${GI10_HEADER_FILES})
assign_source_group("Source Files\\gi10" ${GI10_SOURCE_FILES})
assign_source_group("Shader Files\\gi10" ${GI10_SHADER_FILES})

add_library(D3D12Core SHARED IMPORTED)
set_target_properties(D3D12Core PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/agility_sdk/D3D12Core.dll
    IMPORTED_IMPLIB d3d12.lib
)
add_library(d3d12SDKLayers SHARED IMPORTED)
set_target_properties(d3d12SDKLayers PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/agility_sdk/d3d12SDKLayers.dll
    IMPORTED_IMPLIB d3d12.lib
)

target_link_libraries(core PUBLIC gfx glm
	PRIVATE tinyobjloader miniz D3D12Core d3d12SDKLayers)

set_target_properties(core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CAPSAICIN_RUNTIME_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CAPSAICIN_LIBRARY_OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY ${CAPSAICIN_ARCHIVE_OUTPUT_DIRECTORY}
)

add_custom_command(TARGET core POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:core> $<TARGET_FILE_DIR:core>
	COMMAND_EXPAND_LISTS
)