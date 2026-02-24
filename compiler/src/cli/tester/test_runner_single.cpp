TML_MODULE("test")

//! # Test Runner — Single Test Compilation
//!
//! Single test and fuzz target compilation to shared libraries,
//! combined compile+run functions, and profiled variants.

#include "test_runner_internal.hpp"

namespace tml::cli {

// ============================================================================
// Compile Test to Shared Library
// ============================================================================

CompileToSharedLibResult compile_test_to_shared_lib(const std::string& test_file, bool /*verbose*/,
                                                    bool no_cache) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    CompileToSharedLibResult result;

    // Read source file
    std::string source_code;
    try {
        source_code = read_file(test_file);
    } catch (const std::exception& e) {
        result.error_message = std::string("Failed to read file: ") + e.what();
        return result;
    }

    // Preprocess the source code (handles #if, #ifdef, etc.)
    auto pp_config = preprocessor::Preprocessor::host_config();
    preprocessor::Preprocessor pp(pp_config);
    auto pp_result = pp.process(source_code, test_file);

    if (!pp_result.success()) {
        std::ostringstream oss;
        oss << "Preprocessor errors:\n";
        for (const auto& diag : pp_result.diagnostics) {
            if (diag.severity == preprocessor::DiagnosticSeverity::Error) {
                oss << "  " << diag.line << ":" << diag.column << ": " << diag.message << "\n";
            }
        }
        result.error_message = oss.str();
        return result;
    }

    // Lex (use preprocessed source)
    auto source = lexer::Source::from_string(pp_result.output, test_file);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        result.error_message = "Lexer errors";
        return result;
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(test_file).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        result.error_message = "Parser errors";
        return result;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Type check
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        result.error_message = "Type errors";
        return result;
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
        result.error_message = "Borrow check errors";
        return result;
    }

    // Codegen with shared library entry point
    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.generate_dll_entry = true; // Generate tml_test_entry instead of main
    options.dll_export = true;         // Export symbols
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = test_file;
    options.llvm_source_coverage = CompilerOptions::coverage_source; // LLVM instrprof
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        result.error_message = "Codegen errors";
        return result;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Use run cache for shared library files
    fs::path cache_dir = get_run_cache_dir();
    std::string content_hash = generate_content_hash(source_code);
    std::string cache_key = generate_cache_key(test_file);

    fs::path obj_output = cache_dir / (content_hash + "_shlib" + get_object_extension());

    // Use platform-specific extension for the shared library
    std::string lib_ext = get_shared_lib_extension();
    fs::path lib_output = cache_dir / (module_name + "_" + cache_key + lib_ext);

    // Note: clang may be empty if LLVM backend is available (self-contained mode)
    std::string clang = find_clang();

    // Check for cached object
    bool use_cached_obj = !no_cache && fs::exists(obj_output);

    if (!use_cached_obj) {
        // Compile LLVM IR string directly to object (no .ll on disk)
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = false;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;
        obj_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

        auto obj_result = compile_ir_string_to_object(llvm_ir, obj_output, clang, obj_options);
        if (!obj_result.success) {
            result.error_message = "Compilation failed: " + obj_result.error_message;
            return result;
        }
    }

    // Collect objects to link
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, false);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Link as shared library
    LinkOptions link_options;
    link_options.output_type = LinkOptions::OutputType::DynamicLib;
    link_options.verbose = false;
    link_options.target_triple = tml::CompilerOptions::target_triple;
    link_options.sysroot = tml::CompilerOptions::sysroot;
    link_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

    for (const auto& lib : llvm_gen.get_link_libs()) {
        if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
            link_options.link_flags.push_back("\"" + lib + "\"");
        } else {
            link_options.link_flags.push_back("-l" + lib);
        }
    }

#ifdef _WIN32
    // Link OpenSSL libraries only when crypto modules are actually used
    if (build::has_crypto_modules(registry)) {
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
    link_options.link_flags.push_back("/STACK:67108864");
#endif

    auto link_result = link_objects(object_files, lib_output, clang, link_options);
    if (!link_result.success) {
        result.error_message = "Linking failed: " + link_result.error_message;
        return result;
    }

    auto end = Clock::now();
    result.success = true;
    result.lib_path = lib_output.string();
    result.compile_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return result;
}

