# Proposal: Harden types_compatible to Verify Behavior Conformance

## Why

`types_compatible` in `compiler/src/types/checker/helpers.cpp:91-228` accepts ANY `NamedType` as implementing ANY `ImplBehaviorType` without checking whether the type actually has the behavior registered in `behavior_impls_`. This is documented as bug **B-05** in `docs/specs/typechecker-invariants.md` Appendix B and cross-cutting invariant **CC-16** in Section 5.

The bug is load-bearing: real conformance verification happens during Phase 3 (impl processing) and the body checker relies on the permissive `types_compatible` to avoid ordering cycles. Adding verification naively will reject programs that currently compile because Phase 3 hasn't finished when body checking asks the question.

A correct fix must coordinate type checker phases or defer the check to a point where `behavior_impls_` is fully populated. This is non-trivial and high-risk — the last time someone hardened conformance checks, it took two weeks to unwind the regressions.

The fix is mandatory for self-hosting because a TML-written type checker that verifies conformance will reject real bugs that currently slip through, so the C++ checker and TML checker will diverge on the same input.

## What Changes

1. **Deferred verification pass**: add a new pass after body checking that walks all `ImplBehaviorType` bounds encountered during checking and verifies each type actually implements the claimed behavior via `behavior_impls_` or the superbehavior chain. Emit T-series errors for violations.

2. **Alternative — on-demand strict check at call boundaries**: keep `types_compatible` permissive for local type matching, but at every function call site where an argument is declared as `impl Behavior`, run the strict check against the concrete argument type. Lower risk, less complete.

3. **Regression corpus**: before changing anything, run the full test suite with debug instrumentation that logs every time `types_compatible` returns `true` due to the permissive bypass. Save the log. After fixing, ensure every log entry corresponds to a genuinely correct program or is now an intentional error.

4. **Migration strategy**: land the fix behind a feature flag (`--strict-behavior-check`) for a sprint, fix each flagged test, then flip the default.

## Impact

- **Affected specs**: `docs/specs/typechecker-invariants.md` (CC-16, B-05, Section 6 contract), `docs/specs/28-CHECKER.md`
- **Affected code**: `compiler/src/types/checker/helpers.cpp`, `compiler/src/types/checker/core.cpp` (new pass), possibly `compiler/src/types/env_lookups.cpp`
- **Breaking change**: YES — programs that rely on the permissive check will get new compile errors. Needs the regression-corpus strategy.
- **User benefit**: real type safety for behavior bounds; prevents downstream GEP_UNSIZED errors that RC6/phase0i chased after the fact.
