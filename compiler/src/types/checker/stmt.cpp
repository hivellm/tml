//! # Type Checker - Statements
//!
//! This file implements type checking for statements.
//!
//! ## Statement Types
//!
//! | Statement | Handler       | Description                        |
//! |-----------|---------------|------------------------------------|
//! | `let`     | `check_let`   | Immutable binding with type check  |
//! | `var`     | `check_var`   | Mutable binding with type check    |
//! | `expr`    | `check_expr`  | Expression statement               |
//!
//! ## Type Annotations
//!
//! TML requires explicit type annotations on `let` and `var` statements.
//! Unlike Rust, type inference is limited to the initializer expression.
//!
//! ## Pattern Binding
//!
//! `bind_pattern()` handles destructuring patterns:
//! - `IdentPattern`: Binds name to type in current scope
//! - `TuplePattern`: Destructures tuple types
//! - `EnumPattern`: Matches enum variants with payloads
//! - `WildcardPattern`: Matches any type, binds nothing

#include "common.hpp"
#include "types/checker.hpp"

#include <algorithm>
#include <iostream>

namespace tml::types {

// Forward declarations from helpers.cpp
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

auto TypeChecker::check_stmt(const parser::Stmt& stmt) -> TypePtr {
    return std::visit(
        [this](const auto& s) -> TypePtr {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, parser::LetStmt>) {
                return check_let(s);
            } else if constexpr (std::is_same_v<T, parser::VarStmt>) {
                return check_var(s);
            } else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
                return check_expr(*s.expr);
            } else {
                return make_unit();
            }
        },
        stmt.kind);
}

auto TypeChecker::check_let(const parser::LetStmt& let) -> TypePtr {
    TML_DEBUG_LN("[check_let] Processing let statement");
    // TML requires explicit type annotations on all let statements
    if (!let.type_annotation.has_value()) {
        error("TML requires explicit type annotation on 'let' statements. Add ': Type' after the "
              "variable name.",
              let.span, "T011");
        // Continue with unit type to allow further error checking
        bind_pattern(*let.pattern, make_unit());
        return make_unit();
    }

    TML_DEBUG_LN("[check_let] Has type annotation, calling resolve_type...");
    TML_DEBUG_LN(
        "[check_let] Type annotation variant index: " << (*let.type_annotation)->kind.index());
    TypePtr var_type = resolve_type(**let.type_annotation);
    TML_DEBUG_LN("[check_let] resolved var_type: " << type_to_string(var_type));

    if (let.init) {
        // Pass var_type as expected type for numeric/tuple literal coercion
        TypePtr init_type = check_expr(**let.init, var_type);
        // Check that init type is compatible with declared type
        TypePtr resolved_var = env_.resolve(var_type);
        TypePtr resolved_init = env_.resolve(init_type);
        if (!types_compatible(resolved_var, resolved_init)) {
            error("Type mismatch: expected " + type_to_string(resolved_var) + ", found " +
                      type_to_string(resolved_init),
                  let.span, "T001");
        }
    }

    bind_pattern(*let.pattern, var_type);
    return make_unit();
}

auto TypeChecker::check_var(const parser::VarStmt& var) -> TypePtr {
    // TML requires explicit type annotations on all var statements
    if (!var.type_annotation.has_value()) {
        error("TML requires explicit type annotation on 'var' statements. Add ': Type' after the "
              "variable name.",
              var.span, "T011");
        // Continue with inferred type to allow further error checking
        TypePtr init_type = check_expr(*var.init);
        env_.current_scope()->define(var.name, init_type, true, SourceSpan{});
        return make_unit();
    }

    TypePtr var_type = resolve_type(**var.type_annotation);
    // Pass var_type as expected type for numeric/tuple literal coercion
    TypePtr init_type = check_expr(*var.init, var_type);

    // Type compatibility is checked during expression type checking
    env_.current_scope()->define(var.name, var_type, true, SourceSpan{});
    return make_unit();
}

