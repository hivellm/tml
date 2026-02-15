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

namespace tml::mir {

// ============================================================================
// Concrete Expression Building — Data Expressions
// ============================================================================

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
