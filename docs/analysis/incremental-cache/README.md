# Incremental-Cache Audit — Executive Summary

**Date:** 2026-07-17 · **Scope:** every cache layer of the TML toolchain — what invalidates or reprocesses despite unchanged inputs.
Continues `docs/analysis/tooling-performance/` (F-001..F-018); findings here start at **F-019**.
Explicitly excludes what phase41b (shared stdlib object, F-006/F-007/F-012) and phase41c (test result cache F-014, incr mutex F-010, import scans F-013) already own.

## One-line diagnosis

TML has *seven* independent cache layers, but the only one that could actually short-circuit compilation across
invocations (ADR-002 red-green) persists **only final LLVM IR**, keys it on **unstable identities**
(suite position index), invalidates it on **DLL mtime** (every rebuild), rewrites the **entire 10 MB cache file
once per test file**, and sits one insert away from a **10,000-entry cliff** that silently drops everything —
while 4.3 GB of never-collected artifacts accumulate and the three staleness holes (`.ast.bin`, daemon argv-only
mtimes, daemon-resident meta) are papered over by a `cache invalidate` tool that is mostly a no-op stub.

## Biggest levers (effort-adjusted, ranked)

1. **Stabilize + session-share the incr.bin layer** (F-020, F-021, F-023, F-024, F-025, F-026) — load once per
   run, save once per run, drop `test_entry_index` from the codegen-unit identity, content-hash the compiler DLL,
   partition by options hash. Turns red-green from "pays cost, rarely pays off" into a real cross-run cache.
2. **Close the three staleness holes** (F-027 `.ast.bin` unconditional trust, F-028 daemon argv-only mtimes,
   F-029 daemon-resident stale meta) — these are why manual `cache_invalidate` exists; fixing them removes the
   need for the stub tool (F-030) and a whole class of "flaky" bugs (cf. phase27c).
3. **Garbage-collect everything** (F-022, F-031) — 2.2 GB incr/ir + 609 MB obj_cache + 1.4 GB tests + 51 MB run,
   all unbounded; `enforce_cache_limit()` (LRU) is implemented and never called. One sweep pass + wiring = 4 GB back.
4. **Lazy, import-driven meta loading** (F-036) — stop eagerly deserializing 367 modules + re-hashing 367 source
   files per process; the per-import validated loader already exists.
5. **Route `check` through the query system** (F-019) — `tml check` bypasses every cache except the daemon's
   whole-command result cache; a one-token edit recomputes the universe.

## Raw state measured 2026-07-17 (live tree, tests running concurrently)

| Artifact | Count | Size | Notes |
|---|---|---|---|
| `build/debug/cache/incr/incr.bin` | 9,282 entries | 10.25 MB | header decoded: magic TMIC, v2.0; **93% of the 10,000-entry load cliff** |
| `build/debug/cache/incr/ir/` | 9,720 files | **2.2 GB** | never GC'd |
| `build/debug/cache/tests/obj_cache/` | 6,055 objs | **609 MB** | never GC'd (was 2,755/273 MB when 01-measurements.md was written — doubled) |
| `build/debug/cache/tests/` | 1,516 EXEs | 1.4 GB | `invalidate_all_exes()` clears JSON entries, never deletes files |
| `build/debug/cache/tests.json` | — | 784 KB | now populated (was 618 B in F-014 baseline) |
| `build/debug/cache/run/` | 147 files | 51 MB | LRU evictor exists, is dead code |
| `build/debug/cache/run/cache/incr/` | 94 IR files | 14 MB | separate incr.bin for `tml run` |
| `build/debug/cache/meta/` | 367 `.meta` | 7.4 MB | all eagerly loaded per process |

## Index

- `01-cache-inventory.md` — every cache layer: location, key, granularity, invalidation, state
- `02-findings.md` — F-019 .. F-036 with file:line evidence
- `03-proposed-tasks.md` — three proposed tasks (non-overlapping with phase41b/41c)
