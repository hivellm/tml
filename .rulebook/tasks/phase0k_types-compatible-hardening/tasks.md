# Tasks: Harden types_compatible to Verify Behavior Conformance

**Status**: Planned (0/14)
**Depends on**: None (but recommend landing phase0j first to stabilize type args)
**Blocks**: Self-hosting type checker parity (phase12 Era 1)
**Duration**: 1–2 weeks (high risk, needs migration corpus)
**Risk**: HIGH — breaking change; must coordinate with regression corpus
**Related bug**: B-05, CC-16 in `docs/specs/typechecker-invariants.md`

---

## Investigation

- [ ] I.1 Read `compiler/src/types/checker/helpers.cpp:91-228` — understand every branch that returns `true` for `ImplBehaviorType`.
- [ ] I.2 Identify every call site of `types_compatible` in the type checker. Categorize: (a) local type matching inside bodies, (b) function argument matching, (c) return type coercion, (d) pattern binding.
- [ ] I.3 Read `compiler/src/types/env_lookups.cpp::type_implements` — the strict version. Confirm it checks `behavior_impls_` + superbehavior chain.

## Instrumentation Phase

- [ ] N.1 Add debug logging to `types_compatible` that fires when it returns `true` via the permissive `ImplBehaviorType` branch. Log: (calling function, concrete type, claimed behavior).
- [ ] N.2 Run full test suite with instrumentation enabled. Save log to `.sandbox/types_compatible_bypass.log`.
- [ ] N.3 Categorize log entries: genuine bypass needed (ordering), should-fail-but-slips-through, already-covered-by-another-check.

## Fix Implementation

- [ ] F.1 Add `--strict-behavior-check` flag (default off) to the compiler CLI.
- [ ] F.2 When flag is on, replace the `ImplBehaviorType` permissive branch in `types_compatible` with a call to `type_implements`.
- [ ] F.3 Add a deferred verification pass in `check_module` that runs after Phase 4 (body checking). Walk all `ImplBehaviorType` bounds recorded during body checking and verify each concrete type has the claimed impl.
- [ ] F.4 Record `ImplBehaviorType` verification points during body checking in a side table so the deferred pass knows what to verify.

## Test Corpus Regression

- [ ] R.1 Run full test suite with `--strict-behavior-check` enabled. Expect failures.
- [ ] R.2 For each failure: classify as (a) real bug the test should expose, (b) genuine program the checker now wrongly rejects, (c) missing impl that should be added.
- [ ] R.3 Fix category (c) by adding the missing impls. Fix category (b) by refining the strict check (likely narrow the error case). File individual bugs for category (a).
- [ ] R.4 Re-run with flag enabled until all failures are resolved.

## Flip Default

- [ ] D.1 Change the flag default from off to on.
- [ ] D.2 Remove the permissive branch entirely.
- [ ] D.3 Remove the flag.

## Verification

- [ ] V.1 Build via `scripts\build.bat`.
- [ ] V.2 Full test suite via `mcp__tml__test` `structured=true`. Confirm no regressions.
- [ ] V.3 Manually construct a test that should fail strict conformance and verify it errors.

## Documentation

- [ ] DC.1 Update `docs/specs/typechecker-invariants.md`: remove B-05 and CC-16 from Appendix B / Section 5 "surprising findings", update Section 6 contract to require strict behavior verification.
- [ ] DC.2 Commit with conventional message: `fix(types): verify behavior conformance in types_compatible (phase0k)`.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
