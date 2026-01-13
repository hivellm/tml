# Tasks: Compiler Fixes for Core Library Blockers

## Progress: 98% (52/53 tasks complete)

**Status**: Phase 7 COMPLETE - Closures implement Fn traits with full `.call()` method dispatch support

**Remaining**: Phase 5.2.8 - Codegen support for method dispatch on bounded generics (needs impl lookup during monomorphization)

## Phase 0: Impl Constant Lookup Fix (P0) - COMPLETED

### 0.1 Fix Impl Constants from Imported Modules
- [x] 0.1.1 Fix `compiler/src/codegen/core/runtime.cpp` to extract constants from impl blocks in imported modules
- [x] 0.1.2 Fix `compiler/src/types/checker/types.cpp` to lookup `Type::CONST` paths for primitive types
- [x] 0.1.3 Add `compiler/tests/compiler/core_num_import.test.tml` test file
- [x] 0.1.4 Verify `I32::MIN`, `I32::MAX` constants are accessible
- [x] 0.1.5 Verify all primitive MIN/MAX constants work (I8, I16, I32, I64, U8, U16, U32, U64)

## Phase 1: I8/I16 Negative Literal Returns (P0) - VALIDATED (NO FIX NEEDED)

### 1.1 Small Integer Negation - Already Working
- [x] 1.1.1 Verified negation codegen uses correct operand type
- [x] 1.1.2 Verified negative literals work with type suffixes (e.g., -42i8)
- [x] 1.1.3 Added `compiler/tests/compiler/i8_negative.test.tml` test file
- [x] 1.1.4 Verified `I8::MIN` (-128) returns correct i8 type
- [x] 1.1.5 Verified `I16::MIN` (-32768) returns correct i16 type
- [x] 1.1.6 Verified `core::num` imports work (after Phase 0 fix)

## Phase 2: I8/I16 Impl Method Type Mismatch (P0) - NOT APPLICABLE

### 2.1 Small Integer Operations - No Methods Exist Yet
- [x] 2.1.1 Verified I32 impl methods (abs, signum, pow) work correctly
- [x] 2.1.2 Noted: I8/I16 impl methods not yet implemented in core library
- [x] 2.1.3 Added `compiler/tests/compiler/i8_methods.test.tml` with constant/arithmetic tests
- [x] 2.1.4 I8.abs() not implemented - not a codegen bug
- [x] 2.1.5 Added `compiler/tests/compiler/i32_methods.test.tml` validating I32 methods

## Phase 3: Nested String Return Corruption (P1) - VALIDATED (NO FIX NEEDED)

### 3.1 String Return Calling Convention - Already Working
- [x] 3.1.1 Verified string return handling works correctly
- [x] 3.1.2 Verified string lifetimes through nested calls
- [x] 3.1.3 Added `compiler/tests/compiler/nested_string_return.test.tml` test file
- [x] 3.1.4 Verified nested function calls returning Str work correctly
- [x] 3.1.5 All format behavior tests pass

## Phase 4: Default Behavior Method Dispatch (P1) - VALIDATED (NO FIX NEEDED)

### 4.1 Default Method Return Values - Already Working
- [x] 4.1.1 Verified default method lookup works correctly
- [x] 4.1.2 Verified return type propagation works
- [x] 4.1.3 Added `compiler/tests/compiler/default_method_dispatch.test.tml` test file
- [x] 4.1.4 Verified default methods return correct types
- [x] 4.1.5 All iterator consumer method tests pass

## Phase 5: Parameterized Behavior Bounds (P2) - PARSER + STORAGE DONE

### 5.1 Parser Support for Generic Bounds - COMPLETED
- [x] 5.1.1 Changed `GenericParam::bounds` from `TypePath` to `TypePtr` to support `Behavior[T]`
- [x] 5.1.2 Changed `AssociatedType::bounds` from `TypePath` to `TypePtr`
- [x] 5.1.3 Updated `parse_generic_params` to use `parse_type()` for bounds
- [x] 5.1.4 Updated type checker, formatter, and IR builder for new bounds type

### 5.2 Type Checker Support - PARTIAL
- [x] 5.2.1 Added `BoundConstraint` struct to `compiler/include/types/env.hpp` for parameterized bounds
- [x] 5.2.2 Updated `WhereConstraint` to include `parameterized_bounds` vector
- [x] 5.2.3 Updated `check_func_decl` in `core.cpp` to extract parameterized bounds from where clauses
- [x] 5.2.4 Updated `check_call` in `expr.cpp` to validate parameterized bounds
- [x] 5.2.5 Added `compiler/tests/compiler/parameterized_bounds.test.tml` test file
- [x] 5.2.6 Added `current_where_constraints_` to TypeChecker for method lookup context
- [x] 5.2.7 Added method lookup for bounded generics in `check_method_call` (type checker side)
- [ ] 5.2.8 Codegen support for method dispatch on bounded generics (needs impl lookup during monomorphization)

