# Maybe[T] Generic Method Codegen Investigation (2026-03-03)

## Overview
6 bugs prevented generic methods on `Maybe[T]` from working: `also`, `inspect`, `ok_or`, `contains`, `filter`, `flatten`.

## Root Causes and Fixes

### 1. core::option Module Not in Registry
- **Symptom**: `@tml_N4core10Maybe__I329also__I32E` and `@tml_N4core10Maybe__I327inspectE` undefined
- **Root cause**: `core::option` was NOT in `essential_library_modules` at `runtime_modules.cpp:317`
- **Mechanism**: `generic.cpp:554` searches all modules in registry for impl blocks. Without `core::option`, methods in impl blocks 3-8 of `option.tml` were invisible.
- **Fix**: Added `"core::option"` and `"core::result"` to `essential_library_modules`
- **Key insight**: `pending_generic_impls_` only stores ONE impl per type name (the first one found at `generate.cpp:552`). Methods in later impl blocks must be found via the imported module search fallback.

### 2. Bare Type Parameter Inference
- **Symptom**: `ok_or[E]` produced `%struct.E` instead of `Str` for the `E` parameter
- **Root cause**: `method_impl.cpp:151-213` only handled type params inside generic type_args (e.g., `Maybe[U]`) and FuncType return types
- **Missing case**: Bare type parameter like `err: E` where E appears directly as parameter type
- **Fix**: Added at `method_impl.cpp:160-171` -- check `param_named.name == type_param && param_named.type_args.empty()`

### 3. contains() ref Dereference
- **Symptom**: `contains(value: ref U)` -- `cmp_val` was ptr but used in `icmp eq i32`
- **Root cause**: Inline codegen at `method_maybe.cpp:528` didn't dereference the ref argument
- **Fix**: Added ptr load when `cmp_arg_type == "ptr" && inner_llvm_type != "ptr"`

### 4. flatten() Return Type in Source
- **Symptom**: `Maybe[Maybe[I32]]` vs `Maybe[I32]` -- function returns double-wrapped type
- **Root cause**: `option.tml:658` had `func flatten(this) -> Maybe[T]` but should be `-> T`
- **Fix**: Changed return type to `T` in `option.tml`

### 5. Enum Unit Variant Semantic Type (THE TRICKY ONE)
- **Symptom**: Call to `flatten` on `Nothing` produces `%struct.Maybe` (opaque) return type
- **Debug trace showed**: `type_subs={T=Maybe}` and `receiver_named=Maybe[Maybe, I32]`
  - The type args were FLAT (two separate args) instead of NESTED (one arg with inner)
- **Root cause**: `llvm_ir_gen_stmt.cpp:616` stored `nullptr` as `semantic_type` in `VarInfo` for generic enum unit variants
  - The `let x: Maybe[Maybe[I32]] = Nothing` path at line 574-624 handles unit variants specially
  - It correctly generates the enum value and alloca
  - BUT stores `VarInfo{alloca_reg, var_type, nullptr, ...}` -- the `nullptr` semantic_type
- **Impact**: When `infer_expr_type` later resolves `x`, it falls back to LLVM type string parsing
  - LLVM type `%struct.Maybe__Maybe__I32` is parsed by splitting on `__` which produces flat args
  - Instead of nested `Maybe[Maybe[I32]]`, it produces `Maybe[Maybe, I32]`
  - This corrupts `type_subs` so `T -> Maybe` instead of `T -> Maybe[I32]`
- **Fix**: Changed `nullptr` to `semantic_var_type` (which comes from the type annotation resolution)

### 6. filter() Primitive vs Struct Path
- **Issue**: `ref T` closure parameter has conflicting requirements
  - `do(x) x >= 5` (auto-deref) needs parameter bound as direct value
  - `do(v: ref I32) *v > 0` (explicit deref) needs parameter bound as pointer
- **Decision**: Kept primitive path (direct value) for backwards compat with existing `option.test.tml`
- **Future**: Need auto-deref support in binary comparisons for ptr types with ref semantic_type

## Key Debugging Technique
Added IR comment tracing in `method_impl.cpp` to print:
- `return_type_before_sub` -- the func_sig return type
- `type_subs` -- the substitution map
- `receiver_named` -- the receiver's NamedType with type_args

This immediately revealed the flat type args bug (`Maybe[Maybe, I32]` vs `Maybe[Maybe[I32]]`).

## Files Modified
- `compiler/src/codegen/llvm/core/runtime_modules.cpp` -- essential_library_modules
- `compiler/src/codegen/llvm/expr/method_impl.cpp` -- bare type param inference
- `compiler/src/codegen/llvm/expr/method_maybe.cpp` -- contains ref deref, filter paths
- `compiler/src/codegen/llvm/llvm_ir_gen_stmt.cpp` -- semantic_type for enum unit variants
- `lib/core/src/option.tml` -- flatten return type
- `lib/core/tests/option/option_filter.test.tml` -- adapted to auto-deref convention
