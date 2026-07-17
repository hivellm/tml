# Tooling Performance — Executive Summary

**Date:** 2026-07-17
**Topic:** Why the TML tooling infrastructure (test framework, `check`, build) is extremely slow, and everything that can be optimized.

Builds on prior analyses (does not duplicate):
- `docs/analysis/compiler-internals/single-binary-test-compilation.md` (2026-03-14, "approved" but the default path still doesn't use it)
- `docs/analysis/benchmark/08-compilation.md` (27× vs Rust, DLL-load breakdown, daemon numbers)

## One-line diagnosis

The tooling is slow for three compounding reasons:

1. The compiler everyone uses is an **unoptimized `-O0` debug build** of a **123 MB pair of DLLs**.
2. `tml test` compiles **one native EXE per test file** (~1339 separate LLVM-codegen + LLD-link cycles, each re-emitting the entire stdlib) instead of the aggregated-binary model that already exists behind opt-in flags.
3. The **MCP layer throws away all warm-state**: `mcp__tml__check`/`test` spawn a fresh cold `tml.exe` subprocess every call and never touch the warm daemon that was built for exactly this.

## Biggest levers (ranked by effort-adjusted payoff)

1. Wire MCP → daemon (or in-process warm handler) — turns repeated `check` from ~460 ms cold into ~22 ms. (F-017, F-016)
2. Re-enable the stdlib codegen-state fast-path + stop embedding full stdlib per test obj (F-006, F-007).
3. Make `--suite-mode`/`--unified` the default so full test runs do ~1–30 links instead of ~1339 (F-005). — **DONE (phase41a):** suite-mode default at 25 files/EXE, full-run 2066 → 176 links (**11.7×**); `--coverage`/`--no-suite` force per-file; compile-failure + crash/timeout isolation preserved, parity verified (`test_aggregation.sh` 16/16). See `04-test-framework-performance.md`.
4. Ship a **release** (`-O2/-O3`) compiler for daily use (F-001). — **DONE (phase40b):** `scripts\build.bat release` → `build/release/bin/tml.exe`, measured 1.86–2.11× vs debug; MCP/daemon deliberately stay on debug (see `02-build-performance.md`).

## Index

- `01-measurements.md` — current, freshly-measured baseline
- `02-build-performance.md` — C++ compiler build cost (F-001–F-004)
- `03-check-performance.md` — type-check path (F-015, F-016)
- `04-test-framework-performance.md` — the dominant cost (F-005–F-014)
- `05-mcp-warm-state.md` — MCP discards warm state (F-017, F-018)
- `06-execution-plan.md` — phased remediation plan
