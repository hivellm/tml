# 03 — Type-Check (`check`) Performance

**F-015 — `check` pays full cold-start + loads ALL library metadata regardless of imports.**
Evidence: measured ~461 ms for a trivial file vs ~50 ms bare startup; `init_compile_env()` → `types::preload_all_meta_caches()` (`testing_compile.cpp:90`) and the check path load the full core/std `.meta` set even when the file imports nothing beyond the prelude.
Impact: **Medium** — ~400 ms of avoidable per-call overhead when repeated (linting-on-save, MCP loops). Confidence: **Medium** (the meta-preload attribution needs one profile run to confirm the split; the cold-start component is certain).
**PARTIALLY RESOLVED (phase42c, v0.3.75) — see incremental-cache F-036.** The "load ALL library metadata regardless of
imports" half was investigated for a lazy/import-driven rewrite and **rejected as non-parity**: type/borrow/codegen resolve
unqualified references (primitive impl methods, library behaviors in bounds, behavior-impls, aliases) via full-cache
`GlobalModuleCache::get_all()` scans, so a lazy subset changes diagnostics (`env_lookups.cpp:156-162,190-196,314-321` et al.).
Eager preload therefore stays. The avoidable-work half is fixed instead: a directory-mtime "nothing changed" fast path skips
the per-module source re-hashing (~734 file reads) when the lib tree is unchanged, cutting cold-`check` preload from ~419 ms
to ~285 ms (~134 ms) with byte-identical output (366 modules loaded either way, 0/25 divergences). For the repeated
linting-on-save / MCP-loop workflow the dominant win remains the warm daemon (F-016/F-017, ~6.6 ms warm `check`).

**F-016 — RESOLVED (phase40a, 2026-07-17) — A warm daemon exists that fixes this, but it was opt-in and test-blind.**
Evidence (original): `cmd_daemon.cpp` — keeps LLVM/DLLs resident, in-process mtime result cache (cache-hit 22 ms per `08-compilation.md:59`). But forwarding only happened when `std::getenv("TML_DAEMON")=="1"`, and only for `build`/`run`/`check` (never `test`). Nothing in the normal workflow set `TML_DAEMON=1`. (Also: the `dispatcher.cpp` forward block sat *after* the `check` handler, making daemon forwarding for `check` unreachable in the DLL dispatcher — the thin launcher's pre-DLL forward masked this in modular builds.)
Impact: **High** — the fast path was already built and simply not wired into how anyone calls the tool. Confidence: **High**.
**Resolution:** the MCP server now sets `TML_DAEMON=1` per-child and auto-starts the daemon for `check`/`build`/`emit-ir`/`emit-mir` (F-017); the dispatcher ordering bug is fixed. Warm `check` on an unchanged file: **6.6 ms** median (was ~453 ms). `test` remains outside the daemon (not a forwardable command; unchanged scope).
