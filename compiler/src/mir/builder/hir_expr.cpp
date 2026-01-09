//! # HIR Expression Lowering to MIR
//!
//! This file implements expression lowering from HIR to MIR SSA form.
//! Each HIR expression is converted to one or more MIR instructions.
//!
//! ## Key Differences from AST→MIR
//!
//! - Types are already resolved (no type inference needed)
//! - Field indices are resolved (no lookup required)
//! - Closure captures are explicit

#include "mir/hir_mir_builder.hpp"

#include <stdexcept>

namespace tml::mir {

// ============================================================================
// Expression Building
// ============================================================================

auto HirMirBuilder::build_expr(const hir::HirExprPtr& expr) -> Value {
    if (!expr) {
        return const_unit();
    }

    // Dispatch based on expression kind
    return std::visit(
        [this](const auto& e) -> Value {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, hir::HirLiteralExpr>) {
                return build_literal(e);
            } else if constexpr (std::is_same_v<T, hir::HirVarExpr>) {
                return build_var(e);
            } else if constexpr (std::is_same_v<T, hir::HirBinaryExpr>) {
                return build_binary(e);
            } else if constexpr (std::is_same_v<T, hir::HirUnaryExpr>) {
                return build_unary(e);
            } else if constexpr (std::is_same_v<T, hir::HirCallExpr>) {
                return build_call(e);
            } else if constexpr (std::is_same_v<T, hir::HirMethodCallExpr>) {
                return build_method_call(e);
            } else if constexpr (std::is_same_v<T, hir::HirFieldExpr>) {
                return build_field(e);
            } else if constexpr (std::is_same_v<T, hir::HirIndexExpr>) {
                return build_index(e);
            } else if constexpr (std::is_same_v<T, hir::HirIfExpr>) {
                return build_if(e);
            } else if constexpr (std::is_same_v<T, hir::HirBlockExpr>) {
                return build_block(e);
            } else if constexpr (std::is_same_v<T, hir::HirLoopExpr>) {
                return build_loop(e);
            } else if constexpr (std::is_same_v<T, hir::HirWhileExpr>) {
                return build_while(e);
            } else if constexpr (std::is_same_v<T, hir::HirForExpr>) {
                return build_for(e);
            } else if constexpr (std::is_same_v<T, hir::HirReturnExpr>) {
                return build_return(e);
            } else if constexpr (std::is_same_v<T, hir::HirBreakExpr>) {
                return build_break(e);
            } else if constexpr (std::is_same_v<T, hir::HirContinueExpr>) {
                return build_continue(e);
            } else if constexpr (std::is_same_v<T, hir::HirWhenExpr>) {
                return build_when(e);
            } else if constexpr (std::is_same_v<T, hir::HirStructExpr>) {
                return build_struct_expr(e);
            } else if constexpr (std::is_same_v<T, hir::HirEnumExpr>) {
                return build_enum_expr(e);
            } else if constexpr (std::is_same_v<T, hir::HirTupleExpr>) {
                return build_tuple(e);
            } else if constexpr (std::is_same_v<T, hir::HirArrayExpr>) {
                return build_array(e);
            } else if constexpr (std::is_same_v<T, hir::HirArrayRepeatExpr>) {
                return build_array_repeat(e);
            } else if constexpr (std::is_same_v<T, hir::HirCastExpr>) {
                return build_cast(e);
            } else if constexpr (std::is_same_v<T, hir::HirClosureExpr>) {
                return build_closure(e);
            } else if constexpr (std::is_same_v<T, hir::HirTryExpr>) {
                return build_try(e);
            } else if constexpr (std::is_same_v<T, hir::HirAwaitExpr>) {
                return build_await(e);
            } else if constexpr (std::is_same_v<T, hir::HirAssignExpr>) {
                return build_assign(e);
            } else if constexpr (std::is_same_v<T, hir::HirCompoundAssignExpr>) {
                return build_compound_assign(e);
            } else {
                // Unknown expression type
                return const_unit();
            }
        },
        expr->kind);
}

// ============================================================================
// Literal Expressions
// ============================================================================

auto HirMirBuilder::build_literal(const hir::HirLiteralExpr& lit) -> Value {
    return std::visit(
        [this, &lit](const auto& v) -> Value {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, int64_t>) {
                // Signed integer - determine bit width from type if available
                MirTypePtr mir_type = convert_type(lit.type);
                int bit_width = (mir_type && mir_type->bit_width() > 0) ? mir_type->bit_width() : 32;
                return const_int(v, bit_width, true);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                // Unsigned integer
                MirTypePtr mir_type = convert_type(lit.type);
                int bit_width = (mir_type && mir_type->bit_width() > 0) ? mir_type->bit_width() : 32;
                return const_int(static_cast<int64_t>(v), bit_width, false);
            } else if constexpr (std::is_same_v<T, double>) {
                // Float - check if f64 from type
                MirTypePtr mir_type = convert_type(lit.type);
                bool is_f64 = mir_type && mir_type->bit_width() == 64;
                return const_float(v, is_f64);
            } else if constexpr (std::is_same_v<T, bool>) {
                return const_bool(v);
            } else if constexpr (std::is_same_v<T, char>) {
                // Char as U32
                return const_int(static_cast<int64_t>(v), 32, false);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return const_string(v);
            } else {
                return const_unit();
            }
        },
        lit.value);
}

