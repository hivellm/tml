TML_MODULE("compiler")

//! # Pattern Exhaustiveness Checker Implementation
//!
//! Implements the Maranget usefulness algorithm for pattern exhaustiveness.

#include "thir/exhaustiveness.hpp"

#include "types/env.hpp"

namespace tml::thir {

// ============================================================================
// Pattern Deep-Clone
// ============================================================================

ThirPatternPtr clone_pattern(const ThirPattern& pattern) {
    auto result = std::make_unique<ThirPattern>();
    std::visit(
        [&](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, ThirWildcardPattern>) {
                result->kind = p;
            } else if constexpr (std::is_same_v<T, ThirBindingPattern>) {
                result->kind = ThirBindingPattern{p.id, p.name, p.is_mut, p.type, p.span};
            } else if constexpr (std::is_same_v<T, ThirLiteralPattern>) {
                result->kind = ThirLiteralPattern{p.id, p.value, p.type, p.span};
            } else if constexpr (std::is_same_v<T, ThirTuplePattern>) {
                ThirTuplePattern cloned{p.id, {}, p.type, p.span};
                for (const auto& e : p.elements) {
                    cloned.elements.push_back(clone_pattern(e));
                }
                result->kind = std::move(cloned);
            } else if constexpr (std::is_same_v<T, ThirStructPattern>) {
                ThirStructPattern cloned{p.id, p.struct_name, {}, p.has_rest, p.type, p.span};
                for (const auto& [name, pat] : p.fields) {
                    cloned.fields.push_back({name, clone_pattern(pat)});
                }
                result->kind = std::move(cloned);
            } else if constexpr (std::is_same_v<T, ThirEnumPattern>) {
                ThirEnumPattern cloned{p.id,         p.enum_name, p.variant_name, p.variant_index,
                                       std::nullopt, p.type,      p.span};
                if (p.payload) {
                    cloned.payload = std::vector<ThirPatternPtr>{};
                    for (const auto& sub : *p.payload) {
                        cloned.payload->push_back(clone_pattern(sub));
                    }
                }
                result->kind = std::move(cloned);
            } else if constexpr (std::is_same_v<T, ThirOrPattern>) {
                ThirOrPattern cloned{p.id, {}, p.type, p.span};
                for (const auto& alt : p.alternatives) {
                    cloned.alternatives.push_back(clone_pattern(alt));
                }
                result->kind = std::move(cloned);
            } else if constexpr (std::is_same_v<T, ThirRangePattern>) {
                result->kind = ThirRangePattern{p.id, p.start, p.end, p.inclusive, p.type, p.span};
            } else if constexpr (std::is_same_v<T, ThirArrayPattern>) {
                ThirArrayPattern cloned{p.id, {}, std::nullopt, p.type, p.span};
                for (const auto& e : p.elements) {
                    cloned.elements.push_back(clone_pattern(e));
                }
                if (p.rest) {
                    cloned.rest = clone_pattern(*p.rest);
                }
                result->kind = std::move(cloned);
            }
        },
        pattern.kind);
    return result;
}

// ============================================================================
// Constructor
// ============================================================================

ExhaustivenessChecker::ExhaustivenessChecker(const types::TypeEnv& env) : env_(&env) {}

// ============================================================================
// Public API
// ============================================================================

