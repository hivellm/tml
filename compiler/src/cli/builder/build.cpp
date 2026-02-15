//! # Build Command Implementation
//!
//! This file implements the main `tml build` command that compiles TML source
//! files into executables, libraries, or other output formats.
//!
//! ## Compilation Pipeline
//!
//! ```text
//! run_build()
//!   ├─ Read source file
//!   ├─ Lexer::tokenize()        → Tokens
//!   ├─ Parser::parse_module()   → AST (Module)
//!   ├─ TypeChecker::check()     → TypeEnv
//!   ├─ BorrowChecker::check()   → Ownership validation
//!   ├─ LLVMIRGen::generate()    → LLVM IR (.ll)
//!   ├─ compile_ll_to_object()   → Object file (.obj/.o)
//!   └─ link_objects()           → Final output (.exe/.dll/.rlib)
//! ```
//!
//! ## Caching
//!
//! Object files are cached in `build/debug/.cache/` based on:
//! - Source file modification time
//! - Compiler options (optimization level, debug info)
//!
//! Use `--no-cache` to force recompilation.

#include "builder_internal.hpp"
#include "codegen/codegen_backend.hpp"
#include "codegen/codegen_partitioner.hpp"
#include "query/query_context.hpp"
#include "types/module_binary.hpp"

namespace tml::cli {

// Using helpers from builder namespace
using namespace build;

// Internal implementation that takes BuildOptions
static int run_build_impl(const std::string& path, const BuildOptions& options) {
    // Pre-load all library modules from .tml.meta binary cache
    types::preload_all_meta_caches();

    bool verbose = options.verbose;
    bool emit_ir_only = options.emit_ir_only;
    bool emit_mir = options.emit_mir;
    bool no_cache = options.no_cache;
    BuildOutputType output_type = options.output_type;
    bool emit_header = options.emit_header;
    const std::string& output_dir = options.output_dir;

    // Try to load tml.toml manifest
    auto manifest_opt = Manifest::load_from_current_dir();
    if (manifest_opt) {
        TML_LOG_INFO("build",
                     "Found tml.toml manifest for project: " << manifest_opt->package.name);
    }

    // Apply manifest settings if available (command-line flags override)
    // Note: Command-line parameters are already set, so we only apply defaults from manifest
    if (manifest_opt && !manifest_opt->build.validate()) {
        TML_LOG_WARN("build", "Invalid build settings in tml.toml, using defaults");
    }

    // Initialize diagnostic emitter for Rust-style error output
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        TML_LOG_ERROR("build", e.what());
        return 1;
    }

    // Run preprocessor to handle #if/#define/#ifdef etc.
    auto preproc_result = preprocess_source(source_code, path, options);

    // Emit preprocessor diagnostics (errors and warnings)
    emit_all_preprocessor_diagnostics(diag, preproc_result, path);

    if (!preproc_result.success()) {
        return 1;
    }

    // Use preprocessed source for compilation
    std::string preprocessed_source = preproc_result.output;

    // Register source content with diagnostic emitter for source snippets
    diag.set_source_content(path, preprocessed_source);

