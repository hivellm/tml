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
#include "types/module_binary.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

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

/// Calculate thread count for internal compilation parallelism.
/// Returns 4-8 threads based on hardware cores, never exceeding 50% of total cores.
static unsigned int calc_codegen_threads(unsigned int task_count) {
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0)
        hw = 8; // Fallback for unknown hardware
    unsigned int half_cores = hw / 2;
    unsigned int clamped = std::clamp(half_cores, 4u, 8u);
    return std::min(clamped, task_count);
}

// ============================================================================
// C Runtime Logger Bridge
// ============================================================================

/// Callback that routes C runtime log messages through the C++ Logger.
/// Set via rt_log_set_callback() when loading test DLLs.
static void rt_log_bridge_callback(int level, const char* module, const char* message) {
    auto cpp_level = static_cast<tml::log::LogLevel>(level);
    tml::log::Logger::instance().log(cpp_level, module ? module : "runtime", message ? message : "",
                                     nullptr, 0);
}

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

    // Add vcpkg bin directories to DLL search path for dependencies (zstd, brotli, zlib)
    // This is needed because the test DLL depends on these libraries
    static bool vcpkg_paths_added = false;
    if (!vcpkg_paths_added) {
        vcpkg_paths_added = true;
        // Try common vcpkg bin locations
        std::vector<std::string> vcpkg_paths = {"src/x64-windows/bin", "../src/x64-windows/bin",
                                                "../../src/x64-windows/bin"};
        for (const auto& vcpkg_path : vcpkg_paths) {
            if (fs::exists(vcpkg_path)) {
                fs::path abs_vcpkg_path = fs::absolute(vcpkg_path);
                AddDllDirectory(abs_vcpkg_path.wstring().c_str());
            }
        }
    }

    // Use LoadLibraryExW with optimized flags:
    // - LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR: Search only the DLL's directory for dependencies
    // - LOAD_LIBRARY_SEARCH_DEFAULT_DIRS: Also search system directories
    // - LOAD_LIBRARY_SEARCH_USER_DIRS: Search directories added with AddDllDirectory
    // This avoids searching the entire PATH which can be slow
    handle_ = LoadLibraryExW(wpath.c_str(), nullptr,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                                 LOAD_LIBRARY_SEARCH_USER_DIRS);
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
        // If coverage is enabled, write profile data before unloading
        // __llvm_profile_write_file() is provided by the LLVM profile runtime
        if (CompilerOptions::coverage_source) {
#ifdef _WIN32
            auto write_profile =
                reinterpret_cast<int (*)()>(GetProcAddress(handle_, "__llvm_profile_write_file"));
#else
            auto write_profile =
                reinterpret_cast<int (*)()>(dlsym(handle_, "__llvm_profile_write_file"));
#endif
            if (write_profile) {
                write_profile();
            }
        }

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
        obj_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

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

    // Route C runtime log messages through the C++ Logger
    using RtLogSetCallback = void (*)(void (*)(int, const char*, const char*));
    auto set_log_callback = lib.get_function<RtLogSetCallback>("rt_log_set_callback");
    if (set_log_callback) {
        set_log_callback(rt_log_bridge_callback);
    }
    using RtLogSetLevel = void (*)(int);
    auto set_log_level = lib.get_function<RtLogSetLevel>("rt_log_set_level");
    if (set_log_level) {
        set_log_level(static_cast<int>(tml::log::Logger::instance().level()));
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

    // Route C runtime log messages through the C++ Logger
    {
        using RtLogSetCallback = void (*)(void (*)(int, const char*, const char*));
        auto set_log_callback = lib.get_function<RtLogSetCallback>("rt_log_set_callback");
        if (set_log_callback) {
            set_log_callback(rt_log_bridge_callback);
        }
        using RtLogSetLevel = void (*)(int);
        auto set_log_level = lib.get_function<RtLogSetLevel>("rt_log_set_level");
        if (set_log_level) {
            set_log_level(static_cast<int>(tml::log::Logger::instance().level()));
        }
    }

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
        obj_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

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
        obj_options.coverage = tml::CompilerOptions::coverage_source; // LLVM source coverage

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
    // "compiler_tests_compiler" -> "compiler/compiler"
    // "compiler_tests_runtime" -> "compiler/runtime"
    // "lib_core_tests" -> "lib/core"

    if (key.find("compiler_tests_") == 0) {
        std::string subdir = key.substr(15); // After "compiler_tests_"
        return "compiler/" + subdir;
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
    // Maximum tests per suite - balance between fewer DLLs and parallel compilation
    // Lower = more suites that compile faster in parallel
    // Higher = fewer DLLs but sequential within each suite
    // CRITICAL: Lowered from 15 → 8 to prevent O(n²) codegen slowdown
    // Cannot go lower than 8 without breaking atomic function dependencies
    // TODO: Fix codegen context accumulation bug (lowlevel_misc: 2.2s alone vs 98s in suite)
    constexpr size_t MAX_TESTS_PER_SUITE = 8;

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

// Slow task threshold multiplier - panic if task takes more than this times the average
constexpr double SLOW_TASK_THRESHOLD = 5.0;
// Minimum time before considering a task "slow" (avoid false positives on fast tasks)
// Increased to 45s to accommodate complex tests with heavy imports (std::sync, std::thread)
// on slower machines where compilation can take 20-30 seconds
constexpr int64_t MIN_SLOW_THRESHOLD_US = 45000000; // 45 seconds

// ============================================================================
// Precompiled Symbols Cache - DISABLED
// ============================================================================
// DISABLED: Causes linkage conflicts (see line 2305 for details)
// This code is kept for reference but commented out to avoid unused function warnings.
//
// Compiles lib/std/precompiled_symbols.tml once and caches the object file.
// This object contains common generic instantiations (Mutex[I32], Arc[I32], etc.)
// and is linked into all test suites to avoid regenerating the same LLVM IR.

#if 0  // DISABLED - see comment above
static std::string get_precompiled_symbols_obj(bool verbose, bool no_cache) {
    fs::path cache_dir = get_run_cache_dir();
    fs::path precompiled_obj = cache_dir / ("precompiled_symbols" + get_object_extension());

    // If cached and not forcing rebuild, return existing
    if (!no_cache && fs::exists(precompiled_obj)) {
        return precompiled_obj.string();
    }

    TML_LOG_INFO("test", "[PRECOMPILE] Compiling precompiled_symbols.tml...");

    // Find the source file
    fs::path source = "lib/std/precompiled_symbols.tml";
    if (!fs::exists(source)) {
        // Precompiled symbols are optional - if not found, just skip
        return "";
    }

    try {
        // Read and preprocess source
        std::string source_code = read_file(source.string());
        auto pp_config = preprocessor::Preprocessor::host_config();
        preprocessor::Preprocessor pp(pp_config);
        auto pp_result = pp.process(source_code, source.string());

        if (!pp_result.success()) {
            TML_LOG_WARN("test", "[PRECOMPILE] Preprocessor errors in precompiled_symbols.tml");
            return "";
        }

        std::string preprocessed = pp_result.output;

        // Lex
        auto lex_source = lexer::Source::from_string(preprocessed, source.string());
        lexer::Lexer lex(lex_source);
        auto tokens = lex.tokenize();
        if (lex.has_errors()) {
            TML_LOG_WARN("test", "[PRECOMPILE] Lexer errors in precompiled_symbols.tml");
            return "";
        }

        // Parse
        parser::Parser parser(std::move(tokens));
        auto parse_result = parser.parse_module("precompiled_symbols");
        if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
            TML_LOG_WARN("test", "[PRECOMPILE] Parser errors in precompiled_symbols.tml");
            return "";
        }
        const auto& module = std::get<parser::Module>(parse_result);

        // Type check
        types::TypeChecker checker;
        auto check_result = checker.check_module(module);
        if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
            TML_LOG_WARN("test", "[PRECOMPILE] Type errors in precompiled_symbols.tml");
            return "";
        }
        const auto& env = std::get<types::TypeEnv>(check_result);

        // Borrow check
        borrow::BorrowChecker borrow_checker(env);
        auto borrow_result = borrow_checker.check_module(module);
        if (std::holds_alternative<std::vector<borrow::BorrowError>>(borrow_result)) {
            TML_LOG_WARN("test", "[PRECOMPILE] Borrow check errors in precompiled_symbols.tml");
            return "";
        }

        // Codegen
        codegen::LLVMGenOptions options;
        options.emit_comments = false;
        options.generate_dll_entry = false; // No test entry
        options.dll_export = false;
        options.force_internal_linkage = false; // Allow linking
        options.emit_debug_info = false;
        codegen::LLVMIRGen llvm_gen(env, options);

        auto gen_result = llvm_gen.generate(module);
        if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(gen_result)) {
            TML_LOG_WARN("test", "[PRECOMPILE] Codegen errors in precompiled_symbols.tml");
            return "";
        }

        const auto& llvm_ir = std::get<std::string>(gen_result);

        // Write LLVM IR
        fs::path ll_path = cache_dir / "precompiled_symbols.ll";
        std::ofstream ll_file(ll_path);
        if (!ll_file) {
            TML_LOG_WARN("test", "[PRECOMPILE] Cannot write LLVM IR");
            return "";
        }
        ll_file << llvm_ir;
        ll_file.close();

        // Compile to object
        std::string clang = find_clang();
        ObjectCompileOptions obj_options;
        obj_options.optimization_level = tml::CompilerOptions::optimization_level;
        obj_options.debug_info = false; // No debug info for precompiled symbols
        obj_options.verbose = false;
        obj_options.target_triple = tml::CompilerOptions::target_triple;
        obj_options.sysroot = tml::CompilerOptions::sysroot;
        obj_options.coverage = false; // No coverage for precompiled symbols

        auto obj_result = compile_ll_to_object(ll_path, precompiled_obj, clang, obj_options);
        if (!obj_result.success) {
            TML_LOG_WARN("test", "[PRECOMPILE] Object compilation failed: "
                      << obj_result.error_message);
            fs::remove(ll_path);
            return "";
        }
        fs::remove(ll_path); // Clean up LLVM IR file

        TML_LOG_INFO("test", "[PRECOMPILE] Successfully compiled precompiled_symbols.obj");
        return precompiled_obj.string();

    } catch (const std::exception& e) {
        TML_LOG_WARN("test", "[PRECOMPILE] Exception during compilation: " << e.what());
        return "";
    }
}
#endif // DISABLED precompiled symbols

