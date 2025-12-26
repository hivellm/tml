cmake_minimum_required(VERSION 3.20)
project(tml VERSION 0.1.0 LANGUAGES CXX)

# C++20 required for concepts, ranges, etc.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directory for executables (separate from build cache)
# TML_OUTPUT_DIR is set by build scripts, defaults to CMAKE_BINARY_DIR
if(NOT DEFINED TML_OUTPUT_DIR OR TML_OUTPUT_DIR STREQUAL "")
    set(TML_OUTPUT_DIR ${CMAKE_BINARY_DIR})
endif()

# Put executables in the output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TML_OUTPUT_DIR})

# For multi-config generators (Visual Studio, Xcode), flatten the structure
foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${TML_OUTPUT_DIR})
endforeach()

# Libraries/archives stay in the build cache directory
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Build options
option(TML_BUILD_TESTS "Build tests" ON)
option(TML_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(TML_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
    # GCC-specific: Relax some overly strict requirements
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(
            -Wno-missing-field-initializers  # Allow partial struct init with defaults
            -fpermissive                     # Allow template calls on dependent types without 'template' keyword
        )
    else()
        # Clang: Use -Werror
        add_compile_options(-Werror)
    endif()
    if(TML_ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
    if(TML_ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined)
        add_link_options(-fsanitize=undefined)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /WX /utf-8 /FS)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Lexer library
add_library(tml_lexer STATIC
    src/lexer/token.cpp
    src/lexer/source.cpp
    src/lexer/lexer_core.cpp
    src/lexer/lexer_token.cpp
    src/lexer/lexer_ident.cpp
    src/lexer/lexer_number.cpp
    src/lexer/lexer_string.cpp
    src/lexer/lexer_operator.cpp
    src/lexer/lexer_utils.cpp
)
target_include_directories(tml_lexer PUBLIC include src)

# Parser library
add_library(tml_parser STATIC
    src/parser/ast.cpp
    src/parser/parser_core.cpp
    src/parser/parser_decl.cpp
    src/parser/parser_expr.cpp
    src/parser/parser_stmt.cpp
    src/parser/parser_type.cpp
    src/parser/parser_pattern.cpp
)
target_link_libraries(tml_parser PUBLIC tml_lexer)
target_include_directories(tml_parser PUBLIC include src)

# Type checker library
add_library(tml_types STATIC
    src/types/type.cpp
    src/types/checker.cpp
    src/types/env_scope.cpp
    src/types/env_core.cpp
    src/types/env_builtins.cpp
    src/types/env_builtins_types.cpp
    src/types/env_builtins_io.cpp
    src/types/env_builtins_string.cpp
    src/types/env_builtins_time.cpp
    src/types/env_builtins_mem.cpp
    src/types/env_definitions.cpp
    src/types/env_lookups.cpp
    src/types/module.cpp
    src/types/env_module_support.cpp
)
target_link_libraries(tml_types PUBLIC tml_parser)
target_include_directories(tml_types PUBLIC include src)

# Borrow checker library
add_library(tml_borrow STATIC
    src/borrow/checker_env.cpp
    src/borrow/checker_core.cpp
    src/borrow/checker_stmt.cpp
    src/borrow/checker_expr.cpp
    src/borrow/checker_ops.cpp
)
target_link_libraries(tml_borrow PUBLIC tml_types)
target_include_directories(tml_borrow PUBLIC include src)

# IR library
add_library(tml_ir STATIC
    src/ir/builder_utils.cpp
    src/ir/builder_module.cpp
    src/ir/builder_decls.cpp
    src/ir/builder_expr.cpp
    src/ir/builder_stmt.cpp
    src/ir/builder_type.cpp
    src/ir/emitter_core.cpp
    src/ir/emitter_decls.cpp
    src/ir/emitter_expr.cpp
    src/ir/emitter_stmt.cpp
)
target_link_libraries(tml_ir PUBLIC tml_borrow)
target_include_directories(tml_ir PUBLIC include src)

# Codegen library (LLVM IR text generator + clang backend)
add_library(tml_codegen STATIC
    src/codegen/llvm_ir_gen.cpp
    src/codegen/llvm_ir_gen_decl.cpp
    src/codegen/llvm_ir_gen_stmt.cpp
    src/codegen/llvm_ir_gen_expr.cpp
    src/codegen/llvm_ir_gen_builtins.cpp
    src/codegen/llvm_ir_gen_control.cpp
    src/codegen/llvm_ir_gen_types.cpp
    src/codegen/c_header_gen.cpp
    # src/codegen/c_gen.cpp  # Removed - using LLVM backend only
)
target_link_libraries(tml_codegen PUBLIC tml_ir)
target_include_directories(tml_codegen PUBLIC include src)

# Format library
add_library(tml_format STATIC
    src/format/format_core.cpp
    src/format/format_decl.cpp
    src/format/format_stmt.cpp
    src/format/format_expr.cpp
    src/format/format_type.cpp
    src/format/format_pattern.cpp
)
target_link_libraries(tml_format PUBLIC tml_parser)
target_include_directories(tml_format PUBLIC include src)

# CLI library
add_library(tml_cli STATIC
    src/cli/cli.cpp
    src/cli/utils.cpp
    src/cli/compiler_setup.cpp
    src/cli/cmd_debug.cpp
    src/cli/cmd_build.cpp
    src/cli/cmd_format.cpp
    src/cli/cmd_test.cpp
    src/cli/cmd_cache.cpp
    src/cli/cmd_rlib.cpp
    src/cli/rlib.cpp
    src/cli/dispatcher.cpp
    src/cli/object_compiler.cpp
    src/cli/parallel_build.cpp
)
target_link_libraries(tml_cli PUBLIC tml_codegen tml_format)
target_include_directories(tml_cli PUBLIC include src)

# Main executable
add_executable(tml src/main.cpp)
target_link_libraries(tml PRIVATE tml_cli)

# Tests
if(TML_BUILD_TESTS)
    enable_testing()

    # Fetch GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    # Test executable
    add_executable(tml_tests
        tests/lexer_test.cpp
        tests/parser_test.cpp
        tests/types_test.cpp
        tests/borrow_test.cpp
        tests/ir_test.cpp
        tests/format_test.cpp
        tests/codegen_test.cpp
        tests/cli/object_compiler_test.cpp
        tests/integration/cache_test.cpp
        tests/integration/ffi_test.cpp
    )
    target_link_libraries(tml_tests PRIVATE
        tml_cli
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(tml_tests)
endif()

# Install
install(TARGETS tml RUNTIME DESTINATION bin)
install(DIRECTORY include/tml DESTINATION include)
