# phase0d — `when` exhaustiveness as a real `check` diagnostic

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` finding L-043
> (03-frontend-grammar.md). The language's core idiom (Maybe/Outcome + `when`)
> is unenforced everywhere users look — probe-proven.

## Motivation

The only exhaustiveness engine lives in THIR
(`compiler/src/thir/exhaustiveness.cpp`, 623 LOC) — the layer the shipping AST
path bypasses. Probes with a missing `Color::Blue` arm: `tml check` → "Type
check passed" (silent); MIR-path `run` → a timestamped `WARN [thir]` logger
line with no span/code and the program compiles and runs exit 0; AST-path run →
zero output. Rust makes this a hard error (E0004).

## 1. Implementation
- [ ] 1.1 Invoke the exhaustiveness engine from the type-check phase (the
  `when` handling in `compiler/src/types/checker/`) so it runs for every
  program on every path, under `tml check`, before codegen routing.
- [ ] 1.2 Real diagnostic: stable code in the T-range, span on the `when`,
  missing-variant list, fix-it suggesting the missing arms or an `else` arm;
  deny by default.
- [ ] 1.3 Migration: run the full suite + lib; fix every true positive (a
  missing arm is a real bug). If volume demands, add a one-release
  `--warn-non-exhaustive` downgrade flag — the default stays deny. Record the
  count found.
- [ ] 1.4 Single enforcement point: remove or redirect the THIR-side WARN
  demotion (`compiler/src/query/query_core.cpp:656-658`) so the check-phase
  diagnostic is the only voice.
- [ ] 1.5 Probe gate: missing-arm probe fails `check` with the new code
  regardless of routing path; adding the arm or `else` passes.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (pattern-matching spec: exhaustiveness is
  enforced; new error code in the explain registry)
- [ ] 2.2 Write tests covering the new behavior (enums, Maybe/Outcome, guards,
  nested patterns, `else` satisfying exhaustiveness)
- [ ] 2.3 Run tests and confirm they pass
