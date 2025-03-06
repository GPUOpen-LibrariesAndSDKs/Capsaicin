cmake_minimum_required(VERSION 3.25.0)
cmake_policy(SET CMP0141 NEW)

project(Capsaicin
    LANGUAGES CXX
    VERSION 1.2.0
    DESCRIPTION "AMD experimental real-time rendering framework designed for graphics research and development"
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Disable unused parameters from 3rd party directories
set(GFX_BUILD_EXAMPLES            OFF CACHE BOOL "")
set(BUILD_TESTING                 OFF CACHE BOOL "")

# Enable gfx options
set(GFX_ENABLE_SCENE              ON CACHE BOOL "")
set(GFX_ENABLE_GUI                ON CACHE BOOL "")

# Gather dependencies
include(FetchContent)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/gfx EXCLUDE_FROM_ALL)
set_target_properties(gfx PROPERTIES FOLDER "third_party")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "" FORCE)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cli11/"
    FIND_PACKAGE_ARGS 2.0.0 NAMES CLI11
)
FetchContent_MakeAvailable(CLI11)
if(NOT CLI11_FOUND)
    set_target_properties(CLI11 PROPERTIES FOLDER "third_party")
endif()

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yaml-cpp/"
    FIND_PACKAGE_ARGS 0.7.0 NAMES yaml-cpp
)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "")
FetchContent_MakeAvailable(yaml-cpp)
if(NOT yaml-cpp_FOUND)
    set_target_properties(yaml-cpp PROPERTIES FOLDER "third_party")
endif()

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json/"
    FIND_PACKAGE_ARGS NAMES nlohmann_json
)
FetchContent_MakeAvailable(nlohmann_json)
if(NOT nlohmann_json_FOUND)
    set_target_properties(nlohmann_json PROPERTIES FOLDER "third_party")
endif()

FetchContent_Declare(
    meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
    GIT_TAG        v0.22
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer/"
    FIND_PACKAGE_ARGS NAMES meshoptimizer
)
FetchContent_MakeAvailable(meshoptimizer)
if(NOT meshoptimizer_FOUND)
    set_target_properties(meshoptimizer PROPERTIES FOLDER "third_party")
    add_library(meshoptimizer::meshoptimizer ALIAS meshoptimizer)
endif()

option(CAPSAICIN_DOWNLOAD_TEST_MEDIA "Download test media scenes" ON)
if(CAPSAICIN_DOWNLOAD_TEST_MEDIA)
    FetchContent_Declare(
        CapsaicinTestMedia
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/CapsaicinTestMedia.git
        GIT_TAG        v1.2
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
        SOURCE_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/assets/CapsaicinTestMedia/"
    )
    FetchContent_MakeAvailable(CapsaicinTestMedia)
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
