---
name: where-clause-overwrite-fix
description: pending_generic_impls_ single-entry map overwrites where clause impls — use pending_generic_impls_all_ instead (2026-03-30, FIXED)
type: project
---

## Bug: `Maybe__T {i32,i64}` type mismatch with `Maybe__I32 {i32,i32}` in generic iterator codegen

**Failing tests**: iter_higher_order.test.tml, iter_from_fn.test.tml

**Error**: LLVM IR contained both `%struct.Maybe__T = type { i32, i64 }` and `%struct.Maybe__I32 = type { i32, i32 }`. Call site used `%struct.Maybe__T` as return type but function was defined to return `%struct.Maybe__I32`.

### Root Cause

`pending_generic_impls_` is a `std::unordered_map<std::string, const ImplDecl*>` — stores only ONE impl per type name. When a type like `OnceWith` has multiple impls:
1. `impl[F,T] Iterator for OnceWith[F] where F = func() -> T`
2. `impl[F: Send] Send for OnceWith[F]`
3. `impl[F: Sync] Sync for OnceWith[F]`

The last one registered (Sync, no where clause) overwrites the Iterator impl. So `resolve_impl_where_clause` never derives `T -> I32` from `where F = func() -> T`.

Without `T` in `type_subs`, `substitute_type(Maybe[T], type_subs)` leaves `T` unresolved. Then `llvm_type_from_semantic(Maybe[T])` calls `require_enum_instantiation("Maybe", [NamedType("T")])`, producing `Maybe__T = { i32, i64 }` instead of `Maybe__I32 = { i32, i32 }`.

### Fix

**File**: `compiler/src/codegen/llvm/expr/method_impl.cpp` lines 587-593

Changed from using `pending_generic_impls_.find(named.name)` (single impl) to `pending_generic_impls_all_.find(named.name)` (all impls vector). Now iterates through ALL impls for a type and resolves where clauses from each one.

**Why:** `pending_generic_impls_all_` is populated at `runtime_modules.cpp:1053` with ALL impls, while `pending_generic_impls_` at line 1052 only keeps the last one.
