//! # THIR to MIR Builder Implementation
//!
//! This file contains the ThirMirBuilder implementation.
//! Module/function building and type conversion are here.
//! Expression and statement lowering follow the same pattern as HirMirBuilder
//! but additionally handle ThirCoercionExpr nodes.
//!
//! Since ThirMirBuilder is structurally nearly identical to HirMirBuilder
//! (the only difference being coercion nodes and resolved dispatch),
//! the implementation closely mirrors the HIR version.

#include "mir/thir_mir_builder.hpp"

#include <stdexcept>

namespace tml::mir {

// ============================================================================
// Constructor
// ============================================================================

ThirMirBuilder::ThirMirBuilder(const types::TypeEnv& env) : env_(env), module_(), ctx_() {}

// ============================================================================
// Main Entry Point
// ============================================================================

auto ThirMirBuilder::build(const thir::ThirModule& thir_module) -> Module {
    module_.name = thir_module.name;
    build_declarations(thir_module);
    return std::move(module_);
}

// ============================================================================
// Declaration Building
// ============================================================================

void ThirMirBuilder::build_declarations(const thir::ThirModule& thir_module) {
    for (const auto& s : thir_module.structs) {
        build_struct(s);
    }

    for (const auto& e : thir_module.enums) {
        build_enum(e);
    }

    for (const auto& impl : thir_module.impls) {
        build_impl(impl);
    }

    for (const auto& f : thir_module.functions) {
        build_function(f);
    }
}

void ThirMirBuilder::build_struct(const thir::ThirStruct& s) {
    StructDef mir_struct;
    mir_struct.name = s.mangled_name.empty() ? s.name : s.mangled_name;

    for (const auto& field : s.fields) {
        StructField f;
        f.name = field.name;
        f.type = convert_type(field.type);
        mir_struct.fields.push_back(std::move(f));
    }

    module_.structs.push_back(std::move(mir_struct));
}

void ThirMirBuilder::build_enum(const thir::ThirEnum& e) {
    EnumDef mir_enum;
    mir_enum.name = e.mangled_name.empty() ? e.name : e.mangled_name;

    for (const auto& variant : e.variants) {
        EnumVariant v;
        v.name = variant.name;
        for (const auto& pt : variant.payload_types) {
            v.payload_types.push_back(convert_type(pt));
        }
        mir_enum.variants.push_back(std::move(v));
    }

    module_.enums.push_back(std::move(mir_enum));
}

void ThirMirBuilder::build_impl(const thir::ThirImpl& impl) {
    for (const auto& method : impl.methods) {
        build_function(method);
    }
}

void ThirMirBuilder::build_function(const thir::ThirFunction& func) {
    Function mir_func;
    mir_func.name = func.mangled_name.empty() ? func.name : func.mangled_name;
    mir_func.return_type = convert_type(func.return_type);
    mir_func.is_public = func.is_public;

    // Add parameters
    for (const auto& param : func.params) {
        FunctionParam p;
        p.name = param.name;
        p.type = convert_type(param.type);
        p.value_id = mir_func.fresh_value();
        mir_func.params.push_back(std::move(p));
    }

    // Extern functions have no body
    if (func.is_extern || !func.body) {
        module_.functions.push_back(std::move(mir_func));
        return;
    }

    // Set up build context for this function
    module_.functions.push_back(std::move(mir_func));
    ctx_.current_func = &module_.functions.back();
    ctx_.variables.clear();
    ctx_.drop_scopes.clear();
    // Clear the loop stack (std::stack has no clear())
    while (!ctx_.loop_stack.empty()) {
        ctx_.loop_stack.pop();
    }

    // Create entry block
    auto entry_block = create_block("entry");
    switch_to_block(entry_block);

    // Create parameter variables
    for (size_t i = 0; i < func.params.size(); ++i) {
        const auto& param = func.params[i];
        auto param_type = convert_type(param.type);
        Value param_val{ctx_.current_func->params[i].value_id, param_type};
        set_variable(param.name, param_val);
    }

    // Build the function body
    if (func.body) {
        auto result = build_expr(*func.body);

        // If the block isn't terminated, add a return
        if (!is_terminated()) {
            if (func.return_type && func.return_type->is<types::PrimitiveType>() &&
                func.return_type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Unit) {
                emit_return();
            } else {
                emit_return(result);
            }
        }
    }

    ctx_.current_func = nullptr;
}

// ============================================================================
// Type Conversion
// ============================================================================

auto ThirMirBuilder::convert_type(const thir::ThirType& type) -> MirTypePtr {
    if (!type)
        return make_unit_type();

    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        switch (prim.kind) {
        case types::PrimitiveKind::Bool:
            return make_bool_type();
        case types::PrimitiveKind::I8:
            return make_i8_type();
        case types::PrimitiveKind::I16:
            return make_i16_type();
        case types::PrimitiveKind::I32:
            return make_i32_type();
        case types::PrimitiveKind::I64:
            return make_i64_type();
        case types::PrimitiveKind::I128:
            return make_i64_type(); // Approximate
        case types::PrimitiveKind::U8:
            return make_i8_type(); // Use same size
        case types::PrimitiveKind::U16:
            return make_i16_type();
        case types::PrimitiveKind::U32:
            return make_i32_type();
        case types::PrimitiveKind::U64:
            return make_i64_type();
        case types::PrimitiveKind::U128:
            return make_i64_type();
        case types::PrimitiveKind::F32:
            return make_f32_type();
        case types::PrimitiveKind::F64:
            return make_f64_type();
        case types::PrimitiveKind::Char:
            return make_i32_type();
        case types::PrimitiveKind::Str:
            return make_str_type();
        case types::PrimitiveKind::Unit:
            return make_unit_type();
        case types::PrimitiveKind::Never:
            return make_unit_type();
        default:
            return make_unit_type();
        }
    }

    if (type->is<types::RefType>()) {
        const auto& ref = type->as<types::RefType>();
        auto inner = convert_type(ref.inner);
        return make_pointer_type(std::move(inner), ref.is_mut);
    }

