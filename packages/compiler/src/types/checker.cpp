#include "tml/types/checker.hpp"
#include "tml/lexer/token.hpp"
#include <algorithm>
#include <iostream>

namespace tml::types {

namespace {
// Helper to check if a type is an integer type
bool is_integer_type(const TypePtr& type) {
    if (!type->is<PrimitiveType>()) return false;
    auto kind = type->as<PrimitiveType>().kind;
    return kind == PrimitiveKind::I8 || kind == PrimitiveKind::I16 ||
           kind == PrimitiveKind::I32 || kind == PrimitiveKind::I64 ||
           kind == PrimitiveKind::I128 ||
           kind == PrimitiveKind::U8 || kind == PrimitiveKind::U16 ||
           kind == PrimitiveKind::U32 || kind == PrimitiveKind::U64 ||
           kind == PrimitiveKind::U128;
}

// Helper to check if a type is a float type
bool is_float_type(const TypePtr& type) {
    if (!type->is<PrimitiveType>()) return false;
    auto kind = type->as<PrimitiveType>().kind;
    return kind == PrimitiveKind::F32 || kind == PrimitiveKind::F64;
}

// Check if types are compatible (allowing numeric coercion)
bool types_compatible(const TypePtr& expected, const TypePtr& actual) {
    if (types_equal(expected, actual)) return true;

    // Allow integer literal (I64) to be assigned to any integer type
    if (is_integer_type(expected) && is_integer_type(actual)) return true;

    // Allow float literal (F64) to be assigned to any float type
    if (is_float_type(expected) && is_float_type(actual)) return true;

    // Allow array [T; N] to be assigned to slice [T]
    if (expected->is<SliceType>() && actual->is<ArrayType>()) {
        const auto& slice_elem = expected->as<SliceType>().element;
        const auto& array_elem = actual->as<ArrayType>().element;
        return types_compatible(slice_elem, array_elem);
    }

    // Allow closure to be assigned to function type if signatures match
    if (expected->is<FuncType>() && actual->is<ClosureType>()) {
        const auto& func = expected->as<FuncType>();
        const auto& closure = actual->as<ClosureType>();

        if (func.params.size() != closure.params.size()) return false;
        for (size_t i = 0; i < func.params.size(); ++i) {
            if (!types_equal(func.params[i], closure.params[i])) return false;
        }
        return types_equal(func.return_type, closure.return_type);
    }

    return false;
}
} // anonymous namespace

TypeChecker::TypeChecker() = default;

auto TypeChecker::check_module(const parser::Module& module)
    -> Result<TypeEnv, std::vector<TypeError>> {
    for (const auto& decl : module.decls) {
        if (decl->is<parser::UseDecl>()) {
            process_use_decl(decl->as<parser::UseDecl>());
        }
    }
    // First pass: register all type declarations
    for (const auto& decl : module.decls) {
        if (decl->is<parser::StructDecl>()) {
            register_struct_decl(decl->as<parser::StructDecl>());
        } else if (decl->is<parser::EnumDecl>()) {
            register_enum_decl(decl->as<parser::EnumDecl>());
        } else if (decl->is<parser::TraitDecl>()) {
            register_trait_decl(decl->as<parser::TraitDecl>());
        } else if (decl->is<parser::TypeAliasDecl>()) {
            register_type_alias(decl->as<parser::TypeAliasDecl>());
        }
    }

    // Second pass: register function signatures and constants
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            check_func_decl(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            check_impl_decl(decl->as<parser::ImplDecl>());
        } else if (decl->is<parser::ConstDecl>()) {
            check_const_decl(decl->as<parser::ConstDecl>());
        }
    }

    // Third pass: check function bodies
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            check_func_body(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            check_impl_body(decl->as<parser::ImplDecl>());
        }
    }

    if (has_errors()) {
        return errors_;
    }
    return env_;
}

void TypeChecker::register_struct_decl(const parser::StructDecl& decl) {
    std::vector<std::pair<std::string, TypePtr>> fields;
    for (const auto& field : decl.fields) {
        fields.emplace_back(field.name, resolve_type(*field.type));
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        type_params.push_back(param.name);
    }

    env_.define_struct(StructDef{
        .name = decl.name,
        .type_params = std::move(type_params),
        .fields = std::move(fields),
        .span = decl.span
    });
}

void TypeChecker::register_enum_decl(const parser::EnumDecl& decl) {
    std::vector<std::pair<std::string, std::vector<TypePtr>>> variants;
    for (const auto& variant : decl.variants) {
        std::vector<TypePtr> types;
        if (variant.tuple_fields) {
            for (const auto& type : *variant.tuple_fields) {
                types.push_back(resolve_type(*type));
            }
        }
        variants.emplace_back(variant.name, std::move(types));
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        type_params.push_back(param.name);
    }

    env_.define_enum(EnumDef{
        .name = decl.name,
        .type_params = std::move(type_params),
        .variants = std::move(variants),
        .span = decl.span
    });
}

