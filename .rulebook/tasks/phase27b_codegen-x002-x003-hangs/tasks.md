# phase27b — X002 Hangs / X003 Crashes on Core Features (Stabilization ERA 0, Phase C2)

> Analysis: `docs/analysis/tml-table-analysis/03-codegen-stability.md` (F-006).
> A compiler that non-terminates on `let_patterns` and crashes on closures cannot
> be trusted for a large codebase. Zero tolerance for these on core features.

## 1. Implementation
- [ ] 1.1 `let_patterns` X002: profile the hang (compile with timeout + stack sample / Tracy), identify the non-terminating phase (parser? type inference? MIR pass fixpoint?), fix the root cause
- [ ] 1.2 `slice_split_pred` X002: same treatment
- [ ] 1.3 `builtins_imports` X002: same treatment
- [ ] 1.4 `other/closure_codegen` X003 crash (and its X002 mode): root-cause the crash in the closure emission path
- [ ] 1.5 `core/any` T056 standing failure: root-cause and fix (rides along — it is in the same standing-failure list)
- [ ] 1.6 Add compile-time watchdog diagnostics: when any compiler phase exceeds a time budget, emit which phase + which item is spinning (turns future X002s from mysteries into bug reports)
- [ ] 1.7 Gate: full compiler suite green ×100 consecutive runs (phase25a harness); zero timeouts, zero crashes

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
