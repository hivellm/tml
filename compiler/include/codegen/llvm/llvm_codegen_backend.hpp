//! # LLVM Codegen Backend
//!
//! Concrete implementation of `CodegenBackend` that wraps the existing
//! `MirCodegen`, `LLVMIRGen`, and `LLVMBackend` classes. No reimplementation
//! — just delegation.

#pragma once

#include "codegen/codegen_backend.hpp"

namespace tml::codegen {

/// LLVM-based codegen backend.
///
/// Wraps existing `MirCodegen` (MIR path), `LLVMIRGen` (AST path), and
/// `LLVMBackend` (IR → object compilation).
class LLVMCodegenBackend : public CodegenBackend {
public:
    auto name() const -> std::string_view override {
        return "llvm";
    }

    auto capabilities() const -> BackendCapabilities override;

    auto compile_mir(const mir::Module& module, const CodegenOptions& opts)
        -> CodegenResult override;

    auto compile_mir_cgu(const mir::Module& module, const std::vector<size_t>& func_indices,
                         const CodegenOptions& opts) -> CodegenResult override;

    auto compile_ast(const parser::Module& module, const types::TypeEnv& env,
                     const CodegenOptions& opts) -> CodegenResult override;

    auto generate_ir(const mir::Module& module, const CodegenOptions& opts) -> std::string override;
};

} // namespace tml::codegen
