# phase26a — ADR-009: Memory-Model Soundness Decision (Stabilization ERA 0, Phase B)

> Analysis: `docs/analysis/tml-table-analysis/02-memory-model-unsoundness.md`
> (F-001..F-004, F-013) and `06-execution-plan.md` (Phase B).
> The per-site band-aid path is proven non-convergent (phase24l Attempt log).
> Pick ONE model and commit. **This ADR requires explicit user sign-off before
> phase26b implementation starts.**

## 1. Implementation
- [x] 1.1 Verify F-013 — CONFIRMED at IR level (emit-ir, 2026-07-15): `Shared[T]::increment_count` bitwise-copies the whole `SharedInner[T]` to a stack alloca and its drop-glue calls `Shared[U]::drop` on nested handle fields of the COPY, which decrements the REAL allocation and `mem_free`s at 0 — every `duplicate()` leaks one real decrement per nested handle on this path. Evidence + IR excerpts in `specs/groundwork/spec.md` (Q5). NEW decision-critical finding: the f013 corpus test does NOT bleed at runtime (100/100), so the emit-ir (AST-legacy) path and the test binary's path elaborate this drop DIFFERENTLY — identifying which subsystem is live in practice is now a mandatory spike input (folded into 1.4)
- [x] 1.2 Option B1 documented (ADR-009 + groundwork Q1/Q4/Q6): MIR drop-flags, ready-made init-state dataflow in the borrow checker, dead `is_moved` scaffolding, dual-path implementation burden as the key risk
- [x] 1.3 Option B2 documented (ADR-009 + groundwork Q3/Q7): `ptr_read_clone` as working embryo in both paths, missing "owns-refcount" type query, F-013-unsound retain/release primitives, perf-model change
- [x] 1.4 Spikes executed — findings changed the option space: (a) runtime probes prove the F-013 bleed is REAL on the user path (`tml run`: nested count 2→1→-1, silent UAF reading freed memory); (b) discriminant is the BUILD PATH: test framework compiles via query/MIR pipeline with shared stdlib object (`testing_compile.cpp:69-74,356`) and does NOT bleed, while `tml run/build` of any stdlib-importing program takes the AST-legacy fallback (build.cpp:413) and bleeds — **the test suite validates a path real programs never run**. This surfaced option B3 (unify paths first, then B1 once)
- [x] 1.5 `docs/adr/ADR-009-memory-model-soundness.md` written — context, THREE options (B1/B2/B3), decision matrix, recommendation **B3** with sequencing (immediate shared.tml counter-read mitigation → MIR gap closure + path flip → AST retirement → drop-flag elaboration once → band-aid revert)
- [x] 1.6 Presented to user 2026-07-15 — **B3 ACCEPTED** (unify on query/MIR pipeline, then drop-flag elaboration once). ADR-009 status updated to ACCEPTED with sequencing; decision recorded in rulebook (decision #10, slug adr-009-b3-...); phase26b restructured accordingly

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Update or create documentation covering the implementation — `docs/adr/ADR-009-memory-model-soundness.md` (ACCEPTED) + `specs/groundwork/spec.md` (Q1–Q7 evidence + spike results with runtime numbers)
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
