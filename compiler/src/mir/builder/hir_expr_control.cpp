//! # HIR Expression Lowering to MIR — Control Flow, Construction, and Assignments
//!
//! This file contains the second half of expression lowering:
//! - Control flow (return, break, continue)
//! - Pattern matching (when)
//! - Struct/Enum/Tuple/Array construction
//! - Cast, Closure, Try, Await expressions
//! - Assignment and compound assignment
//! - Lowlevel block expressions
//!
//! See also: hir_expr.cpp for the primary expression dispatch and core expressions.

#include "mir/hir_mir_builder.hpp"

#include <stdexcept>

namespace tml::mir {

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
        // Check if this is a mutable struct variable - need to emit store
        if (ctx_.mut_struct_vars.count(var->name) > 0) {
            auto it = ctx_.variables.find(var->name);
            if (it != ctx_.variables.end()) {
                Value alloca_ptr = it->second;
                StoreInst store;
                store.ptr = alloca_ptr;
                store.value = rhs;
                store.value_type = rhs.type;
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
        // Check if this is a mutable struct variable - need to emit store
        if (ctx_.mut_struct_vars.count(var->name) > 0) {
            auto it = ctx_.variables.find(var->name);
            if (it != ctx_.variables.end()) {
                Value alloca_ptr = it->second;
                StoreInst store;
                store.ptr = alloca_ptr;
                store.value = result;
                store.value_type = result.type;
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
