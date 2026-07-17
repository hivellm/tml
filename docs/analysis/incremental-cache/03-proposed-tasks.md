# 03 — Proposed Tasks

Three coherent tasks. Explicitly non-overlapping with phase41b (stdlib codegen/linkage) and phase41c
(test result-cache persistence/DLL-hash policy for tests.json, incr-save mutex batching, import scans, watchdogs).
Where an item touches 41c's area it is scoped to the *incr.bin/query* side only.

Materialized as rulebook tasks: `phase42a_incr-cache-structural`, `phase42b_cache-staleness-correctness`,
`phase42c_cache-eviction-and-lazy-meta`.

---

## Task A — `phase42a_incr-cache-structural`: make red-green a real cross-run cache

**Why:** the ADR-002 layer pays full cost (per-file 10 MB load+rewrite, F-020) and almost never pays off
(unstable keys F-023, mtime wipe F-024, options eviction F-025, 10K cliff F-021, per-file re-fingerprinting F-026,
2.2 GB orphan store F-022).

**What changes**
- Session-scoped incr state: one shared `PrevSessionCache` + one `IncrCacheWriter` per test/build *run*
  (thread-safe append), load once, save once at end. (Complements 41c's mutex item — 41c batches writes;
  this removes the per-context load/merge/rewrite semantics in `query_context.cpp:343-370`.)
- Key stability: remove `test_entry_index` (and `has_cached_library_state`) from `CodegenUnitKey` *identity*;
  emit the `tml_test_N` wrapper index outside the cached unit (or cache the module body separately from the entry stub).
- Content-based build hash: `compiler_build_hash()` = CRC of DLL *content* (computed once per process, cached in a
  sidecar keyed by mtime:size so it costs one hash per actual rebuild), replacing raw mtime (`query_incr.cpp:40-79`).
- Config partitioning: `incr.<options_hash>.bin` (bounded set, e.g. keep newest 4) instead of one mutually-evicting file.
- Replace the 10,000-entry load rejection with session-timestamp aging (drop entries not touched in N sessions) at save.
- GC `ir/`: after save, delete `ir/*` not referenced by any surviving entry key; stop re-writing IR on GREEN hits
  (`query_context.cpp:408-413`).
- Session-level source-fingerprint memo (path → (mtime,size) → fp) shared across QueryContexts so 300+ stdlib files are
  hashed once per run, not once per file.
- Telemetry: log GREEN/RED counts + cache-file parse time per run (feeds the gate).

**Gates:** GREEN-hit rate >0 measured on a no-change second run with one touched file; incr.bin I/O per full suite run
reduced from ~2×10 MB×N_files to ~2×file-size total; `ir/` bounded (measure before/after, expect ≥2 GB reclaimed);
identical test results; suite wall-clock not worse.

**Findings:** F-020, F-021, F-022, F-023, F-024, F-025, F-026 (+ F-033 framing).

---

## Task B — `phase42b_cache-staleness-correctness`: no stale result, ever — and a real invalidate command

**Why:** three under-invalidation holes force manual cache busting, and the manual tool is a stub (F-027..F-030).
The user-visible symptom is "the toolchain lies until you nuke a cache dir".

**What changes**
- `.ast.bin`: validate against a stored source content-hash (same scheme as `.meta`) before use; on mismatch fall through
  to `force(ParseModuleKey)`; register the ReadSource dep either way (`query_context.cpp:134-173`).
- Daemon result cache: snapshot the *transitive* source set (from the query dep graph /
  `TypeEnv::loaded_source_files`, cheap max-mtime probe as fallback) instead of argv-only `.tml` args
  (`cmd_daemon.cpp:329-347`).
- Daemon meta staleness: on each request, cheap lib-tree change probe (max mtime of `lib/*/src` or the meta dir);
  on change, drop `GlobalModuleCache` + re-run preload (convert `call_once` to a validated epoch;
  `module_binary_read.cpp:1371-1429`) and clear the daemon result cache.
- Rewrite `tml cache invalidate` to do what it says: delete matching test EXEs + rewrite tests.json entries, drop the
  file's incr entries + IR files, delete `.ast.bin` sidecars, and report exactly what was removed; wire `cache info/clean`
  to cover all cache dirs, and call `enforce_cache_limit()` (see Task C) from it (`cmd_cache.cpp:364-524`).

**Gates:** repro scripts for each hole (edit imported module → warm daemon check returns *new* diagnostics; stale
`.ast.bin` → recompiled; lib signature edit → daemon type error appears without restart); `cache invalidate <file>`
verifiably removes ≥1 artifact of each class it claims.

**Findings:** F-027, F-028, F-029, F-030.

---

## Task C — `phase42c_cache-eviction-and-lazy-meta`: bounded caches, import-driven metadata

**Why:** 4.3 GB of unbounded artifacts (F-022 handled in A; obj_cache/tests/run here — F-031), and every process pays a
~734-file-read metadata tax before compiling anything (F-036), even `tml check hello.tml`.

**What changes**
- Wire eviction: size-capped LRU sweep (reuse `enforce_cache_limit`, currently dead code, `cmd_cache.cpp:285-362`) run
  opportunistically at the end of test/build runs for `cache/tests/obj_cache`, `cache/tests/*.exe` not referenced by
  tests.json, and `cache/run`; make caps configurable (defaults: obj_cache 256 MB, tests 512 MB, run 128 MB).
- Delete orphaned EXEs when `invalidate_all_exes()` fires (files, not just JSON fields) — coordinate with 41c so its
  content-aware invalidation lands first (fewer wipes), then this cleans what wipes leave behind.
- Lazy meta: replace the eager `preload_all_meta_caches()` in `check`/`build`/`run` with import-driven loading through the
  existing validated `load_module_from_cache` (`module_binary_read.cpp:863-906`); keep eager preload only where the full
  library is genuinely needed (doc generation, full test runs), gated behind an mtime-based "nothing changed" fast path
  that skips per-module source re-hashing.
- Stretch (own decision point, small ADR): route `tml check` through `QueryContext` so it shares the memo/incr layers
  (F-019) instead of its bespoke pipeline (`cmd_debug.cpp:244-308`).

**Gates:** total cache footprint stays under configured caps across 3 consecutive full runs; `tml check` on a
prelude-only file loads <30 modules (log-verified) with unchanged diagnostics on the full corpus; no test-result changes.

**Findings:** F-031, F-036, F-019 (stretch), F-032 opportunistically (swap `std::hash`/CRC key strings for one shared
128-bit content hash helper while touching the call sites).
