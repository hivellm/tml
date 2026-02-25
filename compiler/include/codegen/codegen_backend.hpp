//! # Codegen Backend Abstraction
//!
//! Abstract behavior for code generation backends. Provides a uniform
//! interface so the query pipeline and build orchestration can work with
//! any backend (LLVM, Cranelift, etc.) without hard-coding the codegen path.
//!
//! ## Architecture
//!
//! ```text
//!     CodegenBackend (abstract)
//!     ├── compile_mir()       → CodegenResult (MIR path)
//!     ├── compile_mir_cgu()   → CodegenResult (CGU path)
//!     ├── compile_ast()       → CodegenResult (AST path)
//!     └── generate_ir()       → std::string   (--emit-ir)
//!            │
//!   ┌────────┴────────┐
//!   │                 │
//! LLVMCodegenBackend  (future: CraneliftCodegenBackend)
//! ```

#pragma once

#include "mir/mir.hpp"
#include "parser/ast.hpp"
#include "types/checker.hpp"

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::codegen {

/// Describes what a backend supports.
struct BackendCapabilities {
    bool supports_mir = false;
    bool supports_ast = false;
    bool supports_generics = false;
    bool supports_debug_info = false;
    bool supports_coverage = false;
    bool supports_cgu = false;
    int max_optimization_level = 3;
};

/// Result of a codegen operation.
struct CodegenResult {
    bool success = false;
    std::string llvm_ir;
    fs::path object_file;
    std::set<std::string> link_libs;
    std::string error_message;
};

/// Options for codegen.
struct CodegenOptions {
    int optimization_level = 0;
    bool debug_info = false;
    bool coverage_enabled = false;
    bool emit_comments = true;
    bool dll_export = false;
    bool generate_exe_main = false; ///< Emit @main(argc,argv) C entry wrapper for executables.
    std::string target_triple;
};

/// Abstract behavior for code generation backends.
class CodegenBackend {
public:
    virtual ~CodegenBackend() = default;

    /// Backend name (e.g. "llvm", "cranelift").
    virtual auto name() const -> std::string_view = 0;

    /// What this backend supports.
    virtual auto capabilities() const -> BackendCapabilities = 0;

    /// Compile a MIR module to an object file (simple code path).
    virtual auto compile_mir(const mir::Module& module, const CodegenOptions& opts)
        -> CodegenResult = 0;

    /// Compile a subset of MIR functions (CGU partitioned path).
    virtual auto compile_mir_cgu(const mir::Module& module, const std::vector<size_t>& func_indices,
                                 const CodegenOptions& opts) -> CodegenResult = 0;

    /// Compile from AST (full features: generics, imports, closures).
    virtual auto compile_ast(const parser::Module& module, const types::TypeEnv& env,
                             const CodegenOptions& opts) -> CodegenResult = 0;

    /// Generate IR text only (for --emit-ir, no object compilation).
    virtual auto generate_ir(const mir::Module& module, const CodegenOptions& opts)
        -> std::string = 0;
};

/// Available backend types.
enum class BackendType { LLVM, Cranelift };

/// Create a backend instance by type.
auto create_backend(BackendType type) -> std::unique_ptr<CodegenBackend>;

/// The default backend type for this platform.
auto default_backend_type() -> BackendType;

} // namespace tml::codegen
