// Type checker expression checking
// Handles: check_expr, check_literal, check_ident, check_binary, check_unary, etc.

#include "lexer/token.hpp"
#include "types/checker.hpp"

#include <algorithm>

namespace tml::types {

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);
bool is_float_type(const TypePtr& type);
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

// Helper to get primitive name as string
static std::string primitive_to_string(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::I8:
        return "I8";
    case PrimitiveKind::I16:
        return "I16";
    case PrimitiveKind::I32:
        return "I32";
    case PrimitiveKind::I64:
        return "I64";
    case PrimitiveKind::I128:
        return "I128";
    case PrimitiveKind::U8:
        return "U8";
    case PrimitiveKind::U16:
        return "U16";
    case PrimitiveKind::U32:
        return "U32";
    case PrimitiveKind::U64:
        return "U64";
    case PrimitiveKind::U128:
        return "U128";
    case PrimitiveKind::F32:
        return "F32";
    case PrimitiveKind::F64:
        return "F64";
    case PrimitiveKind::Bool:
        return "Bool";
    case PrimitiveKind::Char:
        return "Char";
    case PrimitiveKind::Str:
        return "Str";
    case PrimitiveKind::Unit:
        return "Unit";
    case PrimitiveKind::Never:
        return "Never";
    }
    return "unknown";
}

auto TypeChecker::check_expr(const parser::Expr& expr) -> TypePtr {
    return std::visit(
        [this, &expr](const auto& e) -> TypePtr {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                return check_literal(e);
            } else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                return check_ident(e, expr.span);
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                return check_binary(e);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                return check_unary(e);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                return check_call(e);
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                return check_method_call(e);
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                return check_field_access(e);
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                return check_index(e);
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                return check_block(e);
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                return check_if(e);
            } else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
                return check_ternary(e);
            } else if constexpr (std::is_same_v<T, parser::IfLetExpr>) {
                return check_if_let(e);
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                return check_when(e);
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                return check_loop(e);
            } else if constexpr (std::is_same_v<T, parser::ForExpr>) {
                return check_for(e);
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                return check_return(e);
            } else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
                return check_break(e);
            } else if constexpr (std::is_same_v<T, parser::ContinueExpr>) {
                return make_never();
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                return check_tuple(e);
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                return check_array(e);
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                return check_struct_expr(e);
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                return check_closure(e);
            } else if constexpr (std::is_same_v<T, parser::TryExpr>) {
                return check_try(e);
            } else if constexpr (std::is_same_v<T, parser::PathExpr>) {
                return check_path(e, expr.span);
            } else if constexpr (std::is_same_v<T, parser::RangeExpr>) {
                return check_range(e);
            } else if constexpr (std::is_same_v<T, parser::InterpolatedStringExpr>) {
                return check_interp_string(e);
            } else {
                return make_unit();
            }
        },
        expr.kind);
}

auto TypeChecker::check_literal(const parser::LiteralExpr& lit) -> TypePtr {
    switch (lit.token.kind) {
    case lexer::TokenKind::IntLiteral:
        return make_i64();
    case lexer::TokenKind::FloatLiteral:
        return make_f64();
    case lexer::TokenKind::StringLiteral:
        return make_str();
    case lexer::TokenKind::CharLiteral:
        return make_primitive(PrimitiveKind::Char);
    case lexer::TokenKind::BoolLiteral:
        return make_bool();
    default:
        return make_unit();
    }
}

