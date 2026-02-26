TML_MODULE("test")

//! # EXE-Based Test Runner Compilation Pipeline
//!
//! Compiles a TestSuite to an executable instead of a DLL.
//! Adapts the compile_test_suite() pipeline with these changes:
//! - Generates a dispatcher main() that routes --test-index=N to tml_test_N()
//! - Links as Executable instead of DynamicLib
//! - Cache keys use "exe_v2" prefix to avoid DLL cache collisions
//!
//! The codegen pipeline is identical to the DLL path — we still generate
//! tml_test_0(), tml_test_1(), etc. with generate_dll_entry=true.

#include "exe_test_runner.hpp"

#include "cli/builder/builder_internal.hpp"
#include "cli/builder/compiler_setup.hpp"
#include "cli/builder/object_compiler.hpp"
#include "cli/commands/cmd_build.hpp"
#include "cli/tester/tester_internal.hpp"
#include "codegen/llvm/llvm_ir_gen.hpp"
#include "log/log.hpp"
#include "preprocessor/preprocessor.hpp"
#include "types/module_binary.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace tml::cli {

using namespace build;

// ============================================================================
// Global Library IR Cache
// ============================================================================
// Caches generated library LLVM IR strings and pre-populated ModuleRegistries
// keyed by import hash. Avoids re-running typecheck+codegen for library modules
// when multiple suites import the same set of modules. Thread-safe singleton.

class GlobalLibraryIRCache {
public:
    static GlobalLibraryIRCache& instance() {
        static GlobalLibraryIRCache cache;
        return cache;
    }

    // Get cached library IR string by import hash. Returns empty string on miss.
    std::string get_ir(const std::string& import_hash) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = ir_cache_.find(import_hash);
        if (it != ir_cache_.end()) {
            return it->second;
        }
        return {};
    }

    // Cache a library IR string by import hash.
    void put_ir(const std::string& import_hash, const std::string& ir) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        ir_cache_[import_hash] = ir;
    }

    // Get cached pre-populated ModuleRegistry by import hash.
    std::shared_ptr<types::ModuleRegistry> get_registry(const std::string& import_hash) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = registries_.find(import_hash);
        if (it != registries_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Cache a pre-populated ModuleRegistry by import hash.
    void put_registry(const std::string& import_hash,
                      std::shared_ptr<types::ModuleRegistry> registry) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        registries_[import_hash] = std::move(registry);
    }

    // Get cached codegen library state by import hash.
    std::shared_ptr<const codegen::CodegenLibraryState>
    get_codegen_state(const std::string& import_hash) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = codegen_states_.find(import_hash);
        if (it != codegen_states_.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Cache codegen library state by import hash.
    void put_codegen_state(const std::string& import_hash,
                           std::shared_ptr<const codegen::CodegenLibraryState> state) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        codegen_states_[import_hash] = std::move(state);
    }

private:
    GlobalLibraryIRCache() = default;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> ir_cache_;
    std::unordered_map<std::string, std::shared_ptr<types::ModuleRegistry>> registries_;
    std::unordered_map<std::string, std::shared_ptr<const codegen::CodegenLibraryState>>
        codegen_states_;
};

// Reuse calc_codegen_threads logic (same as test_runner.cpp)
static unsigned int exe_calc_codegen_threads(unsigned int task_count) {
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0)
        hw = 8;
    // Use at most 40% of cores per suite, clamped to [2, 6]
    unsigned int per_suite = hw * 2 / 5;
    unsigned int clamped = std::clamp(per_suite, 2u, 6u);
    return std::min(clamped, task_count);
}

