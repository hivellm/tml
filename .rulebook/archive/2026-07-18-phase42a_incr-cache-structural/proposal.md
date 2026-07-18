# Proposal: phase42a_incr-cache-structural

## Why
The ADR-002 red-green incremental cache pays full cost and almost never pays off (findings F-020..F-026 in `docs/analysis/incremental-cache/02-findings.md`):
- The 10.25 MB `incr.bin` is loaded, merged and **fully rewritten once per test file** — ~27 GB of cache I/O per full suite run (F-020, `query_context.cpp:343-370`, `testing_compile.cpp:645-648,714-718`).
- `CodegenUnitKey` embeds `test_entry_index` (position in suite), so adding/removing/reordering ONE test file turns every subsequent file RED — suite *membership*, not content, decides invalidation (F-023, `query_key.hpp:114-123`).
- `compiler_build_hash()` = DLL **mtime**: every rebuild wipes the whole cache even when behavior didn't change (F-024, `query_incr.cpp:40-79`).
- Hard 10,000-entry load cliff at 9,282 entries today — one wave from silent total invalidation (F-021, `query_incr.cpp:326-330`).
- One `options_hash` per file: coverage↔normal runs mutually evict (F-025).
- Each per-file QueryContext re-hashes all 367 `.meta` files and ~300+ stdlib sources — ~1,339× per full run (F-026).
- The `ir/` store (9,720 files / **2.2 GB**) is never GC'd and GREEN hits pointlessly rewrite their own IR (F-022).

Net today: the incremental layer mostly adds I/O. This is exactly the "reprocessing what didn't change" waste the project must not have.

## What Changes
- **Session-scoped incr state:** one shared `PrevSessionCache` + one `IncrCacheWriter` per test/build run (thread-safe append) — load once, save once at end; removes per-context load/merge/rewrite semantics. (Complements 41c's mutex batching; does not overlap it.)
- **Key stability:** remove `test_entry_index` (and `has_cached_library_state`) from `CodegenUnitKey` identity; emit the `tml_test_N` wrapper stub outside the cached unit (or cache module body separately from the entry stub).
- **Content-based build hash:** CRC of DLL *content*, memoized in a sidecar keyed by mtime:size (one hash per actual rebuild), replacing raw mtime.
- **Config partitioning:** `incr.<options_hash>.bin`, bounded set (keep newest ~4), instead of one mutually-evicting file.
- **Aging instead of cliff:** replace the 10,000-entry load rejection with session-timestamp aging at save.
- **GC `ir/`:** delete entries not referenced by surviving keys after save; stop re-writing IR on GREEN hits.
- **Session-level source-fingerprint memo** (path → (mtime,size) → fp) shared across QueryContexts.
- **Telemetry:** GREEN/RED counts + cache parse time per run.

## Impact
- Affected specs: ADR-002 (implementation notes; decision unchanged)
- Affected code: `compiler/src/query/query_context.cpp`, `query_incr.cpp`, `compiler/include/query/query_key.hpp`, `query_incr.hpp`, `compiler/src/testing/testing_compile.cpp`, `testing_compile_parallel.cpp`
- Breaking change: NO (cache format version bump; old caches regenerate once)
- User benefit: red-green becomes a real cross-run cache — a one-file edit stops recompiling the universe; ~26 GB of per-run cache I/O eliminated; ≥2 GB disk reclaimed
