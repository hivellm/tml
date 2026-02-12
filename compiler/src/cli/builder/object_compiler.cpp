//! # Object Compiler
//!
//! This file implements the final stage of compilation: converting LLVM IR
//! to native object files and linking them into executables or libraries.
//!
//! ## Compilation Pipeline
//!
//! ```text
//! .ll (LLVM IR) → clang -c → .obj/.o (Object File)
//!                                    ↓
//! Multiple objects → clang/llvm-ar → .exe/.dll/.a/.so
//! ```
//!
//! ## Output Types
//!
//! | Type       | Windows     | Unix        | Command              |
//! |------------|-------------|-------------|----------------------|
//! | Executable | `.exe`      | (no ext)    | `clang -o`           |
//! | Static Lib | `.lib`      | `.a`        | `llvm-ar rcs`        |
//! | Dynamic Lib| `.dll`      | `.so`       | `clang -shared`      |
//!
//! ## Optimization Levels
//!
//! | Level | Flag  | Description                    |
//! |-------|-------|--------------------------------|
//! | 0     | `-O0` | No optimization                |
//! | 1     | `-O1` | Basic optimizations            |
//! | 2     | `-O2` | Standard optimizations         |
//! | 3     | `-O3` | Aggressive optimizations       |
//! | 4     | `-Os` | Optimize for size              |
//! | 5     | `-Oz` | Optimize for size (aggressive) |

#include "object_compiler.hpp"

#include "backend/lld_linker.hpp"
#include "common.hpp"
#include "compiler_setup.hpp"

#ifdef TML_HAS_LLVM_BACKEND
#include "backend/llvm_backend.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tml::cli {

// Forward declarations for LLD integration
static LinkResult link_objects_with_lld(const std::vector<fs::path>& object_files,
                                        const fs::path& output_file, const LinkOptions& options);
static bool is_lld_available();

// Forward declarations for LLVM backend integration
static ObjectCompileResult compile_ll_with_llvm(const fs::path& ll_file,
                                                const fs::path& output_file,
                                                const ObjectCompileOptions& options);
static ObjectCompileResult compile_ll_with_clang(const fs::path& ll_file,
                                                 const fs::path& output_file,
                                                 const std::string& clang_path,
                                                 const ObjectCompileOptions& options);

/// Returns the platform-specific object file extension.
std::string get_object_extension() {
#ifdef _WIN32
    return ".obj";
#else
    return ".o";
#endif
}

/// Converts an optimization level to the corresponding clang flag.
std::string get_optimization_flag(int level) {
    switch (level) {
    case 0:
        return "-O0";
    case 1:
        return "-O1";
    case 2:
        return "-O2";
    case 3:
        return "-O3";
    case 4:
        return "-Os"; // Optimize for size
    case 5:
        return "-Oz"; // Optimize for size (aggressive)
    default:
        return "-O3";
    }
}

