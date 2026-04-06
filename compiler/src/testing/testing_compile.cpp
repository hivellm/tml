TML_MODULE("test")

//! # Independent Compilation Pipeline
//!
//! Compiles test suites to executables using QueryContext::codegen_unit(),
//! with zero dependency on the old test system. Part of the v3 independent test system.
//!
//! Pipeline:
//!   1. preload_all_meta_caches() (once)
//!   2. find_clang() (once)
//!   3. QueryContext(opts) → codegen_unit(file) → IR string
//!   4. generate_ndjson_dispatcher_ir() → dispatcher IR
//!   5. compile_ir_string_to_object() for both IRs
//!   6. get_runtime_objects() for CRT + runtime
//!   7. link_objects() → .exe

#include "cli/builder/build_script.hpp"
#include "cli/builder/builder_internal.hpp"
#include "cli/builder/compiler_setup.hpp"
#include "cli/builder/native_lib_resolver.hpp"
#include "cli/builder/object_compiler.hpp"
#include "codegen/llvm/llvm_ir_gen.hpp"
#include "common.hpp"
#include "log/log.hpp"
#include "query/query_context.hpp"
#include "query/query_fingerprint.hpp"
#include "query/query_key.hpp"
#include "testing/testing_compile_internal.hpp"
#include "testing/testing_dispatcher_gen.hpp"
#include "types/module_binary.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <llvm/Support/ErrorHandling.h>
#include <mutex>
#include <set>
#include <thread>

namespace fs = std::filesystem;