auto ExhaustivenessChecker::check_when(const ThirWhenExpr& when, ThirType scrutinee_type)
    -> std::vector<std::string> {
    // Build pattern matrix from when arms
    PatternMatrix matrix;
    for (const auto& arm : when.arms) {
        PatternRow row;
        // Clone the pattern into the row
        // Since we only read patterns, we store a non-owning reference via a
        // shallow copy. For the algorithm we need ThirPatternPtr, so we create
        // a new copy.
        row.columns.push_back(clone_pattern(*arm.pattern));
        matrix.rows.push_back(std::move(row));
    }

    // Build the "wildcard" row — represents "any value not yet matched"
    PatternRow wildcard_row;
    wildcard_row.columns.push_back(make_wildcard());

    std::vector<ThirType> types = {scrutinee_type};

    // Check if the wildcard row is useful (i.e., there exist unmatched values)
    if (!is_useful(matrix, wildcard_row, types)) {
        return {}; // Exhaustive — no missing patterns
    }

    // Find specific missing constructors for better error messages
    std::vector<std::string> missing;

    auto ctors = type_constructors(scrutinee_type);
    if (ctors.empty()) {
        // Infinite type (integers, strings, etc.) — just report wildcard missing
        missing.push_back("_");
        return missing;
    }

    // Check which constructors are missing
    for (const auto& ctor : ctors) {
        PatternRow ctor_row;
        // Create a pattern that matches this constructor with wildcard sub-patterns
        auto sub_types = constructor_sub_types(ctor, scrutinee_type);

        PatternRow test_row;
        // Build a row with this constructor and wildcard sub-patterns
        // For simplicity, we test if a wildcard specialized for this ctor is useful
        auto specialized = specialize_matrix(matrix, ctor, types);
        PatternRow wild_sub;
        for (size_t i = 0; i < sub_types.size(); ++i) {
            wild_sub.columns.push_back(make_wildcard());
        }
        // Remaining columns after specialization (just the sub-patterns here)
        if (is_useful(specialized, wild_sub, sub_types)) {
            missing.push_back(format_constructor(ctor, scrutinee_type));
        }
    }

    if (missing.empty()) {
        // Shouldn't happen if is_useful returned true, but be safe
        missing.push_back("_");
    }

    return missing;
}

// ============================================================================
// Usefulness Algorithm
// ============================================================================

auto ExhaustivenessChecker::is_useful(const PatternMatrix& matrix, const PatternRow& row,
                                      const std::vector<ThirType>& types) -> bool {
    // Base cases
    if (row.columns.empty()) {
        // No more columns to check — useful iff matrix has no rows
        return matrix.empty();
    }

    if (matrix.empty()) {
        // No existing patterns — the row is trivially useful
        return true;
    }

    // Get the first column's type
    ThirType head_type = types.empty() ? nullptr : types[0];

    // Get the constructor from the first pattern in the row
    const auto& head_pattern = row.columns[0];
    auto ctor = pattern_constructor(*head_pattern);

    if (ctor.kind == Constructor::Kind::Wildcard) {
        // The row starts with a wildcard — check the default matrix
        auto ctors = type_constructors(head_type);

        if (ctors.empty()) {
            // Infinite type — use default matrix
            auto def = default_matrix(matrix, types);
            PatternRow rest;
            for (size_t i = 1; i < row.columns.size(); ++i) {
                rest.columns.push_back(clone_pattern(*row.columns[i]));
            }
            std::vector<ThirType> rest_types(types.begin() + 1, types.end());
            return is_useful(def, rest, rest_types);
        }

        // Finite type — check if all constructors are covered
        // Collect constructors used in first column of matrix
        std::vector<Constructor> used_ctors;
        for (const auto& mrow : matrix.rows) {
            if (!mrow.columns.empty()) {
                auto mc = pattern_constructor(*mrow.columns[0]);
                if (mc.kind != Constructor::Kind::Wildcard) {
                    used_ctors.push_back(mc);
                }
            }
        }

        // Check if all constructors of the type are covered
        bool all_covered = true;
        for (const auto& tc : ctors) {
            bool found = false;
            for (const auto& uc : used_ctors) {
                if (uc.name == tc.name) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                all_covered = false;
                break;
            }
        }

        if (all_covered) {
            // All constructors covered — split on each and check
            for (const auto& tc : ctors) {
                auto spec_matrix = specialize_matrix(matrix, tc, types);
                auto sub_types = constructor_sub_types(tc, head_type);

                PatternRow spec_row;
                // Wildcard expands to arity wildcards + rest
                for (int i = 0; i < tc.arity; ++i) {
                    spec_row.columns.push_back(make_wildcard());
                }
                for (size_t i = 1; i < row.columns.size(); ++i) {
                    spec_row.columns.push_back(clone_pattern(*row.columns[i]));
                }

                std::vector<ThirType> new_types;
                new_types.insert(new_types.end(), sub_types.begin(), sub_types.end());
                new_types.insert(new_types.end(), types.begin() + 1, types.end());

                if (is_useful(spec_matrix, spec_row, new_types)) {
                    return true;
                }
            }
            return false;
        } else {
            // Not all constructors covered — use default matrix
            auto def = default_matrix(matrix, types);
            PatternRow rest;
            for (size_t i = 1; i < row.columns.size(); ++i) {
                rest.columns.push_back(clone_pattern(*row.columns[i]));
            }
            std::vector<ThirType> rest_types(types.begin() + 1, types.end());
            return is_useful(def, rest, rest_types);
        }
    } else {
        // The row starts with a specific constructor — specialize
        auto spec_matrix = specialize_matrix(matrix, ctor, types);
        auto sub_types = constructor_sub_types(ctor, head_type);
        auto sub_pats = pattern_sub_patterns(*head_pattern, ctor);

        PatternRow spec_row;
        for (auto& sp : sub_pats) {
            spec_row.columns.push_back(std::move(sp));
        }
        for (size_t i = 1; i < row.columns.size(); ++i) {
            spec_row.columns.push_back(clone_pattern(*row.columns[i]));
        }

        std::vector<ThirType> new_types;
        new_types.insert(new_types.end(), sub_types.begin(), sub_types.end());
        new_types.insert(new_types.end(), types.begin() + 1, types.end());

        return is_useful(spec_matrix, spec_row, new_types);
    }
}

