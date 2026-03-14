## 1. Implementation
- [x] 1.1 Add `run_all_mode` flag to TestConfig (default true)
- [x] 1.2 Refactor execute_suites to launch `--run-all` per suite instead of `--test-index=N` per test
- [x] 1.3 Parse multi-test NDJSON stream from single subprocess
- [x] 1.4 Implement crash recovery (retry remaining tests individually on crash)
- [x] 1.5 Handle coverage file aggregation in --run-all mode
- [x] 1.6 Add per_test_timeout_us config (100ms default) with slow test reclassification

## 2. Testing
- [x] 2.1 Run core/str, core/fmt, std/json, std/collections, core/iter — all pass
- [ ] 2.2 Run with --coverage, verify coverage data correct
- [ ] 2.3 Measure performance improvement (subprocess count, wall time)

## 3. Documentation
- [x] 3.1 Analysis report: docs/analyses/single-binary-test-compilation.md
- [ ] 3.2 Update CHANGELOG
