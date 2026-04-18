TML_MODULE("codegen_x86")

//! # Linker Implementation (subprocess-only)
//!
//! Links object files using the native OS linker as a subprocess.
//! On Windows: link.exe (MSVC) or lld-link.exe as fallback.
//! On Linux/macOS: ld or ld.lld as fallback.
//!
//! LLD is no longer embedded — this eliminates ~30-40 MB from the codegen DLL.

// Suppress MSVC warnings about getenv
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "backend/lld_linker.hpp"

#include "log/log.hpp"
#include "profiler.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>

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

    if (!find_lld()) {
        return false;
    }

    initialized_ = true;
    return true;
}

auto LLDLinker::find_lld() -> bool {
    // Build search paths from environment only — no hardcoded absolute paths.
    std::vector<fs::path> search_paths;

    // LLVM_DIR env var (highest priority)
    if (const char* llvm_dir = std::getenv("LLVM_DIR")) {
        search_paths.push_back(fs::path(llvm_dir) / "bin");
    }

    // Project-relative LLVM install (for dev builds)
    search_paths.push_back(fs::current_path() / "src" / "llvm-install" / "bin");

    // Project-relative LLVM build output (produced by scripts/build-llvm.bat)
    // — lld-link.exe lives under build/llvm/bin/ for the locally-vendored
    // LLVM build. No LLVM_DIR env var needed.
    search_paths.push_back(fs::current_path() / "build" / "llvm" / "bin");

    // PATH entries
    if (const char* path_env = std::getenv("PATH")) {
        std::string path_str(path_env);
#ifdef _WIN32
        char delimiter = ';';
#else
        char delimiter = ':';
#endif
        std::istringstream iss(path_str);
        std::string dir;
        while (std::getline(iss, dir, delimiter)) {
            if (!dir.empty()) {
                search_paths.push_back(dir);
            }
        }
    }

#ifdef _WIN32
    // Windows: prefer link.exe (MSVC native), fall back to lld-link.exe
    for (const auto& dir : search_paths) {
        fs::path link_candidate = dir / "link.exe";
        if (file_exists(link_candidate)) {
            // Verify it's MSVC's link.exe (not some other link.exe)
            // by checking if cl.exe or lib.exe is in the same directory
            fs::path cl_candidate = dir / "cl.exe";
            fs::path lib_candidate = dir / "lib.exe";
            if (file_exists(cl_candidate) || file_exists(lib_candidate)) {
                lld_path_ = link_candidate;
                is_msvc_linker_ = true;
                if (file_exists(lib_candidate)) {
                    llvm_ar_path_ = lib_candidate;
                }
                TML_LOG_DEBUG("linker", "Found MSVC link.exe: " << lld_path_);
                return true;
            }
        }
    }

    // Fall back to lld-link.exe
    for (const auto& dir : search_paths) {
        fs::path lld_candidate = dir / "lld-link.exe";
        if (file_exists(lld_candidate)) {
            lld_path_ = lld_candidate;
            is_msvc_linker_ = false;
            fs::path ar_candidate = dir / "llvm-ar.exe";
            if (file_exists(ar_candidate)) {
                llvm_ar_path_ = ar_candidate;
            }
            return true;
        }
    }
#else
    // Unix: prefer system ld, fall back to ld.lld
    for (const auto& dir : search_paths) {
        fs::path ld_candidate = dir / "ld";
        if (file_exists(ld_candidate)) {
            lld_path_ = ld_candidate;
            fs::path ar_candidate = dir / "ar";
            if (file_exists(ar_candidate)) {
                llvm_ar_path_ = ar_candidate;
            }
            return true;
        }
    }

    for (const auto& dir : search_paths) {
        fs::path lld_candidate = dir / "ld.lld";
        if (file_exists(lld_candidate)) {
            lld_path_ = lld_candidate;
            fs::path ar_candidate = dir / "llvm-ar";
            if (file_exists(ar_candidate)) {
                llvm_ar_path_ = ar_candidate;
            }
            return true;
        }
    }
#endif

    last_error_ = "No linker found. The TML compiler requires a system linker.\n"
                  "  Solutions:\n"
#ifdef _WIN32
                  "  1. Run from a Visual Studio Developer Command Prompt (provides link.exe)\n"
                  "  2. Install LLVM and ensure lld-link.exe is in PATH\n"
                  "  3. Set LLVM_DIR environment variable to your LLVM installation";
#else
                  "  1. Install build-essential (provides ld)\n"
                  "  2. Install LLVM and ensure ld.lld is in PATH\n"
                  "  3. Set LLVM_DIR environment variable to your LLVM installation";
#endif
    return false;
}

