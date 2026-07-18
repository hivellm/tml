TML_MODULE("compiler")

//! # THIR to MIR Builder — Core Implementation
//!
//! This file contains the first half of the ThirMirBuilder implementation:
//! - Constructor and main entry point
//! - Declaration building (struct, enum, impl, function)
//! - Type conversion
//! - Expression dispatch and coercion building
//! - Statement building
//! - Control-flow expression builders (literal, var, binary, unary, call,
//!   method_call, field, index, if, block, loop, while, for, return,
//!   break, continue, when)
//!
//! The second half (data expression builders, pattern building, and helper
//! methods) lives in thir_mir_builder_expr.cpp.

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

    // Collect behavior definitions for vtable generation
    for (const auto& behavior : thir_module.behaviors) {
        BehaviorDef bdef;
        bdef.name = behavior.name;
        for (const auto& method : behavior.methods) {
            bdef.methods.push_back(method.name);
        }
        module_.behaviors.push_back(std::move(bdef));
    }

    for (const auto& impl : thir_module.impls) {
        build_impl(impl);

        // Collect impl metadata for vtable generation (only trait impls)
        if (impl.behavior_name.has_value() && !impl.behavior_name->empty()) {
            ImplDef idef;
            idef.type_name = impl.type_name;
            idef.behavior_name = *impl.behavior_name;
            for (const auto& method : impl.methods) {
                std::string func_name =
                    method.mangled_name.empty() ? method.name : method.mangled_name;
                idef.method_functions[method.name] = func_name;
            }
            module_.impls.push_back(std::move(idef));
        }
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
    mir_func.attributes = func.attributes;
    if (func.route_info.has_value()) {
        mir_func.route_info = RouteInfo{
            .method = static_cast<RouteMethod>(static_cast<int>(func.route_info->method)),
            .path = func.route_info->path,
        };
    }

    // Add parameters
    for (const auto& param : func.params) {
        FunctionParam p;
        p.name = param.name;
        p.type = convert_type(param.type);
        p.value_id = mir_func.fresh_value();
        // Propagate HTTP parameter extraction metadata from THIR
        p.extraction.kind =
            static_cast<mir::ParamExtractionKind>(static_cast<int>(param.extraction.kind));
        p.extraction.key = param.extraction.key;
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
    ctx_.push_drop_scope(); // Push initial function-level drop scope
    current_return_type_ = func.return_type;
    context_type_ = nullptr;
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
            emit_all_drops(); // Drop all local variables before implicit return
            if (func.return_type && func.return_type->is<types::PrimitiveType>() &&
                func.return_type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Unit) {
                emit_return();
            } else {
                emit_return(result);
            }
        }
    }

    ctx_.current_func = nullptr;
    current_return_type_ = nullptr;
    context_type_ = nullptr;
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

    if (type->is<types::ClosureType>()) {
        const auto& clos = type->as<types::ClosureType>();
        std::vector<MirTypePtr> params;
        // resolve() is non-const but only reads substitutions_ — safe to const_cast here
        auto& env_mut = const_cast<types::TypeEnv&>(env_);
        for (const auto& p : clos.params) {
            // Resolve type variables before converting (closures may have inferred types)
            auto resolved = env_mut.resolve(p);
            params.push_back(convert_type(resolved));
        }
        auto resolved_ret = env_mut.resolve(clos.return_type);
        auto ret = convert_type(resolved_ret);
        auto mir_type = std::make_shared<MirType>();
        mir_type->kind = MirFunctionType{std::move(params), std::move(ret)};
        return mir_type;
    }

    // Resolve type variables (from type inference) before giving up
    if (type->is<types::TypeVar>()) {
        auto& env_mut = const_cast<types::TypeEnv&>(env_);
        auto resolved = env_mut.resolve(type);
        if (resolved && !resolved->is<types::TypeVar>()) {
            return convert_type(resolved);
        }
    }

    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();

        // Check if it's a type alias — resolve to underlying type
        auto alias = env_.lookup_type_alias(named.name);
        if (alias) {
            return convert_type(*alias);
        }

        std::vector<mir::MirTypePtr> type_args;
        for (const auto& arg : named.type_args) {
            type_args.push_back(convert_type(arg));
        }
        // Check if it's an enum or struct
        if (env_.lookup_enum(named.name)) {
            return make_enum_type(named.name, std::move(type_args));
        }
        return make_struct_type(named.name, std::move(type_args));
    }

    if (type->is<types::ClassType>()) {
        const auto& cls = type->as<types::ClassType>();
        std::vector<mir::MirTypePtr> type_args;
        for (const auto& arg : cls.type_args) {
            type_args.push_back(convert_type(arg));
        }
        // Classes (including sealed classes) are struct-like value types in MIR.
        // Check if this class name is actually an enum (defensive), else make struct.
        if (env_.lookup_enum(cls.name)) {
            return make_enum_type(cls.name, std::move(type_args));
        }
        return make_struct_type(cls.name, std::move(type_args));
    }

    // impl Behavior types are opaque — lower to pointer (fat pointer in future)
    if (type->is<types::ImplBehaviorType>()) {
        return make_pointer_type(make_i8_type(), false);
    }

    // Dynamic trait object types: dyn Behavior -> fat pointer { ptr, ptr }
    if (type->is<types::DynBehaviorType>()) {
        const auto& dyn = type->as<types::DynBehaviorType>();
        std::vector<MirTypePtr> type_args;
        for (const auto& arg : dyn.type_args) {
            type_args.push_back(convert_type(arg));
        }
        return std::make_shared<MirType>(
            MirType{MirDynType{dyn.behavior_name, std::move(type_args)}});
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
        // Propagate declared type as context for literal coercion
        auto saved_context = context_type_;
        if (let.type) {
            context_type_ = let.type;
        }

        // Fast path for array let-bindings: use an alloca-backed variable.
        // This avoids large SSA aggregate values in registers (e.g., [100 x i32])
        // which crash LLVM's x86 SelectionDAG when stored inside loops.
        //
        // For zero-initialized arrays (e.g., `[0; 100]`), we detect the pattern
        // early and emit just the alloca + zeroinitializer store, completely
        // bypassing the ArrayInitInst path (which would emit: alloca, load, then
        // our alloca, store-of-aggregate — a double-alloca that triggers LLVM crash).
        if (let.pattern && let.pattern->is<thir::ThirBindingPattern>()) {
            const auto& bp = let.pattern->as<thir::ThirBindingPattern>();

            // Check if init is an array repeat expression with a zero literal
            bool is_zero_array_repeat = false;
            MirTypePtr array_type_direct = nullptr;
            if ((*let.init)->is<thir::ThirArrayRepeatExpr>()) {
                const auto& repeat = (*let.init)->as<thir::ThirArrayRepeatExpr>();
                if (repeat.value && repeat.value->is<thir::ThirLiteralExpr>()) {
                    const auto& lit = repeat.value->as<thir::ThirLiteralExpr>();
                    bool zero = std::visit(
                        [](const auto& v) -> bool {
                            using T = std::decay_t<decltype(v)>;
                            if constexpr (std::is_same_v<T, int64_t>)
                                return v == 0;
                            else if constexpr (std::is_same_v<T, uint64_t>)
                                return v == 0;
                            else if constexpr (std::is_same_v<T, double>)
                                return v == 0.0;
                            else if constexpr (std::is_same_v<T, bool>)
                                return !v;
                            else
                                return false;
                        },
                        lit.value);
                    if (zero) {
                        MirTypePtr elem_type = convert_type(repeat.value->type());
                        if (elem_type) {
                            array_type_direct = make_array_type(elem_type, repeat.count);
                            is_zero_array_repeat = true;
                        }
                    }
                }
            }

            if (is_zero_array_repeat && array_type_direct) {
                // Create zero-initialized alloca directly — no ArrayInitInst needed.
                // The AllocaInst codegen emits `store [N x T] zeroinitializer` inline,
                // avoiding the double-alloca (alloca+load+alloca+store) that crashes LLVM.
                auto ptr_type = make_pointer_type(array_type_direct, bp.is_mut);
                AllocaInst alloca;
                alloca.alloc_type = array_type_direct;
                alloca.name = bp.name;
                alloca.is_volatile = let.is_volatile;
                alloca.zero_init = true;
                auto alloca_val = emit(std::move(alloca), ptr_type);
                ctx_.variables[bp.name] = alloca_val;
                ctx_.mut_struct_vars.insert(bp.name);
                // Register for drop if needed
                std::string tn = get_type_name(array_type_direct);
                if (!tn.empty() && !env_.is_trivially_destructible(tn)) {
                    ctx_.register_for_drop(bp.name, alloca_val, tn, array_type_direct);
                }
                context_type_ = saved_context;
                return;
            }

            // Non-zero or non-repeat array init: build the expr, then alloca-back.
            auto init_val = build_expr(*let.init);
            context_type_ = saved_context;
            if (init_val.type && init_val.type->is_array()) {
                auto ptr_type = make_pointer_type(init_val.type, bp.is_mut);
                AllocaInst alloca;
                alloca.alloc_type = init_val.type;
                alloca.name = bp.name;
                alloca.is_volatile = let.is_volatile;
                auto alloca_val = emit(std::move(alloca), ptr_type);
                StoreInst store;
                store.ptr = alloca_val;
                store.value = init_val;
                store.value_type = init_val.type;
                emit_void(std::move(store));
                ctx_.variables[bp.name] = alloca_val;
                ctx_.mut_struct_vars.insert(bp.name);
                // Register for drop if needed
                std::string tn = get_type_name(init_val.type);
                if (!tn.empty() && !env_.is_trivially_destructible(tn)) {
                    ctx_.register_for_drop(bp.name, alloca_val, tn, init_val.type);
                }
                return;
            }

            // For mutable struct variables, allocate via alloca so field mutation
            // (p.x = 99) can use GEP + store. Without this, structs live as SSA
            // values (insertvalue chain) which have no address for field stores.
            if (bp.is_mut && init_val.type && init_val.type->is_struct()) {
                auto ptr_type = make_pointer_type(init_val.type, true);
                AllocaInst alloca;
                alloca.alloc_type = init_val.type;
                alloca.name = bp.name;
                alloca.is_volatile = let.is_volatile;
                auto alloca_val = emit(std::move(alloca), ptr_type);
                StoreInst store;
                store.ptr = alloca_val;
                store.value = init_val;
                store.value_type = init_val.type;
                emit_void(std::move(store));
                ctx_.variables[bp.name] = alloca_val;
                ctx_.mut_struct_vars.insert(bp.name);
                // Register for drop if needed
                std::string tn = get_type_name(init_val.type);
                if (!tn.empty() && !env_.is_trivially_destructible(tn)) {
                    ctx_.register_for_drop(bp.name, alloca_val, tn, init_val.type);
                }
                return;
            }

            build_pattern_binding(let.pattern, init_val);
            // Register for drop if needed
            {
                std::string tn = get_type_name(init_val.type);
                if (!tn.empty() && !env_.is_trivially_destructible(tn)) {
                    ctx_.register_for_drop(bp.name, init_val, tn, init_val.type);
                }
            }
            return;
        }

        auto init_val = build_expr(*let.init);
        context_type_ = saved_context;
        build_pattern_binding(let.pattern, init_val);
        // Register for drop if the type needs dropping
        if (let.pattern && let.pattern->is<thir::ThirBindingPattern>()) {
            const auto& bp2 = let.pattern->as<thir::ThirBindingPattern>();
            std::string tn = get_type_name(init_val.type);
            if (!tn.empty() && !env_.is_trivially_destructible(tn)) {
                ctx_.register_for_drop(bp2.name, init_val, tn, init_val.type);
            }
        }
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
                // Prefer context type (from let/var declaration or return type) over
                // literal's own type, since HIR defaults unsuffixed integers to I32
                // even when the type checker resolved them to U8, I64, etc.
                auto type = convert_type(lit.type);
                if (context_type_) {
                    if (auto ctx_converted = convert_type(context_type_)) {
                        if (std::get_if<MirPrimitiveType>(&ctx_converted->kind)) {
                            type = ctx_converted;
                        }
                    }
                }
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
    Value result = get_variable(var.name);

    // If variable not found but type is a FuncType, this is a function reference
    // being passed as a value (e.g., `apply(double, 5)` where `double` is a named function)
    if (result.id == INVALID_VALUE && var.type && var.type->is<types::FuncType>()) {
        MirTypePtr func_type = convert_type(var.type);
        mir::ConstFuncRef func_ref;
        func_ref.func_name = var.name;
        func_ref.func_type = func_type;

        mir::ConstantInst const_inst;
        const_inst.value = func_ref;
        return emit(std::move(const_inst), func_type);
    }

    // For mutable array/struct variables stored via alloca, emit a load to get the current value.
    // The variable holds an alloca pointer; reading the variable should produce the value.
    if (ctx_.mut_struct_vars.count(var.name) > 0 && result.type) {
        if (auto* ptr_type = std::get_if<MirPointerType>(&result.type->kind)) {
            MirTypePtr pointee = ptr_type->pointee;
            // phase27d: scalars are loaded too. A primitive local becomes
            // alloca-backed once its address is taken (`mut ref c` in
            // build_unary), and every later read must observe writes made
            // through that reference instead of the stale SSA binding.
            // Guarded on the binding not itself being a `ref` — a `ref`/`mut ref`
            // parameter holds the pointer as its value and must stay a pointer
            // here (its value-context deref happens in build_binary).
            bool binding_is_ref = var.type && var.type->is<types::RefType>();
            if (pointee && !binding_is_ref &&
                (pointee->is_array() || pointee->is_struct() || pointee->is_primitive())) {
                LoadInst load;
                load.ptr = result;
                load.result_type = pointee;
                return emit(std::move(load), pointee);
            }
        }
    }

    return result;
}