    auto source = lexer::Source::from_string(preprocessed_source, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        emit_all_lexer_errors(diag, lex);
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        emit_all_parser_errors(diag, errors);
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Initialize module registry
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    // Set source directory for local module resolution (use <module_name> to import sibling .tml
    // files)
    auto source_dir = fs::path(path).parent_path();
    if (source_dir.empty()) {
        source_dir = fs::current_path();
    }
    checker.set_source_directory(source_dir.string());
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        emit_all_type_errors(diag, errors);
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    // Run borrow checker (ownership and borrowing validation)
    // Use Polonius if --polonius flag is set, otherwise use default NLL checker
    if (options.polonius) {
        borrow::polonius::PoloniusChecker polonius_checker(env);
        auto borrow_result = polonius_checker.check_module(module);
        if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
            const auto& errors = std::get<std::vector<borrow::BorrowError>>(borrow_result);
            emit_all_borrow_errors(diag, errors);
            return 1;
        }
    } else {
        borrow::BorrowChecker borrow_checker(env);
        auto borrow_result = borrow_checker.check_module(module);
        if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
            const auto& errors = std::get<std::vector<borrow::BorrowError>>(borrow_result);
            emit_all_borrow_errors(diag, errors);
            return 1;
        }
    }

    // Emit MIR if requested (early exit before LLVM codegen)
    if (emit_mir) {
        mir::Module mir_module;

        // Use new HIR pipeline: AST -> HIR -> MIR
        // This provides better type information and enables more optimizations
        // Make a mutable copy of env for HirBuilder (which needs non-const access)
        auto env_copy = env;
        hir::HirBuilder hir_builder(env_copy);
        auto hir_module = hir_builder.lower_module(module);

        TML_LOG_INFO("build", "HIR: Built " << hir_module.functions.size() << " functions, "
                                            << hir_module.structs.size() << " structs, "
                                            << hir_module.enums.size() << " enums");

        mir::HirMirBuilder hir_mir_builder(env);
        mir_module = hir_mir_builder.build(hir_module);

        // Run infinite loop detection (early static analysis)
        // This is a compile-time error - infinite loops are not allowed
        mir::InfiniteLoopCheckPass loop_check;
        loop_check.run(mir_module);
        if (loop_check.has_warnings()) {
            // Print errors (not warnings) for infinite loops
            for (const auto& warning : loop_check.get_warnings()) {
                TML_LOG_ERROR("build", "potential infinite loop in function '"
                                           << warning.function_name << "' at block '"
                                           << warning.block_name << "': " << warning.reason
                                           << " -- infinite loops are not allowed - add a break "
                                              "condition or return statement");
            }
            return 1; // Fail compilation
        }

        // Run memory leak detection (early static analysis)
        // This is a compile-time error - memory leaks are not allowed
        mir::MemoryLeakCheckPass leak_check;
        leak_check.run(mir_module);
        if (leak_check.has_errors()) {
            leak_check.print_warnings();
            return 1; // Fail compilation
        }

        // Apply MIR optimizations based on optimization level
        int opt_level = tml::CompilerOptions::optimization_level;
        if (opt_level > 0) {
            mir::OptLevel mir_opt = mir::OptLevel::O0;
            if (opt_level == 1)
                mir_opt = mir::OptLevel::O1;
            else if (opt_level == 2)
                mir_opt = mir::OptLevel::O2;
            else if (opt_level >= 3)
                mir_opt = mir::OptLevel::O3;

            mir::PassManager pm(mir_opt);
            pm.configure_standard_pipeline(env_copy); // Use OOP-optimized pipeline

            // PGO instrumentation for --profile-generate
            if (options.profile_generate) {
                mir::ProfileInstrumentationPass inst_pass;
                inst_pass.run(mir_module);
                auto stats = inst_pass.get_stats();
                TML_LOG_INFO("build", "PGO instrumentation: " << stats.functions_profiled
                                                              << " functions instrumented");
            }

            int passes_changed = pm.run(mir_module);
            if (passes_changed > 0) {
                TML_LOG_INFO("build", "MIR optimization: " << passes_changed << " passes applied");
            }

            // Apply PGO when using profile data
            if (!options.profile_use.empty()) {
                auto profile_opt = mir::ProfileData::load(options.profile_use);
                if (profile_opt && mir::ProfileIO::validate(*profile_opt, mir_module)) {
                    mir::PgoPass pgo_pass(*profile_opt);
                    pgo_pass.run(mir_module);
                }
            }
        }

        // Use build directory structure
        fs::path build_dir =
            output_dir.empty() ? get_build_dir(false /* debug */) : fs::path(output_dir);
        fs::create_directories(build_dir);

        fs::path mir_output = build_dir / (module_name + ".mir");
        std::ofstream mir_file(mir_output);
        if (!mir_file) {
            TML_LOG_ERROR("build", "Cannot write to " << mir_output);
            return 1;
        }
        mir_file << mir::print_module(mir_module);
        mir_file.close();

        TML_LOG_INFO("build", "emit-mir: " << to_forward_slashes(mir_output.string()));
        return 0;
    }

    std::string llvm_ir;
    std::set<std::string> link_libs; // FFI libraries to link
    bool used_mir_codegen = false;
    std::optional<mir::Module> saved_mir_module;
    codegen::MirCodegenOptions saved_mir_opts;

    // Check if there are imported TML modules that need codegen
    // MIR codegen doesn't support imported module functions yet, so we need to use AST codegen
    bool has_tml_imports_needing_codegen = false;
    if (env.module_registry()) {
        for (const auto& [mod_name, mod] : env.module_registry()->get_all_modules()) {
            if (mod.has_pure_tml_functions && !mod.source_code.empty()) {
                has_tml_imports_needing_codegen = true;
                break;
            }
        }
    }

    // Check if there are local generic types that need instantiation
    // MIR codegen doesn't support generic type instantiation, so we need AST codegen
    bool has_local_generics = false;
    for (const auto& decl : module.decls) {
        if (decl->is<parser::StructDecl>()) {
            if (!decl->as<parser::StructDecl>().generics.empty()) {
                has_local_generics = true;
                break;
            }
        } else if (decl->is<parser::EnumDecl>()) {
            if (!decl->as<parser::EnumDecl>().generics.empty()) {
                has_local_generics = true;
                break;
            }
        } else if (decl->is<parser::ImplDecl>()) {
            const auto& impl = decl->as<parser::ImplDecl>();
            if (!impl.generics.empty()) {
                has_local_generics = true;
                break;
            }
            if (impl.self_type && impl.self_type->is<parser::NamedType>()) {
                const auto& named = impl.self_type->as<parser::NamedType>();
                if (named.generics.has_value() && !named.generics->args.empty()) {
                    has_local_generics = true;
                    break;
                }
            }
        }
    }

    // Use MIR-based codegen for all optimization levels (including O0)
    // This provides consistent behavior for features like volatile that are implemented in MIR
    // But fall back to AST codegen when TML imports need codegen (MIR doesn't support this yet)
    // Also fall back to AST codegen when there are local generic types
    // Also fall back to AST codegen when --emit-ir is requested (AST produces more complete IR)
    int opt_level = tml::CompilerOptions::optimization_level;
    if (!has_tml_imports_needing_codegen && !has_local_generics &&
        !emit_ir_only) { // Use MIR when safe
        // Build MIR from HIR for optimized codegen
        auto env_copy = env;
        hir::HirBuilder hir_builder(env_copy);
        auto hir_module = hir_builder.lower_module(module);

        TML_LOG_INFO("build", "HIR: Built " << hir_module.functions.size() << " functions, "
                                            << hir_module.structs.size() << " structs, "
                                            << hir_module.enums.size() << " enums");

        mir::HirMirBuilder hir_mir_builder(env);
        auto mir_module = hir_mir_builder.build(hir_module);

        // Run infinite loop detection (early static analysis)
        // This is a compile-time error - infinite loops are not allowed
        mir::InfiniteLoopCheckPass loop_check;
        loop_check.run(mir_module);
        if (loop_check.has_warnings()) {
            // Print errors (not warnings) for infinite loops
            for (const auto& warning : loop_check.get_warnings()) {
                TML_LOG_ERROR("build", "potential infinite loop in function '"
                                           << warning.function_name << "' at block '"
                                           << warning.block_name << "': " << warning.reason
                                           << " -- infinite loops are not allowed - add a break "
                                              "condition or return statement");
            }
            return 1; // Infinite loop detected - compilation aborted
        }

        // Run memory leak detection (early static analysis)
        // This is a compile-time error - memory leaks are not allowed
        // Skip when --emit-ir is requested (user wants to inspect IR, not build)
        if (!emit_ir_only) {
            mir::MemoryLeakCheckPass leak_check;
            leak_check.run(mir_module);
            if (leak_check.has_errors()) {
                leak_check.print_warnings();
                return 1; // Memory leak detected - compilation aborted
            }
        }

        // Apply MIR optimizations
        mir::OptLevel mir_opt = mir::OptLevel::O0;
        if (opt_level == 1)
            mir_opt = mir::OptLevel::O1;
        else if (opt_level == 2)
            mir_opt = mir::OptLevel::O2;
        else if (opt_level >= 3)
            mir_opt = mir::OptLevel::O3;

        mir::PassManager pm(mir_opt);
        pm.configure_standard_pipeline(env_copy); // Use OOP-optimized pipeline with inlining fix

        // Profile-Guided Optimization: Add instrumentation pass for --profile-generate
        if (options.profile_generate) {
            mir::ProfileInstrumentationPass inst_pass;
            inst_pass.run(mir_module);
            auto stats = inst_pass.get_stats();
            TML_LOG_INFO("build", "PGO instrumentation: " << stats.functions_profiled
                                                          << " functions instrumented");
        }

        // Profile-Guided Optimization: Load profile data and pass to InliningPass
        std::optional<mir::ProfileData> loaded_profile;
        if (!options.profile_use.empty()) {
            loaded_profile = mir::ProfileData::load(options.profile_use);
            if (loaded_profile) {
                TML_LOG_INFO("build", "PGO: Loaded profile from " << options.profile_use);
                // Pass profile data to InliningPass in the pipeline
                pm.set_profile_data(&*loaded_profile);
            } else {
                TML_LOG_ERROR("build", "Cannot load profile data from " << options.profile_use);
                return 1;
            }
        }

        int passes_changed = pm.run(mir_module);
        if (passes_changed > 0) {
            TML_LOG_INFO("build", "MIR optimization: " << passes_changed << " passes applied");
        }

        // Apply additional PGO passes (branch hints, block layout) after inlining
        if (loaded_profile && mir::ProfileIO::validate(*loaded_profile, mir_module)) {
            mir::PgoPass pgo_pass(*loaded_profile);
            if (pgo_pass.run(mir_module)) {
                auto stats = pgo_pass.get_stats();
                TML_LOG_INFO("build",
                             "PGO applied: " << stats.branch_hints_applied << " branch hints, "
                                             << stats.blocks_reordered << " blocks reordered, "
                                             << stats.hot_functions << " hot functions identified");
            }
        }

        // Generate LLVM IR from optimized MIR
        codegen::MirCodegenOptions mir_opts;
        mir_opts.emit_comments = verbose;
#ifdef _WIN32
        mir_opts.dll_export = (output_type == BuildOutputType::DynamicLib);
        mir_opts.target_triple = "x86_64-pc-windows-msvc";
#else
        mir_opts.target_triple = "x86_64-unknown-linux-gnu";
#endif
        if (!CompilerOptions::target_triple.empty()) {
            mir_opts.target_triple = CompilerOptions::target_triple;
        }

        codegen::MirCodegen mir_codegen(mir_opts);
        llvm_ir = mir_codegen.generate(mir_module);

        TML_LOG_INFO("build", "Generated LLVM IR from optimized MIR ("
                                  << mir_module.functions.size() << " functions)");

        // Save MIR for CGU partitioning in object compilation stage
        used_mir_codegen = true;
        saved_mir_opts = mir_opts;
        saved_mir_module = std::move(mir_module);

        // Extract link_libs from AST @extern/@link decorated functions
        // MIR codegen doesn't track these, so we extract them directly from AST
        for (const auto& decl : module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func = decl->as<parser::FuncDecl>();
                for (const auto& lib : func.link_libs) {
                    link_libs.insert(lib);
                }
            }
        }
    } else {
        // Use AST-based codegen for O0 (no optimizations)
        codegen::LLVMGenOptions llvm_gen_options;
        llvm_gen_options.emit_comments = verbose;
        llvm_gen_options.emit_debug_info = CompilerOptions::debug_info;
        llvm_gen_options.debug_level = CompilerOptions::debug_level;
        llvm_gen_options.coverage_enabled = CompilerOptions::coverage;
        llvm_gen_options.coverage_output_file = CompilerOptions::coverage_output;
        llvm_gen_options.source_file = path;
        if (!CompilerOptions::target_triple.empty()) {
            llvm_gen_options.target_triple = CompilerOptions::target_triple;
        }
#ifdef _WIN32
        // Enable DLL export for dynamic libraries on Windows
        llvm_gen_options.dll_export = (output_type == BuildOutputType::DynamicLib);
#endif
        codegen::LLVMIRGen llvm_gen(env, llvm_gen_options);

        auto gen_result = llvm_gen.generate(module);
        if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
            const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
            emit_all_codegen_errors(diag, errors);
            return 1;
        }

        llvm_ir = std::get<std::string>(gen_result);

        // Get FFI link libraries from AST codegen
        link_libs = llvm_gen.get_link_libs();
    }

    // Use build directory structure (like Rust's target/)
    // If output_dir is specified, use it; otherwise use default build directory
    fs::path build_dir =
        output_dir.empty() ? get_build_dir(false /* debug */) : fs::path(output_dir);
    fs::create_directories(build_dir); // Ensure custom output directory exists

    fs::path ll_output = build_dir / (module_name + ".ll");
    fs::path exe_output = build_dir / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    // Only write .ll file if explicitly requested via --emit-ir
    if (emit_ir_only) {
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            TML_LOG_ERROR("build", "Cannot write to " << ll_output);
            return 1;
        }
        ll_file << llvm_ir;
        ll_file.close();
        TML_LOG_INFO("build", "emit-ir: " << ll_output);
        return 0;
    }

    std::string clang = find_clang();

    // Create deps cache directory for precompiled runtimes
    fs::path deps_dir = build_dir / "deps";
    fs::create_directories(deps_dir);
    std::string deps_cache = to_forward_slashes(deps_dir.string());

    // Create .cache directory for object files
    fs::path cache_dir = build_dir / ".cache";
    fs::create_directories(cache_dir);

    // Step 1: Compile LLVM IR (.ll) to object file(s) (.o/.obj)
    ObjectCompileOptions obj_options;
    obj_options.optimization_level = tml::CompilerOptions::optimization_level;
    obj_options.debug_info = tml::CompilerOptions::debug_info;
    obj_options.verbose = verbose;
    obj_options.target_triple = tml::CompilerOptions::target_triple;
    obj_options.sysroot = tml::CompilerOptions::sysroot;

    // Collect all object files to link
    std::vector<fs::path> object_files;

    // CGU partitioning: split into N codegen units for incremental object caching
    bool use_cgu = used_mir_codegen && saved_mir_module.has_value() &&
                   saved_mir_module->functions.size() >= 2 && !no_cache;

    if (use_cgu) {
        // Partition MIR into codegen units
        bool is_release = (opt_level >= 2);
        codegen::PartitionOptions part_opts;
        part_opts.num_cgus = is_release ? 4 : 16;
        part_opts.codegen_opts = saved_mir_opts;

        codegen::CodegenPartitioner partitioner(part_opts);
        auto partition_result = partitioner.partition(*saved_mir_module);

        if (!partition_result.success) {
            TML_LOG_ERROR("build", "CGU partitioning failed: " << partition_result.error_message);
            return 1;
        }

        TML_LOG_INFO("build",
                     "CGU: Partitioned into " << partition_result.cgus.size() << " codegen units");

        int cache_hits = 0;
        std::string obj_ext = get_object_extension();

        // Separate CGUs into cached (reuse) and uncached (need compilation)
        std::vector<CGUCompileJob> compile_jobs;
        // Track insertion order: pair<is_cached, index_into_cached_or_jobs>
        struct CGUSlot {
            bool cached;
            size_t index;
        };
        std::vector<CGUSlot> slots;

        std::vector<fs::path> cached_objects;

        for (const auto& cgu : partition_result.cgus) {
            std::string fp12 = cgu.fingerprint.substr(0, 12);
            std::string cgu_name =
                module_name + ".cgu" + std::to_string(cgu.cgu_index) + "." + fp12 + obj_ext;
            fs::path cgu_obj_path = cache_dir / cgu_name;

            if (fs::exists(cgu_obj_path)) {
                slots.push_back({true, cached_objects.size()});
                cached_objects.push_back(cgu_obj_path);
                cache_hits++;
                TML_LOG_INFO("build", "CGU " << cgu.cgu_index << ": cache hit (" << fp12 << ")");
            } else {
                CGUCompileJob job;
                job.ir_content = cgu.llvm_ir;
                job.output_path = cgu_obj_path;
                job.cgu_index = cgu.cgu_index;
                job.fingerprint_tag = fp12;
                slots.push_back({false, compile_jobs.size()});
                compile_jobs.push_back(std::move(job));
            }
        }

        // Compile uncached CGUs in parallel
        if (!compile_jobs.empty()) {
            auto batch_result = compile_cgus_parallel(compile_jobs, clang, obj_options);
            if (!batch_result.success) {
                for (const auto& err : batch_result.errors) {
                    TML_LOG_ERROR("build", err);
                }
                return 1;
            }

            // Reassemble object files in original CGU order
            for (const auto& slot : slots) {
                if (slot.cached) {
                    object_files.push_back(cached_objects[slot.index]);
                } else {
                    object_files.push_back(batch_result.object_files[slot.index]);
                }
            }
        } else {
            // All cached
            for (const auto& obj : cached_objects) {
                object_files.push_back(obj);
            }
        }

        TML_LOG_INFO("build",
                     "CGU: " << cache_hits << " cached, " << compile_jobs.size() << " compiled");
    } else {
        // Monolithic path (AST codegen, single function, or --no-cache)
        fs::path obj_output = cache_dir / (module_name + get_object_extension());

        // Check if cached object file is valid (unless --no-cache is set)
        bool use_cached_obj = false;
        if (!no_cache && fs::exists(obj_output)) {
            try {
                auto src_time = fs::last_write_time(fs::path(path));
                auto obj_time = fs::last_write_time(obj_output);
                if (obj_time >= src_time) {
                    use_cached_obj = true;
                    TML_LOG_INFO("build", "Using cached object file: " << obj_output);
                }
            } catch (...) {
                // If we can't check timestamps, recompile
            }
        }

        ObjectCompileResult obj_result;
        if (use_cached_obj) {
            obj_result.success = true;
            obj_result.object_file = obj_output;
        } else {
            obj_result = compile_ir_string_to_object(llvm_ir, obj_output, clang, obj_options);
            if (!obj_result.success) {
                TML_LOG_ERROR("build", obj_result.error_message);
                return 1;
            }
            TML_LOG_INFO("build", "Generated: " << obj_result.object_file);
        }

        object_files.push_back(obj_result.object_file);
    }

    // Add runtime object files only for executables (not for libraries)
    if (output_type == BuildOutputType::Executable) {
        auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());
    }

    // Step 3: Determine output file extension based on output type
    fs::path final_output;
    LinkOptions::OutputType link_output_type = LinkOptions::OutputType::Executable;

    switch (output_type) {
    case BuildOutputType::Executable:
        final_output = exe_output; // Already has .exe extension
        link_output_type = LinkOptions::OutputType::Executable;
        break;
    case BuildOutputType::StaticLib:
#ifdef _WIN32
        final_output = build_dir / (module_name + ".lib");
#else
        final_output = build_dir / ("lib" + module_name + ".a");
#endif
        link_output_type = LinkOptions::OutputType::StaticLib;
        break;
    case BuildOutputType::DynamicLib:
#ifdef _WIN32
        final_output = build_dir / (module_name + ".dll");
#else
#ifdef __APPLE__
        final_output = build_dir / ("lib" + module_name + ".dylib");
#else
        final_output = build_dir / ("lib" + module_name + ".so");
#endif
#endif
        link_output_type = LinkOptions::OutputType::DynamicLib;
        break;
    case BuildOutputType::RlibLib:
        final_output = build_dir / (module_name + ".rlib");
        // RLIB doesn't use standard linking - we'll create it separately
        break;
    }

    // For RLIB: Create RLIB archive instead of linking
    if (output_type == BuildOutputType::RlibLib) {
        // Create metadata
        RlibMetadata metadata;
        metadata.format_version = "1.0";
        metadata.library.name = module_name;

        // Try to read version from manifest (tml.toml)
        std::string version = "0.1.0";
        fs::path manifest_path = fs::path(path).parent_path() / "tml.toml";
        if (fs::exists(manifest_path)) {
            if (auto manifest = Manifest::load(manifest_path)) {
                version = manifest->package.version;
                if (!manifest->package.name.empty()) {
                    metadata.library.name = manifest->package.name;
                }
            }
        }
        metadata.library.version = version;
        metadata.library.tml_version = "0.1.0";

        // Add module information
        RlibModule rlib_module;
        rlib_module.name = module_name;
        rlib_module.file = object_files[0].filename().string();
        rlib_module.hash = calculate_file_hash(fs::path(path));

        // Extract exports from the module with full type information
        for (const auto& decl : module.decls) {
            if (decl->is<parser::FuncDecl>()) {
                const auto& func_decl = decl->as<parser::FuncDecl>();
                if (func_decl.vis == parser::Visibility::Public) {
                    RlibExport exp;
                    exp.name = func_decl.name;
                    exp.symbol = "tml_" + func_decl.name; // Simple mangling

                    // Build type signature: func(T1, T2, ...) -> RetType
                    std::string type_sig = "func(";
                    for (size_t i = 0; i < func_decl.params.size(); ++i) {
                        if (i > 0)
                            type_sig += ", ";
                        // Extract type name from parameter
                        if (func_decl.params[i].type) {
                            type_sig += type_to_string(*func_decl.params[i].type);
                        } else {
                            type_sig += "_";
                        }
                    }
                    type_sig += ")";
                    if (func_decl.return_type.has_value()) {
                        type_sig += " -> " + type_to_string(**func_decl.return_type);
                    }
                    exp.type = type_sig;
                    exp.is_public = true;
                    rlib_module.exports.push_back(exp);
                }
            } else if (decl->is<parser::StructDecl>()) {
                const auto& struct_decl = decl->as<parser::StructDecl>();
                if (struct_decl.vis == parser::Visibility::Public) {
                    RlibExport exp;
                    exp.name = struct_decl.name;
                    exp.symbol = struct_decl.name;
                    exp.type = "struct";
                    exp.is_public = true;
                    rlib_module.exports.push_back(exp);
                }
            } else if (decl->is<parser::EnumDecl>()) {
                const auto& enum_decl = decl->as<parser::EnumDecl>();
                if (enum_decl.vis == parser::Visibility::Public) {
                    RlibExport exp;
                    exp.name = enum_decl.name;
                    exp.symbol = enum_decl.name;
                    exp.type = "enum";
                    exp.is_public = true;
                    rlib_module.exports.push_back(exp);
                }
            }
        }

        metadata.modules.push_back(rlib_module);

        // Create RLIB
        RlibCreateOptions rlib_opts;
        rlib_opts.verbose = verbose;

        auto rlib_result = create_rlib(object_files, metadata, final_output, rlib_opts);
        if (!rlib_result.success) {
            TML_LOG_ERROR("build", rlib_result.message);
            return rlib_result.exit_code;
        }
    } else {
        // Standard linking for executables and libraries
        LinkOptions link_options;
        link_options.output_type = link_output_type;
        link_options.verbose = verbose;
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;

        // Add @link libraries from FFI decorators
        for (const auto& lib : link_libs) {
            // Check if it's a path (contains / or \) or a library name
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                // Full path to library file
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                // Library name - use -l flag
                link_options.link_flags.push_back("-l" + lib);
            }
        }