auto TypeChecker::check_ident(const parser::IdentExpr& ident, SourceSpan span) -> TypePtr {
    auto sym = env_.current_scope()->lookup(ident.name);
    if (!sym) {
        // Check if it's a function
        auto func = env_.lookup_func(ident.name);
        if (func) {
            return make_func(func->params, func->return_type);
        }

        // Check if it's an enum constructor
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (const auto& [variant_name, payload_types] : enum_def.variants) {
                if (variant_name == ident.name) {
                    if (payload_types.empty()) {
                        auto enum_type = std::make_shared<Type>();
                        enum_type->kind = NamedType{enum_name, "", {}};
                        return enum_type;
                    } else {
                        auto enum_type = std::make_shared<Type>();
                        enum_type->kind = NamedType{enum_name, "", {}};
                        return make_func(payload_types, enum_type);
                    }
                }
            }
        }

        // Check if it's a type name
        auto struct_def = env_.lookup_struct(ident.name);
        if (struct_def) {
            auto type = std::make_shared<Type>();
            type->kind = NamedType{ident.name, "", {}};
            return type;
        }

        auto enum_def = env_.lookup_enum(ident.name);
        if (enum_def) {
            auto type = std::make_shared<Type>();
            type->kind = NamedType{ident.name, "", {}};
            return type;
        }

        // Check imported types
        auto imported_path = env_.resolve_imported_symbol(ident.name);
        if (imported_path.has_value()) {
            std::string module_path;
            size_t pos = imported_path->rfind("::");
            if (pos != std::string::npos) {
                module_path = imported_path->substr(0, pos);
            }

            auto module = env_.get_module(module_path);
            if (module) {
                if (module->structs.find(ident.name) != module->structs.end()) {
                    auto type = std::make_shared<Type>();
                    type->kind = NamedType{ident.name, module_path, {}};
                    return type;
                }
                if (module->enums.find(ident.name) != module->enums.end()) {
                    auto type = std::make_shared<Type>();
                    type->kind = NamedType{ident.name, module_path, {}};
                    return type;
                }
            }
        }

        // Check if it's an enum constructor from an imported module
        for (const auto& [import_name, import_info] : env_.all_imports()) {
            auto imported_module = env_.get_module(import_info.module_path);
            if (!imported_module)
                continue;

            for (const auto& [imported_enum_name, imported_enum_def] : imported_module->enums) {
                for (const auto& [variant_name, payload_types] : imported_enum_def.variants) {
                    if (variant_name == ident.name) {
                        if (payload_types.empty()) {
                            auto enum_type = std::make_shared<Type>();
                            enum_type->kind =
                                NamedType{imported_enum_name, import_info.module_path, {}};
                            return enum_type;
                        } else {
                            auto enum_type = std::make_shared<Type>();
                            enum_type->kind =
                                NamedType{imported_enum_name, import_info.module_path, {}};
                            return make_func(payload_types, enum_type);
                        }
                    }
                }
            }
        }

        // Build error message with suggestions
        std::string msg = "Undefined variable: " + ident.name;
        auto all_names = get_all_known_names();
        auto similar = find_similar_names(ident.name, all_names);
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
        return make_unit();
    }
    return sym->type;
}

auto TypeChecker::check_binary(const parser::BinaryExpr& binary) -> TypePtr {
    auto left = check_expr(*binary.left);
    auto right = check_expr(*binary.right);

    auto check_binary_types = [&](const char* op_name) {
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (!types_compatible(resolved_left, resolved_right)) {
            error(std::string("Binary operator '") + op_name + "' requires matching types, found " +
                      type_to_string(resolved_left) + " and " + type_to_string(resolved_right),
                  binary.left->span);
        }
    };

    auto check_assignable = [&]() {
        if (binary.left->is<parser::IdentExpr>()) {
            const auto& ident = binary.left->as<parser::IdentExpr>();
            auto sym = env_.current_scope()->lookup(ident.name);
            if (sym && !sym->is_mutable) {
                error("Cannot assign to immutable variable '" + ident.name + "'",
                      binary.left->span);
            }
        }
    };

    switch (binary.op) {
    case parser::BinaryOp::Add:
    case parser::BinaryOp::Sub:
    case parser::BinaryOp::Mul:
    case parser::BinaryOp::Div:
    case parser::BinaryOp::Mod:
        check_binary_types("+");
        return left;
    case parser::BinaryOp::Lt:
    case parser::BinaryOp::Le:
    case parser::BinaryOp::Gt:
    case parser::BinaryOp::Ge:
    case parser::BinaryOp::Eq:
    case parser::BinaryOp::Ne:
        check_binary_types("comparison");
        return make_bool();
    case parser::BinaryOp::And:
    case parser::BinaryOp::Or:
        return make_bool();
    case parser::BinaryOp::BitAnd:
    case parser::BinaryOp::BitOr:
    case parser::BinaryOp::BitXor:
    case parser::BinaryOp::Shl:
    case parser::BinaryOp::Shr:
        return left;
    case parser::BinaryOp::Assign:
        check_assignable();
        check_binary_types("=");
        return make_unit();
    case parser::BinaryOp::AddAssign:
    case parser::BinaryOp::SubAssign:
    case parser::BinaryOp::MulAssign:
    case parser::BinaryOp::DivAssign:
    case parser::BinaryOp::ModAssign:
    case parser::BinaryOp::BitAndAssign:
    case parser::BinaryOp::BitOrAssign:
    case parser::BinaryOp::BitXorAssign:
    case parser::BinaryOp::ShlAssign:
    case parser::BinaryOp::ShrAssign:
        check_assignable();
        return make_unit();
    }
    return make_unit();
}