// ============================================================================
// Variable Reference
// ============================================================================

auto HirMirBuilder::build_var(const hir::HirVarExpr& var) -> Value {
    Value result = get_variable(var.name);
    // Ensure the value has the correct type from HIR
    if (!result.type || result.type->is_unit()) {
        result.type = convert_type(var.type);
    }
    return result;
}

// ============================================================================
// Binary Expression
// ============================================================================

auto HirMirBuilder::build_binary(const hir::HirBinaryExpr& bin) -> Value {
    // Handle short-circuit operators specially
    if (bin.op == hir::HirBinOp::And) {
        // Short-circuit AND: if left is false, skip right
        Value left = build_expr(bin.left);

        uint32_t right_block = create_block("and.right");
        uint32_t merge_block = create_block("and.merge");

        uint32_t left_block = ctx_.current_block;
        emit_cond_branch(left, right_block, merge_block);

        // Right block
        switch_to_block(right_block);
        Value right = build_expr(bin.right);
        uint32_t right_end_block = ctx_.current_block;
        emit_branch(merge_block);

        // Merge block with phi
        switch_to_block(merge_block);
        PhiInst phi;
        phi.incoming = {
            {const_bool(false), left_block}, // If left was false
            {right, right_end_block}         // If left was true, use right
        };
        phi.result_type = make_bool_type();

        return emit(phi, make_bool_type());
    }
    if (bin.op == hir::HirBinOp::Or) {
        // Short-circuit OR: if left is true, skip right
        Value left = build_expr(bin.left);

        uint32_t right_block = create_block("or.right");
        uint32_t merge_block = create_block("or.merge");

        uint32_t left_block = ctx_.current_block;
        emit_cond_branch(left, merge_block, right_block);

        // Right block
        switch_to_block(right_block);
        Value right = build_expr(bin.right);
        uint32_t right_end_block = ctx_.current_block;
        emit_branch(merge_block);

        // Merge block with phi
        switch_to_block(merge_block);
        PhiInst phi;
        phi.incoming = {
            {const_bool(true), left_block}, // If left was true
            {right, right_end_block}        // If left was false, use right
        };
        phi.result_type = make_bool_type();

        return emit(phi, make_bool_type());
    }

    // Normal binary operation
    Value left = build_expr(bin.left);
    Value right = build_expr(bin.right);

    BinOp op = convert_binop(bin.op);
    MirTypePtr result_type;

    if (is_comparison_op(bin.op)) {
        result_type = make_bool_type();
    } else {
        result_type = convert_type(bin.type);
        // Fallback: if bin.type is empty, use left operand's type
        if (!result_type || result_type->is_unit()) {
            result_type = left.type;
        }
        // Final fallback to i32
        if (!result_type || result_type->is_unit()) {
            result_type = make_i32_type();
        }
    }

    BinaryInst inst;
    inst.op = op;
    inst.left = left;
    inst.right = right;
    inst.result_type = result_type;

    return emit(inst, result_type);
}

// ============================================================================
// Unary Expression
// ============================================================================

