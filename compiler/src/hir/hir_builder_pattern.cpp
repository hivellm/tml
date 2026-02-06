//! # HIR Builder - Pattern Lowering
//!
//! This file implements pattern lowering from AST to HIR.
//!
//! ## Overview
//!
//! Pattern lowering transforms parser AST patterns into HIR patterns. Patterns
//! are used in `let` bindings, `when` expressions, function parameters, and
//! destructuring assignments.
//!
//! ## Pattern Types
//!
//! | AST Pattern       | HIR Pattern          | Example                    |
//! |-------------------|----------------------|----------------------------|
//! | `WildcardPattern` | `HirWildcardPattern` | `_`                        |
//! | `IdentPattern`    | `HirBindingPattern`  | `x`, `mut y`               |
//! | `LiteralPattern`  | `HirLiteralPattern`  | `42`, `"hello"`, `true`    |
//! | `TuplePattern`    | `HirTuplePattern`    | `(a, b, c)`                |
//! | `StructPattern`   | `HirStructPattern`   | `Point { x, y }`           |
//! | `EnumPattern`     | `HirEnumPattern`     | `Maybe::Just(v)`           |
//! | `OrPattern`       | `HirOrPattern`       | `1 \| 2 \| 3`               |
//! | `ArrayPattern`    | `HirArrayPattern`    | `[a, b, ..rest]`           |
//!
//! ## Type Propagation
//!
//! Patterns receive an "expected type" from context which flows downward:
//! - In `let x: T = ...`, the pattern gets type T
//! - In `when` expressions, patterns get the scrutinee type
//! - Nested patterns receive element/field types from their parent
//!
//! ## Binding Extraction
//!
//! Patterns introduce variable bindings. The `HirBindingPattern` records:
//! - Variable name
//! - Mutability (`mut` keyword)
//! - Inferred or annotated type
//!
//! ## See Also
//!
//! - `hir_builder_stmt.cpp` - Uses patterns in let statements
//! - `hir_builder_expr.cpp` - Uses patterns in when/for expressions
//! - `hir_pattern.cpp` - Pattern node implementations

#include "hir/hir_builder.hpp"
#include "lexer/token.hpp"

namespace tml::hir {

// ============================================================================
// Pattern Lowering Dispatch
// ============================================================================
//
// Main entry point for pattern lowering. The expected_type parameter carries
// type information from the surrounding context (e.g., from type annotations
// or scrutinee types). This enables type inference to flow from outer context
// into nested pattern elements.

auto HirBuilder::lower_pattern(const parser::Pattern& pattern, HirType expected_type)
    -> HirPatternPtr {
    return std::visit(
        [&](const auto& p) -> HirPatternPtr {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, parser::WildcardPattern>) {
                return lower_wildcard_pattern(p);
            } else if constexpr (std::is_same_v<T, parser::IdentPattern>) {
                return lower_ident_pattern(p, expected_type);
            } else if constexpr (std::is_same_v<T, parser::LiteralPattern>) {
                return lower_literal_pattern(p, expected_type);
            } else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
                return lower_tuple_pattern(p, expected_type);
            } else if constexpr (std::is_same_v<T, parser::StructPattern>) {
                return lower_struct_pattern(p, expected_type);
            } else if constexpr (std::is_same_v<T, parser::EnumPattern>) {
                return lower_enum_pattern(p, expected_type);
            } else if constexpr (std::is_same_v<T, parser::OrPattern>) {
                return lower_or_pattern(p, expected_type);
            } else if constexpr (std::is_same_v<T, parser::ArrayPattern>) {
                return lower_array_pattern(p, expected_type);
            } else {
                // Fallback to wildcard
                return make_hir_wildcard_pattern(fresh_id(), pattern.span);
            }
        },
        pattern.kind);
}

// ============================================================================
// Wildcard Pattern
// ============================================================================
//
// The `_` pattern matches anything but binds no variables.
// Commonly used for ignored elements or catch-all cases in `when`.

auto HirBuilder::lower_wildcard_pattern(const parser::WildcardPattern& pattern) -> HirPatternPtr {
    return make_hir_wildcard_pattern(fresh_id(), pattern.span);
}

// ============================================================================
// Identifier Pattern
// ============================================================================
//
// Identifier patterns bind values to names. Supports:
// - Simple binding: `x` (immutable)
// - Mutable binding: `mut x`
// - Typed binding: `x: I32` (explicit annotation overrides expected type)