SuiteCompileResult compile_test_suite(const TestSuite& suite, bool verbose, bool no_cache) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    // Phase timing tracking
    int64_t preprocess_time_us = 0;
    int64_t phase1_time_us = 0;
    int64_t phase2_time_us = 0;
    int64_t runtime_time_us = 0;
    int64_t link_time_us = 0;

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

        // Structure to cache preprocessed sources for reuse in Phase 1
        struct PreprocessedSource {
            std::string file_path;
            std::string preprocessed;
            std::string content_hash;
        };
        std::vector<PreprocessedSource> preprocessed_sources;
        preprocessed_sources.reserve(suite.tests.size());

        auto preprocess_start = Clock::now();

        // First pass: preprocess and compute content hashes (cache for Phase 1)
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

            std::string content_hash = build::generate_content_hash(pp_result.output);
            combined_hash += content_hash;

            // Cache preprocessed source for reuse in Phase 1
            preprocessed_sources.push_back(
                {test.file_path, std::move(pp_result.output), content_hash});
        }

        preprocess_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - preprocess_start)
                .count();

        // Include coverage flag in hash to separate coverage-enabled builds
        if (CompilerOptions::coverage) {
            combined_hash += ":coverage";
        }

        // Check for cached DLL using source-only hash (before typechecking)
        std::string source_hash = build::generate_content_hash(combined_hash);
        fs::path cached_dll_by_source = cache_dir / (source_hash + "_suite" + lib_ext);

        if (!no_cache && fs::exists(cached_dll_by_source)) {
            // Cache hit! Skip all typechecking and compilation
            TML_LOG_INFO("test", "EARLY CACHE HIT - skipping compilation");
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

        // Structure to hold pending object compilations
        struct PendingCompile {
            fs::path ll_path;
            fs::path obj_path;
            std::string test_path;
            bool needs_compile = false;
        };

        std::vector<fs::path> object_files;
        std::vector<std::string> link_libs;
        std::vector<PendingCompile> pending_compiles;

        // Track imported module paths from all files (for get_runtime_objects)
        std::mutex modules_mutex;
        std::set<std::string> imported_module_paths;

        // ======================================================================
        // PHASE 1: Parallel lex/parse/typecheck/codegen
        // ======================================================================
        // Only processes files that need compilation (not cached).
        // Uses preprocessed sources cached from the early cache check loop.
        // Each file is processed independently - the shared_registry is populated
        // later when we parse the first module for get_runtime_objects.

        // First, identify which files need compilation vs are cached
        struct CompileTask {
            size_t index;
            std::string file_path;
            std::string preprocessed;
            std::string content_hash;
            fs::path obj_output;
            bool needs_compile;
        };

        std::vector<CompileTask> tasks;
        tasks.reserve(suite.tests.size());

        for (size_t i = 0; i < suite.tests.size(); ++i) {
            const auto& pp_source = preprocessed_sources[i];
            std::string obj_name = pp_source.content_hash + "_suite_" + std::to_string(i);
            fs::path obj_output = cache_dir / (obj_name + get_object_extension());
            bool needs_compile = no_cache || !fs::exists(obj_output);

            combined_hash += pp_source.content_hash;
            object_files.push_back(obj_output);

            if (needs_compile) {
                tasks.push_back({i, pp_source.file_path, pp_source.preprocessed,
                                 pp_source.content_hash, obj_output, true});
            }

            // Collect module imports from ALL files (even cached ones)
            // This is needed for get_runtime_objects to know which runtimes to link
            // Use quick lex/parse to extract use declarations without type-checking
            auto source = lexer::Source::from_string(pp_source.preprocessed, pp_source.file_path);
            lexer::Lexer lex(source);
            auto tokens = lex.tokenize();
            parser::Parser parser(std::move(tokens));
            auto parse_result = parser.parse_module(fs::path(pp_source.file_path).stem().string());
            if (std::holds_alternative<parser::Module>(parse_result)) {
                const auto& mod = std::get<parser::Module>(parse_result);
                for (const auto& decl : mod.decls) {
                    if (decl->is<parser::UseDecl>()) {
                        const auto& use_decl = decl->as<parser::UseDecl>();
                        std::string use_path;
                        for (size_t j = 0; j < use_decl.path.segments.size(); ++j) {
                            if (j > 0)
                                use_path += "::";
                            use_path += use_decl.path.segments[j];
                        }
                        imported_module_paths.insert(use_path);
                        // Also add parent paths
                        std::string parent = use_path;
                        while (true) {
                            auto pos = parent.rfind("::");
                            if (pos == std::string::npos)
                                break;
                            parent = parent.substr(0, pos);
                            imported_module_paths.insert(parent);
                        }
                    }
                }
            }
        }

        // Process compilation tasks in parallel
        // NOTE: When multiple suites are being compiled in parallel (the common case),
        // we limit internal parallelism to avoid thread explosion. With 22 suites and
        // 32 threads each, we'd have 704 threads competing for CPU!
        // Use at most 2 internal threads per suite to balance parallelism.
        auto phase1_start = Clock::now();

        if (!tasks.empty()) {
            std::atomic<size_t> next_task{0};
            std::atomic<bool> has_error{false};
            std::string first_error_msg;
            std::string first_error_file;
            std::mutex error_mutex;
            std::mutex pending_mutex;
            std::mutex libs_mutex;
            std::mutex timing_mutex;

            // Per-task timing tracking for slow task detection
            struct TaskTiming {
                size_t task_idx;
                std::string file_path;
                int64_t duration_us;
                // Sub-phase timings
                int64_t lex_us;
                int64_t parse_us;
                int64_t typecheck_us;
                int64_t borrow_us;
                int64_t codegen_us;
            };
            std::vector<TaskTiming> task_timings;
            task_timings.reserve(tasks.size());

            // Aggregate sub-phase totals for summary
            std::atomic<int64_t> total_lex_us{0};
            std::atomic<int64_t> total_parse_us{0};
            std::atomic<int64_t> total_typecheck_us{0};
            std::atomic<int64_t> total_borrow_us{0};
            std::atomic<int64_t> total_codegen_us{0};

            // Running average for slow task detection
            std::atomic<int64_t> total_task_time_us{0};
            std::atomic<size_t> completed_tasks{0};

            // Phase 1 internal parallelism: lex/parse/typecheck/codegen per test file.
            // Thread-safe: GlobalModuleCache uses shared_mutex, ModuleRegistry is per-thread,
            // path cache uses shared_mutex, TypeEnv is per-thread.
            // Capped at 50% of cores, range [4, 8].
            unsigned int num_threads =
                calc_codegen_threads(static_cast<unsigned int>(tasks.size()));

            // TEMPORARILY DISABLED for baseline comparison
            // Pre-load all library modules from .tml.meta binary cache.
            // This MUST complete before any test compilation starts.
            // It either loads existing .tml.meta files or generates them from source.
            // TML_LOG_INFO("test", ">>> Pre-loading library modules (blocking until complete)...");
            // types::preload_all_meta_caches();
            // TML_LOG_INFO("test", ">>> Pre-load complete. Starting test compilation...");

            auto compile_task_worker = [&]() {
                auto thread_registry = std::make_shared<types::ModuleRegistry>();

                while (!has_error.load()) {
                    size_t task_idx = next_task.fetch_add(1);
                    if (task_idx >= tasks.size())
                        break;

                    auto& task = tasks[task_idx];
                    auto task_start = Clock::now();

                    TML_LOG_INFO("test", "  Processing test " << (task_idx + 1) << "/"
                                                              << tasks.size() << ": "
                                                              << task.file_path);

                    // Sub-phase timing for detailed profiling
                    int64_t lex_us = 0, parse_us = 0, typecheck_us = 0, borrow_us = 0,
                            codegen_us = 0;

                    try {
                        // Lex
                        auto lex_start = Clock::now();
                        auto source = lexer::Source::from_string(task.preprocessed, task.file_path);
                        lexer::Lexer lex(source);
                        auto tokens = lex.tokenize();
                        lex_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                     Clock::now() - lex_start)
                                     .count();

                        if (lex.has_errors()) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                std::ostringstream oss;
                                oss << "Lexer errors in " << task.file_path << ":\n";
                                for (const auto& err : lex.errors()) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }

                        // Parse
                        auto parse_start = Clock::now();
                        parser::Parser parser(std::move(tokens));
                        auto module_name = fs::path(task.file_path).stem().string();
                        auto parse_result = parser.parse_module(module_name);
                        parse_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                       Clock::now() - parse_start)
                                       .count();

                        if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                const auto& errors =
                                    std::get<std::vector<parser::ParseError>>(parse_result);
                                std::ostringstream oss;
                                oss << "Parser errors in " << task.file_path << ":\n";
                                for (const auto& err : errors) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }
                        const auto& module = std::get<parser::Module>(parse_result);

                        // Type check with thread-local registry
                        auto typecheck_start = Clock::now();
                        types::TypeChecker checker;
                        checker.set_module_registry(thread_registry);
                        auto check_result = checker.check_module(module);
                        typecheck_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                           Clock::now() - typecheck_start)
                                           .count();

                        if (std::holds_alternative<std::vector<types::TypeError>>(check_result)) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                const auto& errors =
                                    std::get<std::vector<types::TypeError>>(check_result);
                                std::ostringstream oss;
                                oss << "Type errors in " << task.file_path << ":\n";
                                for (const auto& err : errors) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }
                        const auto& env = std::get<types::TypeEnv>(check_result);

                        // Collect imported module paths for get_runtime_objects
                        {
                            std::lock_guard<std::mutex> lock(modules_mutex);
                            for (const auto& [path, _] : thread_registry->get_all_modules()) {
                                imported_module_paths.insert(path);
                            }
                        }

                        // Borrow check
                        auto borrow_start = Clock::now();
                        borrow::BorrowChecker borrow_checker(env);
                        auto borrow_result = borrow_checker.check_module(module);
                        borrow_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                        Clock::now() - borrow_start)
                                        .count();

                        if (std::holds_alternative<std::vector<borrow::BorrowError>>(
                                borrow_result)) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                const auto& errors =
                                    std::get<std::vector<borrow::BorrowError>>(borrow_result);
                                std::ostringstream oss;
                                oss << "Borrow check errors in " << task.file_path << ":\n";
                                for (const auto& err : errors) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }

                        // Codegen with indexed entry point
                        auto codegen_start = Clock::now();
                        codegen::LLVMGenOptions options;
                        options.emit_comments = false;
                        options.generate_dll_entry = true;
                        options.suite_test_index = static_cast<int>(task.index);
                        options.suite_total_tests = static_cast<int>(suite.tests.size());
                        options.dll_export = true;
                        options.force_internal_linkage = true;
                        options.emit_debug_info = CompilerOptions::debug_info;
                        options.debug_level = CompilerOptions::debug_level;
                        options.source_file = task.file_path;
                        options.coverage_enabled = CompilerOptions::coverage;
                        options.coverage_quiet = CompilerOptions::coverage;
                        options.coverage_output_file = CompilerOptions::coverage_output;
                        options.llvm_source_coverage = CompilerOptions::coverage_source;
                        codegen::LLVMIRGen llvm_gen(env, options);

                        auto gen_result = llvm_gen.generate(module);
                        codegen_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                         Clock::now() - codegen_start)
                                         .count();

                        if (std::holds_alternative<std::vector<codegen::LLVMGenError>>(
                                gen_result)) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                const auto& errors =
                                    std::get<std::vector<codegen::LLVMGenError>>(gen_result);
                                std::ostringstream oss;
                                oss << "Codegen errors in " << task.file_path << ":\n";
                                for (const auto& err : errors) {
                                    oss << "  " << err.span.start.line << ":"
                                        << err.span.start.column << ": " << err.message << "\n";
                                }
                                first_error_msg = oss.str();
                                first_error_file = task.file_path;
                            }
                            continue;
                        }

                        const auto& llvm_ir = std::get<std::string>(gen_result);

                        // Collect link libraries (thread-safe)
                        {
                            std::lock_guard<std::mutex> lock(libs_mutex);
                            for (const auto& lib : llvm_gen.get_link_libs()) {
                                if (std::find(link_libs.begin(), link_libs.end(), lib) ==
                                    link_libs.end()) {
                                    link_libs.push_back(lib);
                                }
                            }
                        }

                        // Write IR for later parallel compilation
                        std::string obj_name =
                            task.content_hash + "_suite_" + std::to_string(task.index);
                        fs::path ll_output = cache_dir / (obj_name + ".ll");
                        std::ofstream ll_file(ll_output);
                        if (!ll_file) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            if (!has_error.load()) {
                                has_error.store(true);
                                first_error_msg = "Cannot write LLVM IR";
                                first_error_file = task.file_path;
                            }
                            continue;
                        }
                        ll_file << llvm_ir;
                        ll_file.close();

                        // Add to pending compiles (thread-safe)
                        {
                            std::lock_guard<std::mutex> lock(pending_mutex);
                            pending_compiles.push_back(
                                {ll_output, task.obj_output, task.file_path, true});
                        }

                        // Track task timing and check for slow tasks
                        auto task_end = Clock::now();
                        int64_t task_duration_us =
                            std::chrono::duration_cast<std::chrono::microseconds>(task_end -
                                                                                  task_start)
                                .count();

                        // Update running totals
                        total_task_time_us.fetch_add(task_duration_us);
                        total_lex_us.fetch_add(lex_us);
                        total_parse_us.fetch_add(parse_us);
                        total_typecheck_us.fetch_add(typecheck_us);
                        total_borrow_us.fetch_add(borrow_us);
                        total_codegen_us.fetch_add(codegen_us);
                        size_t completed = completed_tasks.fetch_add(1) + 1;

                        // Check for abnormally slow task (only after we have some baseline)
                        if (completed >= 3) {
                            int64_t avg_time_us =
                                total_task_time_us.load() / static_cast<int64_t>(completed);
                            int64_t threshold_us =
                                std::max(MIN_SLOW_THRESHOLD_US,
                                         static_cast<int64_t>(avg_time_us * SLOW_TASK_THRESHOLD));

                            if (task_duration_us > threshold_us) {
                                // Log slow task as warning (doesn't fail the test)
                                std::lock_guard<std::mutex> lock(error_mutex);
                                TML_LOG_WARN(
                                    "test",
                                    "[SLOW TASK] "
                                        << task.file_path
                                        << " Duration: " << (task_duration_us / 1000) << " ms"
                                        << " Average: " << (avg_time_us / 1000) << " ms"
                                        << " Threshold: " << (threshold_us / 1000) << " ms ("
                                        << SLOW_TASK_THRESHOLD << "x average)"
                                        << " Sub-phases: lex=" << (lex_us / 1000)
                                        << "ms, parse=" << (parse_us / 1000)
                                        << "ms, typecheck=" << (typecheck_us / 1000)
                                        << "ms, borrow=" << (borrow_us / 1000)
                                        << "ms, codegen=" << (codegen_us / 1000) << "ms"
                                        << " This task took " << std::fixed << std::setprecision(1)
                                        << (static_cast<double>(task_duration_us) / avg_time_us)
                                        << "x longer than average.");
                                // Don't abort - slow tasks still generate valid cache
                            }
                        }

                        // Record timing (thread-safe)
                        {
                            std::lock_guard<std::mutex> lock(timing_mutex);
                            task_timings.push_back({task_idx, task.file_path, task_duration_us,
                                                    lex_us, parse_us, typecheck_us, borrow_us,
                                                    codegen_us});
                        }

                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!has_error.load()) {
                            has_error.store(true);
                            first_error_msg =
                                "Exception while compiling " + task.file_path + ": " + e.what();
                            first_error_file = task.file_path;
                        }
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!has_error.load()) {
                            has_error.store(true);
                            first_error_msg = "Unknown exception while compiling " + task.file_path;
                            first_error_file = task.file_path;
                        }
                    }
                }
            };

            TML_LOG_INFO("test", "  Generating " << tasks.size() << " LLVM IR files with "
                                                 << num_threads << " threads...");

            // Launch worker threads
            std::vector<std::thread> threads;
            for (unsigned int t = 0; t < std::min(num_threads, (unsigned int)tasks.size()); ++t) {
                threads.emplace_back(compile_task_worker);
            }
            for (auto& t : threads) {
                t.join();
            }

            // Check for errors
            if (has_error.load()) {
                result.error_message = first_error_msg;
                result.failed_test = first_error_file;
                return result;
            }

            // Print Phase 1 timing summary if verbose
            if (verbose && !task_timings.empty()) {
                // Sort by duration (slowest first)
                std::sort(task_timings.begin(), task_timings.end(),
                          [](const TaskTiming& a, const TaskTiming& b) {
                              return a.duration_us > b.duration_us;
                          });

                // Print aggregate sub-phase breakdown (single line)
                int64_t total_us = total_task_time_us.load();
                if (total_us > 0) {
                    TML_LOG_DEBUG("test",
                                  "Phase 1 sub-phases:"
                                      << " lex=" << (total_lex_us.load() / 1000) << "ms"
                                      << " parse=" << (total_parse_us.load() / 1000) << "ms"
                                      << " typecheck=" << (total_typecheck_us.load() / 1000) << "ms"
                                      << " borrow=" << (total_borrow_us.load() / 1000) << "ms"
                                      << " codegen=" << (total_codegen_us.load() / 1000) << "ms"
                                      << " total=" << (total_us / 1000) << "ms");
                }

                {
                    // Log each slow file as a separate single-line entry
                    for (size_t i = 0; i < std::min(size_t(5), task_timings.size()); ++i) {
                        const auto& t = task_timings[i];
                        TML_LOG_DEBUG("test", "Phase 1 slow #"
                                                  << i << ": "
                                                  << fs::path(t.file_path).filename().string()
                                                  << " " << (t.duration_us / 1000) << "ms"
                                                  << " [lex=" << (t.lex_us / 1000)
                                                  << " parse=" << (t.parse_us / 1000)
                                                  << " tc=" << (t.typecheck_us / 1000)
                                                  << " borrow=" << (t.borrow_us / 1000)
                                                  << " cg=" << (t.codegen_us / 1000) << "]");
                    }
                }
            }
        }

        phase1_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase1_start)
                .count();

        // ======================================================================
        // PHASE 2: Parallel object compilation (.ll -> .obj)
        // ======================================================================
        // Like Phase 1, limit internal parallelism since suites compile in parallel

        auto phase2_start = Clock::now();

        if (!pending_compiles.empty()) {
            ObjectCompileOptions obj_options;
            obj_options.optimization_level = CompilerOptions::optimization_level;
            obj_options.debug_info = CompilerOptions::debug_info;
            obj_options.verbose = false;
            obj_options.coverage = CompilerOptions::coverage_source;

            std::atomic<size_t> next_compile{0};
            std::atomic<bool> compile_error{false};
            std::string error_message;
            std::string failed_test;
            std::mutex error_mutex;
            std::mutex timing_mutex;

            // Per-task timing for Phase 2
            struct ObjTiming {
                std::string test_path;
                int64_t duration_us;
            };
            std::vector<ObjTiming> obj_timings;
            obj_timings.reserve(pending_compiles.size());

            std::atomic<int64_t> total_obj_time_us{0};
            std::atomic<size_t> completed_objs{0};

            // Phase 2: compile .ll -> .obj using clang subprocesses.
            // Each subprocess is independent (unique file paths). Thread-safe by design.
            unsigned int num_threads =
                calc_codegen_threads(static_cast<unsigned int>(pending_compiles.size()));

            auto compile_worker = [&]() {
                while (!compile_error.load()) {
                    size_t idx = next_compile.fetch_add(1);
                    if (idx >= pending_compiles.size())
                        break;

                    auto& pc = pending_compiles[idx];
                    auto obj_start = Clock::now();

                    auto obj_result =
                        compile_ll_to_object(pc.ll_path, pc.obj_path, clang, obj_options);
                    fs::remove(pc.ll_path);

                    auto obj_end = Clock::now();
                    int64_t obj_duration_us =
                        std::chrono::duration_cast<std::chrono::microseconds>(obj_end - obj_start)
                            .count();

                    // Update timing stats
                    total_obj_time_us.fetch_add(obj_duration_us);
                    size_t completed = completed_objs.fetch_add(1) + 1;

                    // Check for slow object compilation
                    if (completed >= 3) {
                        int64_t avg_us = total_obj_time_us.load() / static_cast<int64_t>(completed);
                        int64_t threshold_us =
                            std::max(MIN_SLOW_THRESHOLD_US,
                                     static_cast<int64_t>(avg_us * SLOW_TASK_THRESHOLD));

                        if (obj_duration_us > threshold_us) {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            TML_LOG_WARN("test",
                                         "[SLOW OBJ] "
                                             << pc.test_path
                                             << " Duration: " << (obj_duration_us / 1000) << " ms"
                                             << " Average: " << (avg_us / 1000) << " ms"
                                             << " This .obj compilation took " << std::fixed
                                             << std::setprecision(1)
                                             << (static_cast<double>(obj_duration_us) / avg_us)
                                             << "x longer than average!");
                        }
                    }

                    // Record timing
                    {
                        std::lock_guard<std::mutex> lock(timing_mutex);
                        obj_timings.push_back({pc.test_path, obj_duration_us});
                    }

                    if (!obj_result.success) {
                        std::lock_guard<std::mutex> lock(error_mutex);
                        if (!compile_error.load()) {
                            compile_error.store(true);
                            error_message = "Compilation failed: " + obj_result.error_message;
                            failed_test = pc.test_path;
                        }
                    }
                }
            };

            TML_LOG_INFO("test", "  Compiling " << pending_compiles.size() << " objects with "
                                                << num_threads << " threads...");

            std::vector<std::thread> threads;
            for (unsigned int t = 0;
                 t < std::min(num_threads, (unsigned int)pending_compiles.size()); ++t) {
                threads.emplace_back(compile_worker);
            }
            for (auto& t : threads) {
                t.join();
            }

            // Print Phase 2 timing summary if verbose
            if (verbose && !obj_timings.empty()) {
                std::sort(obj_timings.begin(), obj_timings.end(),
                          [](const ObjTiming& a, const ObjTiming& b) {
                              return a.duration_us > b.duration_us;
                          });

                for (size_t i = 0; i < std::min(size_t(5), obj_timings.size()); ++i) {
                    const auto& t = obj_timings[i];
                    TML_LOG_INFO("test", "Phase 2 slow #"
                                             << i << ": "
                                             << fs::path(t.test_path).filename().string() << " "
                                             << (t.duration_us / 1000) << "ms");
                }
            }

            if (compile_error.load()) {
                result.error_message = error_message;
                result.failed_test = failed_test;
                return result;
            }
        }

        phase2_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase2_start)
                .count();

        // Get runtime objects (only need to do once for the suite)
        // Register placeholder modules in shared_registry for all imported paths
        // This is much faster than re-type-checking all files
        auto runtime_start = Clock::now();

        // Register placeholder modules for all imported paths AND their parent paths
        // get_runtime_objects uses has_module() checks like has_module("std::file"),
        // so if "std::file::path" is imported, we also need to register "std::file"
        for (const auto& path : imported_module_paths) {
            // Register the full path
            if (!shared_registry->has_module(path)) {
                types::Module placeholder;
                placeholder.name = path;
                shared_registry->register_module(path, std::move(placeholder));
            }
            // Register all parent paths (e.g., "std::file" for "std::file::path")
            std::string parent = path;
            while (true) {
                auto pos = parent.rfind("::");
                if (pos == std::string::npos)
                    break;
                parent = parent.substr(0, pos);
                if (!shared_registry->has_module(parent)) {
                    types::Module placeholder;
                    placeholder.name = parent;
                    shared_registry->register_module(parent, std::move(placeholder));
                }
            }
        }

        // Parse the first file for get_runtime_objects (needs a module reference)
        const auto& first_pp = preprocessed_sources[0];
        auto source = lexer::Source::from_string(first_pp.preprocessed, first_pp.file_path);
        lexer::Lexer lex(source);
        auto tokens = lex.tokenize();
        parser::Parser parser(std::move(tokens));
        auto parse_result = parser.parse_module(fs::path(first_pp.file_path).stem().string());
        const auto& module = std::get<parser::Module>(parse_result);

        std::string deps_cache = to_forward_slashes(get_deps_cache_dir().string());

        TML_LOG_INFO("test", "  Getting runtime objects...");
        // Note: Pass verbose=false to avoid repeated "Including runtime:" messages
        // when compiling multiple suites in parallel. The runtime objects are the
        // same for all suites and would spam the output.
        auto runtime_objects =
            get_runtime_objects(shared_registry, module, deps_cache, clang, false);
        TML_LOG_INFO("test", "  Got " << runtime_objects.size() << " runtime objects");
        object_files.insert(object_files.end(), runtime_objects.begin(), runtime_objects.end());

        // DISABLED: Precompiled symbols cause linkage conflicts with force_internal_linkage
        // Symptoms: "use of undefined value '@atomic_compare_exchange_i32'" errors
        // Root cause: precompiled_symbols uses external linkage, test suites use internal linkage
        // TODO: Fix by:
        //   1. Using same linkage mode in both (but defeats purpose of sharing)
        //   2. LLVM module linking instead of object file linking
        //   3. Weak symbols or link-once semantics
        //
        // static std::string precompiled_obj = get_precompiled_symbols_obj(verbose, no_cache);
        // if (!precompiled_obj.empty() && fs::exists(precompiled_obj)) {
        //     object_files.push_back(fs::path(precompiled_obj));
        //     TML_LOG_INFO("test", "  Using precompiled symbols: " << precompiled_obj);
        // }

        runtime_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - runtime_start)
                .count();

        // Generate suite hash for full caching (includes runtime objects)
        std::string suite_hash = generate_content_hash(combined_hash);
        std::string exe_hash = generate_exe_hash(suite_hash, object_files);

        fs::path cached_dll = cache_dir / (exe_hash + "_suite" + lib_ext);

        bool use_cached_dll = !no_cache && fs::exists(cached_dll);

        auto link_start = Clock::now();

        if (!use_cached_dll) {
            // Link as shared library
            LinkOptions link_options;
            link_options.output_type = LinkOptions::OutputType::DynamicLib;
            link_options.verbose = false;
            link_options.coverage = CompilerOptions::coverage_source; // LLVM source coverage

            for (const auto& lib : link_libs) {
                if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
                    link_options.link_flags.push_back("\"" + lib + "\"");
                } else {
                    link_options.link_flags.push_back("-l" + lib);
                }
            }

            TML_LOG_INFO("test", "  Starting link...");
            auto link_result = link_objects(object_files, cached_dll, clang, link_options);
            TML_LOG_INFO("test", "  Link complete");
            if (!link_result.success) {
                result.error_message = "Linking failed: " + link_result.error_message;
                return result;
            }
        }

        link_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - link_start)
                .count();

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

        // Print timing summary if verbose (single line for clean log output)
        if (verbose) {
            int64_t total_us = result.compile_time_us;
            TML_LOG_INFO("test", "Suite " << suite.name << " timing: preprocess="
                                          << (preprocess_time_us / 1000) << "ms"
                                          << " phase1=" << (phase1_time_us / 1000) << "ms"
                                          << " phase2=" << (phase2_time_us / 1000) << "ms"
                                          << " runtime=" << (runtime_time_us / 1000) << "ms"
                                          << " link=" << (link_time_us / 1000) << "ms"
                                          << " total=" << (total_us / 1000) << "ms");
        }

        return result;

    } catch (const std::exception& e) {
        result.error_message = "FATAL EXCEPTION during suite compilation: " + std::string(e.what());
        if (!suite.tests.empty()) {
            result.failed_test = suite.tests[0].file_path;
        }
        TML_LOG_FATAL("test", "Exception in compile_test_suite: " << e.what());
        return result;
    } catch (...) {
        result.error_message = "FATAL UNKNOWN EXCEPTION during suite compilation";
        if (!suite.tests.empty()) {
            result.failed_test = suite.tests[0].file_path;
        }
        TML_LOG_FATAL("test", "Unknown exception in compile_test_suite");
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
using TmlGetPanicMessage = const char* (*)();
using TmlGetPanicBacktrace = const char* (*)();
using TmlGetPanicBacktraceJson = const char* (*)();
using TmlEnableBacktrace = void (*)();

SuiteTestResult run_suite_test(DynamicLibrary& lib, int test_index, bool verbose,
                               int timeout_seconds, const std::string& test_name, bool backtrace) {
    SuiteTestResult result;

    // Flush output to help debug crashes
    std::cout << std::flush;
    std::cerr << std::flush;
    std::fflush(stdout);
    std::fflush(stderr);

    // Get the indexed test function
    std::string func_name = "tml_test_" + std::to_string(test_index);
    TML_LOG_INFO("test", "  Looking up symbol: " << func_name);
    auto test_func = lib.get_function<TestMainFunc>(func_name.c_str());
    if (!test_func) {
        result.error = "Failed to find " + func_name + " in suite DLL";
        TML_LOG_ERROR("test", result.error);
        return result;
    }

    // Try to get the panic-catching wrapper from the runtime
    auto run_with_catch = lib.get_function<TmlRunTestWithCatch>("tml_run_test_with_catch");
    TML_LOG_INFO("test", "  tml_run_test_with_catch: " << (run_with_catch ? "found" : "NOT FOUND"));

    // Get panic message and backtrace functions
    auto get_panic_msg = lib.get_function<TmlGetPanicMessage>("tml_get_panic_message");
    auto get_panic_bt =
        backtrace ? lib.get_function<TmlGetPanicBacktrace>("tml_get_panic_backtrace") : nullptr;
    auto get_panic_bt_json =
        backtrace ? lib.get_function<TmlGetPanicBacktraceJson>("tml_get_panic_backtrace_json")
                  : nullptr;
    auto enable_bt =
        backtrace ? lib.get_function<TmlEnableBacktrace>("tml_enable_backtrace_on_panic") : nullptr;

    // Enable backtrace for test failures (if available and enabled)
    if (backtrace && enable_bt) {
        enable_bt();
    }

    // Get output suppression function from runtime (to suppress test output when not verbose)
    using TmlSetOutputSuppressed = void (*)(int32_t);
    auto set_output_suppressed =
        lib.get_function<TmlSetOutputSuppressed>("tml_set_output_suppressed");
    TML_LOG_INFO(
        "test", "  tml_set_output_suppressed: " << (set_output_suppressed ? "found" : "NOT FOUND"));

    // Suppress output when not in verbose mode
    if (!verbose) {
        if (set_output_suppressed) {
            set_output_suppressed(1);
        }
        // Also flush to ensure no buffered output appears
        std::fflush(stdout);
        std::fflush(stderr);
    }

    // Route C runtime log messages through the C++ Logger
    using RtLogSetCallback = void (*)(void (*)(int, const char*, const char*));
    auto set_log_callback = lib.get_function<RtLogSetCallback>("rt_log_set_callback");
    if (set_log_callback) {
        set_log_callback(rt_log_bridge_callback);
    }

    // Sync C runtime log level with C++ Logger level
    using RtLogSetLevel = void (*)(int);
    auto set_log_level = lib.get_function<RtLogSetLevel>("rt_log_set_level");
    if (set_log_level) {
        set_log_level(static_cast<int>(tml::log::Logger::instance().level()));
    }

    // Save reference to original stderr BEFORE capture for timeout messages
    // This allows the watchdog to write directly to console even when output is captured
#ifdef _WIN32
    int original_stderr_fd = _dup(_fileno(stderr));
#else
    int original_stderr_fd = dup(STDERR_FILENO);
#endif

    // Skip output capture in suite mode (parallel execution) - stdout/stderr redirection
    // is not thread-safe and causes deadlocks. Instead rely on tml_set_output_suppressed
    // at the TML runtime level to suppress output when not verbose.
    // Only capture output in single-threaded mode (non-suite) for error diagnostics.
    OutputCapture capture;
    bool capture_started = false; // Disabled - causes deadlocks in parallel mode

    // Execute the test
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    TML_LOG_INFO("test", "  Executing test function...");

    // Ensure output is flushed before test execution in case of crash
    std::cout << std::flush;
    std::cerr << std::flush;
    std::fflush(stdout);
    std::fflush(stderr);

    // Timeout watchdog thread - monitors test execution and reports hangs
    std::atomic<bool> test_completed{false};
    std::atomic<bool> timeout_triggered{false};
    std::mutex watchdog_mutex;
    std::condition_variable watchdog_cv;
    std::thread watchdog_thread;

    if (timeout_seconds > 0) {
        watchdog_thread = std::thread([&, original_stderr_fd]() {
            std::unique_lock<std::mutex> lock(watchdog_mutex);
            // Wait for timeout or test completion
            auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);

            // Check every second to provide progress updates for long-running tests
            while (std::chrono::steady_clock::now() < deadline) {
                if (watchdog_cv.wait_for(lock, std::chrono::seconds(1),
                                         [&]() { return test_completed.load(); })) {
                    return; // Test completed normally
                }

                // If still running, check elapsed time
                auto elapsed =
                    std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start).count();

                // After 5 seconds, start showing progress in non-verbose mode
                if (!verbose && elapsed >= 5 && elapsed % 5 == 0) {
                    // Write directly to original stderr (bypasses capture redirection)
                    std::ostringstream progress_msg;
                    progress_msg << "\033[33m[WARNING] Test '"
                                 << (test_name.empty() ? func_name : test_name)
                                 << "' still running... (" << elapsed << "s)\033[0m\n";
                    std::string msg = progress_msg.str();
#ifdef _WIN32
                    _write(original_stderr_fd, msg.c_str(), (unsigned int)msg.size());
#else
                    write(original_stderr_fd, msg.c_str(), msg.size());
#endif
                }
            }

            // Timeout reached - test is hanging
            timeout_triggered.store(true);

            // Restore output first (in case it was suppressed)
            if (set_output_suppressed) {
                set_output_suppressed(0);
            }

            // Build timeout message
            std::string test_display = test_name.empty() ? func_name : test_name;
            std::ostringstream msg;
            msg << "\n\n\033[1;31m" // Bold red
                << "============================================================\n"
                << "               TEST TIMEOUT DETECTED\n"
                << "============================================================\n"
                << " Test:    " << test_display << "\n"
                << " Timeout: " << timeout_seconds << " seconds\n"
                << "\n"
                << " The test appears to be stuck in an infinite loop\n"
                << " or deadlock. Terminating test process...\n"
                << "============================================================\n"
                << "\033[0m\n";

            // Write directly to original stderr (bypasses capture redirection)
            std::string msgStr = msg.str();
#ifdef _WIN32
            _write(original_stderr_fd, msgStr.c_str(), (unsigned int)msgStr.size());
            _commit(original_stderr_fd);
            // Give time for output to be displayed
            Sleep(200);
            TerminateProcess(GetCurrentProcess(), 124); // Exit code 124 = timeout
#else
            write(original_stderr_fd, msgStr.c_str(), msgStr.size());
            fsync(original_stderr_fd);
            usleep(200000); // 200ms
            _exit(124);
#endif
        });
    }

    // Execute test with crash protection
    // The runtime's tml_run_test_with_catch handles both panics (via setjmp/longjmp)
    // and crashes (via exception filter on Windows, signal handlers on Unix).
    //
    // IMPORTANT: On Windows, do NOT wrap tml_run_test_with_catch in SEH (__try/__except)
    // because combining SEH with setjmp/longjmp causes BAD_STACK (0xC0000028).
    // The runtime's exception filter handles crashes, so SEH is not needed.
    if (run_with_catch) {
        TML_LOG_INFO("test", "  Calling tml_run_test_with_catch wrapper...");
        result.exit_code = run_with_catch(test_func);
        if (result.exit_code == -1) {
            // Panic was caught
            result.success = false;
            std::string error_msg = "Test panicked";
            if (get_panic_msg) {
                const char* panic_msg = get_panic_msg();
                if (panic_msg && panic_msg[0] != '\0') {
                    error_msg += ": ";
                    error_msg += panic_msg;
                }
            }
            if (get_panic_bt) {
                const char* bt_str = get_panic_bt();
                if (bt_str && bt_str[0] != '\0') {
                    error_msg += "\n\nBacktrace:\n";
                    error_msg += bt_str;
                }
            }
            // Log JSON backtrace for structured debugging
            if (get_panic_bt_json) {
                const char* bt_json = get_panic_bt_json();
                if (bt_json && bt_json[0] != '\0' && bt_json[0] != ']') {
                    TML_LOG_ERROR("test", "PANIC backtrace (JSON): " << bt_json);
                }
            }
            result.error = error_msg;
        } else if (result.exit_code == -2) {
            // Crash was caught
            result.success = false;
            result.error = "Test crashed (SIGSEGV/SIGFPE/etc)";
        } else {
            result.success = (result.exit_code == 0);
        }
        TML_LOG_INFO("test", "[DEBUG]   tml_run_test_with_catch returned: " << result.exit_code);
    } else {
        // Fallback: direct call with platform-specific crash protection
#ifdef _WIN32
        TML_LOG_INFO("test", "  Calling test function with SEH protection...");
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
        TML_LOG_INFO("test", "  Test returned: " << result.exit_code);
    }

    // Signal watchdog that test completed
    test_completed.store(true);
    watchdog_cv.notify_all();

    // Wait for watchdog thread to finish
    if (watchdog_thread.joinable()) {
        watchdog_thread.join();
    }

    TML_LOG_INFO("test", "  Test execution complete, exit_code=" << result.exit_code);

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (capture_started) {
        result.output = capture.stop();
    }

    // Restore output after test (important for error messages)
    if (!verbose && set_output_suppressed) {
        set_output_suppressed(0);
    }

    // Close the duplicated stderr fd
