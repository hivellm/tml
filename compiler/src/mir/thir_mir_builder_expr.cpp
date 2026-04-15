TML_MODULE("compiler")

//! # THIR to MIR Builder — Expression, Pattern & Helper Methods
//!
//! This file contains the second half of the ThirMirBuilder implementation:
//! - Data expression builders (struct, enum, tuple, array, cast, assign, etc.)
//! - Pattern building (binding and matching)
//! - Helper methods (emit, const, variable, operator conversion, drops)
//!
//! The first half (constructor, declarations, type conversion, expression dispatch,
//! coercion, statements, and control-flow expression builders) lives in
//! thir_mir_builder.cpp.

#include "mir/thir_mir_builder.hpp"

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

// ============================================================================
// Concrete Expression Building — Data Expressions
// ============================================================================

auto ThirMirBuilder::build_struct_expr(const thir::ThirStructExpr& s) -> Value {
    auto result_type = convert_type(s.type);

    // When struct update syntax (..base) is present, we need ALL fields:
    // - Explicitly provided fields use the given expression
    // - Missing fields are extracted from the base expression
    if (s.base.has_value()) {
        auto base_val = build_expr(s.base.value());

        // Build set of explicitly provided field names
        std::unordered_set<std::string> provided;
        std::unordered_map<std::string, const thir::ThirExprPtr*> provided_exprs;
        for (const auto& [name, expr] : s.fields) {
            provided.insert(name);
            provided_exprs[name] = &expr;
        }

        // Look up the struct definition to get ALL field names in order
        auto struct_def = env_.lookup_struct(s.struct_name);
        if (struct_def.has_value()) {
            std::vector<Value> fields;
            std::vector<MirTypePtr> field_types_vec;
            std::vector<bool> from_base_flags;

            for (size_t i = 0; i < struct_def->fields.size(); ++i) {
                const auto& field_def = struct_def->fields[i];
                if (provided.count(field_def.name) != 0) {
                    auto val = build_expr(*provided_exprs[field_def.name]);
                    fields.push_back(val);
                    field_types_vec.push_back(val.type);
                    from_base_flags.push_back(false);
                } else {
                    // Field comes from base — store base_val as the field value.
                    // The MIR codegen emitter will extract field [i] from base_val
                    // using extractvalue before the insertvalue chain.
                    auto field_mir_type = convert_type(field_def.type);
                    fields.push_back(base_val);
                    field_types_vec.push_back(field_mir_type);
                    from_base_flags.push_back(true);
                }
            }

            StructInitInst inst;
            inst.struct_name = s.struct_name;
            inst.fields = std::move(fields);
            inst.field_types = std::move(field_types_vec);
            inst.base = base_val;
            inst.from_base = std::move(from_base_flags);
            return emit(std::move(inst), result_type, s.span);
        }
    }

    // No struct update — emit only explicitly provided fields
    std::vector<Value> fields;
    std::vector<MirTypePtr> field_types_vec;

    for (const auto& field_pair : s.fields) {
        auto val = build_expr(field_pair.second);
        fields.push_back(val);
        field_types_vec.push_back(val.type);
    }

    StructInitInst inst;
    inst.struct_name = s.struct_name;
    inst.fields = std::move(fields);
    inst.field_types = std::move(field_types_vec);
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
    // Extract element types from context (e.g., return type is ([U8; 4], I64))
    std::vector<types::TypePtr> context_elem_types;
    if (context_type_) {
        if (auto* tup = std::get_if<types::TupleType>(&context_type_->kind)) {
            context_elem_types = tup->elements;
        }
    }

    std::vector<Value> elements;
    std::vector<MirTypePtr> element_types;

    for (size_t i = 0; i < tuple.elements.size(); ++i) {
        // Propagate per-element context type
        auto saved_context = context_type_;
        if (i < context_elem_types.size()) {
            context_type_ = context_elem_types[i];
        }
        auto val = build_expr(tuple.elements[i]);
        context_type_ = saved_context;
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
    // Extract element type from context (e.g., var x: [U8; 4] = [0, 0, 0, 0])
    types::TypePtr context_elem_src_type;
    if (context_type_) {
        if (auto* arr_t = std::get_if<types::ArrayType>(&context_type_->kind)) {
            context_elem_src_type = arr_t->element;
        }
    }

    // Propagate element type context to child expressions
    auto saved_context = context_type_;
    if (context_elem_src_type) {
        context_type_ = context_elem_src_type;
    }

    std::vector<Value> elements;
    for (const auto& elem : arr.elements) {
        elements.push_back(build_expr(elem));
    }

    context_type_ = saved_context;

    // Determine element type: context > THIR annotations > inferred from elements
    MirTypePtr element_type;
    if (context_elem_src_type) {
        element_type = convert_type(context_elem_src_type);
    }
    if (!element_type && arr.element_type) {
        element_type = convert_type(arr.element_type);
    }
    if (!element_type && arr.type) {
        if (auto* arr_t = std::get_if<types::ArrayType>(&arr.type->kind)) {
            element_type = convert_type(arr_t->element);
        }
    }
    if (!element_type) {
        element_type = elements.empty() ? make_unit_type() : elements[0].type;
    }
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

    // Check for dyn trait object cast: `ref T as ref dyn Behavior`
    if (cast.type) {
        if (auto* ref_type = std::get_if<types::RefType>(&cast.type->kind)) {
            if (ref_type->inner && ref_type->inner->is<types::DynBehaviorType>()) {
                const auto& dyn = ref_type->inner->as<types::DynBehaviorType>();
                // Extract concrete type name from the source expression's type
                std::string concrete_type;
                // The source expr's semantic type should be available via cast.expr->type()
                // But we need to look at the MIR type or the semantic type
                // For `ref d as ref dyn Greetable`, the source is ptr to Dog
                // We get the concrete type from the semantic type if available
                if (cast.target_type) {
                    // cast.target_type is the FULL target type (ref dyn Behavior)
                    // cast.expr carries the source expression
                    // We need the concrete type from the source
                }
                // Try to get concrete type from the ThirExpr's type
                auto source_type = cast.expr ? cast.expr->type() : nullptr;
                if (source_type) {
                    types::TypePtr unwrapped = source_type;
                    if (unwrapped->is<types::RefType>()) {
                        unwrapped = unwrapped->as<types::RefType>().inner;
                    }
                    if (unwrapped && unwrapped->is<types::NamedType>()) {
                        concrete_type = unwrapped->as<types::NamedType>().name;
                    }
                }

                if (!concrete_type.empty()) {
                    MakeDynObjectInst inst;
                    inst.data_ptr = val;
                    inst.concrete_type = concrete_type;
                    inst.behavior_name = dyn.behavior_name;
                    inst.result_type = result_type;
                    return emit(std::move(inst), result_type, cast.span);
                }
            }
        }
    }

    CastInst inst;
    inst.operand = val;
    inst.source_type = val.type;
    inst.target_type = result_type;

    // Determine correct cast kind based on source and target types
    inst.kind = CastKind::Bitcast; // default fallback
    if (val.type && result_type) {
        if (val.type->is_integer() && result_type->is_integer()) {
            int src_bits = val.type->bit_width();
            int dst_bits = result_type->bit_width();
            if (src_bits > dst_bits) {
                inst.kind = CastKind::Trunc;
            } else if (src_bits < dst_bits) {
                inst.kind = val.type->is_signed() ? CastKind::SExt : CastKind::ZExt;
            }
        } else if (val.type->is_float() && result_type->is_integer()) {
            inst.kind = result_type->is_signed() ? CastKind::FPToSI : CastKind::FPToUI;
        } else if (val.type->is_integer() && result_type->is_float()) {
            inst.kind = val.type->is_signed() ? CastKind::SIToFP : CastKind::UIToFP;
        } else if (val.type->is_float() && result_type->is_float()) {
            int src_bits = val.type->bit_width();
            int dst_bits = result_type->bit_width();
            if (src_bits > dst_bits) {
                inst.kind = CastKind::FPTrunc;
            } else if (src_bits < dst_bits) {
                inst.kind = CastKind::FPExt;
            }
        } else if (val.type->is_pointer() && result_type->is_integer()) {
            inst.kind = CastKind::PtrToInt;
        } else if (val.type->is_integer() && result_type->is_pointer()) {
            inst.kind = CastKind::IntToPtr;
        } else if (std::holds_alternative<MirFunctionType>(val.type->kind) &&
                   result_type->is_integer()) {
            // Function references are pointers at the LLVM level (ptr @func_name)
            inst.kind = CastKind::PtrToInt;
        } else if (val.type->is_integer() &&
                   std::holds_alternative<MirFunctionType>(result_type->kind)) {
            inst.kind = CastKind::IntToPtr;
        }
    }

    return emit(std::move(inst), result_type, cast.span);
}

auto ThirMirBuilder::build_closure(const thir::ThirClosureExpr& closure) -> Value {
    // Convert the closure's overall function/closure type to get accurate param and return types.
    // Individual param types from closure.params may be Unit when the closure has no type
    // annotations (e.g., `do(x) x + 1`), but the overall type from the type checker has
    // the correct inferred types.
    auto result_type = convert_type(closure.type);
    MirTypePtr return_type = make_unit_type();
    std::vector<MirTypePtr> param_types;

    if (auto* ft = std::get_if<MirFunctionType>(&result_type->kind)) {
        if (ft->return_type) {
            return_type = ft->return_type;
        }
        param_types = ft->params;
    }

    // Fallback: if return_type is still Unit, extract from the semantic type directly.
    if (return_type->is_unit() && closure.type) {
        types::TypePtr sem_ret;
        if (closure.type->is<types::FuncType>()) {
            sem_ret = closure.type->as<types::FuncType>().return_type;
        } else if (closure.type->is<types::ClosureType>()) {
            sem_ret = closure.type->as<types::ClosureType>().return_type;
        }
        if (sem_ret) {
            auto converted_ret = convert_type(sem_ret);
            if (converted_ret && !converted_ret->is_unit()) {
                return_type = converted_ret;
            }
        }
    }

    // Fall back to individual param types if function type doesn't have them
    if (param_types.empty()) {
        for (const auto& param : closure.params) {
            param_types.push_back(convert_type(param.second));
        }
    }

    // Generate unique name for the closure function
    static uint32_t closure_counter = 0;
    std::string func_name = "__closure_" + std::to_string(closure_counter++);

    // Create the closure function
    Function closure_func;
    closure_func.name = func_name;
    closure_func.return_type = return_type;

    // Add parameters
    ValueId value_id = 0;
    for (size_t i = 0; i < closure.params.size(); ++i) {
        std::string param_name = closure.params[i].first;
        closure_func.params.push_back({param_name, param_types[i], value_id});
        value_id++;
    }

    // Create entry block
    closure_func.blocks.emplace_back();
    closure_func.blocks[0].id = 0;
    closure_func.blocks[0].name = "entry";
    closure_func.next_block_id = 1;
    closure_func.next_value_id = value_id;

    // Save current builder context.
    // ctx_.current_func points into module_.functions (set by build_function at line 122),
    // so push_back can invalidate it. Save the index for safe restore.
    size_t saved_func_idx = static_cast<size_t>(ctx_.current_func - module_.functions.data());
    auto saved_block = ctx_.current_block;
    auto saved_vars = ctx_.variables;
    auto saved_return_type = current_return_type_;

    // Switch to the closure function context
    ctx_.current_func = &closure_func;
    ctx_.current_block = 0;
    ctx_.variables.clear();
    // Set the semantic return type for the closure so that `return` statements
    // inside the body emit the correct `ret <type>` instead of `ret void`.
    if (closure.type) {
        if (closure.type->is<types::FuncType>()) {
            current_return_type_ = closure.type->as<types::FuncType>().return_type;
        } else if (closure.type->is<types::ClosureType>()) {
            current_return_type_ = closure.type->as<types::ClosureType>().return_type;
        } else {
            current_return_type_ = nullptr;
        }
    } else {
        current_return_type_ = nullptr;
    }

    // Bind parameters to variables
    for (size_t i = 0; i < closure.params.size(); ++i) {
        Value param_val;
        param_val.id = closure_func.params[i].value_id;
        param_val.type = param_types[i];
        ctx_.variables[closure.params[i].first] = param_val;
    }

    // Build the closure body
    Value body_val = build_expr(closure.body);

    // Add return if not already terminated
    if (!is_terminated()) {
        if (!return_type->is_unit()) {
            emit_return(body_val);
        } else {
            emit_return();
        }
    }

    // Add closure function to module (may reallocate module_.functions)
    module_.functions.push_back(std::move(closure_func));

    // Restore context using saved index (pointer may have been invalidated)
    ctx_.current_func = &module_.functions[saved_func_idx];
    ctx_.current_block = saved_block;
    ctx_.variables = saved_vars;
    current_return_type_ = saved_return_type;

    // Build captured values for the ClosureInitInst
    std::vector<std::pair<std::string, Value>> captured_values;
    std::vector<std::pair<std::string, MirTypePtr>> capture_types;

    for (const auto& cap : closure.captures) {
        Value cap_value = get_variable(cap.name);
        MirTypePtr cap_type = convert_type(cap.type);

        if (!cap.by_move && cap.is_mut) {
            captured_values.push_back({cap.name, cap_value});
            capture_types.push_back({cap.name, make_pointer_type(cap_type, true)});
        } else {
            captured_values.push_back({cap.name, cap_value});
            capture_types.push_back({cap.name, cap_type});
        }
    }

    // Emit ClosureInitInst to create the fat pointer { func_ptr, env_ptr }
    ClosureInitInst inst;
    inst.func_name = func_name;
    inst.captures = std::move(captured_values);
    inst.cap_types = std::move(capture_types);
    inst.func_type = result_type;
    inst.result_type = result_type;

    return emit(std::move(inst), result_type, closure.span);
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
    // Exception: alloca-backed struct vars need store to the alloca pointer.
    if (assign.target->is<thir::ThirVarExpr>()) {
        const auto& var = assign.target->as<thir::ThirVarExpr>();
        if (ctx_.mut_struct_vars.count(var.name) > 0) {
            Value alloca_ptr = get_variable(var.name);
            if (alloca_ptr.type) {
                if (auto* ptr_t = std::get_if<MirPointerType>(&alloca_ptr.type->kind)) {
                    if (ptr_t->pointee && ptr_t->pointee->is_struct()) {
                        StoreInst store;
                        store.ptr = alloca_ptr;
                        store.value = value;
                        store.value_type = value.type;
                        emit_void(std::move(store), assign.span);
                        return const_unit();
                    }
                }
            }
        }
        set_variable(var.name, value);
        return const_unit();
    }

    // For index targets (arr[i] = val), build only GEP without the load.
    // build_index always does GEP + LOAD, but we need just the GEP pointer for store.
    if (assign.target->is<thir::ThirIndexExpr>()) {
        const auto& index = assign.target->as<thir::ThirIndexExpr>();
        auto idx = build_expr(index.index);
        auto result_type = convert_type(index.type);

        // For mutable array variables stored via alloca, use the alloca pointer directly.
        // build_expr would load the array value, but GEP needs a pointer to the array.
        Value object;
        MirTypePtr base_type;
        bool using_alloca = false;
        if (index.object->is<thir::ThirVarExpr>()) {
            const auto& var = index.object->as<thir::ThirVarExpr>();
            if (ctx_.mut_struct_vars.count(var.name) > 0) {
                Value alloca_ptr = get_variable(var.name);
                if (alloca_ptr.type) {
                    if (auto* ptr_t = std::get_if<MirPointerType>(&alloca_ptr.type->kind)) {
                        if (ptr_t->pointee && ptr_t->pointee->is_array()) {
                            object = alloca_ptr;
                            base_type = ptr_t->pointee; // The array type, not the pointer type
                            using_alloca = true;
                        }
                    }
                }
            }
        }
        if (!using_alloca) {
            object = build_expr(index.object);
            base_type = object.type;
        }

        GetElementPtrInst gep;
        gep.base = object;
        gep.indices = {idx};
        gep.base_type = base_type;
        gep.result_type = make_pointer_type(result_type, false);

        if (base_type) {
            if (auto* arr_type = std::get_if<MirArrayType>(&base_type->kind)) {
                gep.known_array_size = static_cast<int64_t>(arr_type->size);
            }
        }

        Value ptr = emit(std::move(gep), gep.result_type, index.span);

        StoreInst store;
        store.ptr = ptr;
        store.value = value;
        store.value_type = value.type;
        emit_void(std::move(store), assign.span);
        return const_unit();
    }

    // For field targets (p.x = val), use GEP on the alloca-backed struct pointer.
    // If the base object is an alloca-backed mutable struct variable, we GEP to
    // the field and store directly. For pointer-based objects, the generic
    // fallback handles it (build_expr on field access yields the address).
    if (assign.target->is<thir::ThirFieldExpr>()) {
        const auto& field = assign.target->as<thir::ThirFieldExpr>();

        // Check if the object is a mutable struct variable backed by alloca
        if (field.object->is<thir::ThirVarExpr>()) {
            const auto& var = field.object->as<thir::ThirVarExpr>();
            if (ctx_.mut_struct_vars.count(var.name) > 0) {
                Value alloca_ptr = get_variable(var.name);
                if (alloca_ptr.type) {
                    if (auto* ptr_t = std::get_if<MirPointerType>(&alloca_ptr.type->kind)) {
                        MirTypePtr struct_type = ptr_t->pointee;
                        if (struct_type && struct_type->is_struct()) {
                            // GEP to the field: alloca_ptr[0][field_index]
                            Value zero_idx = const_int(0, 32, false);
                            Value field_idx =
                                const_int(static_cast<int64_t>(field.field_index), 32, false);

                            auto field_ptr_type = make_pointer_type(value.type, false);

                            GetElementPtrInst gep;
                            gep.base = alloca_ptr;
                            gep.base_type = struct_type;
                            gep.indices = {zero_idx, field_idx};
                            gep.result_type = field_ptr_type;

                            Value ptr = emit(std::move(gep), field_ptr_type, field.span);

                            StoreInst store;
                            store.ptr = ptr;
                            store.value = value;
                            store.value_type = value.type;
                            emit_void(std::move(store), assign.span);
                            return const_unit();
                        }
                    }
                }
            }
        }

        // For nested field access (e.g., p.inner.x) or pointer-based field access,
        // try to resolve the chain to an alloca base.
        // Walk up the chain: if the root is an alloca-backed struct var, emit
        // a chain of GEPs. Otherwise, fall through to the generic store path.
    }

    // For non-variable targets (field access, etc.), use memory store
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
        // Normalize :: to __ to match function definition mangling
        {
            size_t pos = 0;
            while ((pos = call.func_name.find("::", pos)) != std::string::npos) {
                call.func_name.replace(pos, 2, "__");
                pos += 2;
            }
        }
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
        throw std::runtime_error("[M003] ThirMirBuilder: No function being built");
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
        throw std::runtime_error("[M003] ThirMirBuilder: No function being built");
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
        throw std::runtime_error("[M003] ThirMirBuilder: No function being built");
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

void ThirMirBuilder::emit_switch(Value discriminant,
                                 std::vector<std::pair<int64_t, uint32_t>> cases,
                                 uint32_t default_block) {
    if (!ctx_.current_func)
        return;
    auto* block = ctx_.current_func->get_block(ctx_.current_block);
    if (!block || block->terminator.has_value())
        return;
    block->terminator = SwitchTerm{discriminant, std::move(cases), default_block};
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

void ThirMirBuilder::emit_drop_for_value(Value value, const MirTypePtr& type,
                                         const std::string& type_name) {
    // Get the effective type name for drop checking
    std::string effective_type_name = type_name;
    if (effective_type_name.empty() && type) {
        effective_type_name = get_type_name(type);
    }

    // Skip if type doesn't need drop at all
    if (!effective_type_name.empty() && !env_.type_needs_drop(effective_type_name)) {
        return;
    }
    // If we still don't have a type name, check for array types
    if (effective_type_name.empty() && type) {
        if (!std::holds_alternative<MirArrayType>(type->kind)) {
            return;
        }
    }

    // Step 1: If type explicitly implements Drop, call Type::drop(value)
    bool has_explicit_drop =
        !effective_type_name.empty() && env_.type_implements(effective_type_name, "Drop");
    if (has_explicit_drop) {
        CallInst call;
        call.func_name = effective_type_name + "::drop";
        call.args = {value};
        call.arg_types = {type};
        call.return_type = make_unit_type();
        emit_void(std::move(call));
    }

    // Step 2: Recursively drop fields for structs (in reverse declaration order)
    if (!effective_type_name.empty()) {
        auto struct_def = env_.lookup_struct(effective_type_name);
        if (struct_def && !struct_def->fields.empty()) {
            size_t total_fields = struct_def->fields.size();
            for (size_t i = total_fields; i > 0; --i) {
                size_t field_idx = i - 1;
                const auto& fld = struct_def->fields[field_idx];

                if (!env_.type_needs_drop(fld.type)) {
                    continue;
                }

                // Get type name for the field
                std::string field_type_name;
                if (fld.type && fld.type->is<types::NamedType>()) {
                    field_type_name = fld.type->as<types::NamedType>().name;
                } else if (fld.type && fld.type->is<types::PrimitiveType>() &&
                           fld.type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str) {
                    field_type_name = "Str";
                }

                MirTypePtr field_mir_type;
                if (!field_type_name.empty()) {
                    field_mir_type = make_struct_type(field_type_name);
                } else {
                    field_mir_type = make_ptr_type();
                }

                ExtractValueInst extract;
                extract.aggregate = value;
                extract.indices = {static_cast<uint32_t>(field_idx)};
                extract.aggregate_type = type;
                extract.result_type = field_mir_type;

                Value field_value = emit(std::move(extract), field_mir_type);
                emit_drop_for_value(field_value, field_mir_type, field_type_name);
            }
        }
    }

    // Step 3: Handle arrays - iterate elements and drop each (in reverse order)
    if (type && std::holds_alternative<MirArrayType>(type->kind)) {
        const auto& arr_type = std::get<MirArrayType>(type->kind);
        std::string elem_type_name = get_type_name(arr_type.element);

        if (!elem_type_name.empty() && env_.type_needs_drop(elem_type_name)) {
            for (size_t i = arr_type.size; i > 0; --i) {
                ExtractValueInst extract;
                extract.aggregate = value;
                extract.indices = {static_cast<uint32_t>(i - 1)};
                extract.aggregate_type = type;
                extract.result_type = arr_type.element;

                Value elem_value = emit(std::move(extract), arr_type.element);
                emit_drop_for_value(elem_value, arr_type.element, elem_type_name);
            }
        }
    }
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