#ifdef _WIN32
        // Add Windows system libraries for socket support
        if (has_socket_functions(module) || registry->has_module("std::net") ||
            registry->has_module("std::net::sys") || registry->has_module("std::net::tcp") ||
            registry->has_module("std::net::udp")) {
            link_options.link_flags.push_back("-lws2_32");
        }
        // Add Windows system libraries for OS module (Registry, user info)
        if (registry->has_module("std::os")) {
            link_options.link_flags.push_back("-ladvapi32");
            link_options.link_flags.push_back("-luserenv");
        }
        // Add OpenSSL libraries for crypto modules
        if (has_crypto_modules(registry)) {
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
#endif

        auto link_result = link_objects(object_files, final_output, clang, link_options);
        if (!link_result.success) {
            TML_LOG_ERROR("build", link_result.error_message);
            return 1;
        }
    }

    TML_LOG_INFO("build", "build: " << to_forward_slashes(final_output.string()));

    // Generate C header if requested (after successful build)
    if (emit_header) {
        codegen::CHeaderGenOptions header_opts;
        codegen::CHeaderGen header_gen(env, header_opts);
        auto header_result = header_gen.generate(module);

        if (!header_result.success) {
            TML_LOG_ERROR("build", "Header generation failed: " << header_result.error_message);
            return 1;
        }

        // Write header file to same directory as the library/executable
        fs::path header_output = build_dir / (module_name + ".h");

        std::ofstream header_file(header_output);
        if (!header_file) {
            TML_LOG_ERROR("build", "Cannot write to " << header_output);
            return 1;
        }
        header_file << header_result.header_content;
        header_file.close();

        TML_LOG_INFO("build", "emit-header: " << to_forward_slashes(header_output.string()));
    }

    return 0;
}

