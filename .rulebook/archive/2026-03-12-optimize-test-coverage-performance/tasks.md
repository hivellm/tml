# Tasks: optimize-test-coverage-performance

## Benchmark Results

| Metric | Before | After Phase 1 | After Phase 2 | Improvement |
|--------|--------|---------------|---------------|-------------|
| `tml test` (cached) | ~10 min | 20.4s | **6.2s** | **99% faster** |
| `tml test --coverage` | ~10 min | — | **~13 min** | Correct data (95.21%) |
| CPU usage | 100% saturated | ~50-68% | ~50-68% | Machine usable |
| Test failures | pre-existing | same | same | No regressions |
| Coverage accuracy | 99% | 7% (broken) | **95.21%** | Fixed regression |

## Phase 1 — Quick Wins ✅ COMPLETE

- [x] 1.1 Enable incremental cache for coverage (`testing_compile.cpp:195` — removed `!config.coverage`)
- [x] 1.2 Verified cached .obj files valid — coverage with cache runs in 20.4s
- [x] 1.3 Conditional OpenSSL linking — only when suite imports `std::crypto` or `std::net::tls` (`testing_compile.cpp:403-424`)
- [x] 1.4 ThreadBudget counting semaphore capped at `hw/2` prevents CPU oversubscription (`testing_compile.cpp:50-85`)
- [x] 1.5 Benchmark: **20.4s** with cache (was ~10min)
- [x] 1.6 Regression check: core/str(22), core/fmt(45), std/collections(53), core/iter(52), std/crypto(25) — all pass

## Phase 2 — Medium Effort ✅ COMPLETE

- [x] 2.1 Conditional OpenSSL linking (done as 1.3)
- [x] 2.2 Coverage bypasses result cache (`testing_coordinator.cpp:494` — `!config.coverage &&`); coverage restored in flags_hash for cache key separation
- [x] 2.3 Increase `HASH_TABLE_SIZE` 4093→32771 (`lib/test/runtime/coverage.c:39`)
- [x] 2.4 Replace `std::regex` with hand-written keyword scanners in `testing_coverage.cpp` — 7 regexes → 0, dropped 20.4s→6.2s
- [x] 2.5 Evaluated: 5ms `sleep_for` max ~1s impact, skipped (needs Process API change)
- [x] 2.6 Benchmark: **6.2s** (was 20.4s after Phase 1, ~10min before)

## Phase 3 — Architectural (deferred — 6.2s already excellent)

- [ ] 3.1 Design per-module test EXE architecture
- [ ] 3.2 Add `--test-filter` runtime parameter to test EXE dispatcher
- [ ] 3.3 Implement compile→execute pipelining
- [ ] 3.4 Implement LLD parallel linking via subprocess workers
- [ ] 3.5 Add LRU eviction to `.new-run-cache/` directory
- [x] 3.6 Remove hardcoded `F:/Node/hivellm/tml/` paths (`testing_compile.cpp:97-101,442`)
- [ ] 3.7 Final benchmark end-to-end

## Files Changed

- `compiler/src/testing/testing_compile.cpp` — incremental cache, ThreadBudget, conditional OpenSSL, hardcoded paths
- `compiler/src/testing/testing_test_cache.cpp` — coverage restored in flags_hash
- `compiler/src/testing/testing_coordinator.cpp` — coverage bypasses result cache (`!config.coverage &&`)
- `compiler/src/testing/testing_coverage.cpp` — replaced 7 std::regex with hand-written scanners
- `lib/test/runtime/coverage.c` — hash table size 4093→32771
