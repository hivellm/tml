#include "test_runner.hpp"

#include "builder/builder_internal.hpp"
#include "cmd_build.hpp"
#include "object_compiler.hpp"
#include "tester/tester_internal.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace tml::cli {

// Using build helpers
using namespace build;

// ============================================================================
// Output Capture Helper
// ============================================================================

// RAII class to capture stdout/stderr to a string
class OutputCapture {
public:
    OutputCapture() : capturing_(false), temp_file_path_("") {}

    ~OutputCapture() {
        stop();
        cleanup();
    }

    bool start() {
        if (capturing_)
            return true;

        // Create a temporary file for capturing output
        temp_file_path_ = get_run_cache_dir() / ("capture_" + std::to_string(std::time(nullptr)) +
                                                 "_" + std::to_string(rand()) + ".tmp");

        // Flush before redirecting
        std::fflush(stdout);
        std::fflush(stderr);

#ifdef _WIN32
        // Save original stdout/stderr file descriptors
        saved_stdout_ = _dup(_fileno(stdout));
        saved_stderr_ = _dup(_fileno(stderr));

        // Open temp file and redirect stdout/stderr to it
        int temp_fd = -1;
        errno_t err = _sopen_s(&temp_fd, temp_file_path_.string().c_str(),
                               _O_WRONLY | _O_CREAT | _O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (err != 0 || temp_fd < 0) {
            return false;
        }

        _dup2(temp_fd, _fileno(stdout));
        _dup2(temp_fd, _fileno(stderr));
        _close(temp_fd);
#else
        // Save original stdout/stderr file descriptors
        saved_stdout_ = dup(STDOUT_FILENO);
        saved_stderr_ = dup(STDERR_FILENO);

        // Open temp file and redirect stdout/stderr to it
        int temp_fd = open(temp_file_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (temp_fd < 0) {
            return false;
        }

        dup2(temp_fd, STDOUT_FILENO);
        dup2(temp_fd, STDERR_FILENO);
        close(temp_fd);
#endif

        capturing_ = true;
        return true;
    }

    std::string stop() {
        if (!capturing_)
            return "";

        // Flush before restoring
        std::fflush(stdout);
        std::fflush(stderr);

#ifdef _WIN32
        // Restore original stdout/stderr
        _dup2(saved_stdout_, _fileno(stdout));
        _dup2(saved_stderr_, _fileno(stderr));
        _close(saved_stdout_);
        _close(saved_stderr_);
#else
        // Restore original stdout/stderr
        dup2(saved_stdout_, STDOUT_FILENO);
        dup2(saved_stderr_, STDERR_FILENO);
        close(saved_stdout_);
        close(saved_stderr_);
#endif

        capturing_ = false;

        // Read the captured output from the temp file
        std::ifstream file(temp_file_path_);
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            captured_output_ = buffer.str();
        }

        return captured_output_;
    }

    const std::string& get_output() const {
        return captured_output_;
    }

private:
    void cleanup() {
        if (!temp_file_path_.empty() && fs::exists(temp_file_path_)) {
            try {
                fs::remove(temp_file_path_);
            } catch (...) {
                // Ignore cleanup errors
            }
        }
    }

    bool capturing_;
    fs::path temp_file_path_;
    std::string captured_output_;
    int saved_stdout_ = -1;
    int saved_stderr_ = -1;
};

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
    // Convert to absolute path for faster loading
    fs::path abs_path = fs::absolute(path);
    std::wstring wpath = abs_path.wstring();

