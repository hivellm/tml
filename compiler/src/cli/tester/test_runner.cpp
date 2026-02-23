TML_MODULE("test")

//\! # Test Runner — Suite Compilation
//\!
//\! Suite-based test compilation: groups tests into DLLs, parallel lex/parse/typecheck/codegen,
//\! parallel object compilation, runtime linking, and caching.
//\!
//\! ## Split Structure
//\!
//\! The test runner is split across multiple files:
//\! - `test_runner.cpp` — This file (suite compilation)
//\! - `test_runner_single.cpp` — Single test compilation, fuzz, profiled variants
//\! - `test_runner_exec.cpp` — DynamicLibrary, OutputCapture, crash handlers, execution

#include "test_runner_internal.hpp"

namespace tml::cli {

// ============================================================================
// Suite-Based Test Compilation
// ============================================================================

// Helper to extract suite key from file path
// Returns: "compiler_tests_compiler", "compiler_tests_runtime", "lib_core_tests", etc.
static std::string extract_suite_key(const std::string& file_path) {
    fs::path path(file_path);
    std::vector<std::string> parts;

    for (auto it = path.begin(); it != path.end(); ++it) {
        parts.push_back(it->string());
    }

    // Find the project root marker ("tml" directory or start of relative path)
    size_t start_idx = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i] == "tml") {
            start_idx = i + 1;
            break;
        }
    }

    // Build suite key from path components
    // For "compiler/tests/compiler/foo.test.tml" -> "compiler_tests_compiler"
    // For "lib/core/tests/bar.test.tml" -> "lib_core_tests"
    std::string key;
    for (size_t i = start_idx; i < parts.size() - 1; ++i) { // -1 to exclude filename
        if (!key.empty()) {
            key += "_";
        }
        key += parts[i];

        // Stop after "tests" directory or after 3 components
        if (parts[i] == "tests" || i - start_idx >= 2) {
            break;
        }
    }

    return key.empty() ? "default" : key;
}

// Helper to extract display group from suite key
static std::string suite_key_to_group(const std::string& key) {
    // "compiler_tests_compiler" -> "compiler/compiler"
    // "compiler_tests_runtime" -> "compiler/runtime"
    // "lib_core_tests" -> "lib/core"

    if (key.find("compiler_tests_") == 0) {
        std::string subdir = key.substr(15); // After "compiler_tests_"
        return "compiler/" + subdir;
    }
    if (key.find("lib_") == 0) {
        // "lib_core_tests" -> "lib/core"
        auto pos = key.find("_tests");
        if (pos != std::string::npos) {
            std::string lib_part = key.substr(4, pos - 4); // "core" from "lib_core_tests"
            return "lib/" + lib_part;
        }
    }
    return key;
}

std::vector<TestSuite> group_tests_into_suites(const std::vector<std::string>& test_files) {
    // Maximum tests per suite - balance between fewer DLLs and parallel compilation
    // Lower = more suites that compile faster in parallel
    // Higher = fewer DLLs but sequential within each suite
    // CRITICAL: Lowered from 15 → 8 to prevent O(n²) codegen slowdown
    // Cannot go lower than 8 without breaking atomic function dependencies
    // TODO: Fix codegen context accumulation bug (lowlevel_misc: 2.2s alone vs 98s in suite)
    constexpr size_t MAX_TESTS_PER_SUITE = 8;

    // Group files by suite key
    std::map<std::string, std::vector<std::string>> groups;
    for (const auto& file : test_files) {
        std::string key = extract_suite_key(file);
        groups[key].push_back(file);
    }

    // Convert to TestSuite structures, splitting large groups into chunks
    std::vector<TestSuite> suites;
    for (auto& [key, files] : groups) {
        // Sort files for deterministic ordering
        std::sort(files.begin(), files.end());

        // Split into chunks of MAX_TESTS_PER_SUITE
        size_t chunk_count = (files.size() + MAX_TESTS_PER_SUITE - 1) / MAX_TESTS_PER_SUITE;

        for (size_t chunk = 0; chunk < chunk_count; ++chunk) {
            TestSuite suite;
            if (chunk_count > 1) {
                suite.name = key + "_" + std::to_string(chunk + 1);
            } else {
                suite.name = key;
            }
            suite.group = suite_key_to_group(key);

            size_t start_idx = chunk * MAX_TESTS_PER_SUITE;
            size_t end_idx = std::min(start_idx + MAX_TESTS_PER_SUITE, files.size());

            for (size_t i = start_idx; i < end_idx; ++i) {
                SuiteTestInfo info;
                info.file_path = files[i];
                info.test_name = fs::path(files[i]).stem().string();
                // Entry function will be: tml_test_0, tml_test_1, etc. (within this chunk)
                info.entry_func_name = "tml_test_" + std::to_string(i - start_idx);
                info.test_count = tester::count_tests_in_file(files[i]);
                suite.tests.push_back(std::move(info));
            }

            suites.push_back(std::move(suite));
        }
    }

    // Sort suites by name for consistent ordering
    std::sort(suites.begin(), suites.end(),
              [](const TestSuite& a, const TestSuite& b) { return a.name < b.name; });

    return suites;
}

// ============================================================================
// Precompiled Symbols Cache - DISABLED
// ============================================================================
// DISABLED: Causes linkage conflicts (see line 2305 for details)
// This code is kept for reference but commented out to avoid unused function warnings.
//
// Compiles lib/std/precompiled_symbols.tml once and caches the object file.
// This object contains common generic instantiations (Mutex[I32], Arc[I32], etc.)
// and is linked into all test suites to avoid regenerating the same LLVM IR.

#if 0  // DISABLED - see comment above
static std::string get_precompiled_symbols_obj(bool verbose, bool no_cache) {
    fs::path cache_dir = get_run_cache_dir();
    fs::path precompiled_obj = cache_dir / ("precompiled_symbols" + get_object_extension());

    // If cached and not forcing rebuild, return existing
    if (!no_cache && fs::exists(precompiled_obj)) {
        return precompiled_obj.string();
    }

    TML_LOG_INFO("test", "[PRECOMPILE] Compiling precompiled_symbols.tml...");

    // Find the source file
    fs::path source = "lib/std/precompiled_symbols.tml";
    if (!fs::exists(source)) {
        // Precompiled symbols are optional - if not found, just skip
        return "";
    }

    try {
        // Read and preprocess source
        std::string source_code = read_file(source.string());
        auto pp_config = preprocessor::Preprocessor::host_config();
        preprocessor::Preprocessor pp(pp_config);
        auto pp_result = pp.process(source_code, source.string());

        if (!pp_result.success()) {
            TML_LOG_WARN("test", "[PRECOMPILE] Preprocessor errors in precompiled_symbols.tml");
            return "";
        }

        std::string preprocessed = pp_result.output;

        // Lex
        auto lex_source = lexer::Source::from_string(preprocessed, source.string());
        lexer::Lexer lex(lex_source);
        auto tokens = lex.tokenize();
        if (lex.has_errors()) {
            TML_LOG_WARN("test", "[PRECOMPILE] Lexer errors in precompiled_symbols.tml");
            return "";
        }

        // Parse
        parser::Parser parser(std::move(tokens));
        auto parse_result = parser.parse_module("precompiled_symbols");
        if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
            TML_LOG_WARN("test", "[PRECOMPILE] Parser errors in precompiled_symbols.tml");
            return "";
        }
        const auto& module = std::get<parser::Module>(parse_result);

        // Type check
        types::TypeChecker checker;
        auto check_result = checker.check_module(module);
        if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
            TML_LOG_WARN("test", "[PRECOMPILE] Type errors in precompiled_symbols.tml");
            return "";
        }
        const auto& env = std::get<types::TypeEnv>(check_result);

        // Borrow check (Polonius or NLL)
        std::variant<bool, std::vector<borrow::BorrowError>> borrow_result;
        if (CompilerOptions::polonius) {
            borrow::polonius::PoloniusChecker polonius_checker(env);
            borrow_result = polonius_checker.check_module(module);
        } else {
            borrow::BorrowChecker borrow_checker(env);
            borrow_result = borrow_checker.check_module(module);
        }
        if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
            TML_LOG_WARN("test", "[PRECOMPILE] Borrow check errors in precompiled_symbols.tml");
            return "";
        }

        // Codegen
        codegen::LLVMGenOptions options;
        options.emit_comments = false;
        options.generate_dll_entry = false; // No test entry
        options.dll_export = false;
        options.force_internal_linkage = false; // Allow linking
        options.emit_debug_info = false;
        codegen::LLVMIRGen llvm_gen(env, options);

        auto gen_result = llvm_gen.generate(module);
        if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
            TML_LOG_WARN("test", "[PRECOMPILE] Codegen errors in precompiled_symbols.tml");
            return "";
        }

        const auto& llvm_ir = std::get<std::string>(gen_result);

        // Compile LLVM IR string directly to object (no .ll on disk)
        std::string clang = find_clang();
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = false; // No debug info for precompiled symbols
        obj_options.verbose = false;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;
        obj_options.coverage = false; // No coverage for precompiled symbols

        auto obj_result = compile_ir_string_to_object(llvm_ir, precompiled_obj, clang, obj_options);
        if (!obj_result.success) {
            TML_LOG_WARN("test", "[PRECOMPILE] Object compilation failed: "
                      << obj_result.error_message);
            return "";
        }

        TML_LOG_INFO("test", "[PRECOMPILE] Successfully compiled precompiled_symbols.obj");
        return precompiled_obj.string();

    } catch (const std::exception& e) {
        TML_LOG_WARN("test", "[PRECOMPILE] Exception during compilation: " << e.what());
        return "";
    }
}
#endif // DISABLED precompiled symbols