void TypeChecker::register_trait_decl(const parser::TraitDecl& decl) {
    std::vector<FuncSig> methods;
    for (const auto& method : decl.methods) {
        std::vector<TypePtr> params;
        for (const auto& p : method.params) {
            params.push_back(resolve_type(*p.type));
        }
        TypePtr ret = method.return_type ? resolve_type(**method.return_type) : make_unit();
        methods.push_back(FuncSig{
            .name = method.name,
            .params = std::move(params),
            .return_type = std::move(ret),
            .type_params = {},
            .is_async = method.is_async,
            .span = method.span
        });
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        type_params.push_back(param.name);
    }

    env_.define_behavior(BehaviorDef{
        .name = decl.name,
        .type_params = std::move(type_params),
        .methods = std::move(methods),
        .super_behaviors = {},
        .span = decl.span
    });
}

void TypeChecker::register_type_alias(const parser::TypeAliasDecl& decl) {
    env_.define_type_alias(decl.name, resolve_type(*decl.type));
}

void TypeChecker::process_use_decl(const parser::UseDecl& use_decl) {
    if (use_decl.path.segments.empty()) {
        return;
    }

    // Build module path from segments
    std::string module_path;
    for (size_t i = 0; i < use_decl.path.segments.size(); ++i) {
        if (i > 0) module_path += "::";
        module_path += use_decl.path.segments[i];
    }

    // Handle grouped imports: use std::math::{abs, sqrt, pow}
    if (use_decl.symbols.has_value()) {
        const auto& symbols = use_decl.symbols.value();

        // Load the module
        env_.load_native_module(module_path);
        auto module_opt = env_.get_module(module_path);

        if (!module_opt.has_value()) {
            errors_.push_back(TypeError{
                "Module '" + module_path + "' not found",
                use_decl.span,
                {}
            });
            return;
        }

        // Import each symbol individually
        for (const auto& symbol : symbols) {
            env_.import_symbol(module_path, symbol, std::nullopt);
        }
        return;
    }

    // Try first as complete module path
    env_.load_native_module(module_path);
    auto module_opt = env_.get_module(module_path);

    // If module not found, last segment might be a symbol name
    if (!module_opt.has_value() && use_decl.path.segments.size() > 1) {
        // Try module path without last segment
        std::string base_module_path;
        for (size_t i = 0; i < use_decl.path.segments.size() - 1; ++i) {
            if (i > 0) base_module_path += "::";
            base_module_path += use_decl.path.segments[i];
        }

        env_.load_native_module(base_module_path);
        module_opt = env_.get_module(base_module_path);

        if (module_opt.has_value()) {
            // Last segment is a symbol name - import only that symbol
            std::string symbol_name = use_decl.path.segments.back();
            env_.import_symbol(base_module_path, symbol_name, use_decl.alias);
            return;
        }
    }

    if (!module_opt.has_value()) {
        errors_.push_back(TypeError{
            "Module '" + module_path + "' not found",
            use_decl.span,
            {}
        });
        return;
    }

    // Import all from module
    env_.import_all_from(module_path);
}

void TypeChecker::check_func_decl(const parser::FuncDecl& func) {
    for (const auto& p : func.params) {
        params.push_back(resolve_type(*p.type));
    }
    TypePtr ret = func.return_type ? resolve_type(**func.return_type) : make_unit();

    // Process where clause constraints
    std::vector<WhereConstraint> where_constraints;
    if (func.where_clause) {
        for (const auto& [type_ptr, behaviors] : func.where_clause->constraints) {
            // Extract type parameter name from type
            std::string type_param_name;
            if (type_ptr->is<parser::NamedType>()) {
                const auto& named = type_ptr->as<parser::NamedType>();
                if (!named.path.segments.empty()) {
                    type_param_name = named.path.segments[0];
                }
            }

            // Extract behavior names
            std::vector<std::string> behavior_names;
            for (const auto& behavior_path : behaviors) {
                if (!behavior_path.segments.empty()) {
                    behavior_names.push_back(behavior_path.segments.back());
                }
            }

            if (!type_param_name.empty() && !behavior_names.empty()) {
                where_constraints.push_back(WhereConstraint{
                    type_param_name,
                    behavior_names
                });
            }
        }
    }

    // Extract generic type parameter names
    std::vector<std::string> func_type_params;
    for (const auto& param : func.generics) {
        func_type_params.push_back(param.name);
    }

    std::cerr << std::endl;

    env_.define_func(FuncSig{
        .name = func.name,
        .params = std::move(params),
        .return_type = std::move(ret),
        .type_params = std::move(func_type_params),
        .is_async = func.is_async,
        .span = func.span,
        .stability = StabilityLevel::Unstable,
        .deprecated_message = "",
        .since_version = "",
        .where_constraints = std::move(where_constraints)
    });
}

