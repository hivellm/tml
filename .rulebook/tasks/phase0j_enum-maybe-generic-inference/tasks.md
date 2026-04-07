# Tasks: Preserve Type Arguments in Enum Constructors and Maybe Combinators

**Status**: Planned (0/12)
**Depends on**: None (can start immediately)
**Blocks**: Self-hosting type checker parity (phase12 Era 1)
**Duration**: 2–3 days
**Risk**: Medium — touches body checking for enum constructors and core combinators
**Related bugs**: B-03, B-04 in `docs/specs/typechecker-invariants.md` Appendix B

---

## Investigation

- [ ] I.1 Read `compiler/src/types/checker/expr_call.cpp:391-393` — confirm enum constructor path that drops type args.
- [ ] I.2 Read `compiler/src/types/checker/expr_call_method_types.cpp:67-69` — confirm Maybe combinator return type handling.
- [ ] I.3 Grep HIR builder for compensating code (`hir_builder_expr.cpp`). Identify which sites re-infer enum/maybe type args and would need removal.

## Fix — Enum Constructors (B-03)

- [ ] E.1 In `checker/expr_call.cpp` at the enum constructor branch, match payload argument types against the variant's declared payload types using `extract_type_params` (or equivalent in `expr_call_method_types.cpp`).
- [ ] E.2 Populate `NamedType::type_args` on the returned type with the inferred concrete arguments (in the enum's declared order).
- [ ] E.3 Handle partial inference: if not all type params can be inferred from payload, leave them as `TypeVar` (unification target) or emit T056-style error if the constructor is used in a context that requires concrete types.
- [ ] E.4 Add checker regression test: `compiler/tests/types/enum_constructor_infers_args.test.tml` asserting that `Just(42)` has type `Maybe[I32]`, not `Maybe`.

## Fix — Maybe/Outcome Combinators (B-04)

- [ ] M.1 In `expr_call_method_types.cpp`, for `Maybe::map`, read the closure parameter's type and extract its return type. Return `Maybe[closure_return]`, not `Maybe[T]`.
- [ ] M.2 Same for `Maybe::and_then`, but the closure's return type must itself be `Maybe[U]`; unwrap one level.
- [ ] M.3 Same for `Maybe::or_else`, `Maybe::filter` (filter keeps `T`), `Outcome::map`, `Outcome::map_err`, `Outcome::and_then`, `Outcome::or_else`.
- [ ] M.4 Add checker regression tests under `compiler/tests/types/maybe_combinator_types.test.tml` asserting that `Just(42).map(|x| x.to_string())` has type `Maybe[Text]`.

## HIR Builder Cleanup

- [ ] H.1 Remove the compensating enum-type-arg re-inference in `hir_builder_expr.cpp`. Compile and run affected tests.
- [ ] H.2 Remove the compensating Maybe combinator type re-derivation. Compile and run affected tests.
- [ ] H.3 If any downstream stage breaks, guard the removal behind a compile-time flag and file a follow-up task.

## Verification

- [ ] V.1 Build via `scripts\build.bat`.
- [ ] V.2 Run `mcp__tml__test` on the new regression tests — must pass.
- [ ] V.3 Run full test suite via `mcp__tml__test` with `structured=true`. Confirm no regressions vs the current ~1845/1874 baseline. Target: ≥ baseline.
- [ ] V.4 Spot-check `core/iter`, `core/option`, `std/promise` suites — these exercise Maybe/Outcome combinators heavily.

## Documentation

- [ ] D.1 Update `docs/specs/typechecker-invariants.md` Appendix B: move B-03 and B-04 to a "Fixed" subsection with commit hashes.
- [ ] D.2 Update Section 6 contract items IN-03/IN-05 (or equivalent): add "must preserve type arguments on enum constructors" and "Maybe combinators must return the closure's result type".
- [ ] D.3 Commit with conventional message: `fix(types): preserve type args on enum constructors and Maybe combinators (phase0j)`.
