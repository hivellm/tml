//! # THIR Module Implementation
//!
//! Implements ThirExpr, ThirStmt, ThirPattern, and ThirModule accessor methods.

#include "thir/thir.hpp"

namespace tml::thir {

// ============================================================================
// ThirExpr accessors
// ============================================================================

auto ThirExpr::id() const -> ThirId {
    return std::visit([](const auto& e) -> ThirId { return e.id; }, kind);
}

auto ThirExpr::type() const -> ThirType {
    return std::visit(
        [](const auto& e) -> ThirType {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, ThirReturnExpr> || std::is_same_v<T, ThirBreakExpr> ||
                          std::is_same_v<T, ThirContinueExpr> ||
                          std::is_same_v<T, ThirAssignExpr> ||
                          std::is_same_v<T, ThirCompoundAssignExpr>) {
                return nullptr; // These don't produce values
            } else {
                return e.type;
            }
        },
        kind);
}

auto ThirExpr::span() const -> SourceSpan {
    return std::visit([](const auto& e) -> SourceSpan { return e.span; }, kind);
}

// ============================================================================
// ThirStmt accessors
// ============================================================================

auto ThirStmt::id() const -> ThirId {
    return std::visit([](const auto& s) -> ThirId { return s.id; }, kind);
}

auto ThirStmt::span() const -> SourceSpan {
    return std::visit([](const auto& s) -> SourceSpan { return s.span; }, kind);
}

// ============================================================================
// ThirPattern accessors
// ============================================================================

auto ThirPattern::id() const -> ThirId {
    return std::visit([](const auto& p) -> ThirId { return p.id; }, kind);
}

auto ThirPattern::type() const -> ThirType {
    return std::visit(
        [](const auto& p) -> ThirType {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, ThirWildcardPattern>) {
                return nullptr;
            } else {
                return p.type;
            }
        },
        kind);
}

auto ThirPattern::span() const -> SourceSpan {
    return std::visit([](const auto& p) -> SourceSpan { return p.span; }, kind);
}

// ============================================================================
// ThirModule lookup methods
// ============================================================================

auto ThirModule::find_struct(const std::string& search_name) const -> const ThirStruct* {
    for (const auto& s : structs) {
        if (s.name == search_name || s.mangled_name == search_name)
            return &s;
    }
    return nullptr;
}

auto ThirModule::find_enum(const std::string& search_name) const -> const ThirEnum* {
    for (const auto& e : enums) {
        if (e.name == search_name || e.mangled_name == search_name)
            return &e;
    }
    return nullptr;
}

auto ThirModule::find_function(const std::string& search_name) const -> const ThirFunction* {
    for (const auto& f : functions) {
        if (f.name == search_name || f.mangled_name == search_name)
            return &f;
    }
    return nullptr;
}

auto ThirModule::find_const(const std::string& search_name) const -> const ThirConst* {
    for (const auto& c : constants) {
        if (c.name == search_name)
            return &c;
    }
    return nullptr;
}

} // namespace tml::thir
