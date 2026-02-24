# Proposal: Add Bitflags Support (C#-style [Flags] Enums)

## Status: PROPOSED

## Why

TML currently has full bitwise operator support (`&`, `|`, `^`, `~`, `<<`, `>>`) and all unsigned integer types (U8-U128), but lacks a first-class mechanism for defining type-safe bitflag sets — a pattern used extensively in game engines, network protocols, permission systems, and OS APIs. Today, users must manually define loose `const` values and a wrapper struct with `add`/`remove`/`has` methods, which is verbose, error-prone (wrong constant type, accidental overlap), and provides no compile-time validation that flag values are powers of two.

Languages like C# (`[Flags]`), Rust (`bitflags!` macro), and C (`enum` + bitmask conventions) all provide ergonomic bitflag support. TML, designed for LLM comprehension and game/systems programming, needs this as a first-class feature to remain competitive for its target use cases (MMORPG servers, ECS systems, protocol parsing, permission models).

Key gaps today:
- **No explicit enum discriminant values** — enum variants auto-increment from 0, cannot be set to powers of 2
- **No `@flags` directive** — no way to declare an enum as a bitflag set
- **No bitwise operators on enums** — `EntityStates.Stunned | EntityStates.Dead` does not compile
- **No combine/check/remove ergonomics** — requires manual struct wrapper boilerplate

## What Changes

### 1. `@flags` Decorator on Enum Types

A new built-in decorator `@flags` that transforms an enum into a bitflag set:

```tml
@flags
type EntityStates {
    Stunned,           // = 1
    Dead,              // = 2
    Ally,              // = 4
    Enemy,             // = 8
    Burning,           // = 16
    Bleeding,          // = 32
    Poisoned,          // = 64
    Stealth,           // = 128
}
```

Variants are automatically assigned powers of 2 (1, 2, 4, 8, ...). A special `None` variant with value 0 is implicitly available.

### 2. Explicit Discriminant Values (Optional Override)

Allow explicit integer values for enum variants when `@flags` is used:

```tml
@flags
type Permissions {
    Read = 1,
    Write = 2,
    Execute = 4,
    ReadWrite = 3,       // Read | Write (composite)
    All = 7,             // Read | Write | Execute
}
```

### 3. Bitwise Operators on @flags Enums

`@flags` enums automatically implement bitwise behaviors:

```tml
@flags
type EntityStates { Stunned, Dead, Ally, Burning }

let state: EntityStates = EntityStates::Stunned | EntityStates::Burning
let cleared: EntityStates = state & (~EntityStates::Burning)
let toggled: EntityStates = state ^ EntityStates::Ally
```

Operators: `|`, `&`, `^`, `~` return the same flags type. Shift operators are not applicable.

### 4. Built-in Methods on @flags Enums

```tml
state.has(EntityStates::Stunned)       // -> Bool (bitwise AND check)
state.add(EntityStates::Dead)          // -> EntityStates (bitwise OR)
state.remove(EntityStates::Stunned)    // -> EntityStates (AND NOT)
state.toggle(EntityStates::Ally)       // -> EntityStates (XOR)
state.is_empty()                       // -> Bool (== 0)
state.bits()                           // -> U32 (raw value)
EntityStates::from_bits(0x05)          // -> EntityStates (from raw)
EntityStates::all()                    // -> EntityStates (all bits set)
EntityStates::none()                   // -> EntityStates (zero)
```

### 5. Underlying Representation Type

By default `@flags` uses `U32`. Can be overridden:

```tml
@flags(U8)
type SmallFlags { A, B, C, D, E, F, G, H }  // max 8 flags

@flags(U64)
type LargeFlags { /* up to 64 flags */ }
```

## Impact

- Affected specs: `04-TYPES.md` (new flags section), `03-GRAMMAR.md` (variant discriminant syntax), `02-LEXICAL.md` (no changes), `25-DECORATORS.md` (`@flags` built-in)
- Affected code:
  - `compiler/include/parser/ast_decls.hpp` — Add `discriminant` field to `EnumVariant`
  - `compiler/src/parser/` — Parse `= expr` after variant names
  - `compiler/src/types/` — Type-check `@flags` semantics, auto-derive bitwise behaviors
  - `compiler/src/codegen/llvm/decl/enum.cpp` — Use explicit discriminant values, generate bitwise operator impls
  - `compiler/src/codegen/llvm/expr/binary.cpp` — Allow bitwise ops on flags types
  - `lib/core/src/` — Flags behavior definitions
- Breaking change: NO (purely additive)
- User benefit: Type-safe bitflag patterns with zero boilerplate, essential for game dev, systems programming, and protocol implementation

## Risks

- Composite flag values (e.g., `ReadWrite = 3`) need validation that they are combinations of existing flags
- Interaction with `when` (pattern matching) on flags types needs design — matching on combined flags is non-trivial
- `@flags(U8)` with more than 8 variants must be a compile-time error
- Serialization/deserialization of flags (to/from JSON, binary) needs to work with the encoding module

## Dependencies

- None — all prerequisite infrastructure (bitwise ops, unsigned types, decorators, enums) already exists

## Success Criteria

- `@flags` enums compile and generate correct LLVM IR with power-of-2 discriminants
- Bitwise operators (`|`, `&`, `^`, `~`) work on flags types and return the flags type
- Built-in methods (`.has()`, `.add()`, `.remove()`, `.toggle()`, `.bits()`) work
- Explicit discriminant values validated at compile time
- Overflow detection (too many variants for underlying type) produces clear error
- Tests cover: creation, combination, removal, checking, iteration, serialization
- Documentation updated with practical examples (permissions, entity states, item flags)