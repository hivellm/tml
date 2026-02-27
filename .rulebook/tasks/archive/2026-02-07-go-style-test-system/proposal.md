# Proposal: Go-Style Test System (PRIORITY: MAXIMUM)

## Problem

The current TML test system takes **~15 minutes** to compile and run 3,632 tests from scratch. This is unacceptable and blocks all development progress. The bottleneck is **99.8% compilation**, not execution.

### Root Cause Analysis

| Metric | Current TML | C++ (clang -O0) | Node.js | Go |
|--------|------------|-----------------|---------|-----|
| Compile equivalent zlib test | 8,600ms | 652ms | N/A | ~50ms |
| Execute equivalent zlib test | 9ms | 0.15ms | 4.3ms | ~1ms |
| Full suite (3,632 tests) | ~15 min | N/A | N/A | seconds (cached) |

The problem is **not** the zlib library. C++ compiles the same code 13x faster. The TML compiler regenerates **all imported library IR** for every test file, producing 10,000+ lines of LLVM IR per file even for simple tests.

### Why the Current Architecture Fails

1. **DLL-per-suite model**: Groups 8 test files into one DLL. Each file within a suite regenerates all library codegen independently (~8.6s per file for zlib tests).

2. **No effective IR sharing**: The shared library mechanism (`library_decls_only`) requires all files to have identical imports. The GlobalLibraryIRCache was disabled due to bugs (duplicate definitions, missing implementations).

3. **Monolithic library codegen**: `emit_module_pure_tml_functions()` re-parses, re-registers types, and re-generates IR for every imported module, for every test file. For zlib tests this means processing 163 modules each time.

4. **Suite grouping overhead**: Tests are grouped into suites of 8, compiled as DLLs, loaded via `LoadLibrary`. Each suite compilation is sequential. The DLL approach adds link time and management overhead.

## Solution: Go's Test Architecture

Go achieves sub-second cached test runs and ~30s full rebuilds for 350K LOC projects. The key principles:

### 1. Package-Level Compilation (Not File-Level)

**Go**: All test files in a directory compile into ONE test binary per package.
**TML current**: Each test file within a suite generates its own IR independently.
**TML new**: All test files in a directory compile into ONE test executable. Library IR is generated exactly ONCE per package, shared across all test functions.

### 2. Code Generation for Test Discovery (Not DLL Loading)

**Go**: A `_testmain.go` file is generated at compile time with static function pointers. The test binary is a plain executable.
**TML current**: Tests are compiled as DLLs, loaded with `LoadLibrary`, and test functions are discovered via `GetProcAddress`.
**TML new**: A `_testmain.tml` or equivalent LLVM IR stub is generated with direct function calls. The test binary is a plain `.exe`.

### 3. Content-Addressed Build Cache

**Go**: ActionID (hash of inputs) maps to OutputID (hash of outputs). Only recompiles when source/deps actually change.
**TML current**: Hash-based test result cache exists, but compilation is the bottleneck (DLLs are always regenerated on cache miss).
**TML new**: Cache the compiled test binary itself. Only recompile the package when source files or dependencies change.

### 4. Separate Process Per Package

**Go**: Each package test runs as a separate OS process. -p flag controls cross-package parallelism.
**TML current**: All tests run in-process via DLL loading.
**TML new**: Each package test binary is a separate process. Run N packages in parallel (default: CPU cores / 2).

## Architecture Changes

### New Compilation Pipeline

```
Current:
  discover files → group into suites of 8 → per-file:
    lex → parse → typecheck → borrow → codegen (8.6s each!) →
  per-suite: compile .ll → .obj → link DLL → LoadLibrary → GetProcAddress

New (Go model):
  discover files → group by directory (package) → per-package:
    lex+parse all files → typecheck all → borrow all →
    codegen ONCE (library IR + all test functions in one .ll) →
    compile .ll → .obj → link .exe → execute as subprocess
```

### Key Insight: ONE Codegen Pass Per Package

Instead of running codegen 8 times per suite (once per file), run it ONCE for the entire package:

1. Parse all test files in the directory
2. Type-check them all (shared TypeEnv)
3. Generate ONE LLVM IR file containing:
   - Library function implementations (generated once)
   - All test function bodies
   - A `main()` that dispatches based on --run-test=N
4. Compile and link into one .exe
5. Run the .exe, passing test indices to execute

This eliminates the #1 bottleneck: library IR regeneration per file.

