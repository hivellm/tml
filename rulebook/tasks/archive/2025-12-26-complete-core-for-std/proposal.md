# Proposal: Complete Core Features for std Implementation

## Why

The TML compiler core must be complete enough to implement the standard library (`std`) entirely in TML without requiring additional core changes. Currently, several critical features are missing or broken that prevent writing low-level code, working with memory, implementing iterators, and using closures with captured variables.

Without these features:
- Cannot implement collections (Vec, HashMap) - need pointer operations
- Cannot implement Option/Result combinators - need closures with captures
- Cannot define Iterator behavior - need associated types
- Cannot provide default implementations - need behavior defaults
- Cannot write any unsafe/FFI code - need lowlevel blocks

This task tracks all missing core features to ensure std can be fully implemented in TML.

## What Changes

### Phase 1 - Critical (Blocks std implementation)

1. **lowlevel blocks**: Enable `lowlevel { ... }` syntax for unsafe/FFI operations
   - Required for: Memory operations, C interop, pointer manipulation
   - Affects: Parser, Type Checker, Codegen

2. **Pointer methods**: Implement `.read()`, `.write(value)`, `.offset(n)` on `Ptr[T]`
   - Required for: Collections, buffer operations, memory management
   - Affects: Type System, Codegen

3. **Closures with captures**: Fix closure capture of outer variables
   - Required for: Option.map(), Result.and_then(), iterator combinators
   - Affects: Parser, Type Checker, Codegen (capture analysis, environment struct)

### Phase 2 - High Priority (Required for idiomatic std)

4. **Associated types in behaviors**: Enable `type Item` in behavior definitions
   - Required for: Iterator behavior, collection behaviors
   - Affects: Parser, Type System

5. **Default implementations in behaviors**: Allow method bodies in behaviors
   - Required for: Iterator combinators (map, filter, fold from next)
   - Affects: Parser, Type Checker, Codegen

6. **Iterator behavior**: Define and implement Iterator protocol
   - Required for: for-in loops on custom types, collection iteration
   - Depends on: Associated types, default implementations

## Impact

- **Affected specs**: 04-TYPES.md, 05-SEMANTICS.md, 06-MEMORY.md, 13-BUILTINS.md
- **Affected code**: Parser, Type Checker, LLVM IR Generator
- **Breaking change**: NO (additive features only)
- **User benefit**: Enables full std library implementation in TML

## Dependencies

- None (core compiler changes)

## Risks

- Capture analysis complexity for closures
- LLVM IR complexity for closure environments
- Associated type resolution in generic contexts
