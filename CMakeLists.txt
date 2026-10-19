cmake_minimum_required(VERSION 3.30)
# basic
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(
        axsed
        VERSION 0.0.1
        DESCRIPTION "a personal cpp(std20) development utility collection"
        HOMEPAGE_URL "https://github.com/AxinChen0531/axsed.git"
)

# config

# probe
include(CheckCXXSourceCompiles)
if (MINGW)
    set(_AXSED_MINGW 1)
    check_cxx_source_compiles(
            "
                #include <cstdio>
                #ifndef _UCRT
                #error \"MinGW runtime is not UCRT\"
                #endif
                int main() { return 0; }
            "
            _AXSED_MINGW_UCRT
    )
endif ()
add_subdirectory(core)
add_subdirectory(demo EXCLUDE_FROM_ALL)

# install
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
install(
        TARGETS
        axsedCore axsedCore_s
        EXPORT axsedTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
        EXPORT axsedTargets
        NAMESPACE axsed::
        FILE axsedTargets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/axsed
)
configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/axsedConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/axsedConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/axsed
        PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)
write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/axsedConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion
)
install(
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/axsedConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/axsedConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/axsed
)