auto HirMirBuilder::build_unary(const hir::HirUnaryExpr& unary) -> Value {
    MirTypePtr result_type = convert_type(unary.type);

    // Handle reference and dereference specially
    if (unary.op == hir::HirUnaryOp::Ref || unary.op == hir::HirUnaryOp::RefMut) {
        // Taking a reference requires getting the address of the operand
        // If the operand is a variable, we need to allocate it first (if not already)
        // Then return a pointer to it

        bool is_mut = (unary.op == hir::HirUnaryOp::RefMut);

        // Check if operand is a simple variable reference
        if (auto* var = std::get_if<hir::HirVarExpr>(&unary.operand->kind)) {
            // For variables, we need to ensure they're in memory
            // Allocate stack slot if not already allocated
            Value var_value = get_variable(var->name);
            MirTypePtr var_type = var_value.type;

            // Create alloca for the variable value
            AllocaInst alloca_inst;
            alloca_inst.alloc_type = var_type;
            alloca_inst.name = var->name + "_ref";

            MirTypePtr ptr_type = make_pointer_type(var_type, is_mut);
            Value ptr = emit(alloca_inst, ptr_type);

            // Store the current value
            StoreInst store;
            store.ptr = ptr;
            store.value = var_value;
            store.value_type = var_type;

            emit_void(store);

            return ptr;
        }

        // For field/index expressions, the address is already computed
        if (auto* field = std::get_if<hir::HirFieldExpr>(&unary.operand->kind)) {
            // Get address of field
            Value base = build_expr(field->object);
            MirTypePtr field_type = convert_type(field->type);

            GetElementPtrInst gep;
            gep.base = base;
            gep.indices = {const_int(field->field_index, 32, false)};
            gep.base_type = base.type;
            gep.result_type = make_pointer_type(field_type, is_mut);

            return emit(gep, gep.result_type);
        }

        if (auto* index = std::get_if<hir::HirIndexExpr>(&unary.operand->kind)) {
            // Get address of array element
            Value base = build_expr(index->object);
            Value idx = build_expr(index->index);
            MirTypePtr elem_type = convert_type(index->type);

            GetElementPtrInst gep;
            gep.base = base;
            gep.indices = {idx};
            gep.base_type = base.type;
            gep.result_type = make_pointer_type(elem_type, is_mut);

            return emit(gep, gep.result_type);
        }

        // For other expressions, evaluate and take address
        Value operand = build_expr(unary.operand);
        MirTypePtr operand_type = operand.type;

        AllocaInst alloca_inst;
        alloca_inst.alloc_type = operand_type;
        alloca_inst.name = "__temp_ref";

        MirTypePtr ptr_type = make_pointer_type(operand_type, is_mut);
        Value ptr = emit(alloca_inst, ptr_type);

        StoreInst store;
        store.ptr = ptr;
        store.value = operand;
        store.value_type = operand_type;

        emit_void(store);

        return ptr;
    }

    if (unary.op == hir::HirUnaryOp::Deref) {
        // Dereference pointer - load the value from memory
        Value operand = build_expr(unary.operand);

        LoadInst load;
        load.ptr = operand;
        load.result_type = result_type;
        return emit(load, result_type);
    }

    // Normal unary operation (Neg, Not, BitNot)
    Value operand = build_expr(unary.operand);
    UnaryOp op = convert_unaryop(unary.op);

    UnaryInst inst;
    inst.op = op;
    inst.operand = operand;
    inst.result_type = result_type;

    return emit(inst, result_type);
}

// ============================================================================
// Call Expression
// ============================================================================

auto HirMirBuilder::build_call(const hir::HirCallExpr& call) -> Value {
    // Build arguments
    std::vector<Value> args;
    std::vector<MirTypePtr> arg_types;

    for (const auto& arg : call.args) {
        Value val = build_expr(arg);
        args.push_back(val);
        arg_types.push_back(val.type);
    }

    MirTypePtr return_type = convert_type(call.type);

    CallInst inst;
    inst.func_name = call.func_name;
    inst.args = std::move(args);
    inst.arg_types = std::move(arg_types);
    inst.return_type = return_type;

    return emit(inst, return_type);
}

// ============================================================================
// Method Call Expression
// ============================================================================

auto HirMirBuilder::build_method_call(const hir::HirMethodCallExpr& call) -> Value {
    // Build receiver
    Value receiver = build_expr(call.receiver);

    // Build arguments
    std::vector<Value> args;
    std::vector<MirTypePtr> arg_types;

    for (const auto& arg : call.args) {
        Value val = build_expr(arg);
        args.push_back(val);
        arg_types.push_back(val.type);
    }

    MirTypePtr return_type = convert_type(call.type);

    MethodCallInst inst;
    inst.receiver = receiver;
    inst.receiver_type = get_type_name(receiver.type);
    inst.method_name = call.method_name;
    inst.args = std::move(args);
    inst.arg_types = std::move(arg_types);
    inst.return_type = return_type;

    return emit(inst, return_type);
}

// ============================================================================
// Field Access
// ============================================================================

auto HirMirBuilder::build_field(const hir::HirFieldExpr& field) -> Value {
    Value base = build_expr(field.object);
    MirTypePtr result_type = convert_type(field.type);

    // HIR already has field_index resolved
    ExtractValueInst inst;
    inst.aggregate = base;
    inst.indices = {static_cast<uint32_t>(field.field_index)};
    inst.aggregate_type = base.type;
    inst.result_type = result_type;

    return emit(inst, result_type);
}

// ============================================================================
// Index Expression
// ============================================================================

