//! # MIR Builder - Helpers
//!
//! This file implements helper functions used throughout the builder.
//!
//! ## Block Management
//!
//! - `create_block()`: Add new basic block to function
//! - `switch_to_block()`: Change emission target
//! - `is_terminated()`: Check if block has terminator
//!
//! ## Instruction Emission
//!
//! - `emit()`: Emit instruction with result value
//! - `emit_void()`: Emit instruction without result
//! - `emit_return/branch/cond_branch()`: Terminators
//!
//! ## Constants
//!
//! - `const_int()`, `const_float()`, `const_bool()`
//! - `const_string()`, `const_unit()`
//!
//! ## Drop Handling (RAII)
//!
//! - `emit_drop_for_value()`: Emit drop call for value
//! - `emit_scope_drops()`: Drop current scope variables
//! - `emit_all_drops()`: Drop all variables (for return)

#include "mir/mir_builder.hpp"

namespace tml::mir {

// ============================================================================
// Block Management
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

// ============================================================================
// Instruction Emission
// ============================================================================

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

auto MirBuilder::emit_at_entry(Instruction inst, MirTypePtr type) -> Value {
    // Insert instruction at the start of the entry block.
    // This is used for allocas that need to dominate all uses (for LLVM's mem2reg).
    auto& entry = ctx_.current_func->entry_block();

    auto id = ctx_.current_func->fresh_value();

    InstructionData data;
    data.result = id;
    data.type = type;
    data.inst = std::move(inst);

    // Insert at the beginning of the entry block
    entry.instructions.insert(entry.instructions.begin(), std::move(data));

    return {id, type};
}

// ============================================================================
// Terminator Emission
// ============================================================================

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

// ============================================================================
// Constant Creation
// ============================================================================

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

// ============================================================================
// Variable Management
// ============================================================================

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

// ============================================================================
// Operator Conversion
// ============================================================================

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

// ============================================================================
// Drop Helpers (RAII)
// ============================================================================

auto MirBuilder::get_type_name(const MirTypePtr& type) const -> std::string {
    if (!type) {
        return "";
    }

    return std::visit(
        [this](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, MirStructType>) {
                return t.name;
            } else if constexpr (std::is_same_v<T, MirEnumType>) {
                return t.name;
            } else if constexpr (std::is_same_v<T, MirPointerType>) {
                // For pointer types (including class instances), get the underlying type name
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
                case PrimitiveType::I128:
                    return "I128";
                case PrimitiveType::U8:
                    return "U8";
                case PrimitiveType::U16:
                    return "U16";
                case PrimitiveType::U32:
                    return "U32";
                case PrimitiveType::U64:
                    return "U64";
                case PrimitiveType::U128:
                    return "U128";
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

void MirBuilder::emit_drop_calls(const std::vector<BuildContext::DropInfo>& drops) {
    for (const auto& drop_info : drops) {
        emit_drop_for_value(drop_info.value, drop_info.type, drop_info.type_name);
    }
}

auto MirBuilder::get_type_name_from_semantic(const types::TypePtr& type) const -> std::string {
    if (!type) {
        return "";
    }

    return std::visit(
        [](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, types::PrimitiveType>) {
                switch (t.kind) {
                case types::PrimitiveKind::I8:
                    return "I8";
                case types::PrimitiveKind::I16:
                    return "I16";
                case types::PrimitiveKind::I32:
                    return "I32";
                case types::PrimitiveKind::I64:
                    return "I64";
                case types::PrimitiveKind::I128:
                    return "I128";
                case types::PrimitiveKind::U8:
                    return "U8";
                case types::PrimitiveKind::U16:
                    return "U16";
                case types::PrimitiveKind::U32:
                    return "U32";
                case types::PrimitiveKind::U64:
                    return "U64";
                case types::PrimitiveKind::U128:
                    return "U128";
                case types::PrimitiveKind::F32:
                    return "F32";
                case types::PrimitiveKind::F64:
                    return "F64";
                case types::PrimitiveKind::Bool:
                    return "Bool";
                case types::PrimitiveKind::Char:
                    return "Char";
                case types::PrimitiveKind::Unit:
                    return "Unit";
                case types::PrimitiveKind::Never:
                    return "Never";
                default:
                    return "";
                }
            } else if constexpr (std::is_same_v<T, types::NamedType>) {
                return t.name;
            } else {
                return "";
            }
        },
        type->kind);
}

void MirBuilder::emit_drop_for_value(Value value, const MirTypePtr& type,
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
        // Only proceed if it's an array type with elements that need drop
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
            // Drop fields in reverse order (LIFO)
            size_t total_fields = struct_def->fields.size();
            for (size_t i = total_fields; i > 0; --i) {
                size_t field_idx = i - 1;
                const auto& fld = struct_def->fields[field_idx];

                // Check if field type needs drop
                if (!env_.type_needs_drop(fld.type)) {
                    continue;
                }

                // Get the type name for the field
                std::string field_type_name = get_type_name_from_semantic(fld.type);

                // Convert semantic TypePtr to MirTypePtr for the field
                MirTypePtr field_mir_type;
                if (!field_type_name.empty()) {
                    field_mir_type = make_struct_type(field_type_name);
                } else {
                    // Fallback: use a generic pointer type
                    field_mir_type = make_ptr_type();
                }

                // Extract field value using ExtractValueInst
                ExtractValueInst extract;
                extract.aggregate = value;
                extract.indices = {static_cast<uint32_t>(field_idx)};
                extract.aggregate_type = type;
                extract.result_type = field_mir_type;

                Value field_value = emit(std::move(extract), field_mir_type);

                // Recursively drop the field
                emit_drop_for_value(field_value, field_mir_type, field_type_name);
            }
        }
    }

    // Step 3: Handle arrays - iterate elements and drop each (in reverse order)
    if (type && std::holds_alternative<MirArrayType>(type->kind)) {
        const auto& arr_type = std::get<MirArrayType>(type->kind);
        std::string elem_type_name = get_type_name(arr_type.element);

        if (!elem_type_name.empty() && env_.type_needs_drop(elem_type_name)) {
            // Drop each element in reverse order
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

    // Note: For enums, the proper approach would be to generate a switch over the
    // discriminant to drop the correct variant's payload. For now, enums that need
    // drop should implement Drop explicitly to handle cleanup.
}

void MirBuilder::emit_scope_drops() {
    auto drops = ctx_.get_drops_for_current_scope();
    emit_drop_calls(drops);
    // Mark these drops as emitted to avoid duplicate drops on return/break paths
    ctx_.mark_scope_dropped();
}

void MirBuilder::emit_all_drops() {
    auto drops = ctx_.get_all_drops();
    emit_drop_calls(drops);
    // Mark all drops as emitted
    ctx_.mark_all_dropped();
}

} // namespace tml::mir
