# Proposal: Preserve Type Arguments in Enum Constructors and Maybe Combinators

## Why

Two type-checker soundness bugs documented in `docs/specs/typechecker-invariants.md` Appendix B (B-03 and B-04) cause generic type information to be silently discarded during body checking. This forces the HIR builder to re-infer type arguments downstream, causing cascading bugs across codegen (e.g. RC7 `Maybe__T` vs `Maybe__I32` mangling, iterator `.map()` chains producing wrong types).

**B-03 — Enum constructors lose type arguments** (`checker/expr_call.cpp:391-393`): `Just(42)` is type-checked as `NamedType{"Maybe", "", {}}` with an empty `type_args` vector. The `I32` from the argument is discarded. The HIR builder must re-infer it from the payload every time.

**B-04 — Maybe[T].map(f) returns Maybe[T] instead of Maybe[U]** (`checker/expr_call_method_types.cpp:67-69`): `map`, `and_then`, `or_else`, `filter` on `Maybe[T]` always return `Maybe[T]` regardless of the closure's return type. Chained `.map()` calls silently discard transformation types. HIR builder must infer the actual result type from the closure's signature.

Both bugs are load-bearing (the HIR builder already compensates), but they corrupt the self-hosting contract in `docs/specs/typechecker-invariants.md` Section 6 because a TML-written type checker that fixes these will produce a different TypeEnv than the C++ checker — breaking parity.

## What Changes

1. **`checker/expr_call.cpp`** — when type-checking an enum constructor expression, infer the concrete type arguments from the payload argument types and store them in `NamedType::type_args`. Use `extract_type_params` or a similar mechanism to unify payload types against variant signature.

2. **`checker/expr_call_method_types.cpp`** — for `Maybe[T]::map`, `and_then`, `or_else`, `filter` (and equivalent on `Outcome[T,E]`), read the closure argument's actual return type and substitute it into the method's return type. The return type becomes `Maybe[closure_return_type]`, not `Maybe[T]`.

3. **HIR builder** — remove the compensating re-inference code for enum constructors and Maybe combinators once the type checker produces correct types. Guard with a temporary compatibility flag if needed for incremental rollout.

4. **Self-hosting contract** — update `docs/specs/typechecker-invariants.md` Section 6 items IN-03, IN-05 (or equivalent) to require concrete type-argument preservation. Move B-03 and B-04 from Appendix B (latent bugs) to a "Fixed" subsection with the commit hash.

## Impact

- **Affected specs**: `docs/specs/typechecker-invariants.md` (Section 6 contract, Appendix B), `docs/specs/28-CHECKER.md` (if it claims current behavior)
- **Affected code**: `compiler/src/types/checker/expr_call.cpp`, `compiler/src/types/checker/expr_call_method_types.cpp`, `compiler/src/hir/hir_builder_expr.cpp` (remove compensation), possibly `compiler/src/thir/thir_lower.cpp`
- **Breaking change**: NO — visible behavior does not change; the HIR output should remain identical. Only the intermediate TypeEnv gets more information.
- **User benefit**: Cleaner self-hosting contract. Removes two silent type-information losses. Simpler HIR builder. Indirect improvement to error messages that currently show `Maybe` without the type argument.
