TML_MODULE("codegen_x86")

//! # LLD Linker Implementation
//!
//! Wraps LLD for cross-platform linking. When TML_HAS_LLD_EMBEDDED is defined,
//! uses the in-process LLD library API (no subprocess). Otherwise falls back to
//! spawning lld-link.exe / ld.lld as a subprocess.

// Suppress MSVC warnings about getenv
#define _CRT_SECURE_NO_WARNINGS

#include "backend/lld_linker.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef TML_HAS_LLD_EMBEDDED
#include "lld/Common/Driver.h"

#include "llvm/Support/raw_ostream.h"

// Declare the LLD drivers we link against
LLD_HAS_DRIVER(coff)
LLD_HAS_DRIVER(elf)
LLD_HAS_DRIVER(mingw)
LLD_HAS_DRIVER(macho)
LLD_HAS_DRIVER(wasm)

#include <atomic>
#include <mutex>

// Global state: LLD uses global mutable state internally (CommonLinkerContext).
// If canRunAgain=false is ever returned, subsequent calls may crash.
// We also serialize calls because LLD is NOT re-entrant.
static std::atomic<bool> g_lld_poisoned{false};
static std::mutex g_lld_mutex;
#endif

#ifdef _WIN32
#include <windows.h>
#endif

namespace tml::backend {

// ============================================================================
// Helper Functions
// ============================================================================

/// Quote a path if it contains spaces (for subprocess command lines).
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
        TML_LOG_DEBUG("linker", cmd);
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

#ifdef TML_HAS_LLD_EMBEDDED
    // With embedded LLD, we don't strictly need the executable.
    // But we still search for llvm-ar (needed for static libraries).
    // Mark as initialized even if find_lld fails — in-process linking
    // doesn't need the executable path.
    find_lld();
    initialized_ = true;
    return true;
#else
    if (!find_lld()) {
        return false;
    }

    initialized_ = true;
    return true;
#endif
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

// ============================================================================
// Argument Builders (produce argv vectors)
// ============================================================================

auto LLDLinker::build_windows_args(const std::vector<fs::path>& object_files,
                                   const fs::path& output_path, const LLDLinkOptions& options)
    -> std::vector<std::string> {
    std::vector<std::string> args;

    // argv[0]: program name (LLD expects this even in-process)
    args.push_back("lld-link");

    // Output file
    args.push_back("/OUT:" + output_path.string());

    // Subsystem
    if (!options.subsystem.empty()) {
        args.push_back("/SUBSYSTEM:" + options.subsystem);
    }

    // Debug info
    if (options.debug_info) {
        args.push_back("/DEBUG");
    }

    // DLL-specific options
    if (options.output_type == LLDOutputType::SharedLib) {
        args.push_back("/DLL");

        if (options.generate_import_lib) {
            fs::path import_lib = output_path;
            import_lib.replace_extension(".lib");
            args.push_back("/IMPLIB:" + import_lib.string());
        }
    }

    // Entry point
    if (!options.entry_point.empty()) {
        args.push_back("/ENTRY:" + options.entry_point);
    } else if (options.output_type != LLDOutputType::SharedLib) {
        args.push_back("/ENTRY:mainCRTStartup");
    }

    // Library paths
    for (const auto& lib_path : options.library_paths) {
        args.push_back("/LIBPATH:" + lib_path.string());
    }

    // Default library paths for Windows CRT
    args.push_back("/DEFAULTLIB:libcmt");
    args.push_back("/DEFAULTLIB:oldnames");

    // Libraries
    for (const auto& lib : options.libraries) {
        if (lib.find('.') != std::string::npos) {
            args.push_back(lib);
        } else {
            args.push_back(lib + ".lib");
        }
    }

    // Object files and static libraries
    for (const auto& obj : object_files) {
        std::string ext = obj.extension().string();
        std::string path_str = obj.string();
        bool is_external_lib = (path_str.find("x64-windows") != std::string::npos ||
                                path_str.find("vcpkg") != std::string::npos ||
                                path_str.find("zstd.lib") != std::string::npos ||
                                path_str.find("brotli") != std::string::npos ||
                                path_str.find("zlib.lib") != std::string::npos);
        if (ext == ".lib" && !is_external_lib) {
            args.push_back("/WHOLEARCHIVE:" + obj.string());
        } else {
            args.push_back(obj.string());
        }
    }

    // Extra flags
    for (const auto& flag : options.extra_flags) {
        args.push_back(flag);
    }

    // Suppress logo
    args.push_back("/NOLOGO");

    return args;
}

auto LLDLinker::build_unix_args(const std::vector<fs::path>& object_files,
                                const fs::path& output_path, const LLDLinkOptions& options)
    -> std::vector<std::string> {
    std::vector<std::string> args;

    // argv[0]: program name
    args.push_back("ld.lld");

    // Output file
    args.push_back("-o");
    args.push_back(output_path.string());

    // Shared library
    if (options.output_type == LLDOutputType::SharedLib) {
        args.push_back("-shared");
        if (options.export_all_symbols) {
            args.push_back("--export-dynamic");
        }
    }

    // Entry point
    if (!options.entry_point.empty()) {
        args.push_back("-e");
        args.push_back(options.entry_point);
    }

    // Library paths
    for (const auto& lib_path : options.library_paths) {
        args.push_back("-L" + lib_path.string());
    }

    // Libraries
    for (const auto& lib : options.libraries) {
        args.push_back("-l" + lib);
    }

    // Object files
    for (const auto& obj : object_files) {
        args.push_back(obj.string());
    }

    // Extra flags
    for (const auto& flag : options.extra_flags) {
        args.push_back(flag);
    }

    // Link against C library
    args.push_back("-lc");

    return args;
}

auto LLDLinker::join_args(const std::vector<std::string>& args) -> std::string {
    std::ostringstream cmd;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            cmd << ' ';
        // Quote args with spaces for subprocess
        if (args[i].find(' ') != std::string::npos) {
            cmd << '"' << args[i] << '"';
        } else {
            cmd << args[i];
        }
    }
    return cmd.str();
}