void TypeChecker::check_func_body(const parser::FuncDecl& func) {

    // Add parameters to scope
    for (const auto& p : func.params) {
        if (p.pattern->is<parser::IdentPattern>()) {
            auto& ident = p.pattern->as<parser::IdentPattern>();
            env_.current_scope()->define(
                ident.name,
                resolve_type(*p.type),
                ident.is_mut,
                p.pattern->span
            );
        }
    }

    if (func.body) {
        auto body_type = check_block(*func.body);

        // Check if function with explicit non-Unit return type has return statement
        if (func.return_type) {
            auto return_type = resolve_type(**func.return_type);
            // Only require return if return type is not Unit
            if (!return_type->is<PrimitiveType>() ||
                return_type->as<PrimitiveType>().kind != PrimitiveKind::Unit) {


                if (!has_ret) {
                    error("Function '" + func.name + "' with return type " +
                          type_to_string(return_type) + " must have an explicit return statement",
                          func.span);
                }
            }
        }

        // Check return type compatibility (simplified for now)
        (void)body_type;
    }

    env_.pop_scope();
    current_return_type_ = nullptr;
}

void TypeChecker::check_const_decl(const parser::ConstDecl& const_decl) {
    // Resolve the declared type
    TypePtr declared_type = resolve_type(*const_decl.type);

    // Type-check the initializer expression
    TypePtr init_type = check_expr(*const_decl.value);

    // Verify the types match
    if (!types_equal(init_type, declared_type)) {
        error("Type mismatch in const initializer: expected " +
              type_to_string(declared_type) + ", found " +
              type_to_string(init_type),
              const_decl.value->span);
        return;
    }

    // Define the const in the global scope (as a variable that's immutable)
    env_.current_scope()->define(const_decl.name, declared_type, false, const_decl.span);
}

void TypeChecker::check_impl_decl(const parser::ImplDecl& impl) {
    // Get the type name from self_type
    std::string type_name = type_to_string(resolve_type(*impl.self_type));

    // Register all methods in the impl block
    for (const auto& method : impl.methods) {
        std::string qualified_name = type_name + "::" + method.name;
        std::vector<TypePtr> params;
        for (const auto& p : method.params) {
            params.push_back(resolve_type(*p.type));
        }
        TypePtr ret = method.return_type ? resolve_type(**method.return_type) : make_unit();
    std::cerr << std::endl;

    env_.define_func(FuncSig{
            .name = qualified_name,
            .params = std::move(params),
            .return_type = std::move(ret),
        .type_params = {},
            .is_async = method.is_async,
            .span = method.span
        });
    }
}

void TypeChecker::check_impl_body(const parser::ImplDecl& impl) {
    for (const auto& method : impl.methods) {
        check_func_body(method);
    }
}

auto TypeChecker::check_expr(const parser::Expr& expr) -> TypePtr {
    return std::visit([this, &expr](const auto& e) -> TypePtr {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
            return check_literal(e);
        }
        else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
            return check_ident(e, expr.span);
        }
        else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
            return check_binary(e);
        }
        else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
            return check_unary(e);
        }
        else if constexpr (std::is_same_v<T, parser::CallExpr>) {
            return check_call(e);
        }
        else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
            return check_method_call(e);
        }
        else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
            return check_field_access(e);
        }
        else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
            return check_index(e);
        }
        else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
            return check_block(e);
        }
        else if constexpr (std::is_same_v<T, parser::IfExpr>) {
            return check_if(e);
        }
        else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
            return check_ternary(e);
        }
        else if constexpr (std::is_same_v<T, parser::IfLetExpr>) {
            return check_if_let(e);
        }
        else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
            return check_when(e);
        }
        else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
            return check_loop(e);
        }
        else if constexpr (std::is_same_v<T, parser::ForExpr>) {
            return check_for(e);
        }
        else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
            return check_return(e);
        }
        else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
            return check_break(e);
        }
        else if constexpr (std::is_same_v<T, parser::ContinueExpr>) {
            return make_never();
        }
        else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
            return check_tuple(e);
        }
        else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
            return check_array(e);
        }
        else if constexpr (std::is_same_v<T, parser::StructExpr>) {
            return check_struct_expr(e);
        }
        else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
            return check_closure(e);
        }
        else if constexpr (std::is_same_v<T, parser::TryExpr>) {
            return check_try(e);
        }
        else if constexpr (std::is_same_v<T, parser::PathExpr>) {
            return check_path(e, expr.span);
        }
        else if constexpr (std::is_same_v<T, parser::RangeExpr>) {
            return check_range(e);
        }
        else {
            return make_unit();
        }
    }, expr.kind);
}

auto TypeChecker::check_literal(const parser::LiteralExpr& lit) -> TypePtr {
    // Determine type based on token kind
    switch (lit.token.kind) {
        case lexer::TokenKind::IntLiteral:
            return make_i64();  // Default to I64 for now
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

        // Check if it's an enum constructor (unit variant or will be called)
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (const auto& [variant_name, payload_types] : enum_def.variants) {
                if (variant_name == ident.name) {
                    // Found matching variant
                    if (payload_types.empty()) {
                        // Unit variant - return enum type directly
                        auto enum_type = std::make_shared<Type>();
                        enum_type->kind = NamedType{enum_name, "", {}};
                        return enum_type;
                    } else {
                        // Variant with payload - will be used as constructor function
                        // Return a function type for this constructor
                        auto enum_type = std::make_shared<Type>();
                        enum_type->kind = NamedType{enum_name, "", {}};
                        return make_func(payload_types, enum_type);
                    }
                }
            }
        }

        error("Undefined variable: " + ident.name, span);
        return make_unit();
    }
    return sym->type;
}

