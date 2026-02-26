TML_MODULE("compiler")

//! # Run Command Implementation
//!
//! This file implements the `tml run` command that compiles and immediately
//! executes TML programs. Uses the query-based pipeline with incremental
//! compilation for fast re-execution.
//!
//! ## Execution Flow
//!
//! ```text
//! run_run()
//!   ├─ compile_via_queries() (8-stage memoized pipeline)
//!   │   └─ QueryContext::codegen_unit() → cascades all stages
//!   ├─ Cache object/executable by content hash
//!   ├─ Execute with provided arguments
//!   └─ Clean up temporary files
//! ```
//!
//! ## Caching Strategy
//!
//! Two layers of caching:
//! 1. Query pipeline: incremental fingerprints skip unchanged stages
//! 2. Object/exe cache: content-hash in `build/debug/.run-cache/`

#include "builder_internal.hpp"
#include "query/query_context.hpp"
#include "types/module_binary.hpp"

namespace tml::cli {

// Using helpers from builder namespace
using namespace build;

// ============================================================================
// Shared query-based compilation helper
// ============================================================================

namespace {

/// Ensure required DLLs are in the given directory (for Windows runtime dependencies).
/// Copies vcpkg DLLs (zlib, zstd, brotli) next to the executable so they're found at runtime.
static void ensure_runtime_dlls(const fs::path& target_dir) {
#ifdef _WIN32
    // vcpkg DLLs that may be needed by tml_zlib_runtime
    static const std::vector<std::string> dll_names = {"zlib1.dll",        "zstd.dll",
                                                       "brotlicommon.dll", "brotlidec.dll",
                                                       "brotlienc.dll",    "sqlite3.dll"};

    // Search for DLLs in known locations
    std::vector<fs::path> search_dirs = {
        "src/x64-windows/bin",
        "../src/x64-windows/bin",
        "F:/Node/hivellm/tml/src/x64-windows/bin",
        "vcpkg_installed/x64-windows/bin",
        "../vcpkg_installed/x64-windows/bin",
        "F:/Node/hivellm/tml/vcpkg_installed/x64-windows/bin",
    };

    for (const auto& dll_name : dll_names) {
        auto target = target_dir / dll_name;
        if (fs::exists(target))
            continue; // Already copied

        for (const auto& dir : search_dirs) {
            auto src = fs::path(dir) / dll_name;
            if (fs::exists(src)) {
                std::error_code ec;
                fs::copy_file(src, target, fs::copy_options::skip_existing, ec);
                break;
            }
        }
    }
#else
    (void)target_dir;
#endif
}

/// Result of compiling via the query pipeline.
struct RunCompileResult {
    bool success = false;
    std::string llvm_ir;
    fs::path object_file; ///< Direct object file (Cranelift path, empty for LLVM)
    std::set<std::string> link_libs;
    std::shared_ptr<types::ModuleRegistry> registry;
    std::shared_ptr<parser::Module> module;
    std::string error_message; // Pre-formatted errors for quiet mode
};

/// Compile a TML file via the query-based pipeline (QueryContext).
///
/// This replaces the manual preprocess->lex->parse->typecheck->borrow->codegen pipeline
/// with the memoized 8-stage query system that supports incremental compilation.
RunCompileResult compile_via_queries(const std::string& path, bool coverage, bool no_cache,
                                     const std::string& backend = "llvm",
                                     const std::string& pipeline_dir = "") {
    RunCompileResult result;

    // Pre-load all library modules from .tml.meta binary cache
    types::preload_all_meta_caches();

    auto module_name = fs::path(path).stem().string();

    // Set up query options from current compiler globals
    query::QueryOptions qopts;
    qopts.verbose = false;
    qopts.debug_info = tml::CompilerOptions::debug_info;
    qopts.coverage = coverage;
    qopts.optimization_level = tml::CompilerOptions::optimization_level;
    qopts.target_triple = tml::CompilerOptions::target_triple;
    qopts.sysroot = tml::CompilerOptions::sysroot;
    qopts.incremental = !no_cache;
    qopts.backend = backend;
    if (!pipeline_dir.empty()) {
        qopts.emit_pipeline = true;
        qopts.pipeline_output_dir = pipeline_dir;
    }

    auto source_dir = fs::path(path).parent_path();
    if (source_dir.empty()) {
        source_dir = fs::current_path();
    }
    qopts.source_directory = source_dir.string();

    query::QueryContext qctx(qopts);

    // Load incremental cache from previous session
    fs::path cache_dir = get_run_cache_dir();
    if (qopts.incremental) {
        qctx.load_incremental_cache(cache_dir);
    }

    // Run the full pipeline via queries (8 memoized stages with GREEN reuse)
    auto codegen_result = qctx.codegen_unit(path, module_name);

    if (!codegen_result.success) {
        // Extract error messages from the first failing stage
        std::string err;
        auto tok = qctx.cache().lookup<query::TokenizeResult>(query::TokenizeKey{path});
        if (tok && !tok->success) {
            for (const auto& e : tok->errors)
                err += e + "\n";
        }
        auto parsed =
            qctx.cache().lookup<query::ParseModuleResult>(query::ParseModuleKey{path, module_name});
        if (parsed && !parsed->success) {
            for (const auto& e : parsed->errors)
                err += e + "\n";
        }
        auto tc = qctx.cache().lookup<query::TypecheckResult>(
            query::TypecheckModuleKey{path, module_name});
        if (tc && !tc->success) {
            for (const auto& e : tc->errors)
                err += e + "\n";
        }
        auto bc = qctx.cache().lookup<query::BorrowcheckResult>(
            query::BorrowcheckModuleKey{path, module_name});
        if (bc && !bc->success) {
            for (const auto& e : bc->errors)
                err += e + "\n";
        }
        if (err.empty()) {
            err = codegen_result.error_message;
        }

        result.error_message = err.empty() ? "compilation failed" : err;
        return result;
    }

    // Extract registry and module from cached intermediate results.
    // During GREEN reuse (incremental), only CodegenUnitResult is loaded from cache —
    // intermediate results like TypecheckResult are NOT populated. In that case,
    // force-run typecheck to get the registry (needed for runtime detection).
    auto tc =
        qctx.cache().lookup<query::TypecheckResult>(query::TypecheckModuleKey{path, module_name});
    if (!tc || !tc->success) {
        // During GREEN reuse (incremental), only CodegenUnitResult is loaded from cache.
        // Force typecheck to populate the registry (needed for runtime detection).
        auto tc_result = qctx.typecheck_module(path, module_name);
        if (tc_result.success) {
            tc = qctx.cache().lookup<query::TypecheckResult>(
                query::TypecheckModuleKey{path, module_name});
        }
    }

    auto parsed =
        qctx.cache().lookup<query::ParseModuleResult>(query::ParseModuleKey{path, module_name});

    result.success = true;

    result.llvm_ir = codegen_result.llvm_ir;
    result.object_file = codegen_result.object_file;
    result.link_libs = codegen_result.link_libs;
    result.registry =
        (tc && tc->success) ? tc->registry : std::make_shared<types::ModuleRegistry>();
    result.module = (parsed && parsed->success) ? parsed->module : nullptr;

    // Save incremental cache for next session
    if (qopts.incremental) {
        qctx.save_incremental_cache(cache_dir);
    }

    return result;
}

} // anonymous namespace

// ============================================================================
// run_run() — Interactive run with diagnostic output
// ============================================================================

/// Compiles and runs a TML program.
///
/// This is the implementation of `tml run <file>`. It compiles the source
/// file using the query-based pipeline (with incremental compilation),
/// then executes the resulting binary.
///
/// ## Return Value
///
/// Returns the exit code of the executed program.
int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose,
            bool coverage, bool no_cache, const std::string& backend,
            const std::string& pipeline_dir) {
    // Compile via query pipeline (incremental + memoized)
    auto compile = compile_via_queries(path, coverage, no_cache, backend, pipeline_dir);
    if (!compile.success) {
        TML_LOG_ERROR("build", compile.error_message);
        return 1;
    }

    const auto& llvm_ir = compile.llvm_ir;
    auto module_name = fs::path(path).stem().string();

    // Use centralized run cache - NEVER create files inside packages
    fs::path cache_dir = get_run_cache_dir();

    // Note: clang may be empty if LLVM backend and LLD are available (self-contained mode)
    std::string clang = find_clang();

    // Use global deps cache for precompiled runtimes
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

    ObjectCompileOptions obj_options;
    obj_options.optimization_level = tml::CompilerOptions::optimization_level;
    obj_options.debug_info = tml::CompilerOptions::debug_info;
    obj_options.verbose = verbose;
    obj_options.target_triple = tml::CompilerOptions::target_triple;
    obj_options.sysroot = tml::CompilerOptions::sysroot;

    fs::path exe_output = cache_dir / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    // Collect all object files to link
    std::vector<fs::path> object_files;

    if (!compile.object_file.empty()) {
        // Cranelift path: object file already produced by the backend
        object_files.push_back(compile.object_file);
        TML_LOG_DEBUG("build", "Using Cranelift object: " << compile.object_file);
    } else {
        // Monolithic LLVM path: compile IR text to object file
        std::string content_hash = generate_content_hash(llvm_ir);
        fs::path obj_output = cache_dir / (content_hash + get_object_extension());

        if (fs::exists(obj_output)) {
            TML_LOG_DEBUG("build", "Using cached object: " << obj_output);
        } else {
            auto obj_result = compile_ir_string_to_object(llvm_ir, obj_output, clang, obj_options);
            if (!obj_result.success) {
                TML_LOG_ERROR("build", "Object compilation failed: " << obj_result.error_message);
                return 1;
            }
            TML_LOG_DEBUG("build", "Compiled to: " << obj_result.object_file);
        }
        object_files.push_back(obj_output);
    }

    // Add runtime object files (registry detects which runtimes are needed)
    {
        parser::Module empty_module;
        const auto& mod = compile.module ? *compile.module : empty_module;
        auto runtime_objects =
            get_runtime_objects(compile.registry, mod, deps_cache, clang, verbose);
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());
    }

    // Generate hash for executable caching (source + all object files)
    std::string content_hash_str = !compile.object_file.empty()
                                       ? compile.object_file.filename().string()
                                       : generate_content_hash(llvm_ir);
    std::string exe_hash = generate_exe_hash(content_hash_str, object_files);
    fs::path cached_exe = cache_dir / (exe_hash + ".exe");

    // Check if we have a cached executable (skip if --no-cache)
    bool use_cached_exe = !no_cache && fs::exists(cached_exe);

    if (use_cached_exe) {
        TML_LOG_DEBUG("build", "Using cached executable: " << cached_exe);
    } else {
        // Link all object files to create executable
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::Executable;
        link_options.verbose = verbose;
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;

        // Add @link libraries from FFI decorators
        for (const auto& lib : compile.link_libs) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

#ifdef _WIN32
        // Add Windows system libraries for socket support
        if ((compile.module && has_socket_functions(*compile.module)) ||
            compile.registry->has_module("std::net") ||
            compile.registry->has_module("std::net::sys") ||
            compile.registry->has_module("std::net::tcp") ||
            compile.registry->has_module("std::net::udp")) {
            link_options.link_flags.push_back("-lws2_32");
        }
        // Add Windows system libraries for OS module (Registry, user info)
        if (compile.registry->has_module("std::os")) {
            link_options.link_flags.push_back("-ladvapi32");
            link_options.link_flags.push_back("-luserenv");
        }
        // Always link OpenSSL libraries (tml_runtime.lib contains crypto objects)
        {
            auto openssl = find_openssl();
            if (openssl.found) {
                link_options.link_flags.push_back(
                    to_forward_slashes((openssl.lib_dir / openssl.crypto_lib).string()));
                link_options.link_flags.push_back(
                    to_forward_slashes((openssl.lib_dir / openssl.ssl_lib).string()));
                link_options.link_flags.push_back("/DEFAULTLIB:crypt32");
                link_options.link_flags.push_back("/DEFAULTLIB:ws2_32");
            }
        }
        link_options.link_flags.push_back("/STACK:67108864");
#endif

        // Link to temporary location first
        fs::path temp_exe = cache_dir / (exe_hash + "_link_temp.exe");

        auto link_result = link_objects(object_files, temp_exe, clang, link_options);
        if (!link_result.success) {
            TML_LOG_ERROR("build", "Linking failed: " << link_result.error_message);
            return 1;
        }

        TML_LOG_DEBUG("build", "Linked executable: " << temp_exe);

        // Try to move to cached location (atomic on same filesystem)
        try {
            if (!fs::exists(cached_exe)) {
                fs::rename(temp_exe, cached_exe);
            } else {
                // Another process created it first
                fs::remove(temp_exe);
            }
        } catch (...) {
            // Clean up temp file on error
            if (fs::exists(temp_exe)) {
                fs::remove(temp_exe);
            }
        }
    }

    // Ensure runtime DLLs are available next to the executable
    ensure_runtime_dlls(cache_dir);

    // Copy cached exe to final location (use hard link for speed)
    if (!fast_copy_file(cached_exe, exe_output)) {
        TML_LOG_ERROR("build", "Failed to copy cached exe to " << exe_output);
        return 1;
    }

    std::string exe_path = to_forward_slashes(exe_output.string());
    std::string run_cmd;
