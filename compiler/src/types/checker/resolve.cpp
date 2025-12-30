// Type checker type resolution
// Handles: resolve_type, resolve_type_path, error, block_has_return, stmt_has_return,
// expr_has_return

#include "common.hpp"
#include "types/checker.hpp"

#include <iostream>

namespace tml::types {

auto TypeChecker::resolve_type(const parser::Type& type) -> TypePtr {
    return std::visit(
        [this](const auto& t) -> TypePtr {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, parser::NamedType>) {
                auto base_type = resolve_type_path(t.path);
                // If the parser type has generic arguments, add them to the semantic type
                if (t.generics && !t.generics->args.empty()) {
                    // Resolve each generic argument
                    std::vector<TypePtr> type_args;
                    for (const auto& arg : t.generics->args) {
                        type_args.push_back(resolve_type(*arg));
                    }
                    // Create new type with type_args
                    if (base_type->template is<types::NamedType>()) {
                        auto& named = base_type->template as<types::NamedType>();
                        auto result = std::make_shared<Type>();
                        result->kind =
                            types::NamedType{named.name, named.module_path, std::move(type_args)};
                        return result;
                    }
                }
                return base_type;
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                return make_ref(resolve_type(*t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                auto type_ptr = std::make_shared<Type>();
                type_ptr->kind = types::PtrType{t.is_mut, resolve_type(*t.inner)};
                return type_ptr;
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                // Evaluate array size from expression (must be compile-time constant)
                size_t arr_size = 0;
                if (t.size) {
                    if (t.size->template is<parser::LiteralExpr>()) {
                        const auto& lit = t.size->template as<parser::LiteralExpr>();
                        if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                            const auto& val = lit.token.int_value();
                            arr_size = static_cast<size_t>(val.value);
                        }
                    }
                }
                return make_array(resolve_type(*t.element), arr_size);
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                return make_slice(resolve_type(*t.element));
            } else if constexpr (std::is_same_v<T, parser::InferType>) {
                return env_.fresh_type_var();
            } else if constexpr (std::is_same_v<T, parser::DynType>) {
                // Convert parser DynType to semantic DynBehaviorType
                std::string behavior_name;
                if (!t.behavior.segments.empty()) {
                    behavior_name = t.behavior.segments.back();
                }

                // Verify the behavior exists
                auto behavior_def = env_.lookup_behavior(behavior_name);
                if (!behavior_def) {
                    error("Unknown behavior '" + behavior_name + "' in dyn type", t.span);
                    return make_unit();
                }

                // Resolve generic arguments
                std::vector<TypePtr> type_args;
                if (t.generics) {
                    for (const auto& arg : t.generics->args) {
                        type_args.push_back(resolve_type(*arg));
                    }
                }

                auto result = std::make_shared<Type>();
                result->kind = DynBehaviorType{behavior_name, std::move(type_args), t.is_mut};
                return result;
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                // Convert parser FuncType to semantic FuncType
                std::vector<TypePtr> param_types;
                for (const auto& param : t.params) {
                    param_types.push_back(resolve_type(*param));
                }
                TypePtr ret = t.return_type ? resolve_type(*t.return_type) : make_unit();
                auto result = std::make_shared<Type>();
                result->kind = types::FuncType{param_types, ret, false};
                return result;
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                // Convert parser TupleType to semantic TupleType
                std::vector<TypePtr> element_types;
                for (const auto& elem : t.elements) {
                    element_types.push_back(resolve_type(*elem));
                }
                return make_tuple(std::move(element_types));
            } else {
                return make_unit();
            }
        },
        type.kind);
}

auto TypeChecker::resolve_type_path(const parser::TypePath& path) -> TypePtr {
    if (path.segments.empty())
        return make_unit();

    // Handle multi-segment paths like This::Owned or T::Owned
    if (path.segments.size() == 2) {
        const auto& first = path.segments[0];
        const auto& second = path.segments[1];

        // Handle This::AssociatedType (e.g., This::Owned)
        if (first == "This" && current_self_type_) {
            auto it = current_associated_types_.find(second);
            if (it != current_associated_types_.end()) {
                return it->second;
            }
            // Fall through to try as a regular path
        }

        // Handle T::AssociatedType where T is a type parameter (e.g., T::Owned)
        auto param_it = current_type_params_.find(first);
        if (param_it != current_type_params_.end()) {
            // For now, if T is a type parameter and second is an associated type name,
            // return the associated type. This is a simplification - in full Rust semantics,
            // we'd need to look up which trait T implements to find the associated type.
            auto assoc_it = current_associated_types_.find(second);
            if (assoc_it != current_associated_types_.end()) {
                return assoc_it->second;
            }
            // If the associated type is not defined locally, return a named type placeholder
            auto type = std::make_shared<Type>();
            type->kind = NamedType{second, "", {}};
            return type;
        }
    }

    const auto& name = path.segments.back();

    // Handle 'This' type in impl blocks (single segment case)
    if (name == "This" && current_self_type_) {
        return current_self_type_;
    }

    auto& builtins = env_.builtin_types();
    auto it = builtins.find(name);
    if (it != builtins.end())
        return it->second;

    auto alias = env_.lookup_type_alias(name);
    if (alias)
        return *alias;

    auto struct_def = env_.lookup_struct(name);
    if (struct_def) {
        auto type = std::make_shared<Type>();
        type->kind = NamedType{name, "", {}};
        return type;
    }

    auto enum_def = env_.lookup_enum(name);
    if (enum_def) {
        auto type = std::make_shared<Type>();
        type->kind = NamedType{name, "", {}};
        return type;
    }

    // Check if this is an imported symbol from a module
    auto imported_path = env_.resolve_imported_symbol(name);
    if (imported_path.has_value()) {
        // Extract module path and symbol name
        std::string module_path;
        std::string symbol_name = name;
        size_t pos = imported_path->rfind("::");
        if (pos != std::string::npos) {
            module_path = imported_path->substr(0, pos);
        }

        // Look up the struct/enum in the module
        auto module = env_.get_module(module_path);
        if (module) {
            // Check if it's a struct in the module
            auto struct_it = module->structs.find(symbol_name);
            if (struct_it != module->structs.end()) {
                auto type = std::make_shared<Type>();
                type->kind = NamedType{symbol_name, module_path, {}};
                return type;
            }

            // Check if it's an enum in the module
            auto enum_it = module->enums.find(symbol_name);
            if (enum_it != module->enums.end()) {
                auto type = std::make_shared<Type>();
                type->kind = NamedType{symbol_name, module_path, {}};
                return type;
            }
        }
    }

    auto type = std::make_shared<Type>();
    type->kind = NamedType{name, "", {}};
    return type;
}

void TypeChecker::error(const std::string& message, SourceSpan span) {
    errors_.push_back(TypeError{message, span, {}});
}

// Check if a block contains a return statement
bool TypeChecker::block_has_return(const parser::BlockExpr& block) {
    // Check if any statement has return
    for (const auto& stmt : block.stmts) {
        if (stmt_has_return(*stmt)) {
            return true;
        }
    }

    // Check the final expression
    if (block.expr) {
        return expr_has_return(**block.expr);
    }

    return false;
}

// Check if a statement contains a return
bool TypeChecker::stmt_has_return(const parser::Stmt& stmt) {
    return std::visit(
        [this](const auto& s) -> bool {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, parser::ExprStmt>) {
                return expr_has_return(*s.expr);
            } else if constexpr (std::is_same_v<T, parser::LetStmt>) {
                // Let statements don't contain returns
                return false;
            } else if constexpr (std::is_same_v<T, parser::VarStmt>) {
                // Var statements don't contain returns
                return false;
            } else {
                return false;
            }
        },
        stmt.kind);
}

// Check if an expression contains a return
bool TypeChecker::expr_has_return(const parser::Expr& expr) {
    return std::visit(
        [this](const auto& e) -> bool {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                return true;
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                return block_has_return(e);
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                // If expr has return if both branches have return
                bool then_has = expr_has_return(*e.then_branch);
                bool else_has = e.else_branch.has_value() && expr_has_return(**e.else_branch);
                return then_has && else_has;
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                // When expr has return if all arms have return
                for (const auto& arm : e.arms) {
                    if (!expr_has_return(*arm.body)) {
                        return false;
                    }
                }
                return !e.arms.empty();
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                // Loop can have return in body
                return expr_has_return(*e.body);
            } else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
                // Ternary has return if both branches have it
                return expr_has_return(*e.true_value) && expr_has_return(*e.false_value);
            } else {
                // Most expressions don't contain returns
                return false;
            }
        },
        expr.kind);
}

} // namespace tml::types