// ============================================================================
// In-Process LLD Linking
// ============================================================================

#ifdef TML_HAS_LLD_EMBEDDED
auto LLDLinker::link_in_process(const std::vector<std::string>& args, const LLDLinkOptions& options)
    -> LLDLinkResult {
    LLDLinkResult result;
    result.success = false;

    // Check if a previous call poisoned LLD's global state
    if (g_lld_poisoned.load(std::memory_order_acquire)) {
        result.error_message = "LLD in-process unavailable (previous call corrupted global state)";
        return result;
    }

    // LLD is NOT re-entrant — serialize all calls
    std::lock_guard<std::mutex> lock(g_lld_mutex);

    // Double-check after acquiring the lock
    if (g_lld_poisoned.load(std::memory_order_acquire)) {
        result.error_message = "LLD in-process unavailable (previous call corrupted global state)";
        return result;
    }

    // Build const char* argv from string storage
    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
    }

    if (options.verbose) {
        TML_LOG_DEBUG("linker", "[in-process] " << join_args(args));
    }

    // Capture LLD stdout/stderr
    std::string stdout_str, stderr_str;
    llvm::raw_string_ostream stdout_os(stdout_str);
    llvm::raw_string_ostream stderr_os(stderr_str);

    // Select drivers based on platform
    std::vector<lld::DriverDef> drivers = {
#ifdef _WIN32
        {lld::WinLink, &lld::coff::link},
#else
        {lld::Gnu, &lld::elf::link},
        {lld::Darwin, &lld::macho::link},
#endif
    };

    lld::Result lld_result = lld::lldMain(argv, stdout_os, stderr_os, drivers);

    stdout_os.flush();
    stderr_os.flush();

    // Check canRunAgain — if false, LLD's global state is corrupted
    if (!lld_result.canRunAgain) {
        g_lld_poisoned.store(true, std::memory_order_release);
        TML_LOG_WARN("linker",
                     "LLD reported canRunAgain=false; switching to subprocess for future calls");
    }

    if (lld_result.retCode != 0) {
        result.error_message =
            "In-process LLD linking failed (exit code " + std::to_string(lld_result.retCode) + ")";
        if (!stderr_str.empty()) {
            result.error_message += ":\n" + stderr_str;
        }
        return result;
    }

    // Capture any warnings from stderr even on success
    if (!stderr_str.empty()) {
        // Split stderr into individual warnings
        std::istringstream iss(stderr_str);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                result.warnings.push_back(line);
            }
        }
    }

    result.success = true;
    return result;
}
#endif

