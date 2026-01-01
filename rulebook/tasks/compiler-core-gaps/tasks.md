# Tasks: Fix Core Compiler Gaps

**Status**: Completed (Phases 1-5 done, documentation pending)

## Phase 1: Borrow Checker Enforcement ✅ COMPLETED

### 1.1 Make Borrow Errors Fatal ✅
- [x] 1.1.1 Integrated borrow checker into build pipeline
  - build.cpp, run.cpp, run_profiled.cpp, test_runner.cpp, parallel_build.cpp
- [x] 1.1.2 Borrow errors now block compilation (fatal errors)
- [x] 1.1.3 Added emit_borrow_error with Rust-style diagnostics (code B001)
- [x] 1.1.4 Related spans shown for conflicting borrows
- [x] 1.1.5 All 599 tests pass with borrow checker integrated

### 1.2 Improve Borrow Error Messages ✅
- [x] 1.2.1 Show conflicting borrow locations
  - Added `move_location` tracking in PlaceState for use-after-move errors
  - Enhanced BorrowError struct with BorrowErrorCode enum (B001-B013)
  - Added related_message field for contextual labels at related spans
  - Updated check_can_borrow, check_can_mutate, move_value, check_can_use
  - Related spans show exact location of conflicting borrows/moves
- [x] 1.2.2 Show lifetime scopes visually
  - Already implemented in diagnostic.cpp via emit_labeled_line/emit_source_snippet
  - Primary spans shown with `^` underlines in red, secondary with `-` in blue
  - Multiple lines shown with gaps indicated by `...`
  - Secondary labels show vertical pipe `|` connecting to messages
- [x] 1.2.3 Suggest fixes (e.g., "consider cloning here")
  - Added BorrowSuggestion struct with message and optional fix code
  - Static helpers: use_after_move, double_mut_borrow, mut_borrow_while_immut, etc.
  - emit_borrow_error now outputs "help:" notes with suggestions
  - Suggestions include: `.duplicate()`, `mut varname`, etc.

## Phase 2: Drop/RAII Implementation ✅ COMPLETED

### 2.1 Drop Behavior Definition ✅
- [x] 2.1.1 Define `Drop` behavior in lib/core/src/ops.tml
```tml
pub behavior Drop {
    func drop(mut this)
}
```
- [x] 2.1.2 Register Drop as known behavior in type checker (compiler/src/types/builtins/types.cpp)

### 2.2 Drop Call Generation ✅
- [x] 2.2.1 Track which types implement Drop (via `type_needs_drop()` in env_lookups.cpp)
- [x] 2.2.2 Generate drop calls at end of variable scope
  - AST codegen: drop.cpp with push/pop_drop_scope, emit_scope_drops in gen_block
  - MIR builder: build_block
- [x] 2.2.3 Generate drop calls on early return
  - AST codegen: emit_all_drops() in gen_return() and implicit returns
  - MIR builder: build_return
- [ ] 2.2.4 Generate drop calls on panic/unwind paths (deferred - requires exception handling)
- [x] 2.2.5 Drop scope tracking with `mark_moved()` for partial moves

### 2.3 Drop Order ✅
- [x] 2.3.1 Implement reverse declaration order for drops (LIFO)
- [x] 2.3.2 Handle nested drops (struct fields) - explicit drop responsible
- [x] 2.3.3 Handle array element drops - MIR builder supports

### 2.4 Drop Tests ✅
- [x] 2.4.1 Test basic scope exit drop (compiler/tests/compiler/drop.test.tml)
- [x] 2.4.2 Test early return drop
- [x] 2.4.3 Test conditional drop (if branches)
- [x] 2.4.4 Test loop drop behavior
- [x] 2.4.5 Test struct with Drop (explicit drop only)
- [x] 2.4.6 Test LIFO drop order (multiple resources in same scope)

## Phase 3: Dynamic Slices ✅ COMPLETED

### 3.1 Slice Type Representation ✅
- [x] 3.1.1 Define slice as fat pointer: `{ ptr, i64 }` in LLVM IR (codegen/core/types.cpp)
- [x] 3.1.2 Type checker recognizes `[T]` as slice type (SliceType in type.hpp)
- [x] 3.1.3 Distinguish between `[T; N]` (array) and `[T]` (slice) - both exist as separate types

### 3.2 Slice Creation ✅
- [x] 3.2.1 Implement array-to-slice coercion: `arr.as_slice()` -> `Slice[T]` (lib/core/src/array/mod.tml)
- [x] 3.2.2 Type compatibility for `[T; N]` -> `[T]` in types_compatible() (helpers.cpp)
- [ ] 3.2.3 Automatic coercion in function calls (deferred - explicit conversion preferred)
- [x] 3.2.4 Implement slice from pointer+length: `Slice::from_raw(ptr, len)` and `MutSlice::from_raw`

### 3.3 Slice Operations Codegen ✅
- [x] 3.3.1 Implement `slice.len()` - load len field with inline codegen (method.cpp)
- [x] 3.3.2 Implement `slice[i]` via slice_get/slice_get_mut intrinsics
- [x] 3.3.3 Implement `slice.get(i)` - safe access returning Maybe (lib/core/src/slice/mod.tml)
- [x] 3.3.4 Implement slice_set, slice_offset, slice_swap intrinsics
  - Added intrinsic declarations to lib/core/src/intrinsics.tml
  - Added codegen to compiler/src/codegen/builtins/intrinsics.cpp
