## 1. Implementation — COMPLETE
- [x] 1.1 Add `run_all_mode` flag to TestConfig (default true)
- [x] 1.2 Refactor execute_suites to launch `--run-all` per suite instead of `--test-index=N` per test
- [x] 1.3 Parse multi-test NDJSON stream from single subprocess
- [x] 1.4 Implement crash recovery (retry remaining tests individually on crash)
- [x] 1.5 Handle coverage file aggregation in --run-all mode
- [x] 1.6 Add per_test_timeout_us config (100ms default) with slow test reclassification

## 2. Testing — COMPLETE
- [x] 2.1 Run core/str, core/fmt, std/json, std/collections, core/iter — all pass
- [x] 2.2 Run with --coverage, verify coverage data correct — verified in rewrite-test-system Phase 5b
- [x] 2.3 Measure performance improvement (subprocess count, wall time) — 1452 → 206 subprocesses

## 3. Documentation — COMPLETE
- [x] 3.1 Analysis report: docs/analyses/single-binary-test-compilation.md
- [x] 3.2 Update CHANGELOG — covered by rewrite-test-system docs

## 4. Future Phases — SUPERSEDED
Phases 2 (mega-binary) and 3 (in-process execution) from the original analysis are now tracked in `zig-inspired-test-migration` task, which provides a more comprehensive 6-phase plan inspired by Zig's test architecture.
