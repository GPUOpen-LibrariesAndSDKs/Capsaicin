file(GLOB_RECURSE CAPSAICIN_HEADER_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/FidelityFX/host/*.h
)

file(GLOB_RECURSE CAPSAICIN_SHADER_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.geom
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mesh
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.task
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hlsl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.rt
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*_shared.h
)

file(GLOB_RECURSE CAPSAICIN_THIRD_PARTY_SHADERS
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/FidelityFX/gpu/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/AmdDxExt/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/AmdDxExt/*.hlsl
)

file(GLOB_RECURSE CAPSAICIN_SOURCE_FILES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

set_source_files_properties(${CAPSAICIN_SHADER_FILES}
    PROPERTIES
    VS_TOOL_OVERRIDE
    "None"
)

add_library(capsaicin SHARED)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
include(GenerateExportHeader)
generate_export_header(capsaicin BASE_NAME capsaicin)

configure_file(include/version.h.in version.h)

target_sources(capsaicin PUBLIC
    FILE_SET capsaicin_shaders TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES "${CAPSAICIN_SHADER_FILES}"
)
target_sources(capsaicin PUBLIC
    FILE_SET capsaicin_thirdparty_shaders TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party FILES "${CAPSAICIN_THIRD_PARTY_SHADERS}"
)
target_sources(capsaicin PRIVATE
    ${CAPSAICIN_HEADER_FILES} ${CAPSAICIN_SOURCE_FILES}
    ${PROJECT_BINARY_DIR}/src/core/capsaicin_export.h
)

target_include_directories(capsaicin
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/src/core
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/capsaicin
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render_techniques
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/
)

target_compile_features(capsaicin PUBLIC cxx_std_20)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(capsaicin PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror>)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(capsaicin PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/MP /W4 /WX /experimental:external /external:anglebrackets /external:W0 /analyze:external->)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if("${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
        target_compile_options(capsaicin PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX>)
    else()
        target_compile_options(capsaicin PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror>)
    endif()
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(capsaicin PRIVATE -march=x86-64-v3)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(capsaicin PRIVATE /arch:AVX2)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        if("${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
            target_compile_options(capsaicin PRIVATE /arch:AVX2)
        else()
            target_compile_options(capsaicin PRIVATE -march=x86-64-v3)
        endif()
    endif()
endif()

target_compile_definitions(capsaicin PRIVATE
    GLM_FORCE_XYZW_ONLY
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
    target_compile_definitions(capsaicin PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _HAS_EXCEPTIONS=0
        _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
        _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
        NOMINMAX
    )
endif()

target_link_options(capsaicin PRIVATE "/SUBSYSTEM:WINDOWS")

function(assign_source_group arg1)
    foreach(_source IN ITEMS ${ARGN})
        get_filename_component(PARENT_DIR "${_source}" DIRECTORY)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/include" "" GROUP "${PARENT_DIR}")
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/src" "" GROUP "${GROUP}")
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${GROUP}")
        string(REPLACE "../" "" GROUP "${GROUP}")
        string(REPLACE "/" "\\" GROUP "${GROUP}")
        source_group("${arg1}\\${GROUP}" FILES "${_source}")
    endforeach()
endfunction(assign_source_group)

assign_source_group("Header Files" ${CAPSAICIN_HEADER_FILES})
assign_source_group("Source Files" ${CAPSAICIN_SOURCE_FILES})
assign_source_group("Shader Files" ${CAPSAICIN_SHADER_FILES})
assign_source_group("Shader Files" ${CAPSAICIN_THIRD_PARTY_SHADERS})

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/stb")
    target_include_directories(capsaicin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/stb")
else()
    find_package(Stb REQUIRED)
    target_include_directories(capsaicin PRIVATE "${Stb_INCLUDE_DIR}")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/tinyexr")
    target_include_directories(capsaicin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/tinyexr")
else()
    find_package(tinyexr REQUIRED)
    target_link_libraries(capsaicin PRIVATE unofficial::tinyexr::tinyexr)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/glm")
    target_include_directories(capsaicin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/glm")
else()
    find_package(glm REQUIRED)
    target_link_libraries(capsaicin PUBLIC glm::glm)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/imgui")
    target_include_directories(capsaicin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/imgui"  
            "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfx/third_party/imgui/misc/cpp")
else()
    find_package(imgui REQUIRED)
    target_link_libraries(capsaicin PUBLIC imgui::imgui)
endif()

target_link_libraries(capsaicin PUBLIC gfx
    PRIVATE yaml-cpp::yaml-cpp meshoptimizer::meshoptimizer)

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

# Install the library, headers and runtime components
include(GNUInstallDirs)
install(TARGETS capsaicin
    EXPORT capsaicin-targets
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/capsaicin
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET capsaicin_shaders DESTINATION ${CMAKE_INSTALL_BINDIR}/src/core/
    FILE_SET capsaicin_thirdparty_shaders DESTINATION ${CMAKE_INSTALL_BINDIR}/third_party
)
