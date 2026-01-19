# Tasks: Memory Safety Parity with Rust

**Status**: Not Started (0%)

**Priority**: High - Core language safety guarantees

## Phase 1: Closure Capture Analysis

- [ ] 1.1 Add `CaptureKind` enum: ByRef, ByMutRef, ByMove, ByCopy
- [ ] 1.2 Implement capture analysis pass (`closure_capture.cpp`)
- [ ] 1.3 Track free variables used inside closure body
- [ ] 1.4 Determine capture kind based on usage (read → ByRef, write → ByMutRef, consume → ByMove)
- [ ] 1.5 Emit B014 error: "closure captures moved value"
- [ ] 1.6 Emit B015 error: "closure captures mutable ref while outer scope borrows"
- [ ] 1.7 Support `move do(x) { ... }` syntax for explicit move capture
- [ ] 1.8 Add closure capture tests

## Phase 2: Partial Move Improvements

- [ ] 2.1 Replace `moved_fields: Set<String>` with `moved_places: Set<PlaceId>`
- [ ] 2.2 Support recursive struct field tracking (`a.b.c` as projection chain)
- [ ] 2.3 Handle tuple element moves: `let (a, _) = pair`
- [ ] 2.4 Handle enum variant field moves in pattern matching
- [ ] 2.5 Track array element moves when index is constant
- [ ] 2.6 Emit B016 error: "use of partially moved value"
- [ ] 2.7 Add partial move tests (nested structs, tuples, enums)

## Phase 3: Interior Mutability Modeling

- [ ] 3.1 Add `@interior_mutable` type attribute
- [ ] 3.2 Track `InteriorMutable` capability in `TypeInfo`
- [ ] 3.3 Mark `Shared[T]`, `Sync[T]`, `Cell[T]`, `Mutex[T]` as interior mutable
- [ ] 3.4 Allow shared ref to interior mutable type to mutate
- [ ] 3.5 Emit warning W001: "interior mutability bypasses borrow checking"
- [ ] 3.6 Document interior mutability patterns in `06-MEMORY.md`
- [ ] 3.7 Add interior mutability tests

## Phase 4: Generic Ownership Bounds

- [ ] 4.1 Add `Duplicate` behavior to core (Copy equivalent)
- [ ] 4.2 Add `Owned` marker behavior (non-Copy types)
- [ ] 4.3 Parse `[T: Duplicate]` syntax in type parameters
- [ ] 4.4 Enforce Duplicate bound in type checker
- [ ] 4.5 Infer Copy/Move for generic instantiations
- [ ] 4.6 Emit E030 error: "type does not satisfy Duplicate bound"
- [ ] 4.7 Update `is_copy_type()` to check behavior implementations
- [ ] 4.8 Add generic bounds tests

## Phase 5: Reborrow Tracking

- [ ] 5.1 Add `Reborrow` struct with origin, lifetime, depth
- [ ] 5.2 Track reborrow chains in `BorrowEnv`
- [ ] 5.3 Validate reborrow doesn't outlive origin
- [ ] 5.4 Handle implicit reborrow in function calls
- [ ] 5.5 Support explicit reborrow: `let r2 = mut ref *r1`
- [ ] 5.6 Emit B017 error: "reborrow outlives original borrow"
- [ ] 5.7 Add reborrow chain tests

## Phase 6: Two-Phase Borrow Enhancement

- [ ] 6.1 Replace boolean flag with `TwoPhaseState` enum (Reserved, Active)
- [ ] 6.2 Implement reservation phase (prepare but don't activate)
- [ ] 6.3 Implement activation phase (on actual use)
- [ ] 6.4 Allow shared borrows during reservation phase
- [ ] 6.5 Support method calls: `v.push(v.len())`
- [ ] 6.6 Support index assignment: `map[key] = compute(&map)`
- [ ] 6.7 Add two-phase borrow tests

## Phase 7: Lifetime Elision Improvements

- [ ] 7.1 Implement Rule 1: each input ref gets own lifetime
- [ ] 7.2 Implement Rule 2: single input ref → output uses same lifetime
- [ ] 7.3 Implement Rule 3: `self`/`this` ref → output uses self lifetime
- [ ] 7.4 Detect ambiguous cases (multiple inputs, no self)
- [ ] 7.5 Emit E031 error with clear explanation when ambiguous
- [ ] 7.6 Suggest workaround in error message
- [ ] 7.7 Add lifetime elision tests

## Phase 8: Optional Explicit Lifetimes

- [ ] 8.1 Design `life` keyword for lifetime parameters
- [ ] 8.2 Parse `func foo[life a](x: ref[a] T) -> ref[a] T`
- [ ] 8.3 Add `LifetimeParam` to function signature AST
- [ ] 8.4 Track named lifetimes in type checker
- [ ] 8.5 Validate lifetime relationships
- [ ] 8.6 Emit E032 error: "lifetime 'a does not live long enough"
- [ ] 8.7 Update `02-LEXICAL.md` with `life` keyword
- [ ] 8.8 Add explicit lifetime tests

## Phase 9: Higher-Kinded Lifetime Bounds

- [ ] 9.1 Parse `[T: life static]` syntax
- [ ] 9.2 Parse `[T: life a]` for named lifetime bounds
- [ ] 9.3 Add lifetime bounds to `TypeParam`
- [ ] 9.4 Validate type satisfies lifetime bound
- [ ] 9.5 Emit E033 error: "type may not live long enough"
- [ ] 9.6 Support `'static` equivalent via `life static`
- [ ] 9.7 Add lifetime bounds tests

## Phase 10: Error Message Improvements

- [ ] 10.1 Add "help" suggestions to B001 (use after move)
- [ ] 10.2 Add "help" suggestions to B002 (move while borrowed)
- [ ] 10.3 Add "help" suggestions to B003-B009 (borrow conflicts)
- [ ] 10.4 Add "help" suggestions to B010 (dangling reference)
- [ ] 10.5 Add "help" suggestions to B011-B013 (partial moves, overlaps)
- [ ] 10.6 Show borrow timeline in error output
- [ ] 10.7 Add "see also" links to documentation
- [ ] 10.8 Add error message quality tests

## Validation

- [ ] All 1577+ existing tests pass after each phase
- [ ] New test file: `compiler/tests/borrow/closure_capture.test.tml`
- [ ] New test file: `compiler/tests/borrow/partial_move.test.tml`
- [ ] New test file: `compiler/tests/borrow/interior_mut.test.tml`
- [ ] New test file: `compiler/tests/borrow/generic_bounds.test.tml`
- [ ] New test file: `compiler/tests/borrow/reborrow.test.tml`
- [ ] New test file: `compiler/tests/borrow/two_phase.test.tml`
- [ ] New test file: `compiler/tests/borrow/lifetime_elision.test.tml`
- [ ] Documentation updated: `docs/06-MEMORY.md`
- [ ] No performance regression in borrow checking phase

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Closure Capture Analysis | Not Started | 0/8 |
| 2 | Partial Move Improvements | Not Started | 0/7 |
| 3 | Interior Mutability Modeling | Not Started | 0/7 |
| 4 | Generic Ownership Bounds | Not Started | 0/8 |
| 5 | Reborrow Tracking | Not Started | 0/7 |
| 6 | Two-Phase Borrow Enhancement | Not Started | 0/7 |
| 7 | Lifetime Elision Improvements | Not Started | 0/7 |
| 8 | Optional Explicit Lifetimes | Not Started | 0/8 |
| 9 | Higher-Kinded Lifetime Bounds | Not Started | 0/7 |
| 10 | Error Message Improvements | Not Started | 0/8 |