// ============================================================================
// Combined: Compile and Run In-Process
// ============================================================================

InProcessTestResult compile_and_run_test_in_process(const std::string& test_file, bool verbose,
                                                    bool no_cache) {
    InProcessTestResult result;

    // Compile to shared library
    auto compile_result = compile_test_to_shared_lib(test_file, verbose, no_cache);
    if (!compile_result.success) {
        result.error = compile_result.error_message;
        return result;
    }

    result.compile_time_us = compile_result.compile_time_us;

    // Run in-process
    auto run_result = run_test_in_process(compile_result.lib_path);
    result.success = run_result.success;
    result.exit_code = run_result.exit_code;
    result.output = std::move(run_result.output);
    if (!run_result.error.empty()) {
        result.error = std::move(run_result.error);
    }
    result.duration_us = run_result.duration_us;

    // Clean up shared library
    try {
        fs::remove(compile_result.lib_path);
#ifdef _WIN32
        // Also remove the import library on Windows
        fs::path lib_file = compile_result.lib_path;
        lib_file.replace_extension(".lib");
        if (fs::exists(lib_file)) {
            fs::remove(lib_file);
        }
#endif
    } catch (...) {
        // Ignore cleanup errors
    }

    return result;
}

// ============================================================================
// Compile Fuzz Target to Shared Library
// ============================================================================

CompileToSharedLibResult compile_fuzz_to_shared_lib(const std::string& fuzz_file, bool /*verbose*/,
                                                    bool no_cache) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    CompileToSharedLibResult result;

    // Read source file
    std::string source_code;
    try {
        source_code = read_file(fuzz_file);
    } catch (const std::exception& e) {
        result.error_message = std::string("Failed to read file: ") + e.what();
        return result;
    }

    // Lex
    auto source = lexer::Source::from_string(source_code, fuzz_file);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        result.error_message = "Lexer errors";
        return result;
    }

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(fuzz_file).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        result.error_message = "Parser errors";
        return result;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Type check
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        result.error_message = "Type errors";
        return result;
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
        result.error_message = "Borrow check errors";
        return result;
    }

    // Codegen with fuzz target entry point
    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.generate_fuzz_entry = true; // Generate tml_fuzz_target instead of main
    options.dll_export = true;          // Export symbols
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = fuzz_file;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        result.error_message = "Codegen errors";
        return result;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Use run cache for shared library files
    fs::path cache_dir = get_run_cache_dir();
    std::string content_hash = generate_content_hash(source_code);
    std::string cache_key = generate_cache_key(fuzz_file);

    fs::path obj_output = cache_dir / (content_hash + "_fuzz" + get_object_extension());

    // Use platform-specific extension for the shared library
    std::string lib_ext = get_shared_lib_extension();
    fs::path lib_output = cache_dir / (module_name + "_fuzz_" + cache_key + lib_ext);

    // Note: clang may be empty if LLVM backend is available (self-contained mode)
    std::string clang = find_clang();

    // Check for cached object
    bool use_cached_obj = !no_cache && fs::exists(obj_output);

    if (!use_cached_obj) {
        // Compile LLVM IR string directly to object (no .ll on disk)
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = false;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;
        obj_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

        auto obj_result = compile_ir_string_to_object(llvm_ir, obj_output, clang, obj_options);
        if (!obj_result.success) {
            result.error_message = "Compilation failed: " + obj_result.error_message;
            return result;
        }
    }

    // Collect objects to link
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, false);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Link as shared library
    LinkOptions link_options;
    link_options.output_type = LinkOptions::OutputType::DynamicLib;
    link_options.verbose = false;
    link_options.target_triple = tml::CompilerOptions::target_triple;
    link_options.sysroot = tml::CompilerOptions::sysroot;
    link_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

    for (const auto& lib : llvm_gen.get_link_libs()) {
        if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
            link_options.link_flags.push_back("\"" + lib + "\"");
        } else {
            link_options.link_flags.push_back("-l" + lib);
        }
    }

