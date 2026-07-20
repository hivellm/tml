# Language Deep Review — Structure & Implementation of the Whole Language

**Date:** 2026-07-20 (complete — all 8 deep-dives + consolidated synthesis)
**Analysis type:** AI architecture + implementation review (8 parallel subsystem deep-dives, evidence-backed with file:line refs and compiled probes)
**Start here:** `09-consolidated-priorities.md` — cross-cutting patterns, corrections to the prior review, priority tiers, and the four decisions (D1–D4) that need a user call.
**Companion:** `docs/analysis/architecture-performance-review/` (findings F-001..F-020) established the root performance conflicts. This review goes one level deeper: full-language structure AND current-implementation detail. Findings here are numbered **L-001..L-159**, one reserved range per subsystem, so they compose with the F-series into a single namespace.

## Scope & method

Eight parallel deep-dives, each auditing **design** (is the decision right for a high-performance, Rust-class language?) and **implementation** (is the current code sound/clean/fast?), with mandatory evidence. Where useful, agents compiled tiny probe programs (`.sandbox/`, cleaned up) and measured real IR / real timings instead of trusting docs.

| # | File | Subsystem | Findings | Status |
|---|------|-----------|----------|--------|
| 01 | `01-type-system-generics.md` | Type system & generics | L-001..L-011 | **done** |
| 02 | `02-memory-model-borrow.md` | Memory model, ownership, borrow checker | L-020..L-030 | **done** |
| 03 | `03-frontend-grammar.md` | Lexer, parser, grammar, AST, HIR/THIR, diagnostics | L-040..L-048 | **done** |
| 04 | `04-shipping-codegen.md` | The AST-legacy LLVM codegen (the path that ships) | L-060..L-068 | **done** |
| 05 | `05-stdlib-implementation.md` | lib/core + lib/std implementation | L-080..L-088 | **done** |
| 06 | `06-runtime-concurrency.md` | Runtime, panic model, threads, atomics, allocator, FFI | L-100..L-108 | **done** |
| 07 | `07-incremental-cpp-health.md` | Query/incremental system + C++ codebase health | L-120..L-128 | **done** |
| 08 | `08-design-coherence.md` | Design coherence, spec drift, feature gap | L-140..L-148 | **done** |
| 09 | `09-consolidated-priorities.md` | Cross-cutting synthesis + updated execution plan | P1–P5, Tiers 0–4, D1–D4 | **done** |

## Headline findings so far (one-liners — see the numbered files)

- **L-060** — Default builds AND all ~12,000 tests run at O0 with **zero LLVM passes**: the naive IR ships verbatim; `--emit-ir` dumps pre-pass IR even with `-O2`.
- **L-061** — Measured on identical source: the dead MIR path emits 4 instructions where the live AST path emits ~28 (5 allocas, 7 blocks) — every parameter is spilled to a fresh alloca.
- **L-062/L-063** — Zero pointer-parameter attributes (`noalias`/`readonly`/`dereferenceable`) and zero TBAA metadata across the 75K-LOC shipping codegen — the two structural blockers to LLVM `-O2` recovering the naive IR on aggregate/collection code.
- **L-065** — Enum `duplicate` is a shallow bitwise copy: `Maybe[Shared[T]]`-class clones skip the refcount bump → latent double-free (the F-016 class, reopened for enums).
- **L-080** — Collections are type-erased `*Unit` blobs with literal header sizes (`mem_alloc(32)`/`mem_alloc(48)` + hand-computed offsets) — the phase44c drift bug-class, systematized library-wide.
- **L-081** — HashMap has Swiss-Table control bytes but probes them one byte at a time; the SIMD group-scan is written, measured 2.25× faster, and **disabled** by a generic+SIMD codegen bug. Tombstones are never reclaimed (O(capacity) cliff under churn).
- **L-082/L-083** — `List::sort` is a last-pivot quicksort (O(n²) + stack overflow on *sorted* input); `BTreeMap` is not a B-tree (sorted parallel arrays, O(n) insert).
- **L-084** — Iterators are eager clone-snapshots: Deque/BTreeMap/HashMap deep-copy the whole collection before yielding the first element (F-016 generalized; root cause = `List` is move-only so borrowing iterators can't exist yet).
- **L-102** — The entire `std::sync::atomic` family is **fake**: plain non-atomic loads/stores, `Ordering` ignored; std `Arc`'s refcount is a data race by construction. (Real seq_cst I32 atomics exist as builtins; a second, correct Arc exists in core.)
- **L-101/L-105** — In-process panic/crash isolation machinery **already exists** (setjmp + VEH + context-restore, used per-test today); thread-level isolation needs ~15 statics made thread-local + a pool dispatcher (≈1.5–2.5 weeks), not an EH project. And the "~100 MB DLLs per test process" premise is stale: test EXEs are 0.3–0.7 MB static binaries, spawn measured at ~11–30 ms.
- **L-120..L-122** — "Red-green incremental compilation" is real for exactly 1 of 9 query kinds (whole-file IR-text replay); intermediate fingerprints are decorative; `check` — the hottest operation — bypasses the query system entirely (ADR-010 rejected re-routing it).
- **L-124** — The pipeline's interchange format is LLVM IR **text** end-to-end: hand-emitted strings, `const char*` across the plugin ABI, re-parsed on every compile, cached as 9,720 `.ll` files / 2.2 GB.
- **L-126** — Four generations of lowering code ship in the DLL; two (~5.2K lines) are dead but still compiled, contradicting the docs' claim that the legacy HIR→MIR path "was removed".
- **L-001** — Three independent type-inference engines with *conflicting* integer defaults (checker=I64, HIR/codegen=I32); probe-demonstrated silent miscompilation (`-5` prints `4294967291` on the MIR path).
- **L-020/L-022** — Move checking is implemented but **off by default** (env var, measured zero blast radius); smart pointers (`Heap`/`Shared`/`Sync`) **never run their payload's destructor** because `drop_in_place` is codegen-broken (both runtime-proven).
- **L-043** — `when` exhaustiveness is checked only in THIR: silent under `tml check`, a log line on the MIR path, absent on the shipping AST path — a missing enum arm compiles and runs today.
- **L-044/L-048** — Type results are parked in a `void*`-keyed map and re-derived by 4 independent inference implementations; every piece of sugar is lowered twice (or interpreted 4-5×) because there is no single desugaring point.
- **L-046** — A second, C#-style object model (~14 keywords: `class`/`interface`/`virtual`/…) was added against RFC-0002 and taxes every layer of the pipeline.
- **L-140..L-143** — Spec front-door examples don't parse (`module`, "unified `loop`"); a full macro system in the spec is unimplemented fiction; `@inline(never)` silently emits `alwaysinline` — the opposite of its documentation.

## How to read this review

Each numbered file is self-contained: findings with Impact/Confidence/Layer, evidence, why it conflicts (or doesn't) with the ≤2× Rust goal, and a recommendation — followed by a subsystem **Verdict**, a **Keep** list (decisions that are good and should survive any refactor), and the agent's **Top 3** highest-leverage recommendations. The consolidated, cross-cutting priority ranking lands in `09-consolidated-priorities.md` once all eight dives are in.