    if (type->is<types::PtrType>()) {
        const auto& ptr = type->as<types::PtrType>();
        auto inner = convert_type(ptr.inner);
        return make_pointer_type(std::move(inner), ptr.is_mut);
    }

    if (type->is<types::ArrayType>()) {
        const auto& arr = type->as<types::ArrayType>();
        auto elem = convert_type(arr.element);
        return make_array_type(std::move(elem), arr.size);
    }

    if (type->is<types::TupleType>()) {
        const auto& tuple = type->as<types::TupleType>();
        std::vector<MirTypePtr> elems;
        for (const auto& e : tuple.elements) {
            elems.push_back(convert_type(e));
        }
        return make_tuple_type(std::move(elems));
    }

    if (type->is<types::FuncType>()) {
        const auto& func = type->as<types::FuncType>();
        std::vector<MirTypePtr> params;
        for (const auto& p : func.params) {
            params.push_back(convert_type(p));
        }
        auto ret = convert_type(func.return_type);
        auto mir_type = std::make_shared<MirType>();
        mir_type->kind = MirFunctionType{std::move(params), std::move(ret)};
        return mir_type;
    }

    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        // Check if it's an enum or struct
        if (env_.lookup_enum(named.name)) {
            return make_enum_type(named.name);
        }
        return make_struct_type(named.name);
    }

    return make_unit_type();
}

// ============================================================================
// Expression Building
// ============================================================================

auto ThirMirBuilder::build_expr(const thir::ThirExprPtr& expr) -> Value {
    if (!expr)
        return const_unit();

    return std::visit(
        [this](const auto& e) -> Value {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, thir::ThirLiteralExpr>) {
                return build_literal(e);
            } else if constexpr (std::is_same_v<T, thir::ThirVarExpr>) {
                return build_var(e);
            } else if constexpr (std::is_same_v<T, thir::ThirBinaryExpr>) {
                return build_binary(e);
            } else if constexpr (std::is_same_v<T, thir::ThirUnaryExpr>) {
                return build_unary(e);
            } else if constexpr (std::is_same_v<T, thir::ThirCallExpr>) {
                return build_call(e);
            } else if constexpr (std::is_same_v<T, thir::ThirMethodCallExpr>) {
                return build_method_call(e);
            } else if constexpr (std::is_same_v<T, thir::ThirFieldExpr>) {
                return build_field(e);
            } else if constexpr (std::is_same_v<T, thir::ThirIndexExpr>) {
                return build_index(e);
            } else if constexpr (std::is_same_v<T, thir::ThirTupleExpr>) {
                return build_tuple(e);
            } else if constexpr (std::is_same_v<T, thir::ThirArrayExpr>) {
                return build_array(e);
            } else if constexpr (std::is_same_v<T, thir::ThirArrayRepeatExpr>) {
                return build_array_repeat(e);
            } else if constexpr (std::is_same_v<T, thir::ThirStructExpr>) {
                return build_struct_expr(e);
            } else if constexpr (std::is_same_v<T, thir::ThirEnumExpr>) {
                return build_enum_expr(e);
            } else if constexpr (std::is_same_v<T, thir::ThirBlockExpr>) {
                return build_block(e);
            } else if constexpr (std::is_same_v<T, thir::ThirIfExpr>) {
                return build_if(e);
            } else if constexpr (std::is_same_v<T, thir::ThirWhenExpr>) {
                return build_when(e);
            } else if constexpr (std::is_same_v<T, thir::ThirLoopExpr>) {
                return build_loop(e);
            } else if constexpr (std::is_same_v<T, thir::ThirWhileExpr>) {
                return build_while(e);
            } else if constexpr (std::is_same_v<T, thir::ThirForExpr>) {
                return build_for(e);
            } else if constexpr (std::is_same_v<T, thir::ThirReturnExpr>) {
                return build_return(e);
            } else if constexpr (std::is_same_v<T, thir::ThirBreakExpr>) {
                return build_break(e);
            } else if constexpr (std::is_same_v<T, thir::ThirContinueExpr>) {
                return build_continue(e);
            } else if constexpr (std::is_same_v<T, thir::ThirClosureExpr>) {
                return build_closure(e);
            } else if constexpr (std::is_same_v<T, thir::ThirCastExpr>) {
                return build_cast(e);
            } else if constexpr (std::is_same_v<T, thir::ThirTryExpr>) {
                return build_try(e);
            } else if constexpr (std::is_same_v<T, thir::ThirAwaitExpr>) {
                return build_await(e);
            } else if constexpr (std::is_same_v<T, thir::ThirAssignExpr>) {
                return build_assign(e);
            } else if constexpr (std::is_same_v<T, thir::ThirCompoundAssignExpr>) {
                return build_compound_assign(e);
            } else if constexpr (std::is_same_v<T, thir::ThirLowlevelExpr>) {
                return build_lowlevel(e);
            } else if constexpr (std::is_same_v<T, thir::ThirCoercionExpr>) {
                return build_coercion(e);
            } else {
                return const_unit();
            }
        },
        expr->kind);
}

// ============================================================================
// Coercion Building — THIR-specific
// ============================================================================

