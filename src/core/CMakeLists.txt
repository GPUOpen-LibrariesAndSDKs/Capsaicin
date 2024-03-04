file(GLOB_RECURSE HEADER_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

file(GLOB_RECURSE SHADER_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.geom
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.rt
)

file(GLOB_RECURSE SOURCE_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

set_source_files_properties(${SHADER_FILES}
    PROPERTIES
    VS_TOOL_OVERRIDE
    "None"
)

add_library(capsaicin SHARED ${SOURCE_FILES})

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
include(GenerateExportHeader)
generate_export_header(capsaicin BASE_NAME capsaicin)

configure_file(include/version.h.in version.h)

target_sources(capsaicin
    PRIVATE ${HEADER_FILES} ${INLINE_FILES} ${SHADER_FILES} ${SHADER_FILES}
    ${PROJECT_BINARY_DIR}/src/core/capsaicin_export.h
)

target_include_directories(capsaicin
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/src/core
)

target_include_directories(capsaicin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/capsaicin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_techniques
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ffx-parallelsort
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/tinyexr
)

target_compile_features(capsaicin PUBLIC cxx_std_20)
target_compile_options(capsaicin PRIVATE
    /W4 /WX /external:anglebrackets /external:W0 /analyze:external-
    -D_CRT_SECURE_NO_WARNINGS
    -D_HAS_EXCEPTIONS=0
    -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
    -D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
    -DGLM_FORCE_CTOR_INIT
    -DGLM_FORCE_XYZW_ONLY
    -DGLM_FORCE_DEPTH_ZERO_TO_ONE
    -DNOMINMAX
)

target_link_options(capsaicin PRIVATE "/SUBSYSTEM:WINDOWS")

function(assign_source_group arg1)
    foreach(_source IN ITEMS ${ARGN})
        get_filename_component(PARENT_DIR "${_source}" DIRECTORY)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "" GROUP "${PARENT_DIR}")
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/src" "" GROUP "${GROUP}")
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${GROUP}")
        string(REPLACE "/" "\\" GROUP "${GROUP}")
        source_group("${arg1}\\${GROUP}" FILES "${_source}")
    endforeach()
endfunction(assign_source_group)

assign_source_group("Header Files" ${HEADER_FILES})
assign_source_group("Source Files" ${SOURCE_FILES})
assign_source_group("Shader Files" ${SHADER_FILES})

add_library(D3D12Core SHARED IMPORTED)
set_target_properties(D3D12Core PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/agility_sdk/D3D12Core.dll
    IMPORTED_IMPLIB ${CMAKE_SOURCE_DIR}/third_party/agility_sdk/dummy.lib
)
add_library(d3d12SDKLayers SHARED IMPORTED)
set_target_properties(d3d12SDKLayers PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/third_party/agility_sdk/d3d12SDKLayers.dll
    IMPORTED_IMPLIB ${CMAKE_SOURCE_DIR}/third_party/agility_sdk/dummy.lib
)

target_link_libraries(capsaicin PUBLIC gfx glm
    PRIVATE d3d12 D3D12Core d3d12SDKLayers tinyexr)

set_target_properties(capsaicin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CAPSAICIN_RUNTIME_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CAPSAICIN_LIBRARY_OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY ${CAPSAICIN_ARCHIVE_OUTPUT_DIRECTORY}
)

add_custom_command(TARGET capsaicin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:capsaicin> $<TARGET_FILE_DIR:capsaicin>
    COMMAND_EXPAND_LISTS
)

set_target_properties(capsaicin PROPERTIES PUBLIC_HEADER "include/capsaicin.h;${CMAKE_BINARY_DIR}/src/core/version.h;${CMAKE_BINARY_DIR}/src/core/capsaicin_export.h")

# Install the library and headers
include(GNUInstallDirs)
install(TARGETS capsaicin
    EXPORT capsaicin-targets
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/capsaicin
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
)

# Install shader files into binary directory
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src
    DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
    FILES_MATCHING
    PATTERN "*.vert"
    PATTERN "*.frag"
    PATTERN "*.geom"
    PATTERN "*.comp"
    PATTERN "*.hlsl"
    PATTERN "*.rt"
)
