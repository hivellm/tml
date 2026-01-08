//! # HIR Pattern Implementation
//!
//! This file implements the HirPattern accessor methods and factory functions.

#include "hir/hir_pattern.hpp"

namespace tml::hir {

// ============================================================================
// HirPattern Accessors
// ============================================================================

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
