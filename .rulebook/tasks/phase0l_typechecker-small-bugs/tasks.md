# Tasks: Fix Small Type Checker Bugs (B-06, B-08, B-11)

**Status**: Planned (0/15)
**Depends on**: None
**Blocks**: Self-hosting type checker parity (minor contributions)
**Duration**: 2–3 days
**Risk**: Low — three small, isolated fixes
**Related bugs**: B-06, B-08, B-11 in `docs/specs/typechecker-invariants.md` Appendix B

---

## B-06 — I32 Silent Fallback

- [ ] B6.1 Read `compiler/src/types/env_module_load_decls.cpp:344-349` to confirm the fallback path.
- [ ] B6.2 Audit all callers of `resolve_simple_type` — what do they do with the return value? Is there a call site that depends on the I32 fallback?
- [ ] B6.3 Replace the silent fallback with a real error. Add new error code `T081-UNRESOLVED-TYPE-IN-METHOD-SIG` (or reuse T038 if appropriate).
- [ ] B6.4 Enable the previously-unreachable debug log message as an actual diagnostic.
- [ ] B6.5 Add regression test asserting that a method with an unresolved type in its signature emits T081.

## B-08 — @derive Silent Skip on Generics

- [ ] B8.1 Read `compiler/src/types/checker/decl_struct.cpp:173` to confirm the skip path.
- [ ] B8.2 Add a new warning code `W-DERIVE-ON-GENERIC` with message: "@derive(...) on generic type '%name' is not supported — derives only apply to non-generic types. Write explicit impl blocks instead."
- [ ] B8.3 Emit the warning at the skip site when `struct.generics` is non-empty and `@derive` is present.
- [ ] B8.4 Add regression test asserting the warning fires for `@derive(Display) struct Foo[T] { x: T }`.

## B-11 — let-else Else Block Must Diverge

- [ ] B11.1 Read `compiler/src/types/checker/stmt.cpp:137-177` and `check_diverges` helper if it exists.
- [ ] B11.2 After type-checking the `else` block in `check_let_else`, call `check_diverges` or verify the block's type is `Never`.
- [ ] B11.3 Emit `T-LET-ELSE-NOT-DIVERGING` error if the else block can fall through.
- [ ] B11.4 Add regression test: `let Just(x) = e else { println("ok") }` must error.
- [ ] B11.5 Add positive regression test: `let Just(x) = e else { return }` must compile.

## Verification

- [ ] V.1 Build via `scripts\build.bat`.
- [ ] V.2 Run `mcp__tml__test` on each new regression test.
- [ ] V.3 Full suite via `mcp__tml__test` `structured=true`. B-06 and B-11 may expose real bugs in the existing test corpus — fix those in the library code, not by reverting the type checker fix. B-08 should have zero breakage.

## Documentation

- [ ] D.1 Update `docs/specs/typechecker-invariants.md` Appendix B: remove B-06, B-08, B-11 after their commits land. Add new error codes to `docs/specs/12-ERRORS.md`.
- [ ] D.2 Commit with conventional message: `fix(types): resolve 3 small typechecker bugs B-06/B-08/B-11 (phase0l)`.
