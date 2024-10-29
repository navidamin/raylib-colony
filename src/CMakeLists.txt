add_executable(colony_game)

# Add all source files
target_sources(colony_game PRIVATE
    src/main.cpp
    src/Colony/colony.cpp
    src/Engine/Engine.cpp
    src/Planet/planet.cpp
    src/Sect/sect.cpp
    src/Unit/unit.cpp
)

# Set C++ standard
set_target_properties(colony_game PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(colony_game PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Colony"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/Planet"
    "${CMAKE_CURRENT_SOURCE_DIR}/Sect"
    "${CMAKE_CURRENT_SOURCE_DIR}/Unit"
)

# Link raylib
target_link_libraries(colony_game raylib)

# Platform specific configurations
if(NOT WIN32)
    target_link_libraries(colony_game m)
endif()

# Web Configurations
if (${PLATFORM} STREQUAL "Web")
    set_target_properties(colony_game PROPERTIES SUFFIX ".html")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_GLFW=3 -s FORCE_FILESYSTEM=1 -s WASM=1")

    set(web_link_flags)
    if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        set(web_link_flags "${web_link_flags} -s ASSERTIONS=1")
    endif()
    set(web_link_flags "${web_link_flags} --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/resources@resources --use-preload-plugins")
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
