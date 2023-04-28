cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

project(usd-simple-decimate-schema)

find_package(pxr REQUIRED)

set(CMAKE_CXX_STANDARD 17)

# external deps
include(FetchContent)
FetchContent_Declare(
    libigl
    GIT_REPOSITORY https://github.com/libigl/libigl.git
    GIT_TAG v2.5.0
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(libigl)

add_subdirectory(schemas)
add_subdirectory(imaging)

install(FILES plugInfo.json DESTINATION plugin)