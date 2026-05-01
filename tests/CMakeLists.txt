add_executable(colony_tests
    test_prospecting_types.cpp
    test_sample_tray.cpp
    test_prospecting_grid.cpp
    ../src/Prospecting/prospecting_types.cpp
    ../src/Prospecting/sample_tray.cpp
    ../src/Prospecting/prospecting_grid.cpp
    ../src/ResourceManager/resource_manager.cpp
)

target_include_directories(colony_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/Prospecting"
    "${CMAKE_SOURCE_DIR}/src/ResourceManager"
    "${CMAKE_SOURCE_DIR}/src/GameTypes"
    "${CMAKE_SOURCE_DIR}/src/UnlockRegistry"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(colony_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(colony_tests PRIVATE Catch2::Catch2WithMain raylib)

if(NOT WIN32 AND NOT "${PLATFORM}" STREQUAL "Web")
    target_link_libraries(colony_tests PRIVATE m)
endif()

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(colony_tests)
