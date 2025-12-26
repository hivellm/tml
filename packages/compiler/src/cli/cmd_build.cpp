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
#include <functional>
#include <thread>
#include <sstream>
#include <iomanip>
#ifndef _WIN32
#include <sys/wait.h>
#include "tml/types/module.hpp"
#endif

namespace fs = std::filesystem;
using namespace tml;

namespace tml::cli {

// Generate a unique cache key for a file path (to avoid collisions in parallel tests)
static std::string generate_cache_key(const std::string& path) {
    // Use hash of full path + thread ID to ensure uniqueness
    std::hash<std::string> hasher;
    size_t path_hash = hasher(path);
    size_t thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    size_t combined = path_hash ^ (thread_hash << 1);

    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << (combined & 0xFFFFFFFF);
    return oss.str();
}

// Find or create build directory for a TML project
// Returns: project_root/build/debug or project_root/build/release
static fs::path get_build_dir(const fs::path& source_file, bool release = false) {
    // For now, use the source file's directory as project root
    // In the future, we could look for tml.toml or similar
    fs::path project_root = source_file.parent_path();

    // Create build directory structure
    fs::path build_dir = project_root / "build" / (release ? "release" : "debug");
    fs::create_directories(build_dir);

    return build_dir;
}

// Helper to check if any function has @bench decorator
static bool has_bench_functions(const parser::Module& module) {
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            for (const auto& decorator : func.decorators) {
                if (decorator.name == "bench") {
                    return true;
                }
            }
        }
    }
    return false;
}

// Helper to link runtime files with caching
// Returns the additional arguments to pass to clang
static std::string link_runtimes_cached(
    const std::shared_ptr<types::ModuleRegistry>& registry,
    const parser::Module& module,
    const std::string& deps_cache,
    const std::string& clang,
    bool verbose
) {
    std::string result;

    // Helper to find and compile runtime with caching
    auto link_runtime = [&](const std::vector<std::string>& search_paths, const std::string& name) {
        for (const auto& path : search_paths) {
            if (fs::exists(path)) {
                std::string obj = ensure_c_compiled(to_forward_slashes(fs::absolute(path).string()), deps_cache, clang, verbose);
                result += " \"" + obj + "\"";
                if (verbose) {
                    std::cout << "Including " << name << ": " << obj << "\n";
                }
                return;
            }
        }
    };

    // Essential runtime
    std::string runtime_path = find_runtime();
    if (!runtime_path.empty()) {
        std::string obj = ensure_c_compiled(runtime_path, deps_cache, clang, verbose);
        result += " \"" + obj + "\"";
        if (verbose) {
            std::cout << "Including runtime: " << obj << "\n";
        }
    }

    // Link core module runtimes if they were imported
    if (registry->has_module("core::mem")) {
        link_runtime({
            "packages/core/runtime/mem.c",
            "../../../core/runtime/mem.c",
            "F:/Node/hivellm/tml/packages/core/runtime/mem.c",
        }, "core::mem");
    }

    // Link time runtime if core::time is imported OR if @bench decorators are present
    if (registry->has_module("core::time") || has_bench_functions(module)) {
        link_runtime({
            "packages/core/runtime/time.c",
            "../../../core/runtime/time.c",
            "F:/Node/hivellm/tml/packages/core/runtime/time.c",
        }, "core::time");
    }

    if (registry->has_module("core::thread") || registry->has_module("core::sync")) {
        link_runtime({
            "packages/core/runtime/thread.c",
            "../../../core/runtime/thread.c",
            "F:/Node/hivellm/tml/packages/core/runtime/thread.c",
        }, "core::thread");
    }

    if (registry->has_module("test")) {
        link_runtime({
            "packages/test/runtime/test.c",
            "../../../test/runtime/test.c",
            "F:/Node/hivellm/tml/packages/test/runtime/test.c",
        }, "test");

        // Also link coverage runtime (part of test module)
        link_runtime({
            "packages/test/runtime/coverage.c",
            "../../../test/runtime/coverage.c",
            "F:/Node/hivellm/tml/packages/test/runtime/coverage.c",
        }, "test::coverage");
    }

    // Link std::collections runtime if imported
    if (registry->has_module("std::collections")) {
        link_runtime({
            "packages/std/runtime/collections.c",
            "../../../std/runtime/collections.c",
            "F:/Node/hivellm/tml/packages/std/runtime/collections.c",
        }, "std::collections");
    }

    // Link std::file runtime if imported
    if (registry->has_module("std::file")) {
        link_runtime({
            "packages/std/runtime/file.c",
            "../../../std/runtime/file.c",
            "F:/Node/hivellm/tml/packages/std/runtime/file.c",
        }, "std::file");
    }

    return result;
}

