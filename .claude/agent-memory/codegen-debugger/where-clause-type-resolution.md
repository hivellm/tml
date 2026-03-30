---
name: where-clause-type-resolution
description: Where clause type parameter resolution in iterator adapters - 3 bugs fixed (ClosureType, associated type substitution, best-impl selection)
type: project
---

## Where Clause Type Parameter Resolution Bugs (2026-03-30, FIXED)

### Bug 1: ClosureType not handled in resolve_impl_where_clause
- **Symptom**: `where F = func() -> T` didn't resolve `T` when concrete type was `ClosureType`
- **Root cause**: `resolve_impl_where_clause` only checked `concrete->is<types::FuncType>()`, not `types::ClosureType`
- **Fix**: Added `extract_func_signature()` helper that handles both FuncType and ClosureType
- **Files**: `method_impl.cpp`, `infer_methods.cpp`

### Bug 2: pending_generic_impls_ stores only LAST impl (single-entry map)
- **Symptom**: `where F = func() -> T` on RepeatWith's Iterator impl was never reached because `pending_generic_impls_` stored the Sync impl (last registered)
- **Root cause**: `infer_methods.cpp` used `pending_generic_impls_` (single entry) for:
  1. Generic param name mapping (line ~558)
  2. Where clause resolution (line ~513)
- **Fix**: Changed both sites to use `pending_generic_impls_all_` (multi-entry map)
- **Files**: `infer_methods.cpp` lines 558, 570+

### Bug 3: Flattened type args + best-impl selection
- **Symptom**: `Cloned[SliceIter[I32]]` had `named.type_args = [SliceIter, I32]` (flattened from mangled type name). Using Sync impl's `[I]` generics only mapped `I=SliceIter`, leaving `T` unresolved.
- **Root cause**: `parse_mangled_type_string("Cloned__SliceIter__I32")` flattens to 2 args. The Iterator impl has `[I, T]` generics which matches 2 args. But `pending_generic_impls_` pointed to Sync (1 generic).
- **Fix**: When building type_subs from impl generics, find the impl with the most generics that still fits within named.type_args.size(). This selects Iterator's `[I, T]` over Sync's `[I]`.
- **Files**: `method_impl.cpp` (line ~545), `infer_methods.cpp` (line ~558)

### Bug 4: Abstract associated types from lookup_associated_type
- **Symptom**: `lookup_associated_type("SliceIter", "Item")` returned `ref T` (abstract) instead of `ref I32`
- **Root cause**: The function calls `resolve_parser_type_with_subs(*binding.type, {})` with empty subs, so type params aren't substituted
- **Fix**: Added `substitute_inner_assoc_type()` helper that looks up the inner struct's type params and substitutes them using the concrete type args
- **Files**: `method_impl.cpp`, `infer_methods.cpp` (both have the helper)

### Affected tests (all FIXED):
- `iter_repeat_with` — `where F = func() -> T` (Bugs 1, 2)
- `iter_map` — `where F = func(I::Item) -> B` (Bug 2)
- `iter_filter_map` — `where F = func(I::Item) -> Maybe[B]` (Bug 2)
- `iter_map_while` — `where F = func(I::Item) -> Maybe[B]` (Bug 2)
- `iter_cloned` — `where I::Item = ref T` (Bugs 3, 4)

### Key insight: TWO parallel code paths
The where clause resolution exists in TWO places:
1. `method_impl.cpp` `resolve_impl_where_clause` — used for method call codegen (return type of call instruction)
2. `infer_methods.cpp` `infer_resolve_where_clause` — used for type inference (receiver type for chained calls)
Both must be kept in sync. The infer path was the primary bottleneck since it determines `receiver_type` for `.unwrap()` and other chained method calls.
