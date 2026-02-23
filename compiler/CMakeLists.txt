cmake_minimum_required(VERSION 3.20)

# CMP0091: MSVC runtime library flags are selected by CMAKE_MSVC_RUNTIME_LIBRARY.
# Must be set before project() for proper effect.
cmake_policy(SET CMP0091 NEW)

# Declare LLVM option early so we can set CRT before project() targets are created.
option(TML_USE_LLVM_BACKEND "Use LLVM C API for self-contained compilation (no clang needed)" ON)

# When LLVM backend is enabled, use Release CRT (/MD) for ALL targets in ALL configs.
# LLVM static libs are built with /MD; mixing /MDd causes LNK2038 linker errors.
# Note: MSVC variable is not set before project(), so we check WIN32 via platform detection.
if(TML_USE_LLVM_BACKEND AND WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

project(tml LANGUAGES CXX C)

# Re-apply after project() and forcefully strip /MDd from debug flags.
# The VS multi-config generator injects /MDd for Debug configs, which conflicts
# with LLVM static libs built with /MD. We must force /MD for all configs.
if(MSVC AND TML_USE_LLVM_BACKEND)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    # Strip /MDd /MTd and add /MD to all configurations
    foreach(flag_var
        CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_C_FLAGS_MINSIZEREL)
        if(${flag_var})
            string(REPLACE "/MDd" "/MD" ${flag_var} "${${flag_var}}")
            string(REPLACE "/MTd" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()

# =============================================================================
# BUILD SCRIPT ENFORCEMENT
# =============================================================================
# This project MUST be built using scripts/build.bat (or scripts/build.sh).
# Direct cmake commands are NOT supported and will cause issues.
#
# The build script passes a special token that proves it's being used.
# If you're seeing this error, use: scripts\build.bat [debug|release]
# =============================================================================
if(NOT DEFINED TML_BUILD_TOKEN OR NOT TML_BUILD_TOKEN STREQUAL "tml_script_build_2026")
    message(FATAL_ERROR "
================================================================================
  ERROR: Direct cmake builds are NOT allowed!
================================================================================

  This project MUST be built using the build scripts:

    Windows:  scripts\\build.bat [debug|release] [--clean] [--no-tests]
    Linux:    scripts/build.sh [debug|release] [--clean] [--no-tests]

  DO NOT use cmake directly. The build scripts handle:
    - Correct output directory structure
    - Environment configuration
    - Cache management
    - Proper sequencing

  Examples:
    scripts\\build.bat              # Debug build
    scripts\\build.bat release      # Release build
    scripts\\build.bat --clean      # Clean rebuild

================================================================================
")
endif()

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
option(TML_BUILD_MODULAR "Build as modular plugins (DLLs) instead of monolithic exe" OFF)

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
    # /EHa enables asynchronous exception handling so __try/__except can catch
    # hardware exceptions (ACCESS_VIOLATION, etc.) across DLL boundaries.
    # Without /EHa, SEH only catches C++ exceptions (the default /EHsc mode).
    add_compile_options(/W4 /WX /utf-8 /FS /EHa)
    # Sanitizer support (MSVC)
    if(TML_ENABLE_ASAN)
        add_compile_options(/fsanitize=address)
        message(STATUS "AddressSanitizer enabled (MSVC)")
    endif()
endif()

# Force-include plugin/module.hpp in every .cpp so TML_MODULE("xxx") is always defined.
# This avoids requiring explicit #include in each source file.
set(_module_header "${CMAKE_CURRENT_SOURCE_DIR}/include/plugin/module.hpp")
if(MSVC)
    add_compile_options(/FI"${_module_header}")
else()
    add_compile_options(-include "${_module_header}")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)  # version_generated.hpp

# ============================================================================
# Version Management
# ============================================================================
# Version is passed from build.bat via -DTML_VERSION_MAJOR/MINOR/BUILD.
# The build script reads/increments the VERSION file at the project root.
if(NOT DEFINED TML_VERSION_MAJOR)
    set(TML_VERSION_MAJOR 0)
endif()
if(NOT DEFINED TML_VERSION_MINOR)
    set(TML_VERSION_MINOR 1)
endif()
if(NOT DEFINED TML_VERSION_BUILD)
    set(TML_VERSION_BUILD 0)
endif()

message(STATUS "TML version: ${TML_VERSION_MAJOR}.${TML_VERSION_MINOR}.${TML_VERSION_BUILD}")

# Generate version header into build dir (NOT source dir) to avoid full rebuilds.
# Only the 2 files that use VERSION include this header directly.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/version_generated.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/version_generated.hpp"
    @ONLY
)

# ============================================================================
# Pre-compiled C Runtime Library
# ============================================================================
# Build the TML runtime as a static library at compile time.
# This eliminates the need for clang at runtime to compile C files.

# Runtime source files
# Note: io.c is excluded - essential.c provides the same functions plus panic catching
set(TML_RUNTIME_SOURCES
    # core/
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/core/essential.c
    # memory/
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/memory/mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/memory/pool.c
    # text/
    # string.c deleted (Phase 31) — all functions migrated to inline LLVM IR or pure TML
    # text.c deleted (Phase 30) — Text migrated to pure TML (Phase 22)
    # collections/
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/collections/collections.c
    # math/
    # math.c deleted (Phase 32) — all functions migrated to inline LLVM IR
    # time/
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/time/time.c
    # concurrency/
    # thread.c deleted (Phase 30) — replaced by sync.c (Phase 24)
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/concurrency/sync.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/concurrency/async.c
    # net/
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/net/net.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/net/dns.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/net/tls.c
    # os/
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/os/os.c
    # crypto/
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/crypto/crypto.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/crypto/crypto_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/crypto/crypto_x509.c
    # diagnostics/
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/diagnostics/backtrace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/diagnostics/log.c
    # search/ — search.c removed in Phase 35 (migrated to pure TML in distance.tml)
)

# Create static library
add_library(tml_runtime STATIC ${TML_RUNTIME_SOURCES})

# C compiler flags for runtime (use generator expressions for build-type-specific flags)
if(MSVC)
    # Debug: no optimization (avoid /O2 vs /RTC1 conflict)
    # Release: full optimization
    # /std:c11 and /experimental:c11atomics kept for compatibility
    target_compile_options(tml_runtime PRIVATE
        /W3
        /std:c11
        /experimental:c11atomics
        $<$<CONFIG:Debug>:/Od>
        $<$<NOT:$<CONFIG:Debug>>:/O2>
    )
    target_compile_definitions(tml_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(tml_runtime PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<NOT:$<CONFIG:Debug>>:-O3 -fomit-frame-pointer -funroll-loops>
    )
    if(WIN32)
        # MinGW: enable MSVC extensions for SEH support
        target_compile_options(tml_runtime PRIVATE -fms-extensions)
    endif()
endif()

# Put runtime library in output directory (alongside tml.exe)
set_target_properties(tml_runtime PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${TML_OUTPUT_DIR}
)
foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(tml_runtime PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${TML_OUTPUT_DIR}
    )
endforeach()

message(STATUS "TML Runtime: Building static library (tml_runtime)")

# OpenSSL support for crypto runtime modules
# Search order: vcpkg_installed (project-local), standalone install (Windows), system (Unix/Mac)
set(TML_OPENSSL_FOUND FALSE)

# 1. Check vcpkg_installed (cross-platform)
if(WIN32)
    set(VCPKG_OPENSSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-windows")
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(VCPKG_OPENSSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/arm64-osx")
    else()
        set(VCPKG_OPENSSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-osx")
    endif()
else()
    set(VCPKG_OPENSSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-linux")
endif()

if(EXISTS "${VCPKG_OPENSSL_DIR}/include/openssl/evp.h")
    message(STATUS "OpenSSL: Found via vcpkg at ${VCPKG_OPENSSL_DIR}")
    target_include_directories(tml_runtime PRIVATE "${VCPKG_OPENSSL_DIR}/include")
    target_include_directories(tml_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime/crypto")
    target_compile_definitions(tml_runtime PRIVATE TML_HAS_OPENSSL=1)
    if(WIN32)
        target_link_libraries(tml_runtime PUBLIC
            "${VCPKG_OPENSSL_DIR}/lib/libcrypto.lib"
            "${VCPKG_OPENSSL_DIR}/lib/libssl.lib"
            crypt32 ws2_32
        )
    else()
        target_link_libraries(tml_runtime PUBLIC
            "${VCPKG_OPENSSL_DIR}/lib/libcrypto.a"
            "${VCPKG_OPENSSL_DIR}/lib/libssl.a"
        )
        if(NOT APPLE)
            target_link_libraries(tml_runtime PUBLIC dl pthread)
        endif()
    endif()
    set(TML_OPENSSL_FOUND TRUE)
endif()

# 2. Windows standalone install (C:/Program Files/OpenSSL-Win64)
if(NOT TML_OPENSSL_FOUND AND WIN32)
    set(OPENSSL_ROOT "C:/Program Files/OpenSSL-Win64")
    if(EXISTS "${OPENSSL_ROOT}/include/openssl/evp.h")
        message(STATUS "OpenSSL: Found at ${OPENSSL_ROOT}")
        target_include_directories(tml_runtime PRIVATE "${OPENSSL_ROOT}/include")
        target_include_directories(tml_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime/crypto")
        target_compile_definitions(tml_runtime PRIVATE TML_HAS_OPENSSL=1)
        target_link_libraries(tml_runtime PUBLIC
            "${OPENSSL_ROOT}/lib/libcrypto_static.lib"
            "${OPENSSL_ROOT}/lib/libssl_static.lib"
            crypt32 ws2_32
        )
        set(TML_OPENSSL_FOUND TRUE)
    endif()
endif()

# 3. System OpenSSL (Linux/macOS - pkg-config or find_package)
if(NOT TML_OPENSSL_FOUND AND NOT WIN32)
    find_package(OpenSSL QUIET)
    if(OPENSSL_FOUND)
        message(STATUS "OpenSSL: Found system OpenSSL ${OPENSSL_VERSION}")
        target_include_directories(tml_runtime PRIVATE ${OPENSSL_INCLUDE_DIR})
        target_include_directories(tml_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime/crypto")
        target_compile_definitions(tml_runtime PRIVATE TML_HAS_OPENSSL=1)
        target_link_libraries(tml_runtime PUBLIC OpenSSL::Crypto OpenSSL::SSL)
        if(NOT APPLE)
            target_link_libraries(tml_runtime PUBLIC dl pthread)
        endif()
        set(TML_OPENSSL_FOUND TRUE)
    endif()
endif()

# Fallback: no OpenSSL
if(NOT TML_OPENSSL_FOUND)
    message(STATUS "OpenSSL: Not found, crypto_x509 will use stub implementations")
    target_include_directories(tml_runtime PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/runtime/crypto")
endif()

# Common utilities library
add_library(tml_common STATIC
    src/common/crc32c.cpp
)
target_include_directories(tml_common PUBLIC include src)

# Logging library (structured logging with levels, sinks, filters)
add_library(tml_log STATIC
    src/log/logger.cpp
    src/log/log_init.cpp
)
target_include_directories(tml_log PUBLIC include src)

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
    src/parser/parser_decl_impl.cpp
    src/parser/parser_oop.cpp
    src/parser/parser_expr.cpp
    src/parser/parser_expr_complex.cpp
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
    src/types/env_module_loading.cpp
    # Checker modules (split from checker.cpp)
    src/types/checker/helpers.cpp
    src/types/checker/core.cpp
    src/types/checker/core_oop.cpp
    src/types/checker/const_eval.cpp
    src/types/checker/expr.cpp
    src/types/checker/stmt.cpp
    src/types/checker/control.cpp
    src/types/checker/types.cpp
    src/types/checker/resolve.cpp
    src/types/checker/decl_struct.cpp
    src/types/checker/expr_call.cpp
    src/types/checker/expr_call_method.cpp
    # Builtin modules (moved from env_builtins*.cpp)
    src/types/builtins/register.cpp
    src/types/builtins/types.cpp
    src/types/builtins/io.cpp
    # src/types/builtins/string.cpp  — removed Phase 29 (dead stub)
    # src/types/builtins/time.cpp    — removed Phase 39 (dead stub)
    src/types/builtins/mem.cpp
    src/types/builtins/atomic.cpp
    src/types/builtins/sync.cpp
    src/types/builtins/math.cpp
    src/types/builtins/async.cpp
    # Caching
    src/types/builtins_cache.cpp
    src/types/module_binary.cpp
    src/types/module_binary_read.cpp
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
    src/borrow/polonius_facts.cpp
    src/borrow/polonius_solver.cpp
    src/borrow/polonius_checker.cpp
)
target_link_libraries(tml_borrow PUBLIC tml_types)
target_include_directories(tml_borrow PUBLIC include src)

# Trait solver library (goal-based behavior resolution)
add_library(tml_traits STATIC
    src/traits/solver.cpp
    src/traits/solver_builtins.cpp
)
target_link_libraries(tml_traits PUBLIC tml_types)
target_include_directories(tml_traits PUBLIC include src)

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
    src/hir/hir_pass_inline.cpp
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

# THIR library (Typed High-level IR with explicit coercions, resolved dispatch)
add_library(tml_thir STATIC
    src/thir/thir_module.cpp
    src/thir/thir_lower.cpp
    src/thir/exhaustiveness.cpp
)
target_link_libraries(tml_thir PUBLIC tml_hir tml_traits)
target_include_directories(tml_thir PUBLIC include src)

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
    src/mir/builder/hir_expr_control.cpp
    src/mir/builder/hir_stmt.cpp
    src/mir/builder/hir_pattern.cpp
    # THIR to MIR builder (uses THIR with explicit coercions)
    src/mir/thir_mir_builder.cpp
    src/mir/thir_mir_builder_expr.cpp
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
    src/mir/passes/builder_opt.cpp
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
    src/mir/passes/constructor_fusion.cpp
    src/mir/passes/destructor_hoist.cpp
    src/mir/passes/batch_destruction.cpp
    src/mir/passes/infinite_loop_check.cpp
    src/mir/passes/memory_leak_check.cpp
    src/mir/passes/bounds_check_elimination.cpp
    src/mir/passes/rvo.cpp
    src/mir/passes/alias_analysis.cpp
    src/mir/passes/ipo.cpp
    src/mir/passes/loop_opts.cpp
    src/mir/passes/pgo.cpp
    src/mir/passes/vectorization.cpp
)
target_link_libraries(tml_mir PUBLIC tml_hir tml_thir tml_types)
target_include_directories(tml_mir PUBLIC include src)

# Codegen library (LLVM IR text generator + clang backend)
add_library(tml_codegen STATIC
    # Backend abstraction layer (backend-agnostic)
    src/codegen/codegen_backend.cpp
    src/codegen/c_header_gen.cpp
    src/codegen/codegen_partitioner.cpp
    # MIR codegen (MIR → LLVM IR text, backend-agnostic interface)
    src/codegen/mir_codegen.cpp
    src/codegen/mir/helpers.cpp
    src/codegen/mir/types.cpp
    src/codegen/mir/terminators.cpp
    src/codegen/mir/instructions.cpp
    src/codegen/mir/instructions_method.cpp
    src/codegen/mir/instructions_misc.cpp
    # LLVM backend implementation
    src/codegen/llvm/llvm_codegen_backend.cpp
    src/codegen/llvm/llvm_ir_gen_stmt.cpp
    src/codegen/llvm/llvm_ir_gen_expr.cpp
    # LLVM declaration codegen
    src/codegen/llvm/decl/struct.cpp
    src/codegen/llvm/decl/enum.cpp
    src/codegen/llvm/decl/func.cpp
    src/codegen/llvm/decl/impl.cpp
    # LLVM derive macro codegen
    src/codegen/llvm/derive/reflect.cpp
    src/codegen/llvm/derive/partial_eq.cpp
    src/codegen/llvm/derive/duplicate.cpp
    src/codegen/llvm/derive/hash.cpp
    src/codegen/llvm/derive/default.cpp
    src/codegen/llvm/derive/partial_ord.cpp
    src/codegen/llvm/derive/debug.cpp
    src/codegen/llvm/derive/display.cpp
    src/codegen/llvm/derive/serialize.cpp
    src/codegen/llvm/derive/deserialize.cpp
    src/codegen/llvm/derive/fromstr.cpp
    # LLVM control flow codegen
    src/codegen/llvm/control/if.cpp
    src/codegen/llvm/control/loop.cpp
    src/codegen/llvm/control/return.cpp
    src/codegen/llvm/control/when.cpp
    # LLVM core codegen
    src/codegen/llvm/core/utils.cpp
    src/codegen/llvm/core/types.cpp
    src/codegen/llvm/core/types_resolve.cpp
    src/codegen/llvm/core/generic.cpp
    src/codegen/llvm/core/runtime.cpp
    src/codegen/llvm/core/runtime_modules.cpp
    src/codegen/llvm/core/dyn.cpp
    src/codegen/llvm/core/generate.cpp
    src/codegen/llvm/core/generate_cache.cpp
    src/codegen/llvm/core/generate_support.cpp
    src/codegen/llvm/core/debug_info.cpp
    src/codegen/llvm/core/target.cpp
    src/codegen/llvm/core/drop.cpp
    src/codegen/llvm/core/class_codegen.cpp
    src/codegen/llvm/core/class_codegen_generic.cpp
    src/codegen/llvm/core/class_codegen_virtual.cpp
    src/codegen/llvm/core/optimization_passes.cpp
    # LLVM expression codegen
    src/codegen/llvm/expr/core.cpp
    src/codegen/llvm/expr/call.cpp
    src/codegen/llvm/expr/call_generic_struct.cpp
    src/codegen/llvm/expr/call_user.cpp
    src/codegen/llvm/expr/binary.cpp
    src/codegen/llvm/expr/binary_ops.cpp
    src/codegen/llvm/expr/unary.cpp
    src/codegen/llvm/expr/cast.cpp
    src/codegen/llvm/expr/closure.cpp
    src/codegen/llvm/expr/infer.cpp
    src/codegen/llvm/expr/infer_methods.cpp
    src/codegen/llvm/expr/struct.cpp
    src/codegen/llvm/expr/struct_field.cpp
    src/codegen/llvm/expr/tuple.cpp
    src/codegen/llvm/expr/print.cpp
    src/codegen/llvm/expr/collections.cpp
    src/codegen/llvm/expr/method.cpp
    src/codegen/llvm/expr/method_static_dispatch.cpp
    src/codegen/llvm/expr/method_generic.cpp
    src/codegen/llvm/expr/method_static.cpp
    src/codegen/llvm/expr/method_primitive.cpp
    src/codegen/llvm/expr/method_primitive_ext.cpp
    src/codegen/llvm/expr/method_collection.cpp
    src/codegen/llvm/expr/method_slice.cpp
    src/codegen/llvm/expr/method_maybe.cpp
    src/codegen/llvm/expr/method_outcome.cpp
    src/codegen/llvm/expr/method_array.cpp
    src/codegen/llvm/expr/method_impl.cpp
    src/codegen/llvm/expr/method_dyn.cpp
    src/codegen/llvm/expr/method_class.cpp
    src/codegen/llvm/expr/method_prim_behavior.cpp
    src/codegen/llvm/expr/await.cpp
    src/codegen/llvm/expr/try.cpp
    # LLVM builtin function codegen
    src/codegen/llvm/builtins/io.cpp
    src/codegen/llvm/builtins/mem.cpp
    src/codegen/llvm/builtins/atomic.cpp
    src/codegen/llvm/builtins/sync.cpp
    # src/codegen/llvm/builtins/time.cpp  — removed Phase 25 (dead stub)
    src/codegen/llvm/builtins/math.cpp
    src/codegen/llvm/builtins/string.cpp
    src/codegen/llvm/builtins/assert.cpp
    src/codegen/llvm/builtins/async.cpp
    src/codegen/llvm/builtins/intrinsics.cpp
    src/codegen/llvm/builtins/intrinsics_extended.cpp
)
target_link_libraries(tml_codegen PUBLIC tml_mir)
target_include_directories(tml_codegen PUBLIC include src)

# Query system library (demand-driven compilation with memoization)
add_library(tml_query STATIC
    src/query/query_fingerprint.cpp
    src/query/query_cache.cpp
    src/query/query_deps.cpp
    src/query/query_provider.cpp
    src/query/query_context.cpp
    src/query/query_core.cpp
    src/query/query_incr.cpp
)
target_link_libraries(tml_query PUBLIC tml_codegen tml_thir tml_traits tml_common tml_log)
target_include_directories(tml_query PUBLIC include src)

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
    src/json/json_fast_parser.cpp
    src/json/json_builder.cpp
    src/json/json_rpc.cpp
    src/json/json_schema.cpp
    src/json/json_allocator.cpp
)
target_include_directories(tml_json PUBLIC include src)
# Put JSON library in output directory (needed by TML programs using std::json)
set_target_properties(tml_json PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${TML_OUTPUT_DIR}
)
foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(tml_json PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${TML_OUTPUT_DIR}
    )
endforeach()

# JSON Runtime library (C linkage for TML programs)
# This provides the native JSON parser functions that TML's std::json module calls
add_library(tml_json_runtime STATIC
    src/json/json_runtime.cpp
)
target_link_libraries(tml_json_runtime PUBLIC tml_json)
target_include_directories(tml_json_runtime PUBLIC include src)
if(MSVC)
    target_compile_options(tml_json_runtime PRIVATE
        /W3
        $<$<CONFIG:Debug>:/Od>
        $<$<NOT:$<CONFIG:Debug>>:/O2>
    )
else()
    target_compile_options(tml_json_runtime PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<NOT:$<CONFIG:Debug>>:-O3 -fomit-frame-pointer>
    )
endif()
# Put JSON runtime library in output directory (alongside tml.exe)
set_target_properties(tml_json_runtime PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${TML_OUTPUT_DIR}
)
foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(tml_json_runtime PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${TML_OUTPUT_DIR}
    )
endforeach()
message(STATUS "TML JSON Runtime: Building static library (tml_json_runtime)")

# Search Runtime library (C++ linkage for TML programs)
# Provides BM25, HNSW, TF-IDF, and F32 SIMD distance functions via extern "C" FFI
add_library(tml_search_runtime STATIC
    runtime/search/search_engine.cpp
)
target_link_libraries(tml_search_runtime PUBLIC tml_search)
target_include_directories(tml_search_runtime PUBLIC include src)
if(MSVC)
    target_compile_options(tml_search_runtime PRIVATE
        /W3
        $<$<CONFIG:Debug>:/Od>
        $<$<NOT:$<CONFIG:Debug>>:/O2>
    )
else()
    target_compile_options(tml_search_runtime PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<NOT:$<CONFIG:Debug>>:-O3 -fomit-frame-pointer>
    )
endif()
set_target_properties(tml_search_runtime PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${TML_OUTPUT_DIR}
)
foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(tml_search_runtime PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${TML_OUTPUT_DIR}
    )
endforeach()
message(STATUS "TML Search Runtime: Building static library (tml_search_runtime)")

# ============================================================================
# Zlib/Compression Runtime Library (optional)
# ============================================================================
# Provides compression support for std::zlib module (zlib, brotli, zstd)

option(TML_BUILD_ZLIB_RUNTIME "Build zlib compression runtime" ON)

if(TML_BUILD_ZLIB_RUNTIME)
    # Find compression libraries
    # Try find_package first, then manual detection for vcpkg
    find_package(ZLIB QUIET)

    # Local vcpkg install path (project-local installation in src/)
    set(VCPKG_LOCAL_PATH "${CMAKE_SOURCE_DIR}/../src/x64-windows")

    # Manual zlib detection if find_package failed
    if(NOT ZLIB_FOUND)
        find_path(ZLIB_INCLUDE_DIR zlib.h
            PATHS
            "$ENV{ZLIB_DIR}/include"
            "${VCPKG_LOCAL_PATH}/include"
            "C:/vcpkg/installed/x64-windows/include"
            "/usr/include"
            "/usr/local/include"
        )
        find_library(ZLIB_LIBRARY NAMES zlib z zlibstatic zlibd zlib1
            PATHS
            "$ENV{ZLIB_DIR}/lib"
            "${VCPKG_LOCAL_PATH}/lib"
            "C:/vcpkg/installed/x64-windows/lib"
            "/usr/lib"
            "/usr/local/lib"
        )
        if(ZLIB_INCLUDE_DIR AND ZLIB_LIBRARY)
            set(ZLIB_FOUND TRUE)
            set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
            set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
            message(STATUS "Zlib Runtime: zlib found manually at ${ZLIB_LIBRARY}")
        endif()
    endif()

    # Try to find Brotli
    find_path(BROTLI_INCLUDE_DIR brotli/encode.h
        PATHS
        "$ENV{BROTLI_DIR}/include"
        "${VCPKG_LOCAL_PATH}/include"
        "C:/vcpkg/installed/x64-windows/include"
        "/usr/include"
        "/usr/local/include"
    )
    find_library(BROTLI_COMMON_LIB NAMES brotlicommon brotlicommon-static
        PATHS
        "$ENV{BROTLI_DIR}/lib"
        "${VCPKG_LOCAL_PATH}/lib"
        "C:/vcpkg/installed/x64-windows/lib"
        "/usr/lib"
        "/usr/local/lib"
    )
    find_library(BROTLI_ENC_LIB NAMES brotlienc brotlienc-static
        PATHS
        "$ENV{BROTLI_DIR}/lib"
        "${VCPKG_LOCAL_PATH}/lib"
        "C:/vcpkg/installed/x64-windows/lib"
        "/usr/lib"
        "/usr/local/lib"
    )
    find_library(BROTLI_DEC_LIB NAMES brotlidec brotlidec-static
        PATHS
        "$ENV{BROTLI_DIR}/lib"
        "${VCPKG_LOCAL_PATH}/lib"
        "C:/vcpkg/installed/x64-windows/lib"
        "/usr/lib"
        "/usr/local/lib"
    )

    # Try to find Zstd
    find_path(ZSTD_INCLUDE_DIR zstd.h
        PATHS
        "$ENV{ZSTD_DIR}/include"
        "${VCPKG_LOCAL_PATH}/include"
        "C:/vcpkg/installed/x64-windows/include"
        "/usr/include"
        "/usr/local/include"
    )
    find_library(ZSTD_LIB NAMES zstd zstd_static libzstd
        PATHS
        "$ENV{ZSTD_DIR}/lib"
        "${VCPKG_LOCAL_PATH}/lib"
        "C:/vcpkg/installed/x64-windows/lib"
        "/usr/lib"
        "/usr/local/lib"
    )

    # Zlib runtime sources
    set(TML_ZLIB_RUNTIME_SOURCES
        ${CMAKE_SOURCE_DIR}/../lib/std/runtime/zlib/zlib_deflate.c
        ${CMAKE_SOURCE_DIR}/../lib/std/runtime/zlib/zlib_exports.c
    )

    # Add optional sources based on available libraries
    set(TML_ZLIB_HAVE_BROTLI OFF)
    set(TML_ZLIB_HAVE_ZSTD OFF)

    if(BROTLI_INCLUDE_DIR AND BROTLI_COMMON_LIB AND BROTLI_ENC_LIB AND BROTLI_DEC_LIB)
        set(TML_ZLIB_HAVE_BROTLI ON)
        list(APPEND TML_ZLIB_RUNTIME_SOURCES
            ${CMAKE_SOURCE_DIR}/../lib/std/runtime/zlib/zlib_brotli.c
        )
        message(STATUS "Zlib Runtime: Brotli support enabled")
    else()
        message(STATUS "Zlib Runtime: Brotli not found (brotli compression disabled)")
    endif()

    if(ZSTD_INCLUDE_DIR AND ZSTD_LIB)
        set(TML_ZLIB_HAVE_ZSTD ON)
        list(APPEND TML_ZLIB_RUNTIME_SOURCES
            ${CMAKE_SOURCE_DIR}/../lib/std/runtime/zlib/zlib_zstd.c
        )
        message(STATUS "Zlib Runtime: Zstd support enabled")
    else()
        message(STATUS "Zlib Runtime: Zstd not found (zstd compression disabled)")
    endif()

    if(ZLIB_FOUND)
        # Create static library
        add_library(tml_zlib_runtime STATIC ${TML_ZLIB_RUNTIME_SOURCES})

        # Include directories
        target_include_directories(tml_zlib_runtime PRIVATE
            ${CMAKE_SOURCE_DIR}/../lib/std/runtime/zlib
            ${ZLIB_INCLUDE_DIRS}
        )

        # Link zlib
        target_link_libraries(tml_zlib_runtime PUBLIC ${ZLIB_LIBRARIES})

        # Add Brotli if available
        if(TML_ZLIB_HAVE_BROTLI)
            target_include_directories(tml_zlib_runtime PRIVATE ${BROTLI_INCLUDE_DIR})
            target_link_libraries(tml_zlib_runtime PUBLIC
                ${BROTLI_ENC_LIB}
                ${BROTLI_DEC_LIB}
                ${BROTLI_COMMON_LIB}
            )
            target_compile_definitions(tml_zlib_runtime PRIVATE TML_HAS_BROTLI=1)
        endif()

        # Add Zstd if available
        if(TML_ZLIB_HAVE_ZSTD)
            target_include_directories(tml_zlib_runtime PRIVATE ${ZSTD_INCLUDE_DIR})
            target_link_libraries(tml_zlib_runtime PUBLIC ${ZSTD_LIB})
            target_compile_definitions(tml_zlib_runtime PRIVATE TML_HAS_ZSTD=1)
        endif()

        # C compiler flags
        if(MSVC)
            target_compile_options(tml_zlib_runtime PRIVATE
                /W3
                $<$<CONFIG:Debug>:/Od>
                $<$<NOT:$<CONFIG:Debug>>:/O2>
            )
            target_compile_definitions(tml_zlib_runtime PRIVATE _CRT_SECURE_NO_WARNINGS)
        else()
            target_compile_options(tml_zlib_runtime PRIVATE
                $<$<CONFIG:Debug>:-O0 -g>
                $<$<NOT:$<CONFIG:Debug>>:-O3 -fomit-frame-pointer>
            )
        endif()

        # Put library in output directory
        set_target_properties(tml_zlib_runtime PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY ${TML_OUTPUT_DIR}
        )
        foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            set_target_properties(tml_zlib_runtime PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${TML_OUTPUT_DIR}
            )
        endforeach()

        message(STATUS "TML Zlib Runtime: Building static library (tml_zlib_runtime)")
    else()
        message(STATUS "TML Zlib Runtime: zlib not found - compression support disabled")
    endif()
endif()

# Profiler library (runtime profiling with cpuprofile export)
add_library(tml_profiler STATIC
    src/profiler/profiler.cpp
)
target_include_directories(tml_profiler PUBLIC include src)
target_link_libraries(tml_profiler PUBLIC tml_log)
if(MSVC)
    target_compile_options(tml_profiler PRIVATE
        /W3
        $<$<CONFIG:Debug>:/Od>
        $<$<NOT:$<CONFIG:Debug>>:/O2>
    )
else()
    target_compile_options(tml_profiler PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<NOT:$<CONFIG:Debug>>:-O3 -fomit-frame-pointer>
    )
endif()
# Put profiler library in output directory (alongside tml.exe)
set_target_properties(tml_profiler PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${TML_OUTPUT_DIR}
)
foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(tml_profiler PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE} ${TML_OUTPUT_DIR}
    )
endforeach()
message(STATUS "TML Profiler: Building static library (tml_profiler)")

# Documentation library
add_library(tml_doc STATIC
    src/doc/doc_model.cpp
    src/doc/doc_parser.cpp
    src/doc/signature.cpp
    src/doc/extractor.cpp
    src/doc/generators.cpp
    src/doc/generators_html.cpp
)
target_link_libraries(tml_doc PUBLIC tml_parser)
target_include_directories(tml_doc PUBLIC include src)

# Search index library (BM25 + HNSW vector search)
add_library(tml_search STATIC
    src/search/simd_distance.cpp
    src/search/bm25_index.cpp
    src/search/hnsw_index.cpp
)
target_include_directories(tml_search PUBLIC include src)

# MCP library (Model Context Protocol server)
# Named tml_mcp_lib to avoid conflict with tml_mcp executable
add_library(tml_mcp_lib STATIC
    src/mcp/mcp_types.cpp
    src/mcp/mcp_server.cpp
    src/mcp/mcp_tools.cpp
    src/mcp/mcp_tools_docs.cpp
    src/mcp/mcp_tools_docs_handlers.cpp
    src/mcp/mcp_tools_project.cpp
)
target_link_libraries(tml_mcp_lib PUBLIC tml_json tml_codegen tml_search)
target_include_directories(tml_mcp_lib PUBLIC include src)

# ============================================================================
# LLVM Backend (enabled by default for self-contained compilation)
# option(TML_USE_LLVM_BACKEND ...) is declared at the top of this file
# ============================================================================
if(TML_USE_LLVM_BACKEND)
    # Find LLVM package
    find_package(LLVM CONFIG QUIET)

    if(NOT LLVM_FOUND)
        # Search paths for LLVM headers (C API headers in source tree)
        set(LLVM_HEADER_PATHS
            "${CMAKE_SOURCE_DIR}/../src/llvm-project/llvm/include"  # Source headers (llvm-c/Core.h etc.)
            "${CMAKE_SOURCE_DIR}/../src/llvm-install/include"
            "$ENV{LLVM_DIR}/include"
            "$ENV{LLVM_ROOT}/include"
            "C:/Program Files/LLVM/include"
            "F:/LLVM/include"
            "/usr/lib/llvm-18/include"
            "/usr/lib/llvm-17/include"
            "/usr/local/opt/llvm/include"
        )

        # Search paths for generated LLVM headers (llvm/Config/llvm-config.h etc.)
        set(LLVM_GENERATED_HEADER_PATHS
            "${CMAKE_SOURCE_DIR}/../build/llvm/include"             # Generated by LLVM build
            "${CMAKE_SOURCE_DIR}/../src/llvm-install/include"
        )

        # Search paths for LLVM static libraries
        set(LLVM_LIB_PATHS
            "${CMAKE_SOURCE_DIR}/../build/llvm/Release/lib"         # Local build output
            "${CMAKE_SOURCE_DIR}/../src/llvm-install/lib"
            "$ENV{LLVM_DIR}/lib"
            "$ENV{LLVM_ROOT}/lib"
            "C:/Program Files/LLVM/lib"
            "F:/LLVM/lib"
            "/usr/lib/llvm-18/lib"
            "/usr/lib/llvm-17/lib"
            "/usr/local/opt/llvm/lib"
        )

        # Find source headers (llvm-c/Core.h)
        foreach(HEADER_PATH ${LLVM_HEADER_PATHS})
            if(EXISTS "${HEADER_PATH}/llvm-c/Core.h")
                set(LLVM_INCLUDE_DIRS "${HEADER_PATH}")
                message(STATUS "Found LLVM headers at: ${HEADER_PATH}")
                break()
            endif()
        endforeach()

        # Find generated headers (llvm/Config/llvm-config.h)
        set(LLVM_GENERATED_INCLUDE_DIR "")
        foreach(GEN_PATH ${LLVM_GENERATED_HEADER_PATHS})
            if(EXISTS "${GEN_PATH}/llvm/Config/llvm-config.h")
                set(LLVM_GENERATED_INCLUDE_DIR "${GEN_PATH}")
                message(STATUS "Found LLVM generated headers at: ${GEN_PATH}")
                break()
            endif()
        endforeach()

        # Find libraries (check for LLVMCore.lib as indicator of static libs)
        foreach(LIB_PATH ${LLVM_LIB_PATHS})
            if(WIN32)
                if(EXISTS "${LIB_PATH}/LLVMCore.lib")
                    set(LLVM_LIBRARY_DIRS "${LIB_PATH}")
                    message(STATUS "Found LLVM static libraries at: ${LIB_PATH}")
                    set(LLVM_FOUND TRUE)
                    break()
                elseif(EXISTS "${LIB_PATH}/LLVM-C.lib")
                    set(LLVM_LIBRARY_DIRS "${LIB_PATH}")
                    message(STATUS "Found LLVM-C library at: ${LIB_PATH}")
                    set(LLVM_FOUND TRUE)
                    break()
                endif()
            else()
                if(EXISTS "${LIB_PATH}/libLLVMCore.a")
                    set(LLVM_LIBRARY_DIRS "${LIB_PATH}")
                    message(STATUS "Found LLVM static libraries at: ${LIB_PATH}")
                    set(LLVM_FOUND TRUE)
                    break()
                elseif(EXISTS "${LIB_PATH}/libLLVM.so" OR EXISTS "${LIB_PATH}/libLLVM.a")
                    set(LLVM_LIBRARY_DIRS "${LIB_PATH}")
                    message(STATUS "Found LLVM library at: ${LIB_PATH}")
                    set(LLVM_FOUND TRUE)
                    break()
                endif()
            endif()
        endforeach()
    endif()

    if(LLVM_FOUND OR EXISTS "${LLVM_INCLUDE_DIRS}/llvm-c/Core.h")
        message(STATUS "LLVM Backend: Enabled")
        message(STATUS "  Headers:   ${LLVM_INCLUDE_DIRS}")
        if(LLVM_GENERATED_INCLUDE_DIR)
            message(STATUS "  Generated: ${LLVM_GENERATED_INCLUDE_DIR}")
        endif()
        message(STATUS "  Libraries: ${LLVM_LIBRARY_DIRS}")

        # LLVM Backend library
        add_library(tml_backend STATIC
            src/backend/llvm_backend.cpp
        )

        # Include paths: source headers + generated headers
        set(LLVM_ALL_INCLUDE_DIRS ${LLVM_INCLUDE_DIRS})
        if(LLVM_GENERATED_INCLUDE_DIR)
            list(APPEND LLVM_ALL_INCLUDE_DIRS ${LLVM_GENERATED_INCLUDE_DIR})
        endif()
        target_include_directories(tml_backend PUBLIC include src ${LLVM_ALL_INCLUDE_DIRS})

        if(LLVM_LIBRARY_DIRS)
            target_link_directories(tml_backend PUBLIC ${LLVM_LIBRARY_DIRS})

            # Check if individual static libraries are available (preferred)
            if(WIN32)
                set(HAS_STATIC_LIBS FALSE)
                if(EXISTS "${LLVM_LIBRARY_DIRS}/LLVMCore.lib")
                    set(HAS_STATIC_LIBS TRUE)
                endif()
            else()
                set(HAS_STATIC_LIBS FALSE)
                if(EXISTS "${LLVM_LIBRARY_DIRS}/libLLVMCore.a")
                    set(HAS_STATIC_LIBS TRUE)
                endif()
            endif()

            if(HAS_STATIC_LIBS)
                # Link individual static LLVM libraries (no clang DLL needed)
                # Complete dependency list with all transitive deps
                target_link_libraries(tml_backend PUBLIC
                    # X86 target
                    LLVMX86CodeGen LLVMX86AsmParser LLVMX86Desc LLVMX86Disassembler LLVMX86Info LLVMX86TargetMCA
                    # AArch64 target (for cross-compilation support)
                    LLVMAArch64CodeGen LLVMAArch64AsmParser LLVMAArch64Desc LLVMAArch64Disassembler LLVMAArch64Info LLVMAArch64Utils
                    # Optimization passes
                    LLVMPasses LLVMCoroutines LLVMipo LLVMVectorize LLVMSandboxIR
                    LLVMAggressiveInstCombine LLVMInstCombine LLVMScalarOpts
                    LLVMObjCARCOpts LLVMInstrumentation LLVMCFGuard
                    # Code generation
                    LLVMCodeGen LLVMCGData LLVMGlobalISel LLVMSelectionDAG LLVMAsmPrinter
                    # Debug info (required by AsmPrinter and CodeGen)
                    LLVMDebugInfoDWARF LLVMDebugInfoDWARFLowLevel LLVMDebugInfoCodeView LLVMDebugInfoMSF LLVMDebugInfoPDB
                    # Profiling and instrumentation data
                    LLVMProfileData LLVMCoverage
                    # Frontend libraries (required by ipo/OpenMP passes)
                    LLVMFrontendOpenMP LLVMFrontendAtomic LLVMFrontendOffloading LLVMFrontendDriver
                    LLVMFrontendHLSL LLVMObjectYAML LLVMHipStdPar
                    # Core
                    LLVMLinker LLVMTransformUtils LLVMAnalysis
                    LLVMIRReader LLVMIRPrinter LLVMAsmParser LLVMBitWriter LLVMBitReader
                    LLVMTarget LLVMCodeGenTypes
                    LLVMCore LLVMRemarks LLVMBitstreamReader
                    # Low-level
                    LLVMObject LLVMMCParser LLVMMCDisassembler LLVMMC
                    LLVMBinaryFormat LLVMTargetParser LLVMTextAPI
                    LLVMSupport LLVMDemangle
                )
                message(STATUS "  Linking: individual static LLVM libraries")

                # LLVM static libs are built in Release mode (/MD, _ITERATOR_DEBUG_LEVEL=0).
                # When TML builds in Debug (/MDd, _ITERATOR_DEBUG_LEVEL=2), this causes
                # LNK2038 mismatch errors. Fix: compile tml_backend with Release CRT
                # to match the LLVM libraries. This is safe because tml_backend only
                # calls the LLVM C API (opaque handles, no STL across boundary).
                # Note: All targets use /MD (Release CRT) when LLVM backend is enabled.
                # See CMAKE_MSVC_RUNTIME_LIBRARY setting at the top of this file.
            else()
                # Fallback: link single shared LLVM library
                if(WIN32)
                    target_link_libraries(tml_backend PUBLIC LLVM-C)
                else()
                    target_link_libraries(tml_backend PUBLIC LLVM)
                endif()
                message(STATUS "  Linking: shared LLVM library")
            endif()

            # Windows system libraries required by LLVM
            if(WIN32)
                target_link_libraries(tml_backend PUBLIC
                    ntdll advapi32 ole32 shell32 uuid
                )
            endif()
        else()
            message(STATUS "  Note: Headers found but no pre-compiled libraries")
            message(STATUS "        Build LLVM first or set LLVM_DIR")
        endif()

        target_compile_definitions(tml_backend PUBLIC TML_HAS_LLVM_BACKEND=1)

        set(TML_BACKEND_LIB tml_backend)
    else()
        message(WARNING "LLVM not found - backend disabled. Build LLVM from src/llvm-project or set LLVM_DIR.")
        set(TML_BACKEND_LIB "")
    endif()
else()
    message(STATUS "LLVM Backend: Disabled (use -DTML_USE_LLVM_BACKEND=ON to enable)")
    set(TML_BACKEND_LIB "")
endif()

# ============================================================================
# LLD/Zig Linker Support (optional, for self-contained linking)
# ============================================================================
option(TML_USE_LLD "Use LLD for self-contained linking" OFF)
option(TML_USE_ZIG_LINKER "Use Zig's linker for optimized linking" OFF)

# LLD Linker library (always built, uses external lld-link.exe or ld.lld)
add_library(tml_lld STATIC
    src/backend/lld_linker.cpp
)
target_include_directories(tml_lld PUBLIC include src)

# Embed LLD libraries for in-process linking (no subprocess needed)
if(TML_USE_LLVM_BACKEND AND LLVM_FOUND)
    # LLD source headers (lld/Common/Driver.h)
    set(LLD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../src/llvm-project/lld/include")
    # LLD generated headers (lld/Common/Version.inc)
    set(LLD_GENERATED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../build/llvm/tools/lld/include")

    if(EXISTS "${LLD_INCLUDE_DIR}/lld/Common/Driver.h")
        # Use SYSTEM to suppress warnings from LLVM/LLD headers (C4245, C4458 etc.)
        target_include_directories(tml_lld SYSTEM PUBLIC
            ${LLD_INCLUDE_DIR} ${LLD_GENERATED_INCLUDE_DIR} ${LLVM_ALL_INCLUDE_DIRS})
        target_link_directories(tml_lld PUBLIC ${LLVM_LIBRARY_DIRS})

        target_link_libraries(tml_lld PUBLIC
            # LLD flavor libraries
            lldCOFF lldCommon lldELF lldMachO lldMinGW lldWasm
            # LLVM libraries required by LLD (transitive dependencies)
            LLVMLTO LLVMDTLTO LLVMOption LLVMLibDriver LLVMPlugins
            LLVMWindowsManifest LLVMWindowsDriver
            # Core deps shared with tml_backend (needed for link resolution)
            LLVMPasses LLVMipo LLVMBitWriter LLVMLinker LLVMTransformUtils
            LLVMCodeGen LLVMTarget LLVMAnalysis LLVMObject LLVMCore
            LLVMIRReader LLVMAsmParser LLVMBitReader LLVMProfileData
            LLVMMC LLVMMCParser LLVMMCDisassembler LLVMBinaryFormat
            LLVMDebugInfoDWARF LLVMDebugInfoCodeView LLVMDebugInfoMSF LLVMDebugInfoPDB
            LLVMSupport LLVMDemangle LLVMTargetParser LLVMRemarks
        )
        target_compile_definitions(tml_lld PUBLIC TML_HAS_LLD_EMBEDDED=1)
        message(STATUS "LLD Embedded: Enabled (in-process linking)")
    else()
        message(STATUS "LLD Embedded: Disabled (headers not found at ${LLD_INCLUDE_DIR})")
    endif()
endif()

if(TML_USE_ZIG_LINKER)
    # Search for Zig installation
    find_program(ZIG_EXECUTABLE zig
        HINTS
        "$ENV{ZIG_DIR}"
        "C:/Program Files/zig"
        "C:/zig"
        "/usr/local/bin"
        "/usr/bin"
    )

    if(ZIG_EXECUTABLE)
        message(STATUS "Zig Linker: Found at ${ZIG_EXECUTABLE}")
        target_compile_definitions(tml_lld PRIVATE TML_USE_ZIG_LINKER=1)
        target_compile_definitions(tml_lld PRIVATE TML_ZIG_PATH="${ZIG_EXECUTABLE}")
    else()
        message(WARNING "Zig not found - falling back to LLD")
    endif()
endif()

if(TML_USE_LLD)
    # Search for LLD (priority: local build > system)
    set(LLD_SEARCH_PATHS
        "${CMAKE_SOURCE_DIR}/../src/llvm-install/bin"  # Local build
        "${CMAKE_SOURCE_DIR}/../src/llvm-project/lld"
        "F:/LLVM/bin"
        "C:/Program Files/LLVM/bin"
        "$ENV{LLVM_DIR}/bin"
        "/usr/bin"
        "/usr/local/bin"
    )

    foreach(LLD_PATH ${LLD_SEARCH_PATHS})
        if(WIN32)
            if(EXISTS "${LLD_PATH}/lld-link.exe")
                message(STATUS "LLD: Found at ${LLD_PATH}")
                target_compile_definitions(tml_lld PRIVATE TML_USE_LLD=1)
                break()
            endif()
        else()
            if(EXISTS "${LLD_PATH}/ld.lld")
                message(STATUS "LLD: Found at ${LLD_PATH}")
                target_compile_definitions(tml_lld PRIVATE TML_USE_LLD=1)
                break()
            endif()
        endif()
    endforeach()
endif()

# ============================================================================
# Cranelift Backend (optional, for faster debug builds)
# ============================================================================
option(TML_USE_CRANELIFT_BACKEND "Use Cranelift backend for fast debug compilation" OFF)

if(TML_USE_CRANELIFT_BACKEND)
    # Path to the pre-built Cranelift bridge static library
    set(CRANELIFT_LIB_DIR "${CMAKE_SOURCE_DIR}/../build/cranelift/release")

    if(WIN32)
        set(CRANELIFT_LIB_PATH "${CRANELIFT_LIB_DIR}/tml_cranelift_bridge.lib")
    else()
        set(CRANELIFT_LIB_PATH "${CRANELIFT_LIB_DIR}/libtml_cranelift_bridge.a")
    endif()

    if(EXISTS "${CRANELIFT_LIB_PATH}")
        message(STATUS "Cranelift Backend: Enabled")
        message(STATUS "  Library: ${CRANELIFT_LIB_PATH}")

        # Add Cranelift wrapper source to tml_codegen
        target_sources(tml_codegen PRIVATE
            src/codegen/cranelift/cranelift_codegen_backend.cpp
        )
        target_compile_definitions(tml_codegen PUBLIC TML_HAS_CRANELIFT_BACKEND=1)

        # Link the Rust static library
        target_link_libraries(tml_codegen PUBLIC "${CRANELIFT_LIB_PATH}")

        # Windows system libraries required by Rust static libraries
        if(WIN32)
            target_link_libraries(tml_codegen PUBLIC
                ws2_32 userenv bcrypt ntdll advapi32
            )
        else()
            target_link_libraries(tml_codegen PUBLIC dl pthread m)
        endif()
    else()
        message(WARNING "Cranelift library not found at ${CRANELIFT_LIB_PATH}")
        message(WARNING "Build it first: cd compiler/cranelift && cargo build --release")
        message(WARNING "Cranelift backend disabled.")
    endif()
else()
    message(STATUS "Cranelift Backend: Disabled (use -DTML_USE_CRANELIFT_BACKEND=ON to enable)")
endif()

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
    # Explain modules
    src/cli/explain/run.cpp
    src/cli/explain/lexer_errors.cpp
    src/cli/explain/parser_errors.cpp
    src/cli/explain/type_errors.cpp
    src/cli/explain/borrow_errors.cpp
    src/cli/explain/codegen_errors.cpp
    src/cli/explain/general_errors.cpp
    src/cli/explain/preproc_errors.cpp
    src/cli/commands/cmd_format.cpp
    src/cli/commands/cmd_init.cpp
    src/cli/commands/cmd_lint.cpp
    src/cli/commands/cmd_mcp.cpp
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
    src/cli/tester/coverage.cpp
    src/cli/tester/diagnostic_execution.cpp
    src/cli/tester/discovery.cpp
    src/cli/tester/execution.cpp
    src/cli/tester/fuzzer.cpp
    src/cli/tester/helpers.cpp
    src/cli/tester/output.cpp
    src/cli/tester/run.cpp
    src/cli/tester/suite_execution.cpp
    src/cli/tester/test_runner.cpp
    src/cli/tester/test_runner_exec.cpp
    src/cli/tester/test_runner_single.cpp
    src/cli/tester/test_cache.cpp
    src/cli/tester/library_coverage.cpp
    src/cli/tester/library_coverage_report.cpp
    # EXE-based test runner (v2 / Go-style)
    src/cli/tester/exe_test_runner.cpp
    src/cli/tester/exe_dispatcher_gen.cpp
    src/cli/tester/exe_test_execution.cpp
    src/cli/tester/exe_suite_runner.cpp
    src/cli/commands/cmd_test_v2.cpp
    # Linter modules
    src/cli/linter/config.cpp
    src/cli/linter/discovery.cpp
    src/cli/linter/helpers.cpp
    src/cli/linter/run.cpp
    src/cli/linter/semantic.cpp
    src/cli/linter/style.cpp
)
target_link_libraries(tml_cli PUBLIC tml_codegen tml_query tml_format tml_doc tml_mcp_lib tml_lld tml_common tml_log ${TML_BACKEND_LIB})
target_include_directories(tml_cli PUBLIC include src)

# OpenSSL for SHA256 hashing (non-Windows)
if(NOT WIN32)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_link_libraries(tml_cli PUBLIC OpenSSL::Crypto)
    endif()
endif()

# ============================================================================
# Modular Plugin Build
# ============================================================================
if(TML_BUILD_MODULAR)
    include(cmake/collect_modules.cmake)
    collect_module_sources("${CMAKE_CURRENT_SOURCE_DIR}/src" MOD)

    # Plugin loader library (shared between launcher and any host that loads plugins)
    add_library(tml_plugin_loader STATIC
        src/plugin/loader.cpp
        third_party/zstd/zstddeclib.c
    )
    target_include_directories(tml_plugin_loader PUBLIC include src)
    target_include_directories(tml_plugin_loader PRIVATE third_party/zstd)
    set_source_files_properties(third_party/zstd/zstddeclib.c PROPERTIES
        LANGUAGE C
        COMPILE_FLAGS "$<$<C_COMPILER_ID:MSVC>:/W0>$<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>"
    )

    # ================================================================
    # Phase 2: tml_codegen_x86 plugin DLL
    # ================================================================
    # This SHARED library contains LLVM codegen + LLD linker + plugin entry points.
    # It links against the heavy LLVM/LLD static libraries so the main executable
    # does NOT need them when the codegen is loaded as a plugin.
    if(TML_USE_LLVM_BACKEND AND LLVM_FOUND)
        add_library(tml_codegen_x86_plugin SHARED
            src/plugin/llvm_plugin.cpp
        )
        target_include_directories(tml_codegen_x86_plugin PRIVATE include src)
        target_compile_definitions(tml_codegen_x86_plugin PRIVATE PLUGIN_BUILDING)
        target_link_libraries(tml_codegen_x86_plugin PRIVATE
            tml_codegen tml_lld ${TML_BACKEND_LIB} tml_log tml_common
        )
        set_target_properties(tml_codegen_x86_plugin PROPERTIES
            OUTPUT_NAME "tml_codegen_x86"
            RUNTIME_OUTPUT_DIRECTORY "${TML_OUTPUT_DIR}/plugins"
            LIBRARY_OUTPUT_DIRECTORY "${TML_OUTPUT_DIR}/plugins"
        )
        # Flatten multi-config output dirs
        foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            set_target_properties(tml_codegen_x86_plugin PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE} "${TML_OUTPUT_DIR}/plugins"
                LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE} "${TML_OUTPUT_DIR}/plugins"
            )
        endforeach()
        message(STATUS "Plugin: tml_codegen_x86 (SHARED)")
    endif()

    # ================================================================
    # Phase 3: tml_compiler plugin DLL (future)
    # ================================================================
    # The compiler plugin wraps the full frontend (lex, parse, check, build, etc.)
    # and exports compiler_main(argc, argv) for the thin launcher.
    # Currently not used as the main exe — the monolithic build serves both modes.
    add_library(tml_compiler_plugin SHARED
        src/plugin/compiler_plugin.cpp
    )
    target_include_directories(tml_compiler_plugin PRIVATE include src)
    target_compile_definitions(tml_compiler_plugin PRIVATE PLUGIN_BUILDING)
    target_link_libraries(tml_compiler_plugin PRIVATE
        tml_cli tml_plugin_loader
    )
    set_target_properties(tml_compiler_plugin PROPERTIES
        OUTPUT_NAME "tml_compiler"
        RUNTIME_OUTPUT_DIRECTORY "${TML_OUTPUT_DIR}/plugins"
        LIBRARY_OUTPUT_DIRECTORY "${TML_OUTPUT_DIR}/plugins"
    )
    foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(tml_compiler_plugin PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE} "${TML_OUTPUT_DIR}/plugins"
            LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE} "${TML_OUTPUT_DIR}/plugins"
        )
    endforeach()
    message(STATUS "Plugin: tml_compiler (SHARED)")

    # ================================================================
    # Phase 4: Auxiliary plugin DLLs
    # ================================================================
    # Helper macro to create a plugin SHARED target with standard settings.
    macro(add_tml_plugin PLUGIN_NAME PLUGIN_SOURCE PLUGIN_DEPS)
        add_library(tml_${PLUGIN_NAME}_plugin SHARED ${PLUGIN_SOURCE})
        target_include_directories(tml_${PLUGIN_NAME}_plugin PRIVATE include src)
        target_compile_definitions(tml_${PLUGIN_NAME}_plugin PRIVATE PLUGIN_BUILDING)
        target_link_libraries(tml_${PLUGIN_NAME}_plugin PRIVATE ${PLUGIN_DEPS})
        set_target_properties(tml_${PLUGIN_NAME}_plugin PROPERTIES
            OUTPUT_NAME "tml_${PLUGIN_NAME}"
            RUNTIME_OUTPUT_DIRECTORY "${TML_OUTPUT_DIR}/plugins"
            LIBRARY_OUTPUT_DIRECTORY "${TML_OUTPUT_DIR}/plugins"
        )
        foreach(CONFIG_TYPE DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            set_target_properties(tml_${PLUGIN_NAME}_plugin PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE} "${TML_OUTPUT_DIR}/plugins"
                LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE} "${TML_OUTPUT_DIR}/plugins"
            )
        endforeach()
        message(STATUS "Plugin: tml_${PLUGIN_NAME} (SHARED)")
    endmacro()

    # Tools plugin: formatter, linter, doc generator, search
    add_tml_plugin(tools
        src/plugin/tools_plugin.cpp
        "tml_format;tml_doc;tml_search;tml_log;tml_common"
    )

    # Test plugin: test runner, coverage, benchmark, fuzzer
    add_tml_plugin(test
        src/plugin/test_plugin.cpp
        "tml_cli;tml_log;tml_common"
    )

    # MCP plugin: Model Context Protocol server
    add_tml_plugin(mcp
        src/plugin/mcp_plugin.cpp
        "tml_mcp_lib;tml_json;tml_log;tml_common"
    )

    # ================================================================
    # Thin launcher — THIS is tml.exe in modular mode
    # ================================================================
    # 367KB binary that loads plugins on demand.
    # Links only the plugin loader — no compiler, no LLVM.
    add_executable(tml src/launcher/main_launcher.cpp)
    target_include_directories(tml PRIVATE include src)
    target_link_libraries(tml PRIVATE tml_plugin_loader)
    message(STATUS "Launcher: tml.exe (thin, ~400KB)")

    # Monolithic fallback — tml_full.exe for debugging/compatibility
    add_executable(tml_full src/main.cpp)
    target_link_libraries(tml_full PRIVATE tml_cli tml_plugin_loader)
    set_target_properties(tml_full PROPERTIES
        OUTPUT_NAME "tml_full"
    )
else()
    # Monolithic build — unchanged from before
    add_executable(tml src/main.cpp)
    target_link_libraries(tml PRIVATE tml_cli)
endif()

# Increase stack size on Windows to handle deep recursion during test execution
# Default is 1MB, increase to 32MB for running large test suites with coverage.
if(MSVC)
    if(TML_BUILD_MODULAR)
        target_link_options(tml_full PRIVATE /STACK:33554432)
    else()
        target_link_options(tml PRIVATE /STACK:33554432)
    endif()
endif()

# MCP server executable (separate from main tml to avoid file locking issues)
# When MCP server is running, tml.exe can still be rebuilt independently
add_executable(tml_mcp src/main_mcp.cpp)
target_link_libraries(tml_mcp PRIVATE tml_mcp_lib tml_codegen tml_doc tml_search tml_log)
target_include_directories(tml_mcp PRIVATE include src)

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

    # Disable -Werror for GoogleTest (it has warnings we can't control)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(gtest PRIVATE -Wno-error)
        target_compile_options(gtest_main PRIVATE -Wno-error)
    endif()

    # Test executable
    add_executable(tml_tests
        # Frontend
        tests/frontend/lexer_test.cpp
        tests/frontend/parser_test.cpp
        tests/frontend/types_test.cpp
        tests/frontend/borrow_test.cpp
        tests/frontend/format_test.cpp
        # IR
        tests/ir/ir_test.cpp
        tests/ir/hir_test.cpp
        tests/ir/mir_test.cpp
        # Codegen
        tests/codegen/codegen_test.cpp
        tests/codegen/codegen_builtins_test.cpp
        tests/codegen/text_test.cpp
        tests/codegen/oop_test.cpp
        # Analysis
        tests/analysis/escape_analysis_test.cpp
        tests/analysis/devirtualization_test.cpp
        # Foundational infrastructure
        tests/foundational/query_fingerprint_test.cpp
        tests/foundational/query_deps_test.cpp
        tests/foundational/query_cache_test.cpp
        tests/foundational/query_system_test.cpp
        tests/foundational/incremental_cache_test.cpp
        tests/foundational/codegen_partitioner_test.cpp
        tests/foundational/mir_codegen_cgu_test.cpp
        tests/foundational/llvm_backend_test.cpp
        tests/foundational/lld_linker_test.cpp
        tests/foundational/codegen_backend_test.cpp
        # CLI
        tests/cli/object_compiler_test.cpp
        tests/cli/parallel_build_test.cpp
        # Integration
        tests/integration/cache_test.cpp
        tests/integration/ffi_test.cpp
        tests/integration/memory_test.cpp
        tests/integration/json_test.cpp
        tests/integration/log_test.cpp
        # Search
        tests/search/simd_distance_test.cpp
        tests/search/bm25_index_test.cpp
        tests/search/hnsw_index_test.cpp
        tests/search/search_benchmark_test.cpp
    )
    target_link_libraries(tml_tests PRIVATE
        tml_cli
        tml_json
        tml_search
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(tml_tests)
endif()

# Install
install(TARGETS tml RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
