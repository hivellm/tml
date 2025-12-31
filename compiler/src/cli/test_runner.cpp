#include "test_runner.hpp"

#include "builder/builder_internal.hpp"
#include "cmd_build.hpp"
#include "object_compiler.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace tml::cli {

// Using build helpers
using namespace build;

// ============================================================================
// DynamicLibrary Implementation
// ============================================================================

DynamicLibrary::~DynamicLibrary() {
    unload();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
    : handle_(other.handle_), error_(std::move(other.error_)) {
    other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        unload();
        handle_ = other.handle_;
        error_ = std::move(other.error_);
        other.handle_ = nullptr;
    }
    return *this;
}

bool DynamicLibrary::load(const std::string& path) {
    unload();
    error_.clear();

#ifdef _WIN32
    handle_ = LoadLibraryA(path.c_str());
    if (!handle_) {
        DWORD err = GetLastError();
        error_ = "LoadLibrary failed with error code " + std::to_string(err);
        return false;
    }
#else
    handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_) {
        const char* err = dlerror();
        error_ = err ? err : "Unknown dlopen error";
        return false;
    }
#endif

    return true;
}

void DynamicLibrary::unload() {
    if (handle_) {
#ifdef _WIN32
        FreeLibrary(handle_);
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
    }
}

bool DynamicLibrary::is_loaded() const {
    return handle_ != nullptr;
}

void* DynamicLibrary::get_symbol(const std::string& name) const {
    if (!handle_) {
        return nullptr;
    }

#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(handle_, name.c_str()));
#else
    return dlsym(handle_, name.c_str());
#endif
}

// ============================================================================
// Compile Test to Shared Library
// ============================================================================

CompileToSharedLibResult compile_test_to_shared_lib(const std::string& test_file, bool verbose,
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

    // Lex
    auto source = lexer::Source::from_string(source_code, test_file);
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

    // Codegen with shared library entry point
    codegen::LLVMGenOptions options;
    options.emit_comments = false;
    options.generate_dll_entry = true; // Generate tml_test_entry instead of main
    options.dll_export = true;         // Export symbols
    options.emit_debug_info = CompilerOptions::debug_info;
    options.debug_level = CompilerOptions::debug_level;
    options.source_file = test_file;
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

    fs::path ll_output = cache_dir / (content_hash + "_shlib.ll");
    fs::path obj_output = cache_dir / (content_hash + "_shlib" + get_object_extension());

    // Use platform-specific extension for the shared library
    std::string lib_ext = get_shared_lib_extension();
    fs::path lib_output = cache_dir / (module_name + "_" + cache_key + lib_ext);

    std::string clang = find_clang();
    if (clang.empty()) {
        result.error_message = "clang not found";
        return result;
    }

    // Check for cached object
    bool use_cached_obj = !no_cache && fs::exists(obj_output);

    if (!use_cached_obj) {
        // Write LLVM IR
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            result.error_message = "Cannot write LLVM IR";
            return result;
        }
        ll_file << llvm_ir;
        ll_file.close();

        // Compile to object
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = tml::CompilerOptions::debug_info;
        obj_options.verbose = false;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;

        auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
        if (!obj_result.success) {
            result.error_message = "Compilation failed: " + obj_result.error_message;
            fs::remove(ll_output);
            return result;
        }
        fs::remove(ll_output);
    }

    // Collect objects to link
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Link as shared library
    LinkOptions link_options;
    link_options.output_type = LinkOptions::OutputType::DynamicLib;
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
// Run Test In-Process
// ============================================================================

InProcessTestResult run_test_in_process(const std::string& lib_path) {
    using Clock = std::chrono::high_resolution_clock;
    InProcessTestResult result;

    // Load the shared library
    DynamicLibrary lib;
    if (!lib.load(lib_path)) {
        result.error = "Failed to load shared library: " + lib.get_error();
        return result;
    }

    // Get the test entry function
    auto test_entry = lib.get_function<TestMainFunc>("tml_test_entry");
    if (!test_entry) {
        result.error = "Failed to find tml_test_entry in shared library";
        return result;
    }

    // Execute the test
    auto start = Clock::now();

    // TODO: Capture stdout/stderr properly
    // For now, just call the function directly
    try {
        result.exit_code = test_entry();
        result.success = (result.exit_code == 0);
    } catch (...) {
        result.error = "Exception during test execution";
        result.exit_code = 1;
    }

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

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

} // namespace tml::cli