auto TypeChecker::check_unary(const parser::UnaryExpr& unary) -> TypePtr {
    auto operand = check_expr(*unary.operand);

    switch (unary.op) {
    case parser::UnaryOp::Neg:
        return operand;
    case parser::UnaryOp::Not:
        return make_bool();
    case parser::UnaryOp::BitNot:
        return operand;
    case parser::UnaryOp::Ref:
        return make_ref(operand, false);
    case parser::UnaryOp::RefMut:
        return make_ref(operand, true);
    case parser::UnaryOp::Deref:
        if (operand->is<RefType>()) {
            return operand->as<RefType>().inner;
        }
        if (operand->is<PtrType>()) {
            return operand->as<PtrType>().inner;
        }
        error("Cannot dereference non-reference type", unary.operand->span);
        return make_unit();
    case parser::UnaryOp::Inc:
    case parser::UnaryOp::Dec:
        return operand;
    }
    return make_unit();
}

auto TypeChecker::check_call(const parser::CallExpr& call) -> TypePtr {
    // Check if this is a polymorphic builtin
    if (call.callee->is<parser::IdentExpr>()) {
        const auto& name = call.callee->as<parser::IdentExpr>().name;
        if (name == "print" || name == "println") {
            for (const auto& arg : call.args) {
                check_expr(*arg);
            }
            return make_unit();
        }
    }

    // Check function lookup first
    if (call.callee->is<parser::IdentExpr>()) {
        auto& ident = call.callee->as<parser::IdentExpr>();

        auto func = env_.lookup_func(ident.name);
        if (func) {
            // Handle generic functions
            if (!func->type_params.empty()) {
                std::unordered_map<std::string, TypePtr> substitutions;
                for (size_t i = 0; i < call.args.size() && i < func->params.size(); ++i) {
                    auto arg_type = check_expr(*call.args[i]);
                    if (func->params[i]->is<NamedType>()) {
                        const auto& named = func->params[i]->as<NamedType>();
                        for (const auto& tp : func->type_params) {
                            if (named.name == tp && named.type_args.empty()) {
                                substitutions[tp] = arg_type;
                                break;
                            }
                        }
                    }
                    if (func->params[i]->is<GenericType>()) {
                        const auto& gen = func->params[i]->as<GenericType>();
                        for (const auto& tp : func->type_params) {
                            if (gen.name == tp) {
                                substitutions[tp] = arg_type;
                                break;
                            }
                        }
                    }
                }

                // Check where clause constraints
                for (const auto& constraint : func->where_constraints) {
                    auto it = substitutions.find(constraint.type_param);
                    if (it != substitutions.end()) {
                        std::string type_name;
                        if (it->second->is<PrimitiveType>()) {
                            type_name = primitive_to_string(it->second->as<PrimitiveType>().kind);
                        } else if (it->second->is<NamedType>()) {
                            type_name = it->second->as<NamedType>().name;
                        }

                        for (const auto& behavior : constraint.required_behaviors) {
                            if (!env_.type_implements(type_name, behavior)) {
                                error("Type '" + type_name + "' does not implement behavior '" +
                                          behavior + "' required by constraint on " +
                                          constraint.type_param,
                                      call.callee->span);
                            }
                        }
                    }
                }

                return substitute_type(func->return_type, substitutions);
            }
            return func->return_type;
        }

        // Try enum constructor lookup
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (const auto& [variant_name, payload_types] : enum_def.variants) {
                if (variant_name == ident.name) {
                    if (call.args.size() != payload_types.size()) {
                        error("Enum variant '" + variant_name + "' expects " +
                                  std::to_string(payload_types.size()) + " arguments, but got " +
                                  std::to_string(call.args.size()),
                              call.callee->span);
                        return make_unit();
                    }

                    for (size_t i = 0; i < call.args.size(); ++i) {
                        auto arg_type = check_expr(*call.args[i]);
                        (void)arg_type;
                    }

                    auto enum_type = std::make_shared<Type>();
                    enum_type->kind = NamedType{enum_name, "", {}};
                    return enum_type;
                }
            }
        }
    }

    // Fallback: check callee type
    auto callee_type = check_expr(*call.callee);
    if (callee_type->is<FuncType>()) {
        auto& func = callee_type->as<FuncType>();
        if (call.args.size() != func.params.size()) {
            error("Wrong number of arguments", call.callee->span);
        }
        for (size_t i = 0; i < std::min(call.args.size(), func.params.size()); ++i) {
            auto arg_type = check_expr(*call.args[i]);
            (void)arg_type;
        }
        return func.return_type;
    }

    return make_unit();
}

