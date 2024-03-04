add_executable(scene_viewer WIN32 ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main_shared.h
	${CMAKE_CURRENT_SOURCE_DIR}/main_shared.cpp
)

target_compile_options(scene_viewer PRIVATE
    /W4 /WX /external:anglebrackets /external:W0 /analyze:external-
    -D_CRT_SECURE_NO_WARNINGS
	-DNOMINMAX
)

target_compile_definitions(scene_viewer PRIVATE "$<$<CONFIG:RelWithDebInfo>:SHADER_DEBUG>")

target_link_libraries(scene_viewer PRIVATE capsaicin CLI11)

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
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
)

# Install dlls
install(FILES $<TARGET_RUNTIME_DLLS:scene_viewer> DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR})

# Install assets
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../assets
	DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
	FILES_MATCHING
	PATTERN "*.gltf"
	PATTERN "*.bin"
	PATTERN "*.png"
	PATTERN "*.ktx2"
)
