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
option(TML_ENABLE_LSAN "Enable LeakSanitizer (Linux/macOS only)" OFF)
option(TML_ENABLE_MSAN "Enable MemorySanitizer (Linux Clang only)" OFF)

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
    # Sanitizer support (GCC/Clang)
    if(TML_ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address)
        message(STATUS "AddressSanitizer enabled")
    endif()
    if(TML_ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=undefined)
        message(STATUS "UndefinedBehaviorSanitizer enabled")
    endif()
    if(TML_ENABLE_LSAN AND NOT APPLE)
        # LeakSanitizer (standalone, Linux only - on macOS use ASan which includes LSan)
        add_compile_options(-fsanitize=leak)
        add_link_options(-fsanitize=leak)
        message(STATUS "LeakSanitizer enabled")
    endif()
    if(TML_ENABLE_MSAN AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # MemorySanitizer (Clang only, detects uninitialized reads)
        add_compile_options(-fsanitize=memory -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=memory)
        message(STATUS "MemorySanitizer enabled")
    endif()
elseif(MSVC)
    add_compile_options(/W4 /WX /utf-8 /FS)
    # Sanitizer support (MSVC)
    if(TML_ENABLE_ASAN)
        add_compile_options(/fsanitize=address)
        message(STATUS "AddressSanitizer enabled (MSVC)")
    endif()
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Preprocessor library (C-style conditional compilation)
add_library(tml_preprocessor STATIC
    src/preprocessor/preprocessor.cpp
)
target_include_directories(tml_preprocessor PUBLIC include src)

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
    src/parser/parser_oop.cpp
    src/parser/parser_expr.cpp
    src/parser/parser_stmt.cpp
    src/parser/parser_type.cpp
    src/parser/parser_pattern.cpp
)
target_link_libraries(tml_parser PUBLIC tml_lexer tml_preprocessor)
target_include_directories(tml_parser PUBLIC include src)

# Type checker library
add_library(tml_types STATIC
    src/types/type.cpp
    src/types/env_scope.cpp
    src/types/env_core.cpp
    src/types/env_definitions.cpp
    src/types/env_lookups.cpp
    src/types/module.cpp
    src/types/env_module_support.cpp
    # Checker modules (split from checker.cpp)
    src/types/checker/helpers.cpp
    src/types/checker/core.cpp
    src/types/checker/const_eval.cpp
    src/types/checker/expr.cpp
    src/types/checker/stmt.cpp
    src/types/checker/control.cpp
    src/types/checker/types.cpp
    src/types/checker/resolve.cpp
    # Builtin modules (moved from env_builtins*.cpp)
    src/types/builtins/register.cpp
    src/types/builtins/types.cpp
    src/types/builtins/io.cpp
    src/types/builtins/string.cpp
    src/types/builtins/time.cpp
    src/types/builtins/mem.cpp
    src/types/builtins/atomic.cpp
    src/types/builtins/sync.cpp
    src/types/builtins/math.cpp
    src/types/builtins/collections.cpp
    src/types/builtins/async.cpp
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
    src/borrow/checker_nll.cpp
)
target_link_libraries(tml_borrow PUBLIC tml_types)
target_include_directories(tml_borrow PUBLIC include src)

# IR library (high-level IR for serialization)
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

# HIR library (High-level IR for type-resolved, desugared AST)
add_library(tml_hir STATIC
    src/hir/hir_pattern.cpp
    src/hir/hir_expr.cpp
    src/hir/hir_stmt.cpp
    src/hir/hir_module.cpp
    src/hir/hir_printer.cpp
    src/hir/hir_builder.cpp
    src/hir/hir_builder_expr.cpp
    src/hir/hir_builder_stmt.cpp
    src/hir/hir_builder_pattern.cpp
    src/hir/hir_pass.cpp
    src/hir/hir_serialize.cpp
    # Serializer modules (split from hir_serialize.cpp for maintainability)
    src/hir/serializer/binary_writer.cpp
    src/hir/serializer/binary_reader.cpp
    src/hir/serializer/text_writer.cpp
    src/hir/serializer/text_reader.cpp
    src/hir/serializer/serialize_utils.cpp
)
target_link_libraries(tml_hir PUBLIC tml_borrow)
target_include_directories(tml_hir PUBLIC include src)

