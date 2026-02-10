//! # Cranelift Codegen Backend
//!
//! Concrete implementation of `CodegenBackend` that delegates to the Rust-based
//! Cranelift bridge via FFI. MIR modules are serialized to binary, passed across
//! the FFI boundary, compiled by Cranelift, and the resulting object bytes are
//! written to a temp file.

#pragma once

#include "backend/cranelift_bridge.h"
#include "codegen/codegen_backend.hpp"

namespace tml::codegen {

/// Cranelift-based codegen backend.
///
/// Delegates all compilation to the Rust `tml_cranelift_bridge` static library
/// via the C API declared in `cranelift_bridge.h`.
class CraneliftCodegenBackend : public CodegenBackend {
public:
    auto name() const -> std::string_view override {
        return "cranelift";
    }

    auto capabilities() const -> BackendCapabilities override;

    auto compile_mir(const mir::Module& module, const CodegenOptions& opts)
        -> CodegenResult override;

    auto compile_mir_cgu(const mir::Module& module, const std::vector<size_t>& func_indices,
                         const CodegenOptions& opts) -> CodegenResult override;

    auto compile_ast(const parser::Module& module, const types::TypeEnv& env,
                     const CodegenOptions& opts) -> CodegenResult override;

    auto generate_ir(const mir::Module& module, const CodegenOptions& opts) -> std::string override;

private:
    /// Convert CodegenOptions to CraneliftOptions FFI struct.
    static auto to_cranelift_opts(const CodegenOptions& opts) -> ::CraneliftOptions;

    /// Write object bytes to a temp file and return the path.
    static auto write_object_file(const uint8_t* data, size_t len) -> fs::path;
};

} // namespace tml::codegen