#ifdef _WIN32
    // Windows: use cmd /c to properly handle relative paths with forward slashes
    run_cmd = "cmd /c \"\"" + exe_path + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }
    run_cmd += "\"";
#else
    run_cmd = "\"" + exe_path + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }
#endif

    TML_LOG_DEBUG("build", "Running: " << run_cmd);

    int run_ret = std::system(run_cmd.c_str());

    // Clean up temporary executable (keep .obj in cache for reuse)
    fs::remove(exe_output);

    TML_LOG_DEBUG("build", "Cleaned up temporary executable");

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

// ============================================================================
// run_run_quiet() — Quiet run with output capture (used by test system)
// ============================================================================

int run_run_quiet(const std::string& path, const std::vector<std::string>& args, bool verbose,
                  std::string* output, bool coverage, bool no_cache) {
    // Compile via query pipeline (incremental + memoized)
    auto compile = compile_via_queries(path, coverage, no_cache);
    if (!compile.success) {
        if (output)
            *output = "compilation error:\n" + compile.error_message;
        return EXIT_COMPILATION_ERROR;
    }

    const auto& llvm_ir = compile.llvm_ir;
    auto module_name = fs::path(path).stem().string();

    // Use centralized run cache - NEVER create files inside packages
    fs::path cache_dir = get_run_cache_dir();

    // Calculate content hash for caching (hash the IR which reflects all source changes)
    std::string content_hash = generate_content_hash(llvm_ir);

    // Generate unique file names using cache key + thread ID for exe/output (to avoid race
    // conditions)
    std::string cache_key = generate_cache_key(path);
    std::string unique_name = module_name + "_" + cache_key;

    fs::path obj_output = cache_dir / (content_hash + get_object_extension());
    fs::path exe_output = cache_dir / unique_name;
    fs::path out_file = cache_dir / (unique_name + "_output.txt");
#ifdef _WIN32
    exe_output += ".exe";
#endif

    // Note: clang may be empty if LLVM backend and LLD are available (self-contained mode)
    std::string clang = find_clang();

    // Use global deps cache for precompiled runtimes
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

    // Check if we have a cached object file
    bool use_cached_obj = fs::exists(obj_output);

    if (!use_cached_obj) {
        // Compile LLVM IR string directly to object file (no .ll on disk)
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = false; // Always quiet for tests
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;

        auto obj_result = compile_ir_string_to_object(llvm_ir, obj_output, clang, obj_options);
        if (!obj_result.success) {
            if (output)
                *output = "compilation error: " + obj_result.error_message;
            return EXIT_COMPILATION_ERROR;
        }
    }

    // Collect all object files to link
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    // Add runtime object files (registry detects which runtimes are needed)
    {
        parser::Module empty_module;
        const auto& mod = compile.module ? *compile.module : empty_module;
        auto runtime_objects =
            get_runtime_objects(compile.registry, mod, deps_cache, clang, verbose);
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());
    }

    // Generate hash for executable caching (source + all object files)
    std::string exe_hash = generate_exe_hash(content_hash, object_files);
    fs::path cached_exe = cache_dir / (exe_hash + ".exe");

    // Check if we have a cached executable (skip if --no-cache)
    bool use_cached_exe = !no_cache && fs::exists(cached_exe);

    if (!use_cached_exe) {
        // Link all object files to create executable
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::Executable;
        link_options.verbose = false; // Always quiet for tests
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;

        // Add @link libraries from FFI decorators
        for (const auto& lib : compile.link_libs) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

#ifdef _WIN32
        // Add Windows system libraries for socket support
        if ((compile.module && has_socket_functions(*compile.module)) ||
            compile.registry->has_module("std::net") ||
            compile.registry->has_module("std::net::sys") ||
            compile.registry->has_module("std::net::tcp") ||
            compile.registry->has_module("std::net::udp")) {
            link_options.link_flags.push_back("-lws2_32");
        }
        // Add Windows system libraries for OS module (Registry, user info)
        if (compile.registry->has_module("std::os")) {
            link_options.link_flags.push_back("-ladvapi32");
            link_options.link_flags.push_back("-luserenv");
        }
        // Always link OpenSSL libraries (tml_runtime.lib contains crypto objects)
        {
            auto openssl = find_openssl();
            if (openssl.found) {
                link_options.link_flags.push_back(
                    to_forward_slashes((openssl.lib_dir / openssl.crypto_lib).string()));
                link_options.link_flags.push_back(
                    to_forward_slashes((openssl.lib_dir / openssl.ssl_lib).string()));
                link_options.link_flags.push_back("/DEFAULTLIB:crypt32");
                link_options.link_flags.push_back("/DEFAULTLIB:ws2_32");
            }
        }
        link_options.link_flags.push_back("/STACK:67108864");
#endif

        // Link to a unique temporary location (avoid race conditions)
        std::string temp_key = generate_cache_key(path);
        fs::path temp_exe = cache_dir / (exe_hash + "_" + temp_key + "_temp.exe");

        auto link_result = link_objects(object_files, temp_exe, clang, link_options);
        if (!link_result.success) {
            if (output)
                *output = "compilation error: " + link_result.error_message;
            return EXIT_COMPILATION_ERROR;
        }

        // Try to move to cached location (may fail if another thread already did it)
        try {
            if (!fs::exists(cached_exe)) {
                fs::rename(temp_exe, cached_exe);
            } else {
                // Another thread created it first, just remove our temp
                fs::remove(temp_exe);
            }
        } catch (...) {
            // Race condition - another thread created it, clean up our temp
            if (fs::exists(temp_exe)) {
                fs::remove(temp_exe);
            }
        }
    }

    // Ensure runtime DLLs are available next to the executable
    ensure_runtime_dlls(cache_dir);

    // Copy cached exe to final unique location (use hard link for speed in parallel tests)
    if (!fast_copy_file(cached_exe, exe_output)) {
        if (output)
            *output = "error: Failed to copy cached exe";
        return 1;
    }

    // Run with output redirected to a file
    // Use native path format for Windows compatibility
    std::string exe_native = exe_output.string();
    std::string out_native = out_file.string();
    std::string run_cmd;

