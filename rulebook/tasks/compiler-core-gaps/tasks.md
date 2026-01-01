# Tasks: Fix Core Compiler Gaps

**Status**: In progress

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

## Phase 2: Drop/RAII Implementation 🟡 IN PROGRESS

### 2.1 Drop Behavior Definition ✅
- [x] 2.1.1 Define `Drop` behavior in lib/core/src/ops.tml
```tml
pub behavior Drop {
    func drop(mut this)
}
```
- [x] 2.1.2 Register Drop as known behavior in type checker (compiler/src/types/builtins/types.cpp)

### 2.2 Drop Call Generation 🟡 PARTIAL
- [x] 2.2.1 Track which types implement Drop (via `type_needs_drop()` in env_lookups.cpp)
- [x] 2.2.2 Generate drop calls at end of variable scope (MIR builder: build_block)
- [x] 2.2.3 Generate drop calls on early return (MIR builder: build_return)
- [ ] 2.2.4 Generate drop calls on panic/unwind paths
- [x] 2.2.5 Drop scope tracking with `mark_moved()` for partial moves

### 2.3 Drop Order ✅
- [x] 2.3.1 Implement reverse declaration order for drops (LIFO in BuildContext::get_drops_for_current_scope)
- [x] 2.3.2 Handle nested drops (struct fields) - Implemented in emit_drop_for_value()
- [x] 2.3.3 Handle array element drops - Implemented in emit_drop_for_value()

### 2.4 Drop Tests
- [ ] 2.4.1 Test basic scope exit drop
- [ ] 2.4.2 Test early return drop
- [ ] 2.4.3 Test conditional drop (if/when branches)
- [ ] 2.4.4 Test loop drop behavior
- [ ] 2.4.5 Test struct with Drop fields

## Phase 3: Dynamic Slices 🟡 IN PROGRESS

### 3.1 Slice Type Representation 🟡 PARTIAL
- [x] 3.1.1 Define slice as fat pointer: `{ ptr, i64 }` in LLVM IR (codegen/core/types.cpp)
- [x] 3.1.2 Type checker recognizes `[T]` as slice type (SliceType in type.hpp)
- [x] 3.1.3 Distinguish between `[T; N]` (array) and `[T]` (slice) - both exist as separate types

### 3.2 Slice Creation 🟡 PARTIAL
- [x] 3.2.1 Implement array-to-slice coercion: `arr.as_slice()` -> `Slice[T]` (lib/core/src/array/mod.tml)
- [x] 3.2.2 Type compatibility for `[T; N]` -> `[T]` in types_compatible() (helpers.cpp)
- [ ] 3.2.3 Automatic coercion in function calls (codegen not complete)
- [ ] 3.2.4 Implement slice from pointer+length: `Slice::from_raw(ptr, len)`

### 3.3 Slice Operations Codegen ✅
- [x] 3.3.1 Implement `slice.len()` - load len field (lib/core/src/slice/mod.tml)
- [x] 3.3.2 Implement `slice[i]` via slice_get/slice_get_mut intrinsics
- [x] 3.3.3 Implement `slice.get(i)` - safe access returning Maybe (lib/core/src/slice/mod.tml)
- [x] 3.3.4 Implement slice_set, slice_offset, slice_swap intrinsics
  - Added intrinsic declarations to lib/core/src/intrinsics.tml
  - Added codegen to compiler/src/codegen/builtins/intrinsics.cpp

### 3.4 Slice Parameter Passing
- [ ] 3.4.1 Handle `ref [T]` in function parameters
- [ ] 3.4.2 Handle `mut ref [T]` for mutable slice access
- [ ] 3.4.3 Generate correct calling convention (pass fat pointer)

### 3.5 Slice Tests
- [ ] 3.5.1 Test slice creation from array
- [ ] 3.5.2 Test slice indexing
- [ ] 3.5.3 Test slice in function parameter
- [ ] 3.5.4 Test slice iteration
- [ ] 3.5.5 Test mutable slice modification

## Phase 4: Error Propagation (`!` Operator) 🟡 IN PROGRESS

Note: TML uses `!` instead of `?` for error propagation (more visible for LLMs)

### 4.1 Parser Verification ✅
- [x] 4.1.1 TryExpr (`expr!`) is parsed correctly (parser_expr.cpp:356-361)
- [x] 4.1.2 TryExpr tests exist in parser_test.cpp

### 4.2 Type Checking 🟡 PARTIAL
- [x] 4.2.1 Validate expression type is Outcome[T, E] or Maybe[T] (checker/types.cpp)
- [x] 4.2.2 Return inner type T on success
- [x] 4.2.3 Report error if `!` used on non-Outcome/Maybe type
- [ ] 4.2.4 Verify function return type is compatible (Outcome[_, E] matches)

### 4.3 Codegen for `!` 🟡 PARTIAL
- [x] 4.3.1 Generate: extract tag from Outcome/Maybe (codegen/expr/try.cpp)
- [x] 4.3.2 Generate: if Err/Nothing, early return with error value
- [x] 4.3.3 Generate: if Ok/Just, extract and continue with value
- [ ] 4.3.4 Handle drop of locals on early return (needs MIR integration)
- [ ] 4.3.5 Proper error type conversion between compatible error types

### 4.4 Try Tests
- [ ] 4.4.1 Test basic `!` propagation
- [ ] 4.4.2 Test chained `!` operators
- [ ] 4.4.3 Test `!` with Maybe type
- [ ] 4.4.4 Test error message when misused

## Phase 5: Complete Trait Objects

### 5.1 Vtable Generation Audit
- [ ] 5.1.1 Review current vtable generation code
- [ ] 5.1.2 Identify failing cases
- [ ] 5.1.3 Fix method pointer resolution for all method types

### 5.2 Trait Object Layout
- [ ] 5.2.1 Verify fat pointer layout: `{ data: *T, vtable: *Vtable }`
- [ ] 5.2.2 Handle generic behaviors in trait objects
- [ ] 5.2.3 Handle associated types in trait objects

### 5.3 Dynamic Dispatch Codegen
- [ ] 5.3.1 Generate vtable lookup for method calls
- [ ] 5.3.2 Handle `this` pointer adjustment
- [ ] 5.3.3 Handle method with generic parameters (monomorphize at call site)

### 5.4 Trait Object Tests
- [ ] 5.4.1 Test basic dyn Behavior usage
- [ ] 5.4.2 Test dyn Behavior in Vec/array
- [ ] 5.4.3 Test dyn Behavior as function parameter
- [ ] 5.4.4 Test dyn Behavior with multiple methods
- [ ] 5.4.5 Test dyn Behavior with inherited behaviors

## Validation

### Integration Tests
- [ ] V.1 Borrow errors block compilation
- [ ] V.2 File handles auto-close on scope exit (Drop)
- [ ] V.3 `func sum(data: ref [I32]) -> I32` works
- [ ] V.4 `let x = parse(input)?` works
- [ ] V.5 `let handlers: [dyn Handler; 3]` works

### Regression Tests
- [ ] V.6 All existing tests still pass
- [ ] V.7 No performance regression in codegen

### Documentation
- [ ] V.8 Update README with working features
- [ ] V.9 Add examples for Drop, slices, `?` operator
- [ ] V.10 Update lib/core documentation
