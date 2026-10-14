file(
        GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

include(FetchContent)
FetchContent_Declare(
        mimalloc EXCLUDE_FROM_ALL
        GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
        GIT_SHALLOW ON
        GIT_PROGRESS ON
        GIT_TAG v3.5.4
)
set(MI_OVERRIDE OFF)
set(MI_DEBUG OFF)
set(MI_USE_CXX ON)
FetchContent_MakeAvailable(mimalloc)

# Shared
add_library(axsedCore SHARED ${SRC_FILES})
target_compile_definitions(axsedCore PRIVATE AXSEDCORE_EXPORT)
target_link_options(axsedCore PRIVATE)
LIST(APPEND AXSEDCORE_LIBS axsedCore)

# Static
add_library(axsedCore_s STATIC ${SRC_FILES})
target_compile_definitions(axsedCore_s PUBLIC AXSEDCORE_NO_EXPORT)
LIST(APPEND AXSEDCORE_LIBS axsedCore_s)

# common setting
foreach (AXSEDCORE_LIB ${AXSEDCORE_LIBS})
    set_target_properties(
            ${AXSEDCORE_LIB} PROPERTIES
            OUTPUT_NAME ${AXSEDCORE_LIB}${LIB_CONFIG_SUFFIX}
            C_VISIBILITY_PRESET hidden
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
    )
    target_include_directories(
            ${AXSEDCORE_LIB} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(${AXSEDCORE_LIB} PRIVATE mimalloc-static)
    if (MSVC)
        target_compile_options(${AXSEDCORE_LIB} PUBLIC /utf-8)
    endif ()
endforeach ()
