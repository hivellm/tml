---
name: dyn-behavior-codegen-fix
description: Full dyn Behavior codegen implementation (2026-03-19, FIXED) — fat pointers, vtables, vtable dispatch
type: project
---

# dyn Behavior Codegen Fix (2026-03-19)

## Root Causes (5 separate bugs fixed)

### 1. HIR Builder missing DynType resolution
- `hir_builder.cpp:resolve_type()` had no `parser::DynType` handler
- `ref dyn Greetable` → `RefType{inner: Unit}` instead of `RefType{inner: DynBehaviorType}`
- **Fix**: Added `parser::DynType` case to `resolve_type()` to produce `DynBehaviorType`

### 2. THIR method resolver didn't unwrap RefType for DynBehaviorType
- `thir_lower.cpp:resolve_method()` checked `receiver_type->is<DynBehaviorType>()`
- But `ref dyn Greetable` is `RefType{inner: DynBehaviorType}` — check failed
- **Fix**: Unwrap RefType before checking DynBehaviorType in resolve_method

### 3. Type checker didn't unwrap RefType for dyn method lookup
- `expr_call_method.cpp:617` — same unwrap bug as THIR resolver
- Methods on `ref dyn Behavior` silently returned Unit type
- **Fix**: Added RefType unwrapping before DynBehaviorType check

### 4. MIR type system had no DynType representation
- `MirDynType` added to MIR type variant
- `MakeDynObjectInst` added to MIR instruction set
- Multiple `convert_type` functions updated (HirMirBuilder, ThirMirBuilder, MirBuilder)

### 5. MIR passes didn't know about MakeDynObjectInst
- **Mem2Reg**: Promoted allocas used by MakeDynObjectInst (fix: mark as address-taken)
- **DCE**: Didn't track value usage through MakeDynObjectInst (fix: added to is_value_used)
- **DFE**: Removed vtable-referenced functions (fix: mark impl methods as live)
- **MIR Printer**: No printer for MakeDynObjectInst (fix: added)

## Critical Architecture Notes

### Default Compilation Path
- `CompilerOptions::use_thir = true` by default
- Query-based build → THIR pipeline (NOT HIR)
- Fixes must go in THIR path: `ThirMirBuilder`, `ThirLower`, not just `HirMirBuilder`

### Fat Pointer Layout
- `ref dyn Behavior` = `{ ptr, ptr }` = `{ data_ptr, vtable_ptr }`
- `MirPointerType{pointee: MirDynType}` → LLVM type `{ ptr, ptr }`
- Regular `ref T` = `ptr`

### Vtable Emission
- `BehaviorDef` and `ImplDef` added to `mir::Module`
- Vtable constants emitted by `MirCodegen::emit_vtables()`
- Format: `@vtable.Type.Behavior = internal constant { ptr, ... } { ptr @func, ... }`

### Files Changed
- `compiler/include/mir/mir.hpp` — MirDynType, MakeDynObjectInst, BehaviorDef, ImplDef, MethodCallInst dyn fields
- `compiler/include/codegen/mir_codegen.hpp` — vtable tracking members, emit_vtables decl
- `compiler/src/hir/hir_builder.cpp` — DynType in resolve_type
- `compiler/src/hir/hir_builder_expr.cpp` — dyn method return type lookup
- `compiler/src/thir/thir_lower.cpp` — RefType unwrap for dyn method resolution
- `compiler/src/types/checker/expr_call_method.cpp` — RefType unwrap for dyn method check
- `compiler/src/mir/hir_mir_builder.cpp` — DynBehaviorType in convert_type_impl, behaviors/impls collection
- `compiler/src/mir/thir_mir_builder.cpp` — DynBehaviorType in convert_type, dyn method call, behaviors/impls
- `compiler/src/mir/thir_mir_builder_expr.cpp` — dyn cast → MakeDynObjectInst
- `compiler/src/mir/builder/hir_expr_control.cpp` — dyn cast → MakeDynObjectInst (HIR path)
- `compiler/src/mir/builder/hir_expr.cpp` — dyn dispatch flag on MethodCallInst (HIR path)
- `compiler/src/mir/builder/builder_types.cpp` — DynBehaviorType in convert_type/convert_semantic_type
- `compiler/src/codegen/mir/mir_types.cpp` — MirDynType and pointer-to-dyn → { ptr, ptr }
- `compiler/src/codegen/mir/instructions.cpp` — MakeDynObjectInst dispatch
- `compiler/src/codegen/mir/instructions_method.cpp` — dyn vtable dispatch codegen
- `compiler/src/codegen/mir/instructions_misc.cpp` — emit_make_dyn_object_inst, emit_vtables
- `compiler/src/codegen/mir_codegen.cpp` — vtable emission call, state clearing
- `compiler/src/mir/mir_pass.cpp` — MakeDynObjectInst in is_value_used
- `compiler/src/mir/mir_printer.cpp` — MakeDynObjectInst printing
- `compiler/src/mir/passes/mem2reg.cpp` — MakeDynObjectInst prevents alloca promotion
- `compiler/src/mir/passes/dead_code_elimination.cpp` — already handles MakeDynObjectInst via else
- `compiler/src/mir/passes/dead_function_elimination.cpp` — vtable functions kept alive
