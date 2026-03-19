---
name: dyn-boxing-casting-fix
description: ref dyn Behavior fat pointer type resolution in AST codegen (2026-03-19, FIXED)
type: project
---

# ref dyn Behavior Fat Pointer Fixes (2026-03-19)

## Root Cause Chain

The initial dyn dispatch fix (dd013978) implemented vtable creation and dispatch but missed
the fundamental type resolution: `ref dyn Behavior` must be a fat pointer `{ ptr, ptr }`, not
a simple `ptr`.

### Bug 1: llvm_type_from_semantic returns "ptr" for ref dyn
- **File**: `compiler/src/codegen/llvm/core/llvm_types.cpp:810`
- `RefType` handler returned `"ptr"` without checking inner type
- **Fix**: Check if inner is `DynBehaviorType`, return `%dyn.BehaviorName`

### Bug 2: llvm_type (parser-level) returns "ptr" for parser::RefType{DynType}
- **File**: `compiler/src/codegen/llvm/core/llvm_types.cpp:275`
- Same issue at parser type level
- **Fix**: Check if inner is `parser::DynType`, delegate to `llvm_type(*ref_inner)`

### Bug 3: calc_type_size doesn't know %dyn.* types
- **File**: `compiler/src/codegen/llvm/decl/enum.cpp:416`
- Enum payload size calculation fell through to default 8 bytes for `%dyn.*`
- `Maybe[ref dyn Error]` got layout `{ i32, i64 }` — only 8 bytes for 16-byte fat pointer
- **Fix**: Added `%dyn.*` check returning 16 bytes (2 pointers)

### Bug 4: emit_store uses "0" for aggregate types
- **File**: `compiler/include/codegen/llvm/llvm_ir_gen.hpp:1389`
- `store %dyn.Error 0` is invalid — `0` is integer, not aggregate
- **Fix**: Use `zeroinitializer` when type starts with `%` or `{`

### Bug 5: gen_cast doesn't handle ptr -> %dyn.* conversion
- **File**: `compiler/src/codegen/llvm/expr/cast.cpp:326`
- `ref this.cause as ref dyn Error` fell through to "unhandled cast" warning
- **Fix**: Added handler that creates fat pointer via `insertvalue` with vtable lookup

### Bug 6: when expression result alloca too small
- **File**: `compiler/src/codegen/llvm/control/when.cpp:389`
- `alloca i64` (8 bytes) insufficient for `%dyn.Error` results (16 bytes)
- **Fix**: Changed to `alloca [4 x i64]` (32 bytes) for universal support

### Bug 7: loop variable zero-init uses "0" for aggregates
- **File**: `compiler/src/codegen/llvm/control/loop.cpp:81`
- Same as Bug 4 but in loop variable initialization
- **Fix**: Use `zeroinitializer` for `%` and `{` types

## Key Insight
`ref dyn Behavior` is semantically a pointer but structurally a TWO-pointer aggregate.
The entire AST codegen assumed `RefType → ptr` universally. This cascaded through:
- Type resolution → wrong enum layouts → wrong alloca sizes → wrong stores
- Cast codegen → no fat pointer construction → data corruption
- Default method generation → wrong zero values → invalid IR