auto HirMirBuilder::build_index(const hir::HirIndexExpr& index) -> Value {
    Value base = build_expr(index.object);
    Value idx = build_expr(index.index);
    MirTypePtr result_type = convert_type(index.type);

    // Get element pointer
    GetElementPtrInst gep;
    gep.base = base;
    gep.indices = {idx};
    gep.base_type = base.type;
    gep.result_type = make_pointer_type(result_type, false);

    Value ptr = emit(gep, gep.result_type);

    // Load the value
    LoadInst load;
    load.ptr = ptr;
    load.result_type = result_type;

    return emit(load, result_type);
}

// ============================================================================
// If Expression
// ============================================================================

auto HirMirBuilder::build_if(const hir::HirIfExpr& if_expr) -> Value {
    Value cond = build_expr(if_expr.condition);
    MirTypePtr result_type = convert_type(if_expr.type);

    uint32_t then_block = create_block("if.then");
    uint32_t else_block = create_block("if.else");
    uint32_t merge_block = create_block("if.merge");

    emit_cond_branch(cond, then_block, else_block);

    // Then branch
    switch_to_block(then_block);
    ctx_.push_drop_scope();
    Value then_val = build_expr(if_expr.then_branch);
    emit_scope_drops();
    ctx_.pop_drop_scope();
    uint32_t then_end = ctx_.current_block;
    if (!is_terminated()) {
        emit_branch(merge_block);
    }

    // Else branch
    switch_to_block(else_block);
    Value else_val;
    uint32_t else_end = else_block;
    if (if_expr.else_branch) {
        ctx_.push_drop_scope();
        else_val = build_expr(*if_expr.else_branch);
        emit_scope_drops();
        ctx_.pop_drop_scope();
        else_end = ctx_.current_block;
    } else {
        else_val = const_unit();
    }
    if (!is_terminated()) {
        emit_branch(merge_block);
    }

    // Merge block
    switch_to_block(merge_block);

    // If both branches produce values, create phi
    if (!result_type->is_unit()) {
        PhiInst phi;
        phi.incoming = {{then_val, then_end}, {else_val, else_end}};
        phi.result_type = result_type;
        return emit(phi, result_type);
    }

    return const_unit();
}

// ============================================================================
// Block Expression
// ============================================================================

auto HirMirBuilder::build_block(const hir::HirBlockExpr& block) -> Value {
    ctx_.push_drop_scope();

    Value result = const_unit();

    for (const auto& stmt : block.stmts) {
        bool terminated = build_stmt(*stmt);
        if (terminated) {
            ctx_.pop_drop_scope();
            return result; // Block terminated early
        }
    }

    if (block.expr) {
        result = build_expr(*block.expr);
    }

    emit_scope_drops();
    ctx_.pop_drop_scope();

    return result;
}

// ============================================================================
// Loop Expressions
// ============================================================================

auto HirMirBuilder::build_loop(const hir::HirLoopExpr& loop) -> Value {
    uint32_t header_block = create_block("loop.header");
    uint32_t body_block = create_block("loop.body");
    uint32_t exit_block = create_block("loop.exit");

    // Push loop context
    ctx_.loop_stack.push({header_block, exit_block, std::nullopt});

    emit_branch(header_block);

    // Header (just branch to body for infinite loop)
    switch_to_block(header_block);
    emit_branch(body_block);

    // Body
    switch_to_block(body_block);
    ctx_.push_drop_scope();
    (void)build_expr(loop.body);
    emit_scope_drops();
    ctx_.pop_drop_scope();
    if (!is_terminated()) {
        emit_branch(header_block);
    }

    // Exit block
    switch_to_block(exit_block);
    ctx_.loop_stack.pop();

    // Loop returns unit unless broken with value
    return const_unit();
}

auto HirMirBuilder::build_while(const hir::HirWhileExpr& while_expr) -> Value {
    uint32_t header_block = create_block("while.header");
    uint32_t body_block = create_block("while.body");
    uint32_t exit_block = create_block("while.exit");

    ctx_.loop_stack.push({header_block, exit_block, std::nullopt});

    emit_branch(header_block);

    // Header with condition
    switch_to_block(header_block);
    Value cond = build_expr(while_expr.condition);
    emit_cond_branch(cond, body_block, exit_block);

    // Body
    switch_to_block(body_block);
    ctx_.push_drop_scope();
    (void)build_expr(while_expr.body);
    emit_scope_drops();
    ctx_.pop_drop_scope();
    if (!is_terminated()) {
        emit_branch(header_block);
    }

    // Exit
    switch_to_block(exit_block);
    ctx_.loop_stack.pop();

    return const_unit();
}