## Phase 6: TypeId Compiler Intrinsic (P2) - COMPLETED

### 6.1 Implement type_id[T]()
- [x] 6.1.1 Added `type_id` to intrinsics set in `compiler/src/codegen/builtins/intrinsics.cpp`
- [x] 6.1.2 Implemented FNV-1a hash of mangled type name for unique IDs
- [x] 6.1.3 Added type checker support in `compiler/src/types/checker/expr.cpp` for intrinsics
- [x] 6.1.4 Added `compiler/tests/compiler/type_id_intrinsic.test.tml` test file
- [x] 6.1.5 Verified `type_id[I32]() != type_id[I64]()` works correctly

## Phase 7: Fn Trait Auto-Implementation (P3) - COMPLETED

### 7.1 Current Closure Support (Function Pointers)
- [x] 7.1.1 Closures work as function pointer types `func(Args) -> Ret`
- [x] 7.1.2 Closures capture variables from enclosing scope
- [x] 7.1.3 Higher-order functions accept closures as `func(Args) -> Ret`
- [x] 7.1.4 Added `compiler/tests/compiler/closure_fn_traits.test.tml` documenting current functionality

### 7.2 Type Checker Support for Fn Bounds - COMPLETED
- [x] 7.2.1 Added `type_implements(TypePtr, behavior)` overload to recognize closures implement Fn/FnMut/FnOnce
- [x] 7.2.2 Updated `check_call` in `expr.cpp` to use TypePtr overload for bound checking
- [x] 7.2.3 Added support for `f.call(args)` method syntax in `check_method_call`
- [x] 7.2.4 Fixed parser to allow `Fn[Args]` as behavior type (not just `Fn(args)` function syntax)

### 7.3 Full Fn Trait Codegen - COMPLETED
- [x] 7.3.1 Closure struct types not needed - using function pointer representation directly
- [x] 7.3.2 Fn/FnMut/FnOnce recognized via type_implements for FuncType and ClosureType
- [x] 7.3.3 Codegen support for `.call()` method dispatch in `method.cpp` + fixed semantic_type in `llvm_ir_gen_stmt.cpp`

## Validation

- [x] V.1 All 970 TML tests pass (was 968) + C++ unit tests pass
- [x] V.2 `core::num` module can be imported without errors (fixed in Phase 0)
- [x] V.3 Iterator consumer methods return correct types
- [x] V.4 Format behaviors work with nested string returns
- [x] V.5 `type_id[T]()` generates unique IDs per type (Phase 6 complete)
- [x] V.6 Closures satisfy Fn trait bounds in type checker (Phase 7.2 complete)
- [x] V.7 `.call()` method dispatch works for closures and function pointers (Phase 7.3 complete)

## Summary

| Phase | Description | Priority | Status | Tasks |
|-------|-------------|----------|--------|-------|
| 0 | Impl Constant Lookup | P0 | DONE | 5/5 |
| 1 | I8/I16 Negative Literals | P0 | VALIDATED | 6/6 |
| 2 | I8/I16 Impl Methods | P0 | N/A | 5/5 |
| 3 | String Return Corruption | P1 | VALIDATED | 5/5 |
| 4 | Default Method Dispatch | P1 | VALIDATED | 5/5 |
| 5 | Parameterized Bounds | P2 | Partial | 7/8 |
| 6 | TypeId Intrinsic | P2 | DONE | 5/5 |
| 7 | Fn Trait Auto-Impl | P3 | DONE | 11/11 |
| **Total** | | | | **52/53** |

## Key Finding

The primary blocker was **impl constant lookup** for imported modules. The codegen was not extracting constants from `impl` blocks when processing imported modules (like `core::num::*`). This caused `I32::MIN`, `I32::MAX`, etc. to resolve to `()` (Unit) instead of the correct integer type.

**Fix applied:**
1. `compiler/src/codegen/core/runtime.cpp` - Extract impl constants from imported modules
2. `compiler/src/types/checker/types.cpp` - Handle `Type::CONST` path lookups for primitive types

The other P0/P1 issues (I8/I16 negation, string corruption, default methods) were either already working or not applicable.
