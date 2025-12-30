// MIR Builder Implementation

#include "mir/mir_builder.hpp"

namespace tml::mir {

MirBuilder::MirBuilder(const types::TypeEnv& env) : env_(env) {}

auto MirBuilder::build(const parser::Module& ast_module) -> Module {
    module_.name = ast_module.name;

    // Process declarations
    for (const auto& decl : ast_module.decls) {
        build_decl(*decl);
    }

    return std::move(module_);
}

// ============================================================================
// Type Conversion
// ============================================================================

auto MirBuilder::convert_type(const parser::Type& type) -> MirTypePtr {
    return std::visit(
        [this](const auto& t) -> MirTypePtr {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
                auto name = t.path.segments.empty() ? "" : t.path.segments.back();

                // Primitive types
                if (name == "Unit" || name == "()")
                    return make_unit_type();
                if (name == "Bool")
                    return make_bool_type();
                if (name == "I8")
                    return make_i32_type(); // TODO: proper i8
                if (name == "I16")
                    return make_i32_type(); // TODO: proper i16
                if (name == "I32")
                    return make_i32_type();
                if (name == "I64")
                    return make_i64_type();
                if (name == "U8")
                    return make_i32_type();
                if (name == "U16")
                    return make_i32_type();
                if (name == "U32")
                    return make_i32_type();
                if (name == "U64")
                    return make_i64_type();
                if (name == "F32")
                    return make_f32_type();
                if (name == "F64")
                    return make_f64_type();
                if (name == "Str")
                    return make_str_type();

                // Generic types
                std::vector<MirTypePtr> type_args;
                if (t.generics.has_value()) {
                    for (const auto& arg : t.generics->args) {
                        type_args.push_back(convert_type(*arg));
                    }
                }

                // Check if it's an enum or struct
                if (env_.lookup_enum(name).has_value()) {
                    return make_enum_type(name, std::move(type_args));
                }
                return make_struct_type(name, std::move(type_args));
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                return make_pointer_type(convert_type(*t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                return make_pointer_type(convert_type(*t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                // TODO: evaluate size expression
                return make_array_type(convert_type(*t.element), 0);
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                auto slice = std::make_shared<MirType>();
                slice->kind = MirSliceType{convert_type(*t.element)};
                return slice;
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                std::vector<MirTypePtr> elements;
                for (const auto& elem : t.elements) {
                    elements.push_back(convert_type(*elem));
                }
                return make_tuple_type(std::move(elements));
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                std::vector<MirTypePtr> params;
                for (const auto& p : t.params) {
                    params.push_back(convert_type(*p));
                }
                auto ret = t.return_type ? convert_type(*t.return_type) : make_unit_type();
                auto func = std::make_shared<MirType>();
                func->kind = MirFunctionType{std::move(params), std::move(ret)};
                return func;
            } else if constexpr (std::is_same_v<T, parser::InferType>) {
                // Inferred type - should be resolved by type checker
                return make_i32_type(); // Fallback
            } else if constexpr (std::is_same_v<T, parser::DynType>) {
                // Trait object - pointer to vtable
                return make_ptr_type();
            } else {
                return make_unit_type();
            }
        },
        type.kind);
}

auto MirBuilder::convert_semantic_type(const types::TypePtr& type) -> MirTypePtr {
    if (!type)
        return make_unit_type();

    return std::visit(
        [this](const auto& t) -> MirTypePtr {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, types::PrimitiveType>) {
                switch (t.kind) {
                case types::PrimitiveKind::Unit:
                    return make_unit_type();
                case types::PrimitiveKind::Bool:
                    return make_bool_type();
                case types::PrimitiveKind::I8:
                case types::PrimitiveKind::I16:
                case types::PrimitiveKind::I32:
                    return make_i32_type();
                case types::PrimitiveKind::I64:
                case types::PrimitiveKind::I128:
                    return make_i64_type();
                case types::PrimitiveKind::U8:
                case types::PrimitiveKind::U16:
                case types::PrimitiveKind::U32:
                    return make_i32_type();
                case types::PrimitiveKind::U64:
                case types::PrimitiveKind::U128:
                    return make_i64_type();
                case types::PrimitiveKind::F32:
                    return make_f32_type();
                case types::PrimitiveKind::F64:
                    return make_f64_type();
                case types::PrimitiveKind::Str:
                    return make_str_type();
                default:
                    return make_unit_type();
                }
            } else if constexpr (std::is_same_v<T, types::NamedType>) {
                std::vector<MirTypePtr> type_args;
                for (const auto& arg : t.type_args) {
                    type_args.push_back(convert_semantic_type(arg));
                }
                if (env_.lookup_enum(t.name).has_value()) {
                    return make_enum_type(t.name, std::move(type_args));
                }
                return make_struct_type(t.name, std::move(type_args));
            } else if constexpr (std::is_same_v<T, types::RefType>) {
                return make_pointer_type(convert_semantic_type(t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, types::PtrType>) {
                return make_pointer_type(convert_semantic_type(t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, types::ArrayType>) {
                return make_array_type(convert_semantic_type(t.element), t.size);
            } else if constexpr (std::is_same_v<T, types::SliceType>) {
                auto slice = std::make_shared<MirType>();
                slice->kind = MirSliceType{convert_semantic_type(t.element)};
                return slice;
            } else if constexpr (std::is_same_v<T, types::TupleType>) {
                std::vector<MirTypePtr> elements;
                for (const auto& elem : t.elements) {
                    elements.push_back(convert_semantic_type(elem));
                }
                return make_tuple_type(std::move(elements));
            } else if constexpr (std::is_same_v<T, types::FuncType>) {
                std::vector<MirTypePtr> params;
                for (const auto& p : t.params) {
                    params.push_back(convert_semantic_type(p));
                }
                auto ret = convert_semantic_type(t.return_type);
                auto func = std::make_shared<MirType>();
                func->kind = MirFunctionType{std::move(params), std::move(ret)};
                return func;
            } else if constexpr (std::is_same_v<T, types::GenericType>) {
                // Generic type parameter - should be instantiated
                return make_i32_type(); // Fallback
            } else {
                return make_unit_type();
            }
        },
        type->kind);
}

// ============================================================================
// Declaration Building
// ============================================================================

void MirBuilder::build_decl(const parser::Decl& decl) {
    std::visit(
        [this](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                build_func_decl(d);
            } else if constexpr (std::is_same_v<T, parser::StructDecl>) {
                build_struct_decl(d);
            } else if constexpr (std::is_same_v<T, parser::EnumDecl>) {
                build_enum_decl(d);
            }
            // Other declarations (trait, impl, use, etc.) handled separately
        },
        decl.kind);
}

void MirBuilder::build_func_decl(const parser::FuncDecl& func) {
    // Skip generic functions (they are instantiated on demand)
    if (!func.generics.empty())
        return;

    // Skip extern functions (no body)
    if (!func.body.has_value())
        return;

    Function mir_func;
    mir_func.name = func.name;
    mir_func.is_public = (func.vis == parser::Visibility::Public);

    // Extract decorator attributes (@inline, @noinline, etc.)
    for (const auto& decorator : func.decorators) {
        mir_func.attributes.push_back(decorator.name);
    }

    // Convert return type
    mir_func.return_type = func.return_type ? convert_type(**func.return_type) : make_unit_type();

    // Set up context
    ctx_.current_func = &mir_func;
    ctx_.variables.clear();

    // Create entry block
    auto entry = mir_func.create_block("entry");
    ctx_.current_block = entry;

    // Add parameters
    for (const auto& param : func.params) {
        // Get parameter name from pattern
        std::string param_name;
        if (param.pattern->is<parser::IdentPattern>()) {
            param_name = param.pattern->as<parser::IdentPattern>().name;
        } else {
            param_name = "_param" + std::to_string(mir_func.params.size());
        }

        auto param_type = convert_type(*param.type);
        auto value_id = mir_func.fresh_value();

        FunctionParam mir_param;
        mir_param.name = param_name;
        mir_param.type = param_type;
        mir_param.value_id = value_id;
        mir_func.params.push_back(mir_param);

        // Add to variable map
        ctx_.variables[param_name] = Value{value_id, param_type};
    }

    // Build function body
    auto body_value = build_block(*func.body);

    // Add implicit return if not terminated
    if (!is_terminated()) {
        if (mir_func.return_type->is_unit()) {
            emit_return();
        } else {
            emit_return(body_value);
        }
    }

    module_.functions.push_back(std::move(mir_func));
    ctx_.current_func = nullptr;
}

void MirBuilder::build_struct_decl(const parser::StructDecl& s) {
    // Skip generic structs
    if (!s.generics.empty())
        return;

    StructDef def;
    def.name = s.name;

    for (const auto& field : s.fields) {
        StructField f;
        f.name = field.name;
        f.type = convert_type(*field.type);
        def.fields.push_back(std::move(f));
    }

    module_.structs.push_back(std::move(def));
}

void MirBuilder::build_enum_decl(const parser::EnumDecl& e) {
    // Skip generic enums
    if (!e.generics.empty())
        return;

    EnumDef def;
    def.name = e.name;

    for (const auto& variant : e.variants) {
        EnumVariant v;
        v.name = variant.name;

        if (variant.tuple_fields.has_value()) {
            for (const auto& field : *variant.tuple_fields) {
                v.payload_types.push_back(convert_type(*field));
            }
        }

        def.variants.push_back(std::move(v));
    }

    module_.enums.push_back(std::move(def));
}

// ============================================================================
// Statement Building
// ============================================================================

void MirBuilder::build_stmt(const parser::Stmt& stmt) {
    std::visit(
        [this](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, parser::LetStmt>) {
                build_let_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::VarStmt>) {
                build_var_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
                build_expr_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
                build_decl(*s);
            }
        },
        stmt.kind);
}

void MirBuilder::build_let_stmt(const parser::LetStmt& let) {
    if (!let.init.has_value())
        return;

    auto init_value = build_expr(**let.init);
    build_pattern_binding(*let.pattern, init_value);
}

void MirBuilder::build_var_stmt(const parser::VarStmt& var) {
    auto init_value = build_expr(*var.init);

    // For mutable variables, allocate stack space
    auto alloca_val =
        emit(AllocaInst{init_value.type, var.name}, make_pointer_type(init_value.type, true));

    // Store initial value
    emit_void(StoreInst{alloca_val, init_value});

    // Map variable to alloca
    ctx_.variables[var.name] = alloca_val;
}

void MirBuilder::build_expr_stmt(const parser::ExprStmt& expr) {
    build_expr(*expr.expr);
}

// ============================================================================
// Expression Building
// ============================================================================

auto MirBuilder::build_expr(const parser::Expr& expr) -> Value {
    return std::visit(
        [this](const auto& e) -> Value {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                return build_literal(e);
            } else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                return build_ident(e);
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                return build_binary(e);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                return build_unary(e);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                return build_call(e);
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                return build_method_call(e);
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                return build_field(e);
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                return build_index(e);
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                return build_if(e);
            } else if constexpr (std::is_same_v<T, parser::TernaryExpr>) {
                return build_ternary(e);
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                return build_block(e);
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                return build_loop(e);
            } else if constexpr (std::is_same_v<T, parser::WhileExpr>) {
                return build_while(e);
            } else if constexpr (std::is_same_v<T, parser::ForExpr>) {
                return build_for(e);
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                return build_return(e);
            } else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
                return build_break(e);
            } else if constexpr (std::is_same_v<T, parser::ContinueExpr>) {
                return build_continue(e);
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                return build_when(e);
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                return build_struct_expr(e);
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                return build_tuple(e);
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                return build_array(e);
            } else if constexpr (std::is_same_v<T, parser::PathExpr>) {
                return build_path(e);
            } else if constexpr (std::is_same_v<T, parser::CastExpr>) {
                return build_cast(e);
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                return build_closure(e);
            } else {
                // Other expressions - return unit for now
                return const_unit();
            }
        },
        expr.kind);
}

auto MirBuilder::build_literal(const parser::LiteralExpr& lit) -> Value {
    const auto& token = lit.token;

    switch (token.kind) {
    case lexer::TokenKind::IntLiteral: {
        int64_t value = token.int_value().value;
        return const_int(value, 32, true);
    }
    case lexer::TokenKind::FloatLiteral: {
        double value = token.float_value().value;
        return const_float(value, false);
    }
    case lexer::TokenKind::StringLiteral:
        return const_string(token.string_value().value);
    case lexer::TokenKind::CharLiteral:
        return const_int(static_cast<int64_t>(token.char_value().value), 32, false);
    case lexer::TokenKind::BoolLiteral:
        return const_bool(token.bool_value());
    default:
        return const_unit();
    }
}

auto MirBuilder::build_ident(const parser::IdentExpr& ident) -> Value {
    return get_variable(ident.name);
}

auto MirBuilder::build_binary(const parser::BinaryExpr& bin) -> Value {
    // Handle assignment
    if (bin.op == parser::BinaryOp::Assign) {
        auto rhs = build_expr(*bin.right);

        // Get the target address
        if (bin.left->is<parser::IdentExpr>()) {
            auto& name = bin.left->as<parser::IdentExpr>().name;
            auto ptr = ctx_.variables[name];
            emit_void(StoreInst{ptr, rhs});
            return rhs;
        }
        // TODO: Handle field/index assignment
        return rhs;
    }

    // Handle short-circuit evaluation for && and ||
    if (bin.op == parser::BinaryOp::And || bin.op == parser::BinaryOp::Or) {
        auto lhs = build_expr(*bin.left);

        auto rhs_block = create_block("and_rhs");
        auto merge_block = create_block("and_merge");

        if (bin.op == parser::BinaryOp::And) {
            // If lhs is false, short-circuit to false
            emit_cond_branch(lhs, rhs_block, merge_block);
        } else {
            // If lhs is true, short-circuit to true
            emit_cond_branch(lhs, merge_block, rhs_block);
        }

        // Evaluate RHS
        switch_to_block(rhs_block);
        auto rhs = build_expr(*bin.right);
        auto rhs_end_block = ctx_.current_block;
        emit_branch(merge_block);

        // Merge
        switch_to_block(merge_block);
        PhiInst phi;
        if (bin.op == parser::BinaryOp::And) {
            phi.incoming = {{const_bool(false), ctx_.current_block - 2}, {rhs, rhs_end_block}};
        } else {
            phi.incoming = {{const_bool(true), ctx_.current_block - 2}, {rhs, rhs_end_block}};
        }
        return emit(std::move(phi), make_bool_type());
    }

    // Regular binary operation
    auto lhs = build_expr(*bin.left);
    auto rhs = build_expr(*bin.right);

    BinaryInst inst;
    inst.op = get_binop(bin.op);
    inst.left = lhs;
    inst.right = rhs;

    auto result_type = is_comparison_op(bin.op) ? make_bool_type() : lhs.type;
    return emit(std::move(inst), result_type);
}

auto MirBuilder::build_unary(const parser::UnaryExpr& unary) -> Value {
    auto operand = build_expr(*unary.operand);

    if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
        // Take address - need to allocate if not already a pointer
        // For now, assume operand is already addressable
        return operand;
    }

    if (unary.op == parser::UnaryOp::Deref) {
        // Dereference
        return emit(LoadInst{operand}, operand.type);
    }

    UnaryInst inst;
    inst.op = get_unaryop(unary.op);
    inst.operand = operand;
    return emit(std::move(inst), operand.type);
}

auto MirBuilder::build_call(const parser::CallExpr& call) -> Value {
    // Get function name
    std::string func_name;
    if (call.callee->is<parser::IdentExpr>()) {
        func_name = call.callee->as<parser::IdentExpr>().name;
    } else if (call.callee->is<parser::PathExpr>()) {
        auto& path = call.callee->as<parser::PathExpr>().path;
        for (size_t i = 0; i < path.segments.size(); ++i) {
            if (i > 0)
                func_name += "::";
            func_name += path.segments[i];
        }
    } else {
        // Function pointer call
        auto callee = build_expr(*call.callee);
        // TODO: indirect call
        return const_unit();
    }

    // Build arguments
    std::vector<Value> args;
    for (const auto& arg : call.args) {
        args.push_back(build_expr(*arg));
    }

    // Get return type from type environment
    auto func_sig = env_.lookup_func(func_name);
    MirTypePtr return_type = make_unit_type();
    if (func_sig.has_value()) {
        return_type = convert_semantic_type(func_sig->return_type);
    }

    CallInst inst;
    inst.func_name = func_name;
    inst.args = std::move(args);
    inst.return_type = return_type;

    if (return_type->is_unit()) {
        emit_void(std::move(inst));
        return const_unit();
    }
    return emit(std::move(inst), return_type);
}

auto MirBuilder::build_method_call(const parser::MethodCallExpr& call) -> Value {
    auto receiver = build_expr(*call.receiver);

    std::vector<Value> args;
    for (const auto& arg : call.args) {
        args.push_back(build_expr(*arg));
    }

    // TODO: Resolve method to actual function
    MethodCallInst inst;
    inst.receiver = receiver;
    inst.method_name = call.method;
    inst.args = std::move(args);
    inst.return_type = make_i32_type(); // TODO: proper type

    return emit(std::move(inst), inst.return_type);
}

auto MirBuilder::build_field(const parser::FieldExpr& field) -> Value {
    auto object = build_expr(*field.object);

    // TODO: Get field index from struct type
    ExtractValueInst inst;
    inst.aggregate = object;
    inst.indices = {0}; // TODO: actual field index

    return emit(std::move(inst), make_i32_type()); // TODO: actual field type
}

auto MirBuilder::build_index(const parser::IndexExpr& index) -> Value {
    auto object = build_expr(*index.object);
    auto idx = build_expr(*index.index);

    GetElementPtrInst gep;
    gep.base = object;
    gep.indices = {idx};
    gep.result_type = make_ptr_type(); // TODO: element type

    auto ptr = emit(std::move(gep), gep.result_type);
    return emit(LoadInst{ptr}, make_i32_type()); // TODO: element type
}

auto MirBuilder::build_if(const parser::IfExpr& if_expr) -> Value {
    auto cond = build_expr(*if_expr.condition);

    auto then_block = create_block("if_then");
    auto else_block = if_expr.else_branch.has_value() ? create_block("if_else") : 0;
    auto merge_block = create_block("if_merge");

    emit_cond_branch(cond, then_block, else_block ? else_block : merge_block);

    // Then branch
    switch_to_block(then_block);
    auto then_val = build_expr(*if_expr.then_branch);
    auto then_end = ctx_.current_block;
    if (!is_terminated()) {
        emit_branch(merge_block);
    }

    // Else branch
    Value else_val = const_unit();
    uint32_t else_end = 0;
    if (if_expr.else_branch.has_value()) {
        switch_to_block(else_block);
        else_val = build_expr(**if_expr.else_branch);
        else_end = ctx_.current_block;
        if (!is_terminated()) {
            emit_branch(merge_block);
        }
    }

    // Merge block
    switch_to_block(merge_block);

    // If both branches return a value, create phi
    if (if_expr.else_branch.has_value() && !then_val.type->is_unit()) {
        PhiInst phi;
        phi.incoming = {{then_val, then_end}, {else_val, else_end}};
        return emit(std::move(phi), then_val.type);
    }

    return const_unit();
}

auto MirBuilder::build_ternary(const parser::TernaryExpr& ternary) -> Value {
    auto cond = build_expr(*ternary.condition);
    auto true_val = build_expr(*ternary.true_value);
    auto false_val = build_expr(*ternary.false_value);

    SelectInst inst;
    inst.condition = cond;
    inst.true_val = true_val;
    inst.false_val = false_val;

    return emit(std::move(inst), true_val.type);
}

auto MirBuilder::build_block(const parser::BlockExpr& block) -> Value {
    for (const auto& stmt : block.stmts) {
        build_stmt(*stmt);
        if (is_terminated())
            return const_unit();
    }

    if (block.expr.has_value()) {
        return build_expr(**block.expr);
    }

    return const_unit();
}

auto MirBuilder::build_loop(const parser::LoopExpr& loop) -> Value {
    auto header = create_block("loop_header");
    auto body = create_block("loop_body");
    auto exit = create_block("loop_exit");

    emit_branch(header);

    // Header just jumps to body (infinite loop)
    switch_to_block(header);
    emit_branch(body);

    // Push loop context
    ctx_.loop_stack.push({header, exit, std::nullopt});

    // Body
    switch_to_block(body);
    build_expr(*loop.body);
    if (!is_terminated()) {
        emit_branch(header);
    }

    ctx_.loop_stack.pop();

    switch_to_block(exit);
    return const_unit();
}

auto MirBuilder::build_while(const parser::WhileExpr& while_expr) -> Value {
    auto header = create_block("while_header");
    auto body = create_block("while_body");
    auto exit = create_block("while_exit");

    emit_branch(header);

    // Header evaluates condition
    switch_to_block(header);
    auto cond = build_expr(*while_expr.condition);
    emit_cond_branch(cond, body, exit);

    // Push loop context
    ctx_.loop_stack.push({header, exit, std::nullopt});

    // Body
    switch_to_block(body);
    build_expr(*while_expr.body);
    if (!is_terminated()) {
        emit_branch(header);
    }

    ctx_.loop_stack.pop();

    switch_to_block(exit);
    return const_unit();
}

auto MirBuilder::build_for(const parser::ForExpr& /*for_expr*/) -> Value {
    // TODO: Implement for loop (requires iterator protocol)
    return const_unit();
}

auto MirBuilder::build_return(const parser::ReturnExpr& ret) -> Value {
    if (ret.value.has_value()) {
        auto val = build_expr(**ret.value);
        emit_return(val);
    } else {
        emit_return();
    }
    return const_unit();
}

auto MirBuilder::build_break(const parser::BreakExpr& brk) -> Value {
    if (ctx_.loop_stack.empty())
        return const_unit();

    auto& loop = ctx_.loop_stack.top();

    if (brk.value.has_value()) {
        auto val = build_expr(**brk.value);
        loop.break_value = val;
    }

    emit_branch(loop.exit_block);
    return const_unit();
}

auto MirBuilder::build_continue(const parser::ContinueExpr& /*cont*/) -> Value {
    if (ctx_.loop_stack.empty())
        return const_unit();

    auto& loop = ctx_.loop_stack.top();
    emit_branch(loop.header_block);
    return const_unit();
}

auto MirBuilder::build_when(const parser::WhenExpr& /*when*/) -> Value {
    // TODO: Implement pattern matching
    return const_unit();
}

auto MirBuilder::build_struct_expr(const parser::StructExpr& s) -> Value {
    std::string struct_name = s.path.segments.empty() ? "" : s.path.segments.back();

    std::vector<Value> fields;
    for (const auto& [name, expr] : s.fields) {
        fields.push_back(build_expr(*expr));
    }

    StructInitInst inst;
    inst.struct_name = struct_name;
    inst.fields = std::move(fields);

    return emit(std::move(inst), make_struct_type(struct_name));
}

auto MirBuilder::build_tuple(const parser::TupleExpr& tuple) -> Value {
    std::vector<Value> elements;
    std::vector<MirTypePtr> types;

    for (const auto& elem : tuple.elements) {
        auto val = build_expr(*elem);
        elements.push_back(val);
        types.push_back(val.type);
    }

    TupleInitInst inst;
    inst.elements = std::move(elements);

    return emit(std::move(inst), make_tuple_type(std::move(types)));
}

auto MirBuilder::build_array(const parser::ArrayExpr& arr) -> Value {
    return std::visit(
        [this](const auto& a) -> Value {
            using T = std::decay_t<decltype(a)>;

            if constexpr (std::is_same_v<T, std::vector<parser::ExprPtr>>) {
                std::vector<Value> elements;
                MirTypePtr elem_type = make_i32_type();

                for (const auto& elem : a) {
                    auto val = build_expr(*elem);
                    elements.push_back(val);
                    elem_type = val.type;
                }

                ArrayInitInst inst;
                inst.elements = std::move(elements);
                inst.element_type = elem_type;

                return emit(std::move(inst), make_array_type(elem_type, a.size()));
            } else {
                // Repeat syntax: [expr; count]
                auto val = build_expr(*a.first);
                // TODO: evaluate count
                return val;
            }
        },
        arr.kind);
}

auto MirBuilder::build_path(const parser::PathExpr& path) -> Value {
    // Path could be a variable, enum variant, or module item
    auto name = path.path.segments.empty() ? "" : path.path.segments.back();
    return get_variable(name);
}

auto MirBuilder::build_cast(const parser::CastExpr& cast) -> Value {
    auto val = build_expr(*cast.expr);
    auto target = convert_type(*cast.target);

    // Determine cast kind based on types
    CastKind kind = CastKind::Bitcast;

    if (val.type->is_integer() && target->is_integer()) {
        int src_width = val.type->bit_width();
        int dst_width = target->bit_width();
        if (src_width < dst_width) {
            kind = val.type->is_signed() ? CastKind::SExt : CastKind::ZExt;
        } else if (src_width > dst_width) {
            kind = CastKind::Trunc;
        }
    } else if (val.type->is_float() && target->is_integer()) {
        kind = target->is_signed() ? CastKind::FPToSI : CastKind::FPToUI;
    } else if (val.type->is_integer() && target->is_float()) {
        kind = val.type->is_signed() ? CastKind::SIToFP : CastKind::UIToFP;
    }

    CastInst inst;
    inst.kind = kind;
    inst.operand = val;
    inst.target_type = target;

    return emit(std::move(inst), target);
}

auto MirBuilder::build_closure(const parser::ClosureExpr& /*closure*/) -> Value {
    // TODO: Implement closure lowering
    return const_unit();
}

// ============================================================================
// Pattern Building
// ============================================================================

void MirBuilder::build_pattern_binding(const parser::Pattern& pattern, Value value) {
    std::visit(
        [this, &value](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, parser::IdentPattern>) {
                ctx_.variables[p.name] = value;
            } else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
                for (size_t i = 0; i < p.elements.size(); ++i) {
                    ExtractValueInst extract;
                    extract.aggregate = value;
                    extract.indices = {static_cast<uint32_t>(i)};

                    // Get element type from tuple
                    MirTypePtr elem_type = make_i32_type();
                    if (auto* tuple = std::get_if<MirTupleType>(&value.type->kind)) {
                        if (i < tuple->elements.size()) {
                            elem_type = tuple->elements[i];
                        }
                    }

                    auto elem = emit(std::move(extract), elem_type);
                    build_pattern_binding(*p.elements[i], elem);
                }
            } else if constexpr (std::is_same_v<T, parser::WildcardPattern>) {
                // Ignore
            }
            // TODO: Handle other patterns (struct, enum, etc.)
        },
        pattern.kind);
}

