# Tasks: Compiler Fixes for Core Library Blockers

## Progress: 0% (0/24 tasks complete)

**Status**: Pending - Compiler bugs and missing features blocking core library completion

## Phase 1: I8/I16 Negative Literal Returns (P0)

### 1.1 Fix Negation Codegen for Small Integers
- [ ] 1.1.1 Fix `compiler/src/codegen/expr/unary.cpp` to use target return type for negation
- [ ] 1.1.2 Fix negative literal codegen to produce correct LLVM type (i8/i16, not i32)
- [ ] 1.1.3 Add `compiler/tests/compiler/i8_i16_negation.test.tml` test file
- [ ] 1.1.4 Verify `I8::MIN` (-128) returns correct i8 type
- [ ] 1.1.5 Verify `I16::MIN` (-32768) returns correct i16 type
- [ ] 1.1.6 Verify `core::num` can be imported without codegen errors

## Phase 2: I8/I16 Impl Method Type Mismatch (P0)

### 2.1 Fix Small Integer Operations
- [ ] 2.1.1 Fix `compiler/src/codegen/expr/binary.cpp` to maintain i8/i16 types through operations
- [ ] 2.1.2 Fix `compiler/src/codegen/expr/unary.cpp` to truncate results back to original type
- [ ] 2.1.3 Add `compiler/tests/compiler/i8_i16_impl_methods.test.tml` test file
- [ ] 2.1.4 Verify `I8.abs()` works correctly
- [ ] 2.1.5 Verify `U8` bitwise operations work correctly

## Phase 3: Nested String Return Corruption (P1)

### 3.1 Fix String Return Calling Convention
- [ ] 3.1.1 Fix `compiler/src/codegen/llvm_ir_gen_stmt.cpp` Str return handling
- [ ] 3.1.2 Fix `compiler/src/codegen/expr/call.cpp` Str return value lifetime
- [ ] 3.1.3 Add `compiler/tests/compiler/string_nested_return.test.tml` test file
- [ ] 3.1.4 Verify nested function calls returning Str work correctly
- [ ] 3.1.5 Verify format behavior tests (Binary, Octal, Hex) pass

## Phase 4: Default Behavior Method Dispatch (P1)

### 4.1 Fix Default Method Return Values
- [ ] 4.1.1 Fix `compiler/src/codegen/expr/method.cpp` default method lookup
- [ ] 4.1.2 Fix `compiler/src/types/checker/method.cpp` return type propagation
- [ ] 4.1.3 Add `compiler/tests/compiler/default_method_dispatch.test.tml` test file
- [ ] 4.1.4 Verify `Iterator::count()` returns I64 (not Unit)
- [ ] 4.1.5 Verify all iterator consumer method tests pass

## Phase 5: Parameterized Behavior Bounds (P2)

### 5.1 Parser Support for Generic Bounds
- [ ] 5.1.1 Update `compiler/src/parser/parser_generics.cpp` to parse `Behavior[T]` in where clauses
- [ ] 5.1.2 Update `compiler/src/types/checker/generics.cpp` to validate parameterized bounds
- [ ] 5.1.3 Add `compiler/tests/compiler/parameterized_bounds.test.tml` test file
- [ ] 5.1.4 Verify `collect[C: FromIterator[T]]()` parses and type-checks

## Phase 6: TypeId Compiler Intrinsic (P2)

### 6.1 Implement TypeId::of[T]()
- [ ] 6.1.1 Add `TypeId::of` intrinsic to `compiler/src/codegen/llvm_ir_gen_builtins.cpp`
- [ ] 6.1.2 Generate unique hash/ID for each monomorphized type
- [ ] 6.1.3 Add `compiler/tests/compiler/typeid_intrinsic.test.tml` test file
- [ ] 6.1.4 Verify `TypeId::of[I32]() != TypeId::of[I64]()` works

## Phase 7: Fn Trait Auto-Implementation (P3)

### 7.1 Auto-implement Fn Traits for Closures
- [ ] 7.1.1 Generate closure struct type in `compiler/src/codegen/expr/closure.cpp`
- [ ] 7.1.2 Implement Fn/FnMut/FnOnce based on capture mode
- [ ] 7.1.3 Add `compiler/tests/compiler/closure_fn_traits.test.tml` test file
- [ ] 7.1.4 Verify generic functions with `F: Fn` bounds work with closures

## Validation

- [ ] V.1 All 939 existing tests continue to pass
- [ ] V.2 `core::num` module can be imported without errors
- [ ] V.3 Iterator consumer methods return correct types
- [ ] V.4 Format behaviors work with nested string returns
- [ ] V.5 `TypeId::of[T]()` generates unique IDs per type

## Summary

| Phase | Description | Priority | Tasks |
|-------|-------------|----------|-------|
| 1 | I8/I16 Negative Literals | P0 | 6 |
| 2 | I8/I16 Impl Methods | P0 | 5 |
| 3 | String Return Corruption | P1 | 5 |
| 4 | Default Method Dispatch | P1 | 5 |
| 5 | Parameterized Bounds | P2 | 4 |
| 6 | TypeId Intrinsic | P2 | 4 |
| 7 | Fn Trait Auto-Impl | P3 | 4 |
| **Total** | | | **33** |
