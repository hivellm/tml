# Tasks: Harden types_compatible to Verify Behavior Conformance

**Status**: Complete (14/14)
**Depends on**: None (but recommend landing phase0j first to stabilize type args)
**Blocks**: Self-hosting type checker parity (phase12 Era 1)
**Duration**: 1–2 weeks (high risk, needs migration corpus)
**Risk**: HIGH — breaking change; must coordinate with regression corpus
**Related bug**: B-05, CC-16 in `docs/specs/typechecker-invariants.md`

---

## Investigation

- [x] I.1 Read `compiler/src/types/checker/helpers.cpp:91-228` — understand every branch that returns `true` for `ImplBehaviorType`.
- [x] I.2 Identify every call site of `types_compatible` in the type checker. Categorize: (a) local type matching inside bodies, (b) function argument matching, (c) return type coercion, (d) pattern binding.
- [x] I.3 Read `compiler/src/types/env_lookups.cpp::type_implements` — the strict version. Confirm it checks `behavior_impls_` + superbehavior chain.

## Instrumentation Phase

- [x] N.1 Add debug logging to `types_compatible` that fires when it returns `true` via the permissive `ImplBehaviorType` branch. Log: (calling function, concrete type, claimed behavior).
- [x] N.2 Run full test suite with instrumentation enabled. Save log to `.sandbox/types_compatible_bypass.log`.
- [x] N.3 Categorize log entries: genuine bypass needed (ordering), should-fail-but-slips-through, already-covered-by-another-check.

## Fix Implementation

- [x] F.1 Add `--strict-behavior-check` flag (default off) to the compiler CLI.
- [x] F.2 When flag is on, record `ImplBehaviorType` bypass points in the side table; the post-body verification pass calls `type_implements` for each.
- [x] F.3 Add a post-body verification pass in `check_module` that runs after Phase 4 (body checking). Walk all `ImplBehaviorType` bounds recorded during body checking and verify each concrete type has the claimed impl.
- [x] F.4 Record `ImplBehaviorType` verification points during body checking in a side table so the post-body pass knows what to verify.

## Test Corpus Regression

- [x] R.1 Run full test suite with strict check enabled (default-on after D.1). No new T099 failures — all 49 failures are pre-existing codegen bugs unrelated to phase0k.
- [x] R.2 Classified: all failures are category (a) pre-existing LLVM IR codegen bugs (Maybe[I32] layout, Pin, Outcome type issues). No category (b) or (c) failures.
- [x] R.3 No category (c) impls needed. No category (b) refining needed.
- [x] R.4 Full suite clean — 0 T099 errors against existing test corpus.

## Flip Default

- [x] D.1 Change the flag default from off to on.
- [x] D.2 Remove the permissive branch entirely (removed `if (strict_behavior_check)` guard — always records bypass points).
- [x] D.3 Remove the flag (from common.hpp, dispatcher.cpp check/build/run sections, cmd_test.cpp).

## Verification

- [x] V.1 Build via `scripts\build.bat`.
- [x] V.2 Full test suite via core suite. 777 passed, 49 pre-existing failures, 0 new T099 regressions.
- [x] V.3 Manually construct a test that should fail strict conformance and verify it errors.

## Documentation

- [x] DC.1 Update `docs/specs/typechecker-invariants.md`: updated B-05, CC-16 (Section 5), GS-05 (Section 6) to reflect the fix.
- [x] DC.2 Commit with conventional message: `fix(types): verify behavior conformance in types_compatible (phase0k)`.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior (`lib/core/tests/traits/behavior_conformance.test.tml`)
- [x] 1.3 Run tests and confirm they pass
