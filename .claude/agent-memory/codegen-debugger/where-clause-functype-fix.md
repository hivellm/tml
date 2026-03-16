---
name: where-clause-functype-fix
description: match_where_pattern_call in call.cpp now handles FuncType patterns for where clause type inference
type: project
---

## Where Clause FuncType Pattern Matching Fix (2026-03-16) -- FIXED

**Bug**: `match_where_pattern_call` in `call.cpp:37-86` only handled `NamedType` and `RefType` patterns.
`FuncType` patterns like `func() -> Maybe[T]` were silently skipped (line 51: `if (!pattern.is<parser::NamedType>()) return;`).

**Effect**: For `from_fn[F, T](gen: F) where F = func() -> Maybe[T]`, type param `T` was never
inferred from the where clause. Fell back to `Unit` at call.cpp:1958, producing monomorphized
name `from_fn__Fn__Unit` with return type `{}` instead of `%struct.Maybe__I32`.

**Fix**: Added `FuncType` and `ClosureType` handling before the `NamedType` check in `match_where_pattern_call`.
Recursively matches return type and parameter types, mirroring `resolve_impl_where_clause` in `method_impl.cpp:93-104`.

**Why method_impl.cpp was already correct**: `resolve_impl_where_clause` destructures `FuncType` at the top level
(checking `rhs->is<parser::FuncType>()` + `concrete->is<types::FuncType>()`) before calling `match_where_pattern`.
`match_where_pattern_call` in call.cpp didn't have this top-level handler.

**File**: `compiler/src/codegen/llvm/expr/call.cpp` lines 51-84 (after fix)

**Impact**: Fixes `from_fn`, `repeat_with`, `once_with` and any generic function with `where F = func(...) -> T` constraint.

**How to apply**: Whenever adding new pattern types to `match_where_pattern_call`, also check `match_where_pattern` in `method_impl.cpp` and `infer_methods.cpp` for consistency.