// ============================================================================
// Type Constructors
// ============================================================================

auto ExhaustivenessChecker::type_constructors(ThirType type) -> std::vector<Constructor> {
    if (!type)
        return {};

    // Bool has exactly two constructors
    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        if (prim.kind == types::PrimitiveKind::Bool) {
            return {
                {Constructor::Kind::BoolTrue, "true", 0, -1},
                {Constructor::Kind::BoolFalse, "false", 0, -1},
            };
        }
        // All other primitives (integers, floats, chars, strings) have infinite constructors
        return {};
    }

    // Enum types have one constructor per variant
    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        auto enum_def = env_->lookup_enum(named.name);
        if (enum_def) {
            std::vector<Constructor> ctors;
            for (size_t i = 0; i < enum_def->variants.size(); ++i) {
                const auto& variant = enum_def->variants[i];
                int arity = static_cast<int>(variant.second.size());
                ctors.push_back(
                    {Constructor::Kind::EnumVariant, variant.first, arity, static_cast<int>(i)});
            }
            return ctors;
        }
        // Named struct type — single constructor
        auto struct_def = env_->lookup_struct(named.name);
        if (struct_def) {
            int arity = static_cast<int>(struct_def->fields.size());
            return {{Constructor::Kind::Struct, named.name, arity, -1}};
        }
    }

    // Tuple types — single constructor with arity = number of elements
    if (type->is<types::TupleType>()) {
        const auto& tuple = type->as<types::TupleType>();
        int arity = static_cast<int>(tuple.elements.size());
        return {{Constructor::Kind::Tuple, "()", arity, -1}};
    }

    // Unit type — single constructor
    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        if (prim.kind == types::PrimitiveKind::Unit) {
            return {{Constructor::Kind::Tuple, "()", 0, -1}};
        }
    }

    return {};
}

// ============================================================================
// Matrix Specialization
// ============================================================================

auto ExhaustivenessChecker::specialize_matrix(const PatternMatrix& matrix, const Constructor& ctor,
                                              const std::vector<ThirType>& types) -> PatternMatrix {
    PatternMatrix result;
    for (const auto& row : matrix.rows) {
        auto spec = specialize_row(row, ctor, types);
        if (spec) {
            result.rows.push_back(std::move(*spec));
        }
    }
    return result;
}

auto ExhaustivenessChecker::specialize_row(const PatternRow& row, const Constructor& ctor,
                                           const std::vector<ThirType>& /*types*/)
    -> std::optional<PatternRow> {
    if (row.columns.empty())
        return std::nullopt;

    const auto& head = *row.columns[0];
    auto head_ctor = pattern_constructor(head);

    if (head_ctor.kind == Constructor::Kind::Wildcard) {
        // Wildcard matches any constructor — expand to arity wildcards
        PatternRow result;
        for (int i = 0; i < ctor.arity; ++i) {
            result.columns.push_back(make_wildcard());
        }
        // Append remaining columns
        for (size_t i = 1; i < row.columns.size(); ++i) {
            result.columns.push_back(clone_pattern(*row.columns[i]));
        }
        return result;
    }

    if (head_ctor.name != ctor.name || head_ctor.kind != ctor.kind) {
        return std::nullopt; // Different constructor — row doesn't match
    }

    // Same constructor — expand sub-patterns
    auto sub_pats = pattern_sub_patterns(head, ctor);
    PatternRow result;
    for (auto& sp : sub_pats) {
        result.columns.push_back(std::move(sp));
    }
    for (size_t i = 1; i < row.columns.size(); ++i) {
        result.columns.push_back(clone_pattern(*row.columns[i]));
    }
    return result;
}

