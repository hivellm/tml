# phase26a — ADR-009: Memory-Model Soundness Decision (Stabilization ERA 0, Phase B)

> Analysis: `docs/analysis/tml-table-analysis/02-memory-model-unsoundness.md`
> (F-001..F-004, F-013) and `06-execution-plan.md` (Phase B).
> The per-site band-aid path is proven non-convergent (phase24l Attempt log).
> Pick ONE model and commit. **This ADR requires explicit user sign-off before
> phase26b implementation starts.**

## 1. Implementation
- [ ] 1.1 Verify F-013: emit MIR for `Shared::increment_count`/`decrement_count` (`lib/core/src/alloc/shared.tml:320-333`) and confirm whether the `inner: SharedInner[T]` bitwise-copy local gets drop-elaborated (would explain phase24l Attempt-2 refcount imbalance)
- [ ] 1.2 Document Option B1 (Rust-faithful): MIR move/init-state + drop-flag elaboration — per-local initialization state through the CFG, conditional drops guarded by drop flags, `.get()` becomes borrow-then-clone or explicit move; estimate compiler work (thir_mir_builder, drop.cpp, borrow checker integration)
- [ ] 1.3 Document Option B2 (ARC/Swift-style): compiler-inserted retain/release on every copy/drop of refcounted owning types; estimate runtime cost + later elision-pass work; document impact on the zero-cost performance story
- [ ] 1.4 Prototype spike for each option on the minimal repro corpus (`c_essential_repro.c`, `sig_alone.c` classes) — enough to validate feasibility claims, not production code
- [ ] 1.5 Write `docs/adr/ADR-009-memory-model-soundness.md` with context, both options, decision matrix (soundness, perf, compiler complexity, migration cost, timeline), and a recommendation
- [ ] 1.6 Present to user for decision; record the decision in the ADR + `rulebook_memory` (kind: decision)

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
