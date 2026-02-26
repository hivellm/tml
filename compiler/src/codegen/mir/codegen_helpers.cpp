TML_MODULE("codegen_x86")

//! MIR Codegen Helper Methods
//!
//! This file contains helper methods for the MIR-based code generator:
//! - get_value_reg: Maps MIR values to LLVM registers
//! - get_binop_name: Gets LLVM binary operation names
//! - get_cmp_predicate: Gets LLVM comparison predicates
//! - atomic_ordering_to_llvm: Converts atomic ordering
//! - atomic_rmw_op_to_llvm: Converts atomic RMW operations
//! - get_type_alignment: Gets type alignment in bytes

#include "codegen/mir_codegen.hpp"

namespace tml::codegen {

auto MirCodegen::get_value_reg(const mir::Value& val) -> std::string {
    if (!val.is_valid()) {
        return "<invalid>";
    }
    auto it = value_regs_.find(val.id);
    if (it != value_regs_.end()) {
        return it->second;
    }
    // If not found, create a new register name
    std::string reg = "%v" + std::to_string(val.id);
    value_regs_[val.id] = reg;
    return reg;
}

auto MirCodegen::atomic_ordering_to_llvm(mir::AtomicOrdering ordering) -> std::string {
    switch (ordering) {
    case mir::AtomicOrdering::Monotonic:
        return "monotonic";
    case mir::AtomicOrdering::Acquire:
        return "acquire";
    case mir::AtomicOrdering::Release:
        return "release";
    case mir::AtomicOrdering::AcqRel:
        return "acq_rel";
    case mir::AtomicOrdering::SeqCst:
        return "seq_cst";
    default:
        return "seq_cst";
    }
}

auto MirCodegen::atomic_rmw_op_to_llvm(mir::AtomicRMWOp op) -> std::string {
    switch (op) {
    case mir::AtomicRMWOp::Xchg:
        return "xchg";
    case mir::AtomicRMWOp::Add:
        return "add";
    case mir::AtomicRMWOp::Sub:
        return "sub";
    case mir::AtomicRMWOp::And:
        return "and";
    case mir::AtomicRMWOp::Nand:
        return "nand";
    case mir::AtomicRMWOp::Or:
        return "or";
    case mir::AtomicRMWOp::Xor:
        return "xor";
    case mir::AtomicRMWOp::Max:
        return "max";
    case mir::AtomicRMWOp::Min:
        return "min";
    case mir::AtomicRMWOp::UMax:
        return "umax";
    case mir::AtomicRMWOp::UMin:
        return "umin";
    default:
        return "xchg";
    }
}

auto MirCodegen::get_type_alignment(const mir::MirTypePtr& type) -> size_t {
    if (!type) {
        return 4;
    }

    if (auto* prim = std::get_if<mir::MirPrimitiveType>(&type->kind)) {
        switch (prim->kind) {
        case mir::PrimitiveType::Bool:
        case mir::PrimitiveType::I8:
        case mir::PrimitiveType::U8:
            return 1;
        case mir::PrimitiveType::I16:
        case mir::PrimitiveType::U16:
            return 2;
        case mir::PrimitiveType::I32:
        case mir::PrimitiveType::U32:
        case mir::PrimitiveType::F32:
            return 4;
        case mir::PrimitiveType::I64:
        case mir::PrimitiveType::U64:
        case mir::PrimitiveType::F64:
        case mir::PrimitiveType::Ptr:
        case mir::PrimitiveType::Str:
            return 8;
        case mir::PrimitiveType::I128:
        case mir::PrimitiveType::U128:
            return 16;
        default:
            return 4;
        }
    } else if (std::holds_alternative<mir::MirPointerType>(type->kind)) {
        return 8; // Pointer alignment
    }

    return 8; // Default alignment for complex types
}

auto MirCodegen::get_binop_name(mir::BinOp op, bool is_float, bool is_signed) -> std::string {
    if (is_float) {
        switch (op) {
        case mir::BinOp::Add:
            return "fadd";
        case mir::BinOp::Sub:
            return "fsub";
        case mir::BinOp::Mul:
            return "fmul";
        case mir::BinOp::Div:
            return "fdiv";
        case mir::BinOp::Mod:
            return "frem";
        default:
            return "fadd";
        }
    } else {
        switch (op) {
        case mir::BinOp::Add:
            return "add";
        case mir::BinOp::Sub:
            return "sub";
        case mir::BinOp::Mul:
            return "mul";
        case mir::BinOp::Div:
            return is_signed ? "sdiv" : "udiv";
        case mir::BinOp::Mod:
            return is_signed ? "srem" : "urem";
        case mir::BinOp::And:
            return "and";
        case mir::BinOp::Or:
            return "or";
        case mir::BinOp::BitAnd:
            return "and";
        case mir::BinOp::BitOr:
            return "or";
        case mir::BinOp::BitXor:
            return "xor";
        case mir::BinOp::Shl:
            return "shl";
        case mir::BinOp::Shr:
            return is_signed ? "ashr" : "lshr";
        default:
            return "add";
        }
    }
}

auto MirCodegen::get_cmp_predicate(mir::BinOp op, bool is_float, bool is_signed) -> std::string {
    if (is_float) {
        switch (op) {
        case mir::BinOp::Eq:
            return "oeq";
        case mir::BinOp::Ne:
            return "one";
        case mir::BinOp::Lt:
            return "olt";
        case mir::BinOp::Le:
            return "ole";
        case mir::BinOp::Gt:
            return "ogt";
        case mir::BinOp::Ge:
            return "oge";
        default:
            return "oeq";
        }
    } else {
        switch (op) {
        case mir::BinOp::Eq:
            return "eq";
        case mir::BinOp::Ne:
            return "ne";
        case mir::BinOp::Lt:
            return is_signed ? "slt" : "ult";
        case mir::BinOp::Le:
            return is_signed ? "sle" : "ule";
        case mir::BinOp::Gt:
            return is_signed ? "sgt" : "ugt";
        case mir::BinOp::Ge:
            return is_signed ? "sge" : "uge";
        default:
            return "eq";
        }
    }
}

} // namespace tml::codegen