auto ExhaustivenessChecker::default_matrix(const PatternMatrix& matrix,
                                           const std::vector<ThirType>& /*types*/)
    -> PatternMatrix {
    PatternMatrix result;
    for (const auto& row : matrix.rows) {
        if (row.columns.empty())
            continue;

        auto ctor = pattern_constructor(*row.columns[0]);
        if (ctor.kind == Constructor::Kind::Wildcard) {
            PatternRow new_row;
            for (size_t i = 1; i < row.columns.size(); ++i) {
                new_row.columns.push_back(clone_pattern(*row.columns[i]));
            }
            result.rows.push_back(std::move(new_row));
        }
    }
    return result;
}

// ============================================================================
// Pattern Constructor Extraction
// ============================================================================

auto ExhaustivenessChecker::pattern_constructor(const ThirPattern& pattern) -> Constructor {
    if (pattern.is<ThirWildcardPattern>()) {
        return {Constructor::Kind::Wildcard, "_", 0, -1};
    }

    if (pattern.is<ThirBindingPattern>()) {
        // Binding patterns match everything (like wildcards for exhaustiveness)
        return {Constructor::Kind::Wildcard, "_", 0, -1};
    }

    if (pattern.is<ThirLiteralPattern>()) {
        const auto& lit = pattern.as<ThirLiteralPattern>();
        return std::visit(
            [](const auto& v) -> Constructor {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>) {
                    return {v ? Constructor::Kind::BoolTrue : Constructor::Kind::BoolFalse,
                            v ? "true" : "false", 0, -1};
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    return {Constructor::Kind::IntLiteral, std::to_string(v), 0, -1};
                } else if constexpr (std::is_same_v<T, uint64_t>) {
                    return {Constructor::Kind::IntLiteral, std::to_string(v), 0, -1};
                } else if constexpr (std::is_same_v<T, char>) {
                    return {Constructor::Kind::CharLiteral, std::string(1, v), 0, -1};
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return {Constructor::Kind::StrLiteral, v, 0, -1};
                } else {
                    // double
                    return {Constructor::Kind::IntLiteral, std::to_string(v), 0, -1};
                }
            },
            lit.value);
    }

    if (pattern.is<ThirEnumPattern>()) {
        const auto& ep = pattern.as<ThirEnumPattern>();
        int arity = ep.payload ? static_cast<int>(ep.payload->size()) : 0;
        return {Constructor::Kind::EnumVariant, ep.variant_name, arity, ep.variant_index};
    }

    if (pattern.is<ThirTuplePattern>()) {
        const auto& tp = pattern.as<ThirTuplePattern>();
        int arity = static_cast<int>(tp.elements.size());
        return {Constructor::Kind::Tuple, "()", arity, -1};
    }

    if (pattern.is<ThirStructPattern>()) {
        const auto& sp = pattern.as<ThirStructPattern>();
        int arity = static_cast<int>(sp.fields.size());
        return {Constructor::Kind::Struct, sp.struct_name, arity, -1};
    }

    if (pattern.is<ThirRangePattern>()) {
        const auto& rp = pattern.as<ThirRangePattern>();
        std::string name;
        if (rp.start && rp.end) {
            name = std::to_string(*rp.start) + (rp.inclusive ? " through " : " to ") +
                   std::to_string(*rp.end);
        } else if (rp.start) {
            name = std::to_string(*rp.start) + "..";
        } else if (rp.end) {
            name = ".." + std::to_string(*rp.end);
        }
        return {Constructor::Kind::Range, name, 0, -1};
    }

    if (pattern.is<ThirOrPattern>()) {
        // Or patterns are treated as wildcards for constructor extraction
        // (they should be expanded before reaching here in a full implementation)
        return {Constructor::Kind::Wildcard, "_", 0, -1};
    }

    if (pattern.is<ThirArrayPattern>()) {
        const auto& ap = pattern.as<ThirArrayPattern>();
        int arity = static_cast<int>(ap.elements.size());
        return {Constructor::Kind::Tuple, "[]", arity, -1};
    }

    return {Constructor::Kind::Wildcard, "_", 0, -1};
}

