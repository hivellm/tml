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
            } else if constexpr (std::is_same_v<T, hir::HirLowlevelExpr>) {
                return build_lowlevel(e);
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
                int bit_width =
                    (mir_type && mir_type->bit_width() > 0) ? mir_type->bit_width() : 32;
                return const_int(v, bit_width, true);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                // Unsigned integer
                MirTypePtr mir_type = convert_type(lit.type);
                int bit_width =
                    (mir_type && mir_type->bit_width() > 0) ? mir_type->bit_width() : 32;
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

    // If result is invalid and the type is a function type, this is a function reference
    if (result.id == INVALID_VALUE && var.type) {
        if (var.type->is<types::FuncType>()) {
            // Create a function reference constant
            MirTypePtr func_type = convert_type(var.type);
            ConstFuncRef func_ref;
            func_ref.func_name = var.name;
            func_ref.func_type = func_type;

            ConstantInst const_inst;
            const_inst.value = func_ref;
            return emit(const_inst, func_type);
        }
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

    return emit(inst, result_type, bin.span);
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

    return emit(inst, result_type, unary.span);
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

    return emit(inst, return_type, call.span);
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

    // Get receiver type name from HIR (preserves class name even when converted to ptr)
    std::string recv_type_name;
    if (call.receiver_type) {
        if (auto* class_type = std::get_if<types::ClassType>(&call.receiver_type->kind)) {
            recv_type_name = class_type->name;
        } else if (auto* named_type = std::get_if<types::NamedType>(&call.receiver_type->kind)) {
            recv_type_name = named_type->name;
        } else {
            recv_type_name = get_type_name(receiver.type);
        }
    } else {
        recv_type_name = get_type_name(receiver.type);
    }

    MethodCallInst inst;
    inst.receiver = receiver;
    inst.receiver_type = recv_type_name;
    inst.method_name = call.method_name;
    inst.args = std::move(args);
    inst.arg_types = std::move(arg_types);
    inst.return_type = return_type;

    return emit(inst, return_type, call.span);
}

// ============================================================================
// Field Access
// ============================================================================

auto HirMirBuilder::build_field(const hir::HirFieldExpr& field) -> Value {
    Value base = build_expr(field.object);
    MirTypePtr result_type = convert_type(field.type);

    // For class types (pointers), we need to load the struct first
    Value aggregate = base;
    MirTypePtr aggregate_type = base.type;

    // Check for pointer types - either MirPointerType or primitive Ptr (used for classes)
    bool is_pointer = false;
    MirTypePtr pointee_type;

    if (aggregate_type) {
        if (std::holds_alternative<MirPointerType>(aggregate_type->kind)) {
            const auto& ptr_type = std::get<MirPointerType>(aggregate_type->kind);
            is_pointer = true;
            pointee_type = ptr_type.pointee;
        } else if (std::holds_alternative<MirPrimitiveType>(aggregate_type->kind)) {
            const auto& prim = std::get<MirPrimitiveType>(aggregate_type->kind);
            if (prim.kind == PrimitiveType::Ptr) {
                // Class type - need to get the struct type from the HIR object type
                is_pointer = true;
                // Get the underlying struct type from field.object's type
                if (field.object) {
                    auto obj_type = field.object->type();
                    if (obj_type && obj_type->is<types::ClassType>()) {
                        const auto& class_type = obj_type->as<types::ClassType>();
                        // Create struct type for the class
                        pointee_type = make_struct_type(class_type.name);
                    }
                }
            }
        }
    }

    if (is_pointer && pointee_type) {
        // Load the struct from the pointer
        LoadInst load;
        load.ptr = base;
        load.result_type = pointee_type;
        aggregate = emit(load, pointee_type, field.span);
        aggregate_type = pointee_type;
    }

    // HIR already has field_index resolved
    ExtractValueInst inst;
    inst.aggregate = aggregate;
    inst.indices = {static_cast<uint32_t>(field.field_index)};
    inst.aggregate_type = aggregate_type;
    inst.result_type = result_type;

    return emit(inst, result_type, field.span);
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

    // Check if base is an array type and record size for bounds check elimination
    if (base.type) {
        if (auto* arr_type = std::get_if<MirArrayType>(&base.type->kind)) {
            gep.known_array_size = static_cast<int64_t>(arr_type->size);
        } else if (auto* ptr_type = std::get_if<MirPointerType>(&base.type->kind)) {
            // Check if pointee is array
            if (ptr_type->pointee) {
                if (auto* inner_arr = std::get_if<MirArrayType>(&ptr_type->pointee->kind)) {
                    gep.known_array_size = static_cast<int64_t>(inner_arr->size);
                }
            }
        }
    }

    Value ptr = emit(gep, gep.result_type, index.span);

    // Load the value
    LoadInst load;
    load.ptr = ptr;
    load.result_type = result_type;

    return emit(load, result_type, index.span);
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

    // Save variable state before branching - both branches start with same values
    auto pre_branch_vars = ctx_.variables;

    emit_cond_branch(cond, then_block, else_block);

    // Then branch (don't emit terminator yet - we may need to insert store)
    switch_to_block(then_block);
    ctx_.push_drop_scope();
    Value then_val = build_expr(if_expr.then_branch);
    emit_scope_drops();
    ctx_.pop_drop_scope();
    uint32_t then_end = ctx_.current_block;
    bool then_needs_branch = !is_terminated();

    // Save variable state after then branch
    auto then_vars = ctx_.variables;

    // Restore pre-branch state before processing else branch
    // This ensures else branch sees the same variable values as then branch did
    ctx_.variables = pre_branch_vars;

    // Else branch
    switch_to_block(else_block);
    Value else_val;
    uint32_t else_end = else_block;
    bool else_needs_branch = false;
    if (if_expr.else_branch) {
        ctx_.push_drop_scope();
        else_val = build_expr(*if_expr.else_branch);
        emit_scope_drops();
        ctx_.pop_drop_scope();
        else_end = ctx_.current_block;
        else_needs_branch = !is_terminated();
    } else {
        else_val = const_unit();
        else_needs_branch = !is_terminated();
    }

    // Save variable state after else branch
    auto else_vars = ctx_.variables;

    // Emit terminators for both branches
    switch_to_block(then_end);
    if (then_needs_branch) {
        emit_branch(merge_block);
    }
    switch_to_block(else_end);
    if (else_needs_branch) {
        emit_branch(merge_block);
    }

    // Switch to merge block for PHI creation
    switch_to_block(merge_block);

    // Create PHIs for variables that were modified in either branch
    // This is critical for correct SSA form in loops with if-else
    if (then_needs_branch || else_needs_branch) {
        for (const auto& [var_name, pre_val] : pre_branch_vars) {
            if (pre_val.id == INVALID_VALUE)
                continue;

            // Get the value of this variable at the end of each branch
            auto then_it = then_vars.find(var_name);
            auto else_it = else_vars.find(var_name);

            Value then_var_val = (then_it != then_vars.end()) ? then_it->second : pre_val;
            Value else_var_val = (else_it != else_vars.end()) ? else_it->second : pre_val;

            // If both branches have the same value, no PHI needed
            if (then_var_val.id == else_var_val.id) {
                set_variable(var_name, then_var_val);
                continue;
            }

            // Create PHI to merge the different values
            // Only include paths that actually reach the merge block
            PhiInst var_phi;
            var_phi.result_type = then_var_val.type ? then_var_val.type : else_var_val.type;

            if (then_needs_branch) {
                var_phi.incoming.push_back({then_var_val, then_end});
            }
            if (else_needs_branch) {
                var_phi.incoming.push_back({else_var_val, else_end});
            }

            // Only emit PHI if we have multiple incoming values
            if (var_phi.incoming.size() > 1) {
                Value phi_val = emit(var_phi, var_phi.result_type);
                set_variable(var_name, phi_val);
            } else if (var_phi.incoming.size() == 1) {
                // Only one path reaches merge, use that value directly
                set_variable(var_name, var_phi.incoming[0].first);
            }
        }
    }

    // Now handle the if expression result
    if (!result_type->is_unit()) {
        if (result_type->is_aggregate()) {
            // Use alloca+store+load pattern for aggregate types.
            // This enables LLVM's SROA to break aggregates into scalars.
            auto ptr_type = make_pointer_type(result_type, true);
            auto alloca_val = emit_at_entry(AllocaInst{result_type, "_if_merge"}, ptr_type);

            // Store in then branch - need to go back and insert stores
            // Since we already emitted terminators, we need to insert before them
            auto* then_block_ptr = ctx_.current_func->get_block(then_end);
            auto* else_block_ptr = ctx_.current_func->get_block(else_end);

            if (then_block_ptr && then_needs_branch) {
                // Insert store before the terminator
                StoreInst store{alloca_val, then_val, result_type};
                InstructionData store_inst;
                store_inst.inst = store;
                store_inst.result = INVALID_VALUE;
                then_block_ptr->instructions.push_back(store_inst);
            }

            if (else_block_ptr && else_needs_branch) {
                // Insert store before the terminator
                StoreInst store{alloca_val, else_val, result_type};
                InstructionData store_inst;
                store_inst.inst = store;
                store_inst.result = INVALID_VALUE;
                else_block_ptr->instructions.push_back(store_inst);
            }

            // Load at merge (we're already at merge_block)
            return emit(LoadInst{alloca_val, result_type}, result_type);
        } else {
            // Use phi for non-aggregate types (primitives, pointers)
            PhiInst phi;
            if (then_needs_branch) {
                phi.incoming.push_back({then_val, then_end});
            }
            if (else_needs_branch) {
                phi.incoming.push_back({else_val, else_end});
            }
            phi.result_type = result_type;

            if (phi.incoming.size() > 1) {
                return emit(phi, result_type);
            } else if (phi.incoming.size() == 1) {
                return phi.incoming[0].first;
            }
        }
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
    // loop (condition) { body } - conditional loop, same semantics as while

    // Handle loop variable declaration: loop (var i: I64 < N)
    // The variable is initialized to 0 before the loop starts
    if (loop.loop_var.has_value()) {
        const auto& var_decl = *loop.loop_var;
        MirTypePtr mir_type = convert_type(var_decl.type);

        // Create constant 0 for initialization
        // Determine bit width from the type (default to 64-bit signed)
        int bit_width = (mir_type && mir_type->bit_width() > 0) ? mir_type->bit_width() : 64;
        Value zero_val = const_int(0, bit_width, true);

        // Set the variable to 0
        set_variable(var_decl.name, zero_val);
    }

    uint32_t entry_block = ctx_.current_block;
    uint32_t header_block = create_block("loop.header");
    uint32_t body_block = create_block("loop.body");
    uint32_t exit_block = create_block("loop.exit");

    // Save all variables before the loop (their pre-loop values)
    auto pre_loop_vars = ctx_.variables;

    // Branch to header
    emit_branch(header_block);

    // Switch to header block
    switch_to_block(header_block);

    // Create phi nodes for all variables that exist before the loop
    std::unordered_map<std::string, ValueId> phi_map;
    for (const auto& [var_name, var_value] : pre_loop_vars) {
        if (var_value.id == INVALID_VALUE)
            continue;

        PhiInst phi;
        phi.incoming = {{var_value, entry_block}};
        phi.result_type = var_value.type;
        Value phi_result = emit(phi, var_value.type);
        phi_map[var_name] = phi_result.id;
        set_variable(var_name, phi_result);
    }

    // Save header variable values (for condition-false path to exit)
    auto header_vars = ctx_.variables;

    ctx_.loop_stack.push({header_block, exit_block, std::nullopt, {}});

    // Header with condition - evaluate and branch conditionally
    Value cond = build_expr(loop.condition);
    emit_cond_branch(cond, body_block, exit_block);

    // Body
    switch_to_block(body_block);
    ctx_.push_drop_scope();
    (void)build_expr(loop.body);
    emit_scope_drops();
    ctx_.pop_drop_scope();

    uint32_t body_end_block = ctx_.current_block;

    // Complete phi nodes with back-edge values
    if (!is_terminated()) {
        auto* header = ctx_.current_func->get_block(header_block);
        if (header) {
            for (auto& inst : header->instructions) {
                if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                    for (const auto& [var_name, phi_id] : phi_map) {
                        if (inst.result == phi_id) {
                            // For phi back-edges, access the variable directly without
                            // emitting volatile loads. Volatile variables use allocas and
                            // the phi should maintain the alloca pointer, not load from it.
                            auto it = ctx_.variables.find(var_name);
                            if (it != ctx_.variables.end()) {
                                phi->incoming.push_back({it->second, body_end_block});
                            }
                            break;
                        }
                    }
                }
            }
        }
        emit_branch(header_block);
    }

    // Get break sources before popping loop context
    auto break_sources = ctx_.loop_stack.top().break_sources;

    // Exit
    switch_to_block(exit_block);
    ctx_.loop_stack.pop();

    // After a loop, variables used after the loop need correct values.
    // The exit block can be reached from:
    // 1. Header (condition false) - variables have header_vars values
    // 2. Break statements - variables have break_sources values
    for (const auto& [var_name, header_val] : header_vars) {
        if (header_val.id == INVALID_VALUE)
            continue;

        if (break_sources.empty()) {
            // No breaks: the only path to exit is from header, use header values
            set_variable(var_name, header_val);
        } else {
            // Check if any break source has a different value for this variable
            bool needs_phi = false;
            for (const auto& [break_block, break_vars] : break_sources) {
                auto it = break_vars.find(var_name);
                if (it != break_vars.end() && it->second.id != header_val.id) {
                    needs_phi = true;
                    break;
                }
            }

            if (needs_phi) {
                // Multiple paths with different values - need a PHI
                PhiInst exit_phi;
                exit_phi.result_type = header_val.type;

                // Add header path (condition was false)
                exit_phi.incoming.push_back({header_val, header_block});

                for (const auto& [break_block, break_vars] : break_sources) {
                    auto it = break_vars.find(var_name);
                    if (it != break_vars.end()) {
                        exit_phi.incoming.push_back({it->second, break_block});
                    } else {
                        exit_phi.incoming.push_back({header_val, break_block});
                    }
                }

                Value exit_val = emit(exit_phi, header_val.type);
                set_variable(var_name, exit_val);
            } else {
                // All paths have same value or only header path exists
                set_variable(var_name, header_val);
            }
        }
    }

    // Loop returns unit
    return const_unit();
}

auto HirMirBuilder::build_while(const hir::HirWhileExpr& while_expr) -> Value {
    uint32_t entry_block = ctx_.current_block;
    uint32_t header_block = create_block("while.header");
    uint32_t body_block = create_block("while.body");
    uint32_t exit_block = create_block("while.exit");

    // Save all variables before the loop (their pre-loop values)
    auto pre_loop_vars = ctx_.variables;

    // Branch to header
    emit_branch(header_block);

    // Switch to header block
    switch_to_block(header_block);

    // Create phi nodes for all variables that exist before the loop
    std::unordered_map<std::string, ValueId> phi_map;
    for (const auto& [var_name, var_value] : pre_loop_vars) {
        if (var_value.id == INVALID_VALUE)
            continue;

        PhiInst phi;
        phi.incoming = {{var_value, entry_block}};
        phi.result_type = var_value.type;
        Value phi_result = emit(phi, var_value.type);
        phi_map[var_name] = phi_result.id;
        set_variable(var_name, phi_result);
    }

    // Save header variable values (for condition-false path to exit)
    auto header_vars = ctx_.variables;

    ctx_.loop_stack.push({header_block, exit_block, std::nullopt, {}});

    // Header with condition (uses phi values for variables)
    Value cond = build_expr(while_expr.condition);
    emit_cond_branch(cond, body_block, exit_block);

    // Body
    switch_to_block(body_block);
    ctx_.push_drop_scope();
    (void)build_expr(while_expr.body);
    emit_scope_drops();
    ctx_.pop_drop_scope();

    uint32_t body_end_block = ctx_.current_block;

    // Complete phi nodes with back-edge values
    if (!is_terminated()) {
        auto* header = ctx_.current_func->get_block(header_block);
        if (header) {
            for (auto& inst : header->instructions) {
                if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                    for (const auto& [var_name, phi_id] : phi_map) {
                        if (inst.result == phi_id) {
                            // For phi back-edges, access the variable directly without
                            // emitting volatile loads. Volatile variables use allocas and
                            // the phi should maintain the alloca pointer, not load from it.
                            auto it = ctx_.variables.find(var_name);
                            if (it != ctx_.variables.end()) {
                                phi->incoming.push_back({it->second, body_end_block});
                            }
                            break;
                        }
                    }
                }
            }
        }
        emit_branch(header_block);
    }

    // Get break sources before popping loop context
    auto break_sources = ctx_.loop_stack.top().break_sources;

    // Exit
    switch_to_block(exit_block);
    ctx_.loop_stack.pop();

    // After a while loop, variables used after the loop need correct values.
    // The exit block can be reached from:
    // 1. Header (condition false) - variables have header_vars values
    // 2. Break statements - variables have break_sources values
    //
    // If there are no breaks, variables must use header_vars (the only valid
    // values at exit). If there are breaks with different values, we need PHIs.
    for (const auto& [var_name, header_val] : header_vars) {
        if (header_val.id == INVALID_VALUE)
            continue;

        if (break_sources.empty()) {
            // No breaks: the only path to exit is from header, use header values
            set_variable(var_name, header_val);
        } else {
            // Check if any break source has a different value for this variable
            bool needs_phi = false;
            for (const auto& [break_block, break_vars] : break_sources) {
                auto it = break_vars.find(var_name);
                if (it != break_vars.end() && it->second.id != header_val.id) {
                    needs_phi = true;
                    break;
                }
            }

            if (needs_phi) {
                PhiInst exit_phi;
                exit_phi.result_type = header_val.type;

                // Add incoming from header (condition false path)
                exit_phi.incoming.push_back({header_val, header_block});

                // Add incoming from each break source
                for (const auto& [break_block, break_vars] : break_sources) {
                    auto it = break_vars.find(var_name);
                    if (it != break_vars.end()) {
                        exit_phi.incoming.push_back({it->second, break_block});
                    } else {
                        // Variable not in break vars, use header value as fallback
                        exit_phi.incoming.push_back({header_val, break_block});
                    }
                }

                Value exit_val = emit(exit_phi, header_val.type);
                set_variable(var_name, exit_val);
            } else {
                // All paths have same value, use header value
                set_variable(var_name, header_val);
            }
        }
    }

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

    ctx_.loop_stack.push({header_block, exit_block, std::nullopt, {}});

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

    // Record break source: current block and all variable values
    // This is needed to create PHI nodes at the exit block
    loop_ctx.break_sources.push_back({ctx_.current_block, ctx_.variables});

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

    // Track arm results for deferred branch emission
    struct ArmResult {
        Value value;
        uint32_t end_block;
    };
    std::vector<ArmResult> arm_results;

    for (size_t i = 0; i < when.arms.size(); ++i) {
        const auto& arm = when.arms[i];

        uint32_t arm_block = create_block("when.arm" + std::to_string(i));
        uint32_t next_block =
            (i + 1 < when.arms.size()) ? create_block("when.next" + std::to_string(i)) : exit_block;

        // Build pattern match condition
        Value matches = build_pattern_match(arm.pattern, scrutinee);
        emit_cond_branch(matches, arm_block, next_block);

        // Arm body (don't emit terminator yet - we may need to insert store)
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
            arm_results.push_back({arm_result, arm_end});
        }

        if (i + 1 < when.arms.size()) {
            switch_to_block(next_block);
        }
    }

    // Now we know the result type - decide between phi and alloca+store+load
    if (!result_type->is_unit() && !arm_results.empty()) {
        if (result_type->is_aggregate()) {
            // Use alloca+store+load pattern for aggregate types.
            // This enables LLVM's SROA to break aggregates into scalars.
            auto ptr_type = make_pointer_type(result_type, true);
            auto alloca_val = emit_at_entry(AllocaInst{result_type, "_when_merge"}, ptr_type);

            // Insert stores at end of each arm
            for (const auto& arm : arm_results) {
                switch_to_block(arm.end_block);
                emit_void(StoreInst{alloca_val, arm.value, result_type});
                emit_branch(exit_block);
            }

            // Load at exit
            switch_to_block(exit_block);
            return emit(LoadInst{alloca_val, result_type}, result_type);
        } else {
            // Use phi for non-aggregate types
            std::vector<std::pair<Value, uint32_t>> phi_inputs;
            for (const auto& arm : arm_results) {
                switch_to_block(arm.end_block);
                emit_branch(exit_block);
                phi_inputs.push_back({arm.value, arm.end_block});
            }

            switch_to_block(exit_block);
            PhiInst phi;
            phi.incoming = std::move(phi_inputs);
            phi.result_type = result_type;
            return emit(phi, result_type);
        }
    }

    // Unit type or no results - emit deferred terminators
    for (const auto& arm : arm_results) {
        switch_to_block(arm.end_block);
        emit_branch(exit_block);
    }

    switch_to_block(exit_block);
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
    return emit(inst, result_type, s.span);
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
    return emit(inst, result_type, e.span);
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

    return emit(inst, inst.result_type, tuple.span);
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

    return emit(inst, result_type, arr.span);
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

    return emit(inst, result_type, arr.span);
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

    return emit(inst, target_type, cast.span);
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

    return emit(inst, result_type, closure.span);
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

    return emit(inst, result_type, await_expr.span);
}

