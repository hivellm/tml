//! # Type Checker - Type Expressions
//!
//! This file implements type checking for composite type expressions.
//!
//! ## Type Constructors
//!
//! | Expression   | Handler              | Result Type               |
//! |--------------|----------------------|---------------------------|
//! | `(a, b, c)`  | `check_tuple`        | `TupleType`               |
//! | `[1, 2, 3]`  | `check_array`        | `ArrayType`               |
//! | `Foo { .. }` | `check_struct_expr`  | `NamedType`               |
//! | `do(x) expr` | `check_closure`      | `ClosureType`             |
//! | `expr!`      | `check_try`          | Unwrapped `Maybe/Outcome` |
//!
//! ## Closure Capture Analysis
//!
//! `collect_captures_from_expr()` identifies variables captured by closures:
//! - Variables not defined in closure scope but in parent scope are captured
//! - Captured variables are stored in `closure.captured_vars` for codegen
//!
//! ## Path Resolution
//!
//! `check_path()` resolves multi-segment paths:
//! - Single segment: variable, function, or type name
//! - Two segments: `Type::method` or `Enum::Variant`

#include "types/checker.hpp"

#include <algorithm>
#include <unordered_set>

namespace tml::types {

auto TypeChecker::check_tuple(const parser::TupleExpr& tuple) -> TypePtr {
    std::vector<TypePtr> element_types;
    for (const auto& elem : tuple.elements) {
        element_types.push_back(check_expr(*elem));
    }
    return make_tuple(std::move(element_types));
}

auto TypeChecker::check_array(const parser::ArrayExpr& array) -> TypePtr {
    return check_array(array, nullptr);
}

auto TypeChecker::check_array(const parser::ArrayExpr& array, TypePtr expected_type) -> TypePtr {
    // Extract element type from expected array type for literal coercion
    TypePtr expected_elem_type = nullptr;
    if (expected_type && expected_type->is<ArrayType>()) {
        expected_elem_type = expected_type->as<ArrayType>().element;
    }

    return std::visit(
        [this, expected_elem_type](const auto& arr) -> TypePtr {
            using T = std::decay_t<decltype(arr)>;

            if constexpr (std::is_same_v<T, std::vector<parser::ExprPtr>>) {
                // [1, 2, 3] form
                if (arr.empty()) {
                    // Use expected element type if available for empty arrays
                    if (expected_elem_type) {
                        return make_array(expected_elem_type, 0);
                    }
                    return make_array(env_.fresh_type_var(), 0);
                }
                // Pass expected element type for numeric literal coercion
                auto first_type = check_expr(*arr[0], expected_elem_type);
                for (size_t i = 1; i < arr.size(); ++i) {
                    check_expr(*arr[i], expected_elem_type ? expected_elem_type : first_type);
                }
                return make_array(expected_elem_type ? expected_elem_type : first_type, arr.size());
            } else {
                // [expr; count] form
                auto elem_type = check_expr(*arr.first, expected_elem_type);
                check_expr(*arr.second); // The count expression

                // Evaluate array size from count expression (must be compile-time constant)
                size_t arr_size = 0;
                if (arr.second->template is<parser::LiteralExpr>()) {
                    const auto& lit = arr.second->template as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                        const auto& val = lit.token.int_value();
                        arr_size = static_cast<size_t>(val.value);
                    }
                }
                return make_array(expected_elem_type ? expected_elem_type : elem_type, arr_size);
            }
        },
        array.kind);
}

