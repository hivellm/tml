# Proposal: phase41c_test-cache-parallelism

## Why
Even with aggregation (phase41a) and shared stdlib (phase41b), the test pipeline leaves large wins on the table (findings F-008/F-009/F-010/F-013/F-014 in `docs/analysis/tooling-performance/04-test-framework-performance.md`):

- **F-014 (High):** the result cache is effectively dead — `tests.json` is 618 bytes (≈empty). `is_cached` requires `all_passed` + exact source-hash match, and `compute_compiler_hash` fingerprints the 71 MB DLL by mtime:size, so **every compiler rebuild wipes the entire EXE cache** (`invalidate_all_exes()`), and something prevents `tests.json` from ever being populated in normal runs.
- **F-010 (Medium):** every file's incremental-cache save serializes all suite workers on a global mutex (`testing_compile.cpp:58`, `707-711`).
- **F-013 (Low-Medium):** each test file is re-read 2-3× to detect imports (`testing_compile.cpp:909-962`, `1175-1263`).
- **F-008 (Medium-High):** per-file codegen inside a suite is forced single-threaded (`testing_compile.cpp:598`, LLVM global-state safety) — revisit after phase41b restructures codegen state.
- **F-009 (Low-Medium):** one detached watchdog thread per file, 100 ms polling (`testing_compile.cpp:645-668`).

## What Changes
- Make the result cache real: diagnose why `tests.json` stays empty and fix persistence; store per-suite results (not only all-passed); make invalidation content-aware instead of whole-DLL mtime:size (candidate: compiler VERSION + DLL content hash, computed once per run) so rebuilds that don't change behavior don't nuke 837 MB of cached EXEs. Correctness bias: when in doubt, invalidate.
- Batch/append incremental-cache writes to remove the global-mutex serialization.
- Scan each test file's imports once and thread the result through.
- Where phase41b's state restructuring allows: raise per-suite file parallelism > 1; replace per-file watchdog threads with a shared timer.

## Impact
- Affected specs: none
- Affected code: `compiler/src/testing/testing_test_cache.cpp`, `testing_compile.cpp`, `testing_compile_parallel.cpp`, `testing_coordinator.cpp`
- Breaking change: NO (identical results; cache is transparent)
- User benefit: unchanged-test reruns become near-free; suite compilation stops serializing on cache I/O; fewer redundant file reads and threads