// Get the global deps cache directory
static fs::path get_deps_cache_dir() {
    // Use a global cache in the TML installation or current working directory
    std::vector<std::string> cache_search = {
        "build/debug/deps",
        "F:/Node/hivellm/tml/build/debug/deps",
    };
    for (const auto& p : cache_search) {
        fs::path dir = p;
        if (fs::exists(dir.parent_path())) {
            fs::create_directories(dir);
            return dir;
        }
    }
    // Fallback to local build/debug/deps
    fs::path deps = fs::current_path() / "build" / "debug" / "deps";
    fs::create_directories(deps);
    return deps;
}

// Get the global run cache directory (for tml run temporary files)
static fs::path get_run_cache_dir() {
    // Use a global cache - not local to each file
    std::vector<std::string> cache_search = {
        "build/debug/.run-cache",
        "F:/Node/hivellm/tml/build/debug/.run-cache",
    };
    for (const auto& p : cache_search) {
        fs::path dir = p;
        if (fs::exists(dir.parent_path().parent_path())) {
            fs::create_directories(dir);
            return dir;
        }
    }
    // Fallback to current directory
    fs::path cache = fs::current_path() / "build" / "debug" / ".run-cache";
    fs::create_directories(cache);
    return cache;
}

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

    // Use build directory structure (like Rust's target/)
    fs::path build_dir = get_build_dir(input_path, false /* debug */);
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
    std::string ll_path = to_forward_slashes(ll_output.string());
    std::string exe_path = to_forward_slashes(exe_output.string());

    // Create deps cache directory for precompiled runtimes
    fs::path deps_dir = build_dir / "deps";
    fs::create_directories(deps_dir);
    std::string deps_cache = to_forward_slashes(deps_dir.string());

    std::string compile_cmd = clang + " -Wno-override-module -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops -o \"" +
                              exe_path + "\" \"" + ll_path + "\"";

    // Link all runtimes with caching
    compile_cmd += link_runtimes_cached(registry, module, deps_cache, clang, verbose);

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

int run_run(const std::string& path, const std::vector<std::string>& args, bool verbose, bool coverage) {
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
    options.coverage_enabled = coverage;
    options.source_file = path;
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

    // Use centralized run cache - NEVER create files inside packages
    fs::path cache_dir = get_run_cache_dir();

    fs::path ll_output = cache_dir / (module_name + ".ll");
    fs::path exe_output = cache_dir / module_name;
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
        fs::remove(ll_output);  // Clean up .ll file
        return 1;
    }

    if (verbose) {
        std::cout << "Using compiler: " << clang << "\n";
    }

    std::string ll_path = to_forward_slashes(ll_output.string());
    std::string exe_path = to_forward_slashes(exe_output.string());

    // Use global deps cache for precompiled runtimes
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

    std::string compile_cmd = clang + " -Wno-override-module -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops -o \"" +
                              exe_path + "\" \"" + ll_path + "\"";

    // Link all runtimes with caching
    compile_cmd += link_runtimes_cached(registry, module, deps_cache, clang, verbose);

    if (verbose) {
        std::cout << "Compiling: " << compile_cmd << "\n";
    }

    int compile_ret = std::system(compile_cmd.c_str());
    if (compile_ret != 0) {
        std::cerr << "error: LLVM compilation failed\n";
        return 1;
    }

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

    // Keep cache files for faster re-runs (in build/.cache/)
    // Only clean up .ll files, keep .exe for potential caching
    if (!verbose) {
        fs::remove(ll_output);  // Remove intermediate .ll file
    } else {
        std::cout << "Build cache at: " << cache_dir << "\n";
    }

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