auto TypeChecker::check_struct_expr(const parser::StructExpr& struct_expr) -> TypePtr {
    std::string name = struct_expr.path.segments.empty() ? "" : struct_expr.path.segments.back();
    auto struct_def = env_.lookup_struct(name);

    if (struct_def) {
        // Build set of provided field names
        std::unordered_set<std::string> provided_fields;
        for (const auto& [field_name, field_expr] : struct_expr.fields) {
            provided_fields.insert(field_name);
        }

        // Check field expressions with expected field types for coercion
        for (const auto& [field_name, field_expr] : struct_expr.fields) {
            // Look up the expected field type
            TypePtr expected_field_type = nullptr;
            bool field_found = false;
            for (const auto& fld : struct_def->fields) {
                if (fld.name == field_name) {
                    expected_field_type = fld.type;
                    field_found = true;
                    break;
                }
            }
            if (!field_found) {
                std::string type_kind = struct_def->is_union ? "union" : "struct";
                error("Unknown field '" + field_name + "' in " + type_kind + " '" + name + "'",
                      struct_expr.span, "T005");
            }
            check_expr(*field_expr, expected_field_type);
        }

        // For unions: exactly one field must be provided
        // For structs: all fields without defaults must be provided
        if (struct_def->is_union) {
            if (provided_fields.empty()) {
                error("Union literal requires exactly one field initializer", struct_expr.span,
                      "T005");
            } else if (provided_fields.size() > 1) {
                error("Union literal can only initialize one field at a time", struct_expr.span,
                      "T005");
            }
        } else {
            // Regular struct - check all fields without defaults are provided
            for (const auto& fld : struct_def->fields) {
                if (provided_fields.find(fld.name) == provided_fields.end()) {
                    // Field is missing - check if it has a default
                    if (!fld.has_default) {
                        error("Missing field '" + fld.name +
                                  "' in struct literal (no default value)",
                              struct_expr.span, "T005");
                    }
                }
            }
        }

        // Struct type
        auto type = std::make_shared<Type>();
        type->kind = NamedType{name, "", {}};
        return type;
    }

    // Check if it's a class type
    auto class_def = env_.lookup_class(name);
    if (class_def) {
        // Check field expressions with expected field types for coercion
        for (const auto& [field_name, field_expr] : struct_expr.fields) {
            // Look up the expected field type from class fields
            TypePtr expected_field_type = nullptr;
            for (const auto& class_field : class_def->fields) {
                if (class_field.name == field_name) {
                    expected_field_type = class_field.type;
                    break;
                }
            }
            check_expr(*field_expr, expected_field_type);
        }

        // Class type - return ClassType
        auto type = std::make_shared<Type>();
        type->kind = ClassType{name, "", {}};
        return type;
    }

    // Unknown type - still check expressions without expected types
    for (const auto& [field_name, field_expr] : struct_expr.fields) {
        check_expr(*field_expr);
    }

    error("Unknown struct or class: " + name, struct_expr.span, "T022");
    return make_unit();
}

auto TypeChecker::check_closure(const parser::ClosureExpr& closure) -> TypePtr {
    // Save parent scope to detect captures
    auto parent_scope = env_.current_scope();

    // Create a temporary empty scope for capture analysis
    env_.push_scope();
    auto temp_scope = env_.current_scope();

    // Collect captures BEFORE adding parameters to scope
    std::vector<CapturedVar> captures;
    collect_captures_from_expr(*closure.body, temp_scope, parent_scope, captures);

    // Define parameters in closure scope (reuse the same scope)
    auto closure_scope = temp_scope;
    std::vector<TypePtr> param_types;
    for (const auto& [pattern, type_opt] : closure.params) {
        TypePtr ptype = type_opt ? resolve_type(**type_opt) : env_.fresh_type_var();
        param_types.push_back(ptype);
        if (pattern->is<parser::IdentPattern>()) {
            auto& ident = pattern->as<parser::IdentPattern>();
            closure_scope->define(ident.name, ptype, ident.is_mut, pattern->span);
        }
    }

    // Store captured variable names in AST for codegen
    closure.captured_vars.clear();
    for (const auto& cap : captures) {
        closure.captured_vars.push_back(cap.name);
    }

    auto body_type = check_expr(*closure.body);
    TypePtr return_type = closure.return_type ? resolve_type(**closure.return_type) : body_type;

    env_.pop_scope();

    return make_closure(std::move(param_types), return_type, std::move(captures));
}

