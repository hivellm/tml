# Tasks: Add Bitflags Support

**Status**: In Progress (95%)

## Phase 1: Parser — Explicit Discriminant Values

- [x] 1.1.1 Add `discriminant` field (`std::optional<ExprPtr>`) to `EnumVariant` in `ast_decls.hpp`
- [x] 1.1.2 Parse `= <integer_expr>` after variant name in `parse_sum_type_variant()`
- [x] 1.1.3 Validate discriminant is a compile-time constant integer expression
- [x] 1.1.4 Fix brace-style enum detection to include `=` token in look-ahead
- [x] 1.1.5 Verify existing enum parsing is unaffected (no regression)

## Phase 2: Parser — @flags Decorator Recognition

- [x] 2.1.1 Register `@flags` as a built-in decorator in the decorator system
- [x] 2.1.2 Parse optional type parameter `@flags(U8)`, `@flags(U16)`, `@flags(U32)`, `@flags(U64)`
- [x] 2.1.3 Default to `U32` when no type parameter specified
- [x] 2.1.4 Store flags metadata (underlying type, is_flags) on `EnumDef` in `env.hpp`

## Phase 3: Type Checker — @flags Validation

- [x] 3.1.1 Validate `@flags` enum variants have no tuple or struct payloads (unit variants only)
- [x] 3.1.2 Auto-assign power-of-2 discriminants (1, 2, 4, 8, ...) when not explicit
- [x] 3.1.3 Validate explicit discriminant values are non-negative integers
- [x] 3.1.4 Validate variant count does not exceed bit width of underlying type
- [x] 3.1.6 Allow bitwise operators (`|`, `&`, `^`, `~`) on `@flags` enum types
- [x] 3.1.8 Register method signatures (.has, .add, .remove, .toggle, .is_empty, .bits, ::from_bits, ::none, ::all)

## Phase 4: Codegen — Discriminant Values & Representation

- [x] 4.1.1 Modify `gen_enum_decl` to detect `@flags` and use power-of-2 discriminants
- [x] 4.1.2 Represent `@flags` enums as `%struct.Name = type { iN }` with correct underlying type
- [x] 4.1.3 Register variant values in `enum_variants_` with correct power-of-2 values
- [x] 4.1.4 Register `FlagsEnumInfo` in `flags_enums_` map for downstream codegen
- [x] 4.1.5 Handle lazy registration of imported `@flags` enums in `gen_path()`

## Phase 5: Codegen — Bitwise Operator Auto-Derivation

- [x] 5.1.1 Auto-derive `BitAnd` for `@flags` enums (result type = self)
- [x] 5.1.2 Auto-derive `BitOr` for `@flags` enums (result type = self)
- [x] 5.1.3 Auto-derive `BitXor` for `@flags` enums (result type = self)
- [x] 5.1.4 Auto-derive `BitNot` for `@flags` enums (complement masked to valid bits)
- [x] 5.1.6 Generate `PartialEq` and `Eq` for `@flags` enums (integer comparison)
- [x] 5.1.7 Fix general BitNot bug (was hardcoded to i32, now uses operand_type)

## Phase 6: Codegen — Built-in Methods

- [x] 6.1.1 Generate `.has(flag) -> Bool` method (bitwise AND + icmp eq)
- [x] 6.1.2 Generate `.add(flag) -> Self` method (bitwise OR)
- [x] 6.1.3 Generate `.remove(flag) -> Self` method (AND NOT)
- [x] 6.1.4 Generate `.toggle(flag) -> Self` method (bitwise XOR)
- [x] 6.1.5 Generate `.is_empty() -> Bool` method (icmp eq 0)
- [x] 6.1.6 Generate `.bits() -> UnderlyingType` method (load raw value)
- [x] 6.1.7 Generate `::from_bits(value) -> Self` static method
- [x] 6.1.8 Generate `::none() -> Self` and `::all() -> Self` static methods
- [x] 6.1.9 Generate `.eq(other) -> Bool` method for equality comparison
- [x] 6.1.10 Add instance method dispatch in `method.cpp`
- [x] 6.1.11 Add static method dispatch in `method_static_dispatch.cpp`

## Phase 7: Core Library — Flags Behavior

- [x] 7.1.1 Define `Flags` behavior in `lib/core/src/ops/flags.tml`
- [x] 7.1.3 Define `has`, `add`, `remove`, `toggle`, `is_empty`, `bits`, `from_bits` in behavior
- [x] 7.1.2 Register in ops/mod.tml
- [x] 7.1.4 Auto-implement `Flags` behavior for `@flags` enums
- [x] 7.1.5 Add Display/Debug formatting for flags (e.g., `"Stunned | Burning"`)

## Phase 8: Integration Tests

- [x] 8.1.1 Test basic `@flags` enum creation and variant access (flags_basic.test.tml)
- [x] 8.1.2 Test combining flags with `|` operator
- [x] 8.1.3 Test checking flags with `.has()` and `&` operator
- [x] 8.1.4 Test removing flags with `.remove()`
- [x] 8.1.5 Test toggling flags with `.toggle()` and `^` operator
- [x] 8.1.6 Test `.is_empty()`, `.bits()`, `::from_bits()`, `::none()`, `::all()` (flags_methods.test.tml)
- [x] 8.1.7 Test explicit discriminant values and composite flags (flags_explicit.test.tml)
- [x] 8.1.8 Test equality, inequality, complement (flags_equality.test.tml)
- [x] 8.1.9 Test `@flags(U8)`, `@flags(U64)` underlying types (flags_underlying.test.tml)
- [x] 8.1.10 Test error cases: payload variants T082, overflow T083 (flags_errors.error.tml, flags_overflow.error.tml)
- [x] 8.1.11 Test real-world scenario: MMORPG entity states + file permissions (flags_realworld.test.tml)

## Phase 9: Documentation & Specification

- [x] 9.1.1 Add `@flags` section to `docs/specs/04-TYPES.md` (section 3.2.1)
- [x] 9.1.2 Add explicit discriminant syntax to `docs/specs/03-GRAMMAR.md` (Variant rule)
- [x] 9.1.3 Document `@flags` in `docs/specs/25-DECORATORS.md` (built-in decorators table)

## Phase 10: Serialization & Pattern Matching

- [ ] 10.1.1 Support `when` matching on individual flag variants
- [ ] 10.1.2 Support serialization of `@flags` enums to JSON
- [ ] 10.1.3 Support `Display` formatting as pipe-separated names
