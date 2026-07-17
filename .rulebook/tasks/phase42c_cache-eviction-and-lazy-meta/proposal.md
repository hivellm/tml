# Proposal: phase42c_cache-eviction-and-lazy-meta

## Why
Two structural wastes remain after phase42a/b (findings F-031, F-036, F-019 in `docs/analysis/incremental-cache/02-findings.md`):

- **F-031 (Medium):** no cache eviction exists anywhere — ~4.3 GB and growing monotonically (incr/ir 2.2 GB is phase42a's; obj_cache 609 MB + tests 1.4 GB + run 51 MB are here). The LRU evictor `enforce_cache_limit()` is fully implemented and has **zero call sites** (`cmd_cache.cpp:285-362`); `invalidate_all_exes()` clears JSON fields but never deletes the 1,516 EXE files.
- **F-036 (Medium-High):** every process eagerly runs `preload_all_meta_caches()` — 367 modules deserialized + ~734 file reads/hashes before the first user line is type-checked, regardless of what the file imports (`module_binary_read.cpp:1073-1157`; call sites in check/build/run/test/coverage). A validated lazy per-import loader already exists (`load_module_from_cache`, `module_binary_read.cpp:863-906`). This is the root of tooling finding F-015.
- **F-019 (High, stretch):** `tml check` bypasses the query system entirely (`cmd_debug.cpp:244-308`) — no memoization, no incremental reuse; a one-token edit recomputes everything.

## What Changes
- **Wire eviction:** size-capped LRU sweep (reuse `enforce_cache_limit`) at the end of test/build runs for `cache/tests/obj_cache`, unreferenced `cache/tests/*.exe`, and `cache/run`; configurable caps (defaults: obj_cache 256 MB, tests 512 MB, run 128 MB). `invalidate_all_exes()` deletes files, not just JSON fields (after 41c's content-aware invalidation lands — fewer wipes first, then clean what wipes leave).
- **Lazy meta:** import-driven loading through the existing validated loader for `check`/`build`/`run`; eager preload only where the full library is genuinely needed (doc generation, full test runs), behind an mtime-based "nothing changed" fast path that skips per-module source re-hashing.
- **Stretch (small ADR):** route `tml check` through `QueryContext` so it shares memo/incr layers.
- Opportunistic (F-032): consolidate `std::hash`/CRC key helpers into one shared 128-bit content-hash helper at touched call sites.

## Impact
- Affected specs: possibly a small ADR for the check→query routing (stretch)
- Affected code: `compiler/src/cli/commands/cmd_cache.cpp`, `cmd_debug.cpp`, `compiler/src/types/module_binary_read.cpp`, `compiler/src/testing/testing_test_cache.*`, `builder_helpers.cpp`
- Breaking change: NO (eviction is transparent; lazy loading preserves diagnostics)
- User benefit: caches stay bounded (~4 GB reclaimed); `tml check hello.tml` stops paying a 367-module tax; cold commands measurably faster
