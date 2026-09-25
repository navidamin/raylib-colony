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
    ${CMAKE_SOURCE_DIR}/src/Prospecting/lab_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/Prospecting/survey_progress_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/Prospecting/prospecting_system.cpp
    ${CMAKE_SOURCE_DIR}/src/Prospecting/rock_texture.cpp
    # unit.h includes excavation_system.h, so anything compiling unit.h needs
    # these -- which is most of this library.
    ${CMAKE_SOURCE_DIR}/src/Excavation/site_view.cpp
    ${CMAKE_SOURCE_DIR}/src/Excavation/estimate_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/Excavation/dig_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/Excavation/auto_pilot.cpp
    ${CMAKE_SOURCE_DIR}/src/Excavation/excavation_system.cpp
    # the survey console, which ProspectingSystem owns
    ${CMAKE_SOURCE_DIR}/src/Survey/survey_ground.cpp
    ${CMAKE_SOURCE_DIR}/src/Survey/survey_block.cpp
    ${CMAKE_SOURCE_DIR}/src/Survey/survey_console.cpp
    ${CMAKE_SOURCE_DIR}/src/Survey/survey_rig.cpp
    # unit_ui.cpp resolves the pointer through ColonyGetMousePosition().
    ${CMAKE_SOURCE_DIR}/src/web_mouse.cpp
    # unit_ui.cpp, sect.cpp and planet.cpp lay out in the logical size.
    ${CMAKE_SOURCE_DIR}/src/display_scale.cpp
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
    ${CMAKE_SOURCE_DIR}/src/Excavation
    # ProspectingSystem owns the survey console's state (survey_console.h,
    # and the C knowledge model in src/ui). Without these the library has not
    # compiled since the console was connected to real data.
    ${CMAKE_SOURCE_DIR}/src/Survey
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/TerrainGen
)

# colony_c2d for the console's C modules (knowledge model, drill, dash
# state) that ProspectingSystem embeds -- the same library the game links.
target_link_libraries(colony_testlib PUBLIC raylib tomlplusplus::tomlplusplus colony_c2d)
if(NOT WIN32)
    target_link_libraries(colony_testlib PUBLIC m)
endif()

# ---- Test executable ----
add_executable(colony_tests
    test_resource_types.cpp
    test_display_scale.cpp
    test_bed_palette.cpp
    test_time_manager.cpp
    test_storage.cpp
    test_prospecting_types.cpp
    test_sample_tray.cpp
    test_prospecting_grid.cpp
    test_sweep.cpp
    test_sampling.cpp
    test_line_hole.cpp
    test_block_pick.cpp
    test_dig_site.cpp
    test_dig_rate.cpp
    test_update_ownership.cpp
    test_rock_texture.cpp
    test_plate_light.cpp
    test_lab_pipeline.cpp
    test_prospecting_integration.cpp
    test_survey_progress.cpp
    test_prospecting_wiring.cpp
    test_resource_class.cpp
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
