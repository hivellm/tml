# Proposal: Fix Small Type Checker Bugs (B-06, B-08, B-11)

## Why

Three small but real bugs were surfaced by the `phase12c` audit and documented in `docs/specs/typechecker-invariants.md` Appendix B. Individually they are each a 1-day fix; bundling them avoids task proliferation.

**B-06 — I32 silent fallback for unresolved types** (`env_module_load_decls.cpp:344-349`): `resolve_simple_type` returns `I32` for any type name it cannot resolve, rather than erroring. The debug log message that should fire is unreachable dead code. Any typo or forward reference in a method signature silently becomes `I32`, corrupting the function's signature in `Module::functions`.

**B-08 — @derive silently skips all generic types** (`decl_struct.cpp:173`): `@derive(Display, Hash, Eq)` on a generic struct registers nothing and emits no warning. Developers writing `@derive` on generics get no feedback that their derive is a no-op. At minimum this should emit a compiler warning (W-series code) with a clear message.

**B-11 — let-else else block not verified to diverge** (`checker/stmt.cpp:137-177`): `let Just(x) = e else { println("ok") }` is accepted by the type checker with no error. The language contract requires the `else` block to have type `Never` (must diverge via `return`, `continue`, `break`, `panic`, or `exit`). HIR lowering is expected to enforce this but the checker should catch it first.

## What Changes

1. **B-06 fix**: Replace the `return make_primitive(PrimitiveKind::I32)` fallback in `resolve_simple_type` with a hard error or a `TypeVar` sentinel that makes the downstream failure visible. Add a T-series error code for unresolved types in method signature positions.

2. **B-08 fix**: In `decl_struct.cpp:173`, when `@derive` is encountered on a generic type, emit a new W-series warning (e.g., `W-DERIVE-ON-GENERIC`) explaining that derives for generic types are not supported in the current compiler and the developer should write explicit impls. Keep the current silent-skip behavior so nothing breaks, just surface it.

3. **B-11 fix**: In `check_let_else` (around `stmt.cpp:137-177`), after checking the `else` block's type, verify it is `Never` or call `check_diverges`. Emit a T-series error if not.

4. Add regression tests for all three.

## Impact

- **Affected specs**: `docs/specs/typechecker-invariants.md` (remove B-06, B-08, B-11 from Appendix B)
- **Affected code**: `compiler/src/types/env_module_load_decls.cpp`, `compiler/src/types/checker/decl_struct.cpp`, `compiler/src/types/checker/stmt.cpp`
- **Breaking change**:
  - B-06: YES — programs with previously silent I32 fallbacks will now error. Should be rare but needs audit.
  - B-08: NO — only adds warnings.
  - B-11: YES — programs with non-diverging let-else else blocks will now error. Should be rare.
- **User benefit**: clearer error messages, fewer silent-corruption classes, better developer feedback on @derive misuse.