/// Converts backslashes to forward slashes for cross-platform compatibility.
///
/// Clang on Windows accepts both path styles, but using forward slashes
/// avoids potential escaping issues in command strings.
static std::string to_forward_slashes(const fs::path& path) {
    std::string result = path.string();
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

/// Quotes a command path if it contains spaces.
static std::string quote_command(const std::string& cmd) {
    if (cmd.find(' ') != std::string::npos) {
        return "\"" + cmd + "\"";
    }
    return cmd;
}

/// Check if LLVM backend is available for self-contained compilation
bool is_llvm_backend_available() {
#ifdef TML_HAS_LLVM_BACKEND
    return backend::is_llvm_backend_available();
#else
    return false;
#endif
}

/// Compiles an LLVM IR file to a native object file.
///
/// Routes to either the built-in LLVM backend (self-contained) or clang
/// subprocess depending on the compiler_backend option.
///
/// ## Backend Selection
///
/// - `Auto`: Use LLVM backend if available, otherwise fall back to clang
/// - `LLVM`: Use built-in LLVM C API (no external dependencies)
/// - `Clang`: Use clang subprocess (requires clang installation)
ObjectCompileResult compile_ll_to_object(const fs::path& ll_file,
                                         const std::optional<fs::path>& output_file,
                                         const std::string& clang_path,
                                         const ObjectCompileOptions& options) {
    ObjectCompileResult result;
    result.success = false;

    // Verify input file exists
    if (!fs::exists(ll_file)) {
        result.error_message = "LLVM IR file not found: " + ll_file.string();
        return result;
    }

    // Determine output file path
    fs::path obj_file;
    if (output_file.has_value()) {
        obj_file = output_file.value();
    } else {
        // Auto-generate: same name as .ll but with .o/.obj extension
        obj_file = ll_file;
        obj_file.replace_extension(get_object_extension());
    }

    // Determine which backend to use
    bool use_llvm_backend = false;
    if (CompilerOptions::use_external_tools) {
        // --use-external-tools: force clang backend
        use_llvm_backend = false;
    } else if (options.compiler_backend == CompilerBackend::LLVM) {
        use_llvm_backend = true;
    } else if (options.compiler_backend == CompilerBackend::Auto) {
        // Auto-detect: prefer LLVM if available (unless LTO enabled, which needs clang)
        if (!options.lto && is_llvm_backend_available()) {
            use_llvm_backend = true;
        }
    }

    // Route to appropriate backend
    if (use_llvm_backend) {
        TML_LOG_DEBUG("build", "[object_compiler] Using LLVM backend");
        return compile_ll_with_llvm(ll_file, obj_file, options);
    } else {
        TML_LOG_DEBUG("build", "[object_compiler] Using clang backend");
        return compile_ll_with_clang(ll_file, obj_file, clang_path, options);
    }
}

// ============================================================================
// LLVM Backend Compilation
// ============================================================================

/// Compiles LLVM IR to object using the built-in LLVM C API backend.
///
/// This is the self-contained compilation path that doesn't require
/// external tools like clang.
static ObjectCompileResult
compile_ll_with_llvm([[maybe_unused]] const fs::path& ll_file,
                     [[maybe_unused]] const fs::path& output_file,
                     [[maybe_unused]] const ObjectCompileOptions& options) {
    ObjectCompileResult result;
    result.success = false;

#ifdef TML_HAS_LLVM_BACKEND
    backend::LLVMBackend llvm_backend;
    if (!llvm_backend.initialize()) {
        result.error_message =
            "Failed to initialize LLVM backend: " + llvm_backend.get_last_error();
        return result;
    }

    // Convert ObjectCompileOptions to LLVMCompileOptions
    backend::LLVMCompileOptions llvm_opts;
    llvm_opts.optimization_level = options.optimization_level;
    llvm_opts.debug_info = options.debug_info;
    llvm_opts.target_triple = options.target_triple;
    llvm_opts.position_independent = options.position_independent;
    llvm_opts.verbose = options.verbose;
    llvm_opts.cpu = "native";

    // Compile IR file to object
    auto llvm_result = llvm_backend.compile_ir_file_to_object(ll_file, output_file, llvm_opts);

    if (!llvm_result.success) {
        result.error_message = "LLVM backend compilation failed: " + llvm_result.error_message;
        return result;
    }

    result.success = true;
    result.object_file = llvm_result.object_file;
    return result;
#else
    result.error_message = "LLVM backend not available (TML_HAS_LLVM_BACKEND not defined)";
    return result;
#endif
}

// ============================================================================
// In-Memory IR String Compilation
// ============================================================================

/// Compiles LLVM IR from an in-memory string directly to an object file.
///
/// When the LLVM backend is available, this avoids all disk I/O for the IR —
/// the string goes directly to LLVM's IR parser → optimizer → codegen → .obj.
/// When LLVM is not available, falls back to writing a temp .ll and using clang.
ObjectCompileResult compile_ir_string_to_object(const std::string& ir_content,
                                                const fs::path& output_file,
                                                const std::string& clang_path,
                                                const ObjectCompileOptions& options) {
    ObjectCompileResult result;
    result.success = false;

    // Determine which backend to use
    bool use_llvm_backend = false;
    if (CompilerOptions::use_external_tools) {
        use_llvm_backend = false;
    } else if (options.compiler_backend == CompilerBackend::LLVM) {
        use_llvm_backend = true;
    } else if (options.compiler_backend == CompilerBackend::Auto) {
        if (!options.lto && is_llvm_backend_available()) {
            use_llvm_backend = true;
        }
    }

    if (use_llvm_backend) {
#ifdef TML_HAS_LLVM_BACKEND
        TML_LOG_DEBUG("build", "[object_compiler] Using LLVM backend (in-memory IR)");

        backend::LLVMBackend llvm_backend;
        if (!llvm_backend.initialize()) {
            result.error_message =
                "Failed to initialize LLVM backend: " + llvm_backend.get_last_error();
            return result;
        }

        // Convert options
        backend::LLVMCompileOptions llvm_opts;
        llvm_opts.optimization_level = options.optimization_level;
        llvm_opts.debug_info = options.debug_info;
        llvm_opts.target_triple = options.target_triple;
        llvm_opts.position_independent = options.position_independent;
        llvm_opts.verbose = options.verbose;
        llvm_opts.cpu = "native";

        auto llvm_result = llvm_backend.compile_ir_to_object(ir_content, output_file, llvm_opts);

        if (!llvm_result.success) {
            result.error_message = "LLVM backend compilation failed: " + llvm_result.error_message;
            return result;
        }

        result.success = true;
        result.object_file = llvm_result.object_file;
        return result;
#else
        // Should not reach here, but fall through to clang
        (void)use_llvm_backend;
#endif
    }

    // Fallback: write temp .ll file and use compile_ll_to_object
    TML_LOG_DEBUG("build", "[object_compiler] Falling back to temp .ll + clang");

    fs::path temp_ll = output_file;
    temp_ll.replace_extension(".ll");

    // Write IR to temp file
    {
        std::ofstream ofs(temp_ll);
        if (!ofs) {
            result.error_message = "Failed to write temporary IR file: " + temp_ll.string();
            return result;
        }
        ofs << ir_content;
    }

    // Compile via the file-based path
    result = compile_ll_to_object(temp_ll, output_file, clang_path, options);

    // Clean up temp file
    std::error_code ec;
    fs::remove(temp_ll, ec);

    return result;
}

// ============================================================================
// In-Memory Buffer Compilation
// ============================================================================

/// Compiles LLVM IR string to an in-memory object buffer (no disk I/O).
///
/// Uses LLVMTargetMachineEmitToMemoryBuffer when the LLVM backend is available.
ObjectCompileResult compile_ir_string_to_buffer(const std::string& ir_content,
                                                const ObjectCompileOptions& options) {
    ObjectCompileResult result;
    result.success = false;

#ifdef TML_HAS_LLVM_BACKEND
    if (!CompilerOptions::use_external_tools && !options.lto && is_llvm_backend_available()) {
        TML_LOG_DEBUG("build", "[object_compiler] Using LLVM backend (in-memory buffer)");

        backend::LLVMBackend llvm_backend;
        if (!llvm_backend.initialize()) {
            result.error_message =
                "Failed to initialize LLVM backend: " + llvm_backend.get_last_error();
            return result;
        }

        backend::LLVMCompileOptions llvm_opts;
        llvm_opts.optimization_level = options.optimization_level;
        llvm_opts.debug_info = options.debug_info;
        llvm_opts.target_triple = options.target_triple;
        llvm_opts.position_independent = options.position_independent;
        llvm_opts.verbose = options.verbose;
        llvm_opts.cpu = "native";

        auto llvm_result = llvm_backend.compile_ir_to_buffer(ir_content, llvm_opts);

        if (!llvm_result.success) {
            result.error_message =
                "LLVM backend in-memory compilation failed: " + llvm_result.error_message;
            return result;
        }

        result.success = true;
        result.object_data = std::move(llvm_result.object_data);
        return result;
    }
#endif

    // Fallback: compile to temp file and read back
    auto temp_dir = fs::temp_directory_path();
    fs::path temp_obj = temp_dir / ("tml_tmp" + get_object_extension());

    auto file_result = compile_ir_string_to_object(ir_content, temp_obj, "", options);
    if (!file_result.success) {
        result.error_message = file_result.error_message;
        return result;
    }

    // Read the temp file into memory
    std::ifstream ifs(temp_obj, std::ios::binary | std::ios::ate);
    if (!ifs) {
        result.error_message = "Failed to read temp object file: " + temp_obj.string();
        return result;
    }
    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    result.object_data.resize(static_cast<size_t>(size));
    ifs.read(reinterpret_cast<char*>(result.object_data.data()), size);
    ifs.close();

    // Clean up temp file
    std::error_code ec;
    fs::remove(temp_obj, ec);

    result.success = true;
    return result;
}

// ============================================================================
// Clang Subprocess Compilation
// ============================================================================

/// Compiles LLVM IR to object using clang subprocess.
///
/// ## Clang Flags Used
///
/// - `-c`: Compile only (no linking)
/// - `-target`: Target triple for cross-compilation
/// - `-march=native -mtune=native`: CPU-specific optimizations
/// - `-fomit-frame-pointer`: Better code generation
/// - `-funroll-loops`: Loop unrolling optimization
/// - `-flto[=thin]`: Link-Time Optimization (if enabled)
/// - `-g`: Debug information (if enabled)
/// - `-fPIC`: Position-independent code (for shared libs)
static ObjectCompileResult compile_ll_with_clang(const fs::path& ll_file,
                                                 const fs::path& output_file,
                                                 const std::string& clang_path,
                                                 const ObjectCompileOptions& options) {
    ObjectCompileResult result;
    result.success = false;

    // Check if clang is available
    if (clang_path.empty()) {
        result.error_message =
            "Compiler backend not available. Neither the built-in LLVM backend nor clang is "
            "available.\n"
            "  This typically means TML was built without LLVM support and clang is not "
            "installed.\n"
            "  Solutions:\n"
            "  1. Install clang/LLVM and ensure it's in your PATH\n"
            "  2. Rebuild TML with -DTML_USE_LLVM_BACKEND=ON for self-contained compilation";
        return result;
    }

    // Build clang command
    std::ostringstream cmd;
    cmd << quote_command(clang_path); // Quote path only if it has spaces
    cmd << " -c";                     // Compile only, don't link

    // Optimization level
    cmd << " " << get_optimization_flag(options.optimization_level);

    // Target triple (use provided or default to host)
    if (!options.target_triple.empty()) {
        cmd << " -target " << options.target_triple;
    } else {
#ifdef _WIN32
        // Windows: use native object format
        cmd << " -target x86_64-pc-windows-msvc";
#else
        // Unix: use ELF object format
        cmd << " -target x86_64-unknown-linux-gnu";
#endif
    }

    // Sysroot for cross-compilation
    if (!options.sysroot.empty()) {
        cmd << " --sysroot=\"" << to_forward_slashes(fs::path(options.sysroot)) << "\"";
    }

    // Position-independent code for shared libraries
    if (options.position_independent) {
        cmd << " -fPIC";
    }

    // Debug information
    if (options.debug_info) {
        cmd << " -g";
    }

    // Additional flags for better codegen
    cmd << " -march=native";
    cmd << " -mtune=native";
    cmd << " -fomit-frame-pointer";
    cmd << " -funroll-loops";

    // SROA (Scalar Replacement of Aggregates) optimization is enabled by default at -O2+
    // LLVM's SROA pass breaks up stack-allocated structs into individual registers
    // This is critical for OOP performance - stack-promoted objects become zero-cost
    // Note: Custom LLVM options removed as they vary by LLVM version

    // Link-Time Optimization
    if (options.lto) {
        if (options.thin_lto) {
            cmd << " -flto=thin";
        } else {
            cmd << " -flto";
        }
    }

    // LLVM Source Code Coverage Instrumentation
    // Note: -fprofile-instr-generate enables instrumentation for profile-guided optimization
    // For LLVM IR input, this links the profile runtime but doesn't add instrumentation
    // TODO: To get true coverage, we'd need to add instrumentation in our own LLVM IR codegen
    if (options.coverage) {
        cmd << " -fprofile-instr-generate";
        cmd << " -fcoverage-mapping";
    }

    // Suppress warnings
    cmd << " -Wno-override-module";

    // Input and output
    cmd << " -o \"" << to_forward_slashes(output_file) << "\"";
    cmd << " \"" << to_forward_slashes(ll_file) << "\"";

    std::string command = cmd.str();

    TML_LOG_DEBUG("build", "[clang] " << command);

    // Execute compilation
    int ret = std::system(command.c_str());

    if (ret != 0) {
        result.error_message = "Clang compilation failed with exit code " + std::to_string(ret);
        return result;
    }

    // Verify object file was created
    if (!fs::exists(output_file)) {
        result.error_message = "Object file was not created: " + output_file.string();
        return result;
    }

    result.success = true;
    result.object_file = output_file;
    return result;
}

/// Links multiple object files into a final output.
///
/// ## Output Types
///
/// - **Executable**: Uses clang as linker driver
/// - **Static Library**: Uses llvm-ar (or system ar)
/// - **Dynamic Library**: Uses clang with -shared
///
/// ## Platform Differences
///
/// | Feature          | Windows              | Unix                 |
/// |------------------|----------------------|----------------------|
/// | Linker           | lld                  | system ld or lld     |
/// | DLL imports      | .lib import library  | not needed           |
/// | Symbol export    | -export-all-symbols  | -fPIC                |
LinkResult link_objects(const std::vector<fs::path>& object_files, const fs::path& output_file,
                        const std::string& clang_path, const LinkOptions& options) {
    LinkResult result;
    result.success = false;

    // Verify at least one object file provided
    if (object_files.empty()) {
        result.error_message = "No object files provided for linking";
        return result;
    }

    // Verify all object files exist
    for (const auto& obj : object_files) {
        if (!fs::exists(obj)) {
            result.error_message = "Object file not found: " + obj.string();
            return result;
        }
    }

    // Check if we should use LLD directly
    bool use_lld = false;
    if (CompilerOptions::use_external_tools) {
        // --use-external-tools: force clang linker driver
        use_lld = false;
    } else if (options.linker_backend == LinkerBackend::LLD) {
        use_lld = true;
    } else if (options.linker_backend == LinkerBackend::Auto) {
        // Auto-detect: use LLD if available and not using LTO (LTO needs clang driver)
        if (!options.lto && is_lld_available()) {
            use_lld = true;
        }
    }

    // Use LLD for direct linking (self-contained)
    if (use_lld) {
        TML_LOG_DEBUG("build", "[linker] Using LLD backend");
        return link_objects_with_lld(object_files, output_file, options);
    }

    // Fall back to clang as linker driver
    // Check if clang is available for fallback
    if (clang_path.empty()) {
        result.error_message = "Linker not available. Neither LLD nor clang is installed.\n"
                               "  The TML compiler normally uses LLD for self-contained linking.\n"
                               "  Solutions:\n"
                               "  1. Ensure lld-link.exe (Windows) or ld.lld (Unix) is available\n"
                               "  2. Install clang/LLVM and ensure it's in your PATH\n"
                               "  3. Set LLVM_DIR environment variable to your LLVM installation";
        return result;
    }

    std::ostringstream cmd;

    // Different linking strategies based on output type
    switch (options.output_type) {
    case LinkOptions::OutputType::Executable: {
        // Link executable using clang
        cmd << quote_command(clang_path); // Quote path only if it has spaces

        // Target triple for cross-compilation
        if (!options.target_triple.empty()) {
            cmd << " -target " << options.target_triple;
        }

        // Sysroot for cross-compilation
        if (!options.sysroot.empty()) {
            cmd << " --sysroot=\"" << to_forward_slashes(fs::path(options.sysroot)) << "\"";
        }

        // Link-Time Optimization
        if (options.lto) {
            if (options.thin_lto) {
                cmd << " -flto=thin";
            } else {
                cmd << " -flto";
            }
            // Parallel LTO jobs
            if (options.lto_jobs > 0) {
                cmd << " -flto-jobs=" << options.lto_jobs;
            }
            // Use LLD for faster LTO linking
            cmd << " -fuse-ld=lld";
        }

        // LLVM Source Code Coverage (links profile runtime)
        if (options.coverage) {
            cmd << " -fprofile-instr-generate";
            // Link the profile runtime library
            std::string profile_rt = find_llvm_profile_runtime();
            if (!profile_rt.empty()) {
                cmd << " \"" << to_forward_slashes(profile_rt) << "\"";
            }
        }

        // Output file
        cmd << " -o \"" << to_forward_slashes(output_file) << "\"";

        // All object files
        for (const auto& obj : object_files) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }

        // Additional objects (runtime libs)
        for (const auto& obj : options.additional_objects) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }

        // Additional link flags
        for (const auto& flag : options.link_flags) {
            cmd << " " << flag;
        }

        break;
    }

    case LinkOptions::OutputType::StaticLib: {
        // Use llvm-ar for cross-platform static library creation
        // Fall back to system ar on Unix if llvm-ar not available
        fs::path clang_dir = fs::path(clang_path).parent_path();
        fs::path llvm_ar = clang_dir / "llvm-ar";
#ifdef _WIN32
        llvm_ar += ".exe";
#endif

        std::string ar_cmd;
        if (fs::exists(llvm_ar)) {
            ar_cmd = to_forward_slashes(llvm_ar.string());
        } else {
#ifdef _WIN32
            // On Windows, llvm-ar should be available with LLVM
            ar_cmd = to_forward_slashes(llvm_ar.string());
#else
            // On Unix, fall back to system ar
            ar_cmd = "ar";
#endif
        }

        cmd << ar_cmd;
        cmd << " rcs \"" << to_forward_slashes(output_file) << "\"";

        for (const auto& obj : object_files) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }
        for (const auto& obj : options.additional_objects) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }
        break;
    }

    case LinkOptions::OutputType::DynamicLib: {
        // Shared library using clang
        cmd << quote_command(clang_path); // Quote path only if it has spaces
        cmd << " -shared";

        // Target triple for cross-compilation
        if (!options.target_triple.empty()) {
            cmd << " -target " << options.target_triple;
        }

        // Sysroot for cross-compilation
        if (!options.sysroot.empty()) {
            cmd << " --sysroot=\"" << to_forward_slashes(fs::path(options.sysroot)) << "\"";
        }

        // Link-Time Optimization for shared libraries
        if (options.lto) {
            if (options.thin_lto) {
                cmd << " -flto=thin";
            } else {
                cmd << " -flto";
            }
            if (options.lto_jobs > 0) {
                cmd << " -flto-jobs=" << options.lto_jobs;
            }
        }

        // LLVM Source Code Coverage (links profile runtime)
        if (options.coverage) {
            cmd << " -fprofile-instr-generate";
            // Link the profile runtime library
            std::string profile_rt = find_llvm_profile_runtime();
            if (!profile_rt.empty()) {
                cmd << " \"" << to_forward_slashes(profile_rt) << "\"";
            }
        }

#ifdef _WIN32
        // Windows: use LLD linker
        cmd << " -fuse-ld=lld";
        // Export all symbols from DLL (MSVC-style flag)
        cmd << " -Wl,-export-all-symbols";
        // Create import library alongside DLL (MSVC-style flag)
        fs::path lib_file = output_file;
        lib_file.replace_extension(".lib");
        cmd << " -Wl,-implib:" << to_forward_slashes(lib_file);
#else
        // Unix: position-independent code required for shared libraries
        cmd << " -fPIC";
        if (options.lto) {
            cmd << " -fuse-ld=lld";
        }
#endif

        // Output file
        cmd << " -o \"" << to_forward_slashes(output_file) << "\"";

        // All object files
        for (const auto& obj : object_files) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }

        // Additional objects
        for (const auto& obj : options.additional_objects) {
            cmd << " \"" << to_forward_slashes(obj) << "\"";
        }

        // Additional link flags
        for (const auto& flag : options.link_flags) {
            cmd << " " << flag;
        }

        break;
    }
    }

    std::string command = cmd.str();

    TML_LOG_DEBUG("build", "[linker] " << command);

    // Execute linking
    int ret = std::system(command.c_str());

    if (ret != 0) {
        result.error_message = "Linking failed with exit code " + std::to_string(ret);
        return result;
    }

    // Verify output file was created
    if (!fs::exists(output_file)) {
        result.error_message = "Output file was not created: " + output_file.string();
        return result;
    }

    result.success = true;
    result.output_file = output_file;
    return result;
}