auto HirMirBuilder::build_for(const hir::HirForExpr& for_expr) -> Value {
    // For loop implements the iterator protocol:
    // let mut iter = iterable.into_iter();
    // loop {
    //     when iter.next() {
    //         Just(x) => { body },
    //         Nothing => break,
    //     }
    // }
    // TML uses Maybe[T] with Just(v) and Nothing variants

    // Build iterator
    Value iterable = build_expr(for_expr.iter);
    MirTypePtr iterable_type = iterable.type;

    // Call into_iter method to get the iterator
    MethodCallInst into_iter;
    into_iter.receiver = iterable;
    into_iter.receiver_type = get_type_name(iterable_type);
    into_iter.method_name = "into_iter";
    into_iter.args = {};
    into_iter.arg_types = {};
    into_iter.return_type = iterable_type; // Iterator type (placeholder)

    Value iter = emit(into_iter, into_iter.return_type);

    // Allocate stack space for the mutable iterator
    AllocaInst alloca_iter;
    alloca_iter.alloc_type = iter.type;
    alloca_iter.name = "__for_iter";

    Value iter_ptr = emit(alloca_iter, make_pointer_type(iter.type, true));

    // Store initial iterator value
    StoreInst store_iter;
    store_iter.ptr = iter_ptr;
    store_iter.value = iter;
    store_iter.value_type = iter.type;

    emit_void(store_iter);

    // Create loop blocks
    uint32_t header_block = create_block("for.header");
    uint32_t check_block = create_block("for.check");
    uint32_t body_block = create_block("for.body");
    uint32_t exit_block = create_block("for.exit");

    ctx_.loop_stack.push({header_block, exit_block, std::nullopt});

    emit_branch(header_block);

    // Header - load iterator and call next()
    switch_to_block(header_block);

    // Load current iterator state
    LoadInst load_iter;
    load_iter.ptr = iter_ptr;
    load_iter.result_type = iter.type;

    Value current_iter = emit(load_iter, iter.type);

    // Call next() - takes &mut self, returns Maybe[T]
    // In TML: Maybe[T] = Just(T) | Nothing
    MirTypePtr element_type = make_unit_type(); // Will be refined by pattern binding

    // Determine element type from the pattern type if available
    if (for_expr.pattern) {
        element_type = convert_type(for_expr.pattern->type());
    }

    MirTypePtr maybe_type = make_enum_type("Maybe", {element_type});

    MethodCallInst next_call;
    next_call.receiver = current_iter;
    next_call.receiver_type = get_type_name(iter.type);
    next_call.method_name = "next";
    next_call.args = {};
    next_call.arg_types = {};
    next_call.return_type = maybe_type;

    Value next_result = emit(next_call, maybe_type);

    emit_branch(check_block);

    // Check block - pattern match on Maybe
    switch_to_block(check_block);

    // Extract discriminant (tag) from Maybe enum
    // Maybe layout: { tag: i32, payload: T }
    // Just is variant 0, Nothing is variant 1
    ExtractValueInst extract_tag;
    extract_tag.aggregate = next_result;
    extract_tag.indices = {0}; // Tag at index 0
    extract_tag.aggregate_type = maybe_type;
    extract_tag.result_type = make_i32_type();

    Value tag = emit(extract_tag, extract_tag.result_type);

    // Compare with Just variant (variant index 0)
    Value just_tag = const_int(0, 32, true); // Just is variant 0

    BinaryInst cmp;
    cmp.op = BinOp::Eq;
    cmp.left = tag;
    cmp.right = just_tag;
    cmp.result_type = make_bool_type();

    Value is_just = emit(cmp, make_bool_type());

    emit_cond_branch(is_just, body_block, exit_block);

    // Body block - extract value and execute body
    switch_to_block(body_block);
    ctx_.push_drop_scope();

    // Extract value from Just(value) - payload at index 1 (after tag)
    // For variant 0 (Just), payload is at indices {0, 0}
    ExtractValueInst extract_value;
    extract_value.aggregate = next_result;
    extract_value.indices = {0, 0}; // Just variant, first payload field
    extract_value.aggregate_type = maybe_type;
    extract_value.result_type = element_type;

    Value element = emit(extract_value, element_type);

    // Bind pattern to extracted value
    build_pattern_binding(for_expr.pattern, element);

    // Execute body
    (void)build_expr(for_expr.body);

    emit_scope_drops();
    ctx_.pop_drop_scope();

    if (!is_terminated()) {
        emit_branch(header_block);
    }

    // Exit block
    switch_to_block(exit_block);
    ctx_.loop_stack.pop();

    return const_unit();
}

// ============================================================================
// Control Flow
// ============================================================================

auto HirMirBuilder::build_return(const hir::HirReturnExpr& ret) -> Value {
    // Emit drops before return
    emit_all_drops();

    if (ret.value) {
        Value val = build_expr(*ret.value);
        emit_return(val);
    } else {
        emit_return(std::nullopt);
    }

    return const_unit();
}

