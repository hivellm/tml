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

#include "testing/testing_compile.hpp"

#include "cli/builder/builder_internal.hpp"
#include "cli/builder/compiler_setup.hpp"
#include "cli/builder/object_compiler.hpp"
#include "common.hpp"
#include "log/log.hpp"
#include "query/query_context.hpp"
#include "query/query_key.hpp"
#include "testing/testing_dispatcher_gen.hpp"
#include "types/module_binary.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <set>
#include <thread>

namespace fs = std::filesystem;

namespace tml::testing {

using Clock = std::chrono::high_resolution_clock;

// ============================================================================
// Global compile environment (initialized once)
// ============================================================================

static std::string g_clang_path;
static bool g_env_initialized = false;
static std::mutex g_env_mutex;

void init_compile_env() {
    std::lock_guard<std::mutex> lock(g_env_mutex);
    if (g_env_initialized)
        return;

    // Pre-load library module metadata
    int loaded = types::preload_all_meta_caches();
    TML_LOG_DEBUG("test", "[compile] Preloaded " << loaded << " library meta caches");

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

static std::string to_fwd_slashes(const std::string& s) {
    std::string r = s;
    std::replace(r.begin(), r.end(), '\\', '/');
    return r;
}

/// Get the cache directory for compiled test executables.
static fs::path get_test_exe_cache_dir() {
    auto build_dir = cli::build::get_build_dir(false);
    auto cache_dir = build_dir / ".new-run-cache";
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    return cache_dir;
}

/// Ensure required vcpkg DLLs are next to compiled test executables.
/// Without these, test exes that use zlib/brotli/zstd/sqlite crash at startup.
static void ensure_runtime_dlls(const fs::path& target_dir) {
#ifdef _WIN32
    static const std::vector<std::string> dll_names = {"zlib1.dll", "zstd.dll", "brotlicommon.dll",
                                                       "brotlidec.dll", "brotlienc.dll",
                                                       "sqlite3.dll",
                                                       // OpenSSL DLLs
                                                       "libcrypto-3-x64.dll", "libssl-3-x64.dll"};
    static const std::vector<fs::path> search_dirs = {
        "src/x64-windows/bin",
        "../src/x64-windows/bin",
        "F:/Node/hivellm/tml/src/x64-windows/bin",
        "vcpkg_installed/x64-windows/bin",
        "../vcpkg_installed/x64-windows/bin",
        "F:/Node/hivellm/tml/vcpkg_installed/x64-windows/bin",
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
    int hw = static_cast<int>(std::thread::hardware_concurrency());
    int num_compile_threads = std::max(1, std::min(4, hw / 2));
    num_compile_threads = std::min(num_compile_threads, static_cast<int>(suite.tests.size()));

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
            // Disable incremental cache for multi-file suites: each file is
            // compiled with a unique test_entry_index (tml_test_N), but incremental
            // cache entries were saved with index 0. Reusing them would produce
            // duplicate tml_test_0 symbols when multiple files are linked together.
            bool is_multi_file_suite = suite.tests.size() > 1;
            qopts.incremental = !config.no_cache && !config.coverage && !is_multi_file_suite;
            qopts.generate_exe_main = false;
            qopts.test_entry_index = static_cast<int>(i);

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

            auto codegen_result = qctx.codegen_unit(file_path, module_name);

            if (!codegen_result.success) {
                std::string err = codegen_result.error_message;
                if (err.empty()) {
                    auto tc_r = qctx.cache().lookup<query::TypecheckResult>(
                        query::TypecheckModuleKey{file_path, module_name});
                    if (tc_r && !tc_r->success && !tc_r->errors.empty())
                        err = tc_r->errors[0];
                    auto pr = qctx.cache().lookup<query::ParseModuleResult>(
                        query::ParseModuleKey{file_path, module_name});
                    if (pr && !pr->success && !pr->errors.empty())
                        err = pr->errors[0];
                }
                TML_LOG_ERROR("test", "  [compile] SKIP " << file_path << ": " << err);
                fr.error_message = err;
                continue;
            }

            // Save incremental cache
            if (qopts.incremental) {
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
                auto obj_result = cli::compile_ir_string_to_object(codegen_result.llvm_ir, obj_path,
                                                                   g_clang_path, obj_opts);
                if (!obj_result.success) {
                    TML_LOG_ERROR("test", "  [compile] SKIP " << file_path << ": "
                                                              << obj_result.error_message);
                    fr.error_message = obj_result.error_message;
                    continue;
                }
                fr.object_file = obj_result.object_file;
                fr.success = true;
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
        result.error_message = "Dispatcher compilation failed: " + disp_result.error_message;
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
    parser::Module empty_module;
    const auto& mod = parsed_module_holder ? *parsed_module_holder : empty_module;

    auto runtime_objs =
        cli::build::get_runtime_objects(registry, mod, deps_cache, g_clang_path, verbose);
    all_object_files.insert(all_object_files.end(), runtime_objs.begin(), runtime_objs.end());

    // Link to executable
    auto exe_path = cache_dir / (suite.name + ".exe");

    cli::LinkOptions link_opts;
    link_opts.output_type = cli::LinkOptions::OutputType::Executable;
    link_opts.verbose = verbose;
    link_opts.coverage = config.coverage;

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
    // Always link OpenSSL libraries (tml_runtime.lib contains crypto objects)
    {
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
            std::vector<std::string> search_paths = {"build/debug", "build/release",
                                                     "F:/Node/hivellm/tml/build/debug",
                                                     "F:/Node/hivellm/tml/build/release"};
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
    // Increase default stack size (debug codegen uses many allocas)
    link_opts.link_flags.push_back("/STACK:67108864");
#endif

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
    return result;
}

// ============================================================================
// Parallel compilation
// ============================================================================

std::vector<CompileResult> compile_suites_parallel(const std::vector<Suite>& suites,
                                                   const CompileConfig& config,
                                                   std::atomic<bool>& should_stop) {

    // Ensure environment is initialized before spawning threads
    init_compile_env();

    std::vector<CompileResult> results(suites.size());

    int num_threads = config.num_threads;
    if (num_threads <= 0) {
        int hw2 = static_cast<int>(std::thread::hardware_concurrency());
        num_threads = std::max(1, std::min(4, hw2 / 2));
    }
    num_threads = std::min(num_threads, static_cast<int>(suites.size()));

    std::atomic<int> next_suite{0};

    auto worker = [&]() {
        while (!should_stop.load(std::memory_order_relaxed)) {
            int idx = next_suite.fetch_add(1, std::memory_order_relaxed);
            if (idx >= static_cast<int>(suites.size()))
                break;

            TML_LOG_DEBUG("test", "[compile] Compiling suite: " << suites[idx].name << " ("
                                                                << suites[idx].tests.size()
                                                                << " tests)");

            results[idx] = compile_suite(suites[idx], config);

            if (!results[idx].success && config.no_cache) {
                // In fail-fast mode with no-cache, signal stop
                // (with cache, we want all compilations to proceed for caching)
            }
        }
    };

    if (num_threads <= 1) {
        worker();
    } else {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker);
        }
        for (auto& t : threads) {
            t.join();
        }
    }

    return results;
}

} // namespace tml::testing
