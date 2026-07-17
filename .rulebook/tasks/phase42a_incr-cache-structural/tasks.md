## 1. Implementation
- [ ] 1.1 Telemetry first (measure before changing): add GREEN/RED hit counters + incr.bin load/save timing per run; record baseline on a representative suite set + full run in `docs/analysis/incremental-cache/` (expected: GREEN ≈ 0 in practice)
- [ ] 1.2 Session-scoped incr state: one shared `PrevSessionCache` load + one `IncrCacheWriter` (thread-safe append) per test/build run; kill the per-context load/merge/rewrite in `save_incremental_cache` (`query_context.cpp:343-370`); wire test paths (`testing_compile.cpp:645-648,714-718`, `testing_compile_parallel.cpp:247-249,273-277`)
- [ ] 1.3 Key stability (F-023): remove `test_entry_index` and `has_cached_library_state` from `CodegenUnitKey` identity (`query_key.hpp:114-123`, serialization `query_incr.cpp:164-176`); generate the `tml_test_N` wrapper stub outside the cached unit; verify a suite-membership change no longer REDs sibling files
- [ ] 1.4 Content-based compiler build hash (F-024): hash DLL content memoized by mtime:size sidecar, replacing raw mtime in `compiler_build_hash()` (`query_incr.cpp:40-79`); verify a no-change rebuild keeps the cache GREEN
- [ ] 1.5 Config partitioning (F-025) + aging (F-021): per-options_hash cache files (bounded set), session-timestamp aging at save instead of the 10,000-entry load rejection (`query_incr.cpp:326-330`)
- [ ] 1.6 IR store GC (F-022): after save, delete `ir/*` not referenced by surviving keys; stop re-saving identical IR on GREEN hits (`query_context.cpp:408-413`); one-time sweep of the current 2.2 GB
- [ ] 1.7 Session-level source-fingerprint memo (F-026): (path → (mtime,size) → fp) shared across QueryContexts so stdlib/meta files are hashed once per run (`query_incr.cpp:639-662`, `query_fingerprint.cpp:59-76`)
- [ ] 1.8 GATE: on a second run with ONE touched file, GREEN-hit rate > 0 and only affected units RED (telemetry-verified); incr I/O per full run ≈ 2× file size total (was ~27 GB); `ir/` ≥2 GB reclaimed and bounded; zero test-result divergence; full-run wall-clock not worse — numbers in `docs/analysis/incremental-cache/`

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation: `02-findings.md` statuses (F-020..F-026), ADR-002 implementation notes, CHANGELOG/VERSION bump
- [ ] 2.2 Write tests covering the new behavior (key stability across suite membership, content-hash rebuild survival, aging, IR GC, memo correctness)
- [ ] 2.3 Run tests and confirm they pass