// ============================================================================
// LLD-based Linking
// ============================================================================

/// Links objects using LLD directly (self-contained, no clang dependency)
static LinkResult link_objects_with_lld(const std::vector<fs::path>& object_files,
                                        const fs::path& output_file, const LinkOptions& options) {
    LinkResult result;
    result.success = false;

    // Initialize LLD linker
    backend::LLDLinker linker;
    if (!linker.initialize()) {
        result.error_message = "Failed to initialize LLD linker: " + linker.get_last_error();
        return result;
    }

    TML_LOG_DEBUG("build", "[lld_linker] Using LLD at: " << linker.get_lld_path());

    // Convert LinkOptions to LLDLinkOptions
    backend::LLDLinkOptions lld_opts;

    switch (options.output_type) {
    case LinkOptions::OutputType::Executable:
        lld_opts.output_type = backend::LLDOutputType::Executable;
        break;
    case LinkOptions::OutputType::StaticLib:
        lld_opts.output_type = backend::LLDOutputType::StaticLib;
        break;
    case LinkOptions::OutputType::DynamicLib:
        lld_opts.output_type = backend::LLDOutputType::SharedLib;
        break;
    }

    lld_opts.verbose = options.verbose;
    lld_opts.debug_info = false; // Could be added to LinkOptions if needed
    lld_opts.target_triple = options.target_triple;
    lld_opts.extra_flags = options.link_flags;

    // Convert additional objects to library paths
    for (const auto& obj : options.additional_objects) {
        lld_opts.library_paths.push_back(obj.parent_path());
    }

    // Combine object files
    std::vector<fs::path> all_objects = object_files;
    for (const auto& obj : options.additional_objects) {
        all_objects.push_back(obj);
    }

    // Add profile runtime library for LLVM source coverage
    if (options.coverage) {
        std::string profile_rt = find_llvm_profile_runtime();
        if (!profile_rt.empty()) {
            all_objects.push_back(fs::path(profile_rt));
        }
        // Export the profile write function so we can call it before DLL unload
        lld_opts.extra_flags.push_back("/EXPORT:__llvm_profile_write_file");
    }

    // For DLLs (test suites), export core test functions
    // LLD on Windows requires explicit exports even for __declspec(dllexport) symbols
    if (options.output_type == LinkOptions::OutputType::DynamicLib) {
        // Core test functions - always exported
        lld_opts.extra_flags.push_back("/EXPORT:tml_run_test_with_catch");
        lld_opts.extra_flags.push_back("/EXPORT:tml_set_output_suppressed");
        // Coverage functions - only exported when coverage is enabled
        if (CompilerOptions::coverage) {
            lld_opts.extra_flags.push_back("/EXPORT:tml_print_coverage_report");
            lld_opts.extra_flags.push_back("/EXPORT:print_coverage_report");
            lld_opts.extra_flags.push_back("/EXPORT:write_coverage_html");
            lld_opts.extra_flags.push_back("/EXPORT:write_coverage_json");
            lld_opts.extra_flags.push_back("/EXPORT:tml_cover_func");
            lld_opts.extra_flags.push_back("/EXPORT:tml_get_func_count");
            lld_opts.extra_flags.push_back("/EXPORT:tml_get_func_name");
            lld_opts.extra_flags.push_back("/EXPORT:tml_get_func_hits");
            lld_opts.extra_flags.push_back("/EXPORT:tml_get_covered_func_count");
        }
    }

    // Link
    auto lld_result = linker.link(all_objects, output_file, lld_opts);

    if (!lld_result.success) {
        result.error_message = "LLD linking failed: " + lld_result.error_message;
        return result;
    }

    result.success = true;
    result.output_file = lld_result.output_file;
    return result;
}