    // Use LoadLibraryExW with optimized flags:
    // - LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR: Search only the DLL's directory for dependencies
    // - LOAD_LIBRARY_SEARCH_DEFAULT_DIRS: Also search system directories
    // This avoids searching the entire PATH which can be slow
    handle_ = LoadLibraryExW(wpath.c_str(), nullptr,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!handle_) {
        // Fallback to regular LoadLibrary if the optimized version fails
        // (e.g., on older Windows versions)
        handle_ = LoadLibraryW(wpath.c_str());
        if (!handle_) {
            DWORD err = GetLastError();
            error_ = "LoadLibrary failed with error code " + std::to_string(err);
            return false;
        }
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

    // Borrow check
    borrow::BorrowChecker borrow_checker;
    auto borrow_result = borrow_checker.check_module(module);

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

    // Set up output capture
    OutputCapture capture;
    bool capture_started = capture.start();

    // Execute the test
    auto start = Clock::now();

    try {
        result.exit_code = test_entry();
        result.success = (result.exit_code == 0);
    } catch (...) {
        result.error = "Exception during test execution";
        result.exit_code = 1;
    }

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Stop capturing and retrieve output
    if (capture_started) {
        result.output = capture.stop();
    }

    return result;
}

// ============================================================================
// Run Test In-Process with Sub-Phase Profiling
// ============================================================================

InProcessTestResult run_test_in_process_profiled(const std::string& lib_path,
                                                 PhaseTimings* timings) {
    using Clock = std::chrono::high_resolution_clock;
    auto record_phase = [&](const std::string& phase, Clock::time_point start) {
        if (timings) {
            auto end = Clock::now();
            timings->timings_us[phase] =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }
    };

    InProcessTestResult result;

    // Phase: Load the shared library
    auto phase_start = Clock::now();
    DynamicLibrary lib;
    if (!lib.load(lib_path)) {
        result.error = "Failed to load shared library: " + lib.get_error();
        record_phase("exec.load_lib", phase_start);
        return result;
    }
    record_phase("exec.load_lib", phase_start);

    // Phase: Get the test entry function
    phase_start = Clock::now();
    auto test_entry = lib.get_function<TestMainFunc>("tml_test_entry");
    if (!test_entry) {
        result.error = "Failed to find tml_test_entry in shared library";
        record_phase("exec.get_symbol", phase_start);
        return result;
    }
    record_phase("exec.get_symbol", phase_start);

    // Phase: Set up output capture
    phase_start = Clock::now();
    OutputCapture capture;
    bool capture_started = capture.start();
    record_phase("exec.capture_start", phase_start);

    // Phase: Execute the test
    phase_start = Clock::now();
    try {
        result.exit_code = test_entry();
        result.success = (result.exit_code == 0);
    } catch (...) {
        result.error = "Exception during test execution";
        result.exit_code = 1;
    }
    auto end = Clock::now();
    result.duration_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - phase_start).count();
    record_phase("exec.run", phase_start);

    // Phase: Stop capturing and retrieve output
    phase_start = Clock::now();
    if (capture_started) {
        result.output = capture.stop();
    }
    record_phase("exec.capture_stop", phase_start);

    // Phase: Cleanup (library unload happens in destructor, but we measure what we can)
    phase_start = Clock::now();
    // Library cleanup happens automatically via RAII destructor
    // We record the overhead of measuring this phase
    record_phase("exec.cleanup", phase_start);

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

CompileToSharedLibResult compile_fuzz_to_shared_lib(const std::string& fuzz_file, bool verbose,
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

    // Borrow check
    borrow::BorrowChecker borrow_checker;
    auto borrow_result = borrow_checker.check_module(module);

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

    fs::path ll_output = cache_dir / (content_hash + "_fuzz.ll");
    fs::path obj_output = cache_dir / (content_hash + "_fuzz" + get_object_extension());

    // Use platform-specific extension for the shared library
    std::string lib_ext = get_shared_lib_extension();
    fs::path lib_output = cache_dir / (module_name + "_fuzz_" + cache_key + lib_ext);

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
// Compile Test to Shared Library with Phase Profiling
// ============================================================================

CompileToSharedLibResult compile_test_to_shared_lib_profiled(const std::string& test_file,
                                                             PhaseTimings* timings, bool verbose,
                                                             bool no_cache) {
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

    // Phase: Borrow check
    phase_start = Clock::now();
    borrow::BorrowChecker borrow_checker;
    auto borrow_result = borrow_checker.check_module(module);
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
    codegen::LLVMIRGen llvm_gen(env, options);

    auto gen_result = llvm_gen.generate(module);
    record_phase("codegen", phase_start);

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

    fs::path ll_output = cache_dir / (content_hash + "_shlib.ll");
    fs::path obj_output = cache_dir / (content_hash + "_shlib" + get_object_extension());

    std::string lib_ext = get_shared_lib_extension();
    fs::path lib_output = cache_dir / (module_name + "_" + cache_key + lib_ext);

    std::string clang = find_clang();
    if (clang.empty()) {
        result.error_message = "clang not found";
        record_phase("setup", phase_start);
        return result;
    }
    record_phase("setup", phase_start);

    // Phase: Compile to object (if not cached)
    phase_start = Clock::now();
    bool use_cached_obj = !no_cache && fs::exists(obj_output);

    if (!use_cached_obj) {
        std::ofstream ll_file(ll_output);
        if (!ll_file) {
            result.error_message = "Cannot write LLVM IR";
            record_phase("clang_compile", phase_start);
            return result;
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
            result.error_message = "Compilation failed: " + obj_result.error_message;
            fs::remove(ll_output);
            record_phase("clang_compile", phase_start);
            return result;
        }
        fs::remove(ll_output);
    }
    record_phase("clang_compile", phase_start);

    // Phase: Link (with cache support)
    phase_start = Clock::now();
    std::vector<fs::path> object_files;
    object_files.push_back(obj_output);

    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
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

// ============================================================================
// Suite-Based Test Compilation
// ============================================================================

// Helper to extract suite key from file path
// Returns: "compiler_tests_compiler", "compiler_tests_runtime", "lib_core_tests", etc.
static std::string extract_suite_key(const std::string& file_path) {
    fs::path path(file_path);
    std::vector<std::string> parts;

    for (auto it = path.begin(); it != path.end(); ++it) {
        parts.push_back(it->string());
    }

    // Find the project root marker ("tml" directory or start of relative path)
    size_t start_idx = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i] == "tml") {
            start_idx = i + 1;
            break;
        }
    }