auto ThirMirBuilder::build_coercion(const thir::ThirCoercionExpr& coerce) -> Value {
    auto inner = build_expr(coerce.inner);
    auto target_type = convert_type(coerce.type);

    switch (coerce.coercion) {
    case thir::CoercionKind::IntWidening: {
        CastInst inst;
        inst.kind = CastKind::SExt;
        inst.operand = inner;
        inst.source_type = inner.type;
        inst.target_type = target_type;
        return emit(std::move(inst), target_type, coerce.span);
    }
    case thir::CoercionKind::UintWidening: {
        CastInst inst;
        inst.kind = CastKind::ZExt;
        inst.operand = inner;
        inst.source_type = inner.type;
        inst.target_type = target_type;
        return emit(std::move(inst), target_type, coerce.span);
    }
    case thir::CoercionKind::FloatWidening: {
        CastInst inst;
        inst.kind = CastKind::FPExt;
        inst.operand = inner;
        inst.source_type = inner.type;
        inst.target_type = target_type;
        return emit(std::move(inst), target_type, coerce.span);
    }
    case thir::CoercionKind::IntToFloat: {
        auto src_type = convert_type(coerce.source_type);
        bool is_signed = src_type && src_type->is_signed();
        CastInst inst;
        inst.kind = is_signed ? CastKind::SIToFP : CastKind::UIToFP;
        inst.operand = inner;
        inst.source_type = src_type;
        inst.target_type = target_type;
        return emit(std::move(inst), target_type, coerce.span);
    }
    case thir::CoercionKind::DerefCoercion: {
        LoadInst inst;
        inst.ptr = inner;
        inst.result_type = target_type;
        return emit(std::move(inst), target_type, coerce.span);
    }
    case thir::CoercionKind::RefCoercion: {
        auto ptr_type = make_pointer_type(inner.type, false);
        AllocaInst alloca_inst;
        alloca_inst.alloc_type = inner.type;
        auto alloca_val = emit_at_entry(std::move(alloca_inst), ptr_type);

        StoreInst store;
        store.ptr = alloca_val;
        store.value = inner;
        store.value_type = inner.type;
        emit_void(std::move(store), coerce.span);
        return alloca_val;
    }
    case thir::CoercionKind::MutToShared: {
        return inner;
    }
    case thir::CoercionKind::NeverCoercion: {
        emit_unreachable();
        return inner;
    }
    case thir::CoercionKind::UnsizeCoercion: {
        return inner;
    }
    }

    return inner;
}

// ============================================================================
// Statement Building
// ============================================================================

auto ThirMirBuilder::build_stmt(const thir::ThirStmt& stmt) -> bool {
    if (stmt.is<thir::ThirLetStmt>()) {
        build_let_stmt(stmt.as<thir::ThirLetStmt>());
        return false;
    }
    if (stmt.is<thir::ThirExprStmt>()) {
        build_expr_stmt(stmt.as<thir::ThirExprStmt>());
        return is_terminated();
    }
    return false;
}

void ThirMirBuilder::build_let_stmt(const thir::ThirLetStmt& let) {
    if (let.init) {
        auto init_val = build_expr(*let.init);
        build_pattern_binding(let.pattern, init_val);
    }
}

void ThirMirBuilder::build_expr_stmt(const thir::ThirExprStmt& expr) {
    (void)build_expr(expr.expr);
}

// ============================================================================
// Concrete Expression Building
// ============================================================================

auto ThirMirBuilder::build_literal(const thir::ThirLiteralExpr& lit) -> Value {
    return std::visit(
        [this, &lit](const auto& v) -> Value {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                auto type = convert_type(lit.type);
                int bits = 32;
                bool is_signed = true;
                if (type) {
                    auto* prim = std::get_if<MirPrimitiveType>(&type->kind);
                    if (prim) {
                        switch (prim->kind) {
                        case PrimitiveType::I8:
                        case PrimitiveType::U8:
                            bits = 8;
                            break;
                        case PrimitiveType::I16:
                        case PrimitiveType::U16:
                            bits = 16;
                            break;
                        case PrimitiveType::I64:
                        case PrimitiveType::U64:
                            bits = 64;
                            break;
                        default:
                            break;
                        }
                        is_signed =
                            (prim->kind == PrimitiveType::I8 || prim->kind == PrimitiveType::I16 ||
                             prim->kind == PrimitiveType::I32 || prim->kind == PrimitiveType::I64 ||
                             prim->kind == PrimitiveType::I128);
                    }
                }
                return const_int(v, bits, is_signed);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                auto type = convert_type(lit.type);
                int bits = 32;
                if (type) {
                    auto* prim = std::get_if<MirPrimitiveType>(&type->kind);
                    if (prim) {
                        switch (prim->kind) {
                        case PrimitiveType::U8:
                            bits = 8;
                            break;
                        case PrimitiveType::U16:
                            bits = 16;
                            break;
                        case PrimitiveType::U64:
                            bits = 64;
                            break;
                        default:
                            break;
                        }
                    }
                }
                return const_int(static_cast<int64_t>(v), bits, false);
            } else if constexpr (std::is_same_v<T, double>) {
                auto type = convert_type(lit.type);
                bool is_f64 = false;
                if (type) {
                    auto* prim = std::get_if<MirPrimitiveType>(&type->kind);
                    if (prim) {
                        is_f64 = (prim->kind == PrimitiveType::F64);
                    }
                }
                return const_float(v, is_f64);
            } else if constexpr (std::is_same_v<T, bool>) {
                return const_bool(v);
            } else if constexpr (std::is_same_v<T, char>) {
                return const_int(static_cast<int64_t>(v), 32, false);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return const_string(v);
            } else {
                return const_unit();
            }
        },
        lit.value);
}

auto ThirMirBuilder::build_var(const thir::ThirVarExpr& var) -> Value {
    return get_variable(var.name);
}

auto ThirMirBuilder::build_binary(const thir::ThirBinaryExpr& bin) -> Value {
    // If operator overloading is resolved, emit as method call
    if (bin.operator_method) {
        auto left = build_expr(bin.left);
        auto right = build_expr(bin.right);
        auto result_type = convert_type(bin.type);

        CallInst inst;
        inst.func_name = bin.operator_method->qualified_name;
        inst.args = {left, right};
        inst.arg_types = {left.type, right.type};
        inst.return_type = result_type;
        return emit(std::move(inst), result_type, bin.span);
    }

    auto left = build_expr(bin.left);
    auto right = build_expr(bin.right);
    auto result_type = convert_type(bin.type);

    if (is_comparison_op(bin.op)) {
        auto cmp_op = convert_binop(bin.op);
        BinaryInst inst;
        inst.op = cmp_op;
        inst.left = left;
        inst.right = right;
        inst.result_type = make_bool_type();
        return emit(std::move(inst), make_bool_type(), bin.span);
    }

    auto op = convert_binop(bin.op);
    BinaryInst inst;
    inst.op = op;
    inst.left = left;
    inst.right = right;
    inst.result_type = result_type;
    return emit(std::move(inst), result_type, bin.span);
}

