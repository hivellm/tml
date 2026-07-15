# Analysis: TML State-of-the-Language — Why UzDB Failed and What Blocks Real Apps

**Slug:** `tml-table-analysis`
**TML version analyzed:** 0.3.52 · **Branch:** `feat/self-hosting-compiler` · **Date:** 2026-07-15
**Motivation:** UzDB (`E:\UzmiGames\UzDB`) — a DB-as-server for MMORPG backends — was abandoned
and rewritten in Rust because TML could not sustain a real application. This analysis maps
every problem class standing between TML today and "a usable language", with evidence, and
defines the corrective roadmap.

Findings are numbered globally F-001..F-014. Confidence ∈ {High, Medium, Low}.

---

## Bottom line

TML at 0.3.52 is a **language with excellent design and a genuinely broad stdlib, sitting on
top of an unsound memory-management foundation and an unstable codegen backend.** The tooling
problems that originally blocked UzDB at 0.1.6 (pipe hangs, stderr routing, MCP crashes,
`match` ICE) have largely been **fixed**. What remains — and what would sink any real
application today — is deeper:

1. **The memory model is unsound in practice.** TML ships Rust-style RAII (`Drop` + move
   semantics) but the compiler inserts drops by lexical scope over smart-pointer types
   (`Heap[T]`, `Shared[T]`) and collections (`HashMap`, `List`, `BTreeMap`) that are built on
   raw `*T` pointers. `.get()` returns a **bitwise copy** that aliases nested owned handles
   without bumping refcounts. The result is a systematic **double-free / use-after-free bug
   class** producing **non-deterministic SIGSEGVs**. The compiler's own self-hosted C-frontend
   has spent **~14 consecutive phases (24a→24n, ~30 commits)** fighting this and still cannot
   self-compile a 1465-line C file.

2. **Codegen emits invalid LLVM IR (K001) and hangs/crashes (X002/X003) on valid programs**,
   persistently, across multiple stdlib collection suites.

3. **These two problems compound**: any non-trivial data-structure-heavy program (which a
   database is, by definition) hits them immediately.

UzDB never got past design + 8 spike programs. Abandoning it was the correct call at the time:
the exact workloads it needed (BTreeMaps holding rows, MVCC snapshot copies, refcounted shared
state, tight allocation/free loops) are precisely the workloads TML's memory model mishandles
today.

## Index

| File | Theme | Findings |
|------|-------|----------|
| [01-context-uzdb-failure.md](01-context-uzdb-failure.md) | What UzDB was, the 0.1.6→0.3.52 timeline, what got fixed | F-011, F-012 |
| [02-memory-model-unsoundness.md](02-memory-model-unsoundness.md) | **Core defect**: RAII drop over raw-pointer smart pointers | F-001, F-002, F-003, F-004, F-013 |
| [03-codegen-stability.md](03-codegen-stability.md) | Invalid IR (K001), timeouts (X002), crashes (X003), non-determinism, dispatch/mono bugs | F-005, F-006, F-007, F-008 |
| [04-self-hosting-and-strategy.md](04-self-hosting-and-strategy.md) | compiler-tml stuck; ambition-vs-foundation mismatch | F-009, F-014 |
| [05-tooling-stdlib-gaps.md](05-tooling-stdlib-gaps.md) | Package manager, remaining app-capability gaps | F-010, F-012 |
| [06-execution-plan.md](06-execution-plan.md) | Phased corrective roadmap (A–E) with exit gates | — |
| [07-determinism-baseline.md](07-determinism-baseline.md) | Measured crash-rate baseline ×100, normal + adversarial allocator (phase25a) | — |
| [08-memory-copy-audit.md](08-memory-copy-audit.md) | Stdlib+codegen audit of the copy-instead-of-move class: no move semantics, 13 UAF sites, pass-by-value, perf | F-015..F-022 |

## Key evidence files (absolute paths)

