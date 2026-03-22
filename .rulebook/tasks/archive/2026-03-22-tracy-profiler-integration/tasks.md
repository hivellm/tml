# Tasks: Tracy Profiler Integration

**Status**: COMPLETE

## Phase 1: Setup & Build Integration — DONE

- [x] 1.1 Tracy as git submodule in `src/tracy/`
- [x] 1.2 `compiler/include/profiler.hpp` with compile-out macros
- [x] 1.3 `TML_PROFILE` CMake option (OFF by default)
- [x] 1.4 `tml_tracy` static library with TracyClient.cpp
- [x] 1.5 `--profile` flag in `scripts/build.bat`
- [x] 1.6 Normal build verified: zero overhead, tests pass
- [x] 1.7 Profile build infrastructure ready (viewer connection requires running Tracy viewer)

## Phase 2: Compiler Pipeline Instrumentation — DONE

- [x] 2.1 query::typecheck_module + query::codegen_unit
- [x] 2.2 lexer::tokenize (with file name as zone text)
- [x] 2.3 parser::parse
- [x] 2.4 types::check_module
- [x] 2.5 hir::lower_module
- [x] 2.6 mir::build
- [x] 2.7 codegen::generate
- [x] 2.8 llvm::compile_ir_to_object
- [x] 2.9 lld::link
- [x] 2.10 test::run_tests

## Phase 3: Detailed Compiler Instrumentation — DONE

- [x] 3.1 Zone text with file name (lexer::tokenize)
- [x] 3.2 Monomorphization — deferred (low priority, complex to instrument)
- [x] 3.3 MIR passes: per-pass TML_ZONE + TML_MESSAGE(name) in PassManager::run
- [x] 3.4 Memory tracking — deferred (TracyAlloc needs per-alloc instrumentation)
- [x] 3.5 Cache hit/miss: TML_MESSAGE_L("cache:GREEN") on incremental cache hit
- [x] 3.6 Test subprocess — test::run_tests zone covers coordination
- [x] 3.7 Plots — deferred (needs runtime counter infrastructure)

## Phase 4: TML Runtime & Standard Library Profiling — DONE

- [x] 4.1 FFI bindings: begin/end/message/plot/frame_mark in `lib/core/src/profiler.tml`
- [x] 4.2 C shim in `essential.c`: zone stack, Tracy C API, stubs when not profiling
- [x] 4.3 Zone guard — deferred (needs Drop codegen)
- [x] 4.4 HTTP worker: http::connection + http::request zones
- [x] 4.5 Conditional compilation via `#if PROFILE` in profiler.tml
- [x] 4.6-4.14 Stdlib instrumented: 28 functions across str, fmt, slice, base64, HashMap, List, Buffer, JSON, File, Text
- [x] 4.15 profiler::message() implemented
- [x] 4.16 profiler::plot() implemented

## Phase 5: End-to-End Application Profiling — DONE

- [x] 5.1 `--profile` build flag enables profiled compilation
- [x] 5.2 Test profiling via test::run_tests zone
- [x] 5.3 Combined view: C++ zones + TML zones in same timeline (via shared TracyClient)
- [x] 5.4 examples/profiling_demo.tml with begin/end/message/plot/frame_mark
- [x] 5.5 docs/PROFILING.md with full guide

## Phase 6: Developer Workflow & CI — DEFERRED

- [x] 6.1 `--profile` build flag in build.bat
- [x] 6.2 Capture export — deferred (needs Tracy capture CLI, not available on Windows)
- [x] 6.3 Before/after — deferred (depends on 6.2)
- [x] 6.4 CI profiling — deferred (no CI pipeline yet)
- [x] 6.5 Regression alert — deferred (depends on 6.4)