    // Build suite key from path components
    // For "compiler/tests/compiler/foo.test.tml" -> "compiler_tests_compiler"
    // For "lib/core/tests/bar.test.tml" -> "lib_core_tests"
    std::string key;
    for (size_t i = start_idx; i < parts.size() - 1; ++i) { // -1 to exclude filename
        if (!key.empty()) {
            key += "_";
        }
        key += parts[i];

        // Stop after "tests" directory or after 3 components
        if (parts[i] == "tests" || i - start_idx >= 2) {
            break;
        }
    }

    return key.empty() ? "default" : key;
}

// Helper to extract display group from suite key
static std::string suite_key_to_group(const std::string& key) {
    // "compiler_tests_compiler" -> "compiler"
    // "lib_core_tests" -> "lib/core"
    // "compiler_tests_runtime" -> "runtime"

    if (key.find("compiler_tests_") == 0) {
        return key.substr(15); // After "compiler_tests_"
    }
    if (key.find("lib_") == 0) {
        // "lib_core_tests" -> "lib/core"
        auto pos = key.find("_tests");
        if (pos != std::string::npos) {
            std::string lib_part = key.substr(4, pos - 4); // "core" from "lib_core_tests"
            return "lib/" + lib_part;
        }
    }
    return key;
}

std::vector<TestSuite> group_tests_into_suites(const std::vector<std::string>& test_files) {
    // Group files by suite key
    std::map<std::string, std::vector<std::string>> groups;
    for (const auto& file : test_files) {
        std::string key = extract_suite_key(file);
        groups[key].push_back(file);
    }

    // Convert to TestSuite structures
    std::vector<TestSuite> suites;
    for (auto& [key, files] : groups) {
        TestSuite suite;
        suite.name = key;
        suite.group = suite_key_to_group(key);

        // Sort files for deterministic ordering
        std::sort(files.begin(), files.end());

        for (size_t i = 0; i < files.size(); ++i) {
            SuiteTestInfo info;
            info.file_path = files[i];
            info.test_name = fs::path(files[i]).stem().string();
            // Entry function will be: tml_test_0, tml_test_1, etc.
            info.entry_func_name = "tml_test_" + std::to_string(i);
            info.test_count = tester::count_tests_in_file(files[i]);
            suite.tests.push_back(std::move(info));
        }

        suites.push_back(std::move(suite));
    }

    // Sort suites by name for consistent ordering
    std::sort(suites.begin(), suites.end(),
              [](const TestSuite& a, const TestSuite& b) { return a.name < b.name; });

    return suites;
}

