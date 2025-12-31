// MIR Builder - Helper Methods Implementation
//
// This file contains helper functions for block management, emitting
// instructions, creating constants, and operator conversion.

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

} // namespace tml::mir
