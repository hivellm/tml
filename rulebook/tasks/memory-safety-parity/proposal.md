# Proposal: Memory Safety Parity with Rust

## Why

TML's memory safety system is currently at ~85% feature parity with Rust. While the core guarantees are solid (no use-after-free, double-free, data races), several advanced features are missing that limit TML's ability to express complex safe patterns:

1. **No explicit lifetime parameters**: TML infers all lifetimes, but some patterns (multiple input refs returning a ref) are ambiguous without explicit annotation.

2. **Incomplete closure capture analysis**: Closures don't track whether they capture by move or by reference, potentially leading to unsafe semantics.

3. **Interior mutability not modeled**: Types like `Shared[T]` and `Sync[T]` bypass borrow checking because the checker doesn't understand interior mutability.

4. **Partial moves only string-based**: Field-level move tracking uses string matching instead of recursive struct analysis.

5. **Generic type bounds for ownership**: No way to constrain generic types to Copy/Move semantics (`[T: Duplicate]`).

6. **Incomplete reborrow tracking**: Complex mutable reborrow chains may allow invalid patterns.

7. **Two-phase borrows minimal**: Current implementation is a boolean flag, not full reservation semantics.

8. **Missing higher-kinded lifetime bounds**: No `where T: 'a` equivalent for constraining type lifetimes.

These gaps mean that while TML is safe for typical code, advanced libraries and APIs may hit limitations that Rust handles correctly.

## What Changes

### Phase 1: Closure Capture Analysis

Add capture analysis to track what closures capture and how:

- `compiler/src/borrow/closure_capture.cpp` - Capture analysis pass
- Track `CaptureKind`: ByRef, ByMutRef, ByMove, ByCopy
- Emit borrow checker errors for invalid captures
- Support `move` keyword for explicit capture semantics

### Phase 2: Partial Move Improvements

Enhance field-level move tracking:

- Replace string-based tracking with `PlaceId` projections
- Support recursive struct field moves
- Handle tuple element moves: `let (a, _) = pair`
- Handle enum variant field moves

### Phase 3: Interior Mutability Modeling

Model interior mutability for shared types:

- Add `@interior_mutable` attribute for types
- Track `InteriorMutable` capability in type system
- Warn when bypassing borrow rules via interior mutability
- Support `Shared[T]`, `Sync[T]`, `Cell[T]`, `Mutex[T]`

### Phase 4: Generic Ownership Bounds

Add ownership constraints to generics:

- `[T: Duplicate]` - T must be Copy
- `[T: Drop]` - T has custom destructor
- `[T: Owned]` - T is not Copy (move semantics)
- Update type checker to enforce bounds

### Phase 5: Reborrow Tracking

Properly track mutable reborrow chains:

- Track reborrow origin and lifetime
- Ensure reborrows don't outlive original
- Support nested reborrow patterns
- Handle reborrow in function calls

### Phase 6: Two-Phase Borrow Enhancement

Full two-phase borrow semantics:

- Reservation phase: prepare borrow, don't activate
- Activation phase: when actually used
- Support method calls: `v.push(v.len())`
- Support index expressions: `m[k] = compute(&m)`

### Phase 7: Lifetime Elision Improvements

Better lifetime inference without explicit syntax:

- Implement all three Rust elision rules correctly
- Handle `self`/`this` receiver lifetimes
- Infer output lifetime from single input ref
- Error clearly when ambiguous (suggest workaround)

### Phase 8: Optional Explicit Lifetimes (Design Decision)

If full parity needed, add opt-in lifetime syntax:

- Syntax: `func foo[life a](x: ref[a] T) -> ref[a] T`
- Only required when inference fails
- Backward compatible - existing code unchanged
- `life` keyword for lifetime parameters

### Phase 9: Higher-Kinded Lifetime Bounds

Add lifetime bounds for type parameters:

- `[T: 'static]` equivalent: `[T: life static]`
- `[T: 'a]` equivalent: `[T: life a]`
- Constrain how long type's references can live

### Phase 10: Error Message Improvements

Better diagnostics for all memory errors:

- Add "help" suggestions to all 13 error codes
- Show borrow timeline visualizations
- Suggest fixes for common patterns
- Link to documentation

## Impact

- **Affected specs**: `docs/06-MEMORY.md` - document new features
- **Affected code**:
  - Modified: `compiler/src/borrow/` - all borrow checker files
  - Modified: `compiler/src/types/` - ownership bounds
  - Modified: `compiler/include/borrow/` - new data structures
  - New: `compiler/src/borrow/closure_capture.cpp`
  - New: `compiler/src/borrow/reborrow.cpp`
- **Breaking change**: NO (all changes are additive)
- **User benefit**:
  - Express more patterns safely
  - Better error messages
  - Library authors have full flexibility
  - True 100% Rust safety parity

## Dependencies

- Borrow checker infrastructure (complete)
- Type system (complete)
- HIR/MIR representation (complete)
- NLL implementation (complete)

## Success Criteria

1. All existing tests pass
2. New test cases for each feature:
   - Closure capture tests (move, ref, mut ref)
   - Partial move tests (nested structs, tuples)
   - Interior mutability tests
   - Generic bounds tests
   - Reborrow chain tests
3. Error messages have suggestions for all codes
4. Documentation updated for new features
5. No performance regression in borrow checking
