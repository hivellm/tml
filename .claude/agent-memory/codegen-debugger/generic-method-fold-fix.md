---
name: Generic Behavior Default Method Monomorphization Fix
description: fold[B] and similar method-level generic default behavior methods now monomorphize in AST codegen (3 bugs fixed)
type: project
---

## Iterator::fold[B] `%struct.B` unsized type fix (2026-03-20, FIXED)

### Root Cause Chain (3 interconnected bugs)

**Bug 1: `generate_default_method` rejected method-level generics**
- File: `compiler/src/codegen/llvm/core/dyn.cpp:417`
- `if (!trait_method.generics.empty()) return false;` — unconditionally rejected all generic methods
- Fix: Allow if all generic params have concrete substitutions in `current_type_subs_`
- Also added `method_type_suffix` parameter for correct name mangling

**Bug 2: Method-level type param inference skipped when `impl_param_count >= type_params.size()`**
- File: `compiler/src/codegen/llvm/expr/method_impl.cpp:229`
- `impl_param_count = named.type_args.size()` was 1 (for ListIter[I64])
- `func_sig->type_params = ["B"]` had size 1
- Loop `for (tp_idx = 1; tp_idx < 1; ...)` never executed
- Fix: When `impl_param_count >= func_sig->type_params.size()`, reset to 0 (all are method-level)

**Bug 3: `GenericType` not matched in type param inference**
- File: `compiler/src/codegen/llvm/expr/method_impl.cpp:217`
- Only checked `NamedType`, not `GenericType` — missed `B` as `GenericType("B")`
- Also: `func_sig->params` omitted `this` for default behavior methods, but param loop started at offset 1
- Fix: Added `GenericType` handling, computed `param_offset` based on params vs args count
- Also: Two-pass inference — FuncType/ClosureType params first (e.g., closure return type I64), then bare params (e.g., literal 0 → I32), to avoid I32 override

### Key Discovery: AST codegen path used for files with imports
- Files with `use` imports go through `provide_codegen_unit` → AST `LLVMIRGen::generate()`, NOT MIR path
- Decision at `query_core.cpp:620`: `has_tml_imports_needing_codegen` is true → AST codegen
- HIR/THIR/MIR builders are NOT called for the user's file in this path

### Architecture Notes
- Default behavior methods (fold, for_each, map, etc.) are stored in `parser::TraitDecl::methods`
- `generate_pending_instantiations()` in `generic.cpp` searches local impls, then module registry, then GlobalModuleCache
- When impl doesn't have the method, falls through to `generate_default_method` for trait defaults
- Iterator trait source loaded from `behavior_src` map: `{"Iterator", "core/src/iter/traits/iterator"}`
- `func_sig->params` for default behavior methods from module binary cache omit `this` (offset=0 not 1)