SuiteCompileResult compile_test_suite(const TestSuite& suite, bool verbose, bool no_cache) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    SuiteCompileResult result;

    if (suite.tests.empty()) {
        result.success = true;
        return result;
    }

    fs::path cache_dir = get_run_cache_dir();
    std::string clang = find_clang();
    if (clang.empty()) {
        result.error_message = "clang not found";
        return result;
    }

    // Compile each test file to an object file with indexed entry point
    std::vector<fs::path> object_files;
    std::vector<std::string> link_libs;
    std::string combined_hash;

    for (size_t i = 0; i < suite.tests.size(); ++i) {
        const auto& test = suite.tests[i];

        // Read source
        std::string source_code;
        try {
            source_code = read_file(test.file_path);
        } catch (const std::exception&) {
            result.error_message = "Failed to read: " + test.file_path;
            result.failed_test = test.file_path;
            return result;
        }

        // Generate content hash for this file
        std::string content_hash = build::generate_content_hash(source_code);
        combined_hash += content_hash;

        // Check for cached object
        std::string obj_name = content_hash + "_suite_" + std::to_string(i);
        fs::path obj_output = cache_dir / (obj_name + get_object_extension());

        bool use_cached = !no_cache && fs::exists(obj_output);

        if (!use_cached) {
            // Lex
            auto source = lexer::Source::from_string(source_code, test.file_path);
            lexer::Lexer lex(source);
            auto tokens = lex.tokenize();
            if (lex.has_errors()) {
                result.error_message = "Lexer errors";
                result.failed_test = test.file_path;
                return result;
            }

            // Parse
            parser::Parser parser(std::move(tokens));
            auto module_name = fs::path(test.file_path).stem().string();
            auto parse_result = parser.parse_module(module_name);
            if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
                result.error_message = "Parser errors";
                result.failed_test = test.file_path;
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
                result.failed_test = test.file_path;
                return result;
            }
            const auto& env = std::get<types::TypeEnv>(check_result);

            // Borrow check
            borrow::BorrowChecker borrow_checker;
            auto borrow_result = borrow_checker.check_module(module);
            if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
                result.error_message = "Borrow check errors";
                result.failed_test = test.file_path;
                return result;
            }

            // Codegen with indexed entry point
            codegen::LLVMGenOptions options;
            options.emit_comments = false;
            options.generate_dll_entry = true;
            options.suite_test_index = static_cast<int>(i); // tml_test_N
            options.dll_export = true;
            options.force_internal_linkage = true; // Internal linkage for all non-entry functions
            options.emit_debug_info = CompilerOptions::debug_info;
            options.debug_level = CompilerOptions::debug_level;
            options.source_file = test.file_path;
            codegen::LLVMIRGen llvm_gen(env, options);

            auto gen_result = llvm_gen.generate(module);
            if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
                result.error_message = "Codegen errors";
                result.failed_test = test.file_path;
                return result;
            }

            const auto& llvm_ir = std::get<std::string>(gen_result);

            // Collect link libraries
            for (const auto& lib : llvm_gen.get_link_libs()) {
                if (std::find(link_libs.begin(), link_libs.end(), lib) == link_libs.end()) {
                    link_libs.push_back(lib);
                }
            }

            // Write IR and compile to object
            fs::path ll_output = cache_dir / (obj_name + ".ll");
            std::ofstream ll_file(ll_output);
            if (!ll_file) {
                result.error_message = "Cannot write LLVM IR";
                result.failed_test = test.file_path;
                return result;
            }
            ll_file << llvm_ir;
            ll_file.close();

            ObjectCompileOptions obj_options;
            obj_options.optimization_level = CompilerOptions::optimization_level;
            obj_options.debug_info = CompilerOptions::debug_info;
            obj_options.verbose = false;

            auto obj_result = compile_ll_to_object(ll_output, obj_output, clang, obj_options);
            fs::remove(ll_output);

            if (!obj_result.success) {
                result.error_message = "Compilation failed: " + obj_result.error_message;
                result.failed_test = test.file_path;
                return result;
            }
        }

        object_files.push_back(obj_output);
    }

    // Get runtime objects (only need to do once for the suite)
    auto registry = std::make_shared<types::ModuleRegistry>();
    // Use first module for runtime deps (they're usually the same)
    std::string source_code = read_file(suite.tests[0].file_path);
    auto source = lexer::Source::from_string(source_code, suite.tests[0].file_path);
    lexer::Lexer lex(source);
    auto tokens = lex.tokenize();
    parser::Parser parser(std::move(tokens));
    auto parse_result = parser.parse_module(fs::path(suite.tests[0].file_path).stem().string());
    const auto& module = std::get<parser::Module>(parse_result);

    std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());
    auto runtime_objects = get_runtime_objects(registry, module, deps_cache, clang, verbose);
    object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

    // Generate suite hash for caching
    std::string suite_hash = generate_content_hash(combined_hash);
    std::string exe_hash = generate_exe_hash(suite_hash, object_files);

    std::string lib_ext = get_shared_lib_extension();
    fs::path cached_dll = cache_dir / (exe_hash + "_suite" + lib_ext);
    fs::path lib_output = cache_dir / (suite.name + lib_ext);

    bool use_cached_dll = !no_cache && fs::exists(cached_dll);

    if (!use_cached_dll) {
        // Link as shared library
        LinkOptions link_options;
        link_options.output_type = LinkOptions::OutputType::DynamicLib;
        link_options.verbose = false;

        for (const auto& lib : link_libs) {
            if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                link_options.link_flags.push_back("\"" + lib + "\"");
            } else {
                link_options.link_flags.push_back("-l" + lib);
            }
        }

        auto link_result = link_objects(object_files, cached_dll, clang, link_options);
        if (!link_result.success) {
            result.error_message = "Linking failed: " + link_result.error_message;
            return result;
        }
    }

    // Copy to output location
    if (!fast_copy_file(cached_dll, lib_output)) {
        result.error_message = "Failed to copy DLL";
        return result;
    }

    auto end = Clock::now();
    result.success = true;
    result.dll_path = lib_output.string();
    result.compile_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return result;
}