auto HirMirBuilder::build_break(const hir::HirBreakExpr& brk) -> Value {
    if (ctx_.loop_stack.empty()) {
        throw std::runtime_error("break outside of loop");
    }

    // Emit scope drops
    emit_scope_drops();

    auto& loop_ctx = ctx_.loop_stack.top();

    if (brk.value) {
        Value val = build_expr(*brk.value);
        loop_ctx.break_value = val;
    }

    emit_branch(loop_ctx.exit_block);
    return const_unit();
}

auto HirMirBuilder::build_continue(const hir::HirContinueExpr& /*cont*/) -> Value {
    if (ctx_.loop_stack.empty()) {
        throw std::runtime_error("continue outside of loop");
    }

    emit_scope_drops();

    auto& loop_ctx = ctx_.loop_stack.top();
    emit_branch(loop_ctx.header_block);
    return const_unit();
}

// ============================================================================
// Pattern Matching (when)
// ============================================================================

auto HirMirBuilder::build_when(const hir::HirWhenExpr& when) -> Value {
    Value scrutinee = build_expr(when.scrutinee);
    MirTypePtr result_type = convert_type(when.type);

    uint32_t exit_block = create_block("when.exit");

    std::vector<std::pair<Value, uint32_t>> phi_inputs;

    for (size_t i = 0; i < when.arms.size(); ++i) {
        const auto& arm = when.arms[i];

        uint32_t arm_block = create_block("when.arm" + std::to_string(i));
        uint32_t next_block = (i + 1 < when.arms.size())
                                  ? create_block("when.next" + std::to_string(i))
                                  : exit_block;

        // Build pattern match condition
        Value matches = build_pattern_match(arm.pattern, scrutinee);
        emit_cond_branch(matches, arm_block, next_block);

        // Arm body
        switch_to_block(arm_block);
        ctx_.push_drop_scope();

        // Bind pattern variables
        build_pattern_binding(arm.pattern, scrutinee);

        // Execute body
        Value arm_result = build_expr(arm.body);

        emit_scope_drops();
        ctx_.pop_drop_scope();

        uint32_t arm_end = ctx_.current_block;
        if (!is_terminated()) {
            phi_inputs.push_back({arm_result, arm_end});
            emit_branch(exit_block);
        }

        if (i + 1 < when.arms.size()) {
            switch_to_block(next_block);
        }
    }

    // Exit block
    switch_to_block(exit_block);

    if (!result_type->is_unit() && !phi_inputs.empty()) {
        PhiInst phi;
        phi.incoming = std::move(phi_inputs);
        phi.result_type = result_type;
        return emit(phi, result_type);
    }

    return const_unit();
}

// ============================================================================
// Struct/Enum/Tuple/Array Construction
// ============================================================================

auto HirMirBuilder::build_struct_expr(const hir::HirStructExpr& s) -> Value {
    std::vector<Value> fields;
    std::vector<MirTypePtr> field_types;

    for (const auto& field : s.fields) {
        Value val = build_expr(field.second);
        fields.push_back(val);
        field_types.push_back(val.type);
    }

    StructInitInst inst;
    inst.struct_name = s.struct_name;
    inst.fields = std::move(fields);
    inst.field_types = std::move(field_types);

    MirTypePtr result_type = convert_type(s.type);
    return emit(inst, result_type);
}

auto HirMirBuilder::build_enum_expr(const hir::HirEnumExpr& e) -> Value {
    std::vector<Value> payload;
    std::vector<MirTypePtr> payload_types;

    for (const auto& arg : e.payload) {
        Value val = build_expr(arg);
        payload.push_back(val);
        payload_types.push_back(val.type);
    }

    EnumInitInst inst;
    inst.enum_name = e.enum_name;
    inst.variant_name = e.variant_name;
    inst.variant_index = e.variant_index;
    inst.payload = std::move(payload);
    inst.payload_types = std::move(payload_types);

    MirTypePtr result_type = convert_type(e.type);
    return emit(inst, result_type);
}

auto HirMirBuilder::build_tuple(const hir::HirTupleExpr& tuple) -> Value {
    std::vector<Value> elements;
    std::vector<MirTypePtr> element_types;

    for (const auto& elem : tuple.elements) {
        Value val = build_expr(elem);
        elements.push_back(val);
        element_types.push_back(val.type);
    }

    TupleInitInst inst;
    inst.elements = std::move(elements);
    inst.element_types = element_types;
    inst.result_type = make_tuple_type(element_types);

    return emit(inst, inst.result_type);
}

auto HirMirBuilder::build_array(const hir::HirArrayExpr& arr) -> Value {
    std::vector<Value> elements;

    for (const auto& elem : arr.elements) {
        elements.push_back(build_expr(elem));
    }

    MirTypePtr element_type = convert_type(arr.element_type);
    MirTypePtr result_type = make_array_type(element_type, arr.elements.size());

    ArrayInitInst inst;
    inst.elements = std::move(elements);
    inst.element_type = element_type;
    inst.result_type = result_type;

    return emit(inst, result_type);
}