# MIR library (Mid-level IR in SSA form for optimization)
add_library(tml_mir STATIC
    src/mir/mir_type.cpp
    src/mir/mir_function.cpp
    src/mir/mir_printer.cpp
    src/mir/mir_builder.cpp
    src/mir/mir_serialize.cpp
    src/mir/mir_pass.cpp
    # Builder modules (split from mir_builder.cpp for maintainability)
    src/mir/builder/types.cpp
    src/mir/builder/expr.cpp
    src/mir/builder/stmt.cpp
    src/mir/builder/pattern.cpp
    src/mir/builder/control.cpp
    src/mir/builder/helpers.cpp
    # HIR to MIR builder (uses HIR instead of AST)
    src/mir/hir_mir_builder.cpp
    src/mir/builder/hir_expr.cpp
    src/mir/builder/hir_stmt.cpp
    src/mir/builder/hir_pattern.cpp
    # Serializer modules (split from mir_serialize.cpp for maintainability)
    src/mir/serializer/binary_writer.cpp
    src/mir/serializer/binary_reader.cpp
    src/mir/serializer/text_writer.cpp
    src/mir/serializer/text_reader.cpp
    src/mir/serializer/serialize_utils.cpp
    # Optimization passes
    src/mir/passes/constant_folding.cpp
    src/mir/passes/constant_propagation.cpp
    src/mir/passes/dead_code_elimination.cpp
    src/mir/passes/dead_function_elimination.cpp
    src/mir/passes/unreachable_code_elimination.cpp
    src/mir/passes/common_subexpression_elimination.cpp
    src/mir/passes/copy_propagation.cpp
    src/mir/passes/escape_analysis.cpp
    src/mir/passes/inlining.cpp
    src/mir/passes/async_lowering.cpp
    src/mir/passes/simplify_cfg.cpp
    src/mir/passes/inst_simplify.cpp
    src/mir/passes/jump_threading.cpp
    src/mir/passes/sroa.cpp
    src/mir/passes/gvn.cpp
    src/mir/passes/match_simplify.cpp
    src/mir/passes/mem2reg.cpp
    src/mir/passes/licm.cpp
    src/mir/passes/strength_reduction.cpp
    src/mir/passes/reassociate.cpp
    src/mir/passes/tail_call.cpp
    src/mir/passes/narrowing.cpp
    src/mir/passes/loop_unroll.cpp
    src/mir/passes/sinking.cpp
    src/mir/passes/adce.cpp
    src/mir/passes/peephole.cpp
    src/mir/passes/block_merge.cpp
    src/mir/passes/dead_arg_elim.cpp
    src/mir/passes/load_store_opt.cpp
    src/mir/passes/loop_rotate.cpp
    src/mir/passes/early_cse.cpp
    src/mir/passes/const_hoist.cpp
    src/mir/passes/simplify_select.cpp
    src/mir/passes/merge_returns.cpp
    src/mir/passes/devirtualization.cpp
    src/mir/passes/dead_method_elimination.cpp
)
target_link_libraries(tml_mir PUBLIC tml_hir tml_types)
target_include_directories(tml_mir PUBLIC include src)

# Codegen library (LLVM IR text generator + clang backend)
add_library(tml_codegen STATIC
    src/codegen/llvm_ir_gen_decl.cpp
    src/codegen/llvm_ir_gen_stmt.cpp
    src/codegen/llvm_ir_gen_expr.cpp
    src/codegen/llvm_ir_gen_builtins.cpp
    src/codegen/llvm_ir_gen_control.cpp
    src/codegen/c_header_gen.cpp
    src/codegen/mir_codegen.cpp
    # Core codegen (split from llvm_ir_gen.cpp for maintainability)
    src/codegen/core/utils.cpp
    src/codegen/core/types.cpp
    src/codegen/core/generic.cpp
    src/codegen/core/runtime.cpp
    src/codegen/core/dyn.cpp
    src/codegen/core/generate.cpp
    src/codegen/core/debug_info.cpp
    src/codegen/core/target.cpp
    src/codegen/core/drop.cpp
    src/codegen/core/class_codegen.cpp
    # Expression codegen (split for maintainability)
    src/codegen/expr/core.cpp
    src/codegen/expr/binary.cpp
    src/codegen/expr/unary.cpp
    src/codegen/expr/cast.cpp
    src/codegen/expr/closure.cpp
    src/codegen/expr/infer.cpp
    src/codegen/expr/struct.cpp
    src/codegen/expr/tuple.cpp
    src/codegen/expr/print.cpp
    src/codegen/expr/collections.cpp
    src/codegen/expr/method.cpp
    src/codegen/expr/method_static.cpp
    src/codegen/expr/method_primitive.cpp
    src/codegen/expr/method_collection.cpp
    src/codegen/expr/method_slice.cpp
    src/codegen/expr/method_maybe.cpp
    src/codegen/expr/method_outcome.cpp
    src/codegen/expr/method_array.cpp
    src/codegen/expr/await.cpp
    src/codegen/expr/try.cpp
    # Builtin function codegen (split for maintainability)
    src/codegen/builtins/io.cpp
    src/codegen/builtins/mem.cpp
    src/codegen/builtins/atomic.cpp
    src/codegen/builtins/sync.cpp
    src/codegen/builtins/time.cpp
    src/codegen/builtins/math.cpp
    src/codegen/builtins/collections.cpp
    src/codegen/builtins/string.cpp
    src/codegen/builtins/assert.cpp
    src/codegen/builtins/async.cpp
    src/codegen/builtins/intrinsics.cpp
)
target_link_libraries(tml_codegen PUBLIC tml_mir)
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

