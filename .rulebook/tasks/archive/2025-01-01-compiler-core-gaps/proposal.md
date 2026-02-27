# Proposal: Fix Core Compiler Gaps

## Status
- **Created**: 2025-12-30
- **Status**: Draft
- **Priority**: Critical

## Why

A detailed comparison between TML and production compilers (Rust, Go) revealed critical gaps that prevent TML from being usable for real programs. While the compiler has solid foundations (parser, type checker, codegen), several core features are incomplete or missing.

### Gap Analysis Summary

| Feature | TML Status | Rust | Go | Impact |
|---------|------------|------|-----|--------|
| Borrow checker enforcement | Warnings only | Errors | N/A | HIGH - Memory safety |
| Drop/RAII | Not implemented | Full | N/A (GC) | HIGH - Resource leaks |
| Dynamic slices `&[T]` | Parser only | Full | Full | HIGH - Common pattern |
| Error propagation `?` | Parser only | Full | N/A | MEDIUM - Ergonomics |
| Async/await | Parser only | Full | Goroutines | MEDIUM - Concurrency |
| Default trait methods | Parser only | Full | Full | LOW - Convenience |

### Current Problems

1. **Borrow checker doesn't block compilation** - Unsafe code compiles without errors
2. **No Drop trait** - Resources (files, memory) leak
3. **Slices are broken** - Can't pass `&[T]` to functions
4. **`?` operator doesn't work** - Manual error handling required
5. **Trait objects incomplete** - `dyn Behavior` doesn't work in all cases

## What Changes

This task focuses on making the existing parsed features actually work in codegen:

### Phase 1: Borrow Checker Enforcement
Make borrow check errors fatal (block compilation)

### Phase 2: Drop/RAII Implementation
Implement automatic cleanup for types with Drop behavior

### Phase 3: Dynamic Slices
Make `&[T]` / `ref [T]` work in function parameters and returns

### Phase 4: Error Propagation
Implement `?` operator for Outcome types

### Phase 5: Complete Trait Objects
Fix `dyn Behavior` codegen for all cases

## Impact

- **Breaking**: Code with borrow errors will stop compiling
- **Compatibility**: Existing working code remains compatible
- **Safety**: Prevents memory/resource leaks
- **Ergonomics**: Standard error handling patterns work

## Success Criteria

1. Borrow check failures block compilation
2. `Drop` behavior called automatically at scope exit
3. `func process(data: ref [I32]) -> I32` compiles and works
4. `let x = fallible_fn()?` works inside `-> Outcome[T,E]` functions
5. `func handler(h: dyn Handler)` works with vtable dispatch

## References

- Rust Drop: https://doc.rust-lang.org/std/ops/trait.Drop.html
- Rust slices: https://doc.rust-lang.org/book/ch04-03-slices.html
- Rust ? operator: https://doc.rust-lang.org/book/ch09-02-recoverable-errors-with-result.html