int run_run_quiet(const std::string& path, const std::vector<std::string>& args,
                  bool verbose, std::string* output, bool coverage) {
    std::string source_code;
    try {
        source_code = read_file(path);
    } catch (const std::exception& e) {
        if (output) *output = std::string("error: ") + e.what();
        return 1;
    }

    auto source = lexer::Source::from_string(source_code, path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();

    if (lex.has_errors()) {
        std::string err_output;
        for (const auto& error : lex.errors()) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " +
                          error.message + "\n";
        }
        if (output) *output = err_output;
        return 1;
    }

    parser::Parser parser(std::move(tokens));
    auto module_name = fs::path(path).stem().string();
    auto parse_result = parser.parse_module(module_name);

    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        std::string err_output;
        const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " +
                          error.message + "\n";
        }
        if (output) *output = err_output;
        return 1;
    }

    const auto& module = std::get<parser::Module>(parse_result);

    auto registry = std::make_shared<types::ModuleRegistry>();
    types::TypeChecker checker;
    checker.set_module_registry(registry);
    auto check_result = checker.check_module(module);

    if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
        std::string err_output;
        const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": error: " +
                          error.message + "\n";
        }
        if (output) *output = err_output;
        return 1;
    }

    const auto& env = std::get<types::TypeEnv>(check_result);

    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.coverage_enabled = coverage;
    options.source_file = path;
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
        std::string err_output;
        const auto& errors = std::get<std::vector<codegen::LLVMGenError>>(gen_result);
        for (const auto& error : errors) {
            err_output += path + ":" + std::to_string(error.span.start.line) + ":" +
                          std::to_string(error.span.start.column) + ": codegen error: " +
                          error.message + "\n";
        }
        if (output) *output = err_output;
        return 1;
    }

    const auto& llvm_ir = std::get<std::string>(gen_result);

    // Use centralized run cache - NEVER create files inside packages
    fs::path cache_dir = get_run_cache_dir();

    // Generate unique file names using path hash + thread ID to avoid race conditions
    std::string cache_key = generate_cache_key(path);
    std::string unique_name = module_name + "_" + cache_key;

    fs::path ll_output = cache_dir / (unique_name + ".ll");
    fs::path exe_output = cache_dir / unique_name;
    fs::path out_file = cache_dir / (unique_name + "_output.txt");
#ifdef _WIN32
    exe_output += ".exe";
#endif

    std::ofstream ll_file(ll_output);
    if (!ll_file) {
        if (output) *output = "error: Cannot write to " + ll_output.string();
        return 1;
    }
    ll_file << llvm_ir;
    ll_file.close();

    std::string clang = find_clang();
    if (clang.empty() || (!fs::exists(clang) && clang != "clang")) {
        if (output) *output = "error: clang not found";
        return 1;
    }

    std::string ll_path = to_forward_slashes(ll_output.string());
    std::string exe_path = to_forward_slashes(exe_output.string());

    // Use global deps cache for precompiled runtimes
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

    std::string compile_cmd = clang + " -Wno-override-module -O3 -march=native -mtune=native -fomit-frame-pointer -funroll-loops -o \"" +
                              exe_path + "\" \"" + ll_path + "\"";

    // Link all runtimes with caching
    compile_cmd += link_runtimes_cached(registry, module, deps_cache, clang, verbose);

    // Redirect compile output to null (only stderr, keep stdout for errors)
#ifdef _WIN32
    compile_cmd += " 2>nul";
#else
    compile_cmd += " 2>/dev/null";
#endif

    int compile_ret = std::system(compile_cmd.c_str());
    if (compile_ret != 0) {
        if (output) *output = "error: LLVM compilation failed";
        fs::remove(ll_output);  // Clean up .ll file
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
        *output = std::string((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());
    }

    // Clean up all intermediate files (unique names per thread, no caching benefit)
    fs::remove(ll_output);
    fs::remove(out_file);
    fs::remove(exe_output);  // Also remove exe since it has unique thread-specific name

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

}
