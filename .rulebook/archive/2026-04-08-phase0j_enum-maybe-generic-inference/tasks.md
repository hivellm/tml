# Tasks: Preserve Type Arguments in Enum Constructors and Maybe Combinators

**Status**: Complete (15/15 done)
**Depends on**: None (can start immediately)
**Blocks**: Self-hosting type checker parity (phase12 Era 1)
**Duration**: 2–3 days
**Risk**: Medium — touches body checking for enum constructors and core combinators
**Related bugs**: B-03, B-04 in `docs/specs/typechecker-invariants.md` Appendix B

---

## Investigation

- [x] I.1 Read `compiler/src/types/checker/expr_call.cpp:391-393` — confirm enum constructor path that drops type args.
- [x] I.2 Read `compiler/src/types/checker/expr_call_method_types.cpp:67-69` — confirm Maybe combinator return type handling.
- [x] I.3 Grep HIR builder for compensating code (`hir_builder_expr.cpp`). Identify which sites re-infer enum/maybe type args and would need removal.

## Fix — Enum Constructors (B-03)

- [x] E.1 In `checker/expr_call.cpp` at the enum constructor branch, match payload argument types against the variant's declared payload types using `extract_type_params`.
- [x] E.2 Populate `NamedType::type_args` on the returned type with the inferred concrete arguments (in the enum's declared order).
- [x] E.3 Handle partial inference: zero-arg variants fall back to `expected_type`'s type args; nullptr placeholder for unresolvable params.
- [x] E.4 Add regression test: `lib/core/tests/option/option_type_inference.test.tml` — passes 1 suite (7 tests).

## Fix — Maybe/Outcome Combinators (B-04)

- [x] M.1 In `expr_call_method_types.cpp`, for `Maybe::map`, extract closure return type. Return `Maybe[U]`.
- [x] M.2 Same for `Maybe::and_then` — closure returns `Maybe[U]`, return that directly.
- [x] M.3 Same for `Maybe::or_else`, `Maybe::filter`, `Outcome::map`, `Outcome::map_err`, `Outcome::and_then`, `Outcome::or_else`.
- [x] M.4 Regression tests in `lib/core/tests/option/option_type_inference.test.tml` — all pass.

## HIR Builder Cleanup

- [x] H.1 Attempted removal of `extract_type_params_from_args` function from `hir_builder_expr.cpp`. Built successfully.
- [x] H.2 Attempted removal of step 3 (arg-type inference loop) from `substitute_method_generics`. Built successfully.
- [x] H.3 Triggered: removing step 3 caused new failures in option_flatten, option_transpose2, option_iter2 (and runtime failures in option_also, option_inspect, option_methods_extra, option_zip_with2). Restored both the function and step 3 with a NOTE comment explaining why removal requires the HIR builder to consume get_expr_type() for method-call return types. Follow-up: phase0j-hir-cleanup. core/iter 56/56, core/fmt 46/46 — no regressions in other suites.

## Verification

- [x] V.1 Build via `scripts\build.bat` — clean build.
- [x] V.2 Regression tests pass (1 suite, 7 tests in option_type_inference.test.tml).
- [x] V.3 Run full test suite — confirm no regressions vs baseline. (3 pre-existing compile errors: option_as_ref/as_mut/blocked — unchanged)
- [x] V.4 core/iter 56/56, core/fmt 46/46, core/option same pre-existing failures as before.

## Documentation

- [x] D.1 Updated `docs/specs/typechecker-invariants.md` Appendix B: B-03 and B-04 marked FIXED with source refs.
- [x] D.2 Update Section 6 contract items for enum constructor and combinator contracts. (TP-14/TP-15 fixed, I-4.15/I-4.26 marked FIXED)
- [x] D.3 Commit with conventional message: `fix(types): preserve type args on enum constructors and Maybe combinators (phase0j)`. (555a3e5a)

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation (Appendix B updated)
- [x] 1.2 Write tests covering the new behavior (option_type_inference.test.tml, 7 tests)
- [x] 1.3 Run tests and confirm they pass (all pass, no regressions)
