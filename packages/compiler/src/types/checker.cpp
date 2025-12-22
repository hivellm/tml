#include "tml/types/checker.hpp"
#include "tml/lexer/token.hpp"

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

    return false;
}
} // anonymous namespace

TypeChecker::TypeChecker() = default;

auto TypeChecker::check_module(const parser::Module& module)
    -> Result<TypeEnv, std::vector<TypeError>> {

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

    // Second pass: register function signatures
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            check_func_decl(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            check_impl_decl(decl->as<parser::ImplDecl>());
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

void TypeChecker::check_func_decl(const parser::FuncDecl& func) {
    std::vector<TypePtr> params;
    for (const auto& p : func.params) {
        params.push_back(resolve_type(*p.type));
    }
    TypePtr ret = func.return_type ? resolve_type(**func.return_type) : make_unit();
    env_.define_func(FuncSig{
        .name = func.name,
        .params = std::move(params),
        .return_type = std::move(ret),
        .type_params = {},
        .is_async = func.is_async,
        .span = func.span
    });
}

void TypeChecker::check_func_body(const parser::FuncDecl& func) {
    env_.push_scope();
    current_return_type_ = func.return_type ? resolve_type(**func.return_type) : make_unit();

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
        // Check return type compatibility (simplified for now)
        (void)body_type;
    }

    env_.pop_scope();
    current_return_type_ = nullptr;
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
    }
    return make_unit();
}

auto TypeChecker::check_call(const parser::CallExpr& call) -> TypePtr {
    auto callee_type = check_expr(*call.callee);

    // Check if this is a variadic builtin (print/println)
    bool is_variadic_builtin = false;
    if (call.callee->is<parser::IdentExpr>()) {
        const auto& name = call.callee->as<parser::IdentExpr>().name;
        if (name == "print" || name == "println") {
            is_variadic_builtin = true;
        }
    }

    if (callee_type->is<FuncType>()) {
        auto& func = callee_type->as<FuncType>();
        // Check argument count (skip for variadic builtins)
        if (!is_variadic_builtin && call.args.size() != func.params.size()) {
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
        auto func = env_.lookup_func(ident.name);
        if (func) {
            return func->return_type;
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
    bind_pattern(*for_expr.pattern, iter_type);

    check_expr(*for_expr.body);

    env_.pop_scope();
    loop_depth_--;

    return make_unit();
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
    env_.push_scope();

    std::vector<TypePtr> param_types;
    for (const auto& [pattern, type_opt] : closure.params) {
        TypePtr ptype = type_opt ? resolve_type(**type_opt) : env_.fresh_type_var();
        param_types.push_back(ptype);
        if (pattern->is<parser::IdentPattern>()) {
            auto& ident = pattern->as<parser::IdentPattern>();
            env_.current_scope()->define(ident.name, ptype, ident.is_mut, pattern->span);
        }
    }

    auto body_type = check_expr(*closure.body);
    TypePtr return_type = closure.return_type ? resolve_type(**closure.return_type) : body_type;

    env_.pop_scope();

    return make_func(std::move(param_types), return_type);
}

auto TypeChecker::check_try(const parser::TryExpr& try_expr) -> TypePtr {
    return check_expr(*try_expr.expr);
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

} // namespace tml::types