# JSON library (native JSON implementation)
add_library(tml_json STATIC
    src/json/json_value.cpp
    src/json/json_serializer.cpp
    src/json/json_parser.cpp
    src/json/json_builder.cpp
    src/json/json_rpc.cpp
    src/json/json_schema.cpp
)
target_include_directories(tml_json PUBLIC include src)

# Documentation library
add_library(tml_doc STATIC
    src/doc/doc_model.cpp
    src/doc/doc_parser.cpp
    src/doc/signature.cpp
    src/doc/extractor.cpp
    src/doc/generators.cpp
)
target_link_libraries(tml_doc PUBLIC tml_parser)
target_include_directories(tml_doc PUBLIC include src)

# CLI library
add_library(tml_cli STATIC
    # Core CLI files
    src/cli/cli.cpp
    src/cli/utils.cpp
    src/cli/diagnostic.cpp
    src/cli/dispatcher.cpp
    # Commands (cmd_*.cpp)
    src/cli/commands/cmd_build.cpp
    src/cli/commands/cmd_cache.cpp
    src/cli/commands/cmd_debug.cpp
    src/cli/commands/cmd_doc.cpp
    src/cli/commands/cmd_format.cpp
    src/cli/commands/cmd_init.cpp
    src/cli/commands/cmd_lint.cpp
    src/cli/commands/cmd_pkg.cpp
    src/cli/commands/cmd_rlib.cpp
    src/cli/commands/cmd_test.cpp
    # Builder modules
    src/cli/builder/build.cpp
    src/cli/builder/build_cache.cpp
    src/cli/builder/build_config.cpp
    src/cli/builder/compiler_setup.cpp
    src/cli/builder/dependency_resolver.cpp
    src/cli/builder/helpers.cpp
    src/cli/builder/object_compiler.cpp
    src/cli/builder/parallel_build.cpp
    src/cli/builder/rlib.cpp
    src/cli/builder/run.cpp
    src/cli/builder/run_profiled.cpp
    # Tester modules
    src/cli/tester/benchmark.cpp
    src/cli/tester/discovery.cpp
    src/cli/tester/execution.cpp
    src/cli/tester/fuzzer.cpp
    src/cli/tester/helpers.cpp
    src/cli/tester/output.cpp
    src/cli/tester/run.cpp
    src/cli/tester/suite_execution.cpp
    src/cli/tester/test_runner.cpp
    # Linter modules
    src/cli/linter/config.cpp
    src/cli/linter/discovery.cpp
    src/cli/linter/helpers.cpp
    src/cli/linter/run.cpp
    src/cli/linter/semantic.cpp
    src/cli/linter/style.cpp
)
target_link_libraries(tml_cli PUBLIC tml_codegen tml_format tml_doc)
target_include_directories(tml_cli PUBLIC include src)

# OpenSSL for SHA256 hashing (non-Windows)
if(NOT WIN32)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_link_libraries(tml_cli PUBLIC OpenSSL::Crypto)
    endif()
endif()

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
        tests/hir_test.cpp
        tests/mir_test.cpp
        tests/format_test.cpp
        tests/codegen_test.cpp
        tests/codegen_builtins_test.cpp
        tests/oop_test.cpp
        tests/json_test.cpp
        tests/cli/object_compiler_test.cpp
        tests/cli/parallel_build_test.cpp
        tests/integration/cache_test.cpp
        tests/integration/ffi_test.cpp
        tests/integration/memory_test.cpp
    )
    target_link_libraries(tml_tests PRIVATE
        tml_cli
        tml_json
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(tml_tests)
endif()

# Install
install(TARGETS tml RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
