# Proposal: Bootstrap Borrow Checker

## Why

The borrow checker is TML's memory safety guarantee mechanism. It ensures that references are always valid, prevents data races, and enforces ownership rules at compile-time without requiring a garbage collector. This is essential for TML's goal of safe, efficient systems programming. The borrow checker analyzes the Typed AST to verify that all borrows are valid and that owned values are not used after being moved.

## What Changes

### New Components

1. **Ownership Model** (`src/borrow/ownership.hpp`)
   - Ownership tracking for values
   - Move semantics
   - Copy vs Move distinction

2. **Borrow Tracker** (`src/borrow/tracker.hpp`, `src/borrow/tracker.cpp`)
   - Active borrow tracking
   - Borrow scope management
   - Mutable vs shared borrow rules

3. **Lifetime Analysis** (`src/borrow/lifetime.hpp`, `src/borrow/lifetime.cpp`)
   - Lifetime inference
   - Lifetime constraint generation
   - Lifetime outlives checking

4. **Control Flow Graph** (`src/borrow/cfg.hpp`, `src/borrow/cfg.cpp`)
   - CFG construction from AST
   - Path-sensitive analysis
   - Loop and branch handling

5. **Borrow Checker** (`src/borrow/checker.hpp`, `src/borrow/checker.cpp`)
   - Main borrow checking pass
   - Error detection and reporting
   - Non-lexical lifetimes (NLL)

### Borrow Rules

Based on `docs/specs/06-MEMORY.md`:
- At any time, you can have either:
  - One mutable reference (&mut T)
  - Any number of shared references (&T)
- References must not outlive their referent
- Values cannot be used after being moved
- Copy types are copied rather than moved

## Impact

- **Affected specs**: 06-MEMORY.md, 16-COMPILER-ARCHITECTURE.md
- **Affected code**: New `src/borrow/` directory
- **Breaking change**: NO (new component)
- **User benefit**: Memory safety without garbage collection
- **Dependencies**: Requires bootstrap-type-checker to be complete

## Success Criteria

1. Move semantics are correctly enforced
2. Borrow rules prevent simultaneous mutable borrows
3. Lifetimes are correctly inferred
4. Use-after-move is detected
5. Dangling reference prevention works
6. Error messages explain the violation clearly
7. Test coverage ≥95%