// ============================================================================
// Helper Methods
// ============================================================================

auto MirBuilder::create_block(const std::string& name) -> uint32_t {
    return ctx_.current_func->create_block(name);
}

void MirBuilder::switch_to_block(uint32_t block_id) {
    ctx_.current_block = block_id;
}

auto MirBuilder::is_terminated() const -> bool {
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    return block && block->terminator.has_value();
}

auto MirBuilder::emit(Instruction inst, MirTypePtr type) -> Value {
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block)
        return {INVALID_VALUE, type};

    auto id = ctx_.current_func->fresh_value();

    InstructionData data;
    data.result = id;
    data.type = type;
    data.inst = std::move(inst);

    block->instructions.push_back(std::move(data));

    return {id, type};
}

void MirBuilder::emit_void(Instruction inst) {
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block)
        return;

    InstructionData data;
    data.result = INVALID_VALUE;
    data.type = make_unit_type();
    data.inst = std::move(inst);

    block->instructions.push_back(std::move(data));
}

void MirBuilder::emit_return(std::optional<Value> value) {
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block)
        return;

    ReturnTerm term;
    term.value = value;
    block->terminator = std::move(term);
}

void MirBuilder::emit_branch(uint32_t target) {
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block)
        return;

    block->terminator = BranchTerm{target};
}

