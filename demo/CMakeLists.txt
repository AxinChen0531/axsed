file(
        GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_executable(axsedDemo ${SRC_FILES})
target_link_libraries(axsedDemo PRIVATE axsedCore)
add_custom_command(
        TARGET axsedDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:axsedDemo>
        $<TARGET_FILE_DIR:axsedDemo>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs next to axsedDemo"
)
