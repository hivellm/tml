//! # LLD Linker Implementation
//!
//! Wraps LLD executables for cross-platform linking.

// Suppress MSVC warnings about getenv
#define _CRT_SECURE_NO_WARNINGS

#include "backend/lld_linker.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tml::backend {

// ============================================================================
// Helper Functions
// ============================================================================

/// Quote a path if it contains spaces.
static std::string quote_path(const fs::path& path) {
    std::string str = path.string();
    if (str.find(' ') != std::string::npos) {
        return "\"" + str + "\"";
    }
    return str;
}

/// Execute a command and return the exit code.
static int execute_command(const std::string& cmd, bool verbose) {
    if (verbose) {
        std::cout << "[lld_linker] " << cmd << "\n";
    }
    return std::system(cmd.c_str());
}

/// Check if a file exists.
static bool file_exists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

// ============================================================================
// LLDLinker Implementation
// ============================================================================

LLDLinker::LLDLinker() = default;

auto LLDLinker::initialize() -> bool {
    if (initialized_) {
        return true;
    }

    if (!find_lld()) {
        return false;
    }

    initialized_ = true;
    return true;
}

auto LLDLinker::find_lld() -> bool {
    // Common LLVM installation paths
    // Priority: local build output > local install > system install
    std::vector<fs::path> search_paths = {
        // Local LLVM build output (raw build artifacts)
        // Note: current_path() is the project root when running from there
        fs::current_path() / "build" / "llvm" / "Release" / "bin",
        // Local LLVM install (from scripts/build_llvm.bat)
        fs::current_path() / "src" / "llvm-install" / "bin",
        // System installations
        "F:/LLVM/bin",
        "C:/Program Files/LLVM/bin",
        "C:/LLVM/bin",
        "/usr/bin",
        "/usr/local/bin",
        "/usr/lib/llvm-18/bin",
        "/usr/lib/llvm-17/bin",
        "/usr/local/opt/llvm/bin",
    };

    // Also check PATH environment variable
    if (const char* path_env = std::getenv("PATH")) {
        std::string path_str(path_env);
#ifdef _WIN32
        char delimiter = ';';
#else
        char delimiter = ':';
#endif
        std::istringstream iss(path_str);
        std::string path;
        while (std::getline(iss, path, delimiter)) {
            search_paths.push_back(path);
        }
    }

    // Check LLVM_DIR environment variable
    if (const char* llvm_dir = std::getenv("LLVM_DIR")) {
        search_paths.insert(search_paths.begin(), fs::path(llvm_dir) / "bin");
    }

#ifdef _WIN32
    const std::string lld_name = "lld-link.exe";
    const std::string ar_name = "llvm-ar.exe";
#else
    const std::string lld_name = "ld.lld";
    const std::string ar_name = "llvm-ar";
#endif

    for (const auto& dir : search_paths) {
        fs::path lld_candidate = dir / lld_name;
        if (file_exists(lld_candidate)) {
            lld_path_ = lld_candidate;

            // Also look for llvm-ar in the same directory
            fs::path ar_candidate = dir / ar_name;
            if (file_exists(ar_candidate)) {
                llvm_ar_path_ = ar_candidate;
            }

            return true;
        }
    }

    last_error_ =
        "LLD linker not found. The TML compiler requires LLD for self-contained linking.\n"
        "  Solutions:\n"
        "  1. Ensure tml_runtime.lib is in the same directory as tml.exe\n"
        "  2. Set LLVM_DIR environment variable to your LLVM installation\n"
        "  3. Use --use-external-tools flag to fall back to system clang";
    return false;
}

auto LLDLinker::link(const std::vector<fs::path>& object_files, const fs::path& output_path,
                     const LLDLinkOptions& options) -> LLDLinkResult {
    LLDLinkResult result;
    result.success = false;

    if (!initialized_) {
        result.error_message = "LLD linker not initialized";
        return result;
    }

    // Verify all object files exist
    for (const auto& obj : object_files) {
        if (!file_exists(obj)) {
            result.error_message = "Object file not found: " + obj.string();
            return result;
        }
    }

    if (object_files.empty()) {
        result.error_message = "No object files provided for linking";
        return result;
    }

    // Build command based on output type
    std::string cmd;
    if (options.output_type == LLDOutputType::StaticLib) {
        cmd = build_static_lib_command(object_files, output_path);
    } else {
#ifdef _WIN32
        cmd = build_windows_command(object_files, output_path, options);
#else
        cmd = build_unix_command(object_files, output_path, options);
#endif
    }

    // Always print command for debugging
    std::cerr << "[DEBUG LLD] Command: " << cmd << "\n";

    // Execute the command
    int ret = execute_command(cmd, options.verbose);

    if (ret != 0) {
        result.error_message = "Linking failed with exit code " + std::to_string(ret);
        return result;
    }

    // Verify output was created
    if (!file_exists(output_path)) {
        result.error_message = "Output file was not created: " + output_path.string();
        return result;
    }

    result.success = true;
    result.output_file = output_path;

    // Check for import library (Windows DLLs)
    if (options.output_type == LLDOutputType::SharedLib && options.generate_import_lib) {
        fs::path import_lib = output_path;
        import_lib.replace_extension(".lib");
        if (file_exists(import_lib)) {
            result.import_lib = import_lib;
        }
    }

    return result;
}