#ifdef _WIN32
    // Link OpenSSL libraries only when crypto modules are actually used
    if (build::has_crypto_modules(registry)) {
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
    link_options.link_flags.push_back("/STACK:67108864");
#endif

    auto link_result = link_objects(object_files, lib_output, clang, link_options);
    if (!link_result.success) {
        result.error_message = "Linking failed: " + link_result.error_message;
        return result;
    }

    auto end = Clock::now();
    result.success = true;
    result.lib_path = lib_output.string();
    result.compile_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return result;
}

// ============================================================================
// Compile Test to Shared Library with Phase Profiling
// ============================================================================

CompileToSharedLibResult compile_test_to_shared_lib_profiled(const std::string& test_file,
                                                             PhaseTimings* timings,
                                                             bool /*verbose*/, bool no_cache) {
    using Clock = std::chrono::high_resolution_clock;
    auto record_phase = [&](const std::string& phase, Clock::time_point start) {
        if (timings) {
            auto end = Clock::now();
            timings->timings_us[phase] =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }
    };

    CompileToSharedLibResult result;
    auto total_start = Clock::now();

    // Phase: Read source file
    auto phase_start = Clock::now();
    std::string source_code;
    try {
        source_code = read_file(test_file);
    } catch (const std::exception& e) {
        result.error_message = std::string("Failed to read file: ") + e.what();
        record_phase("read_file", phase_start);
        return result;
    }
    record_phase("read_file", phase_start);

    // Phase: Lexer
    phase_start = Clock::now();
    auto source = lexer::Source::from_string(source_code, test_file);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();
    record_phase("lexer", phase_start);

    if (lex.has_errors()) {
        result.error_message = "Lexer errors";
        return result;
    }

    // Phase: Parser
    phase_start = Clock::now();
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(test_file).stem().string();
    auto parse_result = parser.parse_module(module_name);
    record_phase("parser", phase_start);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        result.error_message = "Parser errors";
        return result;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Phase: Type check
    phase_start = Clock::now();
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);
    record_phase("type_check", phase_start);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        result.error_message = "Type errors";
        return result;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    // Phase: Borrow check (Polonius or NLL)
    phase_start = Clock::now();
    std::variant<bool, std::vector<borrow::BorrowError>> borrow_result;
    if (CompilerOptions::polonius) {
        borrow::polonius::PoloniusChecker polonius_checker(env);
        borrow_result = polonius_checker.check_module(module);
    } else {
        borrow::BorrowChecker borrow_checker(env);
        borrow_result = borrow_checker.check_module(module);
    }
    record_phase("borrow_check", phase_start);

    if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
        result.error_message = "Borrow check errors";
        return result;
    }

    // Phase: Codegen
    phase_start = Clock::now();
    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.generate_dll_entry = true;
    options.dll_export = true;
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = test_file;
    options.llvm_source_coverage = CompilerOptions::coverage_source; // LLVM instrprof
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    auto codegen_us =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start).count();
    record_phase("codegen", phase_start);

    // Debug: print per-file codegen timing to identify progressive slowdown
    TML_LOG_DEBUG("test", "[CODEGEN] " << fs::path(test_file).filename().string()
                                       << " codegen=" << (codegen_us / 1000) << "ms"
                                       << " ir_size="
                                       << (std::holds_alternative<std::string>(gen_result)
                                               ? std::get<std::string>(gen_result).size()
                                               : 0));

    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        result.error_message = "Codegen errors";
        return result;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Phase: Setup paths
    phase_start = Clock::now();
    fs::path cache_dir = get_run_cache_dir();
    std::string content_hash = generate_content_hash(source_code);
    std::string cache_key = generate_cache_key(test_file);

    fs::path obj_output = cache_dir / (content_hash + "_shlib" + get_object_extension());

    std::string lib_ext = get_shared_lib_extension();
    fs::path lib_output = cache_dir / (module_name + "_" + cache_key + lib_ext);

    // Note: clang may be empty if LLVM backend is available (self-contained mode)
    std::string clang = find_clang();
    record_phase("setup", phase_start);

    // Phase: Compile to object (if not cached)
    phase_start = Clock::now();
    bool use_cached_obj = !no_cache && fs::exists(obj_output);

    if (!use_cached_obj) {
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = false;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;
        obj_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

        auto obj_result = compile_ir_string_to_object(llvm_ir, obj_output, clang, obj_options);
        if (!obj_result.success) {
            result.error_message = "Compilation failed: " + obj_result.error_message;
            record_phase("llvm_compile", phase_start);
            return result;
        }
    }
    record_phase("llvm_compile", phase_start);

    // Phase: Link (with cache support)
    phase_start = Clock::now();
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, false);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Generate hash for cached DLL (like run_profiled does for exe)
    std::string dll_hash = generate_exe_hash(content_hash, object_files);
    fs::path cached_dll = cache_dir / (dll_hash + lib_ext);
    bool use_cached_dll = !no_cache && fs::exists(cached_dll);

    if (!use_cached_dll) {
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::DynamicLib;
        link_options.verbose = false;
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;
        link_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

        for (const auto& lib : llvm_gen.get_link_libs()) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

        // Link to temp file first, then rename to cached path
        fs::path temp_dll = cache_dir / (dll_hash + "_" + cache_key + "_temp" + lib_ext);
        auto link_result = link_objects(object_files, temp_dll, clang, link_options);
        if (!link_result.success) {
            result.error_message = "Linking failed: " + link_result.error_message;
            record_phase("link", phase_start);
            return result;
        }

        // Move to cached location
        try {
            if (!fs::exists(cached_dll)) {
                fs::rename(temp_dll, cached_dll);
            } else {
                fs::remove(temp_dll);
            }
#ifdef _WIN32
            // Also handle .lib file on Windows
            fs::path temp_lib = temp_dll;
            temp_lib.replace_extension(".lib");
            if (fs::exists(temp_lib)) {
                fs::path cached_lib = cached_dll;
                cached_lib.replace_extension(".lib");
                if (!fs::exists(cached_lib)) {
                    fs::rename(temp_lib, cached_lib);
                } else {
                    fs::remove(temp_lib);
                }
            }
#endif
        } catch (...) {
            if (fs::exists(temp_dll)) {
                fs::remove(temp_dll);
            }
        }
    }
    record_phase("link", phase_start);

    // Phase: Copy cached DLL to output location
    phase_start = Clock::now();
    if (!fast_copy_file(cached_dll, lib_output)) {
        result.error_message = "Failed to copy cached DLL";
        record_phase("dll_copy", phase_start);
        return result;
    }
    record_phase("dll_copy", phase_start);

    auto total_end = Clock::now();
    result.success = true;
    result.lib_path = lib_output.string();
    result.compile_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start).count();

    return result;
}