#ifdef _WIN32
    // Windows: use cmd /c to properly handle redirection
    // Format: cmd /c "\"exe_path\" args > \"output_path\" 2>&1"
    run_cmd = "cmd /c \"\"" + exe_native + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }
    run_cmd += " > \"" + out_native + "\" 2>&1\"";
#else
    run_cmd = "\"" + exe_native + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }
    run_cmd += " > \"" + out_native + "\" 2>&1";
#endif

    int run_ret = std::system(run_cmd.c_str());

    // Read captured output
    if (output && fs::exists(out_file)) {
        std::ifstream ifs(out_file);
        *output =
            std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }

    // Clean up temporary files (keep .obj in cache for reuse)
    fs::remove(out_file);
    fs::remove(exe_output);

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

/// Extended run with additional options (profiling, etc.)
int run_run_ex(const std::string& path, const RunOptions& opts) {
    // For now, delegate to standard run with profiling setup
    // If profiling is enabled, we set up the profiler initialization

    if (opts.profile) {
        // Set global profiling flag - this will be used by codegen to inject profiler calls
        CompilerOptions::profile = true;
        CompilerOptions::profile_output = opts.profile_output;

        TML_LOG_INFO("build", "Runtime profiling enabled. Output: " << opts.profile_output);
        TML_LOG_INFO("build",
                     "Note: Automatic instrumentation requires recompilation with --profile flag.");
        TML_LOG_INFO("build", "For manual profiling, use std::profiler module in your code.");
    }

    std::string pipeline_dir;
    if (opts.emit_pipeline) {
        pipeline_dir = opts.pipeline_output_dir.empty()
                           ? (fs::path(path).parent_path() / ".." / ".sandbox" / "pipeline")
                                 .lexically_normal()
                                 .string()
                           : opts.pipeline_output_dir;
    }

    return run_run(path, opts.args, opts.verbose, opts.coverage, opts.no_cache, opts.backend,
                   pipeline_dir);
}

} // namespace tml::cli
