//! # LLVM Backend
//!
//! This module provides direct integration with LLVM for compiling LLVM IR
//! to native object files without requiring external tools like clang.
//!
//! ## Usage
//!
//! ```cpp
//! LLVMBackend backend;
//! if (!backend.initialize()) {
//!     // Handle initialization error
//! }
//!
//! CompileOptions opts;
//! opts.optimization_level = 3;
//! auto result = backend.compile_ir_to_object(ir_string, output_path, opts);
//! ```
//!
//! ## Features
//!
//! - Direct LLVM IR parsing and compilation
//! - Optimization levels O0-O3
//! - Debug info emission
//! - Target-specific code generation
//! - No external tool dependencies

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::backend {

/// Options for LLVM IR compilation.
struct LLVMCompileOptions {
    /// Optimization level (0-3).
    int optimization_level = 0;

    /// Enable debug information.
    bool debug_info = false;

    /// Target triple (e.g., "x86_64-pc-windows-msvc").
    /// Empty means use native target.
    std::string target_triple;

    /// CPU name for target-specific optimizations (e.g., "native", "skylake").
    std::string cpu = "native";

    /// CPU features (e.g., "+avx2,+fma").
    std::string features;

    /// Generate position-independent code (for shared libraries).
    bool position_independent = false;

    /// Enable verbose output.
    bool verbose = false;
};

/// Result of LLVM IR compilation.
struct LLVMCompileResult {
    /// Whether compilation succeeded.
    bool success = false;

    /// Path to the generated object file.
    fs::path object_file;

    /// In-memory object data (populated by compile_ir_to_buffer).
    std::vector<uint8_t> object_data;

    /// Error message if compilation failed.
    std::string error_message;

    /// Warning messages from compilation.
    std::vector<std::string> warnings;
};

/// LLVM Backend for direct IR compilation.
///
/// This class wraps the LLVM C API to provide object file generation
/// from LLVM IR text without spawning external processes.
class LLVMBackend {
public:
    LLVMBackend();
    ~LLVMBackend();

    // Non-copyable
    LLVMBackend(const LLVMBackend&) = delete;
    LLVMBackend& operator=(const LLVMBackend&) = delete;

    /// Initialize the LLVM backend.
    ///
    /// Must be called before any compilation. Initializes LLVM targets
    /// and creates necessary contexts.
    ///
    /// @return true if initialization succeeded
    [[nodiscard]] auto initialize() -> bool;

    /// Check if the backend is initialized.
    [[nodiscard]] auto is_initialized() const -> bool {
        return initialized_;
    }

    /// Compile LLVM IR text to an object file.
    ///
    /// @param ir_content The LLVM IR text content
    /// @param output_path Path for the output object file
    /// @param options Compilation options
    /// @return Compilation result
    [[nodiscard]] auto compile_ir_to_object(const std::string& ir_content,
                                            const fs::path& output_path,
                                            const LLVMCompileOptions& options) -> LLVMCompileResult;

    /// Compile LLVM IR text to an in-memory object buffer.
    ///
    /// Skips disk I/O for the object file. The result's object_data
    /// field contains the raw object bytes. Use this when the object
    /// doesn't need to be cached (e.g., --no-cache builds).
    ///
    /// @param ir_content The LLVM IR text content
    /// @param options Compilation options
    /// @return Compilation result with object_data populated
    [[nodiscard]] auto compile_ir_to_buffer(const std::string& ir_content,
                                            const LLVMCompileOptions& options) -> LLVMCompileResult;

    /// Compile LLVM IR file to an object file.
    ///
    /// @param ir_file Path to the LLVM IR file (.ll)
    /// @param output_path Path for the output object file (optional, auto-generated if empty)
    /// @param options Compilation options
    /// @return Compilation result
    [[nodiscard]] auto compile_ir_file_to_object(const fs::path& ir_file,
                                                 const std::optional<fs::path>& output_path,
                                                 const LLVMCompileOptions& options)
        -> LLVMCompileResult;

    /// Get the default target triple for the host.
    [[nodiscard]] auto get_default_target_triple() const -> std::string;

    /// Get the last error message.
    [[nodiscard]] auto get_last_error() const -> const std::string& {
        return last_error_;
    }

private:
    bool initialized_ = false;
    std::string last_error_;

    // LLVM context handle (opaque pointer to LLVMContextRef)
    void* context_ = nullptr;
};

/// Check if LLVM backend is available on this system.
///
/// Returns true if LLVM libraries are properly linked.
[[nodiscard]] auto is_llvm_backend_available() -> bool;

/// Get the LLVM version string.
[[nodiscard]] auto get_llvm_version() -> std::string;

} // namespace tml::backend
