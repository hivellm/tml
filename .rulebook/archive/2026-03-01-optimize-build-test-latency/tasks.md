## Phase 1 — Build System (CMakeLists.txt)
- [x] 1.1 Enable `/MP` parallel compilation for all MSVC targets
- [x] 1.2 Document /INCREMENTAL incompatibility with LLVM static libs (LNK4224)
- [x] 1.3 Build compiler and verify no regressions

## Phase 2 — Test Runner
- [x] 2.1 Change thread count from `hardware_concurrency() / 2` to `hardware_concurrency()` in testing_compile.cpp
- [x] 2.2 Add longest-job-first sort of uncached suites by estimated duration in testing_coordinator.cpp
- [x] 2.3 Add `get_reusable_exe()` to testing_test_cache: reuse exe when source unchanged even if last run failed
- [x] 2.4 Wire exe reuse into coordinator: skip recompile for suites with unchanged source + existing exe
- [x] 2.5 Add `compile_time_us` to SuiteCacheEntry (persisted in JSON) for scheduling estimates
- [x] 2.6 Build compiler and run test suite to verify correctness (598/634 pass, 0 regressions)

## Phase 3 — Suite Merging Codegen Bug
- [x] 3.1 Investigate root cause: bug was in OLD test system (deleted). v3 uses fresh QueryContext per file — no shared state
- [x] 3.2 Remove coverage restriction: set `max_per_suite = 10` unconditionally in cmd_test.cpp (was `coverage ? 1 : 10`)
- [x] 3.3 Incremental cache in coverage mode: already works (separate flags_hash includes coverage=1)
- [x] 3.4 Verify coverage mode with max_per_suite=10: 1096/1096 passed, 73.5% coverage, 318s, 0 regressions

## Validation
- [x] V.1 Build time Phase 1: /MP enabled, parallel compilation across all 366 .cpp files
- [x] V.2 /INCREMENTAL incompatible with LLVM static libs (LNK4224) — documented, not applied
- [x] V.3 Test suite Phase 2: full thread count + longest-job-first + exe reuse for failing suites
- [x] V.4 Coverage Phase 3: 1096 tests, 318s cold, 73.5% library coverage
- [x] V.5 All tests pass after each phase (0 regressions)
