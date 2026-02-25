TML_MODULE("compiler")

//! # MIR Builder - Expressions
//!
//! This file converts AST expressions to MIR SSA form.
//!
//! ## Expression Categories
//!
//! | Category     | Expressions                              |
//! |--------------|------------------------------------------|
//! | Literals     | int, float, string, char, bool           |
//! | Variables    | identifier, path                         |
//! | Operations   | binary, unary, cast                      |
//! | Calls        | function call, method call               |
//! | Access       | field, index                             |
//! | Control      | if, block, loop, return, break, continue |
//! | Constructors | struct, tuple, array                     |
//! | Async        | closure, await                           |
//!
//! ## Assignment Handling
//!
//! Assignment `x = val` is desugared to store instruction.
//! Field and index assignments generate GEP + store.

#include "mir/mir_builder.hpp"

namespace tml::mir {

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
            } else if constexpr (std::is_same_v<T, parser::AwaitExpr>) {
                return build_await(e);
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
        // Use 64-bit by default for integer literals on 64-bit platforms
        // This prevents type mismatches when assigning to I64 variables
        return const_int(value, 64, true);
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

        // Handle field assignment: obj.field = value
        if (bin.left->is<parser::FieldExpr>()) {
            auto& field_expr = bin.left->as<parser::FieldExpr>();
            auto object = build_expr(*field_expr.object);

            // Get field pointer via GEP
            GetElementPtrInst gep;
            gep.base = object;
            // Field index would be looked up from struct definition
            gep.indices = {const_int(0, 32, false)}; // Index 0 for first field as example
            gep.result_type = make_pointer_type(rhs.type, true);

            auto ptr = emit(std::move(gep), gep.result_type);
            emit_void(StoreInst{ptr, rhs});
            return rhs;
        }

        // Handle index assignment: arr[i] = value
        if (bin.left->is<parser::IndexExpr>()) {
            auto& index_expr = bin.left->as<parser::IndexExpr>();
            auto object = build_expr(*index_expr.object);
            auto idx = build_expr(*index_expr.index);

            GetElementPtrInst gep;
            gep.base = object;
            gep.indices = {idx};
            gep.result_type = make_pointer_type(rhs.type, true);

            auto ptr = emit(std::move(gep), gep.result_type);
            emit_void(StoreInst{ptr, rhs});
            return rhs;
        }

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
        // Function pointer call (indirect call)
        auto callee = build_expr(*call.callee);

        // Build arguments
        std::vector<Value> args;
        for (const auto& arg : call.args) {
            args.push_back(build_expr(*arg));
        }

        // For function pointer calls, we need to extract the return type from the callee's type
        MirTypePtr return_type = make_unit_type();
        if (auto* func_type = std::get_if<MirFunctionType>(&callee.type->kind)) {
            return_type = func_type->return_type;
        }

        // Create indirect call instruction
        CallInst inst;
        inst.func_name = ""; // Empty name indicates indirect call
        inst.args = std::move(args);
        inst.return_type = return_type;

        // Store the callee value in the first argument position for indirect call
        inst.args.insert(inst.args.begin(), callee);

        if (return_type->is_unit()) {
            emit_void(std::move(inst));
            return const_unit();
        }
        return emit(std::move(inst), return_type);
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
    // Fallback: look up in module registry for internal module functions
    if (!func_sig.has_value() && env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto func_it = mod.functions.find(func_name);
            if (func_it != mod.functions.end()) {
                return_type = convert_semantic_type(func_it->second.return_type);
                break;
            }
        }
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

    // Try to resolve method to actual function name
    // The receiver type is used to find the implementing struct/behavior
    std::string receiver_type_name;
    if (auto* struct_type = std::get_if<MirStructType>(&receiver.type->kind)) {
        receiver_type_name = struct_type->name;
    } else if (auto* enum_type = std::get_if<MirEnumType>(&receiver.type->kind)) {
        receiver_type_name = enum_type->name;
    }

    // Look up the function signature in the type environment
    // Method names are typically mangled as TypeName::method_name
    std::string func_name =
        receiver_type_name.empty() ? call.method : receiver_type_name + "::" + call.method;

    MirTypePtr return_type = make_unit_type();

    // Try to find the function signature for proper return type
    if (auto sig = env_.lookup_func(func_name)) {
        return_type = convert_semantic_type(sig->return_type);
    } else if (auto sig2 = env_.lookup_func(call.method)) {
        // Fallback to just the method name
        return_type = convert_semantic_type(sig2->return_type);
    }

    MethodCallInst inst;
    inst.receiver = receiver;
    inst.receiver_type = receiver_type_name;
    inst.method_name = call.method;
    inst.args = std::move(args);
    inst.return_type = return_type;

    if (return_type->is_unit()) {
        emit_void(std::move(inst));
        return const_unit();
    }
    return emit(std::move(inst), inst.return_type);
}