SuiteCompileResult compile_test_suite(const TestSuite& suite, bool verbose, bool no_cache,
                                      const std::string& backend,
                                      const std::vector<std::string>& features) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    // Phase timing tracking
    int64_t preprocess_time_us = 0;
    int64_t phase1_time_us = 0;
    int64_t phase2_time_us = 0;
    int64_t runtime_time_us = 0;
    int64_t link_time_us = 0;

    SuiteCompileResult result;

    try {

        if (suite.tests.empty()) {
            result.success = true;
            return result;
        }

        fs::path cache_dir = get_run_cache_dir();
        // Note: clang may be empty if LLVM backend is available (self-contained mode)
        std::string clang = find_clang();

        // Create a SHARED ModuleRegistry for all tests in this suite
        // This prevents re-parsing the same library modules for each test file
        auto shared_registry = std::make_shared<types::ModuleRegistry>();

        // ======================================================================
        // EARLY CACHE CHECK: Compute source hash first to skip typechecking
        // ======================================================================
        // If the DLL is already cached (same source content), we can skip ALL
        // compilation including type checking. This dramatically speeds up cached runs.

        std::string combined_hash;
        std::string lib_ext = get_shared_lib_extension();
        fs::path lib_output = cache_dir / (suite.name + lib_ext);

        // Structure to cache preprocessed sources for reuse in Phase 1
        struct PreprocessedSource {
            std::string file_path;
            std::string preprocessed;
            std::string content_hash;
        };
        std::vector<PreprocessedSource> preprocessed_sources;
        preprocessed_sources.reserve(suite.tests.size());

        auto preprocess_start = Clock::now();

        // First pass: preprocess and compute content hashes (cache for Phase 1)
        for (const auto& test : suite.tests) {
            std::string source_code;
            try {
                source_code = read_file(test.file_path);
            } catch (const std::exception&) {
                result.error_message = "Failed to read: " + test.file_path;
                result.failed_test = test.file_path;
                return result;
            }

            auto pp_config = preprocessor::Preprocessor::host_config();
            // Inject feature defines: --feature network → FEATURE_NETWORK
            for (const auto& feat : features) {
                std::string upper = feat;
                std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
                pp_config.defines["FEATURE_" + upper] = "1";
            }
            preprocessor::Preprocessor pp(pp_config);
            auto pp_result = pp.process(source_code, test.file_path);

            if (!pp_result.success()) {
                std::ostringstream oss;
                oss << "Preprocessor errors in " << test.file_path << ":\n";
                for (const auto& diag : pp_result.diagnostics) {
                    if (diag.severity == preprocessor::DiagnosticSeverity::Error) {
                        oss << "  " << diag.line << ":" << diag.column << ": " << diag.message
                            << "\n";
                    }
                }
                result.error_message = oss.str();
                result.failed_test = test.file_path;
                return result;
            }

            std::string content_hash = build::generate_content_hash(pp_result.output);
            combined_hash += content_hash;

            // Cache preprocessed source for reuse in Phase 1
            preprocessed_sources.push_back(
                {test.file_path, std::move(pp_result.output), content_hash});
        }

        preprocess_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - preprocess_start)
                .count();

        // Include coverage flag in hash to separate coverage-enabled builds
        if (CompilerOptions::coverage) {
            combined_hash += ":coverage";
        }
        // Include backend in hash to separate LLVM vs Cranelift builds
        if (backend != "llvm") {
            combined_hash += ":backend=" + backend;
        }

        // Check for cached DLL using source-only hash (before typechecking)
        std::string source_hash = build::generate_content_hash(combined_hash);
        fs::path cached_dll_by_source = cache_dir / (source_hash + "_suite" + lib_ext);

        if (!no_cache && fs::exists(cached_dll_by_source)) {
            // Cache hit! Skip all typechecking and compilation
            TML_LOG_INFO("test", "EARLY CACHE HIT - skipping compilation");
            if (!fast_copy_file(cached_dll_by_source, lib_output)) {
                result.error_message = "Failed to copy cached DLL";
                if (!suite.tests.empty()) {
                    result.failed_test = suite.tests[0].file_path;
                }
                return result;
            }

            auto end = Clock::now();
            result.success = true;
            result.dll_path = lib_output.string();
            result.compile_time_us =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            return result;
        }

        // ======================================================================
        // FULL COMPILATION: Cache miss, do full lex/parse/typecheck/codegen
        // ======================================================================

        // Reset combined_hash for per-file tracking in full compilation
        combined_hash.clear();

        // Structure to hold pending object compilations
        struct PendingCompile {
            std::string ir_content; // LLVM IR string (in-memory, no .ll file)
            fs::path obj_path;
            std::string test_path;
            bool needs_compile = false;
        };

        std::vector<fs::path> object_files;
        std::vector<std::string> link_libs;
        std::vector<PendingCompile> pending_compiles;

        // Track imported module paths from all files (for get_runtime_objects)
        std::mutex modules_mutex;
        std::set<std::string> imported_module_paths;

        // ======================================================================
        // PHASE 1: Parallel lex/parse/typecheck/codegen
        // ======================================================================
        // Only processes files that need compilation (not cached).
        // Uses preprocessed sources cached from the early cache check loop.
        // Each file is processed independently - the shared_registry is populated
        // later when we parse the first module for get_runtime_objects.

        // First, identify which files need compilation vs are cached
        struct CompileTask {
            size_t index;
            std::string file_path;
            std::string preprocessed;
            std::string content_hash;
            fs::path obj_output;
            bool needs_compile;
        };

        std::vector<CompileTask> tasks;
        tasks.reserve(suite.tests.size());

        // Track per-file import sets to determine if shared library is safe
        std::vector<std::set<std::string>> per_file_imports;
        per_file_imports.reserve(suite.tests.size());

        for (size_t i = 0; i < suite.tests.size(); ++i) {
            const auto& pp_source = preprocessed_sources[i];
            std::string backend_tag = (backend != "llvm") ? "_" + backend : "";
            std::string obj_name =
                pp_source.content_hash + backend_tag + "_suite_" + std::to_string(i);
            fs::path obj_output = cache_dir / (obj_name + get_object_extension());
            bool needs_compile = no_cache || !fs::exists(obj_output);

            combined_hash += pp_source.content_hash;
            object_files.push_back(obj_output);

            if (needs_compile) {
                tasks.push_back({i, pp_source.file_path, pp_source.preprocessed,
                                 pp_source.content_hash, obj_output, true});
            }

            // Collect module imports from ALL files (even cached ones)
            // This is needed for get_runtime_objects to know which runtimes to link
            // Use quick lex/parse to extract use declarations without type-checking
            std::set<std::string> file_imports;
            auto source = lexer::Source::from_string(pp_source.preprocessed, pp_source.file_path);
            lexer::Lexer lex(source);
            auto tokens = lex.tokenize();
            parser::Parser parser(std::move(tokens));
            auto parse_result = parser.parse_module(fs::path(pp_source.file_path).stem().string());
            if (std::holds_alternative<parser::Module>(parse_result)) {
                const auto& mod = std::get<parser::Module>(parse_result);
                for (const auto& decl : mod.decls) {
                    if (decl->is<parser::UseDecl>()) {
                        const auto& use_decl = decl->as<parser::UseDecl>();
                        std::string use_path;
                        for (size_t j = 0; j < use_decl.path.segments.size(); ++j) {
                            if (j > 0)
                                use_path += "::";
                            use_path += use_decl.path.segments[j];
                        }
                        imported_module_paths.insert(use_path);
                        file_imports.insert(use_path);
                        // Also add parent paths
                        std::string parent = use_path;
                        while (true) {
                            auto pos = parent.rfind("::");
                            if (pos == std::string::npos)
                                break;
                            parent = parent.substr(0, pos);
                            imported_module_paths.insert(parent);
                            file_imports.insert(parent);
                        }
                    }
                }
            }
            per_file_imports.push_back(std::move(file_imports));
        }

        // Check if all files in the suite have the same import set.
        // The shared library is only safe when all files import the same modules,
        // because the library IR is generated from a single file's type environment.
        bool all_imports_match = true;
        if (per_file_imports.size() >= 2) {
            const auto& first_imports = per_file_imports[0];
            for (size_t i = 1; i < per_file_imports.size(); ++i) {
                if (per_file_imports[i] != first_imports) {
                    all_imports_match = false;
                    break;
                }
            }
        }

        // ======================================================================
        // SHARED LIBRARY: Generate library IR once, capture codegen state
        // ======================================================================
        // When there are multiple test files to compile, generate the library IR
        // once from the first test file. Two optimizations:
        //
        // 1. Shared .obj: Compile library IR to a shared object, workers emit
        //    only declarations (library_decls_only). Disabled in coverage mode
        //    because workers need full definitions with coverage instrumentation.
        //
        // 2. Cached codegen state: Capture type defs, declarations, registries
        //    from the library codegen and pass to workers via cached_library_state.
        //    This skips emit_module_pure_tml_functions() entirely (~9s for heavy
        //    modules like net/sync/zlib). Works in ALL modes including coverage.
        fs::path shared_lib_obj;
        bool use_shared_lib = false;
        std::shared_ptr<const codegen::CodegenLibraryState> shared_codegen_state;

        if (tasks.size() >= 2 && all_imports_match) {
            // Use a hash of all imported module paths to identify the shared library
            std::string import_hash;
            for (const auto& path : imported_module_paths) {
                import_hash += path + ";";
            }
            std::string lib_hash = build::generate_content_hash(import_hash);
            shared_lib_obj = cache_dir / (lib_hash + "_sharedlib" + get_object_extension());

            // In non-coverage mode, check for cached shared library object
            if (!CompilerOptions::coverage && !no_cache && fs::exists(shared_lib_obj)) {
                // Cache hit - reuse previously compiled shared library
                use_shared_lib = true;
                TML_LOG_INFO("test", "  Shared library cache hit: " << shared_lib_obj.filename());
            }

            // Always generate library IR to capture codegen state for workers.
            // Even on shared lib cache hit, we need the state (type defs, registries).
            // This is done ONCE per suite; workers skip emit_module_pure_tml_functions().
            {
                TML_LOG_INFO("test", "  Generating shared library IR...");
                auto& first_task = tasks[0];

                try {
                    auto source =
                        lexer::Source::from_string(first_task.preprocessed, first_task.file_path);
                    lexer::Lexer lex(source);
                    auto tokens = lex.tokenize();

                    if (!lex.has_errors()) {
                        parser::Parser parser(std::move(tokens));
                        auto module_name = fs::path(first_task.file_path).stem().string();
                        auto parse_result = parser.parse_module(module_name);

                        if (std::holds_alternative<parser::Module>(parse_result)) {
                            const auto& module = std::get<parser::Module>(parse_result);

                            auto lib_registry = std::make_shared<types::ModuleRegistry>();
                            types::TypeChecker checker;
                            checker.set_module_registry(lib_registry);
                            auto check_result = checker.check_module(module);

                            if (std::holds_alternative<types::TypeEnv>(check_result)) {
                                const auto& env = std::get<types::TypeEnv>(check_result);

                                // Generate library-only IR
                                codegen::LLVMGenOptions lib_options;
                                lib_options.emit_comments = false;
                                lib_options.generate_dll_entry = false;
                                lib_options.dll_export = false;
                                lib_options.force_internal_linkage = false;
                                lib_options.library_ir_only = true;
                                lib_options.emit_debug_info = false;
                                lib_options.coverage_enabled = CompilerOptions::coverage;
                                lib_options.coverage_quiet = CompilerOptions::coverage;
                                codegen::LLVMIRGen lib_gen(env, lib_options);

                                auto gen_result = lib_gen.generate(module);

                                if (std::holds_alternative<std::string>(gen_result)) {
                                    const auto& lib_ir = std::get<std::string>(gen_result);

                                    // Capture codegen state for worker threads
                                    shared_codegen_state = lib_gen.capture_library_state(lib_ir);

                                    // Compile shared .obj only in non-coverage mode and
                                    // only if not already cached
                                    if (!CompilerOptions::coverage && !use_shared_lib) {
                                        ObjectCompileOptions obj_options;
                                        obj_options.optimization_level =
                                            CompilerOptions::optimization_level;
                                        obj_options.debug_info = false;
                                        obj_options.verbose = false;
                                        obj_options.coverage = false;

                                        auto obj_result = compile_ir_string_to_object(
                                            lib_ir, shared_lib_obj, clang, obj_options);

                                        if (obj_result.success) {
                                            use_shared_lib = true;
                                            TML_LOG_INFO("test", "  Shared library compiled: "
                                                                     << shared_lib_obj.filename());
                                        } else {
                                            TML_LOG_WARN("test",
                                                         "  Shared library compilation failed: "
                                                             << obj_result.error_message);
                                        }
                                    }
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    TML_LOG_WARN("test", "  Shared library generation failed: "
                                             << e.what() << " (falling back)");
                }
            }
        }

        // Process compilation tasks in parallel
        // NOTE: When multiple suites are being compiled in parallel (the common case),
        // we limit internal parallelism to avoid thread explosion. With 22 suites and
        // 32 threads each, we'd have 704 threads competing for CPU!
        // Use at most 2 internal threads per suite to balance parallelism.
        auto phase1_start = Clock::now();

        if (!tasks.empty()) {
            std::atomic<size_t> next_task{0};
            std::atomic<bool> has_error{false};
            std::string first_error_msg;
            std::string first_error_file;
            std::mutex error_mutex;
            std::mutex pending_mutex;
            std::mutex libs_mutex;
            std::mutex timing_mutex;

            // Per-task timing tracking for slow task detection
            struct TaskTiming {
                size_t task_idx;
                std::string file_path;
                int64_t duration_us;
                // Sub-phase timings
                int64_t lex_us;
                int64_t parse_us;
                int64_t typecheck_us;
                int64_t borrow_us;
                int64_t codegen_us;
            };
            std::vector<TaskTiming> task_timings;
            task_timings.reserve(tasks.size());

            // Aggregate sub-phase totals for summary
            std::atomic<int64_t> total_lex_us{0};
            std::atomic<int64_t> total_parse_us{0};
            std::atomic<int64_t> total_typecheck_us{0};
            std::atomic<int64_t> total_borrow_us{0};
            std::atomic<int64_t> total_codegen_us{0};

            // Running average for slow task detection
            std::atomic<int64_t> total_task_time_us{0};
            std::atomic<size_t> completed_tasks{0};

            // Phase 1 internal parallelism: lex/parse/typecheck/codegen per test file.
            // Thread-safe: GlobalModuleCache uses shared_mutex, ModuleRegistry is per-thread,
            // path cache uses shared_mutex, TypeEnv is per-thread.
            // Capped at ~33% of cores per suite, range [2, 4] (3 suites run in parallel).
            unsigned int num_threads =
                calc_codegen_threads(static_cast<unsigned int>(tasks.size()));

            // Pre-load all library modules from .tml.meta binary cache.
            // This MUST complete before any test compilation starts.
            // It either loads existing .tml.meta files or generates them from source.
            types::preload_all_meta_caches();

            auto compile_task_worker = [&]() {
                while (!has_error.load()) {
                    size_t task_idx = next_task.fetch_add(1);
                    if (task_idx >= tasks.size())
                        break;

                    // Fresh registry per task to avoid type environment pollution
                    // from previous tasks on the same thread. Library modules are
                    // fast to load from GlobalModuleCache (pre-populated by
                    // preload_all_meta_caches()).
                    auto thread_registry = std::make_shared<types::ModuleRegistry>();

                    auto& task = tasks[task_idx];
                    auto task_start = Clock::now();

                    TML_LOG_INFO("test", "  Processing test " << (task_idx + 1) << "/"
                                                              << tasks.size() << ": "
                                                              << task.file_path);

                    // Sub-phase timing for detailed profiling
                    int64_t lex_us = 0, parse_us = 0, typecheck_us = 0, borrow_us = 0,
                            codegen_us = 0;

                    try {
                        // Lex
                        auto lex_start = Clock::now();
                        auto source = lexer::Source::from_string(task.preprocessed, task.file_path);
                        lexer::Lexer lex(source);
                        auto tokens = lex.tokenize();
                        lex_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                     Clock::now() - lex_start)
                                     .count();

                        if (lex.has_errors()) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                std::ostringstream oss;
                                oss << "Lexer errors in " << task.file_path << ":\n";
                                for (const auto& err : lex.errors()) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }

                        // Parse
                        auto parse_start = Clock::now();
                        parser::Parser parser(std::move(tokens));
                        auto module_name = fs::path(task.file_path).stem().string();
                        auto parse_result = parser.parse_module(module_name);
                        parse_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                       Clock::now() - parse_start)
                                       .count();

                        if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                const auto& errors =
                                    std::get<std::vector<parser::ParseError>>(parse_result);
                                std::ostringstream oss;
                                oss << "Parser errors in " << task.file_path << ":\n";
                                for (const auto& err : errors) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }
                        const auto& module = std::get<parser::Module>(parse_result);

                        // Type check with thread-local registry
                        auto typecheck_start = Clock::now();
                        types::TypeChecker checker;
                        checker.set_module_registry(thread_registry);
                        auto check_result = checker.check_module(module);
                        typecheck_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                           Clock::now() - typecheck_start)
                                           .count();

                        if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                const auto& errors =
                                    std::get<std::vector<types::TypeError>>(check_result);
                                std::ostringstream oss;
                                oss << "Type errors in " << task.file_path << ":\n";
                                for (const auto& err : errors) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }
                        const auto& env = std::get<types::TypeEnv>(check_result);

                        // Collect imported module paths for get_runtime_objects
                        {
                            std::lock_guard<std::mutex> lock(modules_mutex);
                            for (const auto& [path, _] : thread_registry->get_all_modules()) {
                                imported_module_paths.insert(path);
                            }
                        }

                        // Borrow check (Polonius or NLL)
                        auto borrow_start = Clock::now();
                        std::variant<bool, std::vector<borrow::BorrowError>> borrow_result;
                        if (CompilerOptions::polonius) {
                            borrow::polonius::PoloniusChecker polonius_checker(env);
                            borrow_result = polonius_checker.check_module(module);
                        } else {
                            borrow::BorrowChecker borrow_checker(env);
                            borrow_result = borrow_checker.check_module(module);
                        }
                        borrow_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                        Clock::now() - borrow_start)
                                        .count();

                        if (std::holds_alternative<std::vector<borrow::BorrowError>>(
                                borrow_result)) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                const auto& errors =
                                    std::get<std::vector<borrow::BorrowError>>(borrow_result);
                                std::ostringstream oss;
                                oss << "Borrow check errors in " << task.file_path << ":\n";
                                for (const auto& err : errors) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }

                        // Codegen with indexed entry point
                        auto codegen_start = Clock::now();

                        if (backend == "cranelift") {
                            // ======================================================
                            // Cranelift path: HIR → MIR → Cranelift → object file
                            // Plus LLVM IR for library functions + entry stub
                            // ======================================================

                            // 1. Build HIR
                            types::TypeEnv env_copy = env;
                            hir::HirBuilder hir_builder(env_copy);
                            auto hir_module = hir_builder.lower_module(module);

                            // 2. Build MIR
                            mir::HirMirBuilder mir_builder(env);
                            auto mir_module = mir_builder.build(hir_module);

                            // 2b. Rename MIR functions with suite prefix to avoid
                            // name collisions when multiple test files are in one DLL.
                            // This matches what the LLVM path does with force_internal_linkage.
                            // Also mark all functions as public so Cranelift uses Export
                            // linkage — the stub needs to call them across object files.
                            std::string suite_prefix = "s" + std::to_string(task.index) + "_";
                            for (auto& mir_func : mir_module.functions) {
                                mir_func.name = suite_prefix + mir_func.name;
                                mir_func.is_public = true;
                            }

                            // 3. Compile MIR with Cranelift backend
                            codegen::CodegenOptions cg_opts;
                            cg_opts.optimization_level = CompilerOptions::optimization_level;
                            cg_opts.dll_export = true;
#ifdef _WIN32
                            cg_opts.target_triple = "x86_64-pc-windows-msvc";
#else
                            cg_opts.target_triple = "x86_64-unknown-linux-gnu";
#endif
                            auto cl_backend =
                                codegen::create_backend(codegen::BackendType::Cranelift);
                            auto cg_result = cl_backend->compile_mir(mir_module, cg_opts);

                            codegen_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                             Clock::now() - codegen_start)
                                             .count();

                            if (!cg_result.success) {
                                std::lock_guard<std::mutex> lock(error_mutex);
                                if (!has_error.load()) {
                                    has_error.store(true);
                                    first_error_msg = "Cranelift codegen error in " +
                                                      task.file_path + ": " +
                                                      cg_result.error_message;
                                    first_error_file = task.file_path;
                                }
                                continue;
                            }

                            // Copy Cranelift object to the expected output location
                            if (cg_result.object_file != task.obj_output) {
                                std::error_code ec;
                                fs::copy_file(cg_result.object_file, task.obj_output,
                                              fs::copy_options::overwrite_existing, ec);
                                if (ec) {
                                    std::lock_guard<std::mutex> lock(error_mutex);
                                    if (!has_error.load()) {
                                        has_error.store(true);
                                        first_error_msg =
                                            "Failed to copy Cranelift object: " + ec.message();
                                        first_error_file = task.file_path;
                                    }
                                    continue;
                                }
                            }

                            // 4. Collect Cranelift external symbol references from MIR.
                            // The MIR has call targets like "fnv1a32", "assert_eq",
                            // and method calls like "to_hex" on receiver type "Hash32".
                            // Cranelift will reference these as "tml_fnv1a32", "tml_assert_eq",
                            // "tml_to_hex". But LLVM names them "tml_std_hash_fnv1a32",
                            // "tml_test_assert_eq", "tml_Hash32_to_hex".
                            // We build a mapping to generate LLVM aliases.
                            std::set<std::string> cranelift_extern_symbols;
                            // MIR function names are the user functions (already have suite prefix)
                            std::set<std::string> mir_func_names;
                            for (const auto& mf : mir_module.functions) {
                                mir_func_names.insert("tml_" + mf.name);
                            }
                            for (const auto& mf : mir_module.functions) {
                                for (const auto& block : mf.blocks) {
                                    for (const auto& inst : block.instructions) {
                                        std::visit(
                                            [&](const auto& i) {
                                                using T = std::decay_t<decltype(i)>;
                                                if constexpr (std::is_same_v<T, mir::CallInst>) {
                                                    std::string sym = "tml_" + i.func_name;
                                                    if (mir_func_names.find(sym) ==
                                                        mir_func_names.end()) {
                                                        cranelift_extern_symbols.insert(sym);
                                                    }
                                                } else if constexpr (std::is_same_v<
                                                                         T, mir::MethodCallInst>) {
                                                    std::string sym = "tml_" + i.method_name;
                                                    cranelift_extern_symbols.insert(sym);
                                                }
                                            },
                                            inst.inst);
                                    }
                                }
                            }

                            // 5. Generate LLVM IR using full codegen (same as LLVM path)
                            codegen::LLVMGenOptions lib_options;
                            lib_options.emit_comments = false;
                            lib_options.generate_dll_entry = true;
                            lib_options.suite_test_index = static_cast<int>(task.index);
                            lib_options.suite_total_tests = static_cast<int>(suite.tests.size());
                            lib_options.dll_export = true;
                            lib_options.force_internal_linkage = true;
                            lib_options.library_decls_only = use_shared_lib;
                            lib_options.emit_debug_info = false;
                            lib_options.coverage_enabled = CompilerOptions::coverage;
                            lib_options.coverage_quiet = CompilerOptions::coverage;
                            lib_options.cached_library_state = shared_codegen_state;
                            lib_options.lazy_library_defs = true;

                            codegen::LLVMIRGen lib_gen(env, lib_options);
                            auto lib_gen_result = lib_gen.generate(module);

                            std::string combined_ir;
                            if (std::holds_alternative<std::string>(lib_gen_result)) {
                                combined_ir = std::get<std::string>(lib_gen_result);
                            }

                            if (!combined_ir.empty()) {
                                // Post-process the LLVM IR:
                                // 1. Strip user function bodies (suite-prefixed) → declarations
                                // 2. Promote library functions from internal → external linkage
                                // 3. Collect LLVM function names for alias generation
                                std::string search_prefix = "tml_" + suite_prefix;
                                std::string result_ir;
                                result_ir.reserve(combined_ir.size());
                                std::istringstream stream(combined_ir);
                                std::string line;
                                bool skipping_body = false;
                                int brace_depth = 0;
                                std::vector<std::string> llvm_func_names;

                                // Helper: find @funcname or @"funcname" in a line
                                // When prefix_match=true, matches names that START with the given
                                // name Returns position of the @ character
                                auto find_at_name = [](const std::string& ln,
                                                       const std::string& name,
                                                       bool prefix_match = false) -> size_t {
                                    // Try quoted: @"name..."
                                    auto pos = ln.find("@\"" + name);
                                    if (pos != std::string::npos)
                                        return pos;
                                    // Try unquoted: @name...
                                    pos = ln.find("@" + name);
                                    if (pos != std::string::npos) {
                                        if (prefix_match)
                                            return pos;
                                        size_t after = pos + 1 + name.size();
                                        if (after <= ln.size()) {
                                            char c = (after < ln.size()) ? ln[after] : '(';
                                            if (c == '(' || c == ')' || c == ' ' || c == '"') {
                                                return pos;
                                            }
                                        }
                                    }
                                    return std::string::npos;
                                };

                                // Helper: extract function name from a define line
                                auto extract_func_name = [](const std::string& ln) -> std::string {
                                    auto at = ln.find('@');
                                    if (at == std::string::npos)
                                        return "";
                                    at++; // skip @
                                    bool quoted = (at < ln.size() && ln[at] == '"');
                                    if (quoted)
                                        at++; // skip opening quote
                                    size_t end = at;
                                    while (end < ln.size() && ln[end] != '(' && ln[end] != '"' &&
                                           ln[end] != ' ') {
                                        end++;
                                    }
                                    return ln.substr(at, end - at);
                                };

                                while (std::getline(stream, line)) {
                                    if (skipping_body) {
                                        for (char c : line) {
                                            if (c == '{')
                                                brace_depth++;
                                            else if (c == '}')
                                                brace_depth--;
                                        }
                                        if (brace_depth <= 0) {
                                            skipping_body = false;
                                        }
                                        continue;
                                    }

                                    // Check if this is a user function definition (suite-prefixed)
                                    if (line.find("define ") != std::string::npos &&
                                        find_at_name(line, search_prefix, true) !=
                                            std::string::npos) {

                                        auto at_pos = find_at_name(line, search_prefix, true);
                                        // Convert to declaration
                                        std::string prefix_part = line.substr(0, at_pos);
                                        auto last_space = prefix_part.rfind(' ');
                                        std::string ret_type =
                                            (last_space != std::string::npos)
                                                ? prefix_part.substr(last_space + 1)
                                                : "i32";

                                        std::string func_sig = line.substr(at_pos);
                                        auto brace_pos2 = func_sig.find('{');
                                        if (brace_pos2 != std::string::npos) {
                                            func_sig = func_sig.substr(0, brace_pos2);
                                        }
                                        auto hash_pos = func_sig.find(" #");
                                        if (hash_pos != std::string::npos) {
                                            func_sig = func_sig.substr(0, hash_pos);
                                        }

                                        result_ir += "declare " + ret_type + " " + func_sig + "\n";

                                        if (line.find('{') != std::string::npos) {
                                            brace_depth = 1;
                                            for (size_t ci = line.find('{') + 1; ci < line.size();
                                                 ci++) {
                                                if (line[ci] == '{')
                                                    brace_depth++;
                                                else if (line[ci] == '}')
                                                    brace_depth--;
                                            }
                                            if (brace_depth > 0)
                                                skipping_body = true;
                                        }
                                        continue;
                                    }

                                    // For library functions: promote to linkonce_odr, collect
                                    // names. Library functions may have 'internal', 'dllexport', or
                                    // default linkage depending on whether they come from
                                    // cached_library_state (which was generated without
                                    // force_internal_linkage) or from direct codegen. All non-user
                                    // @tml_ defines need linkonce_odr to prevent duplicate symbol
                                    // errors when multiple test files in the same suite import the
                                    // same library functions.
                                    if (line.find("define ") != std::string::npos &&
                                        line.find("@tml_") != std::string::npos &&
                                        find_at_name(line, search_prefix, true) ==
                                            std::string::npos) {
                                        // Collect function name for alias matching
                                        std::string fn_name = extract_func_name(line);
                                        if (!fn_name.empty()) {
                                            llvm_func_names.push_back(fn_name);
                                        }
                                        // Promote to linkonce_odr linkage (deduplicated by linker)
                                        std::string modified = line;
                                        // Replace various linkage specifiers with linkonce_odr
                                        auto ipos = modified.find("define internal ");
                                        if (ipos != std::string::npos) {
                                            modified.replace(ipos, 16, "define linkonce_odr ");
                                        } else {
                                            auto dpos = modified.find("define dllexport ");
                                            if (dpos != std::string::npos) {
                                                modified.replace(dpos, 17, "define linkonce_odr ");
                                            } else {
                                                // Plain 'define' without linkage qualifier
                                                auto ppos = modified.find("define ");
                                                if (ppos != std::string::npos) {
                                                    modified.replace(ppos, 7,
                                                                     "define linkonce_odr ");
                                                }
                                            }
                                        }
                                        result_ir += modified + "\n";
                                    } else {
                                        result_ir += line + "\n";
                                    }
                                }

                                // 6. Generate LLVM aliases for Cranelift symbol references.
                                // For each symbol Cranelift references (e.g., "tml_fnv1a32"),
                                // find the matching LLVM function (e.g., "tml_std_hash_fnv1a32")
                                // and create an alias.
                                std::ostringstream aliases;
                                aliases << "\n; Cranelift symbol aliases\n";
                                for (const auto& cl_sym : cranelift_extern_symbols) {
                                    // Check if this symbol already exists in the LLVM IR
                                    bool found_exact = false;
                                    for (const auto& llvm_fn : llvm_func_names) {
                                        if (llvm_fn == cl_sym) {
                                            found_exact = true;
                                            break;
                                        }
                                    }
                                    if (found_exact)
                                        continue;

                                    // Try suffix matching: "tml_fnv1a32" → look for "*_fnv1a32"
                                    std::string suffix = cl_sym.substr(4); // strip "tml_"
                                    std::string best_match;
                                    for (const auto& llvm_fn : llvm_func_names) {
                                        // Check if llvm_fn ends with _<suffix>
                                        std::string target = "_" + suffix;
                                        if (llvm_fn.size() > target.size() &&
                                            llvm_fn.substr(llvm_fn.size() - target.size()) ==
                                                target) {
                                            best_match = llvm_fn;
                                            break;
                                        }
                                    }
                                    if (!best_match.empty()) {
                                        // Use a weak alias (global alias) so the Cranelift object
                                        // can reference the short name while the definition uses
                                        // the fully-qualified name.
                                        // LLVM IR alias syntax: @alias = alias <type>, ptr @target
                                        // We don't know the exact type, so use a function alias
                                        // via a simple wrapper.
                                        // Actually, we can't create typed aliases without knowing
                                        // the signature. Instead, emit a forwarding function
                                        // definition. But that requires knowing params/returns...
                                        //
                                        // Simpler: find the LLVM function's full declaration line
                                        // and create a bitcast alias.
                                        // Actually simplest: just define the short-name function
                                        // as calling the long-name function. But we don't know
                                        // params.
                                        //
                                        // Use @alias = alias ptr, ptr @target (opaque pointer
                                        // alias)
                                        aliases << "@\"" << cl_sym << "\" = alias i8, ptr @\""
                                                << best_match << "\"\n";
                                    }
                                }
                                result_ir += aliases.str();
                                combined_ir = std::move(result_ir);
                            }

                            // DEBUG: dump IR + symbol info
                            {
                                fs::path debug_path = fs::path(task.obj_output).parent_path() /
                                                      (task.content_hash + "_cranelift_debug.ll");
                                std::ofstream dbg(debug_path);
                                if (dbg.is_open()) {
                                    dbg << "; Cranelift extern symbols:\n";
                                    for (const auto& s : cranelift_extern_symbols) {
                                        dbg << ";   " << s << "\n";
                                    }
                                    dbg << "\n" << combined_ir;
                                    dbg.close();
                                }
                            }

                            // Collect link libraries from LLVM gen
                            {
                                std::lock_guard<std::mutex> lock(libs_mutex);
                                for (const auto& lib : lib_gen.get_link_libs()) {
                                    if (std::find(link_libs.begin(), link_libs.end(), lib) ==
                                        link_libs.end()) {
                                        link_libs.push_back(lib);
                                    }
                                }
                            }

                            // Store the combined library+stub IR for compilation in Phase 2
                            fs::path stub_obj =
                                fs::path(task.obj_output).parent_path() /
                                (task.content_hash + "_cranelift_stub" + get_object_extension());
                            {
                                std::lock_guard<std::mutex> lock(pending_mutex);
                                pending_compiles.push_back({combined_ir, stub_obj,
                                                            task.file_path + ".cranelift_lib",
                                                            true});
                            }
                            {
                                // Add stub object to link list (Cranelift obj is already there)
                                std::lock_guard<std::mutex> lock(libs_mutex);
                                object_files.push_back(stub_obj);
                            }
                        } else {
                            // ======================================================
                            // LLVM path (default): AST → LLVM IR → object
                            // ======================================================
                            codegen::LLVMGenOptions options;
                            options.emit_comments = false;
                            options.generate_dll_entry = true;
                            options.suite_test_index = static_cast<int>(task.index);
                            options.suite_total_tests = static_cast<int>(suite.tests.size());
                            options.dll_export = true;
                            options.force_internal_linkage = true;
                            options.library_decls_only = use_shared_lib;
                            options.emit_debug_info = CompilerOptions::debug_info;
                            options.debug_level = CompilerOptions::debug_level;
                            options.source_file = task.file_path;
                            options.coverage_enabled = CompilerOptions::coverage;
                            options.coverage_quiet = CompilerOptions::coverage;
                            options.coverage_output_file = CompilerOptions::coverage_output;
                            options.llvm_source_coverage = CompilerOptions::coverage_source;
                            options.cached_library_state = shared_codegen_state;
                            // Enable lazy library defs: only emit declarations/definitions
                            // for library functions actually referenced by this test file.
                            // Dramatically reduces codegen time for heavy modules (net/sync/zlib).
                            options.lazy_library_defs = true;
                            codegen::LLVMIRGen llvm_gen(env, options);

                            auto gen_result = llvm_gen.generate(module);
                            codegen_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                             Clock::now() - codegen_start)
                                             .count();

                            if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(
                                    gen_result)) {
                                std::lock_guard<std::mutex> lock(error_mutex);
                                if (!has_error.load()) {
                                    has_error.store(true);
                                    const auto& errors =
                                        std::get<std::vector<codegen::LLVMGenError>>(gen_result);
                                    std::ostringstream oss;
                                    oss << "Codegen errors in " << task.file_path << ":\n";
                                    for (const auto& err : errors) {
                                        oss << "  " << err.span.start.line << ":"
                                            << err.span.start.column << ": " << err.message << "\n";
                                    }
                                    first_error_msg = oss.str();
                                    first_error_file = task.file_path;
                                }
                                continue;
                            }

                            const auto& llvm_ir = std::get<std::string>(gen_result);

                            // Collect link libraries (thread-safe)
                            {
                                std::lock_guard<std::mutex> lock(libs_mutex);
                                for (const auto& lib : llvm_gen.get_link_libs()) {
                                    if (std::find(link_libs.begin(), link_libs.end(), lib) ==
                                        link_libs.end()) {
                                        link_libs.push_back(lib);
                                    }
                                }
                            }

                            // Store IR string for later parallel compilation (no .ll on disk)
                            {
                                std::lock_guard<std::mutex> lock(pending_mutex);
                                pending_compiles.push_back(
                                    {std::move(llvm_ir), task.obj_output, task.file_path, true});
                            }
                        }

                        // Track task timing and check for slow tasks
                        auto task_end = Clock::now();
                        int64_t task_duration_us =
                            std::chrono::duration_cast<std::chrono::microseconds>(task_end -
                                                                                  task_start)
                                .count();

                        // Update running totals
                        total_task_time_us.fetch_add(task_duration_us);
                        total_lex_us.fetch_add(lex_us);
                        total_parse_us.fetch_add(parse_us);
                        total_typecheck_us.fetch_add(typecheck_us);
                        total_borrow_us.fetch_add(borrow_us);
                        total_codegen_us.fetch_add(codegen_us);
                        size_t completed = completed_tasks.fetch_add(1) + 1;

                        // Check for abnormally slow task (only after we have some baseline)
                        if (completed >= 3) {
                            int64_t avg_time_us =
                                total_task_time_us.load() / static_cast<int64_t>(completed);
                            int64_t threshold_us =
                                std::max(MIN_SLOW_THRESHOLD_US,
                                         static_cast<int64_t>(avg_time_us * SLOW_TASK_THRESHOLD));

                            if (task_duration_us > threshold_us) {
                                // Log slow task as warning (doesn't fail the test)
                                std::lock_guard<std::mutex> lock(error_mutex);
                                TML_LOG_WARN(
                                    "test",
                                    "[SLOW TASK] "
                                        << task.file_path
                                        << " Duration: " << (task_duration_us / 1000) << " ms"
                                        << " Average: " << (avg_time_us / 1000) << " ms"
                                        << " Threshold: " << (threshold_us / 1000) << " ms ("
                                        << SLOW_TASK_THRESHOLD << "x average)"
                                        << " Sub-phases: lex=" << (lex_us / 1000)
                                        << "ms, parse=" << (parse_us / 1000)
                                        << "ms, typecheck=" << (typecheck_us / 1000)
                                        << "ms, borrow=" << (borrow_us / 1000)
                                        << "ms, codegen=" << (codegen_us / 1000) << "ms"
                                        << " This task took " << std::fixed << std::setprecision(1)
                                        << (static_cast<double>(task_duration_us) / avg_time_us)
                                        << "x longer than average.");
                                // Don't abort - slow tasks still generate valid cache
                            }
                        }

                        // Record timing (thread-safe)
                        {
                            std::lock_guard<std::mutex> lock(timing_mutex);
                            task_timings.push_back({task_idx, task.file_path, task_duration_us,
                                                    lex_us, parse_us, typecheck_us, borrow_us,
                                                    codegen_us});
                        }

                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!has_error.load()) {
                            has_error.store(true);
                            first_error_msg =
                                "Exception while compiling " + task.file_path + ": " + e.what();
                            first_error_file = task.file_path;
                        }
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!has_error.load()) {
                            has_error.store(true);
                            first_error_msg = "Unknown exception while compiling " + task.file_path;
                            first_error_file = task.file_path;
                        }
                    }
                }
            };

            TML_LOG_INFO("test", "  Generating " << tasks.size() << " LLVM IR files with "
                                                 << num_threads << " threads...");

            // Launch worker threads
            std::vector<std::thread> threads;
            for (unsigned int t = 0; t < std::min(num_threads, (unsigned int)tasks.size()); ++t) {
                threads.emplace_back(compile_task_worker);
            }
            for (auto& t : threads) {
                t.join();
            }

            // Check for errors
            if (has_error.load()) {
                result.error_message = first_error_msg;
                result.failed_test = first_error_file;
                return result;
            }

            // Print Phase 1 timing summary if verbose
            if (verbose && !task_timings.empty()) {
                // Sort by duration (slowest first)
                std::sort(task_timings.begin(), task_timings.end(),
                          [](const TaskTiming& a, const TaskTiming& b) {
                              return a.duration_us > b.duration_us;
                          });

                // Print aggregate sub-phase breakdown (single line)
                int64_t total_us = total_task_time_us.load();
                if (total_us > 0) {
                    TML_LOG_INFO("test",
                                 "Phase 1 sub-phases:"
                                     << " lex=" << (total_lex_us.load() / 1000) << "ms"
                                     << " parse=" << (total_parse_us.load() / 1000) << "ms"
                                     << " typecheck=" << (total_typecheck_us.load() / 1000) << "ms"
                                     << " borrow=" << (total_borrow_us.load() / 1000) << "ms"
                                     << " codegen=" << (total_codegen_us.load() / 1000) << "ms"
                                     << " total=" << (total_us / 1000) << "ms");
                }

                {
                    // Log each file as a separate single-line entry (all files, sorted slowest
                    // first)
                    for (size_t i = 0; i < task_timings.size(); ++i) {
                        const auto& t = task_timings[i];
                        TML_LOG_INFO("test",
                                     "Phase 1 slow #"
                                         << i << ": " << fs::path(t.file_path).filename().string()
                                         << " " << (t.duration_us / 1000) << "ms"
                                         << " [lex=" << (t.lex_us / 1000) << " parse="
                                         << (t.parse_us / 1000) << " tc=" << (t.typecheck_us / 1000)
                                         << " borrow=" << (t.borrow_us / 1000)
                                         << " cg=" << (t.codegen_us / 1000) << "]");
                    }
                }
            }
        }

        phase1_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase1_start)
                .count();

        // ======================================================================
        // PHASE 2: Parallel object compilation (.ll -> .obj)
        // ======================================================================
        // Like Phase 1, limit internal parallelism since suites compile in parallel

        auto phase2_start = Clock::now();

        if (!pending_compiles.empty()) {
            ObjectCompileOptions obj_options;
            obj_options.optimization_level = CompilerOptions::optimization_level;
            obj_options.debug_info = CompilerOptions::debug_info;
            obj_options.verbose = false;
            obj_options.coverage = CompilerOptions::coverage_source;

            std::atomic<size_t> next_compile{0};
            std::atomic<bool> compile_error{false};
            std::string error_message;
            std::string failed_test;
            std::mutex error_mutex;
            std::mutex timing_mutex;

            // Per-task timing for Phase 2
            struct ObjTiming {
                std::string test_path;
                int64_t duration_us;
            };
            std::vector<ObjTiming> obj_timings;
            obj_timings.reserve(pending_compiles.size());

            std::atomic<int64_t> total_obj_time_us{0};
            std::atomic<size_t> completed_objs{0};

            // Phase 2: compile .ll -> .obj using clang subprocesses.
            // Each subprocess is independent (unique file paths). Thread-safe by design.
            unsigned int num_threads =
                calc_codegen_threads(static_cast<unsigned int>(pending_compiles.size()));

            auto compile_worker = [&]() {
                while (!compile_error.load()) {
                    size_t idx = next_compile.fetch_add(1);
                    if (idx >= pending_compiles.size())
                        break;

                    auto& pc = pending_compiles[idx];
                    auto obj_start = Clock::now();

                    auto obj_result =
                        compile_ir_string_to_object(pc.ir_content, pc.obj_path, clang, obj_options);

                    auto obj_end = Clock::now();
                    int64_t obj_duration_us =
                        std::chrono::duration_cast<std::chrono::microseconds>(obj_end - obj_start)
                            .count();

                    // Update timing stats
                    total_obj_time_us.fetch_add(obj_duration_us);
                    size_t completed = completed_objs.fetch_add(1) + 1;

                    // Check for slow object compilation
                    if (completed >= 3) {
                        int64_t avg_us = total_obj_time_us.load() / static_cast<int64_t>(completed);
                        int64_t threshold_us =
                            std::max(MIN_SLOW_THRESHOLD_US,
                                     static_cast<int64_t>(avg_us * SLOW_TASK_THRESHOLD));

                        if (obj_duration_us > threshold_us) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            TML_LOG_WARN("test",
                                         "[SLOW OBJ] "
                                             << pc.test_path
                                             << " Duration: " << (obj_duration_us / 1000) << " ms"
                                             << " Average: " << (avg_us / 1000) << " ms"
                                             << " This .obj compilation took " << std::fixed
                                             << std::setprecision(1)
                                             << (static_cast<double>(obj_duration_us) / avg_us)
                                             << "x longer than average!");
                        }
                    }

                    // Record timing
                    {
                        std::lock_guard<std::mutex> lock(timing_mutex);
                        obj_timings.push_back({pc.test_path, obj_duration_us});
                    }

                    if (!obj_result.success) {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!compile_error.load()) {
                            compile_error.store(true);
                            error_message = "Compilation failed: " + obj_result.error_message;
                            failed_test = pc.test_path;
                        }
                    }
                }
            };

            TML_LOG_INFO("test", "  Compiling " << pending_compiles.size() << " objects with "
                                                << num_threads << " threads...");

            std::vector<std::thread> threads;
            for (unsigned int t = 0;
                 t < std::min(num_threads, (unsigned int)pending_compiles.size()); ++t) {
                threads.emplace_back(compile_worker);
            }
            for (auto& t : threads) {
                t.join();
            }

            // Print Phase 2 timing summary if verbose
            if (verbose && !obj_timings.empty()) {
                std::sort(obj_timings.begin(), obj_timings.end(),
                          [](const ObjTiming& a, const ObjTiming& b) {
                              return a.duration_us > b.duration_us;
                          });

                for (size_t i = 0; i < std::min(size_t(5), obj_timings.size()); ++i) {
                    const auto& t = obj_timings[i];
                    TML_LOG_INFO("test", "Phase 2 slow #"
                                             << i << ": "
                                             << fs::path(t.test_path).filename().string() << " "
                                             << (t.duration_us / 1000) << "ms");
                }
            }

            if (compile_error.load()) {
                result.error_message = error_message;
                result.failed_test = failed_test;
                return result;
            }
        }

        phase2_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase2_start)
                .count();

        // Get runtime objects (only need to do once for the suite)
        // Register placeholder modules in shared_registry for all imported paths
        // This is much faster than re-type-checking all files
        auto runtime_start = Clock::now();

        // Register placeholder modules for all imported paths AND their parent paths
        // get_runtime_objects uses has_module() checks like has_module("std::file"),
        // so if "std::file::path" is imported, we also need to register "std::file"
        for (const auto& path : imported_module_paths) {
            // Register the full path
            if (!shared_registry->has_module(path)) {
                types::Module placeholder;
                placeholder.name = path;
                shared_registry->register_module(path, std::move(placeholder));
            }
            // Register all parent paths (e.g., "std::file" for "std::file::path")
            std::string parent = path;
            while (true) {
                auto pos = parent.rfind("::");
                if (pos == std::string::npos)
                    break;
                parent = parent.substr(0, pos);
                if (!shared_registry->has_module(parent)) {
                    types::Module placeholder;
                    placeholder.name = parent;
                    shared_registry->register_module(parent, std::move(placeholder));
                }
            }
        }

        // Parse the first file for get_runtime_objects (needs a module reference)
        const auto& first_pp = preprocessed_sources[0];
        auto source = lexer::Source::from_string(first_pp.preprocessed, first_pp.file_path);
        lexer::Lexer lex(source);
        auto tokens = lex.tokenize();
        parser::Parser parser(std::move(tokens));
        auto parse_result = parser.parse_module(fs::path(first_pp.file_path).stem().string());
        const auto& module = std::get<parser::Module>(parse_result);

        std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

        TML_LOG_INFO("test", "  Getting runtime objects...");
        // Note: Pass verbose=false to avoid repeated "Including runtime:" messages
        // when compiling multiple suites in parallel. The runtime objects are the
        // same for all suites and would spam the output.
        auto runtime_objects =
            get_runtime_objects(shared_registry, module, deps_cache, clang, false);
        TML_LOG_INFO("test", "  Got " << runtime_objects.size() << " runtime objects");
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

        // Add shared library object if we generated one
        // This contains all library function implementations (compiled once per suite).
        // Test objects only have `declare` stubs that the linker resolves from this object.
        if (use_shared_lib && fs::exists(shared_lib_obj)) {
            object_files.push_back(shared_lib_obj);
            TML_LOG_INFO("test", "  Using shared library: " << shared_lib_obj.filename());
        }

        runtime_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - runtime_start)
                .count();

        // Generate suite hash for full caching (includes runtime objects)
        std::string suite_hash = generate_content_hash(combined_hash);
        std::string exe_hash = generate_exe_hash(suite_hash, object_files);

        fs::path cached_dll = cache_dir / (exe_hash + "_suite" + lib_ext);

        bool use_cached_dll = !no_cache && fs::exists(cached_dll);

        auto link_start = Clock::now();

        if (!use_cached_dll) {
            // Link as shared library
            LinkOptions link_options;
            link_options.output_type = LinkOptions::OutputType::DynamicLib;
            link_options.verbose = false;
            link_options.coverage = CompilerOptions::coverage_source; // LLVM source coverage

            for (const auto& lib : link_libs) {
                if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                    link_options.link_flags.push_back("\"" + lib + "\"");
                } else {
                    link_options.link_flags.push_back("-l" + lib);
                }
            }

