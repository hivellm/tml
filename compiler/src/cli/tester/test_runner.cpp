//! # Test Runner Infrastructure
//!
//! This file provides the core infrastructure for running TML tests in-process.
//!
//! ## In-Process Testing
//!
//! Tests are compiled to shared libraries (DLLs) and loaded into the test process:
//!
//! ```text
//! test.tml → LLVM IR → .obj → .dll → dlopen() → tml_test_entry()
//! ```
//!
//! This avoids subprocess overhead and enables faster test execution.
//!
//! ## Suite Mode
//!
//! Multiple tests can be compiled into a single DLL per suite:
//!
//! ```text
//! suite.dll
//!   ├─ tml_test_0() → test_foo.tml
//!   ├─ tml_test_1() → test_bar.tml
//!   └─ tml_test_2() → test_baz.tml
//! ```
//!
//! ## Key Functions
//!
//! | Function                          | Purpose                              |
//! |-----------------------------------|--------------------------------------|
//! | `compile_test_to_shared_lib()`    | Compile single test to DLL           |
//! | `run_test_in_process()`           | Execute DLL's tml_test_entry()       |
//! | `compile_test_suite()`            | Compile multiple tests to one DLL    |
//! | `run_suite_test()`                | Execute indexed test from suite DLL  |
//!
//! ## Output Capture
//!
//! `OutputCapture` redirects stdout/stderr to a temp file during test execution,
//! then restores original file descriptors and reads captured output.

#include "test_runner.hpp"

#include "cli/builder/builder_internal.hpp"
#include "cli/builder/object_compiler.hpp"
#include "cli/commands/cmd_build.hpp"
#include "cli/tester/tester_internal.hpp"
#include "preprocessor/preprocessor.hpp"

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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace tml::cli {

// ============================================================================
// Windows Crash Handler (at test runner level)
// ============================================================================

#ifdef _WIN32
// Forward declaration for use in SEH wrappers
using TmlRunTestWithCatch = int32_t (*)(TestMainFunc);

// Thread-local storage for crash info
static thread_local char g_crash_msg[256] = {0};
static thread_local bool g_crash_occurred = false;

static const char* get_exception_name(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "ACCESS_VIOLATION (Segmentation fault)";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "INTEGER_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
        return "INTEGER_OVERFLOW";
    case EXCEPTION_STACK_OVERFLOW:
        return "STACK_OVERFLOW";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "FLOAT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "FLOAT_INVALID_OPERATION";
    case 0xC0000028: // STATUS_BAD_STACK
        return "BAD_STACK (Stack corruption)";
    default:
        return "UNKNOWN_EXCEPTION";
    }
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;

    // Format crash message
    snprintf(g_crash_msg, sizeof(g_crash_msg), "CRASH: %s (0x%08lX)", get_exception_name(code),
             (unsigned long)code);
    g_crash_occurred = true;

    // Print to stderr immediately using low-level API for reliability
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD written;
    WriteFile(hErr, g_crash_msg, (DWORD)strlen(g_crash_msg), &written, NULL);
    WriteFile(hErr, "\n", 1, &written, NULL);
    FlushFileBuffers(hErr);

    return EXCEPTION_EXECUTE_HANDLER;
}