void MirBuilder::emit_cond_branch(Value cond, uint32_t true_block, uint32_t false_block) {
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block)
        return;

    block->terminator = CondBranchTerm{cond, true_block, false_block};
}

void MirBuilder::emit_unreachable() {
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block)
        return;

    block->terminator = UnreachableTerm{};
}

auto MirBuilder::const_int(int64_t value, int bit_width, bool is_signed) -> Value {
    ConstantInst inst;
    inst.value = ConstInt{value, is_signed, bit_width};

    MirTypePtr type;
    if (bit_width <= 32) {
        type = make_i32_type();
    } else {
        type = make_i64_type();
    }

    return emit(std::move(inst), type);
}

auto MirBuilder::const_float(double value, bool is_f64) -> Value {
    ConstantInst inst;
    inst.value = ConstFloat{value, is_f64};
    return emit(std::move(inst), is_f64 ? make_f64_type() : make_f32_type());
}

auto MirBuilder::const_bool(bool value) -> Value {
    ConstantInst inst;
    inst.value = ConstBool{value};
    return emit(std::move(inst), make_bool_type());
}

auto MirBuilder::const_string(const std::string& value) -> Value {
    ConstantInst inst;
    inst.value = ConstString{value};
    return emit(std::move(inst), make_str_type());
}