namespace tml::testing {

using Clock = std::chrono::high_resolution_clock;

// ThreadBudget class defined in testing_compile_internal.hpp

// ============================================================================
// Global compile environment (initialized once)
// ============================================================================

std::string g_clang_path;
bool g_env_initialized = false;
std::mutex g_env_mutex;
std::mutex g_incr_cache_mutex;

/// Pre-built runtime archive path (set by compile_suites_parallel before spawning workers).
/// When non-empty, compile_suite() uses this instead of calling get_runtime_objects() per suite.
std::string g_runtime_archive_path;

/// Set of object file stems that are included in the pre-built runtime archive.
/// Used to identify which objects from get_runtime_objects() are already in the archive
/// vs. conditional objects (file.obj, glob.obj, etc.) that must be linked separately.
std::set<std::string> g_archive_obj_stems;

/// Pre-compiled stdlib object path. When non-empty, compile_suite() passes the
/// cached library state to QueryContext so AST codegen skips emit_module_pure_tml_functions().
std::string g_stdlib_obj_path;

/// Captured library codegen state from the stdlib pre-compilation pass.
/// Shared across all compile_suite() workers to skip per-suite stdlib IR generation.
std::shared_ptr<const codegen::CodegenLibraryState> g_stdlib_codegen_state;

// LlvmFatalException defined in testing_compile_internal.hpp

[[maybe_unused]] static void llvm_fatal_handler(void* /*user_data*/, const char* reason,
                                                bool /*gen_crash_diag*/) {
    throw LlvmFatalException(reason ? reason : "unknown LLVM fatal error");
}

void init_compile_env() {
    std::lock_guard<std::mutex> lock(g_env_mutex);
    if (g_env_initialized)
        return;

    // Pre-load library module metadata
    int loaded = types::preload_all_meta_caches();
    TML_LOG_DEBUG("test", "[compile] Preloaded " << loaded << " library meta caches");

    // NOTE: LLVM fatal error handler disabled temporarily for debugging
    // llvm::install_fatal_error_handler(llvm_fatal_handler, nullptr);
    // llvm::install_bad_alloc_error_handler(llvm_fatal_handler, nullptr);

    // Discover clang
    g_clang_path = cli::find_clang();
    if (g_clang_path.empty()) {
        TML_LOG_WARN("test", "[compile] Could not find clang; compilation may fail");
    }

    g_env_initialized = true;
}

// ============================================================================
// Path utilities
// ============================================================================

std::string to_fwd_slashes(const std::string& s) {
    std::string r = s;
    std::replace(r.begin(), r.end(), '\\', '/');
    return r;
}

/// Get the cache directory for compiled test executables.
fs::path get_test_exe_cache_dir() {
    auto build_dir = cli::build::get_build_dir(false);
    auto cache_dir = build_dir / "cache" / "tests";
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    return cache_dir;
}

/// Ensure required vcpkg DLLs are next to compiled test executables.
/// Without these, test exes that use zlib/brotli/zstd/sqlite crash at startup.
void ensure_runtime_dlls(const fs::path& target_dir) {
#ifdef _WIN32
    static const std::vector<std::string> dll_names = {"zlib1.dll", "zstd.dll", "brotlicommon.dll",
                                                       "brotlidec.dll", "brotlienc.dll",
                                                       "sqlite3.dll",
                                                       // OpenSSL DLLs
                                                       "libcrypto-3-x64.dll", "libssl-3-x64.dll"};
    static const std::vector<fs::path> search_dirs = {
        "src/x64-windows/bin",
        "../src/x64-windows/bin",
        "vcpkg_installed/x64-windows/bin",
        "../vcpkg_installed/x64-windows/bin",
    };
    static bool done = false;
    if (done)
        return;
    done = true;
    for (const auto& dll_name : dll_names) {
        auto target = target_dir / dll_name;
        if (fs::exists(target))
            continue;
        for (const auto& dir : search_dirs) {
            auto src = dir / dll_name;
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

// ============================================================================
// Runtime archive (pre-built static lib of all runtime .obj files)
// ============================================================================

/// Build a static archive containing all runtime .obj files.
/// Called once before parallel suite compilation to avoid redundant per-suite linking.
std::string build_runtime_archive(const CompileConfig& config) {
    init_compile_env();

    // Determine cache directory (same as suite cache dir) — use absolute paths
    fs::path cache_dir =
        fs::absolute(fs::path("build") / (config.optimization_level > 0 ? "release" : "debug") /
                     "cache" / "tests");
    fs::create_directories(cache_dir);

    fs::path archive_path = cache_dir / "tml_test_runtime.lib";
    fs::path deps_dir = cache_dir / "deps";

    // Fast path: check if archive is newer than all C source files — skip
    // calling get_runtime_objects() (which compiles 17 C files) entirely.
    if (fs::exists(archive_path)) {
        auto archive_time = fs::last_write_time(archive_path);
        bool archive_fresh = true;

        // Find runtime source directory via find_runtime() which returns essential.c
        std::string essential_path = cli::find_runtime();
        if (!essential_path.empty()) {
            fs::path runtime_dir = fs::path(essential_path).parent_path().parent_path();
            std::error_code ec;
            for (auto it = fs::recursive_directory_iterator(runtime_dir, ec);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (!ec && it->path().extension() == ".c") {
                    if (fs::last_write_time(it->path(), ec) > archive_time) {
                        archive_fresh = false;
                        break;
                    }
                }
            }
        }

        if (archive_fresh) {
            // Populate archive obj stems from the deps directory (cached .obj files)
            g_archive_obj_stems.clear();
            std::error_code ec2;
            for (const auto& entry : fs::directory_iterator(deps_dir, ec2)) {
                auto ext = entry.path().extension().string();
                if (ext == ".obj" || ext == ".o") {
                    g_archive_obj_stems.insert(entry.path().stem().string());
                }
            }
            TML_LOG_INFO("test", "Using cached runtime archive: " << archive_path.string() << " ("
                                                                  << g_archive_obj_stems.size()
                                                                  << " objects)");
            return archive_path.string();
        }
    }

    // Archive missing or stale: compile runtime objects and rebuild.
    auto registry = std::make_shared<types::ModuleRegistry>();
    parser::Module empty_module;
    std::string deps_cache = to_fwd_slashes(deps_dir.string());

    // Pre-register optional runtime modules so the test runtime archive contains
    // ALL conditional C runtimes. The archive is shared across every test EXE,
    // so we cannot know per-test which modules will be imported. Without this,
    // tests using std::glob, std::os::subprocess, etc. fail to link with
    // undefined symbols like glob_pattern_matches, tml_process_spawn.
    {
        registry->register_module("std::glob", types::Module{});
        registry->register_module("std::os", types::Module{});
        registry->register_module("std::os::subprocess", types::Module{});
    }

    auto runtime_objs = cli::build::get_runtime_objects(registry, empty_module, deps_cache,
                                                        g_clang_path, config.verbose);

    if (runtime_objs.empty()) {
        TML_LOG_ERROR("test", "No runtime objects found — cannot build archive");
        return "";
    }

    // Build static archive using llvm-ar directly
    // (LLDLinker's find_lld() may not find llvm-ar if lld-link is embedded)
    std::string ar_path;
    {
        // Search for llvm-ar in common locations
        std::vector<std::string> ar_search = {
            "F:/LLVM/bin/llvm-ar.exe",
            "C:/Program Files/LLVM/bin/llvm-ar.exe",
            "C:/LLVM/bin/llvm-ar.exe",
        };
        // Also check PATH
        for (const auto& p : ar_search) {
            if (fs::exists(p)) {
                ar_path = p;
                break;
            }
        }
        if (ar_path.empty()) {
            // Try `where` on Windows
            auto pipe = _popen("where llvm-ar.exe 2>nul", "r");
            if (pipe) {
                char buf[512];
                if (fgets(buf, sizeof(buf), pipe)) {
                    ar_path = buf;
                    // Trim trailing newline
                    while (!ar_path.empty() && (ar_path.back() == '\n' || ar_path.back() == '\r'))
                        ar_path.pop_back();
                }
                _pclose(pipe);
            }
        }
    }

    if (ar_path.empty()) {
        TML_LOG_ERROR("test", "llvm-ar not found — cannot build runtime archive");
        return "";
    }

    // llvm-ar rcs <output> <obj1> <obj2> ...
    // Use native paths (backslashes on Windows) for std::system() via cmd.exe
    std::ostringstream cmd;
    cmd << "\"" << ar_path << "\" rcs " << fs::absolute(archive_path).string();
    for (const auto& obj : runtime_objs) {
        cmd << " " << fs::absolute(obj).string();
    }

    if (config.verbose) {
        TML_LOG_DEBUG("test", "Archive command: " << cmd.str());
    }

    int ret = std::system(cmd.str().c_str());
    if (ret != 0) {
        TML_LOG_ERROR("test", "Failed to build runtime archive (exit " << ret << ")");
        return "";
    }

    // Record which object stems went into the archive so compile_suite() can
    // identify conditional objects that need separate linking.
    g_archive_obj_stems.clear();
    for (const auto& obj : runtime_objs) {
        if (obj.extension() == ".obj" || obj.extension() == ".o") {
            g_archive_obj_stems.insert(obj.stem().string());
        }
    }

    TML_LOG_INFO("test", "Built runtime archive: " << archive_path.string() << " ("
                                                   << runtime_objs.size() << " objects)");
    return archive_path.string();
}

// ============================================================================
// Stdlib pre-compiled object cache
// ============================================================================

/// Build a pre-compiled stdlib object containing all TML library function
/// definitions. This runs the AST codegen path with library_ir_only=true
/// on a minimal test file, then compiles the resulting IR to a .obj file.
/// The captured CodegenLibraryState is stored globally so compile_suite()
/// workers can skip emit_module_pure_tml_functions() entirely.
///
/// Returns the path to the cached .obj file, or empty string on failure.
std::string build_stdlib_object(const CompileConfig& config) {
    init_compile_env();

    fs::path cache_dir =
        fs::absolute(fs::path("build") / (config.optimization_level > 0 ? "release" : "debug") /
                     "cache" / "tests");
    fs::create_directories(cache_dir);

    // Cache key includes coverage mode (coverage instruments functions differently)
    std::string suffix = config.coverage ? "_cov" : "";
    fs::path obj_path = cache_dir / ("stdlib_precompiled" + suffix + ".obj");

    // Check if cached .obj is up-to-date: newer than all lib/ .meta files and compiler binary
    bool cache_valid = false;
    if (fs::exists(obj_path)) {
        auto obj_time = fs::last_write_time(obj_path);
        cache_valid = true;

        // Check all library .meta files
        for (const auto& lib_dir : {"lib/core/src", "lib/std/src", "lib/test/src"}) {
            if (!fs::exists(lib_dir))
                continue;
            for (const auto& entry : fs::recursive_directory_iterator(lib_dir)) {
                if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().string();
                    if (ext == ".meta" || ext == ".tml") {
                        if (entry.last_write_time() > obj_time) {
                            cache_valid = false;
                            break;
                        }
                    }
                }
            }
            if (!cache_valid)
                break;
        }

        // Check compiler binary
        if (cache_valid) {
            auto exe_path = fs::path("build") /
                            (config.optimization_level > 0 ? "release" : "debug") / "bin" /
                            "tml.exe";
            if (fs::exists(exe_path) && fs::last_write_time(exe_path) > obj_time) {
                cache_valid = false;
            }
        }
    }

    if (cache_valid) {
        // We still need the CodegenLibraryState even with a cached .obj.
        // We have to regenerate it (can't serialize/deserialize easily).
        // But this is fast compared to compiling to .obj.
        TML_LOG_INFO("test", "Stdlib .obj is up-to-date, regenerating codegen state...");
    } else {
        TML_LOG_INFO("test", "Building pre-compiled stdlib object...");
    }

    auto start = Clock::now();

    // Create a minimal test file that imports nothing specific.
    // The codegen path will pick up all registered library modules via
    // emit_module_pure_tml_functions() when library_ir_only=false.
    // We need a real test file to trigger the AST codegen path.
    // Use the first available test file's parse/typecheck environment.

    // Set up QueryContext to do a library-only codegen pass.
    // We compile a minimal synthetic module — the important thing is that
    // the TypeEnv has all library modules registered.
    query::QueryOptions qopts;
    qopts.verbose = config.verbose;
    qopts.coverage = config.coverage;
    qopts.optimization_level = config.optimization_level;
    qopts.generate_exe_main = false;
    qopts.incremental = false; // Don't pollute incremental cache

    // Find bootstrap file for stdlib pre-compilation.
    // Prefer comprehensive bootstrap that imports ALL library modules.
    std::string bootstrap_file;
    // 1. Check for comprehensive bootstrap file (imports all modules)
    for (const auto& path :
         {"compiler/runtime/test_bootstrap.tml", "../compiler/runtime/test_bootstrap.tml"}) {
        if (fs::exists(path)) {
            bootstrap_file = path;
            break;
        }
    }
    // 2. Fallback to any test file
    if (bootstrap_file.empty()) {
        for (const auto& dir : {"lib/std/tests/http", "lib/std/tests/json", "lib/core/tests/str",
                                "lib/core/tests/option"}) {
            if (!fs::exists(dir))
                continue;
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.path().extension() == ".tml") {
                    bootstrap_file = entry.path().string();
                    break;
                }
            }
            if (!bootstrap_file.empty())
                break;
        }
    }

    if (bootstrap_file.empty()) {
        TML_LOG_ERROR("test", "No test file found to bootstrap stdlib codegen");
        return "";
    }

    auto source_dir = fs::path(bootstrap_file).parent_path();
    qopts.source_directory = source_dir.string();

    query::QueryContext qctx(qopts);

    // Parse and typecheck the bootstrap file to populate library module registry
    std::string module_name = fs::path(bootstrap_file).stem().string();
    auto tc = qctx.typecheck_module(bootstrap_file, module_name);
    if (!tc.success) {
        TML_LOG_ERROR("test", "Failed to typecheck bootstrap file for stdlib codegen: "
                                  << (tc.errors.empty() ? "unknown" : tc.errors[0]));
        return "";
    }

    auto parsed = qctx.parse_module(bootstrap_file, module_name);
    if (!parsed.success || !parsed.module) {
        TML_LOG_ERROR("test", "Failed to parse bootstrap file for stdlib codegen");
        return "";
    }

    // Generate library IR using the AST codegen path with library_ir_only=true.
    codegen::LLVMGenOptions lib_opts;
    lib_opts.coverage_enabled = config.coverage;
    lib_opts.library_ir_only = true;
    // Use lazy mode to avoid the hang that occurs when emitting all 5000+ functions
    // from 287 stdlib modules at once (lazy_library_defs=false hung for minutes).
    // With library_decls_only=false in unified mode, the stdlib.obj is no longer
    // linked for symbol resolution — each test .obj has its own internal-linkage
    // library definitions. The primary purpose of build_stdlib_object() is now to
    // capture CodegenLibraryState (via capture_library_state()) for fast per-file
    // codegen. Lazy mode produces the complete set of function signatures needed for
    // state capture without hanging on full-body emission.
    lib_opts.lazy_library_defs = true;
    lib_opts.emit_comments = config.verbose;
#ifdef _WIN32
    lib_opts.target_triple = "x86_64-pc-windows-msvc";
#else
    lib_opts.target_triple = "x86_64-unknown-linux-gnu";
#endif

    codegen::LLVMIRGen lib_gen(*tc.env, lib_opts);
    auto lib_result = lib_gen.generate(*parsed.module);

    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(lib_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(lib_result);
        TML_LOG_ERROR(
            "test", "Stdlib codegen failed: " << (errors.empty() ? "unknown" : errors[0].message));
        return "";
    }

    std::string lib_ir = std::get<std::string>(lib_result);

    // Capture the library state for workers to use
    // Extract preamble headers (everything before first "define" or "declare" from library funcs)
    std::string preamble;
    auto first_define = lib_ir.find("\ndefine ");
    if (first_define != std::string::npos) {
        preamble = lib_ir.substr(0, first_define);
    }
    g_stdlib_codegen_state = lib_gen.capture_library_state(lib_ir, preamble);

    auto state_time = Clock::now();
    auto state_us =
        std::chrono::duration_cast<std::chrono::microseconds>(state_time - start).count();
    TML_LOG_INFO("test", "Stdlib codegen state captured in " << (state_us / 1000) << "ms"
                                                             << " (IR: " << lib_ir.size() / 1024
                                                             << "KB)");

    if (cache_valid) {
        // .obj is up-to-date, we only needed the codegen state
        return obj_path.string();
    }

    // Compile the library IR to a .obj file
    cli::ObjectCompileOptions obj_opts;
    obj_opts.optimization_level = config.optimization_level;
    obj_opts.verbose = config.verbose;
    obj_opts.coverage = config.coverage;

    auto obj_result = cli::compile_ir_string_to_object(lib_ir, obj_path, g_clang_path, obj_opts);

    if (!obj_result.success) {
        TML_LOG_ERROR("test",
                      "Failed to compile stdlib IR to object: " << obj_result.error_message);
        return "";
    }

    auto end = Clock::now();
    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    TML_LOG_INFO("test", "Pre-compiled stdlib object built in " << (total_us / 1000)
                                                                << "ms: " << obj_path.string());

    return obj_path.string();
}

// ============================================================================
// Build script cache helpers
// ============================================================================

/// Returns cached BuildScriptResult if valid (cache newer than build.tml), or empty on miss.
static cli::BuildScriptResult load_build_script_cache(const fs::path& pkg_dir) {
    cli::BuildScriptResult result;
    result.success = false;

    auto cache_file =
        fs::path("build/debug/cache/build-scripts") / pkg_dir.filename() / "test_link_cache.txt";
    auto build_script = pkg_dir / "build.tml";

    if (!fs::exists(cache_file) || !fs::exists(build_script))
        return result;

    // Check if cache is newer than build.tml
    auto cache_time = fs::last_write_time(cache_file);
    auto script_time = fs::last_write_time(build_script);
    if (script_time > cache_time)
        return result; // stale cache

    // Read cached directives
    std::ifstream ifs(cache_file);
    if (!ifs.is_open())
        return result;

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.find("link-lib:") == 0) {
            result.link_libs.insert(line.substr(9));
        } else if (line.find("link-search:") == 0) {
            result.link_search_paths.push_back(fs::path(line.substr(12)));
        } else if (line.find("copy-artifact:") == 0) {
            result.copy_artifacts.push_back(fs::path(line.substr(14)));
        }
    }
    result.success = true;
    return result;
}

