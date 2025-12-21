# Proposal: Bootstrap Stdlib Core

## Why

The standard library core provides the foundational types and traits that TML programs depend on. This includes primitive type operations, core traits (Copy, Clone, Drop, Eq, Ord, Hash, Display, Debug), and essential data structures (Option, Result, Vec, String, HashMap). Without these, TML programs cannot perform basic operations. The stdlib is written in TML itself, making it the first real TML code compiled by the bootstrap compiler.

## What Changes

### New Components

1. **Primitive Operations** (`stdlib/core/primitives.tml`)
   - Integer operations (I8-I64, U8-U64)
   - Float operations (F32, F64)
   - Bool operations
   - Char operations

2. **Core Traits** (`stdlib/core/traits.tml`)
   - Copy, Clone, Drop
   - Eq, PartialEq
   - Ord, PartialOrd
   - Hash
   - Display, Debug
   - Default
   - From, Into

3. **Option Type** (`stdlib/core/option.tml`)
   - Option[T] enum
   - Methods: is_some, is_none, unwrap, unwrap_or, map, and_then

4. **Result Type** (`stdlib/core/result.tml`)
   - Result[T, E] enum
   - Methods: is_ok, is_err, unwrap, unwrap_err, map, map_err, and_then

5. **Vec Type** (`stdlib/collections/vec.tml`)
   - Dynamic array implementation
   - Methods: new, push, pop, len, get, iter

6. **String Type** (`stdlib/string/string.tml`)
   - UTF-8 string implementation
   - Methods: new, len, push, chars, bytes

7. **HashMap Type** (`stdlib/collections/hashmap.tml`)
   - Hash map implementation
   - Methods: new, insert, get, remove, contains_key

### Design Principles

Based on `docs/packages/`:
- Safe by default, unsafe operations gated
- Zero-cost abstractions where possible
- Consistent API design across types
- Full trait implementations for all types

## Impact

- **Affected specs**: All package specs in docs/packages/
- **Affected code**: New `stdlib/` directory (TML source)
- **Breaking change**: NO (new component)
- **User benefit**: Usable standard library for TML programs
- **Dependencies**: Requires bootstrap-llvm-backend to compile TML code

## Success Criteria

1. All primitive types have complete operations
2. Core traits are implemented for appropriate types
3. Option and Result work correctly
4. Vec, String, HashMap work correctly
5. All methods have proper documentation
6. Test coverage ≥95%
