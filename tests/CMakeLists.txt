# Test library: compile game logic sources that can be tested without a display.
# We link raylib for types (Vector2, Color, etc.) but don't open a window.
add_library(colony_testlib STATIC
    ${CMAKE_SOURCE_DIR}/src/Colony/colony.cpp
    ${CMAKE_SOURCE_DIR}/src/Sect/sect.cpp
    ${CMAKE_SOURCE_DIR}/src/Unit/unit.cpp
    ${CMAKE_SOURCE_DIR}/src/Unit/unit_ui.cpp
    ${CMAKE_SOURCE_DIR}/src/ResourceManager/resource_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/TimeManager/time_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/GameTypes/game_types_loader.cpp
    ${CMAKE_SOURCE_DIR}/src/transport_types.cpp
    ${CMAKE_SOURCE_DIR}/src/Planet/planet.cpp
    ${CMAKE_SOURCE_DIR}/src/Prospecting/prospecting_types.cpp
    ${CMAKE_SOURCE_DIR}/src/Prospecting/sample_tray.cpp
    ${CMAKE_SOURCE_DIR}/src/Prospecting/prospecting_grid.cpp
    ${CMAKE_SOURCE_DIR}/src/Prospecting/sweep_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/Prospecting/sampling_engine.cpp
)

set_target_properties(colony_testlib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(colony_testlib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/Colony
    ${CMAKE_SOURCE_DIR}/src/Engine
    ${CMAKE_SOURCE_DIR}/src/Planet
    ${CMAKE_SOURCE_DIR}/src/Sect
    ${CMAKE_SOURCE_DIR}/src/Unit
    ${CMAKE_SOURCE_DIR}/src/ResourceManager
    ${CMAKE_SOURCE_DIR}/src/TimeManager
    ${CMAKE_SOURCE_DIR}/src/GameTypes
    ${CMAKE_SOURCE_DIR}/src/UnlockRegistry
    ${CMAKE_SOURCE_DIR}/src/Prospecting
)

target_link_libraries(colony_testlib PUBLIC raylib tomlplusplus::tomlplusplus)
if(NOT WIN32)
    target_link_libraries(colony_testlib PUBLIC m)
endif()

# ---- Test executable ----
add_executable(colony_tests
    test_resource_types.cpp
    test_time_manager.cpp
    test_storage.cpp
    test_prospecting_types.cpp
    test_sample_tray.cpp
    test_prospecting_grid.cpp
    test_sweep.cpp
    test_sampling.cpp
)

set_target_properties(colony_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(colony_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(colony_tests PRIVATE colony_testlib Catch2::Catch2WithMain)

# Register tests with CTest
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(colony_tests)
