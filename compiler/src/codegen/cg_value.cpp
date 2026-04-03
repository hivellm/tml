TML_MODULE("compiler")

//! CGValue — Typed Codegen Value Wrapper
//!
//! Implements the CGValue struct declared in codegen/cg_value.hpp.
//! All methods are small and focused; see the header for full documentation.

#include "codegen/cg_value.hpp"

#include "codegen/abi.hpp"

#include <string>
#include <utility>

namespace tml::codegen {

// ============================================================================
// Queries
// ============================================================================

bool CGValue::is_aggregate() const {
    return is_aggregate_llvm_type(llvm_type);
}

// ============================================================================
// Conversion free functions
// ============================================================================

CGSpillResult cg_to_address(const CGValue& value, int& counter) {
    // Already an address (or has no runtime representation) — return self.
    if (value.kind != CGValueKind::Immediate) {
        return CGSpillResult{value, {}, {}};
    }

    const int n = counter++;
    const std::string alloca_reg = "%spill." + std::to_string(n);

    const std::string alloca_ir = "  " + alloca_reg + " = alloca " + value.llvm_type;

    const std::string store_ir =
        "  store " + value.llvm_type + " " + value.reg + ", ptr " + alloca_reg;

    CGValue addr;
    addr.reg = alloca_reg;
    addr.llvm_type = value.llvm_type;
    addr.kind = CGValueKind::Address;
    addr.mir_type = value.mir_type;

    return CGSpillResult{std::move(addr), alloca_ir, store_ir};
}

CGLoadResult cg_to_immediate(const CGValue& value, int& counter) {
    // Already an immediate (or has no runtime representation) — return self.
    if (value.kind != CGValueKind::Address) {
        return CGLoadResult{value, {}};
    }

    const int n = counter++;
    const std::string load_reg = "%load." + std::to_string(n);

    const std::string load_ir =
        "  " + load_reg + " = load " + value.llvm_type + ", ptr " + value.reg;

    CGValue imm;
    imm.reg = load_reg;
    imm.llvm_type = value.llvm_type;
    imm.kind = CGValueKind::Immediate;
    imm.mir_type = value.mir_type;

    return CGLoadResult{std::move(imm), load_ir};
}

// ============================================================================
// Factory methods
// ============================================================================

CGValue CGValue::immediate(std::string reg, std::string llvm_type, mir::MirTypePtr mir_type) {
    CGValue v;
    v.reg = std::move(reg);
    v.llvm_type = std::move(llvm_type);
    v.kind = CGValueKind::Immediate;
    v.mir_type = std::move(mir_type);
    return v;
}

CGValue CGValue::address(std::string reg, std::string pointee_type, mir::MirTypePtr mir_type) {
    CGValue v;
    v.reg = std::move(reg);
    v.llvm_type = std::move(pointee_type);
    v.kind = CGValueKind::Address;
    v.mir_type = std::move(mir_type);
    return v;
}

CGValue CGValue::zero_sized() {
    CGValue v;
    v.kind = CGValueKind::ZeroSized;
    return v;
}

} // namespace tml::codegen