auto TypeChecker::check_try(const parser::TryExpr& try_expr) -> TypePtr {
    // The try operator (!) unwraps Outcome[T, E] or Maybe[T], propagating errors.
    // For Outcome[T, E], it returns T on Ok, or early-returns Err(E).
    // For Maybe[T], it returns T on Just, or early-returns/panics on Nothing.
    auto expr_type = check_expr(*try_expr.expr);

    if (!expr_type) {
        return make_unit();
    }

    // Check if it's a NamedType (Outcome or Maybe)
    if (expr_type->is<NamedType>()) {
        const auto& named = expr_type->as<NamedType>();

        // Handle Outcome[T, E] - returns T
        if (named.name == "Outcome" && named.type_args.size() >= 1) {
            return named.type_args[0]; // Return the T (success) type
        }

        // Handle Maybe[T] - returns T
        if (named.name == "Maybe" && named.type_args.size() >= 1) {
            return named.type_args[0]; // Return the T (Just value) type
        }
    }

    // If not Outcome or Maybe, report an error but continue with the original type
    // This allows partial compilation while flagging the issue
    error("try operator (!) can only be used on Outcome[T, E] or Maybe[T] types, got " +
              type_to_string(expr_type),
          try_expr.span, "T033");
    return expr_type;
}

void TypeChecker::collect_captures_from_expr(const parser::Expr& expr,
                                             std::shared_ptr<Scope> closure_scope,
                                             std::shared_ptr<Scope> parent_scope,
                                             std::vector<CapturedVar>& captures) {
    std::visit(
        [&](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                // Check if this identifier is local to closure (parameter)
                auto local_sym = closure_scope->lookup_local(e.name);
                if (!local_sym.has_value() && parent_scope) {
                    // Not a closure parameter, check if it's in parent scope (captured)
                    auto parent_sym = parent_scope->lookup(e.name);
                    if (parent_sym.has_value()) {
                        // This is a captured variable - add it if not already captured
                        bool already_captured = false;
                        for (const auto& cap : captures) {
                            if (cap.name == e.name) {
                                already_captured = true;
                                break;
                            }
                        }
                        if (!already_captured) {
                            captures.push_back(
                                CapturedVar{e.name, parent_sym->type, parent_sym->is_mutable});
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                collect_captures_from_expr(*e.left, closure_scope, parent_scope, captures);
                collect_captures_from_expr(*e.right, closure_scope, parent_scope, captures);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                collect_captures_from_expr(*e.operand, closure_scope, parent_scope, captures);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                collect_captures_from_expr(*e.callee, closure_scope, parent_scope, captures);
                for (const auto& arg : e.args) {
                    collect_captures_from_expr(*arg, closure_scope, parent_scope, captures);
                }
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                for (const auto& stmt : e.stmts) {
                    if (stmt->kind.index() == 3) { // ExprStmt
                        auto& expr_stmt = std::get<parser::ExprStmt>(stmt->kind);
                        collect_captures_from_expr(*expr_stmt.expr, closure_scope, parent_scope,
                                                   captures);
                    }
                }
                if (e.expr) {
                    collect_captures_from_expr(**e.expr, closure_scope, parent_scope, captures);
                }
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                collect_captures_from_expr(*e.condition, closure_scope, parent_scope, captures);
                collect_captures_from_expr(*e.then_branch, closure_scope, parent_scope, captures);
                if (e.else_branch) {
                    collect_captures_from_expr(**e.else_branch, closure_scope, parent_scope,
                                               captures);
                }
            } else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
                collect_captures_from_expr(*e.condition, closure_scope, parent_scope, captures);
                collect_captures_from_expr(*e.true_value, closure_scope, parent_scope, captures);
                collect_captures_from_expr(*e.false_value, closure_scope, parent_scope, captures);
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                if (e.value) {
                    collect_captures_from_expr(**e.value, closure_scope, parent_scope, captures);
                }
            }
            // Add more cases as needed for other expression types
        },
        expr.kind);
}

auto TypeChecker::check_path(const parser::PathExpr& path_expr, SourceSpan span) -> TypePtr {
    const auto& segments = path_expr.path.segments;

    if (segments.empty()) {
        return make_unit();
    }

    if (segments.size() == 1) {
        auto sym = env_.current_scope()->lookup(segments[0]);
        if (sym) {
            return sym->type;
        }
        auto func = env_.lookup_func(segments[0]);
        if (func) {
            return make_func(func->params, func->return_type);
        }

        // Check if this is a type name (for static method calls like List[T].new())
        auto struct_def = env_.lookup_struct(segments[0]);
        if (struct_def) {
            auto type = std::make_shared<Type>();
            // Apply generic arguments if provided
            std::vector<TypePtr> type_args;
            if (path_expr.generics) {
                for (const auto& arg : path_expr.generics->args) {
                    if (arg.is_type()) {
                        type_args.push_back(resolve_type(*arg.as_type()));
                    }
                }
            }
            type->kind = NamedType{segments[0], "", std::move(type_args)};
            return type;
        }

        auto enum_def = env_.lookup_enum(segments[0]);
        if (enum_def) {
            auto type = std::make_shared<Type>();
            std::vector<TypePtr> type_args;
            if (path_expr.generics) {
                for (const auto& arg : path_expr.generics->args) {
                    if (arg.is_type()) {
                        type_args.push_back(resolve_type(*arg.as_type()));
                    }
                }
            }
            type->kind = NamedType{segments[0], "", std::move(type_args)};
            return type;
        }

        // Check imported types from modules
        auto imported_path = env_.resolve_imported_symbol(segments[0]);
        if (imported_path.has_value()) {
            std::string module_path;
            size_t pos = imported_path->rfind("::");
            if (pos != std::string::npos) {
                module_path = imported_path->substr(0, pos);
            }

            auto module = env_.get_module(module_path);
            if (module) {
                // Check if it's a struct
                auto struct_it = module->structs.find(segments[0]);
                if (struct_it != module->structs.end()) {
                    auto type = std::make_shared<Type>();
                    std::vector<TypePtr> type_args;
                    if (path_expr.generics) {
                        for (const auto& arg : path_expr.generics->args) {
                            if (arg.is_type()) {
                                type_args.push_back(resolve_type(*arg.as_type()));
                            }
                        }
                    }
                    type->kind = NamedType{segments[0], module_path, std::move(type_args)};
                    return type;
                }

                // Check if it's an enum
                auto enum_it = module->enums.find(segments[0]);
                if (enum_it != module->enums.end()) {
                    auto type = std::make_shared<Type>();
                    std::vector<TypePtr> type_args;
                    if (path_expr.generics) {
                        for (const auto& arg : path_expr.generics->args) {
                            if (arg.is_type()) {
                                type_args.push_back(resolve_type(*arg.as_type()));
                            }
                        }
                    }
                    type->kind = NamedType{segments[0], module_path, std::move(type_args)};
                    return type;
                }
            }
        }

        // Build error message with suggestions
        std::string msg = "Undefined: " + segments[0];
        auto all_names = get_all_known_names();
        auto similar = find_similar_names(segments[0], all_names);
        if (!similar.empty()) {
            msg += ". Did you mean: ";
            for (size_t i = 0; i < similar.size(); ++i) {
                if (i > 0)
                    msg += ", ";
                msg += "`" + similar[i] + "`";
            }
            msg += "?";
        }
        error(msg, span);
    }

    if (segments.size() == 2) {
        // Try function lookup with full path first (e.g., "Instant::now")
        std::string full_name = segments[0] + "::" + segments[1];
        auto func = env_.lookup_func(full_name);
        if (func) {
            return make_func(func->params, func->return_type);
        }

        // Then try enum variant lookup (local enums first)
        auto enum_def = env_.lookup_enum(segments[0]);
        std::string module_path = "";

        // If not found locally, check imports
        if (!enum_def) {
            auto imported_path = env_.resolve_imported_symbol(segments[0]);
            if (imported_path.has_value()) {
                size_t pos = imported_path->rfind("::");
                if (pos != std::string::npos) {
                    module_path = imported_path->substr(0, pos);
                    auto module = env_.get_module(module_path);
                    if (module) {
                        auto enum_it = module->enums.find(segments[0]);
                        if (enum_it != module->enums.end()) {
                            enum_def = enum_it->second;
                        }
                    }
                }
            }
        }

        if (enum_def) {
            for (const auto& variant_pair : enum_def->variants) {
                if (variant_pair.first == segments[1]) {
                    // Get type arguments from path_expr.generics if provided
                    std::vector<TypePtr> type_args;
                    if (path_expr.generics) {
                        for (const auto& arg : path_expr.generics->args) {
                            if (arg.is_type()) {
                                type_args.push_back(resolve_type(*arg.as_type()));
                            }
                        }
                    }

                    auto enum_type = std::make_shared<Type>();
                    enum_type->kind = NamedType{segments[0], module_path, std::move(type_args)};

                    const auto& payload_types = variant_pair.second;
                    if (payload_types.empty()) {
                        // No payload - return enum type directly
                        return enum_type;
                    } else {
                        // Has payload - return function type: payload_types -> enum_type
                        return make_func(payload_types, enum_type);
                    }
                }
            }
        }

        // Try impl constant lookup (e.g., I32::MIN, I32::MAX)
        std::string qualified_name = segments[0] + "::" + segments[1];
        auto constant_sym = env_.current_scope()->lookup(qualified_name);
        if (constant_sym) {
            return constant_sym->type;
        }

        // Check module registry for impl constants from imported modules
        // This handles cases like AtomicBool::LOCK_FREE where the constant
        // is defined in an impl block in the module
        auto const_imported_path = env_.resolve_imported_symbol(segments[0]);
        if (const_imported_path.has_value()) {
            size_t const_pos = const_imported_path->rfind("::");
            if (const_pos != std::string::npos) {
                std::string const_module_path = const_imported_path->substr(0, const_pos);
                auto const_module = env_.get_module(const_module_path);
                if (const_module) {
                    auto const_it = const_module->constants.find(qualified_name);
                    if (const_it != const_module->constants.end()) {
                        // Convert tml_type to TypePtr
                        const std::string& tml_type = const_it->second.tml_type;
                        if (tml_type == "Bool") {
                            return make_primitive(PrimitiveKind::Bool);
                        } else if (tml_type == "I8") {
                            return make_primitive(PrimitiveKind::I8);
                        } else if (tml_type == "I16") {
                            return make_primitive(PrimitiveKind::I16);
                        } else if (tml_type == "I32") {
                            return make_primitive(PrimitiveKind::I32);
                        } else if (tml_type == "I64") {
                            return make_primitive(PrimitiveKind::I64);
                        } else if (tml_type == "I128") {
                            return make_primitive(PrimitiveKind::I128);
                        } else if (tml_type == "U8") {
                            return make_primitive(PrimitiveKind::U8);
                        } else if (tml_type == "U16") {
                            return make_primitive(PrimitiveKind::U16);
                        } else if (tml_type == "U32") {
                            return make_primitive(PrimitiveKind::U32);
                        } else if (tml_type == "U64") {
                            return make_primitive(PrimitiveKind::U64);
                        } else if (tml_type == "U128") {
                            return make_primitive(PrimitiveKind::U128);
                        } else if (tml_type == "F32") {
                            return make_primitive(PrimitiveKind::F32);
                        } else if (tml_type == "F64") {
                            return make_primitive(PrimitiveKind::F64);
                        }
                        // For other types, create a named type
                        auto type = std::make_shared<Type>();
                        type->kind = NamedType{tml_type, const_module_path, {}};
                        return type;
                    }
                }
            }
        }

        // Also check for primitive type constants directly
        // This handles cases where the constant wasn't registered in scope
        // but the type is a known primitive
        if (segments[1] == "MIN" || segments[1] == "MAX") {
            static const std::unordered_map<std::string, PrimitiveKind> primitive_kinds = {
                {"I8", PrimitiveKind::I8},     {"I16", PrimitiveKind::I16},
                {"I32", PrimitiveKind::I32},   {"I64", PrimitiveKind::I64},
                {"I128", PrimitiveKind::I128}, {"U8", PrimitiveKind::U8},
                {"U16", PrimitiveKind::U16},   {"U32", PrimitiveKind::U32},
                {"U64", PrimitiveKind::U64},   {"U128", PrimitiveKind::U128}};
            auto prim_it = primitive_kinds.find(segments[0]);
            if (prim_it != primitive_kinds.end()) {
                return make_primitive(prim_it->second);
            }
        }

        // Check for class static field access (e.g., Counter::count)
        auto class_def = env_.lookup_class(segments[0]);
        if (class_def.has_value()) {
            for (const auto& field : class_def->fields) {
                if (field.name == segments[1] && field.is_static) {
                    return field.type;
                }
            }
        }
    }

    return make_unit();
}

} // namespace tml::types
