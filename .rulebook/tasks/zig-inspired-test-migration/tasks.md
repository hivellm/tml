# Tasks: Zig-Inspired Test Migration

**Status**: Pending (0%) — All phases pending.

## 1. Stdlib Pre-Compiled Object Cache

- [ ] 1.1 Extract stdlib compilation from `compile_suite()` into standalone `compile_stdlib_objects()` function
- [ ] 1.2 Implement stdlib IR generation: single `QueryContext::codegen_unit()` pass for all core/std modules
- [ ] 1.3 Compile stdlib IR → `.obj` files (one per module or one monolithic)
- [ ] 1.4 Cache stdlib `.obj` with fingerprint (hash of all lib/core/src + lib/std/src source files)
- [ ] 1.5 Invalidate stdlib cache when any library source file changes
- [ ] 1.6 Modify `compile_suite()` to skip stdlib codegen — only codegen test file code, link against cached stdlib `.obj`
- [ ] 1.7 Handle conditional runtime objects (crypto, sqlite, zlib) — detect which suites need them, link selectively
- [ ] 1.8 Verify: all 1452 tests pass with pre-compiled stdlib
- [ ] 1.9 Benchmark: measure wall time reduction vs current system

## 2. Suite Aggregation (Mega-Binary)

- [ ] 2.1 Design mega-suite grouping: `core_all`, `std_all`, `compiler_all` (or finer: `core_str_all`, `core_iter_all`)
- [ ] 2.2 Detect and resolve symbol collisions between test files in same mega-suite
- [ ] 2.3 Implement mega-dispatcher IR generation: single dispatcher referencing all `tml_test_N` across files
- [ ] 2.4 Handle test index remapping: global index across all files in mega-suite
- [ ] 2.5 Compile all test `.obj` files + mega-dispatcher + stdlib `.obj` → single `.exe` per module group
- [ ] 2.6 Update coordinator to launch 3-5 mega-binaries instead of 206 suite binaries
- [ ] 2.7 Update cache system: suite-level cache → mega-suite-level cache with per-file invalidation
- [ ] 2.8 Handle compiler/tests separately (forced max_per_suite=1 due to symbol collisions)
- [ ] 2.9 Verify: all 1452 tests pass with mega-binary architecture
- [ ] 2.10 Benchmark: measure link time reduction (206 links → 3-5 links)

## 3. Incremental Object Cache

- [ ] 3.1 Implement per-file `.obj` cache keyed by IR fingerprint (already partially exists in testing_compile.cpp)
- [ ] 3.2 Promote existing `obj_cache/` from per-suite to global shared cache
- [ ] 3.3 On incremental run: hash each test file → check obj cache → skip LLVM backend for unchanged files
- [ ] 3.4 Incremental re-link: only re-link mega-binary when any constituent `.obj` changed
- [ ] 3.5 Implement dependency tracking: test file imports → if imported module changed, invalidate test `.obj`
- [ ] 3.6 Verify: change 1 test file → only that file recompiles, rest served from cache
- [ ] 3.7 Benchmark: incremental run time (target: <5s for single file change)

## 4. In-Process Execution Mode

- [ ] 4.1 Add `--in-process` CLI flag to TestConfig
- [ ] 4.2 Compile test mega-binary as shared library (.dll/.so) instead of .exe
- [ ] 4.3 Implement test DLL loader: dlopen/LoadLibrary → resolve `run_all_tests` symbol → call
- [ ] 4.4 Implement crash isolation: SEH handler (Windows) / sigaction+siglongjmp (Unix) per test
- [ ] 4.5 Implement output capture: redirect stdout/stderr per test for NDJSON-compatible reporting
- [ ] 4.6 Fallback: on crash, re-run remaining tests via subprocess mode
- [ ] 4.7 Verify: all 1452 tests pass in-process mode
- [ ] 4.8 Benchmark: measure subprocess overhead elimination

## 5. Unified Test Binary (Zig Model)

- [ ] 5.1 Compile ALL test files (1452) + stdlib into single compilation unit
- [ ] 5.2 Resolve all cross-file symbol collisions (module-scoped globals, duplicate type impls)
- [ ] 5.3 Generate unified dispatcher IR with 1452-entry test table
- [ ] 5.4 Single link step: all `.obj` + stdlib `.obj` + runtime → one `.exe`
- [ ] 5.5 Implement test selection: `--filter=core/str` runs subset without recompilation
- [ ] 5.6 Implement parallel test execution within single binary (thread pool + catch per test)
- [ ] 5.7 Implement crash-resilient execution: SEH/signal per test, skip crashed, continue remaining
- [ ] 5.8 Update cache: single binary cache invalidated by any source change, but obj cache still works per-file
- [ ] 5.9 Verify: all 1452 tests pass in unified binary
- [ ] 5.10 Benchmark: target <30s full suite, <5s incremental

## 6. Optional Native Backend (Future/Deferred)

- [ ] 6.1 Research: evaluate Cranelift, custom x86 emitter, or MIR interpreter for debug test builds
- [ ] 6.2 Implement lightweight IR → native code path (skip LLVM for test builds)
- [ ] 6.3 Integrate with unified test binary pipeline
- [ ] 6.4 Benchmark: target <5s full suite compile+run
