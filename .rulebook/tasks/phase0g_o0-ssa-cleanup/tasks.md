# phase0g — SSA cleanup at O0 (default builds and tests stop shipping naive IR)

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` findings
> L-060/L-068 (04-shipping-codegen.md). The single biggest felt-performance
> change available: today `tml build` defaults to O0 and the test harness never
> passes `--release`, and at O0 **zero** LLVM passes run — the alloca-spilled
> IR (~7× the instruction count of clean IR, call-per-element loops,
> `alwaysinline` never firing) ships verbatim to every default build and all
> ~12,000 tests.

## Motivation

The backend gates ALL optimization behind `optimization_level > 0`
(`compiler/src/backend/llvm_backend.cpp:383`); `tml build` defaults 0
(`dispatcher.cpp:359`), tests default 0 (`cmd_test.cpp:313`). Checked-math is a
frontend *semantic* decision currently welded to the same flag. Decoupling them
lets O0 keep debug semantics while shipping clean IR.

## 1. Implementation
- [ ] 1.1 At O0, run a curated LLVM pass set — always-inline, mem2reg/SROA,
  EarlyCSE, InstCombine, DCE — while `checked_math` stays on (keep the
  semantic flag tied to the user-facing level, not to the pass gate).
- [ ] 1.2 Escape hatch: `--no-o0-passes` (or env var) for raw-IR debugging.
  Make `--emit-ir` honest about pre-/post-pass IR (F-004): either emit
  post-pass IR or label the dump.
- [ ] 1.3 Measure and record in this file: compile-time delta on 3
  representative suites (accept ≤ +15% compile cost) and runtime delta on the
  benchmark corpus + one heavy suite's wall-clock (expect execution speedup).
- [ ] 1.4 Full quality gate green with the passes on by default.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (optimization-level semantics table: what O0
  now means; escape hatch)
- [ ] 2.2 Write tests covering the new behavior (IR fixture: param spills
  promoted at O0; checked-math still fires at O0)
- [ ] 2.3 Run tests and confirm they pass