auto TypeChecker::check_binary(const parser::BinaryExpr& binary) -> TypePtr {
    auto left = check_expr(*binary.left);
    auto right = check_expr(*binary.right);

    // Helper to check if types are compatible for binary operations
    auto check_binary_types = [&](const char* op_name) {
        TypePtr resolved_left = env_.resolve(left);
        TypePtr resolved_right = env_.resolve(right);
        if (!types_compatible(resolved_left, resolved_right)) {
            error(std::string("Binary operator '") + op_name + "' requires matching types, found " +
                  type_to_string(resolved_left) + " and " + type_to_string(resolved_right),
                  binary.left->span);
        }
    };

    // Helper to check mutability for assignment
    auto check_assignable = [&]() {
        if (binary.left->is<parser::IdentExpr>()) {
            const auto& ident = binary.left->as<parser::IdentExpr>();
            auto sym = env_.current_scope()->lookup(ident.name);
            if (sym && !sym->is_mutable) {
                error("Cannot assign to immutable variable '" + ident.name + "'", binary.left->span);
            }
        }
    };

    switch (binary.op) {
        case parser::BinaryOp::Add:
            check_binary_types("+");
            return left;
        case parser::BinaryOp::Sub:
            check_binary_types("-");
            return left;
        case parser::BinaryOp::Mul:
            check_binary_types("*");
            return left;
        case parser::BinaryOp::Div:
            check_binary_types("/");
            return left;
        case parser::BinaryOp::Mod:
            check_binary_types("%");
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
            // Increment/decrement return the same type as operand
            return operand;
    }
    return make_unit();
}

auto TypeChecker::check_call(const parser::CallExpr& call) -> TypePtr {
    auto callee_type = check_expr(*call.callee);

    // Check if this is a polymorphic builtin (print/println accept any type)
    if (call.callee->is<parser::IdentExpr>()) {
        const auto& name = call.callee->as<parser::IdentExpr>().name;
        if (name == "print" || name == "println") {
            // print/println accept any single argument of any type
            // Just type-check the argument (without requiring a specific type)
            for (const auto& arg : call.args) {
                check_expr(*arg);  // Type-check but accept any type
            }
            return make_unit();  // print/println return Unit
        }
    }

    if (callee_type->is<FuncType>()) {
        auto& func = callee_type->as<FuncType>();
        // Check argument count
        if (call.args.size() != func.params.size()) {
            error("Wrong number of arguments", call.callee->span);
        }
        // Check argument types
        for (size_t i = 0; i < std::min(call.args.size(), func.params.size()); ++i) {
            auto arg_type = check_expr(*call.args[i]);
            (void)arg_type;  // Type checking would happen here
        }
        return func.return_type;
    }

    // Try to look up as a constructor or function
    if (call.callee->is<parser::IdentExpr>()) {
        auto& ident = call.callee->as<parser::IdentExpr>();

        // First try function lookup
        auto func = env_.lookup_func(ident.name);
        if (func) {
            // DEBUG: Print function info

            // Check if this is a generic function
            if (!func->type_params.empty()) {
                // Infer type arguments from call arguments
                std::unordered_map<std::string, TypePtr> substitutions;
                for (size_t i = 0; i < call.args.size() && i < func->params.size(); ++i) {
                    auto arg_type = check_expr(*call.args[i]);
                    // Check if the param type is a generic type
                    if (func->params[i]->is<GenericType>()) {
                        const auto& generic = func->params[i]->as<GenericType>();
                        substitutions[generic.name] = arg_type;
                    }
                    // Also check NamedType (resolve_type creates NamedType for unknown types like T)
                    else if (func->params[i]->is<NamedType>()) {
                        const auto& named = func->params[i]->as<NamedType>();
                        // Check if name matches a type parameter
                        for (const auto& tp : func->type_params) {
                            if (named.name == tp) {
                                substitutions[tp] = arg_type;
                                break;
                            }
                        }
                    }
                }
                // Substitute the return type
                return substitute_type(func->return_type, substitutions);
            }
            return func->return_type;
        }

        // Then try enum constructor lookup
        // Search all enums for a variant with this name
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (const auto& [variant_name, payload_types] : enum_def.variants) {
                if (variant_name == ident.name) {
                    // Found matching variant - verify argument count
                    if (call.args.size() != payload_types.size()) {
                        error("Enum variant '" + variant_name + "' expects " +
                              std::to_string(payload_types.size()) + " arguments, but got " +
                              std::to_string(call.args.size()), call.callee->span);
                        return make_unit();
                    }

                    // Type check arguments
                    for (size_t i = 0; i < call.args.size(); ++i) {
                        auto arg_type = check_expr(*call.args[i]);
                        (void)arg_type; // Would check against payload_types[i]
                    }

                    // Return the enum type
                    auto enum_type = std::make_shared<Type>();
                    enum_type->kind = NamedType{enum_name, "", {}};
                    return enum_type;
                }
            }
        }
    }

    return make_unit();
}

