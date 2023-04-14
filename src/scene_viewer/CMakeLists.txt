add_executable(scene_viewer ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/main_shared.h
	${CMAKE_CURRENT_SOURCE_DIR}/main_shared.cpp
)

target_compile_options(scene_viewer PRIVATE
    /W3 /WX
    -D_CRT_SECURE_NO_WARNINGS
)

target_link_libraries(scene_viewer PRIVATE core CLI11)

set_target_properties(scene_viewer PROPERTIES
    DEBUG_POSTFIX D
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

set_target_properties(scene_viewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CAPSAICIN_RUNTIME_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CAPSAICIN_LIBRARY_OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY ${CAPSAICIN_ARCHIVE_OUTPUT_DIRECTORY}
)

# Install dlls
install(FILES $<TARGET_RUNTIME_DLLS:scene_viewer> DESTINATION .)

# Install the executable
install(TARGETS scene_viewer RUNTIME DESTINATION .)