auto HirBuilder::lower_ident_pattern(const parser::IdentPattern& pattern, HirType expected_type)
    -> HirPatternPtr {
    HirType type = expected_type;

    // If pattern has type annotation, use that
    if (pattern.type_annotation) {
        type = resolve_type(**pattern.type_annotation);
    }

    return make_hir_binding_pattern(fresh_id(), pattern.name, pattern.is_mut, type, pattern.span);
}

// ============================================================================
// Literal Pattern
// ============================================================================
//
// Literal patterns match exact values: integers, booleans, strings, chars.
// Used in `when` arms for value-based dispatch:
//   when x {
//       0 => "zero",
//       1 => "one",
//       _ => "other"
//   }

auto HirBuilder::lower_literal_pattern(const parser::LiteralPattern& pattern, HirType expected_type)
    -> HirPatternPtr {
    HirId id = fresh_id();

    switch (pattern.literal.kind) {
    case lexer::TokenKind::IntLiteral: {
        if (std::holds_alternative<lexer::IntValue>(pattern.literal.value)) {
            auto int_val = std::get<lexer::IntValue>(pattern.literal.value);
            return make_hir_literal_pattern(id, static_cast<int64_t>(int_val.value),
                                            expected_type ? expected_type : types::make_i64(),
                                            pattern.span);
        }
        break;
    }
    case lexer::TokenKind::BoolLiteral: {
        if (std::holds_alternative<bool>(pattern.literal.value)) {
            return make_hir_literal_pattern(id, std::get<bool>(pattern.literal.value),
                                            types::make_bool(), pattern.span);
        }
        break;
    }
    case lexer::TokenKind::StringLiteral: {
        if (std::holds_alternative<lexer::StringValue>(pattern.literal.value)) {
            auto str_val = std::get<lexer::StringValue>(pattern.literal.value);
            return make_hir_literal_pattern(id, str_val.value, types::make_str(), pattern.span);
        }
        break;
    }
    case lexer::TokenKind::CharLiteral: {
        if (std::holds_alternative<lexer::CharValue>(pattern.literal.value)) {
            auto char_val = std::get<lexer::CharValue>(pattern.literal.value);
            // Store as int64_t since we don't have char in the variant
            return make_hir_literal_pattern(id, static_cast<int64_t>(char_val.value),
                                            types::make_primitive(types::PrimitiveKind::Char),
                                            pattern.span);
        }
        break;
    }
    default:
        break;
    }

    // Fallback
    return make_hir_literal_pattern(id, int64_t(0), types::make_i64(), pattern.span);
}

// ============================================================================
// Tuple Pattern
// ============================================================================
//
// Tuple patterns destructure tuple values: `(a, b, c)`.
// Element types are extracted from the expected TupleType and propagated
// to nested patterns. Missing types default to null (inferred later).

auto HirBuilder::lower_tuple_pattern(const parser::TuplePattern& pattern, HirType expected_type)
    -> HirPatternPtr {
    std::vector<HirPatternPtr> elements;

    // Extract element types from expected type
    std::vector<HirType> element_types;
    if (expected_type && expected_type->is<types::TupleType>()) {
        element_types = expected_type->as<types::TupleType>().elements;
    }

    for (size_t i = 0; i < pattern.elements.size(); ++i) {
        HirType elem_type = (i < element_types.size()) ? element_types[i] : nullptr;
        elements.push_back(lower_pattern(*pattern.elements[i], elem_type));
    }

    return make_hir_tuple_pattern(fresh_id(), std::move(elements), expected_type, pattern.span);
}

// ============================================================================
// Struct Pattern
// ============================================================================
//
// Struct patterns destructure named fields: `Point { x, y }`.
// Field types are looked up from the struct definition in the type environment.
// The `has_rest` flag (`..`) allows ignoring unmatched fields.

auto HirBuilder::lower_struct_pattern(const parser::StructPattern& pattern, HirType expected_type)
    -> HirPatternPtr {
    std::string struct_name;
    if (!pattern.path.segments.empty()) {
        struct_name = pattern.path.segments.back();
    }

    std::vector<std::pair<std::string, HirPatternPtr>> fields;
    for (const auto& [field_name, field_pattern] : pattern.fields) {
        // Get field type from struct definition
        HirType field_type = nullptr;
        if (!struct_name.empty()) {
            if (auto struct_def = type_env_.lookup_struct(struct_name)) {
                for (const auto& f : struct_def->fields) {
                    if (f.name == field_name) {
                        field_type = type_env_.resolve(f.type);
                        break;
                    }
                }
            }
        }
        fields.emplace_back(field_name, lower_pattern(*field_pattern, field_type));
    }

    return make_hir_struct_pattern(fresh_id(), struct_name, std::move(fields), pattern.has_rest,
                                   expected_type, pattern.span);
}