auto MirBuilder::build_field(const parser::FieldExpr& field) -> Value {
    auto object = build_expr(*field.object);

    // Get struct type name to look up field information
    std::string struct_name;
    if (auto* struct_type = std::get_if<MirStructType>(&object.type->kind)) {
        struct_name = struct_type->name;
    }

    // Look up field index and type from struct definition
    uint32_t field_index = 0;
    MirTypePtr field_type = make_i32_type(); // Default fallback

    if (!struct_name.empty()) {
        if (auto struct_def = env_.lookup_struct(struct_name)) {
            for (size_t i = 0; i < struct_def->fields.size(); ++i) {
                if (struct_def->fields[i].name == field.field) {
                    field_index = static_cast<uint32_t>(i);
                    field_type = convert_semantic_type(struct_def->fields[i].type);
                    break;
                }
            }
        }
    }

    // Handle tuple field access (e.g., tuple.0, tuple.1)
    if (auto* tuple_type = std::get_if<MirTupleType>(&object.type->kind)) {
        // Field name should be a number
        try {
            field_index = static_cast<uint32_t>(std::stoul(field.field));
            if (field_index < tuple_type->elements.size()) {
                field_type = tuple_type->elements[field_index];
            }
        } catch (...) {
            // Not a valid tuple index, use default
        }
    }

    ExtractValueInst inst;
    inst.aggregate = object;
    inst.indices = {field_index};
    inst.aggregate_type = object.type;
    inst.result_type = field_type;

    return emit(std::move(inst), field_type);
}

auto MirBuilder::build_index(const parser::IndexExpr& index) -> Value {
    auto object = build_expr(*index.object);
    auto idx = build_expr(*index.index);

    // Determine element type from object type
    MirTypePtr element_type = make_i32_type();
    if (auto* array_type = std::get_if<MirArrayType>(&object.type->kind)) {
        element_type = array_type->element;
    } else if (auto* slice_type = std::get_if<MirSliceType>(&object.type->kind)) {
        element_type = slice_type->element;
    } else if (auto* ptr_type = std::get_if<MirPointerType>(&object.type->kind)) {
        element_type = ptr_type->pointee;
    }

    GetElementPtrInst gep;
    gep.base = object;
    gep.indices = {idx};
    gep.base_type = object.type;
    gep.result_type = make_pointer_type(element_type, false);

    auto ptr = emit(std::move(gep), gep.result_type);

    LoadInst load;
    load.ptr = ptr;
    load.result_type = element_type;

    return emit(std::move(load), element_type);
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
    // Capture and clear the type hint so nested expressions don't inherit it.
    MirTypePtr hint = expr_type_hint_;
    expr_type_hint_ = nullptr;

    // If there's an array type hint, extract the element type from it.
    MirTypePtr hint_elem_type = nullptr;
    if (hint) {
        if (auto* arr_hint = std::get_if<MirArrayType>(&hint->kind)) {
            hint_elem_type = arr_hint->element;
        }
    }

    return std::visit(
        [this, &hint_elem_type](const auto& a) -> Value {
            using T = std::decay_t<decltype(a)>;

            if constexpr (std::is_same_v<T, std::vector<parser::ExprPtr>>) {
                std::vector<Value> elements;
                MirTypePtr elem_type = hint_elem_type ? hint_elem_type : make_i32_type();

                for (const auto& elem : a) {
                    auto val = build_expr(*elem);
                    elements.push_back(val);
                    // Update elem_type from actual value only if no hint was given
                    if (!hint_elem_type) {
                        elem_type = val.type;
                    }
                }

                ArrayInitInst inst;
                inst.elements = std::move(elements);
                inst.element_type = elem_type;

                return emit(std::move(inst), make_array_type(elem_type, a.size()));
            } else {
                // Repeat syntax: [expr; count]
                auto val = build_expr(*a.first);

                // Try to evaluate count as a constant integer
                size_t count = 1;
                if (a.second && a.second->template is<parser::LiteralExpr>()) {
                    const auto& lit = a.second->template as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                        try {
                            count = std::stoull(std::string(lit.token.lexeme));
                        } catch (...) {
                            count = 1;
                        }
                    }
                }

                // Use hint element type if available; otherwise use the value's type.
                MirTypePtr elem_type = hint_elem_type ? hint_elem_type : val.type;

                // Create array with repeated values
                std::vector<Value> elements;
                elements.reserve(count);
                for (size_t i = 0; i < count; ++i) {
                    elements.push_back(val);
                }

                ArrayInitInst inst;
                inst.elements = std::move(elements);
                inst.element_type = elem_type;
                inst.result_type = make_array_type(elem_type, count);

                return emit(std::move(inst), inst.result_type);
            }
        },
        arr.kind);
}

