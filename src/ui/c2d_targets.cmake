# ---------------------------------------------------------------------------
# c2d: the Canvas 2D shim and everything built on it. Part of the c2d
# graphics kit (docs/guides/js-graphics-port.md); included from the top of
# src/CMakeLists.txt so the kit is one line in a shared file.
#
# UI modules ported from the JS reference draw through src/ui/c2d.h and never
# call raylib directly -- see docs/CANVAS2D_PORT_SPEC.md. The shim is C; the
# headers carry extern "C" guards so the C++ game can call it. Link
# colony_c2d into a game target when a ported module is wired in (protocol
# step 7), and add each new port's .c to the library below.
# ---------------------------------------------------------------------------
add_library(colony_c2d STATIC
    "${CMAKE_CURRENT_LIST_DIR}/c2d.c"
    "${CMAKE_CURRENT_LIST_DIR}/toolrack.c")
target_include_directories(colony_c2d PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_link_libraries(colony_c2d PUBLIC raylib)
set_target_properties(colony_c2d PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

# The shim's acceptance tests (tools/c2dtest) and one <name>_visdiff target
# per ported module (tools/visdiff) -- see the harness recipe in the guide.
if(NOT "${PLATFORM}" STREQUAL "Web")
    set(C2D_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../tools")

    add_executable(c2dtest "${C2D_TOOLS_DIR}/c2dtest/c2dtest.c")
    target_link_libraries(c2dtest PRIVATE colony_c2d raylib)

    add_executable(toolrack_visdiff "${C2D_TOOLS_DIR}/visdiff/toolrack_main.c")
    target_link_libraries(toolrack_visdiff PRIVATE colony_c2d raylib)

    foreach(t c2dtest toolrack_visdiff)
        set_target_properties(${t} PROPERTIES
            C_STANDARD 99 C_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
        if(NOT WIN32)
            target_link_libraries(${t} PRIVATE m)
        endif()
    endforeach()
endif()