/// Saves build.tml results to a per-package cache file.
static void save_build_script_cache(const fs::path& pkg_dir, const cli::BuildScriptResult& bsr) {
    auto cache_dir = fs::path("build/debug/cache/build-scripts") / pkg_dir.filename();
    std::error_code ec;
    fs::create_directories(cache_dir, ec);

    auto cache_file = cache_dir / "test_link_cache.txt";
    std::ofstream ofs(cache_file);
    if (!ofs.is_open())
        return;

    for (const auto& lib : bsr.link_libs) {
        ofs << "link-lib:" << lib << "\n";
    }
    for (const auto& sp : bsr.link_search_paths) {
        ofs << "link-search:" << sp.string() << "\n";
    }
    for (const auto& art : bsr.copy_artifacts) {
        ofs << "copy-artifact:" << art.string() << "\n";
    }
}

// ============================================================================
// Single suite compilation
// ============================================================================

CompileResult compile_suite(const Suite& suite, const CompileConfig& config) {
    CompileResult result;
    auto start = Clock::now();

    if (suite.tests.empty()) {
        result.success = false;
        result.error_message = "Suite has no tests";
        return result;
    }

    // Ensure environment is initialized
    init_compile_env();

    auto cache_dir = get_test_exe_cache_dir();
    ensure_runtime_dlls(cache_dir);
    auto deps_dir = cli::build::get_deps_cache_dir();
    std::string deps_cache = to_fwd_slashes(deps_dir.string());
    bool verbose = config.verbose;

    // Parallel per-file compilation within the suite
    // Each file gets its own QueryContext (thread-safe by design)
    struct FileCompileResult {
        bool success = false;
        fs::path object_file;
        std::string error_message;
        std::string file_path;
        std::set<std::string> link_libs;
        std::shared_ptr<types::ModuleRegistry> registry;
        std::shared_ptr<parser::Module> parsed_module;
    };

    std::vector<FileCompileResult> file_results(suite.tests.size());
    std::atomic<int> next_file{0};
    // Force single-threaded per-file compilation to avoid LLVM global state corruption.
    // Each file gets its own QueryContext/LLVMContext, but shared LLVM globals
    // (target registry, pass managers) are not thread-safe in all configurations.
    int num_compile_threads = 1;

    auto file_worker = [&]() {
        while (true) {
            int i = next_file.fetch_add(1, std::memory_order_relaxed);
            if (i >= static_cast<int>(suite.tests.size()))
                break;

            const auto& test = suite.tests[i];
            auto file_path = test.file_path;
            auto module_name = test.test_name;
            auto& fr = file_results[i];
            fr.file_path = file_path;

            // Setup QueryOptions
            query::QueryOptions qopts;
            qopts.verbose = verbose;
            qopts.coverage = config.coverage;
            qopts.optimization_level = config.optimization_level;
            qopts.generate_exe_main = false;
            qopts.test_entry_index = static_cast<int>(i);

            // Use cached library state to skip emit_module_pure_tml_functions().
            // Disable incremental cache when using cached state to avoid stale hits.
            if (g_stdlib_codegen_state) {
                qopts.cached_library_state = g_stdlib_codegen_state;
                qopts.incremental = false;
            } else {
                qopts.incremental = true;
            }

            auto source_dir = fs::path(file_path).parent_path();
            if (source_dir.empty())
                source_dir = fs::current_path();
            qopts.source_directory = source_dir.string();

            // Create QueryContext and compile
            query::QueryContext qctx(qopts);

            if (qopts.incremental) {
                auto build_dir = cli::build::get_build_dir(false);
                qctx.load_incremental_cache(build_dir);
            }

            // Run codegen with a timeout to avoid hangs from LLVM global state
            // corruption after compiling many files in the same process.
            constexpr int CODEGEN_TIMEOUT_SECONDS = 30;
            std::atomic<bool> codegen_done{false};
            query::CodegenUnitResult codegen_result;

            std::thread codegen_thread([&]() {
                codegen_result = qctx.codegen_unit(file_path, module_name);
                codegen_done.store(true, std::memory_order_release);
            });

            bool timed_out = false;
            {
                const auto deadline = std::chrono::steady_clock::now() +
                                      std::chrono::seconds(CODEGEN_TIMEOUT_SECONDS);
                while (!codegen_done.load(std::memory_order_acquire)) {
                    if (std::chrono::steady_clock::now() >= deadline) {
                        codegen_thread.detach();
                        timed_out = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (!timed_out && codegen_thread.joinable())
                    codegen_thread.join();
            }

            if (timed_out) {
                fr.error_message = "[X002] Codegen timed out after " +
                                   std::to_string(CODEGEN_TIMEOUT_SECONDS) + "s";
                continue;
            }

            if (!codegen_result.success) {
                // Collect all diagnostic errors (with file:line info from query_core)
                std::vector<std::string> all_errors;
                if (!codegen_result.error_message.empty()) {
                    all_errors.push_back(codegen_result.error_message);
                } else {
                    auto tc_r = qctx.cache().lookup<query::TypecheckResult>(
                        query::TypecheckModuleKey{file_path, module_name});
                    if (tc_r && !tc_r->success) {
                        all_errors = tc_r->errors;
                    }
                    if (all_errors.empty()) {
                        auto pr = qctx.cache().lookup<query::ParseModuleResult>(
                            query::ParseModuleKey{file_path, module_name});
                        if (pr && !pr->success) {
                            all_errors = pr->errors;
                        }
                    }
                }
                std::string first_err = all_errors.empty() ? "unknown error" : all_errors[0];
                TML_LOG_ERROR("test", "  [compile] SKIP " << file_path << ": " << first_err);
                for (size_t ei = 1; ei < all_errors.size(); ++ei) {
                    // Indent subsequent errors/notes under the SKIP line
                    TML_LOG_ERROR("test", "    " << all_errors[ei]);
                }
                fr.error_message = first_err;
                continue;
            }

            // Save incremental cache (mutex protects concurrent suite workers
            // from corrupting the shared incr.bin file)
            if (qopts.incremental) {
                std::lock_guard<std::mutex> lock(g_incr_cache_mutex);
                auto build_dir = cli::build::get_build_dir(false);
                qctx.save_incremental_cache(build_dir);
            }

            // Compile IR string to object
            auto obj_path = cache_dir / (suite.name + "_test_" + std::to_string(i) +
                                         cli::get_object_extension());

            cli::ObjectCompileOptions obj_opts;
            obj_opts.optimization_level = config.optimization_level;
            obj_opts.verbose = verbose;
            obj_opts.coverage = config.coverage;

            if (codegen_result.has_object_file()) {
                fr.object_file = codegen_result.object_file;
                fr.success = true;
            } else {
                // Object cache: hash IR → check for cached .obj → skip LLVM backend
                auto obj_cache_dir = cache_dir / "obj_cache";
                auto ir_fp = query::fingerprint_bytes(codegen_result.llvm_ir.data(),
                                                      codegen_result.llvm_ir.size());
                // Use full 32-char hex hash to avoid collisions. CRC32C-based fingerprint
                // with 16-char truncation collides when cached library state makes the first
                // half of IR identical across test files in the same suite.
                auto cached_obj = obj_cache_dir / (ir_fp.to_hex() + cli::get_object_extension());
                bool used_cache = false;
                bool obj_exists = fs::exists(cached_obj);
                if (obj_exists) {
                    std::error_code ec;
                    fs::copy_file(cached_obj, obj_path, fs::copy_options::overwrite_existing, ec);
                    if (!ec) {
                        fr.object_file = obj_path.string();
                        fr.success = true;
                        used_cache = true;
                    }
                }
                TML_LOG_DEBUG("test", "  [obj-cache] " << suite.tests[i].test_name << " exists="
                                                       << obj_exists << " hit=" << used_cache);
                if (!used_cache) {
                    auto obj_result = cli::compile_ir_string_to_object(
                        codegen_result.llvm_ir, obj_path, g_clang_path, obj_opts);
                    if (!obj_result.success) {
                        TML_LOG_ERROR("test", "  [compile] SKIP " << file_path << ": "
                                                                  << obj_result.error_message);
                        fr.error_message = obj_result.error_message;
                        continue;
                    }
                    fr.object_file = obj_result.object_file;
                    fr.success = true;
                    // Save to obj cache
                    std::error_code ec;
                    fs::create_directories(obj_cache_dir, ec);
                    fs::copy_file(obj_path, cached_obj, fs::copy_options::overwrite_existing, ec);
                }
            }

            // Collect link libraries
            fr.link_libs.insert(codegen_result.link_libs.begin(), codegen_result.link_libs.end());

            // Extract registry and parsed module
            auto tc = qctx.cache().lookup<query::TypecheckResult>(
                query::TypecheckModuleKey{file_path, module_name});
            if (tc && tc->success && tc->registry) {
                fr.registry = tc->registry;
            }
            auto parsed = qctx.cache().lookup<query::ParseModuleResult>(
                query::ParseModuleKey{file_path, module_name});
            if (parsed && parsed->success && parsed->module) {
                fr.parsed_module = parsed->module;
            }
        }
    };

    // Launch parallel file compilation
    if (num_compile_threads <= 1) {
        file_worker();
    } else {
        std::vector<std::thread> threads;
        threads.reserve(num_compile_threads);
        for (int t = 0; t < num_compile_threads; ++t) {
            threads.emplace_back(file_worker);
        }
        for (auto& t : threads) {
            t.join();
        }
    }

    // Merge results from parallel compilation
    std::vector<fs::path> all_object_files;
    std::set<std::string> all_link_libs;
    std::shared_ptr<types::ModuleRegistry> registry;
    std::shared_ptr<parser::Module> parsed_module_holder;
    std::set<size_t> compiled_indices;

    for (size_t i = 0; i < file_results.size(); ++i) {
        auto& fr = file_results[i];
        if (fr.success) {
            all_object_files.push_back(fr.object_file);
            all_link_libs.insert(fr.link_libs.begin(), fr.link_libs.end());
            compiled_indices.insert(i);
            if (fr.registry) {
                if (!registry) {
                    registry = fr.registry;
                } else {
                    for (const auto& [mod_path, mod_info] : fr.registry->get_all_modules()) {
                        if (!registry->has_module(mod_path)) {
                            registry->register_module(mod_path, mod_info);
                        }
                    }
                }
            }
            if (fr.parsed_module) {
                parsed_module_holder = fr.parsed_module;
            }
        } else {
            result.per_file_errors.push_back({fr.file_path, fr.error_message});
        }
    }

    // If no files compiled at all, report the first error
    if (compiled_indices.empty()) {
        result.success = false;
        result.error_message = result.per_file_errors.empty() ? "All files failed to compile"
                                                              : result.per_file_errors[0].error;
        auto end = Clock::now();
        result.compile_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        return result;
    }

    // Generate NDJSON dispatcher IR (only for successfully compiled files)
    std::vector<DispatcherTestInfo> dispatcher_infos;
    dispatcher_infos.reserve(compiled_indices.size());
    for (size_t i : compiled_indices) {
        dispatcher_infos.push_back(
            {suite.tests[i].test_name, suite.tests[i].file_path, static_cast<int>(i)});
    }
    std::string dispatcher_ir = generate_ndjson_dispatcher_ir(dispatcher_infos, suite.name);

    // Compile dispatcher IR to object
    auto dispatcher_obj_path =
        cache_dir / (suite.name + "_dispatcher" + cli::get_object_extension());
    cli::ObjectCompileOptions disp_obj_opts;
    disp_obj_opts.optimization_level = 0; // dispatcher doesn't need optimization
    disp_obj_opts.verbose = verbose;

    auto disp_result = cli::compile_ir_string_to_object(dispatcher_ir, dispatcher_obj_path,
                                                        g_clang_path, disp_obj_opts);

    if (!disp_result.success) {
        result.success = false;
        result.error_message = "[X001] Dispatcher compilation failed: " + disp_result.error_message;
        auto end = Clock::now();
        result.compile_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        return result;
    }
    all_object_files.push_back(disp_result.object_file);

    // Get runtime objects
    if (!registry) {
        registry = std::make_shared<types::ModuleRegistry>();
    }

    // When the registry is empty (e.g., due to incremental cache hits where typecheck
    // results aren't re-executed), we can't determine which conditional runtimes are needed.
    // In this case, register synthetic modules based on the suite's test file paths so that
    // get_runtime_objects() includes the correct conditional objects (file.obj, glob.obj, etc.).
    if (registry->get_all_modules().empty()) {
        // Map test directory patterns to the module they require
        static const std::pair<std::string, std::string> path_module_map[] = {
            {"std/tests/file/", "std::file"},
            {"std/tests/glob/", "std::glob"},
            {"std/tests/json/", "std::json"},
            {"std/tests/crypto/", "std::crypto"},
            {"std/tests/zlib/", "std::zlib"},
            {"std/tests/http/", "std::zlib"}, // HTTP encoding/compression depends on zlib
            {"std/tests/search/", "std::search"},
            {"std/tests/profiler/", "std::profiler"},
            {"std/tests/io/", "std::json"}, // std::io imports std::json indirectly
        };
        for (const auto& test : suite.tests) {
            auto fwd = to_fwd_slashes(test.file_path);
            for (const auto& [pattern, mod_name] : path_module_map) {
                if (fwd.find(pattern) != std::string::npos && !registry->has_module(mod_name)) {
                    types::Module synth;
                    synth.name = mod_name;
                    registry->register_module(mod_name, synth);
                }
            }
        }

        // Also scan test file contents for `use std::*` imports to handle
        // tests in non-standard locations (e.g., compiler/tests/) that import
        // library modules requiring conditional runtime linking.
        static const std::pair<std::string, std::string> import_module_map[] = {
            {"use std::zlib", "std::zlib"},     {"use std::json", "std::json"},
            {"use std::search", "std::search"}, {"use std::profiler", "std::profiler"},
            {"use std::crypto", "std::crypto"}, {"use std::file", "std::file"},
            {"use std::glob", "std::glob"},     {"use std::net::tls", "std::net::tls"},
        };
        for (const auto& test : suite.tests) {
            try {
                std::ifstream file(test.file_path);
                if (!file.is_open())
                    continue;
                std::string line;
                int line_count = 0;
                while (std::getline(file, line) && line_count < 30) {
                    ++line_count;
                    for (const auto& [import_prefix, mod_name] : import_module_map) {
                        if (line.find(import_prefix) != std::string::npos &&
                            !registry->has_module(mod_name)) {
                            types::Module synth;
                            synth.name = mod_name;
                            registry->register_module(mod_name, synth);
                        }
                    }
                }
            } catch (...) {}
        }
    }

    parser::Module empty_module;
    const auto& mod = parsed_module_holder ? *parsed_module_holder : empty_module;

    auto runtime_objs =
        cli::build::get_runtime_objects(registry, mod, deps_cache, g_clang_path, verbose);

    if (!g_runtime_archive_path.empty()) {
        // Use pre-built archive for base runtime .obj files,
        // but still include conditional .lib/.obj files not in the base archive.
        // The archive was built with an empty registry (no std::file, std::json, etc.),
        // so conditional runtime objects (file.obj, glob.obj, crypto, etc.) must be
        // added separately when the test suite imports those modules.
        all_object_files.push_back(fs::path(g_runtime_archive_path));
        for (const auto& obj : runtime_objs) {
            // Always add .lib/.a files (pre-built libraries like json, zlib)
            if (obj.extension() == ".lib" || obj.extension() == ".a") {
                all_object_files.push_back(obj);
                continue;
            }
            // Skip .obj files that are already in the base archive
            auto stem = obj.stem().string();
            if (g_archive_obj_stems.count(stem)) {
                continue;
            }
            // This is a conditional .obj not in the archive — include it
            all_object_files.push_back(obj);
        }
    } else {
        all_object_files.insert(all_object_files.end(), runtime_objs.begin(), runtime_objs.end());
    }

    // No stdlib .obj linked — each test .obj has full library defs (internal linkage).

    // Link to executable — remove stale exe to avoid "permission denied"
    auto exe_path = cache_dir / (suite.name + ".exe");
    std::error_code ec;
    fs::remove(exe_path, ec);

    cli::LinkOptions link_opts;
    link_opts.output_type = cli::LinkOptions::OutputType::Executable;
    link_opts.verbose = verbose;
    link_opts.coverage = config.coverage;
    // Force subprocess LLD: in-process lldMain deadlocks after compiling
    // many suites in a single process due to accumulated LLVM global state.
    link_opts.force_subprocess_lld = true;

    // Add link libraries from @link() attributes
    for (const auto& lib : all_link_libs) {
        link_opts.link_flags.push_back(lib);
    }

    // Platform-specific library linking (mirrors build.cpp logic)
    // The registry tells us which modules are imported so we can link
    // the correct vcpkg libraries and Windows system libraries.
    // Debug: log registry modules for troubleshooting link failures
    if (verbose && registry) {
        TML_LOG_DEBUG("test", "[compile] Registry has " << registry->list_modules().size()
                                                        << " modules for " << suite.name);
    }
#ifdef _WIN32
    // Windows system libraries for socket support
    if (registry->has_module("std::net") || registry->has_module("std::net::sys") ||
        registry->has_module("std::net::tcp") || registry->has_module("std::net::udp")) {
        link_opts.link_flags.push_back("-lws2_32");
    }
    // Windows system libraries for OS module (Registry, user info)
    if (registry->has_module("std::os")) {
        link_opts.link_flags.push_back("-ladvapi32");
        link_opts.link_flags.push_back("-luserenv");
    }
    // Only link OpenSSL when the suite uses crypto modules (including std::hash)
    {
        bool uses_crypto = cli::build::has_crypto_modules(registry);
        if (uses_crypto) {
            auto openssl = cli::build::find_openssl();
            if (openssl.found) {
                link_opts.link_flags.push_back(
                    to_fwd_slashes((openssl.lib_dir / openssl.crypto_lib).string()));
                link_opts.link_flags.push_back(
                    to_fwd_slashes((openssl.lib_dir / openssl.ssl_lib).string()));
                link_opts.link_flags.push_back("/DEFAULTLIB:crypt32");
                link_opts.link_flags.push_back("/DEFAULTLIB:ws2_32");
            }
        }
    }
    // Link sqlite3 library when sqlite modules are used
    {
        bool uses_sqlite = false;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path == "std::sqlite" || path.find("std::sqlite::") == 0) {
                uses_sqlite = true;
                break;
            }
        }
        if (uses_sqlite) {
            auto sqlite = cli::build::find_sqlite3();
            if (sqlite.found) {
                link_opts.link_flags.push_back(to_fwd_slashes(sqlite.lib_path.string()));
            }
        }
    }
    // Link search runtime when search modules are used
    {
        bool uses_search = false;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path == "std::search" || path.find("std::search::") == 0) {
                uses_search = true;
                break;
            }
        }
        if (uses_search) {
            // Find tml_search_runtime.lib
            std::vector<std::string> search_paths = {"build/debug", "build/release"};
            for (const auto& sp : search_paths) {
                fs::path lib_path = fs::path(sp) / "tml_search_runtime.lib";
                if (fs::exists(lib_path)) {
                    all_object_files.push_back(fs::absolute(lib_path));
                    TML_LOG_DEBUG("test",
                                  "[compile] Including search runtime: " << lib_path.string());
                    // Also find tml_search.lib (dependency of search_runtime)
                    // Check lib/ subdirectory first (CMake outputs there)
                    auto lib_subdir = fs::path(sp) / "lib" / "tml_search.lib";
                    if (fs::exists(lib_subdir)) {
                        all_object_files.push_back(fs::absolute(lib_subdir));
                        TML_LOG_DEBUG("test",
                                      "[compile] Including search core: " << lib_subdir.string());
                    }
                    break;
                }
            }
        }
    }
    // Link zlib/brotli/zstd libraries when zlib modules are used
    {
        bool uses_zlib = false;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path == "std::zlib" || path.find("std::zlib::") == 0 ||
                path == "std::http::encoding" || path == "std::http::compression") {
                uses_zlib = true;
                TML_LOG_DEBUG("test", "  zlib detected via module: " << path);
                break;
            }
        }
        if (!uses_zlib) {
            // Also check source file imports for indirect zlib usage
            for (const auto& [path, _] : registry->get_all_modules()) {
                if (path.find("std::http") == 0) {
                    // Any HTTP module may transitively depend on zlib
                    // via encoding.tml -> zlib imports
                    uses_zlib = true;
                    TML_LOG_DEBUG("test", "  zlib inferred from http module: " << path);
                    break;
                }
            }
        }
        if (uses_zlib) {
            auto vcpkg_lib = fs::path("vcpkg_installed/x64-windows/lib");
            if (fs::exists(vcpkg_lib / "zlib.lib")) {
                link_opts.link_flags.push_back(
                    to_fwd_slashes(fs::absolute(vcpkg_lib / "zlib.lib").string()));
                link_opts.link_flags.push_back(
                    to_fwd_slashes(fs::absolute(vcpkg_lib / "zstd.lib").string()));
                link_opts.link_flags.push_back(
                    to_fwd_slashes(fs::absolute(vcpkg_lib / "brotlidec.lib").string()));
                link_opts.link_flags.push_back(
                    to_fwd_slashes(fs::absolute(vcpkg_lib / "brotlienc.lib").string()));
                link_opts.link_flags.push_back(
                    to_fwd_slashes(fs::absolute(vcpkg_lib / "brotlicommon.lib").string()));
            }
            // Link tml_zlib_runtime (C wrappers for zlib/brotli/zstd streams)
            auto zlib_base =
                fs::path("build") / (config.optimization_level > 0 ? "release" : "debug");
            for (const auto& zlib_lib_name : {"libtml_zlib_runtime.a", "lib/libtml_zlib_runtime.a",
                                              "tml_zlib_runtime.lib", "lib/tml_zlib_runtime.lib"}) {
                auto zlib_lib_path = zlib_base / zlib_lib_name;
                if (fs::exists(zlib_lib_path)) {
                    link_opts.link_flags.push_back(
                        to_fwd_slashes(fs::absolute(zlib_lib_path).string()));
                    break;
                }
            }
        }
    }
    // Link json runtime when json modules are used
    {
        bool uses_json = false;
        for (const auto& [path, _] : registry->get_all_modules()) {
            if (path == "std::json" || path.find("std::json::") == 0) {
                uses_json = true;
                break;
            }
        }
        if (uses_json) {
            // json runtime .obj files are compiled by get_runtime_objects(),
            // but we also need to ensure vcpkg json lib is linked if available
            auto vcpkg_lib = fs::path("vcpkg_installed/x64-windows/lib");
            // No additional vcpkg lib needed — json uses C runtime only
        }
    }
    // Increase default stack size (debug codegen uses many allocas)
    link_opts.link_flags.push_back("/STACK:67108864");
