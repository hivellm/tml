//! # Run Command Implementation
//!
//! This file implements the `tml run` command that compiles and immediately
//! executes TML programs. It uses aggressive caching to minimize recompilation.
//!
//! ## Execution Flow
//!
//! ```text
//! run_run()
//!   ├─ Compile (same as run_build)
//!   ├─ Cache executable by content hash
//!   ├─ Execute with provided arguments
//!   └─ Clean up temporary files
//! ```
//!
//! ## Caching Strategy
//!
//! Executables are cached in `build/debug/.run-cache/`:
//! - Object files: `<content_hash>.obj`
//! - Executables: `<exe_hash>.exe` (hash includes all linked objects)
//!
//! This allows instant re-execution when source hasn't changed.

#include "builder_internal.hpp"

namespace tml::cli {

// Using helpers from builder namespace
using namespace build;

/// Compiles and runs a TML program.
///
/// This is the implementation of `tml run <file>`. It compiles the source
/// file (using caching when possible) and executes the resulting binary.
///
/// ## Return Value
///
/// Returns the exit code of the executed program.
int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose,
            bool coverage, bool no_cache) {
    // Initialize diagnostic emitter for Rust-style error output
    auto& diag = get_diagnostic_emitter();

    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Run preprocessor to handle #if/#define/#ifdef etc.
    BuildOptions preproc_opts; // Default options for run
    auto preproc_result = preprocess_source(source_code, path, preproc_opts);

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
    // Set source directory for local module resolution
    {
        auto source_dir = fs::path(path).parent_path();
        if (source_dir.empty()) {
            source_dir = fs::current_path();
        }
        checker.set_source_directory(source_dir.string());
    }
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        emit_all_type_errors(diag, errors);
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    // Run borrow checker (ownership and borrowing validation)
    borrow::BorrowChecker borrow_checker;
    auto borrow_result = borrow_checker.check_module(module);

    if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
        const auto& errors = std::get<std::vector<borrow::BorrowError>>(borrow_result);
        emit_all_borrow_errors(diag, errors);
        return 1;
    }

    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.coverage_enabled = coverage;
    options.coverage_output_file = CompilerOptions::coverage_output;
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = path;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        emit_all_codegen_errors(diag, errors);
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Use centralized run cache - NEVER create files inside packages
    fs::path cache_dir = get_run_cache_dir();

    // Calculate content hash for caching
    std::string content_hash = generate_content_hash(source_code);

    fs::path ll_output = cache_dir / (content_hash + ".ll");
    fs::path obj_output = cache_dir / (content_hash + get_object_extension());
    fs::path exe_output = cache_dir / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    // Note: clang may be empty if LLVM backend and LLD are available (self-contained mode)
    std::string clang = find_clang();

    // Use global deps cache for precompiled runtimes
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

    // Check if we have a cached object file
    bool use_cached_obj = fs::exists(obj_output);

    if (use_cached_obj) {
        if (verbose) {
            std::cout << "Using cached object: " << obj_output << "\n";
        }
    } else {
        // Write LLVM IR to file
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

        // Compile LLVM IR to object file
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = verbose;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;

        auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            std::cerr << "error: " << obj_result.error_message << "\n";
            fs::remove(ll_output);
            return 1;
        }

        if (verbose) {
            std::cout << "Compiled to: " << obj_result.object_file << "\n";
        }

        // Clean up .ll file (keep .obj for caching)
        fs::remove(ll_output);
    }

    // Collect all object files to link
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    // Add runtime object files
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Generate hash for executable caching (source + all object files)
    std::string exe_hash = generate_exe_hash(content_hash, object_files);
    fs::path cached_exe = cache_dir / (exe_hash + ".exe");

    // Check if we have a cached executable (skip if --no-cache)
    bool use_cached_exe = !no_cache && fs::exists(cached_exe);

    if (use_cached_exe) {
        if (verbose) {
            std::cout << "Using cached executable: " << cached_exe << "\n";
        }
    } else {
        // Link all object files to create executable
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::Executable;
        link_options.verbose = verbose;
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;

        // Add @link libraries from FFI decorators
        for (const auto& lib : llvm_gen.get_link_libs()) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

        // Link to temporary location first
        fs::path temp_exe = cache_dir / (exe_hash + "_link_temp.exe");

        auto link_result = link_objects(object_files, temp_exe, clang, link_options);
        if (!link_result.success) {
            std::cerr << "error: " << link_result.error_message << "\n";
            return 1;
        }

        if (verbose) {
            std::cout << "Linked executable: " << temp_exe << "\n";
        }

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

    // Copy cached exe to final location (use hard link for speed)
    if (!fast_copy_file(cached_exe, exe_output)) {
        std::cerr << "error: Failed to copy cached exe to " << exe_output << "\n";
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

    if (verbose) {
        std::cout << "Running: " << run_cmd << "\n";
    }

    int run_ret = std::system(run_cmd.c_str());

    // Clean up temporary executable (keep .obj in cache for reuse)
    fs::remove(exe_output);

    if (verbose) {
        std::cout << "Cleaned up temporary executable\n";
    }

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

int run_run_quiet(const std::string& path, const std::vector<std::string>& args, bool verbose,
                  std::string* output, bool coverage, bool no_cache) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        if (output)
            *output = std::string("compilation error: ") + e.what();
        return EXIT_COMPILATION_ERROR;
    }

    // Run preprocessor to handle #if/#define/#ifdef etc.
    BuildOptions preproc_opts; // Default options for run
    auto preproc_result = preprocess_source(source_code, path, preproc_opts);

    if (!preproc_result.success()) {
        std::string err_output = "compilation error:\n";
        for (const auto& diag : preproc_result.diagnostics) {
            if (diag.severity == preprocessor::DiagnosticSeverity::Error) {
                err_output += path + ":" + std::to_string(diag.line) + ":" +
                              std::to_string(diag.column) + ": error: " + diag.message + "\n";
            }
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    // Use preprocessed source for compilation
    std::string preprocessed_source = preproc_result.output;

    auto source = lexer::Source::from_string(preprocessed_source, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        std::string err_output = "compilation error:\n";
        for (const auto& error : lex.errors()) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " + error.message +
                          "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        std::string err_output = "compilation error:\n";
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " + error.message +
                          "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        std::string err_output = "compilation error:\n";
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " + error.message +
                          "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    // Run borrow checker (ownership and borrowing validation)
    borrow::BorrowChecker borrow_checker;
    auto borrow_result = borrow_checker.check_module(module);

    if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
        std::string err_output = "Borrow check error:\n";
        const auto& errors = std::get<std::vector<borrow::BorrowError>>(borrow_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " + error.message +
                          "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.coverage_enabled = coverage;
    options.coverage_output_file = CompilerOptions::coverage_output;
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = path;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        std::string err_output = "compilation error:\n";
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) +
                          ": codegen error: " + error.message + "\n";
        }
        if (output)
            *output = err_output;
        return EXIT_COMPILATION_ERROR;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Use centralized run cache - NEVER create files inside packages
    fs::path cache_dir = get_run_cache_dir();

    // Calculate content hash for caching (unique per source content)
    std::string content_hash = generate_content_hash(source_code);

    // Generate unique file names using cache key + thread ID for exe/output (to avoid race
    // conditions)
    std::string cache_key = generate_cache_key(path);
    std::string unique_name = module_name + "_" + cache_key;

    fs::path ll_output = cache_dir / (content_hash + ".ll");
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
        // Write LLVM IR to file
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            if (output)
                *output = "compilation error: Cannot write to " + ll_output.string();
            return EXIT_COMPILATION_ERROR;
        }
        ll_file << llvm_ir;
        ll_file.close();

        // Compile LLVM IR to object file
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = false; // Always quiet for tests
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;

        auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            if (output)
                *output = "compilation error: " + obj_result.error_message;
            fs::remove(ll_output);
            return EXIT_COMPILATION_ERROR;
        }

        // Clean up .ll file (keep .obj for caching)
        fs::remove(ll_output);
    }

    // Collect all object files to link
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    // Add runtime object files
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

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
        for (const auto& lib : llvm_gen.get_link_libs()) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

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

} // namespace tml::cli
