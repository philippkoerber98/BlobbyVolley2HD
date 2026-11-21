cmake_minimum_required(VERSION 3.16)

project(blobby VERSION 1.1.1)

set(BLOBBY_BUILD_VERSION "${PROJECT_VERSION}" CACHE STRING "Distribution version")
set(BLOBBY_BUILD_NUMBER "0" CACHE STRING "Windows build revision")
if(NOT BLOBBY_BUILD_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+(-build\\.[0-9]+\\.[0-9]+)?$")
    message(FATAL_ERROR "Invalid BLOBBY_BUILD_VERSION")
endif()
if(NOT BLOBBY_BUILD_NUMBER MATCHES "^[0-9]+$" OR BLOBBY_BUILD_NUMBER GREATER 65535)
    message(FATAL_ERROR "BLOBBY_BUILD_NUMBER must fit an unsigned Windows version component")
endif()
configure_file(win/version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)


option(BUILD_TESTS "Build test programs" OFF)
option(BUILD_MACOS_BUNDLE "Create a self-containing MacOS bundle" OFF)

# use FindBoost.cmake instead of BoostConfig.cmake
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

include(deps/sdl2.cmake)
include(deps/physfs.cmake)

add_subdirectory(data)
add_subdirectory(src)
add_subdirectory(deps)

add_subdirectory(linux)

add_subdirectory(macos)

if(BUILD_TESTS)
	add_subdirectory(test)
endif()

include(CPack)