auto ThirMirBuilder::build_binary(const thir::ThirBinaryExpr& bin) -> Value {
    // If operator overloading is resolved, emit as method call
    if (bin.operator_method) {
        auto left = build_expr(bin.left);
        auto right = build_expr(bin.right);
        auto result_type = convert_type(bin.type);

        CallInst inst;
        inst.func_name = bin.operator_method->qualified_name;
        // Normalize :: to __ to match function definition mangling
        {
            size_t pos = 0;
            while ((pos = inst.func_name.find("::", pos)) != std::string::npos) {
                inst.func_name.replace(pos, 2, "__");
                pos += 2;
            }
        }
        inst.args = {left, right};
        inst.arg_types = {left.type, right.type};
        inst.return_type = result_type;
        return emit(std::move(inst), result_type, bin.span);
    }

    // phase27d: binary operands are read in VALUE context, so a `ref`/`mut ref`
    // operand must be dereferenced. This mirrors the type checker's `deref_ref`
    // in check_binary (types/checker/expr_ops.cpp), which types `count + 1`
    // (count: mut ref I64) as the pointee I64 — without the load, codegen would
    // pass the pointer where a scalar is expected (`icmp sge ptr %count, %max`).
    //
    // Only `RefType` is unwrapped; a raw `PtrType` is left intact so genuine
    // pointer arithmetic keeps its pointer operand, matching the checker.
    auto deref_ref_operand = [&](const thir::ThirExprPtr& operand_expr, Value val) -> Value {
        auto ty = operand_expr->type();
        if (!ty || !ty->is<types::RefType>()) {
            return val;
        }
        const auto& inner = ty->as<types::RefType>().inner;
        if (!inner) {
            return val;
        }
        LoadInst load;
        load.ptr = val;
        load.result_type = convert_type(inner);
        return emit(std::move(load), load.result_type, operand_expr->span());
    };

    auto left = deref_ref_operand(bin.left, build_expr(bin.left));
    auto right = deref_ref_operand(bin.right, build_expr(bin.right));

    // The RESULT of an arithmetic binary op is a value, never a reference — an
    // arithmetic node whose THIR type is still `ref T` must produce `T`. This
    // mirrors the checker's `return deref_ref(left)` for Add/Sub/Mul/Div/Mod/
    // Bit*/Shl/Shr. It matters beyond this instruction: MergeReturnsPass types
    // the unified-exit phi from InstructionData.type (merge_returns.cpp:76), so
    // leaving `ref I64` here emits `phi ptr` for an `i64`-returning function.
    auto result_thir_type = bin.type;
    if (result_thir_type && result_thir_type->is<types::RefType>() &&
        result_thir_type->as<types::RefType>().inner) {
        result_thir_type = result_thir_type->as<types::RefType>().inner;
    }
    auto result_type = convert_type(result_thir_type);

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
    // phase27d: `mut ref <local>` must yield the address OF THE VARIABLE, not of a
    // fresh copy. The generic Ref path below allocas a temporary and stores the
    // operand's current SSA value into it — writes through that reference would
    // land in the temporary and the caller would never see them (`bump(mut ref c,
    // 10)` left `c` at its old value).
    //
    // Instead, promote the variable to alloca-backed storage (mut_struct_vars) the
    // first time its address is taken, and hand out that alloca. Membership in
    // mut_struct_vars is also what makes the rest of the builder treat the binding
    // as a stable pointer rather than a loop-invariant SSA constant — see the
    // loop-condition invariance checks in thir_mir_builder_control.cpp.
    if (unary.op == hir::HirUnaryOp::RefMut && unary.operand->is<thir::ThirVarExpr>()) {
        const auto& var = unary.operand->as<thir::ThirVarExpr>();
        Value existing = get_variable(var.name);
        if (existing.id != INVALID_VALUE) {
            if (ctx_.mut_struct_vars.count(var.name) > 0 && existing.type &&
                std::holds_alternative<MirPointerType>(existing.type->kind)) {
                // Already alloca-backed — its address is the alloca itself.
                return existing;
            }
            if (existing.type && !existing.type->is_struct() && !existing.type->is_array()) {
                auto ptr_type = make_pointer_type(existing.type, true);
                AllocaInst alloca;
                alloca.alloc_type = existing.type;
                alloca.name = var.name;
                Value alloca_val = emit_at_entry(std::move(alloca), ptr_type);

                StoreInst store;
                store.ptr = alloca_val;
                store.value = existing;
                store.value_type = existing.type;
                emit_void(std::move(store), unary.span);

                set_variable(var.name, alloca_val);
                ctx_.mut_struct_vars.insert(var.name);
                return alloca_val;
            }
        }
    }

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

    // Propagate generic type arguments (e.g., [MyState] in ptr_read[MyState]).
    // These are critical for intrinsics like ptr_read/ptr_write where the type checker
    // registers a placeholder return type (I32) but codegen needs the actual type.
    for (const auto& ta : call.type_args) {
        inst.type_args.push_back(convert_type(ta));
    }

    // Check if the callee is a local variable holding a function pointer.
    // When `let f: func(I32) -> I32 = add100; f(42)` is compiled, the parser
    // creates a CallExpr with func_name="f". We detect that "f" is a local
    // variable (not a global function) and mark this as an indirect call.
    Value callee_var = get_variable(call.func_name);
    if (callee_var.id != INVALID_VALUE && callee_var.type) {
        if (std::holds_alternative<MirFunctionType>(callee_var.type->kind)) {
            inst.callee = callee_var;
            inst.callee_func_type = callee_var.type;
        }
    }

    return emit(std::move(inst), result_type, call.span);
}