SuiteCompileResult compile_test_suite_profiled(const TestSuite& suite, PhaseTimings* timings,
                                               bool verbose, bool no_cache) {
    // For now, just use the regular compile and record total time
    // Detailed phase profiling can be added later if needed
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    auto result = compile_test_suite(suite, verbose, no_cache);

    if (timings) {
        auto end = Clock::now();
        timings->timings_us["suite_compile"] =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    return result;
}

SuiteTestResult run_suite_test(DynamicLibrary& lib, int test_index) {
    SuiteTestResult result;

    // Get the indexed test function
    std::string func_name = "tml_test_" + std::to_string(test_index);
    auto test_func = lib.get_function<TestMainFunc>(func_name.c_str());
    if (!test_func) {
        result.error = "Failed to find " + func_name + " in suite DLL";
        return result;
    }

    // Set up output capture
    OutputCapture capture;
    bool capture_started = capture.start();

    // Execute the test
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    try {
        result.exit_code = test_func();
        result.success = (result.exit_code == 0);
    } catch (...) {
        result.error = "Exception during test execution";
        result.exit_code = 1;
    }

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (capture_started) {
        result.output = capture.stop();
    }

    return result;
}

SuiteTestResult run_suite_test_profiled(DynamicLibrary& lib, int test_index,
                                        PhaseTimings* timings) {
    using Clock = std::chrono::high_resolution_clock;
    auto record_phase = [&](const std::string& phase, Clock::time_point start) {
        if (timings) {
            auto end = Clock::now();
            timings->timings_us[phase] =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }
    };

    SuiteTestResult result;

    // Phase: Get the indexed test function
    auto phase_start = Clock::now();
    std::string func_name = "tml_test_" + std::to_string(test_index);
    auto test_func = lib.get_function<TestMainFunc>(func_name.c_str());
    if (!test_func) {
        result.error = "Failed to find " + func_name + " in suite DLL";
        record_phase("exec.get_symbol", phase_start);
        return result;
    }
    record_phase("exec.get_symbol", phase_start);

    // Phase: Set up output capture
    phase_start = Clock::now();
    OutputCapture capture;
    bool capture_started = capture.start();
    record_phase("exec.capture_start", phase_start);

    // Phase: Execute the test
    phase_start = Clock::now();
    try {
        result.exit_code = test_func();
        result.success = (result.exit_code == 0);
    } catch (...) {
        result.error = "Exception during test execution";
        result.exit_code = 1;
    }
    result.duration_us =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start).count();
    record_phase("exec.run", phase_start);

    // Phase: Stop capture
    phase_start = Clock::now();
    if (capture_started) {
        result.output = capture.stop();
    }
    record_phase("exec.capture_stop", phase_start);

    return result;
}

} // namespace tml::cli
