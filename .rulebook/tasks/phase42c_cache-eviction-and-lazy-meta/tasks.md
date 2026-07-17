## 1. Implementation
- [ ] 1.1 F-031 eviction wiring: size-capped LRU sweep reusing `enforce_cache_limit` (`cmd_cache.cpp:285-362`, currently dead code) at end of test/build runs for `cache/tests/obj_cache`, unreferenced `cache/tests/*.exe`, `cache/run`; configurable caps (obj_cache 256 MB, tests 512 MB, run 128 MB defaults); one-time sweep of current backlog with before/after sizes recorded
- [ ] 1.2 F-031 EXE deletion: `invalidate_all_exes()` deletes the invalidated EXE files, not just JSON fields (`testing_test_cache.hpp:115-120`) — sequence after phase41c's content-aware invalidation so wipes are rare first
- [ ] 1.3 F-036 lazy meta: replace eager `preload_all_meta_caches()` in `check`/`build`/`run` with import-driven loading via the existing validated `load_module_from_cache` (`module_binary_read.cpp:863-906`); keep eager preload for doc generation + full test runs behind an mtime "nothing changed" fast path that skips per-module source re-hashing (`module_binary_read.cpp:1073-1157`)
- [ ] 1.4 Verify lazy-meta correctness: full corpus check diagnostics byte-identical eager vs lazy; measure `tml check` on prelude-only file (modules loaded, wall-clock) and on heavy-import files
- [ ] 1.5 F-019 stretch: evaluate routing `tml check` through `QueryContext` (`cmd_debug.cpp:244-308`) — small ADR with decision + evidence; implement if the win is clear, otherwise document precisely what blocks it
- [ ] 1.6 F-032 opportunistic: consolidate `std::hash`/CRC key generation at touched call sites into one shared 128-bit content-hash helper (`builder_helpers.cpp:124-153`, `query_fingerprint.cpp:28-46`); remove obj-mtime taint from the run exe key
- [ ] 1.7 GATE: total cache footprint stays under caps across 3 consecutive full runs (measured); `tml check` on prelude-only file loads <30 modules (log-verified) with unchanged diagnostics on the full corpus; zero test-result divergence; numbers in `docs/analysis/incremental-cache/`

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation: `docs/analysis/incremental-cache/02-findings.md` statuses (F-031/F-036/F-019/F-032), tooling-performance F-015 status, ADR if written; CHANGELOG/VERSION bump
- [ ] 2.2 Write tests covering the new behavior (eviction caps respected, lazy-load correctness, invalidate deletes files)
- [ ] 2.3 Run tests and confirm they pass