auto ThirMirBuilder::build_unary(const thir::ThirUnaryExpr& unary) -> Value {
    auto operand = build_expr(unary.operand);
    auto result_type = convert_type(unary.type);

    // Handle ref/deref specially
    if (unary.op == hir::HirUnaryOp::Ref || unary.op == hir::HirUnaryOp::RefMut) {
        bool is_mut = (unary.op == hir::HirUnaryOp::RefMut);
        auto ptr_type = make_pointer_type(operand.type, is_mut);

        AllocaInst alloca_inst;
        alloca_inst.alloc_type = operand.type;
        Value ptr = emit_at_entry(std::move(alloca_inst), ptr_type);

        StoreInst store;
        store.ptr = ptr;
        store.value = operand;
        store.value_type = operand.type;
        emit_void(std::move(store), unary.span);

        return ptr;
    }

    if (unary.op == hir::HirUnaryOp::Deref) {
        LoadInst load;
        load.ptr = operand;
        load.result_type = result_type;
        return emit(std::move(load), result_type, unary.span);
    }

    auto op = convert_unaryop(unary.op);
    UnaryInst inst;
    inst.op = op;
    inst.operand = operand;
    inst.result_type = result_type;
    return emit(std::move(inst), result_type, unary.span);
}

auto ThirMirBuilder::build_call(const thir::ThirCallExpr& call) -> Value {
    std::vector<Value> args;
    std::vector<MirTypePtr> arg_types;
    for (const auto& arg : call.args) {
        auto val = build_expr(arg);
        args.push_back(val);
        arg_types.push_back(val.type);
    }
    auto result_type = convert_type(call.type);

    CallInst inst;
    inst.func_name = call.func_name;
    inst.args = std::move(args);
    inst.arg_types = std::move(arg_types);
    inst.return_type = result_type;
    return emit(std::move(inst), result_type, call.span);
}

auto ThirMirBuilder::build_method_call(const thir::ThirMethodCallExpr& call) -> Value {
    auto receiver = build_expr(call.receiver);
    std::vector<Value> args = {receiver};
    std::vector<MirTypePtr> arg_types = {receiver.type};
    for (const auto& arg : call.args) {
        auto val = build_expr(arg);
        args.push_back(val);
        arg_types.push_back(val.type);
    }
    auto result_type = convert_type(call.type);

    CallInst inst;
    inst.func_name = call.resolved.qualified_name;
    inst.args = std::move(args);
    inst.arg_types = std::move(arg_types);
    inst.return_type = result_type;
    return emit(std::move(inst), result_type, call.span);
}

auto ThirMirBuilder::build_field(const thir::ThirFieldExpr& field) -> Value {
    auto object = build_expr(field.object);
    auto result_type = convert_type(field.type);

    // Handle pointer types (auto-deref)
    Value aggregate = object;
    MirTypePtr aggregate_type = object.type;

    if (aggregate_type && std::holds_alternative<MirPointerType>(aggregate_type->kind)) {
        const auto& ptr_type = std::get<MirPointerType>(aggregate_type->kind);
        LoadInst load;
        load.ptr = object;
        load.result_type = ptr_type.pointee;
        aggregate = emit(std::move(load), ptr_type.pointee, field.span);
        aggregate_type = ptr_type.pointee;
    }

    ExtractValueInst inst;
    inst.aggregate = aggregate;
    inst.indices = {static_cast<uint32_t>(field.field_index)};
    inst.aggregate_type = aggregate_type;
    inst.result_type = result_type;
    return emit(std::move(inst), result_type, field.span);
}

auto ThirMirBuilder::build_index(const thir::ThirIndexExpr& index) -> Value {
    auto object = build_expr(index.object);
    auto idx = build_expr(index.index);
    auto result_type = convert_type(index.type);

    GetElementPtrInst gep;
    gep.base = object;
    gep.indices = {idx};
    gep.base_type = object.type;
    gep.result_type = make_pointer_type(result_type, false);

    if (object.type) {
        if (auto* arr_type = std::get_if<MirArrayType>(&object.type->kind)) {
            gep.known_array_size = static_cast<int64_t>(arr_type->size);
        }
    }

    Value ptr = emit(std::move(gep), gep.result_type, index.span);

    LoadInst load;
    load.ptr = ptr;
    load.result_type = result_type;
    return emit(std::move(load), result_type, index.span);
}

auto ThirMirBuilder::build_if(const thir::ThirIfExpr& if_expr) -> Value {
    auto cond = build_expr(if_expr.condition);
    auto result_type = convert_type(if_expr.type);

    auto then_block = create_block("if.then");
    auto else_block = create_block("if.else");
    auto merge_block = create_block("if.merge");

    emit_cond_branch(cond, then_block, else_block);

    switch_to_block(then_block);
    auto then_val = build_expr(if_expr.then_branch);
    if (!is_terminated()) {
        emit_branch(merge_block);
    }

    switch_to_block(else_block);
    Value else_val = const_unit();
    if (if_expr.else_branch) {
        else_val = build_expr(*if_expr.else_branch);
    }
    if (!is_terminated()) {
        emit_branch(merge_block);
    }

    switch_to_block(merge_block);
    return then_val;
}

auto ThirMirBuilder::build_block(const thir::ThirBlockExpr& block) -> Value {
    for (const auto& stmt : block.stmts) {
        if (build_stmt(*stmt)) {
            return const_unit();
        }
    }

    if (block.expr) {
        return build_expr(*block.expr);
    }

    return const_unit();
}

auto ThirMirBuilder::build_loop(const thir::ThirLoopExpr& loop) -> Value {
    auto header = create_block("loop.header");
    auto body = create_block("loop.body");
    auto exit = create_block("loop.exit");

    BuildContext::LoopContext lc;
    lc.header_block = header;
    lc.exit_block = exit;
    ctx_.loop_stack.push(std::move(lc));

    emit_branch(header);
    switch_to_block(header);

    auto cond = build_expr(loop.condition);
    emit_cond_branch(cond, body, exit);

    switch_to_block(body);
    (void)build_expr(loop.body);
    if (!is_terminated()) {
        emit_branch(header);
    }

    ctx_.loop_stack.pop();
    switch_to_block(exit);
    return const_unit();
}