void TypeChecker::bind_pattern(const parser::Pattern& pattern, TypePtr type) {
    std::visit(
        [this, &type, &pattern](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, parser::IdentPattern>) {
                // Check for duplicate definition in current scope
                auto existing = env_.current_scope()->lookup(p.name);
                if (existing) {
                    error("Duplicate definition of variable '" + p.name + "'", pattern.span,
                          "T008");
                }
                env_.current_scope()->define(p.name, type, p.is_mut, pattern.span);
            } else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
                if (!type->is<TupleType>()) {
                    error("Cannot destructure non-tuple type with tuple pattern", pattern.span,
                          "T035");
                    return;
                }
                auto& tuple = type->as<TupleType>();
                if (p.elements.size() != tuple.elements.size()) {
                    error("Tuple pattern has " + std::to_string(p.elements.size()) +
                              " elements, but type has " + std::to_string(tuple.elements.size()),
                          pattern.span, "T036");
                    return;
                }
                for (size_t i = 0; i < p.elements.size(); ++i) {
                    bind_pattern(*p.elements[i], tuple.elements[i]);
                }
            } else if constexpr (std::is_same_v<T, parser::WildcardPattern>) {
                // Wildcard pattern matches any type, doesn't bind anything
            } else if constexpr (std::is_same_v<T, parser::EnumPattern>) {
                // Extract enum name from type
                if (!type->is<NamedType>()) {
                    error("Pattern expects enum type, but got different type", pattern.span,
                          "T035");
                    return;
                }

                auto& named = type->as<NamedType>();
                std::string enum_name = named.name;

                // Lookup enum definition
                auto enum_def = env_.lookup_enum(enum_name);
                if (!enum_def) {
                    error("Unknown enum type '" + enum_name + "' in pattern", pattern.span, "T023");
                    return;
                }

                // Build substitution map for generic type parameters
                // e.g., for Maybe[I64], map T -> I64
                std::unordered_map<std::string, TypePtr> type_subs;
                for (size_t i = 0; i < enum_def->type_params.size() && i < named.type_args.size();
                     ++i) {
                    type_subs[enum_def->type_params[i]] = named.type_args[i];
                }

                // Find matching variant
                std::string variant_name = p.path.segments.back();
                auto variant_it = std::find_if(
                    enum_def->variants.begin(), enum_def->variants.end(),
                    [&variant_name](const auto& v) { return v.first == variant_name; });

                if (variant_it == enum_def->variants.end()) {
                    error("Unknown variant '" + variant_name + "' in enum '" + enum_name + "'",
                          pattern.span, "T024");
                    return;
                }

                auto& variant_payload_types = variant_it->second;

                // Bind payload patterns if present
                if (p.payload) {
                    if (variant_payload_types.empty()) {
                        error("Variant '" + variant_name +
                                  "' has no payload, but pattern expects one",
                              pattern.span, "T034");
                        return;
                    }

                    if (p.payload->size() != variant_payload_types.size()) {
                        error("Variant '" + variant_name + "' expects " +
                                  std::to_string(variant_payload_types.size()) +
                                  " arguments, but pattern has " +
                                  std::to_string(p.payload->size()),
                              pattern.span, "T034");
                        return;
                    }

                    // Recursively bind each payload element with substituted types
                    for (size_t i = 0; i < p.payload->size(); ++i) {
                        // Substitute generic types (e.g., T -> I64 for Maybe[I64])
                        TypePtr payload_type = substitute_type(variant_payload_types[i], type_subs);
                        bind_pattern(*(*p.payload)[i], payload_type);
                    }
                } else if (!variant_payload_types.empty()) {
                    error("Variant '" + variant_name + "' has payload, but pattern doesn't bind it",
                          pattern.span, "T034");
                    return;
                }
            } else if constexpr (std::is_same_v<T, parser::StructPattern>) {
                // Struct pattern destructuring: Point { x, y }
                if (!type->is<NamedType>()) {
                    error("Cannot destructure non-struct type with struct pattern", pattern.span,
                          "T035");
                    return;
                }

                auto& named = type->as<NamedType>();
                std::string struct_name = named.name;

                // Lookup struct definition
                auto struct_def = env_.lookup_struct(struct_name);
                if (!struct_def) {
                    error("Unknown struct type '" + struct_name + "' in pattern", pattern.span,
                          "T022");
                    return;
                }

                // Build field type map from struct definition
                std::unordered_map<std::string, TypePtr> field_types;
                for (const auto& field : struct_def->fields) {
                    field_types[field.first] = field.second;
                }

                // Bind each field pattern
                for (const auto& [field_name, field_pattern] : p.fields) {
                    auto it = field_types.find(field_name);
                    if (it == field_types.end()) {
                        error("Unknown field '" + field_name + "' in struct '" + struct_name + "'",
                              pattern.span, "T005");
                        continue;
                    }

                    // Recursively bind the field pattern
                    bind_pattern(*field_pattern, it->second);
                }
            } else if constexpr (std::is_same_v<T, parser::RangePattern>) {
                // Range patterns don't bind variables, just match values
                // No binding needed
            } else if constexpr (std::is_same_v<T, parser::ArrayPattern>) {
                // Array pattern destructuring: [a, b, c] or [head, ..rest]
                if (!type->is<ArrayType>()) {
                    error("Cannot destructure non-array type with array pattern", pattern.span,
                          "T035");
                    return;
                }

                auto& arr = type->as<ArrayType>();
                TypePtr element_type = arr.element;

                // Bind each element pattern
                for (size_t i = 0; i < p.elements.size(); ++i) {
                    bind_pattern(*p.elements[i], element_type);
                }

                // Bind rest pattern if present (binds to array/slice of remaining elements)
                if (p.rest) {
                    // Rest binds to an array of the same element type
                    bind_pattern(**p.rest, type);
                }
            }
        },
        pattern.kind);
}

} // namespace tml::types