#endif

    // Run build.tml scripts for external packages (e.g., postgresql)
    // Scan source files for "use <pkg>::" imports to detect non-standard packages.
    // This approach works on both the first run and subsequent cached runs because it
    // reads from the source files directly, bypassing the registry (which may be null
    // when the incremental cache GREEN path is taken).
    {
        std::set<std::string> packages_checked;
        for (const auto& test : suite.tests) {
            std::ifstream ifs(test.file_path);
            if (!ifs.is_open())
                continue;
            std::string line;
            while (std::getline(ifs, line)) {
                // Skip empty lines and comments
                if (line.empty() || line[0] == '/' || line[0] == '#')
                    continue;
                // Look for "use <pkg>::" pattern
                auto pos = line.find("use ");
                if (pos == std::string::npos)
                    continue;
                std::string rest = line.substr(pos + 4);
                // Trim leading whitespace
                while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) {
                    rest = rest.substr(1);
                }
                // Extract package name (before first "::")
                auto sep = rest.find("::");
                if (sep == std::string::npos)
                    continue;
                std::string pkg_name = rest.substr(0, sep);
                // Skip standard packages
                if (pkg_name == "core" || pkg_name == "std" || pkg_name == "test" ||
                    pkg_name == "compiler") {
                    continue;
                }
                if (packages_checked.count(pkg_name))
                    continue;
                packages_checked.insert(pkg_name);

                // Check for build.tml in lib/<package>/
                fs::path pkg_dir = fs::path("lib") / pkg_name;
                fs::path build_script = pkg_dir / "build.tml";
                if (!fs::exists(build_script))
                    continue;

                // Try cached result first
                auto cached = load_build_script_cache(pkg_dir);
                cli::BuildScriptResult bsr;
                bsr.success = false;
                if (cached.success) {
                    bsr = cached;
                    TML_LOG_INFO("test",
                                 "[compile] Using cached build.tml for package: " << pkg_name);
                } else {
                    TML_LOG_INFO("test", "[compile] Running build.tml for package: " << pkg_name);
                    bsr = cli::detect_and_run_build_script((pkg_dir / "src" / "mod.tml").string(),
                                                           verbose);
                    if (bsr.success) {
                        save_build_script_cache(pkg_dir, bsr);
                    }
                }

                if (bsr.success) {
                    // Add link libraries
                    for (const auto& lib : bsr.link_libs) {
                        std::string flag = "-l" + lib;
                        link_opts.link_flags.push_back(flag);
                        TML_LOG_DEBUG("test", "[compile] build.tml link lib: " << flag);
                    }
                    // Add search paths via library_search_paths so link_objects
                    // converts them to the correct platform format (/LIBPATH: on Windows)
                    for (const auto& sp : bsr.link_search_paths) {
                        fs::path abs_path = fs::absolute(pkg_dir / sp);
                        link_opts.library_search_paths.push_back(abs_path.string());
                        TML_LOG_DEBUG("test",
                                      "[compile] build.tml search path: " << abs_path.string());
                    }
                    // Copy artifacts (e.g., DLLs) to the test cache directory
                    for (const auto& artifact : bsr.copy_artifacts) {
                        fs::path src = pkg_dir / artifact;
                        if (fs::exists(src)) {
                            fs::path dest = cache_dir / artifact.filename();
                            std::error_code copy_ec;
                            fs::copy_file(src, dest, fs::copy_options::overwrite_existing, copy_ec);
                            if (!copy_ec) {
                                TML_LOG_DEBUG("test",
                                              "[compile] Copied artifact: " << dest.string());
                            }
                        }
                    }
                }
            }
        }
    }

    // Use NativeLibResolver to copy ALL runtime DLLs (including transitive deps) to test cache
    {
        auto nlr_platform = cli::Platform::detect();
        cli::NativeLibResolver nlr(nlr_platform);
        nlr.set_project_root(fs::current_path());
        for (const auto& lib : all_link_libs) {
            auto resolved = nlr.resolve(lib);
            if (resolved.tier != cli::NativeLibTier::NotFound) {
                cli::NativeLibResolver::copy_runtime_libs({resolved}, cache_dir);
            }
        }
    }

    auto link_result = cli::link_objects(all_object_files, exe_path, g_clang_path, link_opts);

    auto end = Clock::now();
    result.compile_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (!link_result.success) {
        result.success = false;
        result.error_message = "Linking failed: " + link_result.error_message;
        return result;
    }

    result.success = true;
    result.exe_path = link_result.output_file.string();

    // Cleanup staging .obj files — obj_cache/ has the persistent copies
    std::error_code rm_ec;
    for (size_t i : compiled_indices) {
        auto obj =
            cache_dir / (suite.name + "_test_" + std::to_string(i) + cli::get_object_extension());
        fs::remove(obj, rm_ec);
    }
    fs::remove(dispatcher_obj_path, rm_ec);

    return result;
}