// ============================================================================
// Enum Pattern
// ============================================================================
//
// Enum patterns match enum variants: `Maybe::Just(v)`, `Outcome::Err(e)`.
// The variant index is resolved from the type environment's enum definition.
// Payload patterns receive types from the variant's associated data types.

auto HirBuilder::lower_enum_pattern(const parser::EnumPattern& pattern, HirType expected_type)
    -> HirPatternPtr {
    std::string enum_name;
    std::string variant_name;

    if (pattern.path.segments.size() >= 2) {
        enum_name = pattern.path.segments[pattern.path.segments.size() - 2];
        variant_name = pattern.path.segments.back();
    } else if (!pattern.path.segments.empty()) {
        variant_name = pattern.path.segments.back();
        // Try to infer enum name from expected type
        if (expected_type && expected_type->is<types::NamedType>()) {
            enum_name = expected_type->as<types::NamedType>().name;
        }
    }

    int variant_index = get_variant_index(enum_name, variant_name);

    std::optional<std::vector<HirPatternPtr>> payload;
    if (pattern.payload) {
        std::vector<HirPatternPtr> payload_patterns;

        // Get payload types from enum variant definition
        // variants is std::vector<std::pair<std::string, std::vector<TypePtr>>>
        std::vector<HirType> payload_types;
        if (!enum_name.empty()) {
            if (auto enum_def = type_env_.lookup_enum(enum_name)) {
                for (const auto& v : enum_def->variants) {
                    if (v.first == variant_name) {
                        for (const auto& payload_type : v.second) {
                            payload_types.push_back(type_env_.resolve(payload_type));
                        }
                        break;
                    }
                }
            }
        }

        size_t idx = 0;
        for (const auto& p : *pattern.payload) {
            HirType payload_type = (idx < payload_types.size()) ? payload_types[idx] : nullptr;
            payload_patterns.push_back(lower_pattern(*p, payload_type));
            ++idx;
        }
        payload = std::move(payload_patterns);
    }

    return make_hir_enum_pattern(fresh_id(), enum_name, variant_name, variant_index,
                                 std::move(payload), expected_type, pattern.span);
}

// ============================================================================
// Or Pattern
// ============================================================================
//
// Or patterns match any of several alternatives: `1 | 2 | 3`.
// All alternatives must bind the same set of variables with compatible types.
// The expected type is propagated to all alternatives.

auto HirBuilder::lower_or_pattern(const parser::OrPattern& pattern, HirType expected_type)
    -> HirPatternPtr {
    std::vector<HirPatternPtr> alternatives;
    for (const auto& alt : pattern.patterns) {
        alternatives.push_back(lower_pattern(*alt, expected_type));
    }

    auto result = std::make_unique<HirPattern>();
    result->kind = HirOrPattern{fresh_id(), std::move(alternatives), expected_type, pattern.span};
    return result;
}

// ============================================================================
// Array Pattern
// ============================================================================
//
// Array patterns match arrays or slices: `[a, b, c]`, `[head, ..tail]`.
// Element type is extracted from the expected ArrayType or SliceType.
// The rest pattern (`..rest`) captures remaining elements as a slice.

auto HirBuilder::lower_array_pattern(const parser::ArrayPattern& pattern, HirType expected_type)
    -> HirPatternPtr {
    // Extract element type from expected type
    HirType element_type = nullptr;
    if (expected_type) {
        if (expected_type->is<types::ArrayType>()) {
            element_type = expected_type->as<types::ArrayType>().element;
        } else if (expected_type->is<types::SliceType>()) {
            element_type = expected_type->as<types::SliceType>().element;
        }
    }

    std::vector<HirPatternPtr> elements;
    for (const auto& elem : pattern.elements) {
        elements.push_back(lower_pattern(*elem, element_type));
    }

    std::optional<HirPatternPtr> rest;
    if (pattern.rest) {
        // Rest pattern captures a slice
        HirType rest_type = expected_type;
        if (element_type) {
            rest_type = types::make_slice(element_type);
        }
        rest = lower_pattern(**pattern.rest, rest_type);
    }

    auto result = std::make_unique<HirPattern>();
    result->kind = HirArrayPattern{fresh_id(), std::move(elements), std::move(rest), expected_type,
                                   pattern.span};
    return result;
}

} // namespace tml::hir
