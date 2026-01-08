// Type checker expression checking
// Handles: check_expr, check_literal, check_ident, check_binary, check_unary, etc.

#include "common.hpp"
#include "lexer/token.hpp"
#include "types/checker.hpp"

#include <algorithm>
#include <iostream>

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
            } else if constexpr (std::is_same_v<T, parser::CastExpr>) {
                return check_cast(e);
            } else if constexpr (std::is_same_v<T, parser::AwaitExpr>) {
                return check_await(e, expr.span);
            } else if constexpr (std::is_same_v<T, parser::LowlevelExpr>) {
                return check_lowlevel(e);
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
    case lexer::TokenKind::NullLiteral:
        return make_ptr(make_unit()); // null has type Ptr[Unit]
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
        // First check locally defined types (with empty module_path)
        auto local_struct_it = env_.all_structs().find(ident.name);
        if (local_struct_it != env_.all_structs().end()) {
            auto type = std::make_shared<Type>();
            type->kind = NamedType{ident.name, "", {}};
            return type;
        }

        auto local_enum_it = env_.all_enums().find(ident.name);
        if (local_enum_it != env_.all_enums().end()) {
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
        // Reborrowing: ref (ref T) -> ref T (like Rust's automatic reborrow)
        if (operand->is<RefType>()) {
            return make_ref(operand->as<RefType>().inner, false);
        }
        return make_ref(operand, false);
    case parser::UnaryOp::RefMut:
        // Reborrowing: mut ref (mut ref T) -> mut ref T (like Rust's automatic reborrow)
        if (operand->is<RefType>() && operand->as<RefType>().is_mut) {
            return operand; // Already a mutable ref, just return it
        }
        // Allow reborrow from mutable to mutable
        if (operand->is<RefType>()) {
            return make_ref(operand->as<RefType>().inner, true);
        }
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

    // Check for static method calls on primitive types via PathExpr (e.g., I32::default())
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path = call.callee->as<parser::PathExpr>();
        if (path.path.segments.size() == 2) {
            const std::string& type_name = path.path.segments[0];
            const std::string& method = path.path.segments[1];

            bool is_primitive_type =
                type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                type_name == "U128" || type_name == "F32" || type_name == "F64" ||
                type_name == "Bool" || type_name == "Str";

            if (is_primitive_type && method == "default") {
                if (type_name == "I8")
                    return make_primitive(PrimitiveKind::I8);
                if (type_name == "I16")
                    return make_primitive(PrimitiveKind::I16);
                if (type_name == "I32")
                    return make_primitive(PrimitiveKind::I32);
                if (type_name == "I64")
                    return make_primitive(PrimitiveKind::I64);
                if (type_name == "I128")
                    return make_primitive(PrimitiveKind::I128);
                if (type_name == "U8")
                    return make_primitive(PrimitiveKind::U8);
                if (type_name == "U16")
                    return make_primitive(PrimitiveKind::U16);
                if (type_name == "U32")
                    return make_primitive(PrimitiveKind::U32);
                if (type_name == "U64")
                    return make_primitive(PrimitiveKind::U64);
                if (type_name == "U128")
                    return make_primitive(PrimitiveKind::U128);
                if (type_name == "F32")
                    return make_primitive(PrimitiveKind::F32);
                if (type_name == "F64")
                    return make_primitive(PrimitiveKind::F64);
                if (type_name == "Bool")
                    return make_primitive(PrimitiveKind::Bool);
                if (type_name == "Str")
                    return make_primitive(PrimitiveKind::Str);
            }

            // Handle Type::from(value) for type conversion
            if (is_primitive_type && method == "from" && !call.args.empty()) {
                // Type check the argument (source type)
                check_expr(*call.args[0]);
                // Return the target type
                if (type_name == "I8")
                    return make_primitive(PrimitiveKind::I8);
                if (type_name == "I16")
                    return make_primitive(PrimitiveKind::I16);
                if (type_name == "I32")
                    return make_primitive(PrimitiveKind::I32);
                if (type_name == "I64")
                    return make_primitive(PrimitiveKind::I64);
                if (type_name == "I128")
                    return make_primitive(PrimitiveKind::I128);
                if (type_name == "U8")
                    return make_primitive(PrimitiveKind::U8);
                if (type_name == "U16")
                    return make_primitive(PrimitiveKind::U16);
                if (type_name == "U32")
                    return make_primitive(PrimitiveKind::U32);
                if (type_name == "U64")
                    return make_primitive(PrimitiveKind::U64);
                if (type_name == "U128")
                    return make_primitive(PrimitiveKind::U128);
                if (type_name == "F32")
                    return make_primitive(PrimitiveKind::F32);
                if (type_name == "F64")
                    return make_primitive(PrimitiveKind::F64);
                if (type_name == "Bool")
                    return make_primitive(PrimitiveKind::Bool);
                if (type_name == "Str")
                    return make_primitive(PrimitiveKind::Str);
            }

            // Handle imported type static methods (e.g., Layout::from_size_align)
            if (!is_primitive_type) {
                // Try to resolve type_name as an imported symbol
                auto imported_path = env_.resolve_imported_symbol(type_name);
                if (imported_path.has_value()) {
                    std::string module_path;
                    size_t pos = imported_path->rfind("::");
                    if (pos != std::string::npos) {
                        module_path = imported_path->substr(0, pos);
                    }

                    // Look up the qualified function name in the module
                    std::string qualified_func = type_name + "::" + method;
                    auto module = env_.get_module(module_path);
                    if (module) {
                        auto func_it = module->functions.find(qualified_func);
                        if (func_it != module->functions.end()) {
                            // Type check arguments
                            for (const auto& arg : call.args) {
                                check_expr(*arg);
                            }
                            return func_it->second.return_type;
                        }
                    }
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

        // Infer generic type substitutions from argument types
        // This is needed for generic enum variant constructors like Option::Some(42)
        std::unordered_map<std::string, TypePtr> substitutions;

        for (size_t i = 0; i < std::min(call.args.size(), func.params.size()); ++i) {
            auto arg_type = check_expr(*call.args[i]);
            auto& param_type = func.params[i];

            // If the parameter type is a NamedType that could be a type parameter (T, U, etc.)
            // and it has no type_args, it might be a generic type parameter
            if (param_type->is<NamedType>()) {
                const auto& named = param_type->as<NamedType>();
                // Check if this is a type parameter: empty type_args, empty module_path,
                // and not a known type (struct, enum, primitive, etc.)
                if (named.type_args.empty() && named.module_path.empty() && !named.name.empty()) {
                    bool is_type_param = true;
                    // Check if it's a known type (struct, enum, primitive, etc.)
                    if (env_.lookup_struct(named.name) || env_.lookup_enum(named.name)) {
                        is_type_param = false;
                    }
                    auto& builtins = env_.builtin_types();
                    if (builtins.find(named.name) != builtins.end()) {
                        is_type_param = false;
                    }
                    if (is_type_param) {
                        substitutions[named.name] = arg_type;
                    }
                }
            } else if (param_type->is<GenericType>()) {
                const auto& gen = param_type->as<GenericType>();
                substitutions[gen.name] = arg_type;
            }
        }

        // Apply substitutions to the return type
        TypePtr return_type = func.return_type;
        if (!substitutions.empty()) {
            return_type = substitute_type(return_type, substitutions);
        }

        return return_type;
    }

    return make_unit();
}

auto TypeChecker::check_method_call(const parser::MethodCallExpr& call) -> TypePtr {
    // Check for static method calls on primitive type names (e.g., I32::default())
    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& type_name = call.receiver->as<parser::IdentExpr>().name;
        // Check if this is a primitive type name used as a static receiver
        bool is_primitive_type = type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                                 type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                                 type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                                 type_name == "U128" || type_name == "F32" || type_name == "F64" ||
                                 type_name == "Bool" || type_name == "Str";

        if (is_primitive_type && call.method == "default") {
            // Return the primitive type itself
            if (type_name == "I8")
                return make_primitive(PrimitiveKind::I8);
            if (type_name == "I16")
                return make_primitive(PrimitiveKind::I16);
            if (type_name == "I32")
                return make_primitive(PrimitiveKind::I32);
            if (type_name == "I64")
                return make_primitive(PrimitiveKind::I64);
            if (type_name == "I128")
                return make_primitive(PrimitiveKind::I128);
            if (type_name == "U8")
                return make_primitive(PrimitiveKind::U8);
            if (type_name == "U16")
                return make_primitive(PrimitiveKind::U16);
            if (type_name == "U32")
                return make_primitive(PrimitiveKind::U32);
            if (type_name == "U64")
                return make_primitive(PrimitiveKind::U64);
            if (type_name == "U128")
                return make_primitive(PrimitiveKind::U128);
            if (type_name == "F32")
                return make_primitive(PrimitiveKind::F32);
            if (type_name == "F64")
                return make_primitive(PrimitiveKind::F64);
            if (type_name == "Bool")
                return make_primitive(PrimitiveKind::Bool);
            if (type_name == "Str")
                return make_primitive(PrimitiveKind::Str);
        }
    }

    auto receiver_type = check_expr(*call.receiver);

    // Helper lambda to apply type arguments to a function signature
    auto apply_type_args = [&](const FuncSig& func) -> TypePtr {
        if (!call.type_args.empty() && !func.type_params.empty()) {
            // Build substitution map from explicit type arguments
            // Need to resolve parser types to semantic types
            std::unordered_map<std::string, TypePtr> subs;
            for (size_t i = 0; i < func.type_params.size() && i < call.type_args.size(); ++i) {
                subs[func.type_params[i]] = resolve_type(*call.type_args[i]);
            }
            return substitute_type(func.return_type, subs);
        }
        return func.return_type;
    };

    if (receiver_type->is<NamedType>()) {
        auto& named = receiver_type->as<NamedType>();
        std::string qualified = named.name + "::" + call.method;

        auto func = env_.lookup_func(qualified);
        if (func) {
            // For generic impl methods (impl[T] Container[T]), substitute type parameters
            // using the receiver's type arguments if no explicit type args are provided
            if (call.type_args.empty() && !func->type_params.empty() && !named.type_args.empty()) {
                std::unordered_map<std::string, TypePtr> subs;
                for (size_t i = 0; i < func->type_params.size() && i < named.type_args.size();
                     ++i) {
                    subs[func->type_params[i]] = named.type_args[i];
                }
                return substitute_type(func->return_type, subs);
            }
            return apply_type_args(*func);
        }

        if (!named.module_path.empty()) {
            auto module = env_.get_module(named.module_path);
            if (module) {
                auto func_it = module->functions.find(qualified);
                if (func_it != module->functions.end()) {
                    return apply_type_args(func_it->second);
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
                    return apply_type_args(func_it->second);
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
                    // Build substitution map from behavior's type params to dyn's type args
                    // e.g., for dyn Processor[I32], map T -> I32
                    if (!dyn.type_args.empty() && !behavior_def->type_params.empty()) {
                        std::unordered_map<std::string, TypePtr> subs;
                        for (size_t i = 0;
                             i < behavior_def->type_params.size() && i < dyn.type_args.size();
                             ++i) {
                            subs[behavior_def->type_params[i]] = dyn.type_args[i];
                        }
                        // Substitute both return type and check parameter types
                        auto return_type = substitute_type(method.return_type, subs);
                        return return_type;
                    }
                    return apply_type_args(method);
                }
            }
            error("Unknown method '" + call.method + "' on behavior '" + dyn.behavior_name + "'",
                  call.receiver->span);
        }
    }

    // Handle primitive type builtin methods (core::ops)
    // Unwrap reference type if present
    TypePtr prim_type = receiver_type;
    if (receiver_type->is<RefType>()) {
        prim_type = receiver_type->as<RefType>().inner;
    }
    if (prim_type->is<PrimitiveType>()) {
        auto& prim = prim_type->as<PrimitiveType>();
        auto kind = prim.kind;

        // Integer and float arithmetic methods
        bool is_numeric = (kind == PrimitiveKind::I8 || kind == PrimitiveKind::I16 ||
                           kind == PrimitiveKind::I32 || kind == PrimitiveKind::I64 ||
                           kind == PrimitiveKind::I128 || kind == PrimitiveKind::U8 ||
                           kind == PrimitiveKind::U16 || kind == PrimitiveKind::U32 ||
                           kind == PrimitiveKind::U64 || kind == PrimitiveKind::U128 ||
                           kind == PrimitiveKind::F32 || kind == PrimitiveKind::F64);
        bool is_integer = (kind == PrimitiveKind::I8 || kind == PrimitiveKind::I16 ||
                           kind == PrimitiveKind::I32 || kind == PrimitiveKind::I64 ||
                           kind == PrimitiveKind::I128 || kind == PrimitiveKind::U8 ||
                           kind == PrimitiveKind::U16 || kind == PrimitiveKind::U32 ||
                           kind == PrimitiveKind::U64 || kind == PrimitiveKind::U128);

        // Arithmetic operations that return Self
        if (is_numeric && (call.method == "add" || call.method == "sub" || call.method == "mul" ||
                           call.method == "div" || call.method == "neg")) {
            return receiver_type;
        }

        // Integer-only operations
        if (is_integer && call.method == "rem") {
            return receiver_type;
        }

        // Bool methods
        if (kind == PrimitiveKind::Bool && call.method == "negate") {
            return receiver_type;
        }

        // Comparison methods - cmp returns Ordering, max/min return Self
        if (is_numeric) {
            if (call.method == "cmp") {
                return std::make_shared<Type>(Type{NamedType{"Ordering", "", {}}});
            }
            if (call.method == "max" || call.method == "min") {
                return receiver_type;
            }
        }

        // duplicate() returns Self for all primitives (copy semantics)
        if (call.method == "duplicate") {
            return receiver_type;
        }

        // to_string() returns Str for all primitives (Display behavior)
        if (call.method == "to_string") {
            return make_primitive(PrimitiveKind::Str);
        }

        // hash() returns I64 for all primitives (Hash behavior)
        if (call.method == "hash") {
            return make_primitive(PrimitiveKind::I64);
        }

        // to_owned() returns Self for all primitives (ToOwned behavior)
        if (call.method == "to_owned") {
            return receiver_type;
        }

        // borrow() returns ref Self for all primitives (Borrow behavior)
        if (call.method == "borrow") {
            return std::make_shared<Type>(RefType{false, receiver_type});
        }

        // borrow_mut() returns mut ref Self for all primitives (BorrowMut behavior)
        if (call.method == "borrow_mut") {
            return std::make_shared<Type>(RefType{true, receiver_type});
        }

        // Str-specific methods
        if (kind == PrimitiveKind::Str) {
            // len() returns I64 (byte length)
            if (call.method == "len") {
                return make_primitive(PrimitiveKind::I64);
            }
            // is_empty() returns Bool
            if (call.method == "is_empty") {
                return make_primitive(PrimitiveKind::Bool);
            }
            // as_bytes() returns ref [U8]
            if (call.method == "as_bytes") {
                auto u8_type = make_primitive(PrimitiveKind::U8);
                auto slice_type = std::make_shared<Type>(SliceType{u8_type});
                return std::make_shared<Type>(RefType{false, slice_type});
            }
        }

        // Try to look up user-defined impl methods for primitive types (e.g., I32::abs)
        std::string type_name = primitive_to_string(kind);
        std::string qualified = type_name + "::" + call.method;
        auto func = env_.lookup_func(qualified);
        if (func) {
            return func->return_type;
        }
    }

    // Handle Ordering enum methods
    if (receiver_type->is<NamedType>()) {
        auto& named = receiver_type->as<NamedType>();
        if (named.name == "Ordering") {
            // is_less, is_equal, is_greater return Bool
            if (call.method == "is_less" || call.method == "is_equal" ||
                call.method == "is_greater") {
                return make_primitive(PrimitiveKind::Bool);
            }
            // reverse, then_cmp return Ordering
            if (call.method == "reverse" || call.method == "then_cmp") {
                return receiver_type;
            }
            // to_string, debug_string return Str
            if (call.method == "to_string" || call.method == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }
        }

        // Handle Maybe[T] methods
        if (named.name == "Maybe" && !named.type_args.empty()) {
            TypePtr inner_type = named.type_args[0];

            // is_just(), is_nothing() return Bool
            if (call.method == "is_just" || call.method == "is_nothing") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // unwrap(), expect(msg) return T
            if (call.method == "unwrap" || call.method == "expect") {
                return inner_type;
            }

            // unwrap_or(default), unwrap_or_else(f), unwrap_or_default() return T
            if (call.method == "unwrap_or" || call.method == "unwrap_or_else" ||
                call.method == "unwrap_or_default") {
                return inner_type;
            }

            // map(f) returns Maybe[U] (same structure)
            if (call.method == "map") {
                return receiver_type;
            }

            // and_then(f) returns Maybe[U]
            if (call.method == "and_then") {
                return receiver_type;
            }

            // or_else(f) returns Maybe[T]
            if (call.method == "or_else") {
                return receiver_type;
            }

            // contains(value) returns Bool
            if (call.method == "contains") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // filter(predicate) returns Maybe[T]
            if (call.method == "filter") {
                return receiver_type;
            }

            // alt(other) returns Maybe[T]
            if (call.method == "alt") {
                return receiver_type;
            }

            // xor(other) returns Maybe[T]
            if (call.method == "xor") {
                return receiver_type;
            }

            // also(other) returns Maybe[U] - returns the other Maybe type
            if (call.method == "also") {
                if (!call.args.empty()) {
                    return check_expr(*call.args[0]);
                }
                return receiver_type;
            }

            // map_or(default, f) returns U
            if (call.method == "map_or") {
                if (call.args.size() >= 1) {
                    return check_expr(*call.args[0]); // Type of default
                }
                return inner_type;
            }

            // ok_or(err) returns Outcome[T, E]
            if (call.method == "ok_or") {
                if (call.args.size() >= 1) {
                    TypePtr err_type = check_expr(*call.args[0]);
                    std::vector<TypePtr> type_args = {inner_type, err_type};
                    return std::make_shared<Type>(NamedType{"Outcome", "", std::move(type_args)});
                }
                return receiver_type;
            }

            // ok_or_else(f) returns Outcome[T, E]
            if (call.method == "ok_or_else") {
                // For now, return a generic Outcome type
                // The actual error type comes from the closure
                return receiver_type; // Simplified - would need proper inference
            }
        }

        // Handle Outcome[T, E] methods
        if (named.name == "Outcome" && named.type_args.size() >= 2) {
            TypePtr ok_type = named.type_args[0];
            TypePtr err_type = named.type_args[1];

            // is_ok(), is_err() return Bool
            if (call.method == "is_ok" || call.method == "is_err") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // is_ok_and(predicate), is_err_and(predicate) return Bool
            if (call.method == "is_ok_and" || call.method == "is_err_and") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // unwrap() returns T
            if (call.method == "unwrap" || call.method == "expect") {
                return ok_type;
            }

            // unwrap_err() returns E
            if (call.method == "unwrap_err" || call.method == "expect_err") {
                return err_type;
            }

            // unwrap_or(default), unwrap_or_else(f), unwrap_or_default() return T
            if (call.method == "unwrap_or" || call.method == "unwrap_or_else" ||
                call.method == "unwrap_or_default") {
                return ok_type;
            }

            // map(f) returns Outcome[U, E] - same structure, potentially different T
            if (call.method == "map") {
                return receiver_type; // Same Outcome type structure
            }

            // map_err(f) returns Outcome[T, F] - same structure, potentially different E
            if (call.method == "map_err") {
                return receiver_type;
            }

            // map_or(default, f) returns U (the default/mapped type)
            if (call.method == "map_or") {
                if (call.args.size() >= 1) {
                    return check_expr(*call.args[0]); // Type of default
                }
                return ok_type;
            }

            // map_or_else(default_f, map_f) returns U
            if (call.method == "map_or_else") {
                return ok_type; // Simplified - returns same type as ok
            }

            // and_then(f) returns Outcome[U, E]
            if (call.method == "and_then") {
                return receiver_type;
            }

            // or_else(f) returns Outcome[T, F]
            if (call.method == "or_else") {
                return receiver_type;
            }

            // alt(other) returns Outcome[T, E]
            if (call.method == "alt") {
                return receiver_type;
            }

            // also(other) returns Outcome[U, E]
            if (call.method == "also") {
                if (!call.args.empty()) {
                    return check_expr(*call.args[0]);
                }
                return receiver_type;
            }

            // ok() returns Maybe[T]
            if (call.method == "ok") {
                std::vector<TypePtr> type_args = {ok_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // err() returns Maybe[E]
            if (call.method == "err") {
                std::vector<TypePtr> type_args = {err_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // contains(ref T), contains_err(ref E) return Bool
            if (call.method == "contains" || call.method == "contains_err") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // flatten() for Outcome[Outcome[T, E], E] returns Outcome[T, E]
            if (call.method == "flatten") {
                if (ok_type->is<NamedType>()) {
                    auto& inner_named = ok_type->as<NamedType>();
                    if (inner_named.name == "Outcome" && !inner_named.type_args.empty()) {
                        return ok_type; // Return the inner Outcome type
                    }
                }
                return receiver_type;
            }

            // iter() returns OutcomeIter[T]
            if (call.method == "iter") {
                std::vector<TypePtr> type_args = {ok_type};
                return std::make_shared<Type>(NamedType{"OutcomeIter", "", type_args});
            }
        }
    }

    // Handle ArrayType methods (e.g., [I32; 3].len(), [I32; 3].get(0), etc.)
    if (receiver_type->is<ArrayType>()) {
        auto& arr = receiver_type->as<ArrayType>();
        TypePtr elem_type = arr.element;
        (void)arr.size; // Size used for array methods like map that preserve size

        // len() returns I64
        if (call.method == "len") {
            return make_primitive(PrimitiveKind::I64);
        }

        // is_empty() returns Bool
        if (call.method == "is_empty") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // get(index) returns Maybe[ref T]
        if (call.method == "get") {
            auto ref_type = std::make_shared<Type>(RefType{false, elem_type});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // first(), last() return Maybe[ref T]
        if (call.method == "first" || call.method == "last") {
            auto ref_type = std::make_shared<Type>(RefType{false, elem_type});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // map(f) returns [U; N] where U is inferred from the closure
        if (call.method == "map") {
            // For now, return the same array type (simplified)
            // The actual mapped type would require closure inference
            return receiver_type;
        }

        // eq(other) and ne(other) return Bool
        if (call.method == "eq" || call.method == "ne") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // cmp(other) returns Ordering
        if (call.method == "cmp") {
            return std::make_shared<Type>(NamedType{"Ordering", "", {}});
        }

        // as_slice() returns Slice[T]
        if (call.method == "as_slice") {
            return std::make_shared<Type>(SliceType{elem_type});
        }

        // as_mut_slice() returns MutSlice[T]
        if (call.method == "as_mut_slice") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"MutSlice", "", type_args});
        }

        // iter() returns ArrayIter[T, N]
        if (call.method == "iter" || call.method == "into_iter") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"ArrayIter", "", type_args});
        }

        // duplicate() returns [T; N] (same type)
        if (call.method == "duplicate") {
            return receiver_type;
        }

        // hash() returns I64
        if (call.method == "hash") {
            return make_primitive(PrimitiveKind::I64);
        }

        // to_string() returns Str
        if (call.method == "to_string" || call.method == "debug_string") {
            return make_primitive(PrimitiveKind::Str);
        }
    }

    // Handle SliceType methods (e.g., [T].len(), [T].get(0), etc.)
    if (receiver_type->is<SliceType>()) {
        auto& slice = receiver_type->as<SliceType>();
        TypePtr elem_type = slice.element;

        // len() returns I64
        if (call.method == "len") {
            return make_primitive(PrimitiveKind::I64);
        }

        // is_empty() returns Bool
        if (call.method == "is_empty") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // get(index) returns Maybe[ref T]
        if (call.method == "get") {
            auto ref_type = std::make_shared<Type>(RefType{false, elem_type});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // first(), last() return Maybe[ref T]
        if (call.method == "first" || call.method == "last") {
            auto ref_type = std::make_shared<Type>(RefType{false, elem_type});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // slice(start, end) returns Slice[T]
        if (call.method == "slice") {
            return receiver_type;
        }

        // iter() returns SliceIter[T]
        if (call.method == "iter" || call.method == "into_iter") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"SliceIter", "", type_args});
        }

        // push() returns unit (for dynamic slices)
        if (call.method == "push") {
            return make_unit();
        }

        // pop() returns Maybe[T]
        if (call.method == "pop") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // to_string(), debug_string() return Str
        if (call.method == "to_string" || call.method == "debug_string") {
            return make_primitive(PrimitiveKind::Str);
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

    // Resolve the type in case it's a type alias
    auto resolved = env_.resolve(obj_type);

    if (resolved->is<ArrayType>()) {
        return resolved->as<ArrayType>().element;
    }
    if (resolved->is<SliceType>()) {
        return resolved->as<SliceType>().element;
    }

    return make_unit();
}

auto TypeChecker::check_block(const parser::BlockExpr& block) -> TypePtr {
    TML_DEBUG_LN("[check_block] Entering block with " << block.stmts.size() << " statements");
    env_.push_scope();
    TypePtr result = make_unit();

    for (const auto& stmt : block.stmts) {
        TML_DEBUG_LN("[check_block] Checking statement at index " << stmt->kind.index());
        result = check_stmt(*stmt);
    }

    if (block.expr) {
        TML_DEBUG_LN("[check_block] Checking trailing expression");
        result = check_expr(**block.expr);
    }

    env_.pop_scope();
    TML_DEBUG_LN("[check_block] Exiting block");
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

auto TypeChecker::check_cast(const parser::CastExpr& cast) -> TypePtr {
    // Check the source expression
    auto source_type = check_expr(*cast.expr);
    (void)source_type; // We allow any source type for now

    // Resolve the target type
    auto target_type = resolve_type(*cast.target);

    // Return the target type - the actual cast is handled by codegen
    return target_type;
}

auto TypeChecker::check_await(const parser::AwaitExpr& await_expr, SourceSpan span) -> TypePtr {
    // Check that we're in an async function
    if (!in_async_func_) {
        error("Cannot use `.await` outside of an async function", span);
        return make_unit();
    }

    // Type-check the awaited expression
    auto expr_type = check_expr(*await_expr.expr);

    // The awaited expression should return a Future[T] - extract the Output type
    // For simplicity, we check if it's a named type that implements Future
    // or if the expression is from an async function call (which implicitly returns Future[T])

    // Case 1: Named type that might be a Future
    if (expr_type->is<NamedType>()) {
        auto& named = expr_type->as<NamedType>();

        // Check if this type implements Future behavior
        if (env_.type_implements(named.name, "Future")) {
            // Look up the behavior impl to get the Output associated type
            // For now, if the type has type_args, assume the first is the Output
            if (!named.type_args.empty()) {
                return named.type_args[0];
            }
        }

        // Special case: Poll[T] - awaiting Poll returns T when Ready
        if (named.name == "Poll" && !named.type_args.empty()) {
            return named.type_args[0];
        }
    }

    // Case 2: Function type with is_async = true
    // Async functions return Future[ReturnType], so .await extracts ReturnType
    if (expr_type->is<FuncType>()) {
        auto& func = expr_type->as<FuncType>();
        if (func.is_async) {
            return func.return_type;
        }
    }

    // Case 3: impl Behavior type (ImplBehaviorType)
    if (expr_type->is<ImplBehaviorType>()) {
        auto& impl_behavior = expr_type->as<ImplBehaviorType>();
        if (impl_behavior.behavior_name == "Future") {
            // Return the Output type if available
            if (!impl_behavior.type_args.empty()) {
                return impl_behavior.type_args[0];
            }
        }
    }

    // For now, return the type itself if we can't determine the Future output
    // This allows async code to type-check even without full Future inference
    return expr_type;
}

auto TypeChecker::check_lowlevel(const parser::LowlevelExpr& lowlevel) -> TypePtr {
    // Save previous lowlevel state
    bool was_in_lowlevel = in_lowlevel_;
    in_lowlevel_ = true;

    env_.push_scope();
    TypePtr result = make_unit();

    // Check statements in lowlevel block
    for (const auto& stmt : lowlevel.stmts) {
        result = check_stmt(*stmt);
    }

    // Check trailing expression if present
    if (lowlevel.expr) {
        result = check_expr(**lowlevel.expr);
    }

    env_.pop_scope();
    in_lowlevel_ = was_in_lowlevel;

    return result;
}

} // namespace tml::types