/// Check if LLD is available for linking
static bool is_lld_available() {
    backend::LLDLinker linker;
    return linker.initialize();
}

// ============================================================================
// Batch Compilation
// ============================================================================

/// Compiles multiple CGU IR strings to object files in parallel.
///
/// Uses a thread pool with atomic index for work distribution.
/// Each thread initializes its own LLVM backend and compiles CGUs
/// independently, enabling true parallel LLVM compilation.
BatchCompileResult compile_cgus_parallel(const std::vector<CGUCompileJob>& jobs,
                                         const std::string& clang_path,
                                         const ObjectCompileOptions& options, int num_threads) {
    BatchCompileResult result;
    result.success = true;

    if (jobs.empty()) {
        return result;
    }

    // Determine number of threads
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0)
            num_threads = 4;
    }

    // Limit threads to number of jobs
    if (jobs.size() < static_cast<size_t>(num_threads)) {
        num_threads = static_cast<int>(jobs.size());
    }

    // Pre-allocate result slots (one per job, in order)
    result.object_files.resize(jobs.size());
    std::vector<bool> slot_ok(jobs.size(), false);

    // Thread-safe error collection
    std::mutex error_mutex;
    std::atomic<size_t> current_index{0};
    std::atomic<bool> any_failure{false};

    // Worker function
    auto worker = [&]() {
        while (!any_failure.load(std::memory_order_relaxed)) {
            size_t index = current_index.fetch_add(1);
            if (index >= jobs.size()) {
                break;
            }

            const auto& job = jobs[index];

            // Compile this CGU's IR string to object file
            auto compile_result =
                compile_ir_string_to_object(job.ir_content, job.output_path, clang_path, options);

            if (compile_result.success) {
                result.object_files[index] = compile_result.object_file;
                slot_ok[index] = true;
                TML_LOG_INFO("build", "CGU " << job.cgu_index << ": compiled ("
                                             << job.fingerprint_tag << ")");
            } else {
                any_failure.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(error_mutex);
                result.success = false;
                result.errors.push_back("CGU " + std::to_string(job.cgu_index) +
                                        " compilation failed: " + compile_result.error_message);
            }
        }
    };

    TML_LOG_INFO("build", "Parallel CGU compilation: " << jobs.size() << " CGUs, " << num_threads
                                                       << " threads");

    // Launch worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back(worker);
    }

    // Wait for completion
    for (auto& thread : workers) {
        thread.join();
    }

    // If there was a failure, clear the object files
    if (!result.success) {
        result.object_files.clear();
        return result;
    }

    // Verify all slots were filled
    for (size_t i = 0; i < jobs.size(); ++i) {
        if (!slot_ok[i]) {
            result.success = false;
            result.errors.push_back("CGU " + std::to_string(jobs[i].cgu_index) +
                                    " produced no output");
            result.object_files.clear();
            return result;
        }
    }

    return result;
}