auto MirBuilder::const_unit() -> Value {
    ConstantInst inst;
    inst.value = ConstUnit{};
    return emit(std::move(inst), make_unit_type());
}

auto MirBuilder::get_variable(const std::string& name) -> Value {
    auto it = ctx_.variables.find(name);
    if (it != ctx_.variables.end()) {
        return it->second;
    }
    // Unknown variable - return invalid
    return {INVALID_VALUE, make_unit_type()};
}

void MirBuilder::set_variable(const std::string& name, Value value) {
    ctx_.variables[name] = value;
}

auto MirBuilder::get_binop(parser::BinaryOp op) -> BinOp {
    switch (op) {
    case parser::BinaryOp::Add:
        return BinOp::Add;
    case parser::BinaryOp::Sub:
        return BinOp::Sub;
    case parser::BinaryOp::Mul:
        return BinOp::Mul;
    case parser::BinaryOp::Div:
        return BinOp::Div;
    case parser::BinaryOp::Mod:
        return BinOp::Mod;
    case parser::BinaryOp::Eq:
        return BinOp::Eq;
    case parser::BinaryOp::Ne:
        return BinOp::Ne;
    case parser::BinaryOp::Lt:
        return BinOp::Lt;
    case parser::BinaryOp::Le:
        return BinOp::Le;
    case parser::BinaryOp::Gt:
        return BinOp::Gt;
    case parser::BinaryOp::Ge:
        return BinOp::Ge;
    case parser::BinaryOp::And:
        return BinOp::And;
    case parser::BinaryOp::Or:
        return BinOp::Or;
    case parser::BinaryOp::BitAnd:
        return BinOp::BitAnd;
    case parser::BinaryOp::BitOr:
        return BinOp::BitOr;
    case parser::BinaryOp::BitXor:
        return BinOp::BitXor;
    case parser::BinaryOp::Shl:
        return BinOp::Shl;
    case parser::BinaryOp::Shr:
        return BinOp::Shr;
    default:
        return BinOp::Add;
    }
}

auto MirBuilder::is_comparison_op(parser::BinaryOp op) -> bool {
    switch (op) {
    case parser::BinaryOp::Eq:
    case parser::BinaryOp::Ne:
    case parser::BinaryOp::Lt:
    case parser::BinaryOp::Le:
    case parser::BinaryOp::Gt:
    case parser::BinaryOp::Ge:
        return true;
    default:
        return false;
    }
}

auto MirBuilder::get_unaryop(parser::UnaryOp op) -> UnaryOp {
    switch (op) {
    case parser::UnaryOp::Neg:
        return UnaryOp::Neg;
    case parser::UnaryOp::Not:
        return UnaryOp::Not;
    case parser::UnaryOp::BitNot:
        return UnaryOp::BitNot;
    default:
        return UnaryOp::Neg;
    }
}

} // namespace tml::mir