// ============================================================================
// Combined: Compile and Run In-Process with Full Profiling
// ============================================================================

InProcessTestResult compile_and_run_test_in_process_profiled(const std::string& test_file,
                                                             PhaseTimings* timings, bool verbose,
                                                             bool no_cache) {
    using Clock = std::chrono::high_resolution_clock;
    InProcessTestResult result;

    // Compile to shared library with phase profiling
    auto compile_result =
        compile_test_to_shared_lib_profiled(test_file, timings, verbose, no_cache);
    if (!compile_result.success) {
        result.error = compile_result.error_message;
        return result;
    }

    result.compile_time_us = compile_result.compile_time_us;

    // Run in-process with sub-phase profiling
    auto run_result = run_test_in_process_profiled(compile_result.lib_path, timings);
    result.success = run_result.success;
    result.exit_code = run_result.exit_code;
    result.output = std::move(run_result.output);
    if (!run_result.error.empty()) {
        result.error = std::move(run_result.error);
    }
    result.duration_us = run_result.duration_us;

    // Cleanup phase
    auto phase_start = Clock::now();
    try {
        fs::remove(compile_result.lib_path);
#ifdef _WIN32
        fs::path lib_file = compile_result.lib_path;
        lib_file.replace_extension(".lib");
        if (fs::exists(lib_file)) {
            fs::remove(lib_file);
        }
#endif
    } catch (...) {
        // Ignore cleanup errors
    }
    if (timings) {
        auto end = Clock::now();
        timings->timings_us["cleanup"] =
            std::chrono::duration_cast<std::chrono::microseconds>(end - phase_start).count();
    }

    return result;
}

} // namespace tml::cli
