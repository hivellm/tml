# Optimize Test Coverage Performance

## Status: in-progress
## Priority: high
## Created: 2026-03-12

## Purpose

Test coverage runs take ~10 minutes and saturate CPU at 100%, making the development feedback loop unacceptably slow. This task implements a 3-tier optimization plan derived from a 4-agent deep analysis (compiler-optimizer, deep-analysis-reviewer, build-engineer, qa-code-analyst) to reduce coverage time from ~10min to under 2min.

## Why

Coverage is essential for tracking regression and ensuring code quality across 1452 tests in 206 suites. At 10 minutes per run, developers avoid running coverage, leading to blind spots. The root causes are architectural: incremental cache disabled for coverage, serialized LLD linking via global mutex, thread oversubscription (16 LLVM threads on 8 cores), and unconditional OpenSSL linking for all test EXEs.

## What Changes

### Tier 1 — Quick Wins (target: 10min → 3min)

1. **Enable incremental cache for coverage mode** — The line `!config.coverage` in `testing_compile.cpp:190` disables all incremental .obj reuse during coverage runs. Coverage instrumentation is injected at the TML codegen level (not at the .obj level), so cached .obj files are valid for coverage. Remove this flag to allow warm-cache coverage runs.

2. **Increase max_per_suite for coverage** — Currently `cmd_test.cpp:253` sets `max_per_suite = 10` globally, but coverage mode effectively uses 1 (forced by earlier architecture). Allow coverage to use the same grouping as normal runs, reducing link count from ~1452 to ~145.

3. **Fix thread oversubscription** — `testing_compile.cpp:164-165` creates `min(4, hw/2)` inner compile threads per suite. `compile_suites_parallel` at line 498-501 creates another `min(4, hw/2)` outer threads. Result: 4×4 = 16 concurrent LLVM compilation jobs on an 8-core machine. Fix: use a shared thread budget capped at `hardware_concurrency()`.

### Tier 2 — Medium Effort (target: 3min → 1min)

4. **Conditional OpenSSL linking** — `testing_compile.cpp:403-414` links libcrypto and libssl to EVERY test EXE unconditionally. Only ~5% of suites use crypto. Add a check for crypto module imports before adding OpenSSL to link flags.

5. **Separate coverage from cache flags_hash** — `testing_test_cache.cpp:567` includes `max_per_suite` in the flags hash. Toggling between `tml test` and `tml test --coverage` invalidates the entire suite cache. Separate compilation cache (reusable) from execution cache (coverage-dependent).

6. **Increase coverage hash table size** — `lib/test/runtime/coverage.c:39` defines `HASH_TABLE_SIZE = 4093` for 16000+ functions. At >100% load factor with open addressing, probe chains degrade. Increase to 32771 (prime, ~50% load factor).

### Tier 3 — Architectural (target: 1min → 20s)

7. **Per-module EXEs** — Instead of 1 EXE per suite chunk, build 1 EXE per module (~50 total). Use runtime `--test-filter` to select which tests run. This is the Go test model.

8. **Pipeline compile→execute** — Start executing completed EXEs while others are still compiling, instead of compile-all-then-run-all.

9. **Parallel LLD linking** — The `g_lld_mutex` in `lld_linker.cpp:41` serializes all link operations. Use LLD subprocess workers (4 concurrent) to bypass the non-reentrant in-process limitation.

## Impact

- **Affected code**: `compiler/src/testing/`, `compiler/src/cli/commands/cmd_test.cpp`, `compiler/src/backend/lld_linker.cpp`, `lib/test/runtime/coverage.c`
- **No breaking changes**: All changes are internal to the test infrastructure
- **User benefit**: Coverage runs drop from ~10min to <2min (Tier 1+2), enabling coverage as part of regular development workflow
- **CPU impact**: Thread budget fix eliminates CPU saturation, improving developer machine usability during test runs

## Analysis Reports

Detailed analysis from 4 specialized agents saved in `.claude/memory/`:
- `test-perf-compiler-optimizer-analysis.md` — LLVM compilation, linking, coverage instrumentation
- `test-perf-deep-analysis.md` — Root-cause: incremental cache, thread explosion, cache invalidation
- `test-perf-build-engineer-analysis.md` — LLD mutex, OpenSSL linking, obj reuse
- `test-perf-qa-analysis.md` — Architecture vs Go model, polling overhead, subprocess count
