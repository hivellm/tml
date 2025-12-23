#include "cmd_build.hpp"
#include "utils.hpp"
#include "compiler_setup.hpp"
#include "tml/common.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include "tml/parser/parser.hpp"
#include "tml/types/checker.hpp"
#include "tml/codegen/llvm_ir_gen.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#ifndef _WIN32
#include <sys/wait.h>
#include "tml/types/module.hpp"
#endif

namespace fs = std::filesystem;
using namespace tml;

namespace tml::cli {

int run_build(const std::string& path, bool verbose, bool emit_ir_only) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
        }
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Initialize module registry for test module
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    codegen::LLVMGenOptions options;
    options.emit_comments = verbose;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": codegen error: "
                      << error.message << "\n";
        }
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    fs::path input_path = fs::absolute(path);
    fs::path ll_output = input_path.parent_path() / (module_name + ".ll");
    fs::path exe_output = input_path.parent_path() / module_name;
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
        std::cout << "build: " << ll_output << "\n";
        return 0;
    }

    std::string clang = find_clang();
    std::string ll_path = to_forward_slashes(ll_output.string());
    std::string exe_path = to_forward_slashes(exe_output.string());

    std::string build_runtime_path = find_runtime();
    std::string compile_cmd = clang + " -Wno-override-module -O3 -march=native -mtune=native -ffast-math -fomit-frame-pointer -funroll-loops -o \"" +
                              exe_path + "\" \"" + ll_path + "\"";

    if (!build_runtime_path.empty()) {
        std::string runtime_to_link = ensure_runtime_compiled(build_runtime_path, clang, verbose);
        compile_cmd += " \"" + runtime_to_link + "\"";
        if (verbose) {
            std::cout << "Including runtime: " << runtime_to_link << "\n";
        }
    }

    if (verbose) {
        std::cout << "Running: " << compile_cmd << "\n";
    }

    int ret = std::system(compile_cmd.c_str());
    if (ret != 0) {
        std::cerr << "error: LLVM compilation failed\n";
        return 1;
    }

    fs::remove(ll_output);

    std::cout << "build: " << exe_path << "\n";
    return 0;
}

int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        for (const auto& error : lex.errors()) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
        }
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    // Initialize module registry for test module
    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": error: "
                      << error.message << "\n";
            for (const auto& note : error.notes) {
                std::cerr << "  note: " << note << "\n";
            }
        }
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        for (const auto& error : errors) {
            std::cerr << path << ":" << error.span.start.line << ":"
                      << error.span.start.column << ": codegen error: "
                      << error.message << "\n";
        }
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    fs::path temp_dir = fs::temp_directory_path() / ("tml_run_" + std::to_string(std::hash<std::string>{}(path)));
    fs::create_directories(temp_dir);

    fs::path ll_output = temp_dir / (module_name + ".ll");
    fs::path exe_output = temp_dir / module_name;
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

    std::string clang = find_clang();
    if (clang.empty() || (!fs::exists(clang) && clang != "clang")) {
        std::cerr << "error: clang not found.\n";
        std::cerr << "Please install LLVM/clang\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    if (verbose) {
        std::cout << "Using compiler: " << clang << "\n";
    }

    std::string ll_path = to_forward_slashes(ll_output.string());
    std::string exe_path = to_forward_slashes(exe_output.string());

    std::string runtime_path = find_runtime();
    std::string compile_cmd = clang + " -Wno-override-module -O3 -march=native -mtune=native -ffast-math -fomit-frame-pointer -funroll-loops -o \"" +
                              exe_path + "\" \"" + ll_path + "\"";

    if (!runtime_path.empty()) {
        std::string runtime_to_link = ensure_runtime_compiled(runtime_path, clang, verbose);
        compile_cmd += " \"" + runtime_to_link + "\"";
        if (verbose) {
            std::cout << "Including runtime: " << runtime_to_link << "\n";
        }
    }

    if (verbose) {
        std::cout << "Compiling: " << compile_cmd << "\n";
    }

    int compile_ret = std::system(compile_cmd.c_str());
    if (compile_ret != 0) {
        std::cerr << "error: LLVM compilation failed\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    std::string run_cmd = "\"" + exe_path + "\"";
    for (const auto& arg : args) {
        run_cmd += " \"" + arg + "\"";
    }

    if (verbose) {
        std::cout << "Running: " << run_cmd << "\n";
    }

    int run_ret = std::system(run_cmd.c_str());

    if (!verbose) {
        fs::remove_all(temp_dir);
    } else {
        std::cout << "Temp files kept at: " << temp_dir << "\n";
    }

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

}
