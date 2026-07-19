TML_MODULE("test")

//! # Testing - Parallel & Unified Binary Compilation
//!
//! Extracted from testing_compile.cpp.
//! - compile_suites_parallel(): parallel suite compilation with thread pool
//! - compile_unified_binary(): Zig-model unified binary compilation

#include "cli/builder/build_script.hpp"
#include "cli/builder/native_lib_resolver.hpp"
#include "testing/testing_compile_internal.hpp"

#include <cstdlib>

namespace tml::testing {

// ============================================================================
// Parallel compilation
// ============================================================================

std::vector<CompileResult> compile_suites_parallel(const std::vector<Suite>& suites,
                                                   const CompileConfig& config,
                                                   std::atomic<bool>& should_stop,
                                                   CompileCallback on_complete) {

    // Ensure environment is initialized before spawning threads
    init_compile_env();

    // Pre-build runtime archive once (all suites share the same base runtime objects)
    g_runtime_archive_path = build_runtime_archive(config);
    if (!g_runtime_archive_path.empty()) {
        TML_LOG_INFO("test", "All suites will link against cached runtime archive");
    }

    // Pre-build stdlib codegen state once, so each per-file compile can skip
    // emit_module_pure_tml_functions() (~9s of library scanning per file).
    //
    // ENABLED as of phase43a Option B. The bootstrap pass now emits ZERO function
    // bodies: it runs with `library_skip_user_code`, which keeps lazy deferral
    // active while skipping user code. Its output is the semantic index plus the
    // two pending-body maps (pending_library_methods_/funcs_), both captured into
    // CodegenLibraryState. Each worker restores them and calls the already-wired
    // emit_referenced_library_definitions(), which emits ONLY the bodies that
    // worker actually references — the same reference-driven set the normal
    // non-cached path emits, and it runs with in_library_body_ == true.
    //
    // This is what unblocked the fast path. Previously the bootstrap set
    // `library_ir_only`, which ALSO disabled the deferral guards, so it eagerly
    // compiled ~5000 library bodies that nothing had ever exercised. That surfaced
    // latent codegen defects in never-compiled code as an unfixable treadmill —
    // each patched body exposed the next (three runs, three different K001s in
    // three different unexercised bodies) — and it ran with in_library_body_ ==
    // false, enabling Phase 4b Str-temp auto-free inside library bodies (a
    // use-after-free hazard). Emitting nothing eliminates the entire class:
    // a body that is never referenced is never compiled.
    //
    // Also fixed under phase43a and required for this to work:
    //   - generic module free-function monomorphization (un-monomorphized `T`
    //     template symbols escaping from library bodies);
    //   - dedup keyspace: generated_impl_methods_ / generated_impl_methods_output_
    //     are captured and restored, not just generated_functions_, so a restored
    //     worker does not re-emit library-instantiated methods (I32::duplicate);
    //   - test_bootstrap.tml import paths (12 modules named by non-existent paths
    //     aborted the bootstrap typecheck, leaving g_stdlib_codegen_state null);
    //   - private-method visibility for non-generic, non-behavior impl blocks.
    //
    // TRAP (still true): state capture is independent of any object compilation,
    // so "fast path enabled" below only means the state exists.
    //
    // Skip for coverage: coverage instrumentation makes stdlib codegen very slow.
    //
    // AMORTIZATION HEURISTIC: the bootstrap costs ~30s per PROCESS and its state
    // cannot be persisted to disk (the pending-body maps hold AST pointers into
    // the in-process GlobalASTCache). A small targeted run (one short suite)
    // would pay 30s to save less than that, so only bootstrap when the run has
    // enough files to amortize. TML_STDLIB_FASTPATH=1 forces it on (used by the
    // zero-divergence gate); =0 forces it off.
    {
        int total_files_for_fastpath = 0;
        for (const auto& s : suites) {
            total_files_for_fastpath += static_cast<int>(s.tests.size());
        }
        const char* fp_env = std::getenv("TML_STDLIB_FASTPATH");
        bool force_on = fp_env && std::string(fp_env) == "1";
        bool force_off = fp_env && std::string(fp_env) == "0";
        bool want_fastpath =
            !config.coverage && !force_off && (force_on || total_files_for_fastpath >= 32);
        if (!g_stdlib_codegen_state && want_fastpath) {
            build_stdlib_object(config);
        } else if (!g_stdlib_codegen_state) {
            TML_LOG_INFO("test", "Stdlib fast-path skipped ("
                                     << total_files_for_fastpath
                                     << " files < 32; set TML_STDLIB_FASTPATH=1 to force)");
        }
    }
    if (g_stdlib_codegen_state) {
        TML_LOG_INFO("test", "All suites will use cached stdlib codegen state");
        // Clear obj_cache to prevent stale .obj files from runs without cached state.
        // The IR fingerprint can collide (truncated to 16 hex chars) between cached
        // and non-cached runs, returning .obj files missing tml_test_N entries.
        auto obj_cache = get_test_exe_cache_dir() / "obj_cache";
        if (fs::exists(obj_cache)) {
            std::error_code ec;
            fs::remove_all(obj_cache, ec);
            TML_LOG_DEBUG("test", "Cleared obj_cache to prevent stale hits");
        }
    }

    std::vector<CompileResult> results(suites.size());

    int num_threads = config.num_threads;
    if (num_threads <= 0) {
        // Use up to 8 threads for suite compilation (main bottleneck).
        // LLVM backend is the slowest step; more parallelism = faster overall.
        int hw2 = ThreadBudget::instance().max_threads();
        num_threads = std::max(1, std::min(8, hw2));
    }
    num_threads = std::min(num_threads, static_cast<int>(suites.size()));

    // phase41a: with aggregated suites a targeted run may have fewer suites
    // than the 8-thread budget. Hand the idle budget to per-file codegen
    // threads inside each suite so `tml test --suite=X` keeps its parallelism.
    // Global concurrency stays <= 8 (num_threads * intra <= 8), the same
    // envelope suite-level parallelism has always used. Full runs keep the
    // historical 8 workers x 1 intra thread.
    CompileConfig effective_config = config;
    effective_config.intra_suite_threads =
        std::max(1, std::min(4, 8 / std::max(1, num_threads)));

    std::atomic<int> next_suite{0};

    int total_suites = static_cast<int>(suites.size());
    std::atomic<int> completed{0};

    auto worker = [&]() {
        while (!should_stop.load(std::memory_order_relaxed)) {
            int idx = next_suite.fetch_add(1, std::memory_order_relaxed);
            if (idx >= static_cast<int>(suites.size()))
                break;
            // Re-check after fetch_add: another worker may have set should_stop
            // between our while-check and grabbing this index.
            if (should_stop.load(std::memory_order_relaxed))
                break;

            const auto& suite_name = suites[idx].name;
            TML_LOG_INFO("test", "[compile] Building " << suite_name << ".exe (" << (idx + 1) << "/"
                                                       << total_suites << ")");

            results[idx] = compile_suite_safe(suites[idx], effective_config);

            int done = completed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (results[idx].success) {
                auto ms = results[idx].compile_time_us / 1000;
                TML_LOG_INFO("test", "[compile] OK " << suite_name << ".exe " << ms << "ms ("
                                                     << done << "/" << total_suites << ")");
            } else {
                TML_LOG_ERROR("test", "[compile] FAIL "
                                          << suite_name << ".exe: " << results[idx].error_message
                                          << " (" << done << "/" << total_suites << ")");
            }

            if (on_complete) {
                on_complete(idx, results[idx]);
            }

            // Fail-fast on real compile errors. Link-stage failures of aggregated
            // suites are recoverable (the coordinator splits the suite per-file),
            // so they must not abort the whole run.
            bool recoverable_link_failure =
                results[idx].link_stage_failure && suites[idx].tests.size() > 1;
            if (config.fail_fast && ((!results[idx].success && !recoverable_link_failure) ||
                                     !results[idx].per_file_errors.empty())) {
                should_stop.store(true, std::memory_order_relaxed);
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

// ============================================================================
// Unified binary compilation (Zig model)
// ============================================================================

CompileResult compile_unified_binary(const std::vector<Suite>& suites, const CompileConfig& config,
                                     std::atomic<bool>& should_stop,
                                     std::vector<UnifiedTestMapping>& out_mapping) {
    CompileResult result;
    auto start = Clock::now();

    init_compile_env();

    auto cache_dir = get_test_exe_cache_dir();
    ensure_runtime_dlls(cache_dir);
    auto deps_dir = cli::build::get_deps_cache_dir();
    std::string deps_cache = to_fwd_slashes(deps_dir.string());
    bool verbose = config.verbose;

    // ---- Step 1: Build runtime archive (C runtime .obj files) ----
    g_runtime_archive_path = build_runtime_archive(config);

    // ---- Step 2: Build stdlib codegen state (for fast library state restoration) ----
    // NOTE: We don't use the stdlib .obj in mega-binary mode (library_decls_only=false).
    // Each test .obj gets full library defs with internal linkage.
    // But we still capture codegen state to skip emit_module_pure_tml_functions() per file.
    if (!g_stdlib_codegen_state) {
        g_stdlib_obj_path = build_stdlib_object(config);
    }
    if (g_stdlib_codegen_state) {
        TML_LOG_INFO("test", "[unified] Stdlib codegen state available (fast path enabled)");
    } else {
        TML_LOG_WARN("test", "[unified] No stdlib codegen state; per-file codegen will be slower");
    }

    // ---- Step 3: Flatten suites into global test list + build mapping ----
    out_mapping.clear();
    int global_index = 0;
    for (int si = 0; si < static_cast<int>(suites.size()); ++si) {
        for (int ti = 0; ti < static_cast<int>(suites[si].tests.size()); ++ti) {
            UnifiedTestMapping m;
            m.global_index = global_index;
            m.suite_index = si;
            m.test_index_in_suite = ti;
            m.test_name = suites[si].tests[ti].test_name;
            m.file_path = suites[si].tests[ti].file_path;
            out_mapping.push_back(m);
            ++global_index;
        }
    }
    int total_files = global_index;
    TML_LOG_INFO("test", "[unified] " << total_files << " test files across " << suites.size()
                                      << " suites");

    // ---- Step 4: Per-file codegen (parallel, using cached library state) ----
    struct FileResult {
        bool success = false;
        fs::path object_file;
        std::string error_message;
        std::string file_path;
        std::set<std::string> link_libs;
        std::shared_ptr<types::ModuleRegistry> registry;
    };

    std::vector<FileResult> file_results(total_files);
    std::atomic<int> next_file{0};
    std::atomic<int> compiled_count{0};
    std::atomic<int> error_count{0};
    // Use up to 4 parallel codegen threads (each gets its own QueryContext)
    int num_codegen_threads = std::max(1, std::min(4, ThreadBudget::instance().max_threads()));
    TML_LOG_INFO("test", "[unified] Using " << num_codegen_threads << " codegen threads");

    auto codegen_worker = [&]() {
        while (!should_stop.load(std::memory_order_relaxed)) {
            int i = next_file.fetch_add(1, std::memory_order_relaxed);
            if (i >= total_files)
                break;

            const auto& m = out_mapping[i];
            auto file_path = m.file_path;
            auto module_name = m.test_name;
            auto& fr = file_results[i];
            fr.file_path = file_path;

            // Setup QueryOptions with cached library state
            query::QueryOptions qopts;
            qopts.verbose = verbose;
            qopts.coverage = config.coverage;
            qopts.optimization_level = config.optimization_level;
            qopts.incremental = true;
            qopts.generate_exe_main = false;
            qopts.test_entry_index = i;
            qopts.cached_library_state = g_stdlib_codegen_state;
            // library_decls_only=false: each test .obj emits full internal-linkage
            // library definitions. This matches per-suite mode and avoids missing-symbol
            // errors from generic instantiations that the stdlib.obj may not contain.
            // The cached_library_state still provides the fast codegen path (skips
            // emit_module_pure_tml_functions) — just without the decl-only optimization.
            qopts.library_decls_only = false;

            auto source_dir = fs::path(file_path).parent_path();
            if (source_dir.empty())
                source_dir = fs::current_path();
            qopts.source_directory = source_dir.string();

            query::QueryContext qctx(qopts);
            if (qopts.incremental) {
                auto build_dir = cli::build::get_build_dir(false);
                qctx.load_incremental_cache(build_dir);
            }

            // Run codegen (no timeout thread — rely on incremental cache for speed)
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
                fr.error_message = err;
                error_count.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            // Save incremental cache
            if (qopts.incremental) {
                std::lock_guard<std::mutex> lock(g_incr_cache_mutex);
                auto build_dir = cli::build::get_build_dir(false);
                qctx.save_incremental_cache(build_dir);
            }

            // Compile IR to .obj (with object cache)
            auto obj_path =
                cache_dir / ("unified_test_" + std::to_string(i) + cli::get_object_extension());

            if (codegen_result.has_object_file()) {
                fr.object_file = codegen_result.object_file;
                fr.success = true;
            } else {
                auto obj_cache_dir = cache_dir / "obj_cache";
                auto ir_fp = query::fingerprint_bytes(codegen_result.llvm_ir.data(),
                                                      codegen_result.llvm_ir.size());
                // Use full 32-char hex hash to avoid collisions. CRC32C-based fingerprint
                // with 16-char truncation collides when cached library state makes the first
                // half of IR identical across test files in the same suite.
                auto cached_obj = obj_cache_dir / (ir_fp.to_hex() + cli::get_object_extension());
                bool used_cache = false;
                if (fs::exists(cached_obj)) {
                    std::error_code ec;
                    fs::copy_file(cached_obj, obj_path, fs::copy_options::overwrite_existing, ec);
                    if (!ec) {
                        fr.object_file = obj_path;
                        fr.success = true;
                        used_cache = true;
                    }
                }
                if (!used_cache) {
                    cli::ObjectCompileOptions obj_opts;
                    obj_opts.optimization_level = config.optimization_level;
                    obj_opts.verbose = verbose;
                    obj_opts.coverage = config.coverage;

                    auto obj_result = cli::compile_ir_string_to_object(
                        codegen_result.llvm_ir, obj_path, g_clang_path, obj_opts);
                    if (!obj_result.success) {
                        fr.error_message = obj_result.error_message;
                        error_count.fetch_add(1, std::memory_order_relaxed);
                        continue;
                    }
                    fr.object_file = obj_result.object_file;
                    fr.success = true;
                    std::error_code ec;
                    fs::create_directories(obj_cache_dir, ec);
                    fs::copy_file(obj_path, cached_obj, fs::copy_options::overwrite_existing, ec);
                }
            }

            // Collect link libraries
            fr.link_libs = codegen_result.link_libs;

            // Extract registry
            auto tc = qctx.cache().lookup<query::TypecheckResult>(
                query::TypecheckModuleKey{file_path, module_name});
            if (tc && tc->success && tc->registry) {
                fr.registry = tc->registry;
            }

            int cc = compiled_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (cc % 200 == 0 || cc == total_files) {
                TML_LOG_INFO("test", "[unified] Compiled " << cc << "/" << total_files << " files ("
                                                           << error_count.load() << " errors)");
            }
        }
    };

    // Launch parallel codegen workers
    {
        std::vector<std::thread> threads;
        threads.reserve(num_codegen_threads);
        for (int t = 0; t < num_codegen_threads; ++t) {
            threads.emplace_back(codegen_worker);
        }
        for (auto& t : threads) {
            t.join();
        }
    }

    // Collect results from parallel compilation
    std::vector<fs::path> all_object_files;
    std::set<std::string> all_link_libs;
    auto master_registry = std::make_shared<types::ModuleRegistry>();

    for (int i = 0; i < total_files; ++i) {
        auto& fr = file_results[i];
        if (fr.success) {
            all_object_files.push_back(fr.object_file);
            all_link_libs.insert(fr.link_libs.begin(), fr.link_libs.end());
            if (fr.registry) {
                for (const auto& [mod_path, mod_info] : fr.registry->get_all_modules()) {
                    if (!master_registry->has_module(mod_path)) {
                        master_registry->register_module(mod_path, mod_info);
                    }
                }
            }
        } else if (!fr.error_message.empty()) {
            result.per_file_errors.push_back({fr.file_path, fr.error_message});
        }
    }

    if (compiled_count == 0) {
        result.error_message = "All files failed to compile";
        auto end = Clock::now();
        result.compile_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        return result;
    }

    // ---- Step 5: Generate mega-dispatcher ----
    std::vector<DispatcherTestInfo> dispatcher_infos;
    // Build dispatcher info only for files that compiled successfully
    // We need to track which global indices succeeded
    std::set<int> compiled_indices;
    for (int i = 0; i < total_files; ++i) {
        // Check if this file had an error
        bool had_error = false;
        for (const auto& fe : result.per_file_errors) {
            if (fe.file_path == out_mapping[i].file_path) {
                had_error = true;
                break;
            }
        }
        if (!had_error) {
            compiled_indices.insert(i);
            DispatcherTestInfo info;
            info.name = out_mapping[i].test_name;
            info.file = out_mapping[i].file_path;
            info.index = i;
            // F-023: entry symbol is file-stable, matching codegen's tml_test_<id>.
            info.symbol_id = query::stable_test_symbol_id(out_mapping[i].file_path);
            dispatcher_infos.push_back(info);
        }
    }

    std::string dispatcher_ir = generate_ndjson_dispatcher_ir(dispatcher_infos, "tml_unified");
    auto dispatcher_obj_path = cache_dir / ("unified_dispatcher" + cli::get_object_extension());
    cli::ObjectCompileOptions disp_opts;
    disp_opts.optimization_level = 0;
    disp_opts.verbose = verbose;

    auto disp_result = cli::compile_ir_string_to_object(dispatcher_ir, dispatcher_obj_path,
                                                        g_clang_path, disp_opts);
    if (!disp_result.success) {
        result.error_message =
            "[X001] Unified dispatcher compilation failed: " + disp_result.error_message;
        auto end = Clock::now();
        result.compile_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        return result;
    }
    all_object_files.push_back(fs::path(disp_result.object_file));

    // ---- Step 6: Collect runtime objects ----
    // Scan all test files for imports to build master registry (fallback)
    if (master_registry->get_all_modules().empty()) {
        static const std::pair<std::string, std::string> import_module_map[] = {
            {"use std::zlib", "std::zlib"},
            {"use std::json", "std::json"},
            {"use std::search", "std::search"},
            {"use std::profiler", "std::profiler"},
            {"use std::crypto", "std::crypto"},
            {"use std::file", "std::file"},
            {"use std::glob", "std::glob"},
            {"use std::net", "std::net"},
            {"use std::os", "std::os"},
            {"use std::sqlite", "std::sqlite"},
            {"use std::net::tls", "std::net::tls"},
        };
        for (const auto& m : out_mapping) {
            try {
                std::ifstream file(m.file_path);
                if (!file.is_open())
                    continue;
                std::string line;
                int line_count = 0;
                while (std::getline(file, line) && line_count < 30) {
                    ++line_count;
                    for (const auto& [import_prefix, mod_name] : import_module_map) {
                        if (line.find(import_prefix) != std::string::npos &&
                            !master_registry->has_module(mod_name)) {
                            types::Module synth;
                            synth.name = mod_name;
                            master_registry->register_module(mod_name, synth);
                        }
                    }
                }
            } catch (...) {}
        }
    }

    parser::Module empty_module;
    auto runtime_objs = cli::build::get_runtime_objects(master_registry, empty_module, deps_cache,
                                                        g_clang_path, verbose);

    // Add runtime archive or individual objects
    if (!g_runtime_archive_path.empty()) {
        all_object_files.push_back(fs::path(g_runtime_archive_path));
        for (const auto& obj : runtime_objs) {
            if (obj.extension() == ".lib" || obj.extension() == ".a") {
                all_object_files.push_back(obj);
                continue;
            }
            auto stem = obj.stem().string();
            if (g_archive_obj_stems.count(stem))
                continue;
            all_object_files.push_back(obj);
        }
    } else {
        all_object_files.insert(all_object_files.end(), runtime_objs.begin(), runtime_objs.end());
    }

    // NOTE: stdlib .obj is NOT linked in unified mode (library_decls_only=false).
    // Each test .obj already contains internal-linkage library definitions.
    // The stdlib.obj would duplicate these and cause LLD multiple-definition errors
    // for external-linkage symbols.

    // ---- Step 7: Link everything into one .exe ----
    auto exe_path = cache_dir / "tml_unified.exe";
    {
        std::error_code ec;
        fs::remove(exe_path, ec);
    }

    cli::LinkOptions link_opts;
    link_opts.output_type = cli::LinkOptions::OutputType::Executable;
    link_opts.verbose = verbose;
    link_opts.coverage = config.coverage;

    for (const auto& lib : all_link_libs) {
        link_opts.link_flags.push_back(lib);
    }

    // Add libraries and search paths from build.tml for any imported packages
    for (const auto& suite : suites) {
        for (const auto& test : suite.tests) {
            auto bs_result = cli::detect_and_run_build_script(test.file_path, false);
            if (bs_result.success) {
                fs::path package_dir =
                    cli::detect_package_dir(fs::absolute(fs::path(test.file_path)));
                for (const auto& sp : bs_result.link_search_paths) {
                    fs::path abs_path = sp.is_absolute() ? sp : package_dir / sp;
                    link_opts.library_search_paths.push_back(abs_path.string());
                }
                for (const auto& lib : bs_result.link_libs) {
                    link_opts.link_flags.push_back(
                        (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos)
                            ? "\"" + lib + "\""
                            : "-l" + lib);
                }
            }
        }
    }

#ifdef _WIN32
    // Platform-specific linking (same as compile_suite but using master_registry)
    if (master_registry->has_module("std::net") || master_registry->has_module("std::net::sys") ||
        master_registry->has_module("std::net::tcp") ||
        master_registry->has_module("std::net::udp")) {
        link_opts.link_flags.push_back("-lws2_32");
    }
    if (master_registry->has_module("std::os")) {
        link_opts.link_flags.push_back("-ladvapi32");
        link_opts.link_flags.push_back("-luserenv");
    }
    {
        bool uses_crypto = cli::build::has_crypto_modules(master_registry);
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
    {
        bool uses_sqlite = false;
        for (const auto& [path, _] : master_registry->get_all_modules()) {
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
    {
        bool uses_search = false;
        for (const auto& [path, _] : master_registry->get_all_modules()) {
            if (path == "std::search" || path.find("std::search::") == 0) {
                uses_search = true;
                break;
            }
        }
        if (uses_search) {
            std::vector<std::string> search_paths = {"build/debug", "build/release"};
            for (const auto& sp : search_paths) {
                fs::path lib_path = fs::path(sp) / "tml_search_runtime.lib";
                if (fs::exists(lib_path)) {
                    all_object_files.push_back(fs::absolute(lib_path));
                    auto lib_subdir = fs::path(sp) / "lib" / "tml_search.lib";
                    if (fs::exists(lib_subdir)) {
                        all_object_files.push_back(fs::absolute(lib_subdir));
                    }
                    break;
                }
            }
        }
    }
    {
        bool uses_zlib = false;
        for (const auto& [path, _] : master_registry->get_all_modules()) {
            if (path == "std::zlib" || path.find("std::zlib::") == 0) {
                uses_zlib = true;
                break;
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
    // Larger stack for unified binary (all tests share one stack)
    link_opts.link_flags.push_back("/STACK:134217728");
#endif

    // Use NativeLibResolver to copy ALL runtime DLLs to test cache
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

    TML_LOG_INFO("test", "[unified] Linking " << all_object_files.size() << " objects...");
    auto link_result = cli::link_objects(all_object_files, exe_path, g_clang_path, link_opts);

    auto end = Clock::now();
    result.compile_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (!link_result.success) {
        result.error_message = "Unified binary linking failed: " + link_result.error_message;
        return result;
    }

    result.success = true;
    result.exe_path = link_result.output_file.string();
    result.compiled_test_count = compiled_count;
    TML_LOG_INFO("test", "[unified] Built " << result.exe_path << " with " << compiled_count
                                            << " tests in " << (result.compile_time_us / 1000000)
                                            << "s");

    return result;
}

} // namespace tml::testing