### Expected Performance

| Phase | Current (zlib suite, 8 files) | New (single package) |
|-------|-------------------------------|----------------------|
| Library codegen | 8 x 8.6s = 68.8s | 1 x 8.6s = 8.6s |
| Test codegen | included above | ~0.5s (8 small functions) |
| Obj compile | 8 x 0.18s = 1.4s | 1 x 0.2s = 0.2s |
| Link | ~0.5s (DLL) | ~0.3s (EXE) |
| Total | ~71s | ~9.6s |
| **Speedup** | | **7.4x per suite** |

For the full test suite (47 suites), with cross-package parallelism:
- Current: ~15 minutes (sequential suite compilation)
- New: ~2-3 minutes (parallel package compilation, each package does codegen once)

With build cache (only recompile changed packages):
- Current: ~2s (DLL cache hit)
- New: ~0.5s (binary cache hit)

## Files to Modify

### Phase 1: Core Pipeline (EXE-per-package)

| File | Change |
|------|--------|
| `compiler/src/cli/tester/test_runner.cpp` | Replace DLL compilation with EXE compilation. One codegen pass per package. |
| `compiler/src/cli/tester/test_runner.hpp` | Update SuiteCompileResult to produce EXE path instead of DLL. |
| `compiler/src/cli/tester/suite_execution.cpp` | Replace DLL loading with subprocess execution. Parallel package execution. |
| `compiler/src/cli/tester/tester_internal.hpp` | Update TestSuite struct for package model. |
| `compiler/src/cli/tester/execution.cpp` | Replace `LoadLibrary`/`GetProcAddress` with subprocess launch. |
| `compiler/src/codegen/core/generate.cpp` | Support multi-function test binary (all test funcs in one module). |
| `compiler/src/codegen/core/runtime.cpp` | Generate test dispatcher `main()` instead of `DllMain`. |

### Phase 2: Test Discovery & Dispatch

| File | Change |
|------|--------|
| `compiler/src/cli/tester/discovery.cpp` | Group tests by directory (package). |
| `compiler/src/cli/tester/run.cpp` | Orchestrate package-level compilation and execution. |

### Phase 3: Binary Cache

| File | Change |
|------|--------|
| `compiler/src/cli/builder/build_cache.cpp` | Cache compiled test EXEs with content-addressed hashing. |
| `compiler/src/cli/tester/test_cache.hpp` | Extend cache to store binary paths and input hashes. |

### Phase 4: Cleanup

| File | Change |
|------|--------|
| `compiler/src/cli/tester/output.cpp` | Adapt output formatting for subprocess model. |
| `compiler/runtime/essential.c` | Remove DLL entry point code (DllMain). |

## Design Decisions

### Why EXE Instead of DLL

1. **Simpler**: No LoadLibrary/GetProcAddress, no symbol export/import
2. **Faster**: No DLL link overhead, no symbol resolution at runtime
3. **Portable**: Works identically on Windows/Linux/macOS
4. **Debuggable**: Standard executable, works with all debuggers
5. **Cacheable**: A single file to cache per package

### Why Subprocess Instead of In-Process

1. **Isolation**: A crash in one test package can't take down the test runner
2. **Parallelism**: OS-level process scheduling, no shared state
3. **Simplicity**: No global state management between test packages
4. **Go-proven**: This model handles millions of tests at Google scale

### Why NOT IR-Level Caching

The failed GlobalLibraryIRCache attempt proved that caching individual IR snippets is fragile:
- Duplicate function definitions across files
- Missing implementations when linking
- Complex thread-safety requirements
- State dependency between generated functions

The Go approach sidesteps this entirely: compile the package ONCE, cache the binary.

## Risk Assessment

### Low Risk
- Test output format: minor changes to parse subprocess output
- Test cache: extend existing hash-based cache
- Discovery: existing directory grouping works

### Medium Risk
- Single codegen pass with multiple test files: need to merge ASTs or process sequentially with shared codegen state
- Cross-platform subprocess execution: need proper pipe handling for stdout/stderr

### High Risk
- None identified. Go has proven this model at massive scale.

## Success Criteria

1. Full test suite completes in **< 3 minutes** from scratch (currently ~15 min)
2. Cached test suite completes in **< 5 seconds** (currently ~2s, should stay similar)
3. All 3,632 tests pass
4. No DLL-related code remains in test pipeline
5. Cross-package parallelism enabled by default