#ifdef _WIN32
            // Add Windows system libraries for socket support
            if (shared_registry->has_module("std::net") ||
                shared_registry->has_module("std::net::sys") ||
                shared_registry->has_module("std::net::tcp") ||
                shared_registry->has_module("std::net::udp")) {
                link_options.link_flags.push_back("-lws2_32");
            }
            // Add Windows system libraries for OS module (Registry, user info)
            if (shared_registry->has_module("std::os")) {
                link_options.link_flags.push_back("-ladvapi32");
                link_options.link_flags.push_back("-luserenv");
            }
            // Add OpenSSL libraries for crypto modules
            if (build::has_crypto_modules(shared_registry)) {
                auto openssl = build::find_openssl();
                if (openssl.found) {
                    link_options.link_flags.push_back(
                        to_forward_slashes((openssl.lib_dir / openssl.crypto_lib).string()));
                    link_options.link_flags.push_back(
                        to_forward_slashes((openssl.lib_dir / openssl.ssl_lib).string()));
                    link_options.link_flags.push_back("/DEFAULTLIB:crypt32");
                    link_options.link_flags.push_back("/DEFAULTLIB:ws2_32");
                }
            }
#endif

            TML_LOG_INFO("test", "  Starting link...");
            auto link_result = link_objects(object_files, cached_dll, clang, link_options);
            TML_LOG_INFO("test", "  Link complete");
            if (!link_result.success) {
                result.error_message = "Linking failed: " + link_result.error_message;
                if (!suite.tests.empty()) {
                    result.failed_test = suite.tests[0].file_path;
                }
                return result;
            }
        }

        link_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - link_start)
                .count();

        // Always save with source-only hash for early cache check on next run
        // This allows skipping typechecking entirely when source hasn't changed
        // Do this even when using cached_dll so we populate the source-hash cache
        if (!fs::exists(cached_dll_by_source)) {
            try {
                fs::copy_file(cached_dll, cached_dll_by_source,
                              fs::copy_options::overwrite_existing);
            } catch (...) {
                // Ignore errors creating source-hash cache
            }
        }

        // Copy to output location
        if (!fast_copy_file(cached_dll, lib_output)) {
            result.error_message = "Failed to copy DLL";
            if (!suite.tests.empty()) {
                result.failed_test = suite.tests[0].file_path;
            }
            return result;
        }

        auto end = Clock::now();
        result.success = true;
        result.dll_path = lib_output.string();
        result.compile_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // Print timing summary if verbose (single line for clean log output)
        if (verbose) {
            int64_t total_us = result.compile_time_us;
            TML_LOG_INFO("test", "Suite " << suite.name << " timing: preprocess="
                                          << (preprocess_time_us / 1000) << "ms"
                                          << " phase1=" << (phase1_time_us / 1000) << "ms"
                                          << " phase2=" << (phase2_time_us / 1000) << "ms"
                                          << " runtime=" << (runtime_time_us / 1000) << "ms"
                                          << " link=" << (link_time_us / 1000) << "ms"
                                          << " total=" << (total_us / 1000) << "ms");
        }

        return result;

    } catch (const std::exception& e) {
        result.error_message = "FATAL EXCEPTION during suite compilation: " + std::string(e.what());
        if (!suite.tests.empty()) {
            result.failed_test = suite.tests[0].file_path;
        }
        TML_LOG_FATAL("test", "Exception in compile_test_suite: " << e.what());
        return result;
    } catch (...) {
        result.error_message = "FATAL UNKNOWN EXCEPTION during suite compilation";
        if (!suite.tests.empty()) {
            result.failed_test = suite.tests[0].file_path;
        }
        TML_LOG_FATAL("test", "Unknown exception in compile_test_suite");
        return result;
    }
}

SuiteCompileResult compile_test_suite_profiled(const TestSuite& suite, PhaseTimings* timings,
                                               bool verbose, bool no_cache,
                                               const std::vector<std::string>& features) {
    // For now, just use the regular compile and record total time
    // Detailed phase profiling can be added later if needed
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    auto result = compile_test_suite(suite, verbose, no_cache, "llvm", features);

    if (timings) {
        auto end = Clock::now();
        timings->timings_us["suite_compile"] =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    return result;
}

} // namespace tml::cli