auto LLDLinker::build_windows_command(const std::vector<fs::path>& object_files,
                                      const fs::path& output_path, const LLDLinkOptions& options)
    -> std::string {
    std::ostringstream cmd;

    cmd << quote_path(lld_path_);

    // Output file
    cmd << " /OUT:" << quote_path(output_path);

    // Subsystem
    if (!options.subsystem.empty()) {
        cmd << " /SUBSYSTEM:" << options.subsystem;
    }

    // Debug info
    if (options.debug_info) {
        cmd << " /DEBUG";
    }

    // DLL-specific options
    if (options.output_type == LLDOutputType::SharedLib) {
        cmd << " /DLL";

        if (options.export_all_symbols) {
            // lld-link doesn't have export-all-symbols directly
            // We'd need to use a .def file or /EXPORT directives
            // For now, export common C runtime symbols
        }

        if (options.generate_import_lib) {
            fs::path import_lib = output_path;
            import_lib.replace_extension(".lib");
            cmd << " /IMPLIB:" << quote_path(import_lib);
        }
    }

    // Entry point
    if (!options.entry_point.empty()) {
        cmd << " /ENTRY:" << options.entry_point;
    } else if (options.output_type == LLDOutputType::SharedLib) {
        // DLLs don't need entry point
    } else {
        // Default entry point for executables
        cmd << " /ENTRY:mainCRTStartup";
    }

    // Library paths
    for (const auto& lib_path : options.library_paths) {
        cmd << " /LIBPATH:" << quote_path(lib_path);
    }

    // Default library paths for Windows CRT
    // These are needed for linking standard C library functions
    cmd << " /DEFAULTLIB:libcmt";
    cmd << " /DEFAULTLIB:oldnames";

    // Libraries
    for (const auto& lib : options.libraries) {
        if (lib.find('.') != std::string::npos) {
            // Full filename provided
            cmd << " " << quote_path(fs::path(lib));
        } else {
            // Just library name
            cmd << " " << lib << ".lib";
        }
    }

    // Object files and static libraries
    for (const auto& obj : object_files) {
        // For static libraries (.lib), use /WHOLEARCHIVE to include all objects
        // This is needed for FFI functions that may not be referenced until runtime
        std::string ext = obj.extension().string();
        if (ext == ".lib") {
            cmd << " /WHOLEARCHIVE:" << quote_path(obj);
        } else {
            cmd << " " << quote_path(obj);
        }
    }

    // Extra flags
    for (const auto& flag : options.extra_flags) {
        cmd << " " << flag;
    }

    // Suppress logo
    cmd << " /NOLOGO";

    return cmd.str();
}

auto LLDLinker::build_unix_command(const std::vector<fs::path>& object_files,
                                   const fs::path& output_path, const LLDLinkOptions& options)
    -> std::string {
    std::ostringstream cmd;

    cmd << quote_path(lld_path_);

    // Output file
    cmd << " -o " << quote_path(output_path);

    // Shared library
    if (options.output_type == LLDOutputType::SharedLib) {
        cmd << " -shared";
        if (options.export_all_symbols) {
            cmd << " --export-dynamic";
        }
    }

    // Debug info (for separate debug file)
    // Note: debug info is typically in the object files already

    // Entry point
    if (!options.entry_point.empty()) {
        cmd << " -e " << options.entry_point;
    }

    // Library paths
    for (const auto& lib_path : options.library_paths) {
        cmd << " -L" << quote_path(lib_path);
    }

    // Libraries
    for (const auto& lib : options.libraries) {
        cmd << " -l" << lib;
    }

    // Object files
    for (const auto& obj : object_files) {
        cmd << " " << quote_path(obj);
    }

    // Extra flags
    for (const auto& flag : options.extra_flags) {
        cmd << " " << flag;
    }

    // Link against C library
    cmd << " -lc";

    return cmd.str();
}

auto LLDLinker::build_static_lib_command(const std::vector<fs::path>& object_files,
                                         const fs::path& output_path) -> std::string {
    std::ostringstream cmd;

    if (!llvm_ar_path_.empty() && file_exists(llvm_ar_path_)) {
        cmd << quote_path(llvm_ar_path_);
    } else {
        // Fall back to system ar
#ifdef _WIN32
        cmd << "lib.exe";
#else
        cmd << "ar";
#endif
    }

#ifdef _WIN32
    // MSVC-style lib.exe / llvm-ar as lib
    cmd << " /OUT:" << quote_path(output_path);
    for (const auto& obj : object_files) {
        cmd << " " << quote_path(obj);
    }
#else
    // Unix ar style
    cmd << " rcs " << quote_path(output_path);
    for (const auto& obj : object_files) {
        cmd << " " << quote_path(obj);
    }
#endif

    return cmd.str();
}

// ============================================================================
// Module-level Functions
// ============================================================================

auto is_lld_available() -> bool {
    LLDLinker linker;
    return linker.initialize();
}

auto get_lld_version() -> std::string {
    LLDLinker linker;
    if (!linker.initialize()) {
        return "unknown";
    }

    // Run lld --version
    std::string cmd = linker.get_lld_path().string() + " --version";
    // This would require capturing output, for now return placeholder
    return "LLD (compatible with LLVM)";
}

} // namespace tml::backend
