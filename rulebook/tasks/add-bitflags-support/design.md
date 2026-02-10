# Design: Bitflags Support

## Architecture

The `@flags` feature builds on three existing systems: the decorator pipeline, the enum codegen, and the bitwise operator infrastructure. No new compiler passes are needed — the work is localized to parsing, type checking, and code generation for enum declarations.

### Compilation Flow

```
@flags enum → Parser (read discriminants) → TypeChecker (validate flags rules)
           → Codegen (emit as unsigned int, derive bitwise ops + methods)
```

## Key Decisions

### 1. Decorator vs Keyword

**Decision**: Use `@flags` decorator, not a new keyword.

**Rationale**: TML already has a decorator system. Adding a keyword (`flags type`) would increase the grammar surface. `@flags` is consistent with `@test`, `@extern`, `@inline`, and is immediately recognizable.

### 2. Auto-Assignment vs Explicit-Only

**Decision**: Auto-assign powers of 2 by default, allow explicit override.

**Rationale**: Most flag enums follow the 1, 2, 4, 8 pattern. Forcing users to write `= 1`, `= 2`, `= 4` for every variant is tedious. Auto-assignment eliminates boilerplate while explicit values support composite flags like `ReadWrite = 3`.

```tml
// Auto-assigned (most common case)
@flags
type States { A, B, C }  // A=1, B=2, C=4

// Explicit (for composites)
@flags
type Perms { Read = 1, Write = 2, Execute = 4, ReadWrite = 3 }
```

### 3. Underlying Type

**Decision**: Default `U32`, configurable via `@flags(U8|U16|U32|U64)`.

**Rationale**: U32 covers 32 flags which is sufficient for most cases. U8 saves memory for small flag sets (network packets). U64 handles large flag sets. U128 is excluded — 128 flags suggests a design problem.

### 4. LLVM IR Representation

**Decision**: `@flags` enums are represented as their underlying unsigned integer type, not as `{ i32 }` struct.

**Rationale**: Regular enums use `%struct.EnumName = type { i32 }` for tagged unions. Flags don't need a union — they're just an integer with bitwise semantics. Using the raw integer type enables LLVM to optimize bitwise operations directly.

```llvm
; Regular enum
%struct.Color = type { i32 }

; @flags enum — just the underlying type
; EntityStates is i32 (U32)
@EntityStates.Stunned = private unnamed_addr constant i32 1
@EntityStates.Dead = private unnamed_addr constant i32 2
@EntityStates.Ally = private unnamed_addr constant i32 4
@EntityStates.Enemy = private unnamed_addr constant i32 8
```

### 5. BitNot Semantics

**Decision**: `~flags` masks to valid bits only.

**Rationale**: If a flags type has 4 variants (bits 0-3), `~flags` should only flip bits 0-3, not all 32 bits. This prevents spurious "unknown" bits from appearing.

```
all_bits = Stunned | Dead | Ally | Enemy  // 0b1111
~state = all_bits ^ state                 // only flip known bits
```

### 6. Pattern Matching

**Decision**: Support matching individual variants in `when`, not combinations.

**Rationale**: Matching all possible combinations of N flags is 2^N cases — impractical. Instead, `when` matches single flags, and users use `if state.has(...)` for combinations.

```tml
// Supported — match individual flags
when state {
    EntityStates::Stunned => "stunned",
    EntityStates::Dead => "dead",
    _ => "other",
}

// For combinations, use if/has
if state.has(EntityStates::Stunned) and state.has(EntityStates::Burning) {
    // stunned AND burning
}
```

## Implementation Approach

### Phase 1-2: Parser Changes

Minimal AST changes:

```cpp
// ast_decls.hpp
struct EnumVariant {
    std::string name;
    std::optional<ExprPtr> discriminant;  // NEW: = <expr>
    // ... existing fields
};

struct EnumDecl {
    // ... existing fields
    bool is_flags = false;                 // NEW
    std::optional<std::string> flags_type; // NEW: "U8", "U16", "U32", "U64"
};
```

### Phase 3: Type Checker

New validation in `check_enum_decl()`:
1. If `is_flags`, verify all variants are unit variants (no payloads)
2. Auto-assign discriminants: variant[i].value = 1 << i
3. Verify explicit values fit in underlying type
4. Register flags type as supporting BitAnd/BitOr/BitXor/BitNot behaviors

### Phase 4-6: Codegen

Modify `gen_enum_decl()`:
- If `is_flags`, emit raw integer type instead of struct
- Emit named constants for each variant
- Generate bitwise operator functions
- Generate `.has()`, `.add()`, `.remove()`, `.toggle()` as simple inline functions

### Phase 7: Core Library

```tml
// lib/core/src/ops/flags.tml
pub behavior Flags {
    type Bits

    func has(this, flag: Self) -> Bool
    func add(this, flag: Self) -> Self
    func remove(this, flag: Self) -> Self
    func toggle(this, flag: Self) -> Self
    func is_empty(this) -> Bool
    func bits(this) -> This::Bits
    func from_bits(value: This::Bits) -> Self
    func none() -> Self
    func all() -> Self
}
```

## Performance Considerations

- Zero overhead: `@flags` enums compile to the same LLVM IR as manual `U32` bitwise operations
- No boxing, no indirection, no runtime dispatch
- `has()`, `add()`, `remove()`, `toggle()` are all single-instruction operations at the LLVM level
- Inline: all methods should be marked `@inline` for zero-cost abstraction

## Alternatives Considered

### 1. Macro-based (`flags!` macro like Rust's `bitflags!`)

Rejected: TML doesn't have procedural macros yet. A decorator is simpler and more consistent with TML's design philosophy of readability over metaprogramming.

### 2. Standalone `Flags[T]` wrapper type

Rejected: Requires wrapping every enum, adds `.inner` access overhead in user code, and doesn't provide the ergonomic `Type::Variant | Type::Variant` syntax.

### 3. Allow bitwise ops on ALL enums

Rejected: Most enums are not flags. Allowing `Color::Red | Color::Blue` on a non-flags enum is a logical error. `@flags` makes the intent explicit.