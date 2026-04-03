# Proposal: Unit Type Cleanup — Consistent {} Representation

## Why

The Unit type maps to `"void"` in `mir_primitive_to_llvm()`, but LLVM doesn't allow `void` as a value type (can't `load void`, `store void`, `alloca void`, `phi void`). This forces 15+ ad-hoc patches throughout the codegen that check `if (type_str == "void") { type_str = "{}"; }`. The dual representation (void for returns, {} for values) causes confusion and bugs when a Unit value flows through a path that doesn't have the patch.

## What Changes

1. **Core change**: Map `PrimitiveType::Unit` to `"{}"` (empty struct) instead of `"void"` in `mir_types.cpp`
2. **Return type handling**: Add `is_unit_return()` helper — emit `"void"` only for function return types in `emit_function()` and `emit_function_declaration()`
3. **Remove patches**: Delete all 15+ `if (type_str == "void") { type_str = "{}"; }` blocks from LoadInst, StoreInst, AllocaInst, PhiInst, and declaration handlers
4. **Simplify ReturnTerm**: single check for unit return type instead of cascading void/{} checks

## Impact

- Affected specs: None (internal compiler change)
- Affected code: `compiler/src/codegen/mir/mir_types.cpp` (1 line), `compiler/src/codegen/mir_codegen.cpp` (declarations), `compiler/src/codegen/mir/instructions.cpp`, `compiler/src/codegen/mir/instructions_misc.cpp`, `compiler/src/codegen/mir/terminators.cpp`
- Breaking change: NO — same LLVM IR output (void returns stay void, value positions become {})
- User benefit: Eliminates entire class of "can't load/store/alloca void" bugs
