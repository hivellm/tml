# Tasks: Fix Core Compiler Gaps

## Phase 1: Borrow Checker Enforcement

### 1.1 Make Borrow Errors Fatal
- [ ] 1.1.1 Find where borrow check results are reported (compiler/src/borrow/)
- [ ] 1.1.2 Change borrow violations from warnings to errors
- [ ] 1.1.3 Add `--allow-borrow-warnings` flag for gradual migration
- [ ] 1.1.4 Update error messages to be actionable
- [ ] 1.1.5 Add tests that verify compilation fails on borrow errors

### 1.2 Improve Borrow Error Messages
- [ ] 1.2.1 Show conflicting borrow locations
- [ ] 1.2.2 Show lifetime scopes visually
- [ ] 1.2.3 Suggest fixes (e.g., "consider cloning here")

## Phase 2: Drop/RAII Implementation

### 2.1 Drop Behavior Definition
- [ ] 2.1.1 Define `Drop` behavior in lib/core/src/ops.tml
```tml
pub behavior Drop {
    pub func drop(mut this)
}
```
- [ ] 2.1.2 Register Drop as known behavior in type checker

### 2.2 Drop Call Generation
- [ ] 2.2.1 Track which types implement Drop (via impl resolution)
- [ ] 2.2.2 Generate drop calls at end of variable scope
- [ ] 2.2.3 Generate drop calls on early return
- [ ] 2.2.4 Generate drop calls on panic/unwind paths
- [ ] 2.2.5 Handle partial moves (don't drop moved fields)

### 2.3 Drop Order
- [ ] 2.3.1 Implement reverse declaration order for drops
- [ ] 2.3.2 Handle nested drops (struct fields)
- [ ] 2.3.3 Handle array element drops

### 2.4 Drop Tests
- [ ] 2.4.1 Test basic scope exit drop
- [ ] 2.4.2 Test early return drop
- [ ] 2.4.3 Test conditional drop (if/when branches)
- [ ] 2.4.4 Test loop drop behavior
- [ ] 2.4.5 Test struct with Drop fields

## Phase 3: Dynamic Slices

### 3.1 Slice Type Representation
- [ ] 3.1.1 Define slice as fat pointer: `{ ptr: *T, len: I64 }`
- [ ] 3.1.2 Update type checker to recognize `[T]` as slice type
- [ ] 3.1.3 Distinguish between `[T; N]` (array) and `[T]` (slice)

### 3.2 Slice Creation
- [ ] 3.2.1 Implement array-to-slice coercion: `arr.as_slice()` -> `&[T]`
- [ ] 3.2.2 Implement automatic coercion `&[T; N]` -> `&[T]` in function calls
- [ ] 3.2.3 Implement slice from pointer+length: `Slice::from_raw(ptr, len)`

### 3.3 Slice Operations Codegen
- [ ] 3.3.1 Implement `slice.len()` - load len field
- [ ] 3.3.2 Implement `slice[i]` - bounds check + GEP + load
- [ ] 3.3.3 Implement `slice.get(i)` - safe access returning Maybe
- [ ] 3.3.4 Implement slice iteration

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

## Phase 4: Error Propagation (`?` Operator)

### 4.1 Parser Verification
- [ ] 4.1.1 Verify TryExpr (`expr?`) is parsed correctly
- [ ] 4.1.2 Add missing TryExpr test cases

### 4.2 Type Checking
- [ ] 4.2.1 Verify function return type is Outcome[T, E]
- [ ] 4.2.2 Verify expression type is Outcome[U, E] with compatible E
- [ ] 4.2.3 Handle Maybe[T] with `?` (convert Nothing to error)
- [ ] 4.2.4 Report clear error if `?` used outside Outcome-returning function

### 4.3 Codegen for `?`
- [ ] 4.3.1 Generate: extract tag from Outcome
- [ ] 4.3.2 Generate: if Err, early return with error
- [ ] 4.3.3 Generate: if Ok, extract and continue with value
- [ ] 4.3.4 Handle drop of locals on early return

### 4.4 Try Tests
- [ ] 4.4.1 Test basic `?` propagation
- [ ] 4.4.2 Test chained `?` operators
- [ ] 4.4.3 Test `?` with Maybe type
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
