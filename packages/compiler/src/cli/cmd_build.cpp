#include "cmd_build.hpp"
#include "utils.hpp"
#include "compiler_setup.hpp"
#include "object_compiler.hpp"
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

// Generate a content hash for caching compiled object files
static std::string generate_content_hash(const std::string& content) {
    std::hash<std::string> hasher;
    size_t hash = hasher(content);

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

// Generate a combined hash for executable caching (source + all object files)
static std::string generate_exe_hash(const std::string& source_hash, const std::vector<fs::path>& obj_files) {
    std::hash<std::string> hasher;
    size_t combined_hash = hasher(source_hash);

    // Combine hashes of all object file paths and timestamps
    for (const auto& obj : obj_files) {
        if (fs::exists(obj)) {
            // Include file path and last write time
            combined_hash ^= hasher(obj.string()) + 0x9e3779b9 + (combined_hash << 6) + (combined_hash >> 2);
            auto ftime = fs::last_write_time(obj).time_since_epoch().count();
            combined_hash ^= std::hash<decltype(ftime)>{}(ftime) + 0x9e3779b9 + (combined_hash << 6) + (combined_hash >> 2);
        }
    }

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << combined_hash;
    return oss.str();
}

// Find the project root by looking for markers like .git, CLAUDE.md, etc.
static fs::path find_project_root() {
    fs::path current = fs::current_path();

    // Walk up the directory tree looking for project markers
    while (!current.empty() && current != current.parent_path()) {
        // Check for common project markers
        if (fs::exists(current / ".git") ||
            fs::exists(current / "CLAUDE.md") ||
            fs::exists(current / "packages")) {
            return current;
        }
        current = current.parent_path();
    }

    // Fallback to current working directory
    return fs::current_path();
}

// Find or create build directory for a TML project
// Returns: project_root/build/debug or project_root/build/release
static fs::path get_build_dir(bool release = false) {
    // Always use the project root, never create build dirs next to source files
    fs::path project_root = find_project_root();

    // Create build directory structure in project root
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

// Helper to get runtime object files as a vector
// Returns a vector of compiled runtime object file paths
static std::vector<fs::path> get_runtime_objects(
    const std::shared_ptr<types::ModuleRegistry>& registry,
    const parser::Module& module,
    const std::string& deps_cache,
    const std::string& clang,
    bool verbose
) {
    std::vector<fs::path> objects;

    // Helper to find and compile runtime with caching
    auto add_runtime = [&](const std::vector<std::string>& search_paths, const std::string& name) {
        for (const auto& path : search_paths) {
            if (fs::exists(path)) {
                std::string obj = ensure_c_compiled(to_forward_slashes(fs::absolute(path).string()), deps_cache, clang, verbose);
                objects.push_back(fs::path(obj));
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
        objects.push_back(fs::path(obj));
        if (verbose) {
            std::cout << "Including runtime: " << obj << "\n";
        }
    }

    // Link core module runtimes if they were imported
    if (registry->has_module("core::mem")) {
        add_runtime({
            "packages/core/runtime/mem.c",
            "../../../core/runtime/mem.c",
            "F:/Node/hivellm/tml/packages/core/runtime/mem.c",
        }, "core::mem");
    }

    // Link time runtime if core::time is imported OR if @bench decorators are present
    if (registry->has_module("core::time") || has_bench_functions(module)) {
        add_runtime({
            "packages/core/runtime/time.c",
            "../../../core/runtime/time.c",
            "F:/Node/hivellm/tml/packages/core/runtime/time.c",
        }, "core::time");
    }

    if (registry->has_module("core::thread") || registry->has_module("core::sync")) {
        add_runtime({
            "packages/core/runtime/thread.c",
            "../../../core/runtime/thread.c",
            "F:/Node/hivellm/tml/packages/core/runtime/thread.c",
        }, "core::thread");
    }

    if (registry->has_module("test")) {
        add_runtime({
            "packages/test/runtime/test.c",
            "../../../test/runtime/test.c",
            "F:/Node/hivellm/tml/packages/test/runtime/test.c",
        }, "test");

        // Also link coverage runtime (part of test module)
        add_runtime({
            "packages/test/runtime/coverage.c",
            "../../../test/runtime/coverage.c",
            "F:/Node/hivellm/tml/packages/test/runtime/coverage.c",
        }, "test::coverage");
    }

    // Link std::collections runtime if imported
    if (registry->has_module("std::collections")) {
        add_runtime({
            "packages/std/runtime/collections.c",
            "../../../std/runtime/collections.c",
            "F:/Node/hivellm/tml/packages/std/runtime/collections.c",
        }, "std::collections");
    }

    // Link std::file runtime if imported
    if (registry->has_module("std::file")) {
        add_runtime({
            "packages/std/runtime/file.c",
            "../../../std/runtime/file.c",
            "F:/Node/hivellm/tml/packages/std/runtime/file.c",
        }, "std::file");
    }

    return objects;
}

// Get the global deps cache directory
static fs::path get_deps_cache_dir() {
    // Always use project root for deps cache
    fs::path project_root = find_project_root();
    fs::path deps = project_root / "build" / "debug" / "deps";
    fs::create_directories(deps);
    return deps;
}

// Get the global run cache directory (for tml run temporary files)
static fs::path get_run_cache_dir() {
    // Always use project root for run cache
    fs::path project_root = find_project_root();
    fs::path cache = project_root / "build" / "debug" / ".run-cache";
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

    // Use build directory structure (like Rust's target/)
    fs::path build_dir = get_build_dir(false /* debug */);
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
    obj_options.optimization_level = 3;  // -O3
    obj_options.debug_info = false;      // No debug info in release mode
    obj_options.verbose = verbose;

    fs::path obj_output = cache_dir / (module_name + get_object_extension());

    auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
    if (!obj_result.success) {
        std::cerr << "error: " << obj_result.error_message << "\n";
        return 1;
    }

    if (verbose) {
        std::cout << "Generated: " << obj_result.object_file << "\n";
    }

    // Step 2: Collect all object files to link (main .o + runtime .o files)
    std::vector<fs::path> object_files;
    object_files.push_back(obj_result.object_file);

    // Add runtime object files
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Step 3: Link all object files to create executable
    LinkOptions link_options;
    link_options.output_type = LinkOptions::OutputType::Executable;
    link_options.verbose = verbose;

    auto link_result = link_objects(object_files, exe_output, clang, link_options);
    if (!link_result.success) {
        std::cerr << "error: " << link_result.error_message << "\n";
        return 1;
    }

    // Clean up .ll file (keep .o file in cache for potential reuse)
    fs::remove(ll_output);

    std::cout << "build: " << to_forward_slashes(exe_output.string()) << "\n";
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

    // Calculate content hash for caching
    std::string content_hash = generate_content_hash(source_code);

    fs::path ll_output = cache_dir / (content_hash + ".ll");
    fs::path obj_output = cache_dir / (content_hash + get_object_extension());
    fs::path exe_output = cache_dir / module_name;
#ifdef _WIN32
    exe_output += ".exe";
#endif

    std::string clang = find_clang();
    if (clang.empty() || (!fs::exists(clang) && clang != "clang")) {
        std::cerr << "error: clang not found.\n";
        std::cerr << "Please install LLVM/clang\n";
        return 1;
    }

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
        obj_options.optimization_level = 3;  // -O3
        obj_options.debug_info = false;
        obj_options.verbose = verbose;

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

    // Check if we have a cached executable
    bool use_cached_exe = fs::exists(cached_exe);

    if (use_cached_exe) {
        if (verbose) {
            std::cout << "Using cached executable: " << cached_exe << "\n";
        }
    } else {
        // Link all object files to create executable
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::Executable;
        link_options.verbose = verbose;

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

    // Copy cached exe to final location
    try {
        fs::copy_file(cached_exe, exe_output, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        std::cerr << "error: Failed to copy cached exe: " << e.what() << "\n";
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

    // Calculate content hash for caching (unique per source content)
    std::string content_hash = generate_content_hash(source_code);

    // Generate unique file names using cache key + thread ID for exe/output (to avoid race conditions)
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
        if (output) *output = "error: clang not found";
        return 1;
    }

    // Use global deps cache for precompiled runtimes
    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

    // Check if we have a cached object file
    bool use_cached_obj = fs::exists(obj_output);

    if (!use_cached_obj) {
        // Write LLVM IR to file
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            if (output) *output = "error: Cannot write to " + ll_output.string();
            return 1;
        }
        ll_file << llvm_ir;
        ll_file.close();

        // Compile LLVM IR to object file
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = 3;  // -O3
        obj_options.debug_info = false;
        obj_options.verbose = false;  // Always quiet for tests

        auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            if (output) *output = "error: " + obj_result.error_message;
            fs::remove(ll_output);
            return 1;
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

    // Check if we have a cached executable
    bool use_cached_exe = fs::exists(cached_exe);

    if (!use_cached_exe) {
        // Link all object files to create executable
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::Executable;
        link_options.verbose = false;  // Always quiet for tests

        // Link to a unique temporary location (avoid race conditions)
        std::string temp_key = generate_cache_key(path);
        fs::path temp_exe = cache_dir / (exe_hash + "_" + temp_key + "_temp.exe");

        auto link_result = link_objects(object_files, temp_exe, clang, link_options);
        if (!link_result.success) {
            if (output) *output = "error: " + link_result.error_message;
            return 1;
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

    // Copy cached exe to final unique location (for parallel test execution)
    try {
        fs::copy_file(cached_exe, exe_output, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        if (output) *output = std::string("error: Failed to copy cached exe: ") + e.what();
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

    // Clean up temporary files (keep .obj in cache for reuse)
    fs::remove(out_file);
    fs::remove(exe_output);

#ifdef _WIN32
    return run_ret;
#else
    return WEXITSTATUS(run_ret);
#endif
}

}
