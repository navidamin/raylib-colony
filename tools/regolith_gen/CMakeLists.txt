cmake_minimum_required(VERSION 3.14)
project(regolith_gen C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use the same raylib from the parent project's build
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

add_executable(regolith_gen main.cpp)
target_link_libraries(regolith_gen raylib)

if(NOT WIN32 AND NOT "${PLATFORM}" STREQUAL "Web")
    target_link_libraries(regolith_gen m)
endif()

if(APPLE)
    target_link_libraries(regolith_gen "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
