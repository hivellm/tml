# Tasks: Add Bitflags Support

**Status**: Proposed (0%)

## Phase 1: Parser — Explicit Discriminant Values

- [ ] 1.1.1 Add `discriminant` field (`std::optional<ExprPtr>`) to `EnumVariant` in `ast_decls.hpp`
- [ ] 1.1.2 Parse `= <integer_expr>` after variant name in `parse_sum_type_variant()`
- [ ] 1.1.3 Validate discriminant is a compile-time constant integer expression
- [ ] 1.1.4 Add parser tests for explicit discriminant syntax
- [ ] 1.1.5 Verify existing enum parsing is unaffected (no regression)

## Phase 2: Parser — @flags Decorator Recognition

- [ ] 2.1.1 Register `@flags` as a built-in decorator in the decorator system
- [ ] 2.1.2 Parse optional type parameter `@flags(U8)`, `@flags(U16)`, `@flags(U32)`, `@flags(U64)`
- [ ] 2.1.3 Default to `U32` when no type parameter specified
- [ ] 2.1.4 Store flags metadata (underlying type, is_flags) on `EnumDecl`
- [ ] 2.1.5 Add parser tests for `@flags` decorator with and without type parameter

## Phase 3: Type Checker — @flags Validation

- [ ] 3.1.1 Validate `@flags` enum variants have no tuple or struct payloads (unit variants only)
- [ ] 3.1.2 Auto-assign power-of-2 discriminants (1, 2, 4, 8, ...) when not explicit
- [ ] 3.1.3 Validate explicit discriminant values are non-negative integers
- [ ] 3.1.4 Validate variant count does not exceed bit width of underlying type
- [ ] 3.1.5 Validate composite discriminant values are combinations of existing flags
- [ ] 3.1.6 Allow bitwise operators (`|`, `&`, `^`, `~`) on `@flags` enum types
- [ ] 3.1.7 Reject shift operators (`<<`, `>>`) on `@flags` enum types
- [ ] 3.1.8 Add type checker error messages for all validation failures
- [ ] 3.1.9 Add type checker tests for valid and invalid `@flags` enums

## Phase 4: Codegen — Discriminant Values & Representation

- [ ] 4.1.1 Modify `gen_enum_decl` to use explicit discriminant values instead of auto-increment
- [ ] 4.1.2 Represent `@flags` enums as the underlying unsigned type (not struct with i32 tag)
- [ ] 4.1.3 Generate variant constants as global named constants in LLVM IR
- [ ] 4.1.4 Generate `EnumName::none()` as zero constant
- [ ] 4.1.5 Generate `EnumName::all()` as OR of all variant values
- [ ] 4.1.6 Add codegen tests verifying correct LLVM IR output

## Phase 5: Codegen — Bitwise Operator Auto-Derivation

- [ ] 5.1.1 Auto-derive `BitAnd` for `@flags` enums (result type = self)
- [ ] 5.1.2 Auto-derive `BitOr` for `@flags` enums (result type = self)
- [ ] 5.1.3 Auto-derive `BitXor` for `@flags` enums (result type = self)
- [ ] 5.1.4 Auto-derive `BitNot` for `@flags` enums (complement masked to valid bits)
- [ ] 5.1.5 Auto-derive `BitAndAssign`, `BitOrAssign`, `BitXorAssign` compound assignments
- [ ] 5.1.6 Generate `PartialEq` and `Eq` for `@flags` enums (integer comparison)
- [ ] 5.1.7 Add codegen tests for all bitwise operations on flags

## Phase 6: Codegen — Built-in Methods

- [ ] 6.1.1 Generate `.has(flag) -> Bool` method (bitwise AND != 0)
- [ ] 6.1.2 Generate `.add(flag) -> Self` method (bitwise OR)
- [ ] 6.1.3 Generate `.remove(flag) -> Self` method (bitwise AND NOT)
- [ ] 6.1.4 Generate `.toggle(flag) -> Self` method (bitwise XOR)
- [ ] 6.1.5 Generate `.is_empty() -> Bool` method (== 0)
- [ ] 6.1.6 Generate `.bits() -> UnderlyingType` method (raw integer value)
- [ ] 6.1.7 Generate `::from_bits(value) -> Self` static method
- [ ] 6.1.8 Generate `::none() -> Self` and `::all() -> Self` static methods
- [ ] 6.1.9 Add codegen tests for all built-in methods

## Phase 7: Core Library — Flags Behavior

- [ ] 7.1.1 Define `Flags` behavior in `lib/core/src/ops/flags.tml`
- [ ] 7.1.2 Define associated type `Bits` for underlying integer type
- [ ] 7.1.3 Define `has`, `add`, `remove`, `toggle`, `is_empty`, `bits`, `from_bits` in behavior
- [ ] 7.1.4 Auto-implement `Flags` behavior for `@flags` enums
- [ ] 7.1.5 Add Display/Debug formatting for flags (e.g., `"Stunned | Burning"`)

## Phase 8: Integration Tests

- [ ] 8.1.1 Test basic `@flags` enum creation and variant access
- [ ] 8.1.2 Test combining flags with `|` operator
- [ ] 8.1.3 Test checking flags with `.has()` and `&` operator
- [ ] 8.1.4 Test removing flags with `.remove()` and `& ~` pattern
- [ ] 8.1.5 Test toggling flags with `.toggle()` and `^` operator
- [ ] 8.1.6 Test `.is_empty()`, `.bits()`, `::from_bits()`, `::none()`, `::all()`
- [ ] 8.1.7 Test explicit discriminant values and composite flags
- [ ] 8.1.8 Test `@flags(U8)`, `@flags(U16)`, `@flags(U64)` underlying types
- [ ] 8.1.9 Test error cases: too many variants, payload variants, negative values
- [ ] 8.1.10 Test real-world scenario: MMORPG entity states (EntityStates, BuffStates, ItemStates)
- [ ] 8.1.11 Test real-world scenario: file permission flags
- [ ] 8.1.12 Test Display/Debug output format

## Phase 9: Documentation & Specification

- [ ] 9.1.1 Add `@flags` section to `docs/specs/04-TYPES.md`
- [ ] 9.1.2 Add explicit discriminant syntax to `docs/specs/03-GRAMMAR.md`
- [ ] 9.1.3 Document `@flags` in `docs/specs/25-DECORATORS.md`
- [ ] 9.1.4 Add `@flags` user guide chapter `docs/user/ch10-00-bitflags.md`
- [ ] 9.1.5 Add practical examples: game entity states, permissions, protocol flags
- [ ] 9.1.6 Update operator precedence table if needed

## Phase 10: Serialization & Pattern Matching

- [ ] 10.1.1 Support `when` matching on individual flag variants
- [ ] 10.1.2 Support serialization of `@flags` enums to JSON as integer or array of names
- [ ] 10.1.3 Support deserialization from integer or array of names
- [ ] 10.1.4 Support `Display` formatting as pipe-separated names (e.g., `"Stunned | Burning"`)
- [ ] 10.1.5 Add serialization/deserialization tests