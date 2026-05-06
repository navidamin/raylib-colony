cmake_minimum_required(VERSION 3.14)
project(sample_render_test C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Reuse parent's raylib if already fetched, otherwise fetch
find_package(raylib QUIET)
if(NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG master
        GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(sample_render_test main.cpp)
target_link_libraries(sample_render_test raylib)

if(NOT WIN32 AND NOT "${PLATFORM}" STREQUAL "Web")
    target_link_libraries(sample_render_test m)
endif()

if(APPLE)
    target_link_libraries(sample_render_test "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
