TML_MODULE("compiler")

//! # HIR Pattern Implementation
//!
//! This file implements the HirPattern accessor methods and factory functions.
//!
//! ## Overview
//!
//! HIR patterns are used for destructuring values in:
//! - `let` bindings: `let (a, b) = tuple`
//! - `when` expressions: `when x { Just(v) => ..., Nothing => ... }`
//! - `for` loops: `for (k, v) in map`
//! - Function parameters: `func foo((x, y): (I32, I32))`
//!
//! ## Pattern Types
//!
//! | Pattern          | Use Case                        | Example             |
//! |------------------|---------------------------------|---------------------|
//! | Wildcard         | Ignore a value                  | `_`                 |
//! | Binding          | Bind to a name                  | `x`, `mut y`        |
//! | Literal          | Match exact value               | `42`, `"hello"`     |
//! | Tuple            | Destructure tuples              | `(a, b, c)`         |
//! | Struct           | Destructure structs             | `Point { x, y }`    |
//! | Enum             | Match enum variants             | `Maybe::Just(v)`    |
//! | Or               | Match alternatives              | `1 \| 2 \| 3`        |
//! | Array            | Destructure arrays              | `[a, b, ..rest]`    |
//!
//! ## Type Information
//!
//! Each pattern carries type information used for:
//! - Type checking (ensuring pattern matches scrutinee type)
//! - Code generation (knowing sizes and layouts)
//! - Binding type resolution (what type is bound to each name)
//!
//! ## See Also
//!
//! - `hir_pattern.hpp` - Pattern type definitions
//! - `hir_builder_pattern.cpp` - Pattern lowering from AST

#include "hir/hir_pattern.hpp"

namespace tml::hir {

// ============================================================================
// HirPattern Accessors
// ============================================================================
//
// Uniform accessors for pattern properties. The wildcard pattern is special:
// it has no associated type (returns nullptr) since it matches anything.

auto HirPattern::id() const -> HirId {
    return std::visit([](const auto& p) { return p.id; }, kind);
}

auto HirPattern::type() const -> HirType {
    return std::visit(
        [](const auto& p) -> HirType {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, HirWildcardPattern>) {
                return nullptr; // Wildcard has no type
            } else {
                return p.type;
            }
        },
        kind);
}

auto HirPattern::span() const -> SourceSpan {
    return std::visit([](const auto& p) { return p.span; }, kind);
}

// ============================================================================
// Pattern Factory Functions
// ============================================================================
//
// Factory functions for creating HIR pattern nodes. Each function:
// 1. Allocates a new HirPattern via make_unique
// 2. Sets the appropriate variant in the `kind` field
// 3. Returns ownership via unique_ptr
//
// Literal pattern has overloads for: int64_t, bool, string

auto make_hir_wildcard_pattern(HirId id, SourceSpan span) -> HirPatternPtr {
    auto pattern = std::make_unique<HirPattern>();
    pattern->kind = HirWildcardPattern{id, span};
    return pattern;
}

auto make_hir_binding_pattern(HirId id, const std::string& name, bool is_mut, HirType type,
                              SourceSpan span) -> HirPatternPtr {
    auto pattern = std::make_unique<HirPattern>();
    pattern->kind = HirBindingPattern{id, name, is_mut, std::move(type), span};
    return pattern;
}

auto make_hir_literal_pattern(HirId id, int64_t value, HirType type, SourceSpan span)
    -> HirPatternPtr {
    auto pattern = std::make_unique<HirPattern>();
    pattern->kind = HirLiteralPattern{id, value, std::move(type), span};
    return pattern;
}

auto make_hir_literal_pattern(HirId id, bool value, HirType type, SourceSpan span)
    -> HirPatternPtr {
    auto pattern = std::make_unique<HirPattern>();
    pattern->kind = HirLiteralPattern{id, value, std::move(type), span};
    return pattern;
}

auto make_hir_literal_pattern(HirId id, const std::string& value, HirType type, SourceSpan span)
    -> HirPatternPtr {
    auto pattern = std::make_unique<HirPattern>();
    pattern->kind = HirLiteralPattern{id, value, std::move(type), span};
    return pattern;
}

auto make_hir_tuple_pattern(HirId id, std::vector<HirPatternPtr> elements, HirType type,
                            SourceSpan span) -> HirPatternPtr {
    auto pattern = std::make_unique<HirPattern>();
    pattern->kind = HirTuplePattern{id, std::move(elements), std::move(type), span};
    return pattern;
}

auto make_hir_struct_pattern(HirId id, const std::string& struct_name,
                             std::vector<std::pair<std::string, HirPatternPtr>> fields,
                             bool has_rest, HirType type, SourceSpan span) -> HirPatternPtr {
    auto pattern = std::make_unique<HirPattern>();
    pattern->kind =
        HirStructPattern{id, struct_name, std::move(fields), has_rest, std::move(type), span};
    return pattern;
}

auto make_hir_enum_pattern(HirId id, const std::string& enum_name, const std::string& variant_name,
                           int variant_index, std::optional<std::vector<HirPatternPtr>> payload,
                           HirType type, SourceSpan span) -> HirPatternPtr {
    auto pattern = std::make_unique<HirPattern>();
    pattern->kind = HirEnumPattern{
        id, enum_name, variant_name, variant_index, std::move(payload), std::move(type), span};
    return pattern;
}

} // namespace tml::hir
