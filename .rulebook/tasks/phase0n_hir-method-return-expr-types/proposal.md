# Proposal: HIR Builder — Use Type Checker Expression Map for Method Return Types

## Why

Phase0j fixed the type checker (B-03, B-04) so it now correctly stores per-call-site return types
(e.g. `Maybe[I32]` for `Just(42)`, `Maybe[I64]` for `maybe.map(do(n) -> I64 { ... })`) in the
expression type map (`expr_types_`). However, the HIR builder does not consume this map for method
call or call expression return types — it independently re-derives them via `lookup_func()` +
`substitute_method_generics` + `extract_type_params_from_args` (step 3).

The `extract_type_params_from_args` function in `hir_builder_expr.cpp` was labeled "compensating
code" in phase0j and phase0j attempted to remove it (H.1/H.2). Removal caused regressions in
option_flatten, option_transpose2, option_iter2, option_also, and others — because the HIR builder
still needs step 3 to infer return types from argument types (closure return type, nested Maybe,
etc.). The type checker has the correct answers in `expr_types_` but the HIR builder cannot access
them without a dedicated path.

This task adds that path: for method call expressions and call expressions, the HIR builder should
consult `get_expr_type()` (which reads from the type checker's per-expression map) before falling
back to the `lookup_func()` substitution approach. Once this is in place, step 3 of
`substitute_method_generics` becomes redundant and can be removed along with the
`extract_type_params_from_args` function.

## What Changes

1. **`compiler/src/hir/hir_builder.cpp`** — extend `get_expr_type()` for `MethodCallExpr` to look
   up the resolved return type from the type checker's expression type map (if available).
2. **`compiler/src/hir/hir_builder_expr.cpp`** — in `lower_method_call`, use `get_expr_type()` on
   the method call expression before calling `substitute_method_generics`. If the type checker
   already has a concrete type, use it directly.
3. **`compiler/src/hir/hir_builder_expr.cpp`** — once step 2 is verified, remove step 3 of
   `substitute_method_generics` (the `extract_type_params_from_args` loop) and the entire
   `extract_type_params_from_args` static function (lines 49-156).

## Impact

- Affected code: `compiler/src/hir/hir_builder.cpp`, `compiler/src/hir/hir_builder_expr.cpp`
- Affected specs: `docs/specs/typechecker-invariants.md` (self-hosting contract clean-up)
- Breaking change: NO — visible behavior stays the same; intermediate HIR gets simpler
- User benefit: Cleaner HIR builder, removes duplicated type inference logic, improves self-hosting
  contract parity (HIR and type checker agree on types from the same source)