// ============================================================================
// Sub-patterns
// ============================================================================

auto ExhaustivenessChecker::constructor_sub_types(const Constructor& ctor, ThirType type)
    -> std::vector<ThirType> {
    if (ctor.arity == 0)
        return {};

    if (ctor.kind == Constructor::Kind::EnumVariant && type && type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        auto enum_def = env_->lookup_enum(named.name);
        if (enum_def && ctor.variant_index >= 0 &&
            static_cast<size_t>(ctor.variant_index) < enum_def->variants.size()) {
            return enum_def->variants[ctor.variant_index].second;
        }
    }

    if (ctor.kind == Constructor::Kind::Tuple && type && type->is<types::TupleType>()) {
        return type->as<types::TupleType>().elements;
    }

    if (ctor.kind == Constructor::Kind::Struct && type && type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        auto struct_def = env_->lookup_struct(named.name);
        if (struct_def) {
            std::vector<ThirType> sub_types;
            for (const auto& field : struct_def->fields) {
                sub_types.push_back(field.type);
            }
            return sub_types;
        }
    }

    // Fallback: return arity many nullptr types
    return std::vector<ThirType>(ctor.arity, nullptr);
}

auto ExhaustivenessChecker::pattern_sub_patterns(const ThirPattern& pattern,
                                                 const Constructor& ctor)
    -> std::vector<ThirPatternPtr> {
    if (pattern.is<ThirEnumPattern>()) {
        const auto& ep = pattern.as<ThirEnumPattern>();
        if (ep.payload) {
            std::vector<ThirPatternPtr> result;
            for (const auto& p : *ep.payload) {
                result.push_back(clone_pattern(*p));
            }
            return result;
        }
    }

    if (pattern.is<ThirTuplePattern>()) {
        const auto& tp = pattern.as<ThirTuplePattern>();
        std::vector<ThirPatternPtr> result;
        for (const auto& p : tp.elements) {
            result.push_back(clone_pattern(*p));
        }
        return result;
    }

    if (pattern.is<ThirStructPattern>()) {
        const auto& sp = pattern.as<ThirStructPattern>();
        std::vector<ThirPatternPtr> result;
        for (const auto& [_, p] : sp.fields) {
            result.push_back(clone_pattern(*p));
        }
        return result;
    }

    if (pattern.is<ThirArrayPattern>()) {
        const auto& ap = pattern.as<ThirArrayPattern>();
        std::vector<ThirPatternPtr> result;
        for (const auto& p : ap.elements) {
            result.push_back(clone_pattern(*p));
        }
        return result;
    }

    // For wildcards/bindings matched against a multi-arity ctor: expand to wildcards
    std::vector<ThirPatternPtr> result;
    for (int i = 0; i < ctor.arity; ++i) {
        result.push_back(make_wildcard());
    }
    return result;
}

// ============================================================================
// Helpers
// ============================================================================

auto ExhaustivenessChecker::make_wildcard() -> ThirPatternPtr {
    auto pat = std::make_unique<ThirPattern>();
    pat->kind = ThirWildcardPattern{INVALID_THIR_ID, SourceSpan{}};
    return pat;
}

auto ExhaustivenessChecker::format_constructor(const Constructor& ctor, ThirType type)
    -> std::string {
    switch (ctor.kind) {
    case Constructor::Kind::EnumVariant:
        if (type && type->is<types::NamedType>()) {
            return type->as<types::NamedType>().name + "::" + ctor.name;
        }
        return ctor.name;
    case Constructor::Kind::BoolTrue:
        return "true";
    case Constructor::Kind::BoolFalse:
        return "false";
    case Constructor::Kind::IntLiteral:
        return ctor.name;
    case Constructor::Kind::CharLiteral:
        return "'" + ctor.name + "'";
    case Constructor::Kind::StrLiteral:
        return "\"" + ctor.name + "\"";
    case Constructor::Kind::Wildcard:
        return "_";
    case Constructor::Kind::Tuple:
        return "(...)";
    case Constructor::Kind::Struct:
        return ctor.name + " { ... }";
    case Constructor::Kind::Range:
        return ctor.name;
    case Constructor::Kind::Missing:
        return "_";
    }
    return "_";
}

} // namespace tml::thir
