add_executable(scene_viewer WIN32 ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main_shared.h
	${CMAKE_CURRENT_SOURCE_DIR}/main_shared.cpp
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(scene_viewer PRIVATE -march=x86-64-v3)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(scene_viewer PRIVATE /arch:AVX2)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        if("${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
            target_compile_options(scene_viewer PRIVATE /arch:AVX2)
        else()
            target_compile_options(scene_viewer PRIVATE -march=x86-64-v3)
        endif()
    endif()
endif()

target_compile_features(scene_viewer PUBLIC cxx_std_20)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(scene_viewer PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror>)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    target_compile_options(scene_viewer PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/MP /W4 /WX /experimental:external /external:anglebrackets /external:W0 /analyze:external->)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if("${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
        target_compile_options(scene_viewer PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX>)
    else()
        target_compile_options(scene_viewer PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror>)
    endif()
endif()
target_compile_definitions(scene_viewer PRIVATE
    GLM_FORCE_XYZW_ONLY
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

target_compile_definitions(scene_viewer PRIVATE "$<$<CONFIG:RelWithDebInfo>:SHADER_DEBUG>")

target_link_libraries(scene_viewer PRIVATE capsaicin CLI11::CLI11)

target_link_options(scene_viewer PRIVATE "/SUBSYSTEM:WINDOWS")

set_target_properties(scene_viewer PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set_target_properties(scene_viewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CAPSAICIN_RUNTIME_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CAPSAICIN_LIBRARY_OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY ${CAPSAICIN_ARCHIVE_OUTPUT_DIRECTORY}
)

# Install the executable
include(GNUInstallDirs)
install(TARGETS scene_viewer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install dlls
install(FILES $<TARGET_RUNTIME_DLLS:scene_viewer> DESTINATION ${CMAKE_INSTALL_BINDIR})

IF(CAPSAICIN_PREPARE_TEST_MEDIA_SCENES)
    # Install assets
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/CapsaicinTestMedia
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/assets
        FILES_MATCHING
        PATTERN "*.gltf"
        PATTERN "*.bin"
        PATTERN "*.png"
        PATTERN "*.ktx2"
        PATTERN "*.dds"
    )
endif()
