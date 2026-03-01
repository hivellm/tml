# Proposal: optimize-build-test-latency

## Why

Build and test latency are the primary bottleneck to developer velocity. The TML compiler takes ~100 seconds to build from scratch and the test suite takes 10-20 minutes cold. Three root causes identified via profiling:

1. **No parallel C++ compilation** — 366 `.cpp` files compile serially (no `/MP` flag on MSVC)
2. **No incremental linking** — every build re-links 2.4 GB of static libs (37s out of 100s total)
3. **Test thread pool at half capacity** — `hardware_concurrency() / 2` wastes half the cores
4. **Failing suites always recompiled** — even when source is unchanged, failing suites recompile
5. **No longest-job-first scheduling** — thread pool stalls waiting for slow tail suites
6. **Suite merging codegen bug** — forces `max_per_suite=1` in coverage mode (1084 DLLs vs ~108)

## What Changes

### Phase 1 — CMakeLists.txt (immediate, low risk)
- Enable `/MP` for parallel C++ compilation across all MSVC targets
- Enable `/INCREMENTAL:YES` for incremental linking on executables and DLLs

### Phase 2 — Test runner (testing_compile.cpp, testing_coordinator.cpp)
- Change default thread count from `hardware_concurrency() / 2` to `hardware_concurrency()`
- Add longest-job-first scheduling using timing data from the test result cache
- Add exe reuse for failing suites: if source unchanged, reuse existing exe instead of recompiling

### Phase 3 — Suite merging bug (testing_compile.cpp)
- Fix symbol deduplication during suite merging to allow `max_per_suite=10` in coverage mode
- Re-enable incremental cache in coverage mode after fix

## Impact

- Affected specs: none (internal build system changes)
- Affected code: `compiler/CMakeLists.txt`, `compiler/src/testing/testing_compile.cpp`, `compiler/src/testing/testing_coordinator.cpp`, `compiler/src/testing/testing_test_cache.cpp`
- Breaking change: NO
- User benefit:
  - **Build cold:** ~100s → ~45s (Phase 1)
  - **Build incremental:** ~100s → ~5-15s (Phase 1)
  - **Test cold:** 10-20min → 2-4min (Phase 1+2)
  - **Test coverage cold:** 10-20min → 30-90s (Phase 1+2+3)
  - **Edit-test-fix cycle:** 10-30s recompile → <5s exe reuse (Phase 2)
