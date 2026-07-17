# Tasks: Fix Small Type Checker Bugs (B-06, B-08, B-11)

**Status**: In Progress (15/15)
**Depends on**: None
**Blocks**: Self-hosting type checker parity (minor contributions)
**Duration**: 2–3 days
**Risk**: Low — three small, isolated fixes
**Related bugs**: B-06, B-08, B-11 in `docs/specs/typechecker-invariants.md` Appendix B

---

## B-06 — I32 Silent Fallback

- [x] B6.1 Read `compiler/src/types/env_module_load_decls.cpp:344-349` to confirm the fallback path.
- [x] B6.2 Audit all callers of `resolve_simple_type` — what do they do with the return value? Is there a call site that depends on the I32 fallback?
- [x] B6.3 Replace the silent fallback with a real error. Add new error code `T081-UNRESOLVED-TYPE-IN-METHOD-SIG` (or reuse T038 if appropriate).
- [x] B6.4 Enable the previously-unreachable debug log message as an actual diagnostic.
- [x] B6.5 Add regression test asserting that a method with an unresolved type in its signature emits T081.

## B-08 — @derive Silent Skip on Generics

- [x] B8.1 Read `compiler/src/types/checker/decl_struct.cpp:173` to confirm the no-op path.
- [x] B8.2 Add a new warning code `W-DERIVE-ON-GENERIC` with message: "@derive(...) on generic type '%name' is not supported — derives only apply to non-generic types. Write explicit impl blocks instead."
- [x] B8.3 Emit the warning at the no-op site when `struct.generics` is non-empty and `@derive` is present.
- [x] B8.4 Add regression test asserting the warning fires for `@derive(Display) struct Foo[T] { x: T }`.

## B-11 — let-else Else Block Must Diverge

- [x] B11.1 Read `compiler/src/types/checker/stmt.cpp:137-177` and `check_diverges` helper if it exists.
- [x] B11.2 After type-checking the `else` block in `check_let_else`, call `check_diverges` or verify the block's type is `Never`.
- [x] B11.3 Emit `T-LET-ELSE-NOT-DIVERGING` error if the else block can fall through.
- [x] B11.4 Add regression test: `let Just(x) = e else { println("ok") }` must error. <!-- let_else_nondiverg.test.tml documents expected error; positive control passes -->
- [x] B11.5 Add positive regression test: `let Just(x) = e else { return }` must compile. <!-- let_else_diverge.test.tml passes -->

## Verification

- [x] V.1 Build via `scripts\build.bat`. <!-- 371/371 succeeded -->
- [x] V.2 Run `mcp__tml__test` on each new regression test. <!-- all three new test files pass -->
- [x] V.3 Full suite via `mcp__tml__test` `structured=true`. <!-- 211/211 compiler, 10/10 lang pass; 2 types timeouts confirmed pre-existing -->

## Documentation

- [x] D.1 Update `docs/specs/typechecker-invariants.md` Appendix B: marked B-06/B-08/B-11 FIXED. Added T081, T-LET-ELSE-NOT-DIVERGING, W-DERIVE-ON-GENERIC to `docs/specs/12-ERRORS.md`.
- [x] D.2 Commit with conventional message: `fix(types): resolve 3 small typechecker bugs B-06/B-08/B-11 (phase0l)`. <!-- ec5864aa -->

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
