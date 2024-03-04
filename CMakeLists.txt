cmake_minimum_required(VERSION 3.24.0)

project(Capsaicin
    VERSION 1.1.0
    DESCRIPTION "AMD experimental real-time rendering framework designed for graphics research and development"
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set preprocessor definitions
add_definitions(    
    /MP
)

# Disable unused parameters from 3rd party directories
set(GFX_BUILD_EXAMPLES            OFF CACHE BOOL "")
set(BUILD_TESTING                 OFF CACHE BOOL "")

# Enable gfx options
set(GFX_ENABLE_SCENE              ON CACHE BOOL "")
set(GFX_ENABLE_GUI                ON CACHE BOOL "")

# Gather dependencies
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/gfx EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/CLI11 EXCLUDE_FROM_ALL)
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)

# Organize third party projects
set_target_properties(uninstall PROPERTIES FOLDER "third_party")
set_target_properties(gfx PROPERTIES FOLDER "third_party")
set_target_properties(CLI11 PROPERTIES FOLDER "third_party")
set_target_properties(tinyobjloader PROPERTIES FOLDER "third_party/gfx_deps")
set_target_properties(tinyexr PROPERTIES FOLDER "third_party/gfx_deps")
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

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")

# Build Capsaicin
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Set up startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             PROPERTY VS_STARTUP_PROJECT scene_viewer)
