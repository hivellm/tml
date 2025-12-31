#include "builder_internal.hpp"

namespace tml::cli {

// Using helpers from builder namespace
using namespace build;

// ============================================================================
// Profiled Run (with phase timing breakdown)
// ============================================================================

int run_run_profiled(const std::string& path, const std::vector<std::string>& args, bool verbose,
                     std::string* output, PhaseTimings* timings, bool coverage, bool no_cache) {
    using Clock = std::chrono::high_resolution_clock;
    auto record_phase = [&](const std::string& phase, Clock::time_point start) {
        if (timings) {
            auto end = Clock::now();
            timings->timings_us[phase] =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }
    };

    // Phase 1: Read file
    auto phase_start = Clock::now();
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        if (output)
            *output = std::string("compilation error: ") + e.what();
        return EXIT_COMPILATION_ERROR;
    }
    record_phase("read_file", phase_start);

    // Phase 2: Lexer
    phase_start = Clock::now();
    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();
    record_phase("lexer", phase_start);

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

    // Phase 3: Parser
    phase_start = Clock::now();
    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);
    record_phase("parser", phase_start);

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

    // Phase 4: Type Checker
    phase_start = Clock::now();
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);
    record_phase("type_check", phase_start);

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

    // Phase 5: Code Generation
    phase_start = Clock::now();
    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.coverage_enabled = coverage;
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = path;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    record_phase("codegen", phase_start);

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

    // Phase 6: Prepare for compilation
    phase_start = Clock::now();
    fs::path cache_dir = get_run_cache_dir();
    std::string content_hash = generate_content_hash(source_code);
    std::string cache_key = generate_cache_key(path);
    std::string unique_name = module_name + "_" + cache_key;

    fs::path ll_output = cache_dir / (content_hash + ".ll");
    fs::path obj_output = cache_dir / (content_hash + get_object_extension());
    fs::path exe_output = cache_dir / unique_name;
    fs::path out_file = cache_dir / (unique_name + "_output.txt");
#ifdef _WIN32
    exe_output += ".exe";
#endif

    std::string clang = find_clang();
    if (clang.empty() || (!fs::exists(clang) && clang != "clang")) {
        if (output)
            *output = "compilation error: clang not found";
        return EXIT_COMPILATION_ERROR;
    }
    record_phase("setup", phase_start);

    // Phase 7: Compile to object (if not cached)
    phase_start = Clock::now();
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());
    bool use_cached_obj = fs::exists(obj_output);

    if (!use_cached_obj) {
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            if (output)
                *output = "compilation error: Cannot write to " + ll_output.string();
            return EXIT_COMPILATION_ERROR;
        }
        ll_file << llvm_ir;
        ll_file.close();

        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = false;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;

        auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            if (output)
                *output = "compilation error: " + obj_result.error_message;
            fs::remove(ll_output);
            return EXIT_COMPILATION_ERROR;
        }
        fs::remove(ll_output);
    }
    record_phase("clang_compile", phase_start);

    // Phase 8: Link
    phase_start = Clock::now();
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    std::string exe_hash = generate_exe_hash(content_hash, object_files);
    fs::path cached_exe = cache_dir / (exe_hash + ".exe");
    bool use_cached_exe = !no_cache && fs::exists(cached_exe);

    if (!use_cached_exe) {
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::Executable;
        link_options.verbose = false;
        link_options.target_triple = tml::CompilerOptions::target_triple;
        link_options.sysroot = tml::CompilerOptions::sysroot;

        for (const auto& lib : llvm_gen.get_link_libs()) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

        std::string temp_key = generate_cache_key(path);
        fs::path temp_exe = cache_dir / (exe_hash + "_" + temp_key + "_temp.exe");

        auto link_result = link_objects(object_files, temp_exe, clang, link_options);
        if (!link_result.success) {
            if (output)
                *output = "compilation error: " + link_result.error_message;
            return EXIT_COMPILATION_ERROR;
        }

        try {
            if (!fs::exists(cached_exe)) {
                fs::rename(temp_exe, cached_exe);
            } else {
                fs::remove(temp_exe);
            }
        } catch (...) {
            if (fs::exists(temp_exe)) {
                fs::remove(temp_exe);
            }
        }
    }
    record_phase("link", phase_start);

    // Phase 9a: Copy executable
    phase_start = Clock::now();
    if (!fast_copy_file(cached_exe, exe_output)) {
        if (output)
            *output = "error: Failed to copy cached exe";
        return 1;
    }
    record_phase("exe_copy", phase_start);

    // Phase 9b: Build command
    phase_start = Clock::now();
    std::string exe_native = exe_output.string();
    std::string out_native = out_file.string();
    std::string run_cmd;

#ifdef _WIN32
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
    record_phase("cmd_build", phase_start);

    // Phase 9c: Execute process
    phase_start = Clock::now();
    int run_ret = std::system(run_cmd.c_str());
    record_phase("exec", phase_start);

    // Phase 9d: Read captured output
    phase_start = Clock::now();
    if (output && fs::exists(out_file)) {
        std::ifstream ifs(out_file);
        *output =
            std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }
    record_phase("read_output", phase_start);

    // Phase 9e: Clean up
    phase_start = Clock::now();
    fs::remove(out_file);
    fs::remove(exe_output);
    record_phase("cleanup", phase_start);

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

} // namespace tml::cli