auto ThirMirBuilder::build_method_call(const thir::ThirMethodCallExpr& call) -> Value {
    auto receiver = build_expr(call.receiver);
    auto result_type = convert_type(call.type);

    // Handle dyn dispatch: emit MethodCallInst with dyn flags
    if (call.resolved.is_virtual && call.resolved.behavior_name.has_value()) {
        std::vector<Value> extra_args;
        std::vector<MirTypePtr> extra_arg_types;
        for (const auto& arg : call.args) {
            auto val = build_expr(arg);
            extra_args.push_back(val);
            extra_arg_types.push_back(val.type);
        }

        MethodCallInst inst;
        inst.receiver = receiver;
        // Extract method name from qualified name (e.g., "Greetable::greet" -> "greet")
        std::string method_name = call.resolved.qualified_name;
        auto colon_pos = method_name.rfind("::");
        if (colon_pos != std::string::npos) {
            method_name = method_name.substr(colon_pos + 2);
        }
        inst.method_name = method_name;
        inst.receiver_type = *call.resolved.behavior_name;
        inst.args = std::move(extra_args);
        inst.arg_types = std::move(extra_arg_types);
        inst.return_type = result_type;
        inst.is_dyn_dispatch = true;
        inst.dyn_behavior_name = *call.resolved.behavior_name;
        return emit(std::move(inst), result_type, call.span);
    }

    std::vector<Value> args = {receiver};
    std::vector<MirTypePtr> arg_types = {receiver.type};
    for (const auto& arg : call.args) {
        auto val = build_expr(arg);
        args.push_back(val);
        arg_types.push_back(val.type);
    }

    CallInst inst;
    // Normalize :: to __ in method call names to match function definition mangling
    // (HIR sets mangled_name = "TypeName__method", THIR resolver uses "TypeName::method")
    inst.func_name = call.resolved.qualified_name;
    {
        size_t pos = 0;
        while ((pos = inst.func_name.find("::", pos)) != std::string::npos) {
            inst.func_name.replace(pos, 2, "__");
            pos += 2;
        }
    }
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
    // For alloca-backed array variables (both mutable and immutable), use the
    // alloca pointer directly for GEP to avoid spilling the large SSA aggregate
    // value to a fresh alloca on every access (especially in loops).
    Value object;
    MirTypePtr base_type;
    bool using_alloca_ptr = false;

    if (index.object && index.object->is<thir::ThirVarExpr>()) {
        const auto& var = index.object->as<thir::ThirVarExpr>();
        if (ctx_.mut_struct_vars.count(var.name) > 0) {
            Value alloca_ptr = get_variable(var.name);
            if (alloca_ptr.type) {
                if (auto* ptr_t = std::get_if<MirPointerType>(&alloca_ptr.type->kind)) {
                    if (ptr_t->pointee && ptr_t->pointee->is_array()) {
                        object = alloca_ptr;
                        base_type = ptr_t->pointee;
                        using_alloca_ptr = true;
                    }
                }
            }
        }
    }

    if (!using_alloca_ptr) {
        object = build_expr(index.object);
        base_type = object.type;
    }

    auto idx = build_expr(index.index);
    auto result_type = convert_type(index.type);

    // If the THIR type is missing (null → unit/void), derive element type from
    // the object's array type. This happens when the HIR builder doesn't track
    // the element type through extractvalue + index chains.
    bool is_void = false;
    if (result_type) {
        if (auto* prim = std::get_if<MirPrimitiveType>(&result_type->kind)) {
            is_void = (prim->kind == PrimitiveType::Unit);
        }
    }
    if (!result_type || is_void) {
        if (base_type) {
            if (auto* arr = std::get_if<MirArrayType>(&base_type->kind)) {
                result_type = arr->element;
            }
        }
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

    LoadInst load;
    load.ptr = ptr;
    load.result_type = result_type;
    return emit(std::move(load), result_type, index.span);
}

} // namespace tml::mir