auto TypeChecker::check_method_call(const parser::MethodCallExpr& call) -> TypePtr {
    auto receiver_type = check_expr(*call.receiver);

    if (receiver_type->is<NamedType>()) {
        auto& named = receiver_type->as<NamedType>();
        std::string qualified = named.name + "::" + call.method;

        auto func = env_.lookup_func(qualified);
        if (func) {
            return func->return_type;
        }

        if (!named.module_path.empty()) {
            auto module = env_.get_module(named.module_path);
            if (module) {
                auto func_it = module->functions.find(qualified);
                if (func_it != module->functions.end()) {
                    return func_it->second.return_type;
                }
            }
        }

        auto imported_path = env_.resolve_imported_symbol(named.name);
        if (imported_path.has_value()) {
            std::string module_path;
            size_t pos = imported_path->rfind("::");
            if (pos != std::string::npos) {
                module_path = imported_path->substr(0, pos);
            }

            auto module = env_.get_module(module_path);
            if (module) {
                auto func_it = module->functions.find(qualified);
                if (func_it != module->functions.end()) {
                    return func_it->second.return_type;
                }
            }
        }
    }

    if (receiver_type->is<DynBehaviorType>()) {
        auto& dyn = receiver_type->as<DynBehaviorType>();
        auto behavior_def = env_.lookup_behavior(dyn.behavior_name);
        if (behavior_def) {
            for (const auto& method : behavior_def->methods) {
                if (method.name == call.method) {
                    return method.return_type;
                }
            }
            error("Unknown method '" + call.method + "' on behavior '" + dyn.behavior_name + "'",
                  call.receiver->span);
        }
    }
    return make_unit();
}

auto TypeChecker::check_field_access(const parser::FieldExpr& field) -> TypePtr {
    auto obj_type = check_expr(*field.object);

    if (obj_type->is<RefType>()) {
        obj_type = obj_type->as<RefType>().inner;
    }

    if (obj_type->is<NamedType>()) {
        auto& named = obj_type->as<NamedType>();
        auto struct_def = env_.lookup_struct(named.name);
        if (struct_def) {
            std::unordered_map<std::string, TypePtr> subs;
            if (!struct_def->type_params.empty() && !named.type_args.empty()) {
                for (size_t i = 0; i < struct_def->type_params.size() && i < named.type_args.size();
                     ++i) {
                    subs[struct_def->type_params[i]] = named.type_args[i];
                }
            }

            for (const auto& [fname, ftype] : struct_def->fields) {
                if (fname == field.field) {
                    if (!subs.empty()) {
                        return substitute_type(ftype, subs);
                    }
                    return ftype;
                }
            }
            error("Unknown field: " + field.field, field.object->span);
        }
    }

    if (obj_type->is<TupleType>()) {
        auto& tuple = obj_type->as<TupleType>();
        try {
            size_t idx = std::stoul(field.field);
            if (idx < tuple.elements.size()) {
                return tuple.elements[idx];
            }
        } catch (...) {}
        error("Invalid tuple field: " + field.field, field.object->span);
    }

    return make_unit();
}

auto TypeChecker::check_index(const parser::IndexExpr& idx) -> TypePtr {
    auto obj_type = check_expr(*idx.object);
    check_expr(*idx.index);

    if (obj_type->is<ArrayType>()) {
        return obj_type->as<ArrayType>().element;
    }
    if (obj_type->is<SliceType>()) {
        return obj_type->as<SliceType>().element;
    }

    return make_unit();
}

auto TypeChecker::check_block(const parser::BlockExpr& block) -> TypePtr {
    env_.push_scope();
    TypePtr result = make_unit();

    for (const auto& stmt : block.stmts) {
        result = check_stmt(*stmt);
    }

    if (block.expr) {
        result = check_expr(**block.expr);
    }

    env_.pop_scope();
    return result;
}

auto TypeChecker::check_interp_string(const parser::InterpolatedStringExpr& interp) -> TypePtr {
    for (const auto& segment : interp.segments) {
        if (std::holds_alternative<parser::ExprPtr>(segment.content)) {
            const auto& expr = std::get<parser::ExprPtr>(segment.content);
            auto expr_type = check_expr(*expr);
            (void)expr_type;
        }
    }
    return make_str();
}

} // namespace tml::types
