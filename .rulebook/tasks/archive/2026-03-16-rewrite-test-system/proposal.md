# Proposal: Rewrite Test System

**Status**: Approved
**Priority**: Critical
**Estimated Effort**: 8-12 weeks
**Impact**: Core developer infrastructure

---

## Why

The current TML test system is a **15,464-line patchwork across 25 C++ files** that has grown through accretion rather than design. It has critical bugs (coverage hangs, suite codegen bug, pipe leaks), multiple workarounds masking unresolved issues, and two incompatible execution models neither of which works fully. This is the most important developer tool in the project and it needs to be rebuilt from scratch with a clean architecture inspired by Go, Rust, and industry best practices.

## What Changes

Complete replacement of the test runner infrastructure with a subprocess-based architecture:
- **Subprocess execution only** (no DLL loading) for crash isolation
- **JSON protocol** between test subprocess and coordinator
- **Correct test caching** (source + dependencies + compiler version + flags)
- **Reliable coverage** via codegen instrumentation (no LLVM profiling runtime)
- **Per-test timeouts** (hung test does not lose other results)
- **Code reduction**: 25 files / 15,464 lines to ~15 files / ~7,000 lines

## Impact

- Affected specs: docs/specs/09-CLI.md, docs/specs/10-TESTING.md
- Affected code: compiler/src/cli/tester/ (rewrite), lib/test/runtime/ (simplification), cmd_test.cpp
- Breaking change: NO (same CLI interface)
- User benefit: Coverage works, crash isolation, per-test timeouts, JSON output

---

## 1. Problem Statement (15 Issues)

### Critical Architectural

1. **Two incompatible execution models** (DLL vs EXE) - neither fully works
2. **Suite merging codegen bug** - workaround destroys 8x speedup (suite_execution.cpp:236, exe_suite_runner.cpp:122)
3. **Coverage hangs** - `tml test --coverage` hangs on specific tests, root cause unknown
4. **Disabled precompiled symbol cache** - `#if 0` in test_runner.cpp:141-267
5. **Thread-unsafe output capture** - global fd redirection (test_runner_exec.cpp:165+)

### Reliability

6. **Fragile crash handling** - 1024-byte buffer, manual context tracking
7. **String-based error detection** - matches substrings "Lexer", "Parser" (execution.cpp:54-60)
8. **Incomplete Unix async** - not implemented (exe_test_execution.cpp:261-264)
9. **Pipe handle leak** - handles not stored in AsyncSubprocessHandle (TODO line 305)
10. **Cache misses library changes** - only hashes source, not deps/compiler/flags

### Performance and Complexity

11. I/O-bound linking (37s/100s)
12. Redundant generic compilation per DLL
13. Coverage doubles execution time
14. 25 files, 15,464 lines, 4 execution paths
15. C runtime: coverage.c (676 lines) with fixed 4093-entry hash table

---

## 2. Research Summary

### Go (gold standard): process-per-package, static discovery, built-in caching/timeouts, source-level coverage
### Rust: compile-time `#[test]`, thread parallelism with panic catching, nextest for process isolation
### pytest: fixture system, plugin hooks, xdist for multi-process
### Google Test: death tests, static registration, sharding, structured output

### Priority order: isolation > caching > timeouts > discovery > parallelism > output > coverage

---

## 3. New Architecture

### 3.1 Principles

1. **Subprocess-only** - no DLL loading (Go model)
2. **Compile-time registration** - `@test` generates dispatch tables (Rust model)
3. **Correct caching** - hash source + deps + compiler + flags (Go model)
4. **Codegen coverage** - no LLVM profiling runtime (Go model)
5. **JSON protocol** - structured subprocess communication (Go + GTest)

### 3.2 Architecture

```
                        tml test (CLI)
                            |
              +-------------+-------------+
              |             |             |
         Discovery     Coordinator    Reporter
              |             |             |
              +-------------+-------------+
                            |
                   +--------+--------+
                   |        |        |
              Compiler  Compiler  Compiler   (parallel)
                   |        |        |
              suite.exe suite.exe suite.exe  (EXE binaries)
                   |        |        |
              subprocess subprocess subprocess
                   |        |        |
                JSON stdout (structured results)
```

### 3.3 JSON Protocol

```json
{"event":"suite_start","name":"core_str","test_count":12}
{"event":"test_start","index":0,"name":"test_concat","file":"basic.test.tml"}
{"event":"test_pass","index":0,"duration_us":1523}
{"event":"test_fail","index":1,"error":"assert_eq: expected 3, got 2","duration_us":892}
{"event":"test_crash","index":2,"signal":"SIGSEGV","backtrace":"..."}
{"event":"test_timeout","index":3,"timeout_ms":20000}
{"event":"coverage","functions":["str_concat","str_split"],"hits":[5,3]}
{"event":"suite_end","passed":10,"failed":1,"crashed":1,"timed_out":1}
```

### 3.4 Cache Key

source_hash + dependency_hashes + compiler_version + flags (backend, coverage, release, features)

### 3.5 File Structure (New)

```
compiler/src/cli/tester/
  coordinator.cpp/.hpp   - Main orchestration (replaces 3 files)
  discovery.cpp          - Test file discovery (cleaned up)
  compiler.cpp/.hpp      - Suite compilation to EXE (extracted)
  process.cpp/.hpp       - Cross-platform subprocess (replaces 1 file)
  cache.cpp/.hpp         - Correct cache invalidation (rewrite)
  coverage.cpp/.hpp      - Codegen-based coverage (rewrite)
  reporter.cpp/.hpp      - Multi-format output (rewrite)
  dispatcher_gen.cpp     - Test dispatcher IR (cleaned)
  benchmark.cpp          - Keep
  fuzzer.cpp             - Keep
  diagnostic.cpp         - Keep
```

**15 files removed** (~10,000 lines):
test_runner.cpp/hpp, test_runner_exec.cpp, test_runner_single.cpp, test_runner_internal.hpp, suite_execution.cpp, exe_suite_runner.cpp, exe_test_runner.cpp/hpp, exe_test_execution.cpp, tester_run.cpp, tester_internal.hpp, tester_helpers.cpp, execution.cpp, library_coverage_report.cpp

### 3.6 Key Decisions

**Subprocess-only eliminates**: output capture races, crash non-isolation, coverage hangs, DLL unload issues, TLS conflicts. Cost: ~5ms/suite overhead (negligible).

**Codegen coverage**: compiler already inserts `tml_cover_func()` calls. Subprocess writes covered names to file. Coordinator reads and aggregates. No LLVM profiling, no hangs.

**Per-test timeouts in dispatcher**: timer per test, emit timeout event, continue to next test. Coordinator has suite-level timeout as safety net.

---

## 4. Migration Strategy

1. Build new system in `tml::cli::tester_v2` namespace
2. Add `--new-runner` flag to switch to new coordinator
3. Run both in parallel during transition
4. Validate: all tests pass, coverage matches, performance comparable
5. Remove old code (15 files, ~10,000 lines)

---

## 5. Success Criteria

1. All existing tests pass (zero regressions)
2. Coverage mode works without hangs
3. Cache invalidation correct (no false hits)
4. Crash isolation works (subprocess model)
5. Per-test timeouts work
6. Code: 15,464 -> ~7,000 lines
7. JSON output available
8. Cross-platform (Windows + Linux + macOS)
9. Performance within 10% of current
10. `tml test --coverage` under 3 minutes
