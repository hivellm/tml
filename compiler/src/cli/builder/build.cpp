#include "builder_internal.hpp"

namespace tml::cli {

// Using helpers from builder namespace
using namespace build;

int run_build(const std::string& path, bool verbose, bool emit_ir_only, bool emit_mir,
              bool no_cache, BuildOutputType output_type, bool emit_header,
              const std::string& output_dir) {
    // no_cache is now used to skip object file caching during compilation

    // Try to load tml.toml manifest
    auto manifest_opt = Manifest::load_from_current_dir();
    if (manifest_opt && verbose) {
        std::cout << "Found tml.toml manifest for project: " << manifest_opt->package.name << "\n";
    }

    // Apply manifest settings if available (command-line flags override)
    // Note: Command-line parameters are already set, so we only apply defaults from manifest
    if (manifest_opt && !manifest_opt->build.validate()) {
        std::cerr << "Warning: Invalid build settings in tml.toml, using defaults\n";
    }

    // Initialize diagnostic emitter for Rust-style error output
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Register source content with diagnostic emitter for source snippets
    diag.set_source_content(path, source_code);

    auto source = lexer::Source::from_string(source_code, path);
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

    // Emit MIR if requested (early exit before LLVM codegen)
    if (emit_mir) {
        mir::MirBuilder mir_builder(env);
        auto mir_module = mir_builder.build(module);

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
            pm.configure_standard_pipeline();
            int passes_changed = pm.run(mir_module);
            if (verbose && passes_changed > 0) {
                std::cout << "  MIR optimization: " << passes_changed << " passes applied\n";
            }
        }

        // Use build directory structure
        fs::path build_dir =
            output_dir.empty() ? get_build_dir(false /* debug */) : fs::path(output_dir);
        fs::create_directories(build_dir);

        fs::path mir_output = build_dir / (module_name + ".mir");
        std::ofstream mir_file(mir_output);
        if (!mir_file) {
            std::cerr << "error: Cannot write to " << mir_output << "\n";
            return 1;
        }
        mir_file << mir::print_module(mir_module);
        mir_file.close();

        std::cout << "emit-mir: " << to_forward_slashes(mir_output.string()) << "\n";
        return 0;
    }

    codegen::LLVMGenOptions options;
    options.emit_comments = verbose;
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.coverage_enabled = CompilerOptions::coverage;
    options.coverage_output_file = CompilerOptions::coverage_output;
    options.source_file = path;
    if (!CompilerOptions::target_triple.empty()) {
        options.target_triple = CompilerOptions::target_triple;
    }
#ifdef _WIN32
    // Enable DLL export for dynamic libraries on Windows
    options.dll_export = (output_type == BuildOutputType::DynamicLib);
#endif
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        emit_all_codegen_errors(diag, errors);
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

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

    std::ofstream ll_file(ll_output);
    if (!ll_file) {
        std::cerr << "error: Cannot write to " << ll_output << "\n";
        return 1;
    }
    ll_file << llvm_ir;
    ll_file.close();

    if (verbose) {
        std::cout << "Generated: " << ll_output << "\n";
    }

    if (emit_ir_only) {
        std::cout << "emit-ir: " << ll_output << "\n";
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

    // Step 1: Compile LLVM IR (.ll) to object file (.o/.obj)
    ObjectCompileOptions obj_options;
    obj_options.optimization_level = tml::CompilerOptions::optimization_level;
    obj_options.debug_info = tml::CompilerOptions::debug_info;
    obj_options.verbose = verbose;
    obj_options.target_triple = tml::CompilerOptions::target_triple;
    obj_options.sysroot = tml::CompilerOptions::sysroot;

    fs::path obj_output = cache_dir / (module_name + get_object_extension());

    // Check if cached object file is valid (unless --no-cache is set)
    bool use_cached_obj = false;
    if (!no_cache && fs::exists(obj_output)) {
        try {
            auto src_time = fs::last_write_time(fs::path(path));
            auto obj_time = fs::last_write_time(obj_output);
            if (obj_time >= src_time) {
                use_cached_obj = true;
                if (verbose) {
                    std::cout << "Using cached object file: " << obj_output << "\n";
                }
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
        obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            std::cerr << "error: " << obj_result.error_message << "\n";
            return 1;
        }

        if (verbose) {
            std::cout << "Generated: " << obj_result.object_file << "\n";
        }
    }

    // Step 2: Collect all object files to link (main .o + runtime .o files)
    std::vector<fs::path> object_files;
    object_files.push_back(obj_result.object_file);

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
            std::cerr << "error: " << rlib_result.message << "\n";
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
        for (const auto& lib : llvm_gen.get_link_libs()) {
            // Check if it's a path (contains / or \) or a library name
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                // Full path to library file
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                // Library name - use -l flag
                link_options.link_flags.push_back("-l" + lib);
            }
        }

        auto link_result = link_objects(object_files, final_output, clang, link_options);
        if (!link_result.success) {
            std::cerr << "error: " << link_result.error_message << "\n";
            return 1;
        }
    }

    // Clean up .ll file (keep .o file in cache for potential reuse)
    fs::remove(ll_output);

    std::cout << "build: " << to_forward_slashes(final_output.string()) << "\n";

    // Generate C header if requested (after successful build)
    if (emit_header) {
        codegen::CHeaderGenOptions header_opts;
        codegen::CHeaderGen header_gen(env, header_opts);
        auto header_result = header_gen.generate(module);

        if (!header_result.success) {
            std::cerr << "error: Header generation failed: " << header_result.error_message << "\n";
            return 1;
        }

        // Write header file to same directory as the library/executable
        fs::path header_output = build_dir / (module_name + ".h");

        std::ofstream header_file(header_output);
        if (!header_file) {
            std::cerr << "error: Cannot write to " << header_output << "\n";
            return 1;
        }
        header_file << header_result.header_content;
        header_file.close();

        std::cout << "emit-header: " << to_forward_slashes(header_output.string()) << "\n";
    }

    return 0;
}

int run_build_ex(const std::string& path, const BuildOptions& options) {
    // For now, delegate to the standard build function
    // Full timing integration requires refactoring the build pipeline
    return run_build(path, options.verbose, options.emit_ir_only, options.emit_mir,
                     options.no_cache, options.output_type, options.emit_header,
                     options.output_dir);
}

} // namespace tml::cli
