file(
        GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

include(FetchContent)
FetchContent_Declare(
        mimalloc EXCLUDE_FROM_ALL
        GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
        GIT_TAG v3.5.4
        GIT_SHALLOW ON
        GIT_PROGRESS ON
)
set(MI_OVERRIDE OFF)
FetchContent_Declare(
        miniaudio EXCLUDE_FROM_ALL
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG 0.11.25
        GIT_SHALLOW ON
        GIT_PROGRESS ON
)
FetchContent_MakeAvailable(mimalloc miniaudio)

# Shared
add_library(axsedCore SHARED ${SRC_FILES})
target_compile_definitions(axsedCore PRIVATE AXSEDCORE_EXPORT)
LIST(APPEND __AXSEDCORE_LIBS axsedCore)

# Static
add_library(axsedCore_s STATIC ${SRC_FILES})
target_compile_definitions(axsedCore_s PUBLIC AXSEDCORE_NO_EXPORT)
LIST(APPEND __AXSEDCORE_LIBS axsedCore_s)

# common setting
foreach (_AXSEDCORE_LIB ${__AXSEDCORE_LIBS})
    set_target_properties(
            ${_AXSEDCORE_LIB} PROPERTIES
            OUTPUT_NAME ${_AXSEDCORE_LIB}${LIB_CONFIG_SUFFIX}
            C_VISIBILITY_PRESET hidden
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES
    )
    target_include_directories(
            ${_AXSEDCORE_LIB} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
            $<INSTALL_INTERFACE:include>
    )
    if (${_AXSED_MINGW_UCRT})
        target_compile_definitions(mimalloc-static PUBLIC MI_MINGW_UCRT64)
    endif ()
    target_link_libraries(${_AXSEDCORE_LIB} PRIVATE mimalloc-static)
    target_sources(${_AXSEDCORE_LIB} PRIVATE ${miniaudio_SOURCE_DIR}/miniaudio.c)
    target_include_directories(${_AXSEDCORE_LIB} PRIVATE ${miniaudio_SOURCE_DIR})
    if (MSVC)
        target_compile_options(${_AXSEDCORE_LIB} PUBLIC /utf-8)
    endif ()
endforeach ()
