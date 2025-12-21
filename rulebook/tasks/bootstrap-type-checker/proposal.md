# Proposal: Bootstrap Type Checker

## Why

The type checker is responsible for verifying that TML programs are type-safe. It performs type inference, generic instantiation, trait resolution, and ensures all operations are valid for their operand types. This is critical for catching errors at compile-time rather than runtime. The type checker also produces a Typed AST (TAST) which is required by subsequent compiler stages including the borrow checker and IR generator.

## What Changes

### New Components

1. **Type System Core** (`src/types/types.hpp`)
   - Type representation (primitives, structs, enums, functions, generics)
   - Type constraints (trait bounds, where clauses)
   - Type environment and scopes

2. **Type Inference Engine** (`src/types/infer.hpp`, `src/types/infer.cpp`)
   - Hindley-Milner style type inference
   - Unification algorithm
   - Constraint solving
   - Type variable management

3. **Type Checker** (`src/types/checker.hpp`, `src/types/checker.cpp`)
   - AST traversal and type annotation
   - Expression type checking
   - Statement type checking
   - Declaration type checking
   - Pattern type checking

4. **Trait Resolver** (`src/types/traits.hpp`, `src/types/traits.cpp`)
   - Trait implementation lookup
   - Method resolution
   - Associated type handling
   - Coherence checking

5. **Typed AST** (`src/types/tast.hpp`)
   - AST nodes annotated with resolved types
   - All type variables resolved to concrete types

### Type System Features

Based on `docs/specs/04-TYPES.md`:
- **Primitive types**: I8-I128, U8-U128, F32, F64, Bool, Char, String, Unit
- **Compound types**: Tuples, arrays, slices
- **User-defined types**: Structs, enums, type aliases
- **Generic types**: Type parameters with trait bounds
- **Function types**: With parameter and return types
- **Reference types**: Shared (&T) and mutable (&mut T)
- **Pointer types**: Raw pointers (*const T, *mut T)

## Impact

- **Affected specs**: 04-TYPES.md, 05-SEMANTICS.md, 16-COMPILER-ARCHITECTURE.md
- **Affected code**: New `src/types/` directory
- **Breaking change**: NO (new component)
- **User benefit**: Compile-time type safety guarantees
- **Dependencies**: Requires bootstrap-parser to be complete

## Success Criteria

1. All primitive types are handled correctly
2. Generic type instantiation works
3. Type inference produces correct types
4. Trait bounds are enforced
5. Method resolution is correct
6. Error messages are helpful (expected vs found)
7. Test coverage ≥95%
