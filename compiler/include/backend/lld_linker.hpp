//! # LLD Linker Interface
//!
//! This module provides a wrapper around LLD (LLVM's linker) for creating
//! executables and shared libraries without external tool dependencies.
//!
//! ## Supported Platforms
//!
//! | Platform | Linker      | Format |
//! |----------|-------------|--------|
//! | Windows  | lld-link    | COFF   |
//! | Linux    | ld.lld      | ELF    |
//! | macOS    | ld64.lld    | Mach-O |
//!
//! ## Usage
//!
//! ```cpp
//! LLDLinker linker;
//! if (!linker.initialize()) {
//!     // Handle error - LLD not found
//! }
//!
//! LLDLinkOptions opts;
//! opts.output_type = LLDOutputType::Executable;
//! auto result = linker.link({obj1, obj2}, "output.exe", opts);
//! ```

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::backend {

/// Output type for LLD linking.
enum class LLDOutputType {
    Executable, ///< Standalone executable (.exe on Windows)
    SharedLib,  ///< Shared library (.dll/.so/.dylib)
    StaticLib,  ///< Static library (.lib/.a)
};

/// Options for LLD linking.
struct LLDLinkOptions {
    /// Output type (executable, shared lib, static lib).
    LLDOutputType output_type = LLDOutputType::Executable;

    /// Additional library search paths.
    std::vector<fs::path> library_paths;

    /// Libraries to link against (without -l prefix or extension).
    std::vector<std::string> libraries;

    /// Additional linker flags.
    std::vector<std::string> extra_flags;

    /// Target triple (for cross-linking).
    std::string target_triple;

    /// Subsystem for Windows (console, windows, etc.).
    std::string subsystem = "console";

    /// Generate debug information.
    bool debug_info = false;

    /// Enable verbose output.
    bool verbose = false;

    /// Entry point symbol (default: main or _main).
    std::string entry_point;

    /// Export all symbols (for DLLs).
    bool export_all_symbols = false;

    /// Generate import library for DLLs (Windows).
    bool generate_import_lib = true;
};

/// Result of LLD linking.
struct LLDLinkResult {
    /// Whether linking succeeded.
    bool success = false;

    /// Path to the output file.
    fs::path output_file;

    /// Path to import library (if generated, Windows DLLs only).
    fs::path import_lib;

    /// Error message if linking failed.
    std::string error_message;

    /// Warning messages.
    std::vector<std::string> warnings;
};

/// LLD Linker wrapper.
///
/// Provides cross-platform linking using LLVM's LLD linker.
class LLDLinker {
public:
    LLDLinker();
    ~LLDLinker() = default;

    // Non-copyable
    LLDLinker(const LLDLinker&) = delete;
    LLDLinker& operator=(const LLDLinker&) = delete;

    /// Initialize the linker.
    ///
    /// Searches for LLD executables in common locations.
    /// @return true if LLD was found
    [[nodiscard]] auto initialize() -> bool;

    /// Check if the linker is initialized and ready.
    [[nodiscard]] auto is_initialized() const -> bool {
        return initialized_;
    }

    /// Link object files into an output.
    ///
    /// @param object_files List of object files to link
    /// @param output_path Path for the output file
    /// @param options Link options
    /// @return Link result
    [[nodiscard]] auto link(const std::vector<fs::path>& object_files, const fs::path& output_path,
                            const LLDLinkOptions& options) -> LLDLinkResult;

    /// Get the path to the LLD executable being used.
    [[nodiscard]] auto get_lld_path() const -> const fs::path& {
        return lld_path_;
    }

    /// Get the last error message.
    [[nodiscard]] auto get_last_error() const -> const std::string& {
        return last_error_;
    }

private:
    bool initialized_ = false;
    fs::path lld_path_;     ///< Path to lld-link (Windows) or ld.lld (Unix)
    fs::path llvm_ar_path_; ///< Path to llvm-ar for static libraries
    std::string last_error_;

    /// Find LLD executables.
    auto find_lld() -> bool;

    /// Build link command for Windows (COFF).
    auto build_windows_command(const std::vector<fs::path>& object_files,
                               const fs::path& output_path, const LLDLinkOptions& options)
        -> std::string;

    /// Build link command for Unix (ELF).
    auto build_unix_command(const std::vector<fs::path>& object_files, const fs::path& output_path,
                            const LLDLinkOptions& options) -> std::string;

    /// Build static library command using llvm-ar.
    auto build_static_lib_command(const std::vector<fs::path>& object_files,
                                  const fs::path& output_path) -> std::string;
};

/// Check if LLD is available on this system.
[[nodiscard]] auto is_lld_available() -> bool;

/// Get the LLD version string.
[[nodiscard]] auto get_lld_version() -> std::string;

} // namespace tml::backend