auto ThirMirBuilder::build_while(const thir::ThirWhileExpr& while_expr) -> Value {
    auto header = create_block("while.header");
    auto body = create_block("while.body");
    auto exit = create_block("while.exit");

    BuildContext::LoopContext lc;
    lc.header_block = header;
    lc.exit_block = exit;
    ctx_.loop_stack.push(std::move(lc));

    emit_branch(header);
    switch_to_block(header);

    auto cond = build_expr(while_expr.condition);
    emit_cond_branch(cond, body, exit);

    switch_to_block(body);
    (void)build_expr(while_expr.body);
    if (!is_terminated()) {
        emit_branch(header);
    }

    ctx_.loop_stack.pop();
    switch_to_block(exit);
    return const_unit();
}

auto ThirMirBuilder::build_for(const thir::ThirForExpr& for_expr) -> Value {
    auto iter_val = build_expr(for_expr.iter);

    auto header = create_block("for.header");
    auto body = create_block("for.body");
    auto exit = create_block("for.exit");

    BuildContext::LoopContext lc;
    lc.header_block = header;
    lc.exit_block = exit;
    ctx_.loop_stack.push(std::move(lc));

    emit_branch(header);
    switch_to_block(header);

    auto result_type = convert_type(for_expr.type);
    CallInst next_inst;
    next_inst.func_name = "next";
    next_inst.args = {iter_val};
    next_inst.arg_types = {iter_val.type};
    next_inst.return_type = result_type;
    auto next_val = emit(std::move(next_inst), result_type);

    // Simplified: check for value
    auto has_value = const_bool(true);
    emit_cond_branch(has_value, body, exit);

    switch_to_block(body);
    build_pattern_binding(for_expr.pattern, next_val);
    (void)build_expr(for_expr.body);
    if (!is_terminated()) {
        emit_branch(header);
    }

    ctx_.loop_stack.pop();
    switch_to_block(exit);
    return const_unit();
}

auto ThirMirBuilder::build_return(const thir::ThirReturnExpr& ret) -> Value {
    if (ret.value) {
        auto val = build_expr(*ret.value);
        emit_return(val);
    } else {
        emit_return();
    }
    return const_unit();
}

auto ThirMirBuilder::build_break(const thir::ThirBreakExpr& /*brk*/) -> Value {
    if (!ctx_.loop_stack.empty()) {
        emit_branch(ctx_.loop_stack.top().exit_block);
    }
    return const_unit();
}

auto ThirMirBuilder::build_continue(const thir::ThirContinueExpr& /*cont*/) -> Value {
    if (!ctx_.loop_stack.empty()) {
        emit_branch(ctx_.loop_stack.top().header_block);
    }
    return const_unit();
}

auto ThirMirBuilder::build_when(const thir::ThirWhenExpr& when) -> Value {
    auto scrutinee = build_expr(when.scrutinee);
    auto result_type = convert_type(when.type);
    auto merge_block = create_block("when.merge");

    Value result = const_unit();

    for (size_t i = 0; i < when.arms.size(); ++i) {
        const auto& arm = when.arms[i];
        auto match_block = create_block("when.arm." + std::to_string(i));
        auto next_block = (i + 1 < when.arms.size())
                              ? create_block("when.next." + std::to_string(i))
                              : merge_block;

        auto cond = build_pattern_match(arm.pattern, scrutinee);

        if (arm.guard) {
            auto guard_block = create_block("when.guard." + std::to_string(i));
            emit_cond_branch(cond, guard_block, next_block);
            switch_to_block(guard_block);
            auto guard_cond = build_expr(*arm.guard);
            emit_cond_branch(guard_cond, match_block, next_block);
        } else {
            emit_cond_branch(cond, match_block, next_block);
        }

        switch_to_block(match_block);
        build_pattern_binding(arm.pattern, scrutinee);
        result = build_expr(arm.body);
        if (!is_terminated()) {
            emit_branch(merge_block);
        }

        if (i + 1 < when.arms.size()) {
            switch_to_block(next_block);
        }
    }

    switch_to_block(merge_block);
    return result;
}

auto ThirMirBuilder::build_struct_expr(const thir::ThirStructExpr& s) -> Value {
    std::vector<Value> fields;
    std::vector<MirTypePtr> field_types;

    for (const auto& field_pair : s.fields) {
        auto val = build_expr(field_pair.second);
        fields.push_back(val);
        field_types.push_back(val.type);
    }

    auto result_type = convert_type(s.type);

    StructInitInst inst;
    inst.struct_name = s.struct_name;
    inst.fields = std::move(fields);
    inst.field_types = std::move(field_types);
    return emit(std::move(inst), result_type, s.span);
}

auto ThirMirBuilder::build_enum_expr(const thir::ThirEnumExpr& e) -> Value {
    std::vector<Value> payload;
    std::vector<MirTypePtr> payload_types;

    for (const auto& p : e.payload) {
        auto val = build_expr(p);
        payload.push_back(val);
        payload_types.push_back(val.type);
    }

    auto result_type = convert_type(e.type);

    EnumInitInst inst;
    inst.enum_name = e.enum_name;
    inst.variant_name = e.variant_name;
    inst.variant_index = static_cast<int>(e.variant_index);
    inst.payload = std::move(payload);
    inst.payload_types = std::move(payload_types);
    return emit(std::move(inst), result_type, e.span);
}

auto ThirMirBuilder::build_tuple(const thir::ThirTupleExpr& tuple) -> Value {
    std::vector<Value> elements;
    std::vector<MirTypePtr> element_types;

    for (const auto& elem : tuple.elements) {
        auto val = build_expr(elem);
        elements.push_back(val);
        element_types.push_back(val.type);
    }

    auto result_type = make_tuple_type(element_types);

    TupleInitInst inst;
    inst.elements = std::move(elements);
    inst.element_types = std::move(element_types);
    inst.result_type = result_type;
    return emit(std::move(inst), result_type, tuple.span);
}