// ============================================================================
// Main Link Method
// ============================================================================

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

    // Static libraries use llvm-ar (LLD doesn't do archiving)
    if (options.output_type == LLDOutputType::StaticLib) {
        std::string cmd = build_static_lib_command(object_files, output_path);
        if (options.verbose) {
            TML_LOG_DEBUG("linker", "Static lib command: " << cmd);
        }
        int ret = execute_command(cmd, options.verbose);
        if (ret != 0) {
            result.error_message =
                "Static library creation failed with exit code " + std::to_string(ret);
            return result;
        }
        if (!file_exists(output_path)) {
            result.error_message = "Output file was not created: " + output_path.string();
            return result;
        }
        result.success = true;
        result.output_file = output_path;
        return result;
    }

    // Build argument list
    std::vector<std::string> args;
#ifdef _WIN32
    args = build_windows_args(object_files, output_path, options);
#else
    args = build_unix_args(object_files, output_path, options);
#endif

#ifdef TML_HAS_LLD_EMBEDDED
    // Try in-process LLD linking first (fastest path, no subprocess)
    if (!g_lld_poisoned.load(std::memory_order_acquire)) {
        TML_LOG_DEBUG("linker", "[lld] Using in-process LLD");
        result = link_in_process(args, options);

        // If in-process succeeded, we're done
        if (result.success) {
            // Fall through to output verification below
        } else if (!g_lld_poisoned.load(std::memory_order_acquire)) {
            // In-process failed but state is still good — return the error
            return result;
        }
        // If poisoned, fall through to subprocess below
    }

    // Subprocess fallback (when in-process is poisoned or unavailable)
    if (!result.success && !lld_path_.empty() && file_exists(lld_path_)) {
        TML_LOG_DEBUG("linker", "[lld] Falling back to subprocess LLD");
        args[0] = lld_path_.string();
        std::string cmd = join_args(args);
        if (options.verbose) {
            TML_LOG_DEBUG("linker", "LLD command: " << cmd);
        }
        int ret = execute_command(cmd, options.verbose);
        if (ret != 0) {
            result.error_message = "Linking failed with exit code " + std::to_string(ret);
            return result;
        }
        result.success = true;
    } else if (!result.success) {
        // No subprocess available either
        return result;
    }
#else
    // Subprocess only (no embedded LLD)
    args[0] = lld_path_.string();
    std::string cmd = join_args(args);
    if (options.verbose) {
        TML_LOG_DEBUG("linker", "LLD command: " << cmd);
    }
    int ret = execute_command(cmd, options.verbose);
    if (ret != 0) {
        result.error_message = "Linking failed with exit code " + std::to_string(ret);
        return result;
    }
    result.success = true;
#endif

    if (result.success) {
        // Verify output was created
        if (!file_exists(output_path)) {
            result.error_message = "Output file was not created: " + output_path.string();
            result.success = false;
            return result;
        }
        result.output_file = output_path;

        // Check for import library (Windows DLLs)
        if (options.output_type == LLDOutputType::SharedLib && options.generate_import_lib) {
            fs::path import_lib = output_path;
            import_lib.replace_extension(".lib");
            if (file_exists(import_lib)) {
                result.import_lib = import_lib;
            }
        }
    }

    return result;
}

// ============================================================================
// Static Library Command (subprocess only — LLD doesn't do archiving)
// ============================================================================

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
#ifdef TML_HAS_LLD_EMBEDDED
    return "LLD (embedded, LLVM)";
#else
    LLDLinker linker;
    if (!linker.initialize()) {
        return "unknown";
    }
    return "LLD (compatible with LLVM)";
#endif
}

} // namespace tml::backend
