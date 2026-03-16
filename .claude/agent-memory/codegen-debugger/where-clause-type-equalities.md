---
name: Where-Clause Type Equality Resolution
description: Fix for `where I::Item = ref T` constraints — Issues 1 and 1b fixed, Issue 2 open
type: project
---

## Where-Clause Type Equality Constraint Resolution (2026-03-15/16) — MOSTLY FIXED

### Issue 1: Inner iterator method dependency — FIXED (2026-03-16)
- **Bug**: METHOD 4b dispatch in `method_generic.cpp` emitted call instructions to
  concrete generic methods (e.g., `SliceIter__I32::next`) but never queued them for
  code generation via `pending_impl_method_instantiations_`.
- **Root cause**: Both the `parameterized_bounds` path and the `required_behaviors` path
  in `gen_method_bounded_generic_dispatch` computed `fn_name` and emitted the call, but
  never triggered monomorphization of the target function body.
- **Fix**: Added instantiation queueing after `fn_name` computation in both paths.
  For generic types, build `inner_type_subs` from concrete type args and queue a
  `PendingImplMethod`. For non-generic library types, queue with empty type_subs.
- **File**: `compiler/src/codegen/llvm/expr/method_generic.cpp`

### Issue 1b: Nullable Maybe pattern binding shadows enum variant — FIXED (2026-03-16)
- **Bug**: In `when` matching on nullable Maybe (ptr-optimized), `Nothing` arm's
  `IdentPattern("Nothing")` was treated as a variable binding, shadowing the variant.
  `return Nothing` then loaded from null pointer (segfault).
- **Root cause**: `when.cpp:1127` only checked `scrutinee_type.starts_with("%struct.")`
  for unit variant detection. Nullable Maybe uses `scrutinee_type == "ptr"`.
- **Fix**: When `is_nullable_maybe` is true, search `pending_generic_enums_` for the
  scrutinee's enum type and check if the ident is a known unit variant.
- **File**: `compiler/src/codegen/llvm/control/when.cpp`

### Issue 2: Standalone constructor function monomorphization — OPEN
- **Bug**: `cloned[I, T](iter: I) -> Cloned[I] where I::Item = ref T` produces
  `Cloned__UNRESOLVED` because `T` is not resolved during function instantiation.
- **Root cause**: `unify_types` stores `bindings["I"]` with empty type_args (loses
  the `[I32]` from the inferred type). This prevents `resolve_assoc_type_for_concrete`
  from substituting T with I32 in the associated type binding.
- **Partial fix**: Added where-clause resolution in `call.cpp` (line ~1897) with
  arg_types_for_where saved from infer_expr_type. Also added binding repair logic.
  Neither fully fixes the issue — `unify_types` type_args loss not root-caused.
- **Files**: `compiler/src/codegen/llvm/expr/call.cpp` (where clause + match_where_pattern_call helper)
- **Workaround**: Use direct struct construction `Cloned { iter: iter }` instead of `cloned(iter)`.

### Issue 3: Maybe__T vs Maybe__I32 type mismatch — NOT OBSERVED
- The generated IR correctly uses concrete types. The where-clause resolution in
  `generic.cpp` (already implemented in 2026-03-15 session) correctly derives T=I32.

### Previously Fixed (2026-03-15 session)
- Type checker where_clause processing in check_impl_decl/body
- Codegen match_pattern_type/match_where_pattern RefType handling
- resolve_where_clause_type_equalities for associated type paths
- resolve_assoc_type_for_concrete method
- Specialized impl detection for fewer self_type args than impl generics
- Token-based parser greedy token consumption for nested generics

### Test File
- `lib/core/tests/iter/iter_cloned.test.tml` — 3 tests pass (basic, exhaustion, empty)
