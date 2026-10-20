file(
        GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(axsedCore SHARED ${SRC_FILES})
target_compile_definitions(axsedCore PRIVATE AXSEDCORE_EXPORT)
set_target_properties(
        axsedCore PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
target_include_directories(
        axsedCore PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)
if (MSVC)
    target_compile_options(axsedCore PUBLIC /utf-8)
endif ()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
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
set(MINIAUDIO_NO_DECODING ON)
set(MINIAUDIO_NO_ENCODING ON)
FetchContent_MakeAvailable(
        mimalloc
        miniaudio
)
if (${_AXSED_MINGW_UCRT})
    target_compile_definitions(mimalloc-static PUBLIC MI_MINGW_UCRT64)
endif ()

target_link_libraries(axsedCore PRIVATE mimalloc-static)
target_link_libraries(axsedCore PRIVATE miniaudio)