auto ThirMirBuilder::build_array(const thir::ThirArrayExpr& arr) -> Value {
    std::vector<Value> elements;
    for (const auto& elem : arr.elements) {
        elements.push_back(build_expr(elem));
    }

    MirTypePtr element_type = elements.empty() ? make_unit_type() : elements[0].type;
    MirTypePtr result_type = make_array_type(element_type, elements.size());

    ArrayInitInst inst;
    inst.elements = std::move(elements);
    inst.element_type = element_type;
    inst.result_type = result_type;
    return emit(std::move(inst), result_type, arr.span);
}

auto ThirMirBuilder::build_array_repeat(const thir::ThirArrayRepeatExpr& arr) -> Value {
    auto val = build_expr(arr.value);

    MirTypePtr element_type = val.type;
    MirTypePtr result_type = make_array_type(element_type, arr.count);

    // Replicate the value into an array
    std::vector<Value> elements(arr.count, val);

    ArrayInitInst inst;
    inst.elements = std::move(elements);
    inst.element_type = element_type;
    inst.result_type = result_type;
    return emit(std::move(inst), result_type, arr.span);
}

auto ThirMirBuilder::build_cast(const thir::ThirCastExpr& cast) -> Value {
    auto val = build_expr(cast.expr);
    auto result_type = convert_type(cast.type);

    CastInst inst;
    inst.kind = CastKind::Bitcast;
    inst.operand = val;
    inst.source_type = val.type;
    inst.target_type = result_type;
    return emit(std::move(inst), result_type, cast.span);
}

auto ThirMirBuilder::build_closure(const thir::ThirClosureExpr& /*closure*/) -> Value {
    return const_unit();
}

auto ThirMirBuilder::build_try(const thir::ThirTryExpr& try_expr) -> Value {
    auto val = build_expr(try_expr.expr);
    return val;
}

auto ThirMirBuilder::build_await(const thir::ThirAwaitExpr& await_expr) -> Value {
    auto val = build_expr(await_expr.expr);
    return val;
}

auto ThirMirBuilder::build_assign(const thir::ThirAssignExpr& assign) -> Value {
    auto value = build_expr(assign.value);

    // For simple variable targets, use SSA-style set_variable (no alloca/store)
    if (assign.target->is<thir::ThirVarExpr>()) {
        const auto& var = assign.target->as<thir::ThirVarExpr>();
        set_variable(var.name, value);
        return const_unit();
    }

    // For non-variable targets (field access, index, etc.), use memory store
    auto target = build_expr(assign.target);
    StoreInst store;
    store.ptr = target;
    store.value = value;
    store.value_type = value.type;
    emit_void(std::move(store), assign.span);
    return const_unit();
}

auto ThirMirBuilder::build_compound_assign(const thir::ThirCompoundAssignExpr& assign) -> Value {
    if (assign.operator_method) {
        auto target = build_expr(assign.target);
        auto value = build_expr(assign.value);
        auto result_type = convert_type(assign.target->type());

        CallInst call;
        call.func_name = assign.operator_method->qualified_name;
        call.args = {target, value};
        call.arg_types = {target.type, value.type};
        call.return_type = result_type;
        auto result = emit(std::move(call), result_type);

        // For simple variable targets, use SSA-style set_variable
        if (assign.target->is<thir::ThirVarExpr>()) {
            const auto& var = assign.target->as<thir::ThirVarExpr>();
            set_variable(var.name, result);
            return const_unit();
        }

        StoreInst store;
        store.ptr = target;
        store.value = result;
        store.value_type = result_type;
        emit_void(std::move(store), assign.span);
        return const_unit();
    }

    auto value = build_expr(assign.value);
    auto op = convert_compound_op(assign.op);
    auto result_type = convert_type(assign.target->type());

    // For simple variable targets, use SSA-style (no alloca/load/store)
    if (assign.target->is<thir::ThirVarExpr>()) {
        const auto& var = assign.target->as<thir::ThirVarExpr>();
        auto current = get_variable(var.name);

        BinaryInst bin;
        bin.op = op;
        bin.left = current;
        bin.right = value;
        bin.result_type = result_type;
        auto result = emit(std::move(bin), result_type);

        set_variable(var.name, result);
        return const_unit();
    }

    // For non-variable targets (field access, index, etc.), use memory load/store
    auto target = build_expr(assign.target);

    LoadInst load;
    load.ptr = target;
    load.result_type = result_type;
    auto current = emit(std::move(load), result_type);

    BinaryInst bin;
    bin.op = op;
    bin.left = current;
    bin.right = value;
    bin.result_type = result_type;
    auto result = emit(std::move(bin), result_type);

    StoreInst store;
    store.ptr = target;
    store.value = result;
    store.value_type = result_type;
    emit_void(std::move(store), assign.span);
    return const_unit();
}

auto ThirMirBuilder::build_lowlevel(const thir::ThirLowlevelExpr& lowlevel) -> Value {
    for (const auto& stmt : lowlevel.stmts) {
        if (build_stmt(*stmt)) {
            return const_unit();
        }
    }
    if (lowlevel.expr) {
        return build_expr(*lowlevel.expr);
    }
    return const_unit();
}

// ============================================================================
// Pattern Building
// ============================================================================

void ThirMirBuilder::build_pattern_binding(const thir::ThirPatternPtr& pattern, Value value) {
    if (!pattern)
        return;

    if (pattern->is<thir::ThirBindingPattern>()) {
        const auto& bp = pattern->as<thir::ThirBindingPattern>();
        set_variable(bp.name, value);
    } else if (pattern->is<thir::ThirWildcardPattern>()) {
        // Nothing to bind
    } else if (pattern->is<thir::ThirTuplePattern>()) {
        const auto& tp = pattern->as<thir::ThirTuplePattern>();
        for (size_t i = 0; i < tp.elements.size(); ++i) {
            auto elem_type = convert_type(tp.elements[i]->type());

            ExtractValueInst extract;
            extract.aggregate = value;
            extract.indices = {static_cast<uint32_t>(i)};
            extract.aggregate_type = value.type;
            extract.result_type = elem_type;
            auto elem = emit(std::move(extract), elem_type);

            build_pattern_binding(tp.elements[i], elem);
        }
    } else if (pattern->is<thir::ThirStructPattern>()) {
        const auto& sp = pattern->as<thir::ThirStructPattern>();
        for (size_t i = 0; i < sp.fields.size(); ++i) {
            auto field_type = convert_type(sp.fields[i].second->type());

            ExtractValueInst extract;
            extract.aggregate = value;
            extract.indices = {static_cast<uint32_t>(i)};
            extract.aggregate_type = value.type;
            extract.result_type = field_type;
            auto field_val = emit(std::move(extract), field_type);

            build_pattern_binding(sp.fields[i].second, field_val);
        }
    }
}

