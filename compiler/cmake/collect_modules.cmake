# collect_modules.cmake
#
# Scans all .cpp files under a given directory for TML_MODULE("xxx")
# declarations and generates per-module source lists.
#
# Usage:
#   include(cmake/collect_modules.cmake)
#   collect_module_sources("${CMAKE_CURRENT_SOURCE_DIR}/src" MOD)
#
# After calling, the following variables are set in the parent scope:
#   MOD_compiler         - sources for the compiler core plugin
#   MOD_codegen_x86      - sources for the LLVM x86_64 backend
#   MOD_codegen_arm64    - sources for the LLVM AArch64 backend
#   MOD_codegen_cranelift - sources for the Cranelift backend
#   MOD_tools            - sources for formatter/linter/doc
#   MOD_test             - sources for test runner
#   MOD_mcp              - sources for MCP server
#   MOD_ALL_NAMES        - list of all module names found

function(collect_module_sources ROOT_DIR PREFIX)
    # Find all .cpp files
    file(GLOB_RECURSE _all_sources "${ROOT_DIR}/*.cpp")

    # Known module names
    set(_known_modules
        compiler
        codegen_x86
        codegen_arm64
        codegen_cranelift
        tools
        test
        mcp
        launcher
    )

    # Initialize empty lists
    foreach(_mod ${_known_modules})
        set(_${PREFIX}_${_mod} "")
    endforeach()
    set(_found_modules "")

    foreach(_src ${_all_sources})
        # Skip plugin loader itself (belongs to launcher, not any plugin)
        get_filename_component(_fname "${_src}" NAME)
        if(_fname STREQUAL "main_launcher.cpp" OR
           _fname STREQUAL "loader.cpp" AND _src MATCHES "plugin/")
            continue()
        endif()

        # Read file and look for TML_MODULE("xxx")
        file(READ "${_src}" _content)
        string(REGEX MATCH "TML_MODULE\\(\"([^\"]+)\"\\)" _match "${_content}")

        if(_match)
            string(REGEX REPLACE "TML_MODULE\\(\"([^\"]+)\"\\)" "\\1" _mod "${_match}")
        else()
            # Default: files without TML_MODULE go to "compiler"
            set(_mod "compiler")
        endif()

        # Append to the appropriate list
        list(APPEND _${PREFIX}_${_mod} "${_src}")

        # Track discovered modules
        if(NOT "${_mod}" IN_LIST _found_modules)
            list(APPEND _found_modules "${_mod}")
        endif()
    endforeach()

    # Export to parent scope
    foreach(_mod ${_known_modules})
        set(${PREFIX}_${_mod} "${_${PREFIX}_${_mod}}" PARENT_SCOPE)
    endforeach()

    # Also export any unknown modules that were found
    foreach(_mod ${_found_modules})
        if(NOT "${_mod}" IN_LIST _known_modules)
            set(${PREFIX}_${_mod} "${_${PREFIX}_${_mod}}" PARENT_SCOPE)
            message(STATUS "  [collect_modules] Found unknown module: ${_mod}")
        endif()
    endforeach()

    set(${PREFIX}_ALL_NAMES "${_found_modules}" PARENT_SCOPE)

    # Report
    foreach(_mod ${_known_modules})
        list(LENGTH _${PREFIX}_${_mod} _count)
        if(_count GREATER 0)
            message(STATUS "  [collect_modules] ${_mod}: ${_count} sources")
        endif()
    endforeach()
endfunction()