auto HirMirBuilder::build_array_repeat(const hir::HirArrayRepeatExpr& arr) -> Value {
    Value element = build_expr(arr.value);
    MirTypePtr element_type = element.type;
    MirTypePtr result_type = make_array_type(element_type, arr.count);

    // Build array by repeating element
    std::vector<Value> elements(arr.count, element);

    ArrayInitInst inst;
    inst.elements = std::move(elements);
    inst.element_type = element_type;
    inst.result_type = result_type;

    return emit(inst, result_type);
}

// ============================================================================
// Cast Expression
// ============================================================================

auto HirMirBuilder::build_cast(const hir::HirCastExpr& cast) -> Value {
    Value operand = build_expr(cast.expr);
    MirTypePtr source_type = operand.type;
    MirTypePtr target_type = convert_type(cast.target_type);

    // Determine cast kind
    CastKind kind = CastKind::Bitcast; // Default

    if (source_type->is_integer() && target_type->is_integer()) {
        int src_bits = source_type->bit_width();
        int tgt_bits = target_type->bit_width();

        if (src_bits > tgt_bits) {
            kind = CastKind::Trunc;
        } else if (src_bits < tgt_bits) {
            kind = source_type->is_signed() ? CastKind::SExt : CastKind::ZExt;
        }
    } else if (source_type->is_float() && target_type->is_float()) {
        int src_bits = source_type->bit_width();
        int tgt_bits = target_type->bit_width();

        if (src_bits > tgt_bits) {
            kind = CastKind::FPTrunc;
        } else {
            kind = CastKind::FPExt;
        }
    } else if (source_type->is_integer() && target_type->is_float()) {
        kind = source_type->is_signed() ? CastKind::SIToFP : CastKind::UIToFP;
    } else if (source_type->is_float() && target_type->is_integer()) {
        kind = target_type->is_signed() ? CastKind::FPToSI : CastKind::FPToUI;
    }

    CastInst inst;
    inst.kind = kind;
    inst.operand = operand;
    inst.source_type = source_type;
    inst.target_type = target_type;

    return emit(inst, target_type);
}

// ============================================================================
// Closure Expression
// ============================================================================

auto HirMirBuilder::build_closure(const hir::HirClosureExpr& closure) -> Value {
    // Closures in HIR have explicit captures
    // Generate a closure instruction that captures values from the environment

    MirTypePtr result_type = convert_type(closure.type);

    // Generate unique name for the closure function
    static uint32_t closure_counter = 0;
    std::string func_name = "__closure_" + std::to_string(closure_counter++);

    // Build captured values
    std::vector<std::pair<std::string, Value>> captured_values;
    std::vector<std::pair<std::string, MirTypePtr>> capture_types;

    for (const auto& cap : closure.captures) {
        // Get the value from the enclosing scope
        Value cap_value = get_variable(cap.name);
        MirTypePtr cap_type = convert_type(cap.type);

        // If captured by reference, take address; if by move, use value directly
        if (!cap.by_move && !cap.is_mut) {
            // Captured by shared reference - in MIR we just use the value
            captured_values.push_back({cap.name, cap_value});
            capture_types.push_back({cap.name, cap_type});
        } else if (!cap.by_move && cap.is_mut) {
            // Captured by mutable reference - need pointer
            captured_values.push_back({cap.name, cap_value});
            capture_types.push_back({cap.name, make_pointer_type(cap_type, true)});
        } else {
            // Captured by move - take ownership
            captured_values.push_back({cap.name, cap_value});
            capture_types.push_back({cap.name, cap_type});
        }
    }

    // Build function type from closure signature
    std::vector<MirTypePtr> param_types;
    for (const auto& param : closure.params) {
        param_types.push_back(convert_type(param.second));
    }

    // Get return type from the function type
    MirTypePtr func_type = result_type;

    // Create ClosureInitInst
    ClosureInitInst inst;
    inst.func_name = func_name;
    inst.captures = std::move(captured_values);
    inst.cap_types = std::move(capture_types);
    inst.func_type = func_type;
    inst.result_type = result_type;

    return emit(inst, result_type);
}

// ============================================================================
// Try Expression
// ============================================================================