auto ThirMirBuilder::build_pattern_match(const thir::ThirPatternPtr& pattern, Value scrutinee)
    -> Value {
    if (!pattern)
        return const_bool(true);

    if (pattern->is<thir::ThirWildcardPattern>() || pattern->is<thir::ThirBindingPattern>()) {
        return const_bool(true);
    }

    if (pattern->is<thir::ThirLiteralPattern>()) {
        const auto& lp = pattern->as<thir::ThirLiteralPattern>();
        auto lit_val = std::visit(
            [this](const auto& v) -> Value {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, int64_t>) {
                    return const_int(v);
                } else if constexpr (std::is_same_v<T, uint64_t>) {
                    return const_int(static_cast<int64_t>(v), 32, false);
                } else if constexpr (std::is_same_v<T, bool>) {
                    return const_bool(v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return const_string(v);
                } else {
                    return const_unit();
                }
            },
            lp.value);

        BinaryInst cmp;
        cmp.op = BinOp::Eq;
        cmp.left = scrutinee;
        cmp.right = lit_val;
        cmp.result_type = make_bool_type();
        return emit(std::move(cmp), make_bool_type());
    }

    if (pattern->is<thir::ThirEnumPattern>()) {
        const auto& ep = pattern->as<thir::ThirEnumPattern>();

        // Extract discriminant (tag at index 0)
        ExtractValueInst extract_tag;
        extract_tag.aggregate = scrutinee;
        extract_tag.indices = {0};
        extract_tag.aggregate_type = scrutinee.type;
        extract_tag.result_type = make_i32_type();
        auto disc = emit(std::move(extract_tag), make_i32_type());

        auto expected = const_int(ep.variant_index);

        BinaryInst cmp;
        cmp.op = BinOp::Eq;
        cmp.left = disc;
        cmp.right = expected;
        cmp.result_type = make_bool_type();
        return emit(std::move(cmp), make_bool_type());
    }

    return const_bool(true);
}

// ============================================================================
// Helper Methods
// ============================================================================

auto ThirMirBuilder::create_block(const std::string& name) -> uint32_t {
    if (!ctx_.current_func) {
        throw std::runtime_error("ThirMirBuilder: No function being built");
    }
    return ctx_.current_func->create_block(name);
}

void ThirMirBuilder::switch_to_block(uint32_t block_id) {
    ctx_.current_block = block_id;
}

auto ThirMirBuilder::is_terminated() const -> bool {
    if (!ctx_.current_func)
        return true;
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    return block && block->terminator.has_value();
}

auto ThirMirBuilder::emit(Instruction inst, MirTypePtr type, SourceSpan span) -> Value {
    if (!ctx_.current_func) {
        throw std::runtime_error("ThirMirBuilder: No function being built");
    }
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block) {
        return {INVALID_VALUE, type};
    }

    auto id = ctx_.current_func->fresh_value();

    InstructionData data;
    data.result = id;
    data.type = type;
    data.inst = std::move(inst);
    data.span = span;
    block->instructions.push_back(std::move(data));

    return {id, type};
}

void ThirMirBuilder::emit_void(Instruction inst, SourceSpan span) {
    if (!ctx_.current_func)
        return;
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block)
        return;

    InstructionData data;
    data.result = INVALID_VALUE;
    data.type = make_unit_type();
    data.inst = std::move(inst);
    data.span = span;
    block->instructions.push_back(std::move(data));
}

auto ThirMirBuilder::emit_at_entry(Instruction inst, MirTypePtr type) -> Value {
    if (!ctx_.current_func) {
        throw std::runtime_error("ThirMirBuilder: No function being built");
    }
    auto& entry = ctx_.current_func->entry_block();

    auto id = ctx_.current_func->fresh_value();

    InstructionData data;
    data.result = id;
    data.type = type;
    data.inst = std::move(inst);
    entry.instructions.insert(entry.instructions.begin(), std::move(data));

    return {id, type};
}

void ThirMirBuilder::emit_return(std::optional<Value> value) {
    if (!ctx_.current_func)
        return;
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value())
        return;
    block->terminator = ReturnTerm{value};
}

void ThirMirBuilder::emit_branch(uint32_t target) {
    if (!ctx_.current_func)
        return;
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value())
        return;
    block->terminator = BranchTerm{target};
}

void ThirMirBuilder::emit_cond_branch(Value cond, uint32_t true_block, uint32_t false_block) {
    if (!ctx_.current_func)
        return;
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value())
        return;
    block->terminator = CondBranchTerm{cond, true_block, false_block};
}

void ThirMirBuilder::emit_unreachable() {
    if (!ctx_.current_func)
        return;
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value())
        return;
    block->terminator = UnreachableTerm{};
}