// ============================================================================
// Argument Builders (produce argv vectors)
// ============================================================================

auto LLDLinker::build_windows_args(const std::vector<fs::path>& object_files,
                                   const fs::path& output_path, const LLDLinkOptions& options)
    -> std::vector<std::string> {
    std::vector<std::string> args;

    // argv[0]: linker executable
    args.push_back(lld_path_.string());

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

    // Extra flags — convert Unix-style -l flags to /DEFAULTLIB: for COFF linkers
    for (const auto& flag : options.extra_flags) {
        if (flag.size() > 2 && flag[0] == '-' && flag[1] == 'l') {
            args.push_back("/DEFAULTLIB:" + flag.substr(2));
        } else {
            args.push_back(flag);
        }
    }

    // Suppress logo
    args.push_back("/NOLOGO");

    return args;
}

auto LLDLinker::build_unix_args(const std::vector<fs::path>& object_files,
                                const fs::path& output_path, const LLDLinkOptions& options)
    -> std::vector<std::string> {
    std::vector<std::string> args;

    // argv[0]: linker executable
    args.push_back(lld_path_.string());

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
// Main Link Method
// ============================================================================

auto LLDLinker::link(const std::vector<fs::path>& object_files, const fs::path& output_path,
                     const LLDLinkOptions& options) -> LLDLinkResult {
    TML_ZONE("lld::link");
    LLDLinkResult result;
    result.success = false;

    if (!initialized_) {
        result.error_message = "[N004] Linker not initialized";
        return result;
    }

    // Verify all object files exist
    for (const auto& obj : object_files) {
        if (!file_exists(obj)) {
            result.error_message = "[N006] Object file not found: " + obj.string();
            return result;
        }
    }

    if (object_files.empty()) {
        result.error_message = "[N005] No object files provided for linking";
        return result;
    }

    // Static libraries use llvm-ar or lib.exe (linkers don't do archiving)
    if (options.output_type == LLDOutputType::StaticLib) {
        std::string cmd = build_static_lib_command(object_files, output_path);
        if (options.verbose) {
            TML_LOG_DEBUG("linker", "Static lib command: " << cmd);
        }
        int ret = execute_command(cmd, options.verbose);
        if (ret != 0) {
            result.error_message =
                "[N008] Static library creation failed with exit code " + std::to_string(ret);
            return result;
        }
        if (!file_exists(output_path)) {
            result.error_message = "[N007] Output file was not created: " + output_path.string();
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

    // Always use subprocess
    std::string cmd = join_args(args);
    if (options.verbose) {
        TML_LOG_DEBUG("linker", "Link command: " << cmd);
    }
    int ret = execute_command(cmd, options.verbose);
    if (ret != 0) {
        result.error_message = "[N001] Linking failed with exit code " + std::to_string(ret);
        return result;
    }
    result.success = true;

    if (result.success) {
        // Verify output was created
        if (!file_exists(output_path)) {
            result.error_message = "[N007] Output file was not created: " + output_path.string();
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
// Static Library Command (subprocess only — linkers don't do archiving)
// ============================================================================

auto LLDLinker::build_static_lib_command(const std::vector<fs::path>& object_files,
                                         const fs::path& output_path) -> std::string {
    std::ostringstream cmd;

    // Find archiver: use cached path, or search PATH/project-relative locations
    fs::path ar = llvm_ar_path_;
    bool is_msvc_lib = false;

    if (ar.empty() || !file_exists(ar)) {
        // Search project-relative llvm-install
        fs::path project_ar = fs::current_path() / "src" / "llvm-install" / "bin" /
#ifdef _WIN32
                              "llvm-ar.exe";
#else
                              "llvm-ar";
#endif
        if (file_exists(project_ar)) {
            ar = project_ar;
        }
    }

#ifdef _WIN32
    // If still not found, search PATH for llvm-ar.exe then lib.exe
    if (ar.empty() || !file_exists(ar)) {
        if (const char* path_env = std::getenv("PATH")) {
            std::string path_s(path_env);
            std::istringstream iss(path_s);
            std::string dir;
            while (std::getline(iss, dir, ';')) {
                if (dir.empty())
                    continue;
                fs::path candidate = fs::path(dir) / "llvm-ar.exe";
                if (file_exists(candidate)) {
                    ar = candidate;
                    break;
                }
            }
        }
    }
    if (ar.empty() || !file_exists(ar)) {
        if (const char* path_env = std::getenv("PATH")) {
            std::string path_s(path_env);
            std::istringstream iss(path_s);
            std::string dir;
            while (std::getline(iss, dir, ';')) {
                if (dir.empty())
                    continue;
                fs::path candidate = fs::path(dir) / "lib.exe";
                // Verify it's MSVC lib.exe (same dir has cl.exe or link.exe)
                if (file_exists(candidate)) {
                    fs::path cl = fs::path(dir) / "cl.exe";
                    fs::path link = fs::path(dir) / "link.exe";
                    if (file_exists(cl) || file_exists(link)) {
                        ar = candidate;
                        is_msvc_lib = true;
                        break;
                    }
                }
            }
        }
    }
#else
    if (ar.empty() || !file_exists(ar)) {
        if (const char* path_env = std::getenv("PATH")) {
            std::string path_s(path_env);
            std::istringstream iss(path_s);
            std::string dir;
            while (std::getline(iss, dir, ':')) {
                if (dir.empty())
                    continue;
                for (const char* name : {"llvm-ar", "ar"}) {
                    fs::path candidate = fs::path(dir) / name;
                    if (file_exists(candidate)) {
                        ar = candidate;
                        break;
                    }
                }
                if (!ar.empty())
                    break;
            }
        }
    }
#endif

    // Fallback: `zig ar` ships with the Zig toolchain, which is TML's
    // preferred C/C++ compiler per ADR-007. It is LLVM-ar compatible and
    // available on developer machines that do not have llvm-ar / lib.exe
    // on PATH. Search for `zig` as a last resort before giving up.
    bool is_zig_ar = false;
    if (ar.empty() || !file_exists(ar)) {
        if (const char* path_env = std::getenv("PATH")) {
            std::string path_s(path_env);
            std::istringstream iss(path_s);
            std::string dir;
#ifdef _WIN32
            const char sep = ';';
            const char* zig_name = "zig.exe";
#else
            const char sep = ':';
            const char* zig_name = "zig";
#endif
            while (std::getline(iss, dir, sep)) {
                if (dir.empty())
                    continue;
                fs::path candidate = fs::path(dir) / zig_name;
                if (file_exists(candidate)) {
                    ar = candidate;
                    is_zig_ar = true;
                    break;
                }
            }
        }
    }

    if (ar.empty() || !file_exists(ar)) {
        // Last resort: bare command names, hope they're in PATH
#ifdef _WIN32
        cmd << "llvm-ar.exe";
#else
        cmd << "ar";
#endif
    } else {
        cmd << quote_path(ar);
    }

    // Determine if this is MSVC lib.exe by filename
    if (!is_msvc_lib && !ar.empty()) {
        std::string filename = ar.filename().string();
        if (filename == "lib.exe" || filename == "LIB.EXE") {
            is_msvc_lib = true;
        }
    }

    if (is_msvc_lib) {
        // MSVC lib.exe: /NOLOGO /OUT:<archive> <obj1> <obj2> ...
        cmd << " /NOLOGO /OUT:" << quote_path(output_path);
        for (const auto& obj : object_files) {
            cmd << " " << quote_path(obj);
        }
    } else if (is_zig_ar) {
        // `zig ar` takes the same arg shape as llvm-ar, prefixed by the
        // `ar` subcommand to select the archiver tool.
        cmd << " ar rcs " << quote_path(output_path);
        for (const auto& obj : object_files) {
            cmd << " " << quote_path(obj);
        }
    } else {
        // llvm-ar / ar: rcs <archive> <obj1> <obj2> ...
        cmd << " rcs " << quote_path(output_path);
        for (const auto& obj : object_files) {
            cmd << " " << quote_path(obj);
        }
    }

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
    return "Linker (subprocess: " + linker.get_lld_path().filename().string() + ")";
}

} // namespace tml::backend
