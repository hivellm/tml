# phase26b — Memory-Model Implementation (Stabilization ERA 0, Phase B)

> Implements the model chosen in ADR-009 (phase26a). Checklist below assumes
> either option; refine after the ADR decision. Analysis:
> `docs/analysis/tml-table-analysis/02-memory-model-unsoundness.md` + `06-execution-plan.md`.

## 1. Implementation
- [ ] 1.1 Land the core mechanism per ADR-009 (B1: move/init-state tracking + drop-flag elaboration in MIR; B2: auto retain/release insertion in codegen for refcounted owning types)
- [ ] 1.2 Make container read-out well-defined: `Shared/Heap/HashMap/List/BTreeMap.get` semantics are sound by construction (no bitwise-copy aliasing of nested owned handles) — supersedes the `ptr_read_clone` conservative-detection intrinsic
- [ ] 1.3 Fix the refcount machinery itself: `Shared::increment_count`/`decrement_count` must not bitwise-copy `SharedInner[T]` (F-013)
- [ ] 1.4 Extend the borrow checker (NLL/Polonius) or MIR lowering so the stdlib's `lowlevel` raw-pointer internals are covered by the model (F-004) — drop insertion must never fire on moved-from/aliased values
- [ ] 1.5 Determinism gates (uses phase25a harness, adversarial mode ON): `sig_alone.c` 100/100, `c_essential_repro.c` 100/100 (was 28/30), `essential.c --emit=ast` 100/100 (was 0/5)
- [ ] 1.6 No regressions: compiler suite + lib/core + lib/std baselines preserved or improved; document any suite deltas with root cause

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