/// Main build command implementation.
///
/// Compiles a TML source file through the full pipeline and produces
/// the specified output type (executable, library, etc.).
int run_build(const std::string& path, bool verbose, bool emit_ir_only, bool emit_mir,
              bool no_cache, BuildOutputType output_type, bool emit_header,
              const std::string& output_dir) {
    // Build options struct and delegate to implementation
    BuildOptions opts;
    opts.verbose = verbose;
    opts.emit_ir_only = emit_ir_only;
    opts.emit_mir = emit_mir;
    opts.no_cache = no_cache;
    opts.output_type = output_type;
    opts.emit_header = emit_header;
    opts.output_dir = output_dir;
    return run_build_impl(path, opts);
}

int run_build_ex(const std::string& path, const BuildOptions& options) {
    return run_build_impl(path, options);
}

// ============================================================================
// Query-based build (Phase 3: Query System Foundation)
// ============================================================================

int run_build_with_queries(const std::string& path, const BuildOptions& options) {
    // Pre-load all library modules from .tml.meta binary cache
    types::preload_all_meta_caches();

    // Set up query options from build options
    query::QueryOptions qopts;
    qopts.verbose = options.verbose;
    qopts.debug_info = tml::CompilerOptions::debug_info;
    qopts.coverage = tml::CompilerOptions::coverage;
    qopts.optimization_level = tml::CompilerOptions::optimization_level;
    qopts.target_triple = tml::CompilerOptions::target_triple;
    qopts.sysroot = tml::CompilerOptions::sysroot;
    qopts.defines = options.defines;
    qopts.profile_generate = options.profile_generate;
    qopts.profile_use = options.profile_use;
    qopts.incremental = !options.no_cache;
    qopts.backend = options.backend;

    auto source_dir = fs::path(path).parent_path();
    if (source_dir.empty()) {
        source_dir = fs::current_path();
    }
    qopts.source_directory = source_dir.string();

    query::QueryContext qctx(qopts);
    auto module_name = fs::path(path).stem().string();

    // Load incremental cache from previous session
    fs::path build_dir = options.output_dir.empty() ? get_build_dir(false /* debug */)
                                                    : fs::path(options.output_dir);
    if (qopts.incremental) {
        qctx.load_incremental_cache(build_dir);
    }

    // Run the full front-end pipeline via queries (with incremental reuse)
    auto codegen_result = qctx.codegen_unit(path, module_name);

    if (!codegen_result.success) {
        // Emit errors from the first failing stage
        auto tok = qctx.cache().lookup<query::TokenizeResult>(query::TokenizeKey{path});
        if (tok && !tok->success) {
            for (const auto& err : tok->errors)
                TML_LOG_ERROR("build", err);
            return 1;
        }

        auto parsed =
            qctx.cache().lookup<query::ParseModuleResult>(query::ParseModuleKey{path, module_name});
        if (parsed && !parsed->success) {
            for (const auto& err : parsed->errors)
                TML_LOG_ERROR("build", err);
            return 1;
        }

        auto tc = qctx.cache().lookup<query::TypecheckResult>(
            query::TypecheckModuleKey{path, module_name});
        if (tc && !tc->success) {
            for (const auto& err : tc->errors)
                TML_LOG_ERROR("build", err);
            return 1;
        }

        auto bc = qctx.cache().lookup<query::BorrowcheckResult>(
            query::BorrowcheckModuleKey{path, module_name});
        if (bc && !bc->success) {
            for (const auto& err : bc->errors)
                TML_LOG_ERROR("build", err);
            return 1;
        }

        TML_LOG_ERROR("build", codegen_result.error_message);
        return 1;
    }

    const auto& llvm_ir = codegen_result.llvm_ir;
    const auto& link_libs = codegen_result.link_libs;

    // Extract intermediate results for post-codegen steps
    auto tc =
        qctx.cache().lookup<query::TypecheckResult>(query::TypecheckModuleKey{path, module_name});
    auto parsed =
        qctx.cache().lookup<query::ParseModuleResult>(query::ParseModuleKey{path, module_name});

    auto registry = (tc && tc->success) ? tc->registry : std::make_shared<types::ModuleRegistry>();

    // Log cache stats
    auto stats = qctx.cache_stats();
    TML_LOG_INFO("build", "Query cache: " << stats.total_entries << " entries, " << stats.hits
                                          << " hits, " << stats.misses << " misses");

    // Post-codegen: object compile + link (same as run_build_impl)
    bool verbose = options.verbose;
    bool emit_ir_only = options.emit_ir_only;
    BuildOutputType output_type = options.output_type;

    // build_dir already computed above for incremental cache
    fs::create_directories(build_dir);

    fs::path exe_output = build_dir / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    if (emit_ir_only) {
        fs::path ll_output = build_dir / (module_name + ".ll");
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            TML_LOG_ERROR("build", "Cannot write to " << ll_output);
            return 1;
        }
        ll_file << llvm_ir;
        ll_file.close();
        TML_LOG_INFO("build", "emit-ir: " << ll_output);
        return 0;
    }

    std::string clang = find_clang();

    fs::path deps_dir = build_dir / "deps";
    fs::create_directories(deps_dir);
    std::string deps_cache = to_forward_slashes(deps_dir.string());

    fs::path cache_dir = build_dir / ".cache";
    fs::create_directories(cache_dir);

    ObjectCompileOptions obj_options;
    obj_options.optimization_level = tml::CompilerOptions::optimization_level;
    obj_options.debug_info = tml::CompilerOptions::debug_info;
    obj_options.verbose = verbose;
    obj_options.target_triple = tml::CompilerOptions::target_triple;
    obj_options.sysroot = tml::CompilerOptions::sysroot;

    std::vector<fs::path> object_files;

    // Check if the backend already produced an object file (Cranelift path)
    if (codegen_result.has_object_file()) {
        object_files.push_back(codegen_result.object_file);
        TML_LOG_INFO("build", "Using Cranelift object: " << codegen_result.object_file);
    }

    // Try CGU partitioning if MIR is available from query cache
    bool use_cgu = !codegen_result.has_object_file();
    if (use_cgu && !options.no_cache) {
        auto mir =
            qctx.cache().lookup<query::MirBuildResult>(query::MirBuildKey{path, module_name});
        if (mir && mir->success && mir->mir_module && mir->mir_module->functions.size() >= 2) {
            use_cgu = true;

            bool is_release = (tml::CompilerOptions::optimization_level >= 2);
            codegen::PartitionOptions part_opts;
            part_opts.num_cgus = is_release ? 4 : 16;
            part_opts.codegen_opts.emit_comments = verbose;
#ifdef _WIN32
            part_opts.codegen_opts.dll_export = (output_type == BuildOutputType::DynamicLib);
            part_opts.codegen_opts.target_triple = "x86_64-pc-windows-msvc";
#else
            part_opts.codegen_opts.target_triple = "x86_64-unknown-linux-gnu";
#endif
            if (!tml::CompilerOptions::target_triple.empty()) {
                part_opts.codegen_opts.target_triple = tml::CompilerOptions::target_triple;
            }

            codegen::CodegenPartitioner partitioner(part_opts);
            auto partition_result = partitioner.partition(*mir->mir_module);

            if (partition_result.success) {
                TML_LOG_INFO("build", "CGU: Partitioned into " << partition_result.cgus.size()
                                                               << " codegen units");

                int cache_hits = 0;
                std::string obj_ext = get_object_extension();

                // Separate cached vs uncached CGUs
                std::vector<CGUCompileJob> compile_jobs;
                struct CGUSlot {
                    bool cached;
                    size_t index;
                };
                std::vector<CGUSlot> slots;
                std::vector<fs::path> cached_objects;

                for (const auto& cgu : partition_result.cgus) {
                    std::string fp12 = cgu.fingerprint.substr(0, 12);
                    std::string cgu_name =
                        module_name + ".cgu" + std::to_string(cgu.cgu_index) + "." + fp12 + obj_ext;
                    fs::path cgu_obj_path = cache_dir / cgu_name;

                    if (fs::exists(cgu_obj_path)) {
                        slots.push_back({true, cached_objects.size()});
                        cached_objects.push_back(cgu_obj_path);
                        cache_hits++;
                        TML_LOG_INFO("build",
                                     "CGU " << cgu.cgu_index << ": cache hit (" << fp12 << ")");
                    } else {
                        CGUCompileJob job;
                        job.ir_content = cgu.llvm_ir;
                        job.output_path = cgu_obj_path;
                        job.cgu_index = cgu.cgu_index;
                        job.fingerprint_tag = fp12;
                        slots.push_back({false, compile_jobs.size()});
                        compile_jobs.push_back(std::move(job));
                    }
                }

                // Compile uncached CGUs in parallel
                if (!compile_jobs.empty()) {
                    auto batch_result = compile_cgus_parallel(compile_jobs, clang, obj_options);
                    if (!batch_result.success) {
                        for (const auto& err : batch_result.errors) {
                            TML_LOG_ERROR("build", err);
                        }
                        return 1;
                    }

                    for (const auto& slot : slots) {
                        if (slot.cached) {
                            object_files.push_back(cached_objects[slot.index]);
                        } else {
                            object_files.push_back(batch_result.object_files[slot.index]);
                        }
                    }
                } else {
                    for (const auto& obj : cached_objects) {
                        object_files.push_back(obj);
                    }
                }

                TML_LOG_INFO("build", "CGU: " << cache_hits << " cached, " << compile_jobs.size()
                                              << " compiled");
            } else {
                use_cgu = false; // Fall through to monolithic
                TML_LOG_INFO("build", "CGU partitioning failed, using monolithic path");
            }
        }
    }

    if (!use_cgu && !codegen_result.has_object_file()) {
        // Monolithic path (LLVM IR → object)
        fs::path obj_output = cache_dir / (module_name + get_object_extension());
        auto obj_result = compile_ir_string_to_object(llvm_ir, obj_output, clang, obj_options);
        if (!obj_result.success) {
            TML_LOG_ERROR("build", obj_result.error_message);
            return 1;
        }
        object_files.push_back(obj_result.object_file);
    }

    if (output_type == BuildOutputType::Executable && parsed && parsed->success) {
        auto runtime_objects =
            get_runtime_objects(registry, *parsed->module, deps_cache, clang, verbose);
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());
    }

    // Link
    LinkOptions link_options;
    link_options.output_type = LinkOptions::OutputType::Executable;
    link_options.verbose = verbose;
    link_options.target_triple = tml::CompilerOptions::target_triple;
    link_options.sysroot = tml::CompilerOptions::sysroot;

    for (const auto& lib : link_libs) {
        if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
            link_options.link_flags.push_back("\"" + lib + "\"");
        } else {
            link_options.link_flags.push_back("-l" + lib);
        }
    }

#ifdef _WIN32
    // Add Windows system libraries for socket support
    if ((parsed && parsed->success && has_socket_functions(*parsed->module)) ||
        registry->has_module("std::net") || registry->has_module("std::net::sys") ||
        registry->has_module("std::net::tcp") || registry->has_module("std::net::udp")) {
        link_options.link_flags.push_back("-lws2_32");
    }
    // Add Windows system libraries for OS module (Registry, user info)
    if (registry->has_module("std::os")) {
        link_options.link_flags.push_back("-ladvapi32");
        link_options.link_flags.push_back("-luserenv");
    }
    // Add OpenSSL libraries for crypto modules
    if (has_crypto_modules(registry)) {
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
#endif

    auto link_result = link_objects(object_files, exe_output, clang, link_options);
    if (!link_result.success) {
        TML_LOG_ERROR("build", link_result.error_message);
        return 1;
    }

    // Save incremental cache for next session
    if (qopts.incremental) {
        qctx.save_incremental_cache(build_dir);
    }

    TML_LOG_INFO("build", "build: " << to_forward_slashes(exe_output.string()));
    return 0;
}

} // namespace tml::cli
