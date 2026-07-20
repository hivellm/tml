# phase0e — One integer-literal truth (single default, conflict = error)

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` finding L-001
> (01-type-system-generics.md). Probe-demonstrated silent miscompilation and
> the root of the T6 ":I64 annotation" tax.

## Motivation

Three inference engines disagree on unsuffixed integer literals: the checker
defaults **I64** (`compiler/src/types/checker/expr.cpp:409`), HIR lowering
defaults **I32** (`compiler/src/hir/hir_builder_expr.cpp:158`), the AST
codegen's own 2,832-LOC shadow-inference defaults **I32**
(`compiler/src/codegen/llvm/expr/infer.cpp`), and the spec says **I32**
(docs/specs/04-TYPES.md:19). Probes: `let a = 5` passed to both I64 and I32
parameters type-checks clean and the MIR path emits a mismatched
`call i64 f(i32 5)`; `want_i64(-5)` printed 4294967291; unannotated
`4294967296` printed 0 with no overflow error.

## 1. Implementation
- [ ] 1.1 Decide and record THE default in the proposal: I32 (spec-conformant;
  measure churn) vs I64 (low-churn; requires a spec edit). Whichever wins, all
  four sources of truth (spec, checker, HIR, codegen) must agree by the end of
  this task.
- [ ] 1.2 Literal inference: a literal's resolved width is unified across its
  uses via the existing expected-type propagation; conflicting demands (same
  literal required as I32 and I64) become a type error — never a silent
  truncation. Out-of-range for the resolved width fires the existing overflow
  diagnostic (probe: unannotated 4294967296).
- [ ] 1.3 Downstream consumers read the checker's resolved type instead of
  re-defaulting: the HIR W3 re-default (`hir_builder_expr.cpp:158,222-231`) and
  the codegen literal rule (`expr/infer.cpp`) consult the checker's
  `expr_types_` result first, own default only as last resort.
- [ ] 1.4 Probe gate: the dive-01 probes print correct values on BOTH routing
  paths; remove now-unneeded `:I64` annotations in 2-3 sample lib files to
  prove the tax is gone; update the T6 gotcha entry in AGENTS.override.md.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (docs/specs/04-TYPES.md literal rules match the
  implementation; CHANGELOG)
- [ ] 2.2 Write tests covering the new behavior (width-conflict error fixture,
  overflow fixture, cross-path consistency fixtures)
- [ ] 2.3 Run tests and confirm they pass