- [x] 3.3.5 Fixed struct literal codegen to properly cast integer types for i64 fields

### 3.4 Slice Parameter Passing ✅
- [x] 3.4.1 Slice types work as struct types with `{ ptr, len }` layout
- [x] 3.4.2 Pass by value - slice is fat pointer (16 bytes)
- [x] 3.4.3 Reference semantics via explicit `ref Slice[T]` if needed

### 3.5 Slice Tests ✅
- [x] 3.5.1 Test slice creation (lib/core/tests/slice.test.tml: test_slice_len_basic, test_empty_slice)
- [x] 3.5.2 Test slice indexing via raw pointer (test_slice_indexing)
- [ ] 3.5.3 Test slice in function parameter (deferred - uses same struct passing)
- [ ] 3.5.4 Test slice iteration (deferred - requires generic impl method instantiation)
- [x] 3.5.5 Test mutable slice (test_mut_slice_len, test_mut_slice_basic)

**Note**: Generic impl method instantiation (e.g., `Slice[I32].get()` returning `Maybe[ref I32]`) requires additional work. Current tests use raw pointer access as workaround.

## Phase 4: Error Propagation (`!` Operator) ✅ COMPLETED

Note: TML uses `!` instead of `?` for error propagation (more visible for LLMs)

### 4.1 Parser Verification ✅
- [x] 4.1.1 TryExpr (`expr!`) is parsed correctly (parser_expr.cpp:356-361)
- [x] 4.1.2 TryExpr tests exist in parser_test.cpp

### 4.2 Type Checking ✅
- [x] 4.2.1 Validate expression type is Outcome[T, E] or Maybe[T] (checker/types.cpp)
- [x] 4.2.2 Return inner type T on success
- [x] 4.2.3 Report error if `!` used on non-Outcome/Maybe type
- [x] 4.2.4 Type inference from mangled type names (Outcome__I32__Str)

### 4.3 Codegen for `!` ✅
- [x] 4.3.1 Generate: extract tag from Outcome/Maybe (codegen/expr/try.cpp)
- [x] 4.3.2 Generate: if Err/Nothing, early return with error value
- [x] 4.3.3 Generate: if Ok/Just, extract and continue with value
- [x] 4.3.4 Handle drop of locals on early return (emit_all_drops in try.cpp)
- [ ] 4.3.5 Proper error type conversion between compatible error types (deferred)

### 4.4 Try Tests ✅
- [x] 4.4.1 Test basic `!` propagation (compiler/tests/compiler/try_operator.test.tml)
- [x] 4.4.2 Test chained `!` operators
- [x] 4.4.3 Test `!` with Maybe type
- [ ] 4.4.4 Test error message when misused (requires type checker error tests)

## Phase 5: Complete Trait Objects ✅ COMPLETED

### 5.1 Vtable Generation Audit ✅
- [x] 5.1.1 Review current vtable generation code (dyn.cpp)
- [x] 5.1.2 Identify failing cases (multi-method behaviors had wrong vtable type)
- [x] 5.1.3 Fix method pointer resolution for all method types
  - Fixed vtable struct type to match method count: `{ ptr, ptr, ... }`
  - Set last_expr_type_ after dyn dispatch

### 5.2 Trait Object Layout ✅
- [x] 5.2.1 Verify fat pointer layout: `%dyn.BehaviorName = type { ptr, ptr }`
  - Field 0: data pointer to concrete type
  - Field 1: vtable pointer
- [ ] 5.2.2 Handle generic behaviors in trait objects (deferred - requires generic instantiation)
- [ ] 5.2.3 Handle associated types in trait objects (deferred - requires associated type support)

### 5.3 Dynamic Dispatch Codegen ✅
- [x] 5.3.1 Generate vtable lookup for method calls (method.cpp lines 485-540)
- [x] 5.3.2 Handle `this` pointer adjustment (data_ptr passed to method)
- [ ] 5.3.3 Handle method with generic parameters (deferred - monomorphize at call site)

### 5.4 Trait Object Tests ✅
- [x] 5.4.1 Test basic dyn Behavior usage (dyn_basic.test.tml)
- [x] 5.4.2 Test dyn Behavior heterogeneous dispatch (dyn_array.test.tml)
- [x] 5.4.3 Test dyn Behavior as function parameter (dyn.test.tml, dyn_advanced.test.tml)
- [x] 5.4.4 Test dyn Behavior with multiple methods (dyn_advanced.test.tml - Shape with area, perimeter, name)
- [ ] 5.4.5 Test dyn Behavior with inherited behaviors (deferred - requires behavior inheritance)

## Validation

### Integration Tests
- [x] V.1 Borrow errors block compilation (borrow_test.cpp in C++ tests)
- [x] V.2 File handles auto-close on scope exit (Drop) - drop.test.tml
- [x] V.3 Slices work with Slice[T] struct type (slice.test.tml)
- [x] V.4 `expr!` works for Outcome/Maybe propagation (try_operator.test.tml)
- [x] V.5 dyn Behavior works with multiple methods (dyn_advanced.test.tml)
- [x] V.6 dyn Behavior as function parameters works (dyn_array.test.tml)

### Regression Tests
- [x] V.7 All existing tests still pass (728+ tests pass)
- [x] V.8 No performance regression in codegen (tests complete in ~267ms)

### Documentation
- [ ] V.8 Update README with working features
- [ ] V.9 Add examples for Drop, slices, `!` operator
- [ ] V.10 Update lib/core documentation