auto TypeChecker::check_method_call(const parser::MethodCallExpr& call) -> TypePtr {
    auto receiver_type = check_expr(*call.receiver);

    // Look up method on the receiver type
    if (receiver_type->is<NamedType>()) {
        auto& named = receiver_type->as<NamedType>();
        std::string qualified = named.name + "::" + call.method;
        auto func = env_.lookup_func(qualified);
        if (func) {
            return func->return_type;
        }
    }

    return make_unit();
}

auto TypeChecker::check_field_access(const parser::FieldExpr& field) -> TypePtr {
    auto obj_type = check_expr(*field.object);

    if (obj_type->is<NamedType>()) {
        auto& named = obj_type->as<NamedType>();
        auto struct_def = env_.lookup_struct(named.name);
        if (struct_def) {
            for (const auto& [fname, ftype] : struct_def->fields) {
                if (fname == field.field) {
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
        } catch (...) {
            // Not a numeric index
        }
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

auto TypeChecker::check_stmt(const parser::Stmt& stmt) -> TypePtr {
    return std::visit([this](const auto& s) -> TypePtr {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, parser::LetStmt>) {
            return check_let(s);
        }
        else if constexpr (std::is_same_v<T, parser::VarStmt>) {
            return check_var(s);
        }
        else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
            return check_expr(*s.expr);
        }
        else {
            return make_unit();
        }
    }, stmt.kind);
}

auto TypeChecker::check_let(const parser::LetStmt& let) -> TypePtr {
    // TML requires explicit type annotations on all let statements
    if (!let.type_annotation.has_value()) {
        error("TML requires explicit type annotation on 'let' statements. Add ': Type' after the variable name.", let.span);
        // Continue with unit type to allow further error checking
        bind_pattern(*let.pattern, make_unit());
        return make_unit();
    }

    TypePtr var_type = resolve_type(**let.type_annotation);

    if (let.init) {
        TypePtr init_type = check_expr(**let.init);
        // Check that init type is compatible with declared type
        TypePtr resolved_var = env_.resolve(var_type);
        TypePtr resolved_init = env_.resolve(init_type);
        if (!types_compatible(resolved_var, resolved_init)) {
            error("Type mismatch: expected " + type_to_string(resolved_var) +
                  ", found " + type_to_string(resolved_init), let.span);
        }
    }

    bind_pattern(*let.pattern, var_type);
    return make_unit();
}

auto TypeChecker::check_var(const parser::VarStmt& var) -> TypePtr {
    // TML requires explicit type annotations on all var statements
    if (!var.type_annotation.has_value()) {
        error("TML requires explicit type annotation on 'var' statements. Add ': Type' after the variable name.", var.span);
        // Continue with inferred type to allow further error checking
        TypePtr init_type = check_expr(*var.init);
        env_.current_scope()->define(var.name, init_type, true, SourceSpan{});
        return make_unit();
    }

    TypePtr var_type = resolve_type(**var.type_annotation);
    TypePtr init_type = check_expr(*var.init);

    // Type compatibility is checked during expression type checking
    env_.current_scope()->define(var.name, var_type, true, SourceSpan{});
    return make_unit();
}

void TypeChecker::bind_pattern(const parser::Pattern& pattern, TypePtr type) {
    std::visit([this, &type, &pattern](const auto& p) {
        using T = std::decay_t<decltype(p)>;

        if constexpr (std::is_same_v<T, parser::IdentPattern>) {
            // Check for duplicate definition in current scope
            auto existing = env_.current_scope()->lookup(p.name);
            if (existing) {
                error("Duplicate definition of variable '" + p.name + "'", pattern.span);
            }
            env_.current_scope()->define(p.name, type, p.is_mut, pattern.span);
        }
        else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
            if (type->is<TupleType>()) {
                auto& tuple = type->as<TupleType>();
                for (size_t i = 0; i < std::min(p.elements.size(), tuple.elements.size()); ++i) {
                    bind_pattern(*p.elements[i], tuple.elements[i]);
                }
            }
        }
        else if constexpr (std::is_same_v<T, parser::EnumPattern>) {
            // Extract enum name from type
            if (!type->is<NamedType>()) {
                error("Pattern expects enum type, but got different type", pattern.span);
                return;
            }

            auto& named = type->as<NamedType>();
            std::string enum_name = named.name;

            // Lookup enum definition
            auto enum_def = env_.lookup_enum(enum_name);
            if (!enum_def) {
                error("Unknown enum type '" + enum_name + "' in pattern", pattern.span);
                return;
            }

            // Find matching variant
            std::string variant_name = p.path.segments.back();
            auto variant_it = std::find_if(
                enum_def->variants.begin(),
                enum_def->variants.end(),
                [&variant_name](const auto& v) { return v.first == variant_name; }
            );

            if (variant_it == enum_def->variants.end()) {
                error("Unknown variant '" + variant_name + "' in enum '" + enum_name + "'", pattern.span);
                return;
            }

            auto& variant_payload_types = variant_it->second;

            // Bind payload patterns if present
            if (p.payload) {
                if (variant_payload_types.empty()) {
                    error("Variant '" + variant_name + "' has no payload, but pattern expects one", pattern.span);
                    return;
                }

                if (p.payload->size() != variant_payload_types.size()) {
                    error("Variant '" + variant_name + "' expects " +
                          std::to_string(variant_payload_types.size()) + " arguments, but pattern has " +
                          std::to_string(p.payload->size()), pattern.span);
                    return;
                }

                // Recursively bind each payload element
                for (size_t i = 0; i < p.payload->size(); ++i) {
                    bind_pattern(*(*p.payload)[i], variant_payload_types[i]);
                }
            } else if (!variant_payload_types.empty()) {
                error("Variant '" + variant_name + "' has payload, but pattern doesn't bind it", pattern.span);
                return;
            }
        }
    }, pattern.kind);
}

auto TypeChecker::check_if(const parser::IfExpr& if_expr) -> TypePtr {
    auto cond_type = check_expr(*if_expr.condition);
    if (!types_equal(env_.resolve(cond_type), make_bool())) {
        error("If condition must be Bool", if_expr.condition->span);
    }

    auto then_type = check_expr(*if_expr.then_branch);

    if (if_expr.else_branch) {
        check_expr(**if_expr.else_branch);
        return then_type;
    }

    return make_unit();
}

auto TypeChecker::check_ternary(const parser::TernaryExpr& ternary) -> TypePtr {
    // Check condition is Bool
    auto cond_type = check_expr(*ternary.condition);
    if (!types_equal(env_.resolve(cond_type), make_bool())) {
        error("Ternary condition must be Bool", ternary.condition->span);
    }

    // Check both branches and ensure they return the same type
    auto true_type = check_expr(*ternary.true_value);
    auto false_type = check_expr(*ternary.false_value);

    // Both branches must have the same type
    if (!types_equal(env_.resolve(true_type), env_.resolve(false_type))) {
        error("Ternary branches must have the same type", ternary.span);
    }

    return true_type;
}

auto TypeChecker::check_if_let(const parser::IfLetExpr& if_let) -> TypePtr {
    // Type check the scrutinee
    auto scrutinee_type = check_expr(*if_let.scrutinee);

    // Type check the then branch with pattern bindings in scope
    env_.push_scope();
    bind_pattern(*if_let.pattern, scrutinee_type);
    auto then_type = check_expr(*if_let.then_branch);
    env_.pop_scope();

    // Type check the else branch if present
    if (if_let.else_branch) {
        check_expr(**if_let.else_branch);
        return then_type;
    }

    return make_unit();
}

auto TypeChecker::check_when(const parser::WhenExpr& when) -> TypePtr {
    auto scrutinee_type = check_expr(*when.scrutinee);
    TypePtr result_type = nullptr;

    for (const auto& arm : when.arms) {
        env_.push_scope();
        bind_pattern(*arm.pattern, scrutinee_type);

        if (arm.guard) {
            check_expr(**arm.guard);
        }

        auto arm_type = check_expr(*arm.body);
        if (!result_type) {
            result_type = arm_type;
        }

        env_.pop_scope();
    }

    return result_type ? result_type : make_unit();
}

auto TypeChecker::check_loop(const parser::LoopExpr& loop) -> TypePtr {
    loop_depth_++;
    check_expr(*loop.body);
    loop_depth_--;
    return make_unit();
}

auto TypeChecker::check_for(const parser::ForExpr& for_expr) -> TypePtr {
    loop_depth_++;
    env_.push_scope();

    auto iter_type = check_expr(*for_expr.iter);

    // Extract element type from slice or collection for pattern binding
    TypePtr element_type = make_unit();
    if (iter_type->is<SliceType>()) {
        element_type = iter_type->as<SliceType>().element;
    } else if (iter_type->is<NamedType>()) {
        // Check if it's a collection type (List, HashMap, Buffer, Vec)
        const auto& named = iter_type->as<NamedType>();
        if (named.name == "List" || named.name == "Vec" || named.name == "Buffer") {
            // For List/Vec/Buffer, elements are I32 (stored as i64 but converted)
            element_type = make_primitive(PrimitiveKind::I32);
        } else if (named.name == "HashMap") {
            // For HashMap iteration, we get values (I32)
            element_type = make_primitive(PrimitiveKind::I32);
        } else if (iter_type->is<PrimitiveType>()) {
            // Allow iteration over integer ranges (for i in 0 to 10)
            element_type = iter_type;
        } else {
            error("For loop requires slice or collection type, found: " + type_to_string(iter_type), for_expr.span);
            element_type = make_unit();
        }
    } else if (iter_type->is<PrimitiveType>()) {
        // Allow iteration over integer ranges (for i in 0 to 10)
        element_type = iter_type;
    } else {
        error("For loop requires slice or collection type, found: " + type_to_string(iter_type), for_expr.span);
        element_type = make_unit();
    }

    bind_pattern(*for_expr.pattern, element_type);

    check_expr(*for_expr.body);

    env_.pop_scope();
    loop_depth_--;

    return make_unit();
}

auto TypeChecker::check_range(const parser::RangeExpr& range) -> TypePtr {
    // Check start expression (if present)
    TypePtr start_type = make_primitive(PrimitiveKind::I64);
    if (range.start) {
        start_type = check_expr(**range.start);
        if (!is_integer_type(start_type)) {
            error("Range start must be an integer type", range.span);
        }
    }

    // Check end expression (if present)
    TypePtr end_type = make_primitive(PrimitiveKind::I64);
    if (range.end) {
        end_type = check_expr(**range.end);
        if (!is_integer_type(end_type)) {
            error("Range end must be an integer type", range.span);
        }
    }

    // Both start and end should have compatible types
    // For simplicity, ranges always produce I64 slices
    // In a more sophisticated implementation, we could infer the element type
    return make_slice(make_primitive(PrimitiveKind::I64));
}

auto TypeChecker::check_return(const parser::ReturnExpr& ret) -> TypePtr {
    TypePtr value_type = make_unit();
    if (ret.value) {
        value_type = check_expr(**ret.value);
    }

    // Check return type matches function signature
    if (current_return_type_) {
        TypePtr resolved_expected = env_.resolve(current_return_type_);
        TypePtr resolved_actual = env_.resolve(value_type);
        if (!types_compatible(resolved_expected, resolved_actual)) {
            error("Return type mismatch: expected " + type_to_string(resolved_expected) +
                  ", found " + type_to_string(resolved_actual), SourceSpan{});
        }
    }

    return make_never();
}

auto TypeChecker::check_break(const parser::BreakExpr& brk) -> TypePtr {
    if (loop_depth_ == 0) {
        error("break outside of loop", SourceSpan{});
    }
    if (brk.value) {
        check_expr(**brk.value);
    }
    return make_never();
}

auto TypeChecker::check_tuple(const parser::TupleExpr& tuple) -> TypePtr {
    std::vector<TypePtr> element_types;
    for (const auto& elem : tuple.elements) {
        element_types.push_back(check_expr(*elem));
    }
    return make_tuple(std::move(element_types));
}

auto TypeChecker::check_array(const parser::ArrayExpr& array) -> TypePtr {
    return std::visit([this](const auto& arr) -> TypePtr {
        using T = std::decay_t<decltype(arr)>;

        if constexpr (std::is_same_v<T, std::vector<parser::ExprPtr>>) {
            // [1, 2, 3] form
            if (arr.empty()) {
                return make_array(env_.fresh_type_var(), 0);
            }
            auto first_type = check_expr(*arr[0]);
            for (size_t i = 1; i < arr.size(); ++i) {
                check_expr(*arr[i]);
            }
            return make_array(first_type, arr.size());
        } else {
            // [expr; count] form
            auto elem_type = check_expr(*arr.first);
            check_expr(*arr.second);  // The count expression
            return make_array(elem_type, 0);  // Size unknown at compile time
        }
    }, array.kind);
}

auto TypeChecker::check_struct_expr(const parser::StructExpr& struct_expr) -> TypePtr {
    std::string name = struct_expr.path.segments.empty() ? "" : struct_expr.path.segments.back();
    auto struct_def = env_.lookup_struct(name);

    if (!struct_def) {
        error("Unknown struct: " + name, SourceSpan{});
        return make_unit();
    }

    for (const auto& [field_name, field_expr] : struct_expr.fields) {
        check_expr(*field_expr);
    }

    auto type = std::make_shared<Type>();
    type->kind = NamedType{name, "", {}};
    return type;
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
    return check_expr(*try_expr.expr);
}

void TypeChecker::collect_captures_from_expr(const parser::Expr& expr,
                                               std::shared_ptr<Scope> closure_scope,
                                               std::shared_ptr<Scope> parent_scope,
                                               std::vector<CapturedVar>& captures) {
    std::visit([&](const auto& e) {
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
                        captures.push_back(CapturedVar{
                            e.name,
                            parent_sym->type,
                            parent_sym->is_mutable
                        });
                    }
                }
            }
        }
        else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
            collect_captures_from_expr(*e.left, closure_scope, parent_scope, captures);
            collect_captures_from_expr(*e.right, closure_scope, parent_scope, captures);
        }
        else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
            collect_captures_from_expr(*e.operand, closure_scope, parent_scope, captures);
        }
        else if constexpr (std::is_same_v<T, parser::CallExpr>) {
            collect_captures_from_expr(*e.callee, closure_scope, parent_scope, captures);
            for (const auto& arg : e.args) {
                collect_captures_from_expr(*arg, closure_scope, parent_scope, captures);
            }
        }
        else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
            for (const auto& stmt : e.stmts) {
                if (stmt->kind.index() == 3) { // ExprStmt
                    auto& expr_stmt = std::get<parser::ExprStmt>(stmt->kind);
                    collect_captures_from_expr(*expr_stmt.expr, closure_scope, parent_scope, captures);
                }
            }
            if (e.expr) {
                collect_captures_from_expr(**e.expr, closure_scope, parent_scope, captures);
            }
        }
        else if constexpr (std::is_same_v<T, parser::IfExpr>) {
            collect_captures_from_expr(*e.condition, closure_scope, parent_scope, captures);
            collect_captures_from_expr(*e.then_branch, closure_scope, parent_scope, captures);
            if (e.else_branch) {
                collect_captures_from_expr(**e.else_branch, closure_scope, parent_scope, captures);
            }
        }
        else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
            collect_captures_from_expr(*e.condition, closure_scope, parent_scope, captures);
            collect_captures_from_expr(*e.true_value, closure_scope, parent_scope, captures);
            collect_captures_from_expr(*e.false_value, closure_scope, parent_scope, captures);
        }
        else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
            if (e.value) {
                collect_captures_from_expr(**e.value, closure_scope, parent_scope, captures);
            }
        }
        // Add more cases as needed for other expression types
    }, expr.kind);
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
        error("Undefined: " + segments[0], span);
    }

    if (segments.size() == 2) {
        // Try function lookup with full path first (e.g., "Instant::now")
        std::string full_name = segments[0] + "::" + segments[1];
        auto func = env_.lookup_func(full_name);
        if (func) {
            return make_func(func->params, func->return_type);
        }

        // Then try enum variant lookup
        auto enum_def = env_.lookup_enum(segments[0]);
        if (enum_def) {
            for (const auto& variant_pair : enum_def->variants) {
                if (variant_pair.first == segments[1]) {
                    auto type = std::make_shared<Type>();
                    type->kind = NamedType{segments[0], "", {}};
                    return type;
                }
            }
        }
    }

    return make_unit();
}