#ifdef _WIN32
    _close(original_stderr_fd);
#else
    close(original_stderr_fd);
#endif

    return result;
}

SuiteTestResult run_suite_test_profiled(DynamicLibrary& lib, int test_index, PhaseTimings* timings,
                                        bool /*verbose*/, bool backtrace) {
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

    // Get panic message and backtrace functions
    auto get_panic_msg = lib.get_function<TmlGetPanicMessage>("tml_get_panic_message");
    auto get_panic_bt =
        backtrace ? lib.get_function<TmlGetPanicBacktrace>("tml_get_panic_backtrace") : nullptr;
    auto enable_bt =
        backtrace ? lib.get_function<TmlEnableBacktrace>("tml_enable_backtrace_on_panic") : nullptr;

    // Enable backtrace for test failures (if available and enabled)
    if (backtrace && enable_bt) {
        enable_bt();
    }

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
    // DISABLED: OutputCapture causes deadlocks in parallel/suite mode because it
    // manipulates global stdout/stderr file descriptors. Use tml_set_output_suppressed
    // at the TML runtime level instead.
    phase_start = Clock::now();
    OutputCapture capture;
    bool capture_started = false; // Disabled - causes deadlocks
    record_phase("exec.capture_start", phase_start);

    // Phase: Execute the test
    // Use tml_run_test_with_catch if available (handles panic and crashes)
    // On Windows, do NOT wrap in SEH to avoid BAD_STACK issues with setjmp/longjmp
    phase_start = Clock::now();
    if (run_with_catch) {
        result.exit_code = run_with_catch(test_func);
        if (result.exit_code == -1) {
            result.success = false;
            std::string error_msg = "Test panicked";
            if (get_panic_msg) {
                const char* panic_msg = get_panic_msg();
                if (panic_msg && panic_msg[0] != '\0') {
                    error_msg += ": ";
                    error_msg += panic_msg;
                }
            }
            if (get_panic_bt) {
                const char* bt_str = get_panic_bt();
                if (bt_str && bt_str[0] != '\0') {
                    error_msg += "\n\nBacktrace:\n";
                    error_msg += bt_str;
                }
            }
            result.error = error_msg;
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
