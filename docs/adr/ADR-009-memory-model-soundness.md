# ADR-009: Memory-Model Soundness — Drop Elaboration for Owned Handles

**Status:** PROPOSED (awaiting user decision) · **Date:** 2026-07-15
**Deciders:** project owner + ERA 0 stabilization effort
**Evidence base:** `docs/analysis/tml-table-analysis/` (F-001..F-014),
`.rulebook/tasks/phase26a_memmodel-adr-decision/specs/groundwork/spec.md` (file:line
citations for every claim below), phase25a determinism baseline, phase24l attempt log.

---

## Context

TML ships Rust-style RAII (`Drop` + moves) but inserts drops **lexically**, with no
move/init tracking that survives to codegen, over smart pointers built on raw `*T`.
Result: a systematic double-free / use-after-free class (analysis F-001..F-004,
F-013). Fourteen phases of per-site workarounds did not converge (phase24l attempt
log: broader `.get_clone()` migration REGRESSED 30/30→25/30; language-level deep-copy
`.get()` broke 4 unrelated monomorphizations).

### What the phase26a spikes established (all confirmed at runtime, 2026-07-15)

1. **F-013 is live and catastrophic on the user path.** `Shared[T]::increment_count`
   bitwise-copies the whole `SharedInner[T]` to read a counter; drop-glue on that
   copy calls `Shared::drop` on nested handle fields, decrementing the REAL
   allocation. Runtime probe via `tml run`: nested handle count **2 → 1 → -1**
   across three `duplicate()` calls; the allocation is freed at the 0-crossing while
   its owner lives; `get()` then reads freed memory **silently** (no crash — counts
   go negative and never re-free).

2. **The test suite and real programs run DIFFERENT codegen.** Test-framework
   builds go through the query pipeline + pre-compiled stdlib object (MIR path,
   `testing_compile.cpp:69-74,356`) — no bleed, f013 corpus test passes 100/100.
   `tml run`/`tml build` of ANY stdlib-importing program falls to the AST-legacy
   path (`build.cpp:413`) — bleeds. **"12,000+ tests green" and "UzDB corrupts in
   production" are simultaneously true.** Any fix must land on both paths or make
   user builds take the tested path.

3. Supporting inventory (groundwork Q1–Q7): MIR has no first-class drop
   (`CallInst("Type::drop")` + string-matching passes); MIR `BuildContext.is_moved`
   scaffolding exists but is **never called**; the borrow checker already computes
   `OwnershipState`/init-state dataflow but discards it at AST level; `ptr_read_clone`
   (v0.3.52) is a working auto-clone-on-copy in both paths with name/decorator-driven
   detection; no "transitively owns a refcount" type query exists.

## Options

### B1 — Rust-faithful: move/init-state tracking + drop-flag elaboration in MIR

Track per-local initialization/moved-out state through the CFG; drops fire only on
initialized, non-moved values (conditional drops via flags where control-flow-dependent).
Container read-out becomes borrow-then-clone or explicit move.

- **For:** preserves zero-cost semantics; borrow checker's existing init-state
  dataflow (checker.hpp:799-816) is a ready-made algorithm to port; `is_moved`
  scaffolding is already written (unwired); makes `.get()` well-defined.
- **Against / risks:** drop is not a first-class MIR node (touches instruction set,
  builder, ≥3 string-matching passes); **must be implemented twice** (AST path is
  what users run today) or the AST fallback must be retired first; borrow-checker
  facts are blind below `lowlevel { *this.ptr }`, so new raw-pointer-aware dataflow
  is still needed for stdlib internals.

### B2 — ARC/Swift-style: compiler-inserted retain/release

Owning refcounted types get retain on every copy, release on every drop, inserted by
codegen; no hand-written `.duplicate()` discipline.

- **For:** simpler soundness story; `ptr_read_clone` proves the insertion machinery
  works in BOTH paths today; removes the entire "did you clone before moving" class.
- **Against / risks:** the "transitively owns a refcount" predicate must be built
  from scratch (current state of the art is a hardcoded name table,
  instructions_call.cpp:1008-1040); the retain/release primitives themselves are
  TML source that is unsound today (F-013) and must become intrinsics first;
  redefines the performance model (refcount traffic on every copy until elision
  passes exist); stdlib-wide semantic migration.