ExeCompileResult compile_test_suite_exe(const TestSuite& suite, bool verbose, bool no_cache) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    int64_t preprocess_time_us = 0;
    int64_t phase1_time_us = 0;
    int64_t phase2_time_us = 0;
    int64_t runtime_time_us = 0;
    int64_t link_time_us = 0;

    ExeCompileResult result;

    try {
        if (suite.tests.empty()) {
            result.success = true;
            return result;
        }

        fs::path cache_dir = get_run_cache_dir();
        std::string clang = find_clang();

        auto shared_registry = std::make_shared<types::ModuleRegistry>();

        // ======================================================================
        // EARLY CACHE CHECK
        // ======================================================================

        std::string combined_hash;
        fs::path exe_output = cache_dir / (suite.name + ".exe");

        struct PreprocessedSource {
            std::string file_path;
            std::string preprocessed;
            std::string content_hash;
        };
        std::vector<PreprocessedSource> preprocessed_sources;
        preprocessed_sources.reserve(suite.tests.size());

        auto preprocess_start = Clock::now();

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

            std::string content_hash = generate_content_hash(pp_result.output);
            combined_hash += content_hash;

            preprocessed_sources.push_back(
                {test.file_path, std::move(pp_result.output), content_hash});
        }

        preprocess_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - preprocess_start)
                .count();

        if (CompilerOptions::coverage) {
            combined_hash += ":coverage";
        }
        // Mark as exe_v2 to avoid collisions with DLL cache
        combined_hash += ":exe_v2";

        std::string source_hash = generate_content_hash(combined_hash);
        fs::path cached_exe_by_source = cache_dir / (source_hash + "_exe.exe");

        if (!no_cache && fs::exists(cached_exe_by_source)) {
            // Copy to output location
            if (fast_copy_file(cached_exe_by_source, exe_output)) {
                result.success = true;
                result.exe_path = exe_output.string();
                result.compile_time_us =
                    std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start)
                        .count();
                TML_LOG_INFO("test", "  [exe] Cache hit (source hash): " << suite.name);
                return result;
            }
        }

        // ======================================================================
        // PHASE 1: Parallel codegen (lex/parse/typecheck/borrow/codegen)
        // ======================================================================

        auto phase1_start = Clock::now();

        std::vector<fs::path> object_files;
        object_files.reserve(suite.tests.size() + 10);

        std::set<std::string> imported_module_paths;
        std::vector<std::string> link_libs;
        std::mutex libs_mutex;

        // Shared library mechanism: check imports like the DLL path
        std::vector<std::set<std::string>> per_file_imports;
        per_file_imports.reserve(suite.tests.size());

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

        for (size_t i = 0; i < suite.tests.size(); ++i) {
            const auto& pp_source = preprocessed_sources[i];
            std::string obj_name = pp_source.content_hash + "_exesuite_" + std::to_string(i);
            fs::path obj_output = cache_dir / (obj_name + get_object_extension());
            bool needs_compile = no_cache || !fs::exists(obj_output);

            object_files.push_back(obj_output);

            if (needs_compile) {
                tasks.push_back({i, pp_source.file_path, pp_source.preprocessed,
                                 pp_source.content_hash, obj_output, true});
            }

            // Collect module imports
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

        // Generate shared library object using union of all imports
        // Enhanced with cross-suite caching: library IR and ModuleRegistry are
        // cached globally so suites with identical imports skip typecheck+codegen.
        fs::path shared_lib_obj;
        bool use_shared_lib = false;
        std::string lib_hash;

        // Pre-populated registry from shared lib typecheck — used to seed worker
        // thread registries so they don't need to re-load all library modules.
        std::shared_ptr<types::ModuleRegistry> shared_lib_registry;

        // Pre-computed codegen library state — skips emit_module_pure_tml_functions()
        // in worker threads (the ~9 second bottleneck for zlib tests).
        std::shared_ptr<const codegen::CodegenLibraryState> shared_codegen_state;

        if (tasks.size() >= 2) {
            std::string import_hash;
            for (const auto& path : imported_module_paths) {
                import_hash += path + ";";
            }
            import_hash += ":exe_v2";
            lib_hash = generate_content_hash(import_hash);
            shared_lib_obj = cache_dir / (lib_hash + "_exelib" + get_object_extension());

            auto& ir_cache = GlobalLibraryIRCache::instance();

            // Always check in-memory caches (cross-suite reuse within this run).
            // --no-cache only skips disk caches, not in-memory caches from this process.
            shared_lib_registry = ir_cache.get_registry(lib_hash);
            shared_codegen_state = ir_cache.get_codegen_state(lib_hash);

            if (!no_cache && fs::exists(shared_lib_obj)) {
                use_shared_lib = true;
                TML_LOG_INFO("test",
                             "  [exe] Reusing shared lib object: " << shared_lib_obj.filename());
                if (shared_lib_registry) {
                    TML_LOG_INFO("test", "  [exe] Registry cache hit for worker threads");
                }
            } else {
                // Check if library IR is in the in-memory cache (cross-suite reuse).
                // This is always checked — --no-cache only affects disk caches.
                std::string cached_ir = ir_cache.get_ir(lib_hash);

                if (!cached_ir.empty()) {
                    // IR cache hit — skip typecheck+codegen, just compile to .obj
                    TML_LOG_INFO("test", "  [exe] Library IR cache hit — skipping codegen");

                    ObjectCompileOptions obj_opts;
                    obj_opts.optimization_level = CompilerOptions::optimization_level;
                    obj_opts.debug_info = CompilerOptions::debug_info;
                    obj_opts.coverage = CompilerOptions::coverage_source;

                    auto obj_result =
                        compile_ir_string_to_object(cached_ir, shared_lib_obj, clang, obj_opts);

                    if (obj_result.success) {
                        use_shared_lib = true;
                        TML_LOG_INFO("test", "  [exe] Compiled shared lib from cached IR");
                    }
                } else {
                    // Full pipeline: typecheck + codegen + compile
                    // Use the file with the MOST imports as the template for the
                    // shared lib.  This maximises the number of generic instantiations
                    // and library functions that end up in the shared object so that
                    // worker threads using the codegen cache don't hit "undefined".
                    // Prepend use statements from ALL other files to cover their imports.
                    size_t best_idx = 0;
                    size_t max_imports = 0;
                    for (size_t fi = 0; fi < per_file_imports.size(); ++fi) {
                        if (per_file_imports[fi].size() > max_imports) {
                            max_imports = per_file_imports[fi].size();
                            best_idx = fi;
                        }
                    }
                    // Collect use lines from ALL files (union)
                    std::set<std::string> seen_use_lines;
                    std::string extra_use_lines;
                    for (const auto& pp : preprocessed_sources) {
                        std::istringstream stream(pp.preprocessed);
                        std::string line;
                        while (std::getline(stream, line)) {
                            if (line.size() >= 4 && line.substr(0, 4) == "use ") {
                                if (seen_use_lines.insert(line).second) {
                                    extra_use_lines += line + "\n";
                                }
                            }
                        }
                    }
                    // Build merged source: union of use statements + body of best file
                    const auto& body_pp = preprocessed_sources[best_idx];
                    std::string merged_source = extra_use_lines;
                    {
                        std::istringstream stream(body_pp.preprocessed);
                        std::string line;
                        while (std::getline(stream, line)) {
                            // Skip use lines — already in extra_use_lines
                            if (line.size() >= 4 && line.substr(0, 4) == "use ") {
                                continue;
                            }
                            merged_source += line + "\n";
                        }
                    }

                    auto source = lexer::Source::from_string(merged_source, body_pp.file_path);
                    lexer::Lexer lex(source);
                    auto tokens = lex.tokenize();
                    parser::Parser parser(std::move(tokens));
                    auto parse_result =
                        parser.parse_module(fs::path(body_pp.file_path).stem().string());

                    if (std::holds_alternative<parser::Module>(parse_result)) {
                        const auto& module = std::get<parser::Module>(parse_result);
                        auto lib_registry = std::make_shared<types::ModuleRegistry>();
                        types::TypeChecker checker;
                        checker.set_module_registry(lib_registry);
                        auto check_result = checker.check_module(module);

                        if (!std::holds_alternative<types::TypeEnv>(check_result)) {
                            TML_LOG_WARN("test",
                                         "[exe] Type errors in shared lib generation, skipping");
                        } else {
                            const auto& env = std::get<types::TypeEnv>(check_result);

                            // Cache the registry for worker threads (this suite and future suites)
                            shared_lib_registry = lib_registry;
                            ir_cache.put_registry(lib_hash, lib_registry);

                            codegen::LLVMGenOptions lib_options;
                            lib_options.emit_comments = false;
                            lib_options.library_ir_only = true;
                            lib_options.emit_debug_info = CompilerOptions::debug_info;
                            lib_options.debug_level = CompilerOptions::debug_level;
                            lib_options.source_file = body_pp.file_path;
                            lib_options.coverage_enabled = CompilerOptions::coverage;
                            lib_options.coverage_quiet = CompilerOptions::coverage;
                            lib_options.llvm_source_coverage = CompilerOptions::coverage_source;
                            lib_options.lazy_library_defs = true;
                            codegen::LLVMIRGen lib_gen(env, lib_options);

                            auto lib_result = lib_gen.generate(module);
                            if (std::holds_alternative<std::string>(lib_result)) {
                                const auto& ir_string = std::get<std::string>(lib_result);

                                // Capture codegen state for worker threads to skip
                                // emit_module_pure_tml_functions() (~9s per zlib file)
                                shared_codegen_state = lib_gen.capture_library_state(ir_string);
                                ir_cache.put_codegen_state(lib_hash, shared_codegen_state);

                                // Cache the IR for cross-suite reuse
                                ir_cache.put_ir(lib_hash, ir_string);

                                ObjectCompileOptions obj_opts;
                                obj_opts.optimization_level = CompilerOptions::optimization_level;
                                obj_opts.debug_info = CompilerOptions::debug_info;
                                obj_opts.coverage = CompilerOptions::coverage_source;

                                auto obj_result = compile_ir_string_to_object(
                                    ir_string, shared_lib_obj, clang, obj_opts);

                                if (obj_result.success) {
                                    use_shared_lib = true;
                                    TML_LOG_INFO("test", "  [exe] Generated shared lib object");
                                }
                            }
                        }
                    }
                }
            }
        }

        // If we don't have a shared lib but do have tasks, try to get cached
        // registry and codegen state based on the first file's imports.
        if ((!shared_lib_registry || !shared_codegen_state) && !tasks.empty() &&
            !imported_module_paths.empty()) {
            std::string import_hash;
            for (const auto& path : imported_module_paths) {
                import_hash += path + ";";
            }
            import_hash += ":exe_v2";
            std::string hash = generate_content_hash(import_hash);
            if (!shared_lib_registry)
                shared_lib_registry = GlobalLibraryIRCache::instance().get_registry(hash);
            if (!shared_codegen_state)
                shared_codegen_state = GlobalLibraryIRCache::instance().get_codegen_state(hash);
        }

        // Parallel codegen of test files
        if (!tasks.empty()) {
            // Pending compile items for Phase 2
            struct PendingCompile {
                std::string ir_content; // LLVM IR string (in-memory, no .ll file)
                fs::path obj_path;
                std::string test_path;
                size_t task_index; // index into tasks[] for fallback re-codegen
                bool needs_compile;
            };
            std::vector<PendingCompile> pending_compiles;
            std::mutex pending_mutex;

            std::atomic<size_t> next_task{0};
            std::atomic<bool> has_error{false};
            std::string first_error_msg;
            std::string first_error_file;
            std::mutex error_mutex;

            unsigned int num_threads =
                exe_calc_codegen_threads(static_cast<unsigned int>(tasks.size()));

            types::preload_all_meta_caches();

            auto compile_task_worker = [&]() {
                // Pre-populate thread registry from shared lib typecheck results.
                // This avoids each thread re-loading all library modules from
                // GlobalModuleCache (which involves copying Module structs).
                auto thread_registry = std::make_shared<types::ModuleRegistry>(
                    shared_lib_registry ? shared_lib_registry->clone() : types::ModuleRegistry());

                while (!has_error.load()) {
                    size_t task_idx = next_task.fetch_add(1);
                    if (task_idx >= tasks.size())
                        break;

                    auto& task = tasks[task_idx];

                    TML_LOG_INFO("test", "  [exe] Processing test " << (task_idx + 1) << "/"
                                                                    << tasks.size() << ": "
                                                                    << task.file_path);

                    try {
                        using TClock = std::chrono::high_resolution_clock;

                        // Lex
                        auto lex_start = TClock::now();
                        auto source = lexer::Source::from_string(task.preprocessed, task.file_path);
                        lexer::Lexer lex(source);
                        auto tokens = lex.tokenize();
                        auto lex_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                          TClock::now() - lex_start)
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
                        auto parse_start = TClock::now();
                        parser::Parser parser(std::move(tokens));
                        auto module_name = fs::path(task.file_path).stem().string();
                        auto parse_result = parser.parse_module(module_name);
                        auto parse_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                            TClock::now() - parse_start)
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
                        auto tc_start = TClock::now();
                        types::TypeChecker checker;
                        checker.set_module_registry(thread_registry);
                        auto check_result = checker.check_module(module);
                        auto tc_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                         TClock::now() - tc_start)
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

                        // Borrow check (Polonius or NLL)
                        auto bc_start = TClock::now();
                        std::variant<bool, std::vector<borrow::BorrowError>> borrow_result;
                        if (CompilerOptions::polonius) {
                            borrow::polonius::PoloniusChecker polonius_checker(env);
                            borrow_result = polonius_checker.check_module(module);
                        } else {
                            borrow::BorrowChecker borrow_checker(env);
                            borrow_result = borrow_checker.check_module(module);
                        }
                        auto bc_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                         TClock::now() - bc_start)
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

                        // Codegen — same options as DLL path
                        auto cg_start = TClock::now();
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
                        options.lazy_library_defs = true;
                        // Pass pre-computed codegen state to skip emit_module_pure_tml_functions()
                        options.cached_library_state = shared_codegen_state;
                        codegen::LLVMIRGen llvm_gen(env, options);

                        auto gen_result = llvm_gen.generate(module);
                        auto cg_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                         TClock::now() - cg_start)
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

                        // Collect link libraries
                        {
                            std::lock_guard<std::mutex> lock(libs_mutex);
                            for (const auto& lib : llvm_gen.get_link_libs()) {
                                if (std::find(link_libs.begin(), link_libs.end(), lib) ==
                                    link_libs.end()) {
                                    link_libs.push_back(lib);
                                }
                            }
                        }

                        // Write IR
                        TML_LOG_INFO("test", "  [exe] "
                                                 << fs::path(task.file_path).filename().string()
                                                 << " lex=" << (lex_us / 1000) << "ms"
                                                 << " parse=" << (parse_us / 1000) << "ms"
                                                 << " typecheck=" << (tc_us / 1000) << "ms"
                                                 << " borrow=" << (bc_us / 1000) << "ms"
                                                 << " codegen=" << (cg_us / 1000) << "ms");

                        // Store IR string for later parallel compilation (no .ll on disk)
                        {
                            std::lock_guard<std::mutex> lock(pending_mutex);
                            pending_compiles.push_back({std::move(llvm_ir), task.obj_output,
                                                        task.file_path, task_idx, true});
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

            TML_LOG_INFO("test", "  [exe] Generating " << tasks.size() << " LLVM IR files with "
                                                       << num_threads << " threads...");

            constexpr size_t COMPILE_STACK = 32 * 1024 * 1024; // 32 MB for LLVM
            std::vector<tester::NativeThread> threads;
            for (unsigned int t = 0; t < std::min(num_threads, (unsigned int)tasks.size()); ++t) {
                threads.emplace_back(compile_task_worker, COMPILE_STACK);
            }
            for (auto& t : threads) {
                t.join();
            }

            if (has_error.load()) {
                result.error_message = first_error_msg;
                result.failed_test = first_error_file;
                return result;
            }

            // ======================================================================
            // PHASE 2: Parallel object compilation (.ll -> .obj)
            //          With per-file fallback on clang failure
            // ======================================================================

            auto phase2_start = Clock::now();

            if (!pending_compiles.empty()) {
                ObjectCompileOptions obj_options;
                obj_options.optimization_level = CompilerOptions::optimization_level;
                obj_options.debug_info = CompilerOptions::debug_info;
                obj_options.coverage = CompilerOptions::coverage_source;

                // Track files that fail compilation (need fallback re-codegen)
                struct FailedCompile {
                    size_t pending_index;
                    std::string error_msg;
                };
                std::vector<FailedCompile> failed_compiles;
                std::mutex failed_mutex;

                std::atomic<size_t> next_compile{0};

                unsigned int comp_threads =
                    exe_calc_codegen_threads(static_cast<unsigned int>(pending_compiles.size()));

                auto compile_worker = [&]() {
                    while (true) {
                        size_t idx = next_compile.fetch_add(1);
                        if (idx >= pending_compiles.size())
                            break;

                        auto& pc = pending_compiles[idx];
                        auto obj_result = compile_ir_string_to_object(pc.ir_content, pc.obj_path,
                                                                      clang, obj_options);

                        if (!obj_result.success) {
                            std::lock_guard<std::mutex> lock(failed_mutex);
                            failed_compiles.push_back({idx, obj_result.error_message});
                        }
                    }
                };

                TML_LOG_INFO("test", "  [exe] Compiling " << pending_compiles.size()
                                                          << " objects with " << comp_threads
                                                          << " threads...");

                constexpr size_t OBJ_COMPILE_STACK = 32 * 1024 * 1024; // 32 MB
                std::vector<tester::NativeThread> obj_threads;
                for (unsigned int t = 0;
                     t < std::min(comp_threads, (unsigned int)pending_compiles.size()); ++t) {
                    obj_threads.emplace_back(compile_worker, OBJ_COMPILE_STACK);
                }
                for (auto& t : obj_threads) {
                    t.join();
                }

                // ======================================================================
                // PHASE 2 FALLBACK: Parallel re-codegen of failed files without cached state
                // ======================================================================
                if (!failed_compiles.empty() && use_shared_lib && shared_codegen_state) {
                    TML_LOG_INFO(
                        "test",
                        "  [exe] " << failed_compiles.size()
                                   << " files failed with cached state, retrying without cache...");

                    std::atomic<bool> fallback_failed{false};
                    std::string fallback_error_msg;
                    std::string fallback_error_file;
                    std::mutex fb_error_mutex;

                    std::atomic<size_t> next_fb{0};

                    auto fallback_worker = [&]() {
                        auto fb_registry = std::make_shared<types::ModuleRegistry>(
                            shared_lib_registry ? shared_lib_registry->clone()
                                                : types::ModuleRegistry());

                        while (!fallback_failed.load()) {
                            size_t fb_idx = next_fb.fetch_add(1);
                            if (fb_idx >= failed_compiles.size())
                                break;

                            const auto& fc = failed_compiles[fb_idx];
                            const auto& pc = pending_compiles[fc.pending_index];
                            const auto& task = tasks[pc.task_index];

                            TML_LOG_INFO("test",
                                         "  [exe] Fallback re-codegen: "
                                             << fs::path(task.file_path).filename().string());

                            try {
                                auto source =
                                    lexer::Source::from_string(task.preprocessed, task.file_path);
                                lexer::Lexer lex(source);
                                auto tokens = lex.tokenize();
                                if (lex.has_errors()) {
                                    std::lock_guard<std::mutex> lock(fb_error_mutex);
                                    if (!fallback_failed.load()) {
                                        fallback_failed.store(true);
                                        fallback_error_msg =
                                            "Lexer errors in fallback for " + task.file_path;
                                        fallback_error_file = task.file_path;
                                    }
                                    continue;
                                }

                                parser::Parser parser(std::move(tokens));
                                auto module_name = fs::path(task.file_path).stem().string();
                                auto parse_result = parser.parse_module(module_name);
                                if (!std::holds_alternative<parser::Module>(parse_result)) {
                                    std::lock_guard<std::mutex> lock(fb_error_mutex);
                                    if (!fallback_failed.load()) {
                                        fallback_failed.store(true);
                                        fallback_error_msg =
                                            "Parse errors in fallback for " + task.file_path;
                                        fallback_error_file = task.file_path;
                                    }
                                    continue;
                                }
                                const auto& module = std::get<parser::Module>(parse_result);

                                types::TypeChecker checker;
                                checker.set_module_registry(fb_registry);
                                auto check_result = checker.check_module(module);
                                if (!std::holds_alternative<types::TypeEnv>(check_result)) {
                                    std::lock_guard<std::mutex> lock(fb_error_mutex);
                                    if (!fallback_failed.load()) {
                                        fallback_failed.store(true);
                                        fallback_error_msg =
                                            "Type errors in fallback for " + task.file_path;
                                        fallback_error_file = task.file_path;
                                    }
                                    continue;
                                }
                                const auto& env = std::get<types::TypeEnv>(check_result);

                                std::variant<bool, std::vector<borrow::BorrowError>> borrow_result;
                                if (CompilerOptions::polonius) {
                                    borrow::polonius::PoloniusChecker polonius_checker(env);
                                    borrow_result = polonius_checker.check_module(module);
                                } else {
                                    borrow::BorrowChecker borrow_checker(env);
                                    borrow_result = borrow_checker.check_module(module);
                                }
                                if (std::holds_alternative<std::vector<borrow::BorrowError>>(
                                        borrow_result)) {
                                    std::lock_guard<std::mutex> lock(fb_error_mutex);
                                    if (!fallback_failed.load()) {
                                        fallback_failed.store(true);
                                        fallback_error_msg =
                                            "Borrow errors in fallback for " + task.file_path;
                                        fallback_error_file = task.file_path;
                                    }
                                    continue;
                                }

                                // Codegen WITHOUT cached_library_state and library_decls_only
                                codegen::LLVMGenOptions fb_options;
                                fb_options.emit_comments = false;
                                fb_options.generate_dll_entry = true;
                                fb_options.suite_test_index = static_cast<int>(task.index);
                                fb_options.suite_total_tests = static_cast<int>(suite.tests.size());
                                fb_options.dll_export = true;
                                fb_options.force_internal_linkage = true;
                                fb_options.library_decls_only = false;
                                fb_options.emit_debug_info = CompilerOptions::debug_info;
                                fb_options.debug_level = CompilerOptions::debug_level;
                                fb_options.source_file = task.file_path;
                                fb_options.coverage_enabled = CompilerOptions::coverage;
                                fb_options.coverage_quiet = CompilerOptions::coverage;
                                fb_options.coverage_output_file = CompilerOptions::coverage_output;
                                fb_options.llvm_source_coverage = CompilerOptions::coverage_source;
                                fb_options.lazy_library_defs = true;
                                codegen::LLVMIRGen llvm_gen(env, fb_options);
                                auto gen_result = llvm_gen.generate(module);

                                if (!std::holds_alternative<std::string>(gen_result)) {
                                    std::lock_guard<std::mutex> lock(fb_error_mutex);
                                    if (!fallback_failed.load()) {
                                        fallback_failed.store(true);
                                        fallback_error_msg =
                                            "Codegen errors in fallback for " + task.file_path;
                                        fallback_error_file = task.file_path;
                                    }
                                    continue;
                                }

                                const auto& llvm_ir = std::get<std::string>(gen_result);

                                {
                                    std::lock_guard<std::mutex> lock(libs_mutex);
                                    for (const auto& lib : llvm_gen.get_link_libs()) {
                                        if (std::find(link_libs.begin(), link_libs.end(), lib) ==
                                            link_libs.end()) {
                                            link_libs.push_back(lib);
                                        }
                                    }
                                }

                                auto obj_result = compile_ir_string_to_object(llvm_ir, pc.obj_path,
                                                                              clang, obj_options);

                                if (!obj_result.success) {
                                    std::lock_guard<std::mutex> lock(fb_error_mutex);
                                    if (!fallback_failed.load()) {
                                        fallback_failed.store(true);
                                        fallback_error_msg = "Fallback compilation failed for " +
                                                             task.file_path + ": " +
                                                             obj_result.error_message;
                                        fallback_error_file = task.file_path;
                                    }
                                    continue;
                                }

                                TML_LOG_INFO("test",
                                             "  [exe] Fallback succeeded: "
                                                 << fs::path(task.file_path).filename().string());

                            } catch (const std::exception& e) {
                                std::lock_guard<std::mutex> lock(fb_error_mutex);
                                if (!fallback_failed.load()) {
                                    fallback_failed.store(true);
                                    fallback_error_msg = "Fallback exception for " +
                                                         task.file_path + ": " + e.what();
                                    fallback_error_file = task.file_path;
                                }
                            }
                        }
                    };

                    // Run fallbacks in parallel
                    unsigned int fb_threads =
                        exe_calc_codegen_threads(static_cast<unsigned int>(failed_compiles.size()));
                    constexpr size_t FB_COMPILE_STACK = 32 * 1024 * 1024; // 32 MB
                    std::vector<tester::NativeThread> fb_thread_pool;
                    for (unsigned int t = 0;
                         t < std::min(fb_threads, (unsigned int)failed_compiles.size()); ++t) {
                        fb_thread_pool.emplace_back(fallback_worker, FB_COMPILE_STACK);
                    }
                    for (auto& t : fb_thread_pool) {
                        t.join();
                    }

                    if (fallback_failed.load()) {
                        result.error_message = fallback_error_msg;
                        result.failed_test = fallback_error_file;
                        return result;
                    }
                } else if (!failed_compiles.empty()) {
                    const auto& fc = failed_compiles[0];
                    const auto& pc = pending_compiles[fc.pending_index];
                    result.error_message =
                        "Object compilation failed for " + pc.test_path + ": " + fc.error_msg;
                    result.failed_test = pc.test_path;
                    return result;
                }
            }

            phase2_time_us =
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase1_start)
                    .count();
        }

        phase1_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase1_start)
                .count();

        // ======================================================================
        // PHASE 2b: Generate and compile dispatcher main()
        // ======================================================================

        auto dispatcher_start = Clock::now();

        std::string dispatcher_ir =
            generate_dispatcher_ir(static_cast<int>(suite.tests.size()), suite.name);

        std::string disp_hash = generate_content_hash(dispatcher_ir + ":exe_v2");
        fs::path dispatcher_obj = cache_dir / (disp_hash + "_dispatcher" + get_object_extension());

        if (no_cache || !fs::exists(dispatcher_obj)) {
            ObjectCompileOptions disp_obj_opts;
            disp_obj_opts.optimization_level = CompilerOptions::optimization_level;
            disp_obj_opts.debug_info = false;

            auto disp_result =
                compile_ir_string_to_object(dispatcher_ir, dispatcher_obj, clang, disp_obj_opts);

            if (!disp_result.success) {
                result.error_message =
                    "Dispatcher compilation failed: " + disp_result.error_message;
                return result;
            }
        }

        object_files.push_back(dispatcher_obj);

        TML_LOG_INFO("test", "  [exe] Dispatcher compiled in "
                                 << (std::chrono::duration_cast<std::chrono::microseconds>(
                                         Clock::now() - dispatcher_start)
                                         .count() /
                                     1000)
                                 << "ms");

        // ======================================================================
        // Get runtime objects
        // ======================================================================

        auto runtime_start = Clock::now();

        for (const auto& path : imported_module_paths) {
            if (!shared_registry->has_module(path)) {
                types::Module placeholder;
                placeholder.name = path;
                shared_registry->register_module(path, std::move(placeholder));
            }
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

        // Parse first file for get_runtime_objects
        const auto& first_pp = preprocessed_sources[0];
        auto source = lexer::Source::from_string(first_pp.preprocessed, first_pp.file_path);
        lexer::Lexer lex(source);
        auto tokens = lex.tokenize();
        parser::Parser parser(std::move(tokens));
        auto parse_result = parser.parse_module(fs::path(first_pp.file_path).stem().string());
        const auto& module = std::get<parser::Module>(parse_result);

        std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

        TML_LOG_INFO("test", "  [exe] Getting runtime objects...");
        auto runtime_objects =
            get_runtime_objects(shared_registry, module, deps_cache, clang, false);
        TML_LOG_INFO("test", "  [exe] Got " << runtime_objects.size() << " runtime objects");
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

        if (use_shared_lib && fs::exists(shared_lib_obj)) {
            object_files.push_back(shared_lib_obj);
            TML_LOG_INFO("test", "  [exe] Using shared library: " << shared_lib_obj.filename());
        }

        runtime_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - runtime_start)
                .count();

        // ======================================================================
        // PHASE 3: Link as EXECUTABLE (not DLL)
        // ======================================================================

        std::string suite_hash = generate_content_hash(combined_hash);
        std::string exe_hash = generate_exe_hash(suite_hash, object_files);
        fs::path cached_exe = cache_dir / (exe_hash + "_exe.exe");

        bool use_cached_exe = !no_cache && fs::exists(cached_exe);

        auto link_start = Clock::now();

        if (!use_cached_exe) {
            LinkOptions link_options;
            link_options.output_type = LinkOptions::OutputType::Executable;
            link_options.verbose = false;
            link_options.coverage = CompilerOptions::coverage_source;

            for (const auto& lib : link_libs) {
                if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                    link_options.link_flags.push_back("\"" + lib + "\"");
                } else {
                    link_options.link_flags.push_back("-l" + lib);
                }
            }

