# Architecture Review — Decisions That Conflict With the High-Performance Goal

**Date:** 2026-07-20  
**Analysis type:** AI architecture review session  
**Topic:** Root structural conflicts in TML's compiler architecture that fight the stated Rust-class performance goal. Test speed is the primary felt symptom, but the causes are deeper.

Builds on prior analyses (does not duplicate):
- `docs/analysis/tooling-performance/` (01–06): detailed tool performance bottlenecks and tactical fixes (already landing); this review identifies the *architectural* conflicts those fixes work around.
- `docs/analysis/benchmark/08-compilation.md`: 27× slower than Rust; DLL-load breakdown and daemon measurements.
- `docs/analysis/compiler-internals/single-binary-test-compilation.md`: deep dive on the subprocess-per-test isolation requirement.

## One-line diagnosis

TML's stated performance goal is Rust-class native output (AGENTS.override T4: "TML should not exceed ~2× Rust instruction count"). The project is AI-built across ~44 phase-eras, and a handful of early structural decisions, each locally reasonable, have compounded into an architecture that actively fights that goal. The single deepest one: **there are two parallel code generators, and the optimizing one is dead code on the hot path.**

## Ranked root conflicts (highest leverage first)

| Rank | Conflict | Findings | Impact | Why it matters |
|------|----------|----------|--------|----------------|
| 1 | **Dual codegen split** | F-001, F-002, F-003, F-004 | Very High | ~50K LOC of HIR→THIR→MIR + 30 optimization passes exist, but every program with imports/generics/unions/derives routes to the 75K-LOC AST-legacy generator that those passes never touch. The "high-performance pipeline" doesn't run on real code. |
| 2 | **Subprocess-per-test isolation** | F-007 | Medium-High | Tests execute as OS subprocesses (per-suite), each loading ~100 MB of runtime DLLs. Forced by lack of in-process panic isolation, not by choice. Aggregation cut process count ~12×, but the per-spawn + DLL-load cost per suite remains and cannot be removed without language support. |
| 3 | **Embed-full-stdlib-per-EXE** | F-006 | Very High | Driven directly by #1: the shared-stdlib object was un-buildable for months because the AST path's eager monomorphization surfaces a K001 bug treadmill. Months of "optional-in" work that could not land because the hot path doesn't use it. |
| 4 | **Memory model on raw pointers with no move-tracking** | F-015, F-016, F-017 | Very High | Rust-style RAII bolted onto `*T` smart pointers with no init/move state surviving to codegen — a double-free/UAF class that took 14+ phases. Band-aids *added copies* that cost performance, the opposite of zero-cost. |
| 5 | **Frozen self-hosting compiler** | F-020 | Medium | 45K LOC of frozen compiler carried as dead weight, appearing in docs/test/build surface but neither shipped nor maintained. |

## Index

| File | Findings | Theme |
|------|----------|-------|
| `01-dual-codegen-split.md` | F-001–F-004 | The architecture that routes real programs around the optimizer |
| `02-test-speed-architecture.md` | F-005–F-011 | Test framework design forced by/enabling dual-codegen split; most tactical fixes already landed |
| `03-compiler-startup-cost.md` | F-012–F-014 | DLL load, metadata eagerness, compile-vs-Rust fixed overheads |
| `04-memory-model-foundation.md` | F-015–F-017 | RAII without move-tracking; performance cost is added copies; correctness cost is double-free/UAF |
| `05-test-reliability-tax.md` | F-018–F-019 | Nondeterministic heap corruption that defeats caching; test results themselves were unreliable |
| `06-frozen-self-host.md` | F-020 | Dead-weight self-hosting compiler still in the build/test/docs surface |
| `07-execution-plan.md` | Phases A–D | Proposed phased remediation: fix memory foundation → address dual-codegen split → attack test-speed floor → shed dead weight |

## Load-bearing files

These files are central to understanding or fixing the conflicts:

**Codegen routing gate (the dual-path decision point):**
- `compiler/src/query/query_core.cpp:800-992` — routing decision for AST vs MIR path
- `compiler/src/cli/builder/build.cpp:327,339,548` — builder-level path selection

**Memory-model foundation (F-015-F-017):**
- `compiler/src/codegen/llvm/builtins/intrinsics.cpp:854` — clone-read/drop asymmetry bug
- `compiler/src/borrow/checker.hpp:799-816` (path not verified) — borrow facts computed-then-discarded before codegen

**Test pipeline (F-005-F-011):**
- `compiler/src/testing/testing_compile.cpp`
- `compiler/src/testing/testing_compile_parallel.cpp`
- `compiler/src/testing/testing_coordinator.cpp`
- `compiler/src/testing/testing_test_cache.cpp`
- `compiler/src/cli/commands/cmd_test.cpp` / `.hpp`

**Architecture and memory-model ADRs:**
- `docs/adr/ADR-009-memory-model-soundness.md` — the dual-path divergence and its resolution
- `docs/adr/ADR-010-check-query-routing.md` — metadata preload strategy

**Active task specs (tracing back to F-016-F-019):**
- `.rulebook/tasks/phase44b_collections-standalone-heap-corruption/tasks.md`
- `.rulebook/tasks/phase44c_hand-rolled-alloc-size-lint/tasks.md`

## Key insight: The tactical/strategic split

Most *tactical* test-speed items were already fixed in phases 40–43:
- Release build (F-009) ✓
- Warm daemon, MCP connection (F-010) ✓
- Suite aggregation (F-005) ✓
- Shared-stdlib fast-path (F-006) ✓
- Content-addressed cache (F-011) ✓

What remains are the **root architectural conflicts** (above table) that these tactical fixes worked *around* instead of solving. Fixing those unlocks another 2–4× on the hot path, depending on which option is chosen for Phase B.
