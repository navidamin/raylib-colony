# Core game sources shared by the game executable and the offscreen preview tool
set(COLONY_CORE_SOURCES
    Colony/colony.cpp
    Engine/Engine.cpp
    Engine/inputmanager.cpp
    Engine/viewmanager.cpp
    Engine/gamemanager.cpp
    Engine/rendermanager.cpp
    Planet/planet.cpp
    Sect/sect.cpp
    Unit/unit.cpp
    Unit/unit_ui.cpp
    ResourceManager/resource_manager.cpp
    TimeManager/time_manager.cpp
    GameTypes/game_types_loader.cpp
    Prospecting/prospecting_types.cpp
    Prospecting/sample_tray.cpp
    Prospecting/prospecting_grid.cpp
    Prospecting/sweep_engine.cpp
    Prospecting/sampling_engine.cpp
    Prospecting/lab_engine.cpp
    Prospecting/survey_progress_engine.cpp
    Prospecting/prospecting_system.cpp
    transport_types.cpp
)

set(COLONY_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Colony"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/Planet"
    "${CMAKE_CURRENT_SOURCE_DIR}/Sect"
    "${CMAKE_CURRENT_SOURCE_DIR}/Unit"
    "${CMAKE_CURRENT_SOURCE_DIR}/ResourceManager"
    "${CMAKE_CURRENT_SOURCE_DIR}/TimeManager"
    "${CMAKE_CURRENT_SOURCE_DIR}/GameTypes"
    "${CMAKE_CURRENT_SOURCE_DIR}/UnlockRegistry"
    "${CMAKE_CURRENT_SOURCE_DIR}/Prospecting"
)

add_executable(colony_game)

# Add all source files
target_sources(colony_game PRIVATE
    main.cpp
    ${COLONY_CORE_SOURCES}
)

# Set C++ standard
set_target_properties(colony_game PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(colony_game PRIVATE ${COLONY_INCLUDE_DIRS})

# Link libraries
target_link_libraries(colony_game raylib tomlplusplus::tomlplusplus)

# Platform specific configurations
if(NOT WIN32 AND NOT "${PLATFORM}" STREQUAL "Web")
    target_link_libraries(colony_game m)
endif()

# Web Configurations
if ("${PLATFORM}" STREQUAL "Web")
    set_target_properties(colony_game PROPERTIES SUFFIX ".html")

    set(web_link_flags "-s USE_GLFW=3 -s FORCE_FILESYSTEM=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s GL_ENABLE_GET_PROC_ADDRESS=1")
    if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        set(web_link_flags "${web_link_flags} -s ASSERTIONS=1")
    endif()
    # Preload assets into the virtual filesystem at the path the game expects (src/assets/...)
    set(web_link_flags "${web_link_flags} --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets@src/assets --use-preload-plugins")
    set(web_link_flags "${web_link_flags} --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/minshell.html")

    set_target_properties(colony_game PROPERTIES LINK_FLAGS "${web_link_flags}")
endif()

# MacOS specific configurations
if(APPLE)
    target_link_libraries(colony_game
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()

# Set working directory for Visual Studio
set_target_properties(colony_game PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Set as startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT colony_game
)

# ---------------------------------------------------------------------------
# colony_preview: offscreen UI preview tool
#
# Renders a single module panel to a PNG without running the game loop, so UI
# work can be reviewed headlessly (see tools/preview/README.md). Not built for
# the Web platform, which has no filesystem to export screenshots to.
# ---------------------------------------------------------------------------
if(NOT "${PLATFORM}" STREQUAL "Web")
    add_executable(colony_preview)

    target_sources(colony_preview PRIVATE
        "${CMAKE_SOURCE_DIR}/tools/preview/preview_main.cpp"
        ${COLONY_CORE_SOURCES}
    )

    set_target_properties(colony_preview PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_include_directories(colony_preview PRIVATE ${COLONY_INCLUDE_DIRS})

    target_link_libraries(colony_preview raylib tomlplusplus::tomlplusplus)

    if(NOT WIN32)
        target_link_libraries(colony_preview m)
    endif()

    if(APPLE)
        target_link_libraries(colony_preview
            "-framework IOKit"
            "-framework Cocoa"
            "-framework OpenGL"
        )
    endif()

endif()

# ---------------------------------------------------------------------------
# colony_playtest: interactive prospecting sandbox
#
# Boots straight into the prospecting module with a live game loop, for
# gameplay testing without navigating the full game (see
# tools/playtest/playtest_main.cpp for controls). Builds for Web too, so it
# can be played on phone/tablet via GitHub Pages.
# ---------------------------------------------------------------------------
add_executable(colony_playtest)

target_sources(colony_playtest PRIVATE
    "${CMAKE_SOURCE_DIR}/tools/playtest/playtest_main.cpp"
    ${COLONY_CORE_SOURCES}
)

set_target_properties(colony_playtest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(colony_playtest PRIVATE ${COLONY_INCLUDE_DIRS})

target_link_libraries(colony_playtest raylib tomlplusplus::tomlplusplus)

if(NOT WIN32 AND NOT "${PLATFORM}" STREQUAL "Web")
    target_link_libraries(colony_playtest m)
endif()

if ("${PLATFORM}" STREQUAL "Web")
    # Same shell, preloaded assets, and link flags as the main game
    set_target_properties(colony_playtest PROPERTIES SUFFIX ".html")
    set_target_properties(colony_playtest PROPERTIES LINK_FLAGS "${web_link_flags}")
endif()

if(APPLE)
    target_link_libraries(colony_playtest
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()

# ---------------------------------------------------------------------------
# colony_inspect: terminal dump of generated game data
#
# Prints real ResourceManager / ProspectingGrid numbers so value bugs can be
# diagnosed from data instead of from reading generation code. Not built for
# Web. See tools/inspect/README.md.
# ---------------------------------------------------------------------------
if(NOT "${PLATFORM}" STREQUAL "Web")
    add_executable(colony_inspect)

    target_sources(colony_inspect PRIVATE
        "${CMAKE_SOURCE_DIR}/tools/inspect/inspect_resources.cpp"
        ResourceManager/resource_manager.cpp
        Prospecting/prospecting_grid.cpp
        Prospecting/prospecting_types.cpp
    )

    set_target_properties(colony_inspect PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_include_directories(colony_inspect PRIVATE ${COLONY_INCLUDE_DIRS})
    target_link_libraries(colony_inspect raylib tomlplusplus::tomlplusplus)

    if(NOT WIN32)
        target_link_libraries(colony_inspect m)
    endif()
endif()