#ifndef _WIN32
            // Unix system libraries (macOS clang links libSystem automatically)
  #ifndef __APPLE__
            link_options.link_flags.push_back("-lm");
            link_options.link_flags.push_back("-lpthread");
            link_options.link_flags.push_back("-ldl");
  #endif
            {
                auto openssl = build::find_openssl();
                if (openssl.found) {
                    link_options.link_flags.push_back("-L" + to_forward_slashes(openssl.lib_dir.string()));
                    link_options.link_flags.push_back("-lssl");
                    link_options.link_flags.push_back("-lcrypto");
                }
            }
            link_options.link_flags.push_back("-lz");
#endif

            TML_LOG_INFO("test", "  [exe] Starting link...");
            auto link_result = link_objects(object_files, cached_exe, clang, link_options);
            TML_LOG_INFO("test", "  [exe] Link complete");

            if (!link_result.success) {
                result.error_message = "Linking failed: " + link_result.error_message;
                return result;
            }
        }

        link_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - link_start)
                .count();

        // Save source-hash cache
        if (!fs::exists(cached_exe_by_source)) {
            try {
                fs::copy_file(cached_exe, cached_exe_by_source,
                              fs::copy_options::overwrite_existing);
            } catch (...) {}
        }

        // Copy to output location
        if (!fast_copy_file(cached_exe, exe_output)) {
            result.error_message = "Failed to copy EXE";
            return result;
        }

        auto end = Clock::now();
        result.success = true;
        result.exe_path = exe_output.string();
        result.compile_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        if (verbose) {
            // Show test file names instead of generic suite name
            std::string test_names;
            for (size_t i = 0; i < suite.tests.size(); ++i) {
                if (i > 0)
                    test_names += ", ";
                test_names += fs::path(suite.tests[i].file_path).filename().string();
            }
            TML_LOG_INFO("test", "[exe] Compiled: "
                                     << test_names << " timing: preprocess="
                                     << (preprocess_time_us / 1000) << "ms"
                                     << " phase1=" << (phase1_time_us / 1000) << "ms"
                                     << " phase2=" << (phase2_time_us / 1000) << "ms"
                                     << " runtime=" << (runtime_time_us / 1000) << "ms"
                                     << " link=" << (link_time_us / 1000) << "ms"
                                     << " total=" << (result.compile_time_us / 1000) << "ms");
        }

        return result;

    } catch (const std::exception& e) {
        result.error_message =
            "FATAL EXCEPTION during EXE suite compilation: " + std::string(e.what());
        if (!suite.tests.empty()) {
            result.failed_test = suite.tests[0].file_path;
        }
        TML_LOG_FATAL("test", "[exe] Exception in compile_test_suite_exe: " << e.what());
        return result;
    } catch (...) {
        result.error_message = "FATAL UNKNOWN EXCEPTION during EXE suite compilation";
        if (!suite.tests.empty()) {
            result.failed_test = suite.tests[0].file_path;
        }
        TML_LOG_FATAL("test", "[exe] Unknown exception in compile_test_suite_exe");
        return result;
    }
}

} // namespace tml::cli
