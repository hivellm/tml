## Phase 1 — Build System (CMakeLists.txt)
- [ ] 1.1 Enable `/MP` parallel compilation for all MSVC targets
- [ ] 1.2 Enable `/INCREMENTAL:YES` for incremental linking on tml.exe, tml_mcp.exe, plugin DLLs
- [ ] 1.3 Build compiler and verify no regressions

## Phase 2 — Test Runner
- [ ] 2.1 Change thread count from `hardware_concurrency() / 2` to `hardware_concurrency()` in testing_compile.cpp
- [ ] 2.2 Add longest-job-first sort of uncached suites by estimated duration in testing_coordinator.cpp
- [ ] 2.3 Add `exe_is_reusable()` to testing_test_cache: reuse exe when source unchanged even if last run failed
- [ ] 2.4 Wire exe reuse into coordinator: skip recompile for suites with unchanged source + existing exe
- [ ] 2.5 Build compiler and run test suite to verify correctness

## Phase 3 — Suite Merging Codegen Bug
- [ ] 3.1 Investigate root cause of symbol conflict between generic `repeat[T]` and concrete functions during suite merge
- [ ] 3.2 Fix deduplication in suite merging to allow `max_per_suite=10` in coverage mode
- [ ] 3.3 Re-enable incremental cache in coverage mode
- [ ] 3.4 Verify coverage mode with max_per_suite=10 produces correct results

## Validation
- [ ] V.1 Measure build time before/after Phase 1 (cold build)
- [ ] V.2 Measure build time before/after Phase 1 (incremental, single file change)
- [ ] V.3 Measure test suite time before/after Phase 2 (cold run)
- [ ] V.4 Measure coverage time before/after Phase 3
- [ ] V.5 All tests pass after each phase
