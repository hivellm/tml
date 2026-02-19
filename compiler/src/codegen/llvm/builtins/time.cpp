//! # LLVM IR Generator - Time Builtins (REMOVED)
//!
//! Phase 25: All time builtins migrated to @extern("c") FFI in std::time.
//! time_ns, sleep_ms now declared as @extern in lib/std/src/time.tml.
//! This file is kept as a stub for API compatibility.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_time(const std::string& /*fn_name*/,
                                     const parser::CallExpr& /*call*/)
    -> std::optional<std::string> {
    return std::nullopt;
}

} // namespace tml::codegen
