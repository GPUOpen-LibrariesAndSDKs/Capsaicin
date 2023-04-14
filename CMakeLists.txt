cmake_minimum_required(VERSION 3.10.0)

project(Capsaicin
    VERSION 1.0.0
    DESCRIPTION "AMD experimental real-time rendering framework designed for graphics research and development"
)

set(GFX_BUILD_EXAMPLES            OFF CACHE BOOL "Build gfx examples")
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "Build loader_example")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set preprocessor definitions
add_definitions(/MP
    -D_HAS_ITERATOR_DEBUGGING=0
)

# Gather dependencies
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/gfx)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11)
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz)

# Set linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Organize third party projects
set_target_properties(uninstall PROPERTIES FOLDER "third_party")
set_target_properties(miniz PROPERTIES FOLDER "third_party")
set_target_properties(gfx PROPERTIES FOLDER "third_party")
set_target_properties(CLI11 PROPERTIES FOLDER "third_party")
set_target_properties(tinyobjloader PROPERTIES FOLDER "third_party/gfx_deps")
set_target_properties(ktx PROPERTIES FOLDER "third_party/gfx_deps")
set_target_properties(astcenc-avx2-static PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")
set_target_properties(ktx_read PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")
set_target_properties(ktx_version PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")
set_target_properties(obj_basisu_cbind PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")
set_target_properties(objUtil PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")

if(TARGET mkvk)
    set_target_properties(mkvk PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")
    set_target_properties(makedfd2vk PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")
    set_target_properties(makevk2dfd PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")
    set_target_properties(mkvkformatfiles PROPERTIES FOLDER "third_party/gfx_deps/ktx_deps")
endif()

# Set project output directory variables.
IF(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  SET(CAPSAICIN_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin" CACHE STRING "Path for runtime output files")
ELSE()
  SET(CAPSAICIN_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} CACHE STRING "Path for runtime output files")
ENDIF()

IF(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  SET(CAPSAICIN_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin" CACHE STRING "Path for library output files")
ELSE()
  SET(CAPSAICIN_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} CACHE STRING "Path for library output files")
ENDIF()

IF(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  SET(CAPSAICIN_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib" CACHE STRING "Path for archive output files")
ELSE()
  SET(CAPSAICIN_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY} CACHE STRING "Path for archive output files")
ENDIF()

# Build Capsaicin
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Set up startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             PROPERTY VS_STARTUP_PROJECT scene_viewer)

# Install assets and shaders directories
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION . FILES_MATCHING PATTERN "*.*")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/core/shaders DESTINATION src/core FILES_MATCHING PATTERN "*.*")

# Configure CPack
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}")
include(CPack)