// ============================================================================
// Assignment Expressions
// ============================================================================

auto HirMirBuilder::build_assign(const hir::HirAssignExpr& assign) -> Value {
    Value rhs = build_expr(assign.value);

    // Get the target as pointer
    // For simple variable assignment, update the variable map
    if (auto* var = std::get_if<hir::HirVarExpr>(&assign.target->kind)) {
        // Check if this is a volatile variable - need to emit volatile store
        if (ctx_.volatile_vars.count(var->name) > 0) {
            auto it = ctx_.variables.find(var->name);
            if (it != ctx_.variables.end()) {
                Value alloca_ptr = it->second;
                StoreInst store;
                store.ptr = alloca_ptr;
                store.value = rhs;
                store.value_type = rhs.type;
                store.is_volatile = true;
                emit_void(store);
                return const_unit();
            }
        }
        set_variable(var->name, rhs);
        return const_unit();
    }

    // For field assignment on dereferenced pointer: (*ptr).field = value
    // We need to GEP to the field address and store there
    if (auto* field_expr = std::get_if<hir::HirFieldExpr>(&assign.target->kind)) {
        // Check if the object is a deref expression: (*ptr).field
        // Deref is represented as HirUnaryExpr with op == Deref
        if (auto* unary_expr = std::get_if<hir::HirUnaryExpr>(&field_expr->object->kind)) {
            if (unary_expr->op == hir::HirUnaryOp::Deref) {
                // Get the pointer being dereferenced
                Value ptr = build_expr(unary_expr->operand);

                // Get the struct type from the deref result type (the type after dereferencing)
                // The deref expression's type is the pointee (struct) type
                MirTypePtr struct_type;
                if (unary_expr->type) {
                    struct_type = convert_type(unary_expr->type);
                } else {
                    // Fallback: get pointee from ptr's type
                    if (ptr.type && std::holds_alternative<MirPointerType>(ptr.type->kind)) {
                        struct_type = std::get<MirPointerType>(ptr.type->kind).pointee;
                    }
                }

                // GEP to the field within the struct: ptr[0][field_index]
                MirTypePtr field_ptr_type = make_pointer_type(rhs.type, false);
                Value zero_idx = const_int(0, 32, false);
                Value field_idx =
                    const_int(static_cast<int64_t>(field_expr->field_index), 32, false);

                GetElementPtrInst gep;
                gep.base = ptr;
                gep.base_type = struct_type; // Use struct type, not pointer type
                gep.indices = {zero_idx, field_idx};
                gep.result_type = field_ptr_type;

                Value field_ptr = emit(gep, field_ptr_type, assign.span);

                // Store to the field pointer
                StoreInst store;
                store.ptr = field_ptr;
                store.value = rhs;
                store.value_type = rhs.type;
                emit_void(store);
                return const_unit();
            }
        }

        // Check if object is a direct pointer to struct (e.g., this.field where this is Ptr[T])
        Value base = build_expr(field_expr->object);
        MirTypePtr base_type = base.type;

        // Get the struct type for the base
        MirTypePtr struct_type;
        bool is_ptr = false;
        if (base_type) {
            if (std::holds_alternative<MirPointerType>(base_type->kind)) {
                const auto& ptr = std::get<MirPointerType>(base_type->kind);
                struct_type = ptr.pointee;
                is_ptr = true;
            } else if (std::holds_alternative<MirPrimitiveType>(base_type->kind)) {
                const auto& prim = std::get<MirPrimitiveType>(base_type->kind);
                if (prim.kind == PrimitiveType::Ptr) {
                    is_ptr = true;
                    // Get struct type from the field object's type
                    if (field_expr->object) {
                        auto obj_type = field_expr->object->type();
                        if (obj_type) {
                            if (obj_type->is<types::NamedType>()) {
                                const auto& named = obj_type->as<types::NamedType>();
                                struct_type = make_struct_type(named.name);
                            } else if (obj_type->is<types::PtrType>()) {
                                const auto& ptr = obj_type->as<types::PtrType>();
                                if (ptr.inner && ptr.inner->is<types::NamedType>()) {
                                    const auto& inner_named = ptr.inner->as<types::NamedType>();
                                    struct_type = make_struct_type(inner_named.name);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (is_ptr) {
            // GEP to the field within the struct
            MirTypePtr field_ptr_type = make_pointer_type(rhs.type, false);

            // Create constant indices for GEP: base[0][field_index]
            // First index 0 dereferences the pointer, second is field index
            Value zero_idx = const_int(0, 32, false);
            Value field_idx = const_int(static_cast<int64_t>(field_expr->field_index), 32, false);

            GetElementPtrInst gep;
            gep.base = base;
            gep.base_type = struct_type ? struct_type : base_type;
            gep.indices = {zero_idx, field_idx};
            gep.result_type = field_ptr_type;

            Value field_ptr = emit(gep, field_ptr_type, assign.span);

            // Store to the field pointer
            StoreInst store;
            store.ptr = field_ptr;
            store.value = rhs;
            store.value_type = rhs.type;
            emit_void(store);
            return const_unit();
        }
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

    Value result = emit(bin, result_type, assign.span);

    // Store back
    if (auto* var = std::get_if<hir::HirVarExpr>(&assign.target->kind)) {
        // Check if this is a volatile variable - need to emit volatile store
        if (ctx_.volatile_vars.count(var->name) > 0) {
            auto it = ctx_.variables.find(var->name);
            if (it != ctx_.variables.end()) {
                Value alloca_ptr = it->second;
                StoreInst store;
                store.ptr = alloca_ptr;
                store.value = result;
                store.value_type = result.type;
                store.is_volatile = true;
                emit_void(store);
                return const_unit();
            }
        }
        set_variable(var->name, result);
    }

    return const_unit();
}

// ============================================================================
// Lowlevel Block Expression
// ============================================================================

auto HirMirBuilder::build_lowlevel(const hir::HirLowlevelExpr& lowlevel) -> Value {
    // Lowlevel blocks are like regular blocks but without safety checks.
    // The HIR already ensures correct typing for the statements and expressions.
    ctx_.push_drop_scope();

    Value result = const_unit();

    for (const auto& stmt : lowlevel.stmts) {
        bool terminated = build_stmt(*stmt);
        if (terminated) {
            ctx_.pop_drop_scope();
            return result; // Block terminated early
        }
    }

    if (lowlevel.expr) {
        result = build_expr(*lowlevel.expr);
    }

    emit_scope_drops();
    ctx_.pop_drop_scope();

    return result;
}

} // namespace tml::mir