- `lib/core/src/alloc/shared.tml` — `Shared[T]`; docstring at lines 118–150 explicitly names
  "the `Shared.get` aliasing class of bug (phase24k diagnosis)".
- `lib/core/src/alloc/heap.tml` — `Heap[T]`; `impl Drop` (263–270), `into_raw` null-the-pointer
  workaround (231–236).
- `compiler/src/codegen/llvm/core/drop.cpp` — scope-based drop insertion.
- `.rulebook/tasks/phase24l_shared-get-aliasing-deep-fix/tasks.md` — the decisive "Attempt log"
  (three fix strategies, two reverted with regressions).
- `.rulebook/PLANS.md` — project's own root-cause statement + persistent-failure list
  (K001/X002/X003 standing failures, lines 56–60).
- `CHANGELOG.md` — the phase24a..24n grind, versions 0.3.39–0.3.52.
- `E:\UzmiGames\UzDB\docs\letter-to-tml-dev.md` — the original 0.1.6 feedback.
- `E:\UzmiGames\UzDB\docs\specs\00-gaps-analysis.md` — what UzDB needed from a DB engine.

## Findings at a glance

| ID | Finding | Confidence | Impact |
|----|---------|------------|--------|
| F-001 | RAII `Drop` inserted by lexical scope, not ownership/move analysis, over raw-pointer smart pointers | High | Critical |
| F-002 | `.get()` returns bitwise copy aliasing nested owned handles without refcount bump | High | Critical |
| F-003 | Bug class not localizable; ~14 phases of band-aids failed to close it | High | Critical |
| F-004 | Borrow checker (NLL + Polonius) exists but is blind to this class | Medium-High | High |
| F-005 | Codegen emits invalid LLVM IR (K001), persistently | High | High |
| F-006 | Compiler hangs (X002) / crashes (X003) on valid core-feature programs | High | High |
| F-007 | Generic monomorphization / method-dispatch mangling fragile | High | High |
| F-008 | Crashes are non-deterministic (heap-layout dependent) | High | Critical (adoption) |
| F-009 | Self-hosted compiler cannot compile a real input; native-backend roadmap gated on it | High | High |
| F-010 | No production package registry | Medium | Medium-High |
| F-011 | Original UzDB 0.1.6 tooling blockers are RESOLVED | High | — (progress) |
| F-012 | Remaining app-capability gaps are modest (async I/O maturity, registry) | Medium-High | Medium |
| F-013 | `Shared::increment_count`/`decrement_count` bitwise-copy the whole inner value | Medium | Medium-High |
| F-014 | Roadmap ambition (DB drivers, CUDA, LoRA) dramatically exceeds foundation readiness | High | Strategic |
| F-015 | No real move semantics at codegen (mark_moved dead, consumed_vars_ syntactic, borrow facts discarded) | High | Critical |
| F-016 | 13 confirmed double-free/UAF sites (unfixed F-013/F-002 siblings across alloc/collections/iterators) | High | Critical |
| F-017 | 2 move-outs double-free unconditionally (`Arc::try_unwrap`, `AnyValue::into_inner`) — fixable now | High | Critical |
| F-018 | `Sync[T]::get` copies with NO safe alternative (no get_ref/get_clone) | High | High |
| F-019 | Read/iterate asymmetry: `get` deep-clones (sound), iterators alias (unsound) | High | Medium-High |
| F-020 | Pass-by-value MUST-BORROW (phase24b class alive): BigInt, str::join, HTTP/2 — one-token `ref` fixes | High | High |
| F-021 | Collections have no borrow accessor; `get` returns owned deep clone (language gap — the zero-cost blocker) | High | Critical |
| F-022 | `List`/`HashMap` `destroy` skip per-element Drop → leak every handle (mirror of copy hazard) | High | High |
| F-023 | `Shared`/`Sync` `try_unwrap` free ignoring weak refs → UAF (distinct class, fixable now) | High | High |
