# CMake helper functions.
#
# get_all_installable_targets() is called from the root CMakeLists.txt's
# MSVC block. It comes from the cpp-starter project template this build
# was seeded from, but the template's cmake/ helpers were never vendored
# in — so every Windows CI run failed at configure time with
# "Unknown CMake command get_all_installable_targets". Defining it here
# restores the intended behaviour instead of dropping the MSVC block.

# Collect the targets defined in `dir` and all of its subdirectories.
macro(get_all_targets_recursive targets dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()

    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND ${targets} ${current_targets})
endmacro()

function(get_all_targets var)
    set(targets)
    get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR})
    set(${var} ${targets} PARENT_SCOPE)
endfunction()

# Targets that can carry arbitrary properties. INTERFACE libraries are
# excluded because CMake only allows a whitelist of properties on them —
# setting VS_DEBUGGER_ENVIRONMENT on one is a hard error, and this build
# has interface targets (tomlplusplus).
function(get_all_installable_targets var)
    set(targets)
    get_all_targets(all_targets)
    foreach(_target ${all_targets})
        get_target_property(_target_type ${_target} TYPE)
        if(NOT _target_type MATCHES ".*INTERFACE.*")
            list(APPEND targets ${_target})
        endif()
    endforeach()
    set(${var} ${targets} PARENT_SCOPE)
endfunction()