### B3 — Path unification first: retire the AST-legacy fallback, then B1 on the single path

Finish MIR's imported-module + generics support so `build.cpp` always uses the
query/MIR pipeline (exactly what the test framework already uses), THEN implement
B1's move/drop-flag elaboration once, on the only remaining path.

- **For:** kills the tests-vs-production divergence — the single highest-leverage
  fact from the spikes: users start running the code path that 12,000+ tests
  already validate, **including the no-bleed drop elaboration** (the F-013 bleed
  disappears from user builds as a side effect of unification, before B1 even
  lands); B1 is then implemented ONCE instead of twice; every future codegen fix
  stops being a dual-path tax. The phase25b verifier + phase25a determinism gates
  protect the migration.
- **Against / risks:** "MIR codegen doesn't support imported module functions /
  generic instantiation" (build.cpp:320-335) is the reason the fallback exists —
  closing that gap is real work (monomorphization of imported generics in the query
  pipeline); risk that MIR-path drop elaboration has its own latent holes currently
  masked (it must gain B1's discipline, not be assumed sound); AST path removal
  needs a deprecation window gated by the full suite + determinism corpus.

## Decision matrix

| Criterion | B1 (drop-flags, dual-path) | B2 (ARC) | B3 (unify → B1 once) |
|---|---|---|---|
| Closes F-001..F-004 (aliasing/double-free) | Yes | Yes | Yes (unification removes the live bleed; B1 pass closes the class) |
| Closes F-013 (refcount core) | Needs intrinsic fix too | Needs intrinsic fix first | Same fix, once |
| Fixes tests-vs-production divergence | No (both paths stay) | No | **Yes — by construction** |
| Implementation surface | 2× (AST + MIR) | 2× + new type query + perf model change | 1× + MIR feature-gap closure |
| Perf model | Zero-cost preserved | Refcount traffic on copies | Zero-cost preserved |
| Leverages existing half-built infra | is_moved, init-state dataflow | ptr_read_clone, get_clone/get_ref | All of B1's, plus the already-tested MIR path |
| Interim mitigations available | ptr_read_clone stays | — | ptr_read_clone stays; F-013 point-fix in shared.tml (read counter via field ptr, no whole-inner copy) |
| Biggest risk | Dual-path drift forever | Predicate + migration breadth | MIR generics/imports gap is underestimated |

## Recommendation

**B3.** The spike evidence makes the dual-path situation the dominant problem: the
bug class users hit lives precisely in the path tests do not exercise. Fixing B1
twice entrenches that divergence; B2 rebuilds its machinery twice too. Unification
makes user builds run the already-tested pipeline (immediately removing the F-013
bleed from production builds), then B1's move/drop-flag elaboration lands once on
one path — with the phase25 harness + verifier as migration gates.

Sequencing under B3 (phase26b restructure):
1. **Immediate mitigation (independent of decision):** fix
   `increment_count`/`decrement_count`/`take` in `shared.tml` to read/write the
   counter through a field pointer instead of copying `SharedInner[T]` — removes
   the live production bleed at its worst site.
2. Close MIR's imported-module + generic-instantiation gaps (build.cpp:320-407
   conditions) behind a flag; flip `tml build`/`run` to the query/MIR pipeline;
   gate with determinism corpus ×100 + full suite + verifier.
3. Retire the AST-legacy emission path (keep `--emit-ir` faithful to the LIVE path
   — today it lies about what executes).
4. Land move/init-state + drop-flag elaboration in MIR (B1 mechanics, once):
   first-class `DropInst`, wire `mark_moved`, port the borrow checker's init-state
   merge down to MIR, extend below `lowlevel` for stdlib internals.
5. phase26c: revert ALL phase24 band-aids; gates `essential.c` ×100 = 100/100
   adversarial, collections suites K001-free.

## Consequences

- `docs/analysis/tml-table-analysis/06-execution-plan.md` Phase B is refined:
  26b becomes "unify + elaborate" rather than "elaborate twice".
- `--emit-ir` output becomes trustworthy again (it currently shows IR from a path
  users run but tests don't — after unification both agree).
- The 4 phase27a K001 specimens found while building the corpus remain owned by
  phase27a; unification will likely subsume several (they are AST-path
  monomorphization bugs).

## Decision

_Pending user sign-off. Options: **B3 (recommended)** · B1 · B2 · other._