auto MirBuilder::build_path(const parser::PathExpr& path) -> Value {
    // Path could be a variable, enum variant, or module item

    // Handle enum variants (e.g., Ordering::Less, Color::Red)
    if (path.path.segments.size() >= 2) {
        std::string enum_name = path.path.segments[path.path.segments.size() - 2];
        std::string variant_name = path.path.segments.back();

        // Look up the enum in the type environment
        if (auto enum_def = env_.lookup_enum(enum_name)) {
            // Find the variant index
            int variant_index = -1;
            for (size_t i = 0; i < enum_def->variants.size(); ++i) {
                if (enum_def->variants[i].first == variant_name) {
                    variant_index = static_cast<int>(i);
                    break;
                }
            }

            if (variant_index >= 0) {
                // Create an enum value with the variant tag
                EnumInitInst inst;
                inst.enum_name = enum_name;
                inst.variant_name = variant_name;
                inst.variant_index = variant_index;
                // No payload for unit variants like Ordering::Less
                inst.payload = {};
                inst.payload_types = {};

                MirTypePtr result_type = make_enum_type(enum_name);
                return emit(inst, result_type);
            }
        }
    }

    // Fall back to variable lookup for single-segment paths
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

auto MirBuilder::build_closure(const parser::ClosureExpr& closure) -> Value {
    // Closure lowering strategy:
    // 1. Create a synthetic function for the closure body
    // 2. Create a struct to hold captured variables
    // 3. Return a function pointer + capture struct (fat pointer)

    // Generate unique name for the closure function
    static int closure_counter = 0;
    std::string closure_name = "__closure_" + std::to_string(closure_counter++);

    // Build the function type
    std::vector<MirTypePtr> param_types;
    for (const auto& [pattern, type] : closure.params) {
        if (type.has_value()) {
            param_types.push_back(convert_type(**type));
        } else {
            param_types.push_back(make_i32_type()); // Inferred as i32 for now
        }
    }

    MirTypePtr return_type =
        closure.return_type.has_value() ? convert_type(**closure.return_type) : make_unit_type();

    // Create the closure function in the module
    Function closure_func;
    closure_func.name = closure_name;
    closure_func.return_type = return_type;

    // Add parameters
    ValueId value_id = 0;
    for (size_t i = 0; i < closure.params.size(); ++i) {
        const auto& [pattern, type] = closure.params[i];
        MirTypePtr param_type = param_types[i];

        std::string param_name;
        if (pattern->is<parser::IdentPattern>()) {
            param_name = pattern->as<parser::IdentPattern>().name;
        } else {
            param_name = "_arg" + std::to_string(i);
        }

        closure_func.params.push_back({param_name, param_type, value_id});
        value_id++;
    }

    // Save current context
    auto saved_func = ctx_.current_func;
    auto saved_block = ctx_.current_block;
    auto saved_vars = ctx_.variables;

    // Build closure body
    closure_func.blocks.emplace_back();
    closure_func.blocks[0].id = 0;
    closure_func.blocks[0].name = "entry";
    closure_func.next_block_id = 1;
    closure_func.next_value_id = value_id;

    ctx_.current_func = &closure_func;
    ctx_.current_block = 0;
    ctx_.variables.clear();

    // Bind parameters to variables
    for (size_t i = 0; i < closure.params.size(); ++i) {
        const auto& [pattern, type] = closure.params[i];
        if (pattern->is<parser::IdentPattern>()) {
            const auto& ident = pattern->as<parser::IdentPattern>();
            Value param_val;
            param_val.id = closure_func.params[i].value_id;
            param_val.type = param_types[i];
            ctx_.variables[ident.name] = param_val;
        }
    }

    // Build the body expression
    auto body_val = build_expr(*closure.body);

    // Add return if not already terminated
    if (!is_terminated()) {
        if (!return_type->is_unit()) {
            emit_return(body_val);
        } else {
            emit_return();
        }
    }

    // Restore context
    ctx_.current_func = saved_func;
    ctx_.current_block = saved_block;
    ctx_.variables = saved_vars;

    // Add closure function to module
    module_.functions.push_back(std::move(closure_func));

    // Create function type for the closure
    auto func_type = std::make_shared<MirType>();
    func_type->kind = MirFunctionType{param_types, return_type};

    // Return a pointer to the closure function
    // In a full implementation, this would also include captured variables
    return const_unit(); // For now, return unit; full impl would return function pointer
}

auto MirBuilder::build_await(const parser::AwaitExpr& await_expr) -> Value {
    // Build the expression being awaited (should return Poll[T])
    Value poll_value = build_expr(*await_expr.expr);

    // Determine the inner type (T from Poll[T])
    MirTypePtr inner_type = make_i64_type(); // Default

    if (auto* enum_type = std::get_if<MirEnumType>(&poll_value.type->kind)) {
        if (enum_type->name == "Poll" && !enum_type->type_args.empty()) {
            inner_type = enum_type->type_args[0];
        }
    }

    // Create the await instruction with a unique suspension ID
    AwaitInst inst;
    inst.poll_value = poll_value;
    inst.poll_type = poll_value.type;
    inst.result_type = inner_type;
    inst.suspension_id = ctx_.next_suspension_id++;

    // The await instruction marks a potential suspension point
    // The async lowering pass will later transform this into proper state machine code
    return emit(std::move(inst), inner_type);
}

} // namespace tml::mir
