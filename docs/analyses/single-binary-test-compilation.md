# Single Binary Test Compilation — Architecture Analysis

**Date**: 2026-03-14
**Status**: Approved for implementation
**Impact**: ~10x reduction in test compilation time

## Problem Statement

TML compiles **~206 test suites** into separate executables, each requiring:
1. LLVM codegen per file (~15-20s)
2. Dispatcher IR generation + compilation
3. Linking against runtime archive + conditional libs
4. Subprocess execution per test (1 process per test)

Total: **~10 minutes** for full suite with coverage. The bottleneck is **206 separate LLVM codegen + link cycles**, not individual test execution.

## How Other Languages Do It

| Language | Strategy | Binaries | Link Steps | Test Isolation | Discovery |
|----------|----------|----------|------------|----------------|-----------|
| **Go** | 1 binary per package | ~1 per package | 1 | Goroutines (in-process) | `func Test*` naming |
| **Zig** | 1 binary per `zig test` invocation | 1 | 1 | None (in-process, crash kills all) | `test "name" { }` comptime |
| **Rust** | 1 binary per crate | 1 per crate | 1 per crate | Threads + `catch_unwind` | `#[test]` attribute |
| **Crystal** | 1 binary for all specs | 1 | 1 | In-process | `it "name" { }` macro |
| **D** | 1 binary per module | 1 per module | 1 per module | In-process | `unittest { }` block |
| **Nim** | 1 binary per file | N | N | Separate processes | `test "name":` macro |
| **Swift** | 1 binary per target | 1 per target | 1 per target | XCTest in-process | `func test*()` |
| **C++** | Framework-dependent | Varies | Varies | Framework-dependent | Framework macros |
| **TML (current)** | 1 binary per suite (8 files) | **~206** | **~206** | **Subprocess per test** | File pattern `*.test.tml` |

### Key Insight

All high-performance compiled languages (Go, Rust, Zig, Crystal) produce **1 binary** (or 1 per large module group) and run tests **in-process**. TML produces 206 binaries and runs each test as a separate subprocess.

## Proposed Architecture: Aggregated Single Binary

### Overview

Merge all test `.obj` files + a single aggregated dispatcher into **one executable**. The coordinator launches this binary with `--run-all` or `--test-index=N` to run tests. Crash isolation is preserved via subprocess-per-test execution (existing model).

### Phase 1: Use `--run-all` Mode (Quick Win)

**Current state**: Each suite binary supports `--run-all` but the coordinator uses `--test-index=N` (1 subprocess per test).

**Change**: For each compiled suite, launch ONE subprocess with `--run-all` instead of N subprocesses with `--test-index=N`. The dispatcher already handles sequential execution with NDJSON events per test.

**Impact**: Reduces subprocess count from ~1452 to ~206. No compilation changes needed.

**Trade-off**: Crash in one test kills remaining tests in that suite. Acceptable because:
- Crashes are rare (<0.1% of tests)
- Suite grouping already batches 8 files together
- Can fall back to per-test mode for crashed suites (retry logic)

### Phase 2: Merge Suites into Mega-Binary

**Change**: Instead of 206 separate compilations + links, merge all `.obj` files into a single binary with an aggregated dispatcher.

**Implementation**:
1. Compile all test files individually (LLVM codegen — parallelizable, already cached)
2. Generate ONE mega-dispatcher IR that references ALL `tml_test_N` functions
3. Link everything into ONE `.exe` (~1 link step instead of 206)
4. Run with `--run-all` for full suite or `--test-index=N` for specific tests

**Impact**: Eliminates 205 redundant link steps. Each link takes ~1-2s → saves ~200-400s.

**Risk**: Symbol collisions between test files (e.g., duplicate `main` or module-level globals). Mitigation: test files already use unique `tml_test_N` entry points and module-scoped globals.

### Phase 3: Parallel In-Process Execution (Future)

Replace subprocess-per-test with thread-per-test (like Rust's libtest). Each test runs in a thread with `catch_unwind`-style crash isolation.

**Not recommended yet**: Requires TML to support thread-level panic isolation, which doesn't exist.

## Implementation Plan (Phase 1)

### Files to Modify

1. **`compiler/src/testing/testing_coordinator.cpp`** (~30 lines)
   - Change subprocess launch from `--test-index=N` to `--run-all`
   - Parse NDJSON stream for all tests in one subprocess
   - On crash (non-zero exit without proper NDJSON), retry failed tests individually with `--test-index=N`

2. **`compiler/include/testing/testing_coordinator.hpp`** (~2 lines)
   - Add `bool run_all_mode = true` to TestConfig

### What Does NOT Change

- Compilation pipeline (compile_suite, compile_suites_parallel)
- Dispatcher IR generation (already supports `--run-all`)
- Coverage integration (TML_COVERAGE_FILE works with `--run-all`)
- Cache system (suite-level cache unchanged)
- Test discovery and grouping

### Expected Performance

| Metric | Current | Phase 1 | Phase 2 |
|--------|---------|---------|---------|
| Subprocesses | ~1452 | ~206 | ~1-10 |
| Link steps | ~206 | ~206 | ~1 |
| Process spawn overhead | ~150s | ~20s | ~1s |
| Total time (no cache) | ~10min | ~8min | ~4min |
| Total time (cached) | ~3min | ~1min | ~30s |

### Crash Recovery Strategy

When `--run-all` subprocess crashes:
1. Parse NDJSON to determine which tests passed before crash
2. Identify the crashing test (last `test_start` without `test_pass`/`test_fail`)
3. Mark it as crashed
4. Re-launch subprocess with `--test-index=N` for remaining untested tests
5. OR: accept the crash and report remaining tests as "not run"

## References

- Go test runner: `cmd/go/internal/test/test.go`
- Rust libtest: `library/test/src/lib.rs`
- Zig test runner: `lib/std/testing.zig` + `src/Compilation.zig`
- TML dispatcher: `compiler/src/testing/testing_dispatcher_gen.cpp`
- TML coordinator: `compiler/src/testing/testing_coordinator.cpp`