auto HirMirBuilder::build_try(const hir::HirTryExpr& try_expr) -> Value {
    // Try operator: expr! extracts Ok value or propagates Err
    // TML uses Outcome[T, E] with Ok(v) and Err(e) variants
    //
    // Desugaring of `expr!`:
    // when expr {
    //     Ok(v) => v,
    //     Err(e) => return Err(e),
    // }

    Value outcome = build_expr(try_expr.expr);
    MirTypePtr outcome_type = outcome.type;
    MirTypePtr ok_type = convert_type(try_expr.type); // The T in Outcome[T, E]

    // Extract discriminant (tag at index 0 of Outcome enum)
    ExtractValueInst extract_tag;
    extract_tag.aggregate = outcome;
    extract_tag.indices = {0}; // Tag is at index 0
    extract_tag.aggregate_type = outcome_type;
    extract_tag.result_type = make_i32_type();

    Value tag = emit(extract_tag, extract_tag.result_type);

    // Compare with Ok variant (variant index 0)
    // In TML: enum Outcome[T, E] { Ok(T), Err(E) }
    Value ok_tag = const_int(0, 32, true); // Ok is variant 0

    BinaryInst cmp;
    cmp.op = BinOp::Eq;
    cmp.left = tag;
    cmp.right = ok_tag;
    cmp.result_type = make_bool_type();

    Value is_ok = emit(cmp, make_bool_type());

    // Create blocks for Ok and Err branches
    uint32_t ok_block = create_block("try.ok");
    uint32_t err_block = create_block("try.err");
    uint32_t merge_block = create_block("try.merge");

    emit_cond_branch(is_ok, ok_block, err_block);

    // Ok branch - extract value and continue
    switch_to_block(ok_block);
    ExtractValueInst extract_ok;
    extract_ok.aggregate = outcome;
    extract_ok.indices = {0, 0}; // Ok variant, first field
    extract_ok.aggregate_type = outcome_type;
    extract_ok.result_type = ok_type;

    Value ok_value = emit(extract_ok, ok_type);
    emit_branch(merge_block);

    // Err branch - propagate error with early return
    switch_to_block(err_block);

    // Extract error value: Err is variant 1
    ExtractValueInst extract_err;
    extract_err.aggregate = outcome;
    extract_err.indices = {1, 0}; // Err variant, first field
    extract_err.aggregate_type = outcome_type;
    extract_err.result_type = make_unit_type(); // Error type (unknown here)

    Value err_value = emit(extract_err, extract_err.result_type);

    // Wrap error in Outcome::Err and return
    EnumInitInst wrap_err;
    wrap_err.enum_name = "Outcome";
    wrap_err.variant_name = "Err";
    wrap_err.variant_index = 1;
    wrap_err.payload = {err_value};
    wrap_err.payload_types = {err_value.type};

    Value wrapped_err = emit(wrap_err, outcome_type);

    // Emit drops before early return
    emit_all_drops();
    emit_return(wrapped_err);

    // Merge block - continue with ok value
    switch_to_block(merge_block);

    // Use phi to get the ok value (only one incoming since err returns)
    // Since err block returns, we don't need a phi - just use ok_value
    return ok_value;
}

// ============================================================================
// Await Expression
// ============================================================================

auto HirMirBuilder::build_await(const hir::HirAwaitExpr& await_expr) -> Value {
    Value poll_value = build_expr(await_expr.expr);
    MirTypePtr result_type = convert_type(await_expr.type);

    AwaitInst inst;
    inst.poll_value = poll_value;
    inst.poll_type = poll_value.type;
    inst.result_type = result_type;
    inst.suspension_id = ctx_.next_suspension_id++;

    return emit(inst, result_type);
}

// ============================================================================
// Assignment Expressions
// ============================================================================

auto HirMirBuilder::build_assign(const hir::HirAssignExpr& assign) -> Value {
    Value rhs = build_expr(assign.value);

    // Get the target as pointer
    // For simple variable assignment, update the variable map
    if (auto* var = std::get_if<hir::HirVarExpr>(&assign.target->kind)) {
        set_variable(var->name, rhs);
        return const_unit();
    }

    // For field/index assignment, need store instruction
    Value target_ptr = build_expr(assign.target); // Should return pointer

    StoreInst store;
    store.ptr = target_ptr;
    store.value = rhs;
    store.value_type = rhs.type;

    emit_void(store);
    return const_unit();
}

auto HirMirBuilder::build_compound_assign(const hir::HirCompoundAssignExpr& assign) -> Value {
    // a += b becomes a = a + b
    Value lhs = build_expr(assign.target);
    Value rhs = build_expr(assign.value);

    BinOp op = convert_compound_op(assign.op);
    MirTypePtr result_type = lhs.type;

    BinaryInst bin;
    bin.op = op;
    bin.left = lhs;
    bin.right = rhs;
    bin.result_type = result_type;

    Value result = emit(bin, result_type);

    // Store back
    if (auto* var = std::get_if<hir::HirVarExpr>(&assign.target->kind)) {
        set_variable(var->name, result);
    }

    return const_unit();
}

} // namespace tml::mir