auto TypeChecker::resolve_type(const parser::Type& type) -> TypePtr {
    return std::visit([this](const auto& t) -> TypePtr {
        using T = std::decay_t<decltype(t)>;
        if constexpr (std::is_same_v<T, parser::NamedType>) {
            return resolve_type_path(t.path);
        }
        else if constexpr (std::is_same_v<T, parser::RefType>) {
            return make_ref(resolve_type(*t.inner), t.is_mut);
        }
        else if constexpr (std::is_same_v<T, parser::PtrType>) {
            auto type_ptr = std::make_shared<Type>();
            type_ptr->kind = types::PtrType{t.is_mut, resolve_type(*t.inner)};
            return type_ptr;
        }
        else if constexpr (std::is_same_v<T, parser::ArrayType>) {
            // Size is an expression - would need const evaluation
            // For now, use 0 as placeholder
            return make_array(resolve_type(*t.element), 0);
        }
        else if constexpr (std::is_same_v<T, parser::SliceType>) {
            return make_slice(resolve_type(*t.element));
        }
        else if constexpr (std::is_same_v<T, parser::InferType>) {
            return env_.fresh_type_var();
        }
        else if constexpr (std::is_same_v<T, parser::FuncType>) {
            // Convert parser FuncType to semantic FuncType
            std::vector<TypePtr> param_types;
            for (const auto& param : t.params) {
                param_types.push_back(resolve_type(*param));
            }
            TypePtr ret = t.return_type ? resolve_type(*t.return_type) : make_unit();
            auto type = std::make_shared<Type>();
            type->kind = types::FuncType{param_types, ret, false};
            return type;
        }
        else {
            return make_unit();
        }
    }, type.kind);
}