/// Compiles multiple LLVM IR files to objects in parallel.
///
/// Uses a thread pool with atomic index for work distribution.
/// Each thread grabs the next file index and compiles it until
/// all files are processed.
BatchCompileResult compile_ll_batch(const std::vector<fs::path>& ll_files,
                                    const std::string& clang_path,
                                    const ObjectCompileOptions& options, int num_threads) {
    BatchCompileResult result;
    result.success = true;

    if (ll_files.empty()) {
        return result;
    }

    // Determine number of threads
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0)
            num_threads = 4;
    }

    // Limit threads to number of files
    if (ll_files.size() < static_cast<size_t>(num_threads)) {
        num_threads = static_cast<int>(ll_files.size());
    }

    // Thread-safe result collection
    std::mutex result_mutex;
    std::atomic<size_t> current_index{0};

    // Worker function
    auto worker = [&]() {
        while (true) {
            size_t index = current_index.fetch_add(1);
            if (index >= ll_files.size()) {
                break;
            }

            const auto& ll_file = ll_files[index];

            // Compile this file
            auto compile_result = compile_ll_to_object(ll_file, std::nullopt, clang_path, options);

            // Store result
            std::lock_guard<std::mutex> lock(result_mutex);
            if (compile_result.success) {
                result.object_files.push_back(compile_result.object_file);
            } else {
                result.success = false;
                result.errors.push_back(compile_result.error_message);
            }
        }
    };

    // Launch worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back(worker);
    }

    // Wait for completion
    for (auto& thread : workers) {
        thread.join();
    }

    return result;
}

} // namespace tml::cli