auto ThirMirBuilder::const_int(int64_t value, int bit_width, bool is_signed) -> Value {
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

auto ThirMirBuilder::const_float(double value, bool is_f64) -> Value {
    ConstantInst inst;
    inst.value = ConstFloat{value, is_f64};
    return emit(std::move(inst), is_f64 ? make_f64_type() : make_f32_type());
}

auto ThirMirBuilder::const_bool(bool value) -> Value {
    ConstantInst inst;
    inst.value = ConstBool{value};
    return emit(std::move(inst), make_bool_type());
}

auto ThirMirBuilder::const_string(const std::string& value) -> Value {
    ConstantInst inst;
    inst.value = ConstString{value};
    return emit(std::move(inst), make_str_type());
}

auto ThirMirBuilder::const_unit() -> Value {
    ConstantInst inst;
    inst.value = ConstUnit{};
    return emit(std::move(inst), make_unit_type());
}

auto ThirMirBuilder::get_variable(const std::string& name) -> Value {
    auto it = ctx_.variables.find(name);
    if (it != ctx_.variables.end()) {
        return it->second;
    }
    return {INVALID_VALUE, make_unit_type()};
}

void ThirMirBuilder::set_variable(const std::string& name, Value value) {
    ctx_.variables[name] = value;
}

auto ThirMirBuilder::convert_binop(hir::HirBinOp op) -> BinOp {
    switch (op) {
    case hir::HirBinOp::Add:
        return BinOp::Add;
    case hir::HirBinOp::Sub:
        return BinOp::Sub;
    case hir::HirBinOp::Mul:
        return BinOp::Mul;
    case hir::HirBinOp::Div:
        return BinOp::Div;
    case hir::HirBinOp::Mod:
        return BinOp::Mod;
    case hir::HirBinOp::Eq:
        return BinOp::Eq;
    case hir::HirBinOp::Ne:
        return BinOp::Ne;
    case hir::HirBinOp::Lt:
        return BinOp::Lt;
    case hir::HirBinOp::Le:
        return BinOp::Le;
    case hir::HirBinOp::Gt:
        return BinOp::Gt;
    case hir::HirBinOp::Ge:
        return BinOp::Ge;
    case hir::HirBinOp::And:
        return BinOp::And;
    case hir::HirBinOp::Or:
        return BinOp::Or;
    case hir::HirBinOp::BitAnd:
        return BinOp::BitAnd;
    case hir::HirBinOp::BitOr:
        return BinOp::BitOr;
    case hir::HirBinOp::BitXor:
        return BinOp::BitXor;
    case hir::HirBinOp::Shl:
        return BinOp::Shl;
    case hir::HirBinOp::Shr:
        return BinOp::Shr;
    default:
        return BinOp::Add;
    }
}

auto ThirMirBuilder::convert_compound_op(hir::HirCompoundOp op) -> BinOp {
    switch (op) {
    case hir::HirCompoundOp::Add:
        return BinOp::Add;
    case hir::HirCompoundOp::Sub:
        return BinOp::Sub;
    case hir::HirCompoundOp::Mul:
        return BinOp::Mul;
    case hir::HirCompoundOp::Div:
        return BinOp::Div;
    case hir::HirCompoundOp::Mod:
        return BinOp::Mod;
    case hir::HirCompoundOp::BitAnd:
        return BinOp::BitAnd;
    case hir::HirCompoundOp::BitOr:
        return BinOp::BitOr;
    case hir::HirCompoundOp::BitXor:
        return BinOp::BitXor;
    case hir::HirCompoundOp::Shl:
        return BinOp::Shl;
    case hir::HirCompoundOp::Shr:
        return BinOp::Shr;
    default:
        return BinOp::Add;
    }
}

auto ThirMirBuilder::is_comparison_op(hir::HirBinOp op) -> bool {
    switch (op) {
    case hir::HirBinOp::Eq:
    case hir::HirBinOp::Ne:
    case hir::HirBinOp::Lt:
    case hir::HirBinOp::Le:
    case hir::HirBinOp::Gt:
    case hir::HirBinOp::Ge:
        return true;
    default:
        return false;
    }
}

auto ThirMirBuilder::convert_unaryop(hir::HirUnaryOp op) -> UnaryOp {
    switch (op) {
    case hir::HirUnaryOp::Neg:
        return UnaryOp::Neg;
    case hir::HirUnaryOp::Not:
        return UnaryOp::Not;
    case hir::HirUnaryOp::BitNot:
        return UnaryOp::BitNot;
    default:
        return UnaryOp::Neg;
    }
}

void ThirMirBuilder::emit_drop_calls(const std::vector<BuildContext::DropInfo>& drops) {
    for (const auto& drop_info : drops) {
        emit_drop_for_value(drop_info.value, drop_info.type, drop_info.type_name);
    }
}

void ThirMirBuilder::emit_drop_for_value(Value /*value*/, const MirTypePtr& /*type*/,
                                         const std::string& /*type_name*/) {
    // Drop for specific value — simplified for initial implementation
}

void ThirMirBuilder::emit_scope_drops() {
    auto drops = ctx_.get_drops_for_current_scope();
    emit_drop_calls(drops);
    ctx_.mark_scope_dropped();
}

void ThirMirBuilder::emit_all_drops() {
    auto drops = ctx_.get_all_drops();
    emit_drop_calls(drops);
    ctx_.mark_all_dropped();
}

auto ThirMirBuilder::get_type_name(const MirTypePtr& type) const -> std::string {
    if (!type)
        return "unit";

    return std::visit(
        [this](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, MirStructType>) {
                return t.name;
            } else if constexpr (std::is_same_v<T, MirEnumType>) {
                return t.name;
            } else if constexpr (std::is_same_v<T, MirPointerType>) {
                if (t.pointee) {
                    return get_type_name(t.pointee);
                }
                return "";
            } else if constexpr (std::is_same_v<T, MirPrimitiveType>) {
                switch (t.kind) {
                case PrimitiveType::I8:
                    return "I8";
                case PrimitiveType::I16:
                    return "I16";
                case PrimitiveType::I32:
                    return "I32";
                case PrimitiveType::I64:
                    return "I64";
                case PrimitiveType::U8:
                    return "U8";
                case PrimitiveType::U16:
                    return "U16";
                case PrimitiveType::U32:
                    return "U32";
                case PrimitiveType::U64:
                    return "U64";
                case PrimitiveType::F32:
                    return "F32";
                case PrimitiveType::F64:
                    return "F64";
                case PrimitiveType::Bool:
                    return "Bool";
                case PrimitiveType::Str:
                    return "Str";
                default:
                    return "";
                }
            } else {
                return "";
            }
        },
        type->kind);
}

} // namespace tml::mir