// ============================================================================
// Crash-safe compilation wrapper
// ============================================================================

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <eh.h>
#include <windows.h>

/// SEH exception translated to C++ exception for crash isolation.
struct SehException : std::runtime_error {
    DWORD code;
    SehException(DWORD c) : std::runtime_error("SEH exception"), code(c) {}
};

static void __cdecl seh_translator(unsigned int code, struct _EXCEPTION_POINTERS*) {
    throw SehException(static_cast<DWORD>(code));
}

/// Wraps compile_suite to catch crashes (access violations, stack overflows, etc.)
CompileResult compile_suite_safe(const Suite& suite, const CompileConfig& config) {
    // Install per-thread SEH translator
    _se_translator_function prev = _set_se_translator((_se_translator_function)seh_translator);
    CompileResult result;
    try {
        result = compile_suite(suite, config);
    } catch (const SehException& e) {
        result.success = false;
        char buf[64];
        snprintf(buf, sizeof(buf), "Suite compilation crashed (SEH 0x%08lX)", e.code);
        result.error_message = buf;
        TML_LOG_ERROR("test", "  [compile] CRASH " << suite.name << ": " << result.error_message);
    } catch (const LlvmFatalException& e) {
        result.success = false;
        result.error_message = std::string("LLVM fatal: ") + e.what();
        TML_LOG_ERROR("test", "  [compile] CRASH " << suite.name << ": " << result.error_message);
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Suite compilation threw: ") + e.what();
        TML_LOG_ERROR("test", "  [compile] CRASH " << suite.name << ": " << result.error_message);
    }
    _set_se_translator(prev);
    return result;
}
#else
CompileResult compile_suite_safe(const Suite& suite, const CompileConfig& config) {
    CompileResult result;
    try {
        result = compile_suite(suite, config);
    } catch (const LlvmFatalException& e) {
        result.success = false;
        result.error_message = std::string("LLVM fatal: ") + e.what();
        TML_LOG_ERROR("test", "  [compile] CRASH " << suite.name << ": " << result.error_message);
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Suite compilation threw: ") + e.what();
        TML_LOG_ERROR("test", "  [compile] CRASH " << suite.name << ": " << result.error_message);
    }
    return result;
}
#endif

} // namespace tml::testing