// SEH wrapper for calling test functions
// Can't be in same function as C++ try/catch, so it's a separate function
// Returns: function result, or -2 on crash
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#endif
static int call_test_with_seh(TestMainFunc func) {
    g_crash_occurred = false;
    g_crash_msg[0] = '\0';

    int result = 0;
    __try {
        result = func();
    } __except (crash_filter(GetExceptionInformation())) {
        return -2;
    }
    return result;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif

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

        // Sync C++ streams with C stdio and flush all buffers
        std::ios_base::sync_with_stdio(true);
        std::cout << std::flush;
        std::cerr << std::flush;
        std::fflush(stdout);
        std::fflush(stderr);

#ifdef _WIN32
        // Save original stdout/stderr file descriptors
        saved_stdout_ = _dup(_fileno(stdout));
        saved_stderr_ = _dup(_fileno(stderr));

        if (saved_stdout_ < 0 || saved_stderr_ < 0) {
            return false;
        }

        // Open temp file for capturing output
        int temp_fd = -1;
        errno_t err = _sopen_s(&temp_fd, temp_file_path_.string().c_str(),
                               _O_WRONLY | _O_CREAT | _O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (err != 0 || temp_fd < 0) {
            _close(saved_stdout_);
            _close(saved_stderr_);
            saved_stdout_ = -1;
            saved_stderr_ = -1;
            return false;
        }

        // Redirect stdout/stderr to temp file
        _dup2(temp_fd, _fileno(stdout));
        _dup2(temp_fd, _fileno(stderr));
        _close(temp_fd);
#else
        // Save original stdout/stderr file descriptors
        saved_stdout_ = dup(STDOUT_FILENO);
        saved_stderr_ = dup(STDERR_FILENO);

        if (saved_stdout_ < 0 || saved_stderr_ < 0) {
            return false;
        }

        // Open temp file for capturing output
        int temp_fd = open(temp_file_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (temp_fd < 0) {
            close(saved_stdout_);
            close(saved_stderr_);
            saved_stdout_ = -1;
            saved_stderr_ = -1;
            return false;
        }

        // Redirect stdout/stderr to temp file
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

        // Ensure C++ streams are synced and flushed
        std::ios_base::sync_with_stdio(true);
        std::cout << std::flush;
        std::cerr << std::flush;
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

        saved_stdout_ = -1;
        saved_stderr_ = -1;
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

    // Borrow check
    borrow::BorrowChecker borrow_checker(env);
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

    // Note: clang may be empty if LLVM backend is available (self-contained mode)
    std::string clang = find_clang();

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
    borrow::BorrowChecker borrow_checker(env);
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

    // Note: clang may be empty if LLVM backend is available (self-contained mode)
    std::string clang = find_clang();

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
    borrow::BorrowChecker borrow_checker(env);
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

    // Note: clang may be empty if LLVM backend is available (self-contained mode)
    std::string clang = find_clang();
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
    // Maximum tests per suite to avoid DLL conflicts/hangs
    // Empirically determined: suites with many tests can hang on Windows
    constexpr size_t MAX_TESTS_PER_SUITE = 5;

    // Group files by suite key
    std::map<std::string, std::vector<std::string>> groups;
    for (const auto& file : test_files) {
        std::string key = extract_suite_key(file);
        groups[key].push_back(file);
    }

    // Convert to TestSuite structures, splitting large groups into chunks
    std::vector<TestSuite> suites;
    for (auto& [key, files] : groups) {
        // Sort files for deterministic ordering
        std::sort(files.begin(), files.end());

        // Split into chunks of MAX_TESTS_PER_SUITE
        size_t chunk_count = (files.size() + MAX_TESTS_PER_SUITE - 1) / MAX_TESTS_PER_SUITE;

        for (size_t chunk = 0; chunk < chunk_count; ++chunk) {
            TestSuite suite;
            if (chunk_count > 1) {
                suite.name = key + "_" + std::to_string(chunk + 1);
            } else {
                suite.name = key;
            }
            suite.group = suite_key_to_group(key);

            size_t start_idx = chunk * MAX_TESTS_PER_SUITE;
            size_t end_idx = std::min(start_idx + MAX_TESTS_PER_SUITE, files.size());

            for (size_t i = start_idx; i < end_idx; ++i) {
                SuiteTestInfo info;
                info.file_path = files[i];
                info.test_name = fs::path(files[i]).stem().string();
                // Entry function will be: tml_test_0, tml_test_1, etc. (within this chunk)
                info.entry_func_name = "tml_test_" + std::to_string(i - start_idx);
                info.test_count = tester::count_tests_in_file(files[i]);
                suite.tests.push_back(std::move(info));
            }

            suites.push_back(std::move(suite));
        }
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

    try {

        if (suite.tests.empty()) {
            result.success = true;
            return result;
        }

        fs::path cache_dir = get_run_cache_dir();
        // Note: clang may be empty if LLVM backend is available (self-contained mode)
        std::string clang = find_clang();

        // Create a SHARED ModuleRegistry for all tests in this suite
        // This prevents re-parsing the same library modules for each test file
        auto shared_registry = std::make_shared<types::ModuleRegistry>();

        // ======================================================================
        // EARLY CACHE CHECK: Compute source hash first to skip typechecking
        // ======================================================================
        // If the DLL is already cached (same source content), we can skip ALL
        // compilation including type checking. This dramatically speeds up cached runs.

        std::string combined_hash;
        std::string lib_ext = get_shared_lib_extension();
        fs::path lib_output = cache_dir / (suite.name + lib_ext);

        // First pass: just preprocess and compute content hashes (fast)
        for (const auto& test : suite.tests) {
            std::string source_code;
            try {
                source_code = read_file(test.file_path);
            } catch (const std::exception&) {
                result.error_message = "Failed to read: " + test.file_path;
                result.failed_test = test.file_path;
                return result;
            }

            auto pp_config = preprocessor::Preprocessor::host_config();
            preprocessor::Preprocessor pp(pp_config);
            auto pp_result = pp.process(source_code, test.file_path);

            if (!pp_result.success()) {
                std::ostringstream oss;
                oss << "Preprocessor errors in " << test.file_path << ":\n";
                for (const auto& diag : pp_result.diagnostics) {
                    if (diag.severity == preprocessor::DiagnosticSeverity::Error) {
                        oss << "  " << diag.line << ":" << diag.column << ": " << diag.message
                            << "\n";
                    }
                }
                result.error_message = oss.str();
                result.failed_test = test.file_path;
                return result;
            }

            combined_hash += build::generate_content_hash(pp_result.output);
        }

        // Check for cached DLL using source-only hash (before typechecking)
        std::string source_hash = build::generate_content_hash(combined_hash);
        fs::path cached_dll_by_source = cache_dir / (source_hash + "_suite" + lib_ext);

        if (!no_cache && fs::exists(cached_dll_by_source)) {
            // Cache hit! Skip all typechecking and compilation
            if (verbose) {
                std::cerr << "[DEBUG] EARLY CACHE HIT - skipping compilation\n";
            }
            if (!fast_copy_file(cached_dll_by_source, lib_output)) {
                result.error_message = "Failed to copy cached DLL";
                return result;
            }

            auto end = Clock::now();
            result.success = true;
            result.dll_path = lib_output.string();
            result.compile_time_us =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            return result;
        }

        // ======================================================================
        // FULL COMPILATION: Cache miss, do full lex/parse/typecheck/codegen
        // ======================================================================

        // Reset combined_hash for per-file tracking in full compilation
        combined_hash.clear();

        // Compile each test file to an object file with indexed entry point
        std::vector<fs::path> object_files;
        std::vector<std::string> link_libs;

        for (size_t i = 0; i < suite.tests.size(); ++i) {
            const auto& test = suite.tests[i];

            if (verbose) {
                std::cerr << "[DEBUG]   Compiling test " << (i + 1) << "/" << suite.tests.size()
                          << ": " << test.file_path << "\n"
                          << std::flush;
            }

            try {
                // Read source
                std::string source_code;
                try {
                    source_code = read_file(test.file_path);
                } catch (const std::exception&) {
                    result.error_message = "Failed to read: " + test.file_path;
                    result.failed_test = test.file_path;
                    return result;
                }

                // Preprocess the source code (handles #if, #ifdef, etc.)
                auto pp_config = preprocessor::Preprocessor::host_config();
                preprocessor::Preprocessor pp(pp_config);
                auto pp_result = pp.process(source_code, test.file_path);

                if (!pp_result.success()) {
                    std::ostringstream oss;
                    oss << "Preprocessor errors in " << test.file_path << ":\n";
                    for (const auto& diag : pp_result.diagnostics) {
                        if (diag.severity == preprocessor::DiagnosticSeverity::Error) {
                            oss << "  " << diag.line << ":" << diag.column << ": " << diag.message
                                << "\n";
                        }
                    }
                    result.error_message = oss.str();
                    result.failed_test = test.file_path;
                    return result;
                }

                // Use preprocessed source for subsequent steps
                std::string preprocessed_source = pp_result.output;

                // Generate content hash for this file (use preprocessed source for accurate
                // caching)
                std::string content_hash = build::generate_content_hash(preprocessed_source);
                combined_hash += content_hash;

                // Check for cached object
                std::string obj_name = content_hash + "_suite_" + std::to_string(i);
                fs::path obj_output = cache_dir / (obj_name + get_object_extension());

                bool use_cached = !no_cache && fs::exists(obj_output);

                // ALWAYS lex/parse/typecheck to populate the shared_registry with imports
                // This ensures get_runtime_objects can find all required modules (e.g., JSON)
                // even when using cached object files.

                // Lex (use preprocessed source)
                auto source = lexer::Source::from_string(preprocessed_source, test.file_path);
                lexer::Lexer lex(source);
                auto tokens = lex.tokenize();
                if (lex.has_errors()) {
                    std::ostringstream oss;
                    oss << "Lexer errors in " << test.file_path << ":\n";
                    for (const auto& err : lex.errors()) {
                        oss << "  " << err.span.start.line << ":" << err.span.start.column << ": "
                            << err.message << "\n";
                    }
                    result.error_message = oss.str();
                    result.failed_test = test.file_path;
                    return result;
                }

                // Parse
                parser::Parser parser(std::move(tokens));
                auto module_name = fs::path(test.file_path).stem().string();
                auto parse_result = parser.parse_module(module_name);
                if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
                    const auto& errors = std::get<std::vector<parser::ParseError>>(parse_result);
                    std::ostringstream oss;
                    oss << "Parser errors in " << test.file_path << ":\n";
                    for (const auto& err : errors) {
                        oss << "  " << err.span.start.line << ":" << err.span.start.column << ": "
                            << err.message << "\n";
                    }
                    result.error_message = oss.str();
                    result.failed_test = test.file_path;
                    return result;
                }
                const auto& module = std::get<parser::Module>(parse_result);

                // Type check - use shared registry to avoid re-parsing modules for each test
                // This populates the registry with imported modules for later use by
                // get_runtime_objects
                types::TypeChecker checker;
                checker.set_module_registry(shared_registry);
                auto check_result = checker.check_module(module);
                if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
                    const auto& errors = std::get<std::vector<types::TypeError>>(check_result);
                    std::ostringstream oss;
                    oss << "Type errors in " << test.file_path << ":\n";
                    for (const auto& err : errors) {
                        oss << "  " << err.span.start.line << ":" << err.span.start.column << ": "
                            << err.message << "\n";
                    }
                    result.error_message = oss.str();
                    result.failed_test = test.file_path;
                    return result;
                }
                const auto& env = std::get<types::TypeEnv>(check_result);

                // Only do codegen and object compilation if not cached
                if (!use_cached) {

                    // Borrow check
                    borrow::BorrowChecker borrow_checker(env);
                    auto borrow_result = borrow_checker.check_module(module);
                    if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
                        const auto& errors =
                            std::get<std::vector<borrow::BorrowError>>(borrow_result);
                        std::ostringstream oss;
                        oss << "Borrow check errors in " << test.file_path << ":\n";
                        for (const auto& err : errors) {
                            oss << "  " << err.span.start.line << ":" << err.span.start.column
                                << ": " << err.message << "\n";
                        }
                        result.error_message = oss.str();
                        result.failed_test = test.file_path;
                        return result;
                    }

                    // Codegen with indexed entry point
                    codegen::LLVMGenOptions options;
                    options.emit_comments = false;
                    options.generate_dll_entry = true;
                    options.suite_test_index = static_cast<int>(i); // tml_test_N
                    options.dll_export = true;
                    options.force_internal_linkage =
                        true; // Internal linkage for all non-entry functions
                    options.emit_debug_info = CompilerOptions::debug_info;
                    options.debug_level = CompilerOptions::debug_level;
                    options.source_file = test.file_path;
                    codegen::LLVMIRGen llvm_gen(env, options);

                    auto gen_result = llvm_gen.generate(module);
                    if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
                        const auto& errors =
                            std::get<std::vector<codegen::LLVMGenError>>(gen_result);
                        std::ostringstream oss;
                        oss << "Codegen errors in " << test.file_path << ":\n";
                        for (const auto& err : errors) {
                            oss << "  " << err.span.start.line << ":" << err.span.start.column
                                << ": " << err.message << "\n";
                        }
                        result.error_message = oss.str();
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

                    auto obj_result =
                        compile_ll_to_object(ll_output, obj_output, clang, obj_options);
                    fs::remove(ll_output);

                    if (!obj_result.success) {
                        result.error_message = "Compilation failed: " + obj_result.error_message;
                        result.failed_test = test.file_path;
                        return result;
                    }
                }

                object_files.push_back(obj_output);
            } catch (const std::exception& e) {
                result.error_message =
                    "Exception while compiling " + test.file_path + ": " + e.what();
                result.failed_test = test.file_path;
                return result;
            } catch (...) {
                result.error_message = "Unknown exception while compiling " + test.file_path;
                result.failed_test = test.file_path;
                return result;
            }
        }

        // Get runtime objects (only need to do once for the suite)
        // Reuse shared_registry which already has all loaded modules
        // Use first module for runtime deps (they're usually the same)
        std::string rt_source_code = read_file(suite.tests[0].file_path);

        // Preprocess the source (required for #if WINDOWS etc.)
        auto rt_pp_config = preprocessor::Preprocessor::host_config();
        preprocessor::Preprocessor rt_pp(rt_pp_config);
        auto rt_pp_result = rt_pp.process(rt_source_code, suite.tests[0].file_path);
        std::string rt_preprocessed = rt_pp_result.success() ? rt_pp_result.output : rt_source_code;

        auto source = lexer::Source::from_string(rt_preprocessed, suite.tests[0].file_path);
        lexer::Lexer lex(source);
        auto tokens = lex.tokenize();
        parser::Parser parser(std::move(tokens));
        auto parse_result = parser.parse_module(fs::path(suite.tests[0].file_path).stem().string());
        const auto& module = std::get<parser::Module>(parse_result);

        std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

        auto runtime_objects =
            get_runtime_objects(shared_registry, module, deps_cache, clang, verbose);
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

        // Generate suite hash for full caching (includes runtime objects)
        std::string suite_hash = generate_content_hash(combined_hash);
        std::string exe_hash = generate_exe_hash(suite_hash, object_files);

        fs::path cached_dll = cache_dir / (exe_hash + "_suite" + lib_ext);

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

        // Always save with source-only hash for early cache check on next run
        // This allows skipping typechecking entirely when source hasn't changed
        // Do this even when using cached_dll so we populate the source-hash cache
        if (!fs::exists(cached_dll_by_source)) {
            try {
                fs::copy_file(cached_dll, cached_dll_by_source,
                              fs::copy_options::overwrite_existing);
            } catch (...) {
                // Ignore errors creating source-hash cache
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

    } catch (const std::exception& e) {
        result.error_message = "FATAL EXCEPTION during suite compilation: " + std::string(e.what());
        if (!suite.tests.empty()) {
            result.failed_test = suite.tests[0].file_path;
        }
        std::cerr << "\n[FATAL] Exception in compile_test_suite: " << e.what() << "\n";
        return result;
    } catch (...) {
        result.error_message = "FATAL UNKNOWN EXCEPTION during suite compilation";
        if (!suite.tests.empty()) {
            result.failed_test = suite.tests[0].file_path;
        }
        std::cerr << "\n[FATAL] Unknown exception in compile_test_suite\n";
        return result;
    }
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

// Function pointer type for tml_run_test_with_catch from runtime
using TmlRunTestWithCatch = int32_t (*)(TestMainFunc);

SuiteTestResult run_suite_test(DynamicLibrary& lib, int test_index, bool verbose) {
    SuiteTestResult result;

    // Flush output to help debug crashes
    std::cout << std::flush;
    std::cerr << std::flush;
    std::fflush(stdout);
    std::fflush(stderr);

    // Get the indexed test function
    std::string func_name = "tml_test_" + std::to_string(test_index);
    if (verbose) {
        std::cerr << "[DEBUG]   Looking up symbol: " << func_name << "\n" << std::flush;
    }
    auto test_func = lib.get_function<TestMainFunc>(func_name.c_str());
    if (!test_func) {
        result.error = "Failed to find " + func_name + " in suite DLL";
        std::cerr << "[ERROR] " << result.error << "\n" << std::flush;
        return result;
    }

    // Try to get the panic-catching wrapper from the runtime
    auto run_with_catch = lib.get_function<TmlRunTestWithCatch>("tml_run_test_with_catch");
    if (verbose) {
        std::cerr << "[DEBUG]   tml_run_test_with_catch: "
                  << (run_with_catch ? "found" : "NOT FOUND") << "\n"
                  << std::flush;
    }

    // Get output suppression function from runtime (to suppress test output when not verbose)
    using TmlSetOutputSuppressed = void (*)(int32_t);
    auto set_output_suppressed =
        lib.get_function<TmlSetOutputSuppressed>("tml_set_output_suppressed");
    if (verbose) {
        std::cerr << "[DEBUG]   tml_set_output_suppressed: "
                  << (set_output_suppressed ? "found" : "NOT FOUND") << "\n"
                  << std::flush;
    }

    // Suppress output when not in verbose mode
    if (!verbose) {
        if (set_output_suppressed) {
            set_output_suppressed(1);
        }
        // Also flush to ensure no buffered output appears
        std::fflush(stdout);
        std::fflush(stderr);
    }

    // Set up output capture (skip in verbose mode to see crash output directly)
    OutputCapture capture;
    bool capture_started = verbose ? false : capture.start();

    // Execute the test
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    if (verbose) {
        std::cerr << "[DEBUG]   Executing test function...\n" << std::flush;
    }

    // Ensure output is flushed before test execution in case of crash
    std::cout << std::flush;
    std::cerr << std::flush;
    std::fflush(stdout);
    std::fflush(stderr);

    // Execute test with crash protection
    // The runtime's tml_run_test_with_catch handles both panics (via setjmp/longjmp)
    // and crashes (via exception filter on Windows, signal handlers on Unix).
    //
    // IMPORTANT: On Windows, do NOT wrap tml_run_test_with_catch in SEH (__try/__except)
    // because combining SEH with setjmp/longjmp causes BAD_STACK (0xC0000028).
    // The runtime's exception filter handles crashes, so SEH is not needed.
    if (run_with_catch) {
        if (verbose) {
            std::cerr << "[DEBUG]   Calling tml_run_test_with_catch wrapper...\n" << std::flush;
        }
        result.exit_code = run_with_catch(test_func);
        if (result.exit_code == -1) {
            // Panic was caught
            result.success = false;
            result.error = "Test panicked";
        } else if (result.exit_code == -2) {
            // Crash was caught
            result.success = false;
            result.error = "Test crashed (SIGSEGV/SIGFPE/etc)";
        } else {
            result.success = (result.exit_code == 0);
        }
        if (verbose) {
            std::cerr << "[DEBUG]   tml_run_test_with_catch returned: " << result.exit_code << "\n"
                      << std::flush;
        }
    } else {
        // Fallback: direct call with platform-specific crash protection
#ifdef _WIN32
        if (verbose) {
            std::cerr << "[DEBUG]   Calling test function with SEH protection...\n" << std::flush;
        }
        result.exit_code = call_test_with_seh(test_func);
        if (g_crash_occurred) {
            result.success = false;
            result.error = std::string("Test crashed: ") + g_crash_msg;
        } else {
            result.success = (result.exit_code == 0);
        }
#else
        // Unix: direct call (may exit on panic)
        result.exit_code = test_func();
        result.success = (result.exit_code == 0);
#endif
        if (verbose) {
            std::cerr << "[DEBUG]   Test returned: " << result.exit_code << "\n" << std::flush;
        }
    }

    if (verbose) {
        std::cerr << "[DEBUG]   Test execution complete, exit_code=" << result.exit_code << "\n"
                  << std::flush;
    }

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (capture_started) {
        result.output = capture.stop();
    }

    // Restore output after test (important for error messages)
    if (!verbose && set_output_suppressed) {
        set_output_suppressed(0);
    }

    return result;
}

SuiteTestResult run_suite_test_profiled(DynamicLibrary& lib, int test_index, PhaseTimings* timings,
                                        bool /*verbose*/) {
    // Note: verbose is unused here - profiled version just times, no debug output
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

    // Try to get the panic-catching wrapper from the runtime
    auto run_with_catch = lib.get_function<TmlRunTestWithCatch>("tml_run_test_with_catch");

    // Get output suppression function from runtime
    using TmlSetOutputSuppressed = void (*)(int32_t);
    auto set_output_suppressed =
        lib.get_function<TmlSetOutputSuppressed>("tml_set_output_suppressed");
    record_phase("exec.get_symbol", phase_start);

    // Suppress output for profiled tests (cleaner profiling output)
    if (set_output_suppressed) {
        set_output_suppressed(1);
    }

    // Phase: Set up output capture
    phase_start = Clock::now();
    OutputCapture capture;
    bool capture_started = capture.start();
    record_phase("exec.capture_start", phase_start);

    // Phase: Execute the test
    // Use tml_run_test_with_catch if available (handles panic and crashes)
    // On Windows, do NOT wrap in SEH to avoid BAD_STACK issues with setjmp/longjmp
    phase_start = Clock::now();
    if (run_with_catch) {
        result.exit_code = run_with_catch(test_func);
        if (result.exit_code == -1) {
            result.success = false;
            result.error = "Test panicked";
        } else if (result.exit_code == -2) {
            result.success = false;
            result.error = "Test crashed";
        } else {
            result.success = (result.exit_code == 0);
        }
    } else {
        // Fallback: direct call with platform-specific crash protection
#ifdef _WIN32
        result.exit_code = call_test_with_seh(test_func);
        if (g_crash_occurred) {
            result.success = false;
            result.error = std::string("Test crashed: ") + g_crash_msg;
        } else {
            result.success = (result.exit_code == 0);
        }
#else
        try {
            result.exit_code = test_func();
            result.success = (result.exit_code == 0);
        } catch (...) {
            result.error = "Exception during test execution";
            result.exit_code = 1;
        }
#endif
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

    // Restore output after test
    if (set_output_suppressed) {
        set_output_suppressed(0);
    }

    return result;
}

} // namespace tml::cli
