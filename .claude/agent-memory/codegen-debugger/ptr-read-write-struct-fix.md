---
name: ptr_read/ptr_write multi-field struct fix
description: 4-bug fix chain for ptr_read[T] where T is a multi-field struct - type checker, HIR builder, MIR CallInst, codegen
type: project
---

## Bug: ptr_read[MyState] generates `extractvalue i32 %v, 4294967295`

**Date**: 2026-03-20, FIXED

### Root Cause Chain (4 interconnected bugs)

1. **Type checker** (`types/builtins/mem.cpp`): `ptr_read` registered with `return_type=I32` and `type_params={}`. Should be `GenericType{"T"}` with `type_params={"T"}`. This caused the type checker to assign I32 as the return type for ALL ptr_read calls regardless of the type argument.

2. **HIR builder `get_expr_type`** (`hir/hir_builder.cpp`): Didn't handle `parser::LowlevelExpr` (returned Unit) and didn't do generic substitution for `CallExpr` with PathExpr generics. This caused `let s2 = lowlevel { ptr_read[MyState](p) }` to infer `s2` as Unit type.

3. **HIR builder `lower_let`/`lower_var`** (`hir/hir_builder_stmt.cpp`): Called `get_expr_type` (unreliable) BEFORE `lower_expr` (accurate). Since the init wasn't lowered yet, the variable got the wrong type from get_expr_type.

4. **MIR codegen** (`codegen/mir/instructions.cpp`): ptr_read handler couldn't determine element type because `CallInst` had no `type_args` field. Only fallback sources (pointee type, return type) were available, both returning i32/void.

### Fix Summary

- `types/builtins/mem.cpp`: Register all 6 ptr_read/write variants with `type_params={"T"}` and appropriate `GenericType{"T"}` in params/returns
- `hir/hir_builder.cpp`: get_expr_type handles LowlevelExpr (recurse) and CallExpr generic substitution
- `hir/hir_builder_stmt.cpp`: lower_let/lower_var now lower init FIRST, use init->type() if get_expr_type returned Unit or GenericType
- `hir/hir_builder_expr.cpp`: lower_call extracts type_args from PathExpr::generics, overrides return_type for read intrinsics
- `mir/mir.hpp`: Added `std::vector<MirTypePtr> type_args` to CallInst
- `mir/thir_mir_builder.cpp` + `builder/hir_expr.cpp`: Propagate type_args from THIR/HIR to MIR CallInst
- `codegen/mir/instructions.cpp`: All 6 ptr_read/write handlers check `i.type_args[0]` as highest priority

### Key Insight

The legacy AST codegen never had this bug because it reads the type parameter directly from the parser AST's `PathExpr::generics` at codegen time. The MIR path loses this info because the type checker strips it during resolution. The fix propagates the type argument through the entire pipeline: TypeChecker → HIR → THIR → MIR → Codegen.

**Why:** The MIR pipeline needs type information propagated through every stage. Any stage that drops generic type arguments will cause downstream failures.
**How to apply:** When adding new generic intrinsics, always register them with proper type_params in the FuncSig.