auto TypeChecker::resolve_type_path(const parser::TypePath& path) -> TypePtr {
    if (path.segments.empty()) return make_unit();

    const auto& name = path.segments.back();

    auto& builtins = env_.builtin_types();
    auto it = builtins.find(name);
    if (it != builtins.end()) return it->second;

    auto alias = env_.lookup_type_alias(name);
    if (alias) return *alias;

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
    return std::visit([this](const auto& s) -> bool {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, parser::ExprStmt>) {
            return expr_has_return(*s.expr);
        }
        else if constexpr (std::is_same_v<T, parser::LetStmt>) {
            // Let statements don't contain returns
            return false;
        }
        else if constexpr (std::is_same_v<T, parser::VarStmt>) {
            // Var statements don't contain returns
            return false;
        }
        else {
            return false;
        }
    }, stmt.kind);
}

// Check if an expression contains a return
bool TypeChecker::expr_has_return(const parser::Expr& expr) {
    return std::visit([this](const auto& e) -> bool {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
            return true;
        }
        else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
            return block_has_return(e);
        }
        else if constexpr (std::is_same_v<T, parser::IfExpr>) {
            // If expr has return if both branches have return
            bool then_has = expr_has_return(*e.then_branch);
            bool else_has = e.else_branch.has_value() && expr_has_return(**e.else_branch);
            return then_has && else_has;
        }
        else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
            // When expr has return if all arms have return
            for (const auto& arm : e.arms) {
                if (!expr_has_return(*arm.body)) {
                    return false;
                }
            }
            return !e.arms.empty();
        }
        else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
            // Loop can have return in body
            return expr_has_return(*e.body);
        }
        else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
            // Ternary has return if both branches have it
            return expr_has_return(*e.true_value) && expr_has_return(*e.false_value);
        }
        else {
            // Most expressions don't contain returns
            return false;
        }
    }, expr.kind);
}

} // namespace tml::types
