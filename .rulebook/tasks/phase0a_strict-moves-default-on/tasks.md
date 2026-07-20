# phase0a — Enforce move semantics by default (strict moves on)

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` findings L-020/L-021
> (02-memory-model-borrow.md). Part of the phase0 series — execute in
> alphabetical order. First because it is the highest soundness-per-effort item
> in the review: the check already exists, its blast radius was measured at
> **zero over 183 files** (ADR-009, v0.3.65), and it was simply never turned on.

## Motivation

With `TML_STRICT_MOVES` unset (the default), every move-derived diagnostic
(B001/B002/B005, assign-after-move) is suppressed. Probes proved the failure
modes users get instead: use-after-move dies in codegen with
`Unknown variable:` internal errors, double-consume produces "generated LLVM IR
is invalid", and on the MIR routing path `let b = a` **double-drops** at
runtime (prints `drop R` twice). The language's core ownership contract is
implemented but unenforced.

## 1. Implementation
- [ ] 1.1 Flip the default: `strict_moves_` starts true
  (`compiler/src/borrow/checker_core.cpp:79-91`, flag decl
  `compiler/include/borrow/checker.hpp:969-976`). The env var becomes an
  opt-OUT escape hatch (`TML_STRICT_MOVES=0`) kept for one release; update the
  flag comment and CHANGELOG.
- [ ] 1.2 Full quality gate (check → lint → full suite). Expected fallout ≈ 0
  per the ADR-009 measurement; fix anything that surfaces — a failure here is a
  real latent move bug, not a reason to revert.
- [ ] 1.3 Close the MIR double-drop hole (L-021) with the smallest sound diff:
  either (a) wire the borrow facts / `mark_moved` (exists with zero call sites,
  `compiler/include/mir/mir_builder.hpp:68,92-96`) into `thir_mir_builder` so a
  moved-from local is not dropped again, or (b) extend the routing gate
  (`compiler/src/query/query_core.cpp:992`) to send any program containing a
  droppable local to the AST path until MIR unification. Record which option
  was taken and why.
- [ ] 1.4 Probe gate: `type R` with `impl Drop`; `let a = R{..}; let b = a`
  prints `drop R` exactly once on BOTH routing paths. Convert the two
  probe-confirmed internal-error cases (use-after-move, double-consume) into
  fixtures asserting real B-diagnostics instead of codegen errors.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation: move semantics documented as enforced
  (docs/specs/06-MEMORY.md), env-var escape hatch documented as deprecated
- [ ] 2.2 Write tests covering the new behavior (B001/B002/B005 fire by
  default; single-drop probes on both paths)
- [ ] 2.3 Run tests and confirm they pass
