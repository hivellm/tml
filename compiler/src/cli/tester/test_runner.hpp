//! # Test Runner Interface
//!
//! This header defines the in-process test execution system.
//!
//! ## Test Execution Modes
//!
//! | Mode       | Description                                 |
//! |------------|---------------------------------------------|
//! | Individual | Each test compiled to separate DLL          |
//! | Suite      | Tests grouped by directory into single DLL  |
//!
//! ## Key Types
//!
//! - `DynamicLibrary`: Cross-platform DLL/SO loader
//! - `TestSuite`: Group of tests compiled together
//! - `InProcessTestResult`: Test result with captured output
//!
//! ## Function Signatures
//!
//! - `TestMainFunc`: `int tml_test_entry(void)` - Test entry point
//! - `FuzzTargetFunc`: `int tml_fuzz_target(data, len)` - Fuzz target

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace tml::cli {

// Global mutex for synchronized verbose output in parallel test execution
// Use this when printing debug output from test runner threads
extern std::mutex g_verbose_output_mutex;

// Get the platform-specific shared library extension
// Windows: .dll, macOS: .dylib, Linux: .so
inline std::string get_shared_lib_extension() {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

// Forward declaration for PhaseTimings
struct PhaseTimings;

// Result of running a test in-process
struct InProcessTestResult {
    bool success = false;
    int exit_code = 0;
    std::string output;
    std::string error;
    int64_t duration_us = 0;
    int64_t compile_time_us = 0; // Time to compile to DLL
};

// Test function signature: int tml_test_entry(void)
// Returns 0 on success, non-zero on failure
using TestMainFunc = int (*)();

// Fuzz target function signature: int tml_fuzz_target(const uint8_t* data, size_t len)
// Returns 0 on success, non-zero on crash/failure
using FuzzTargetFunc = int (*)(const uint8_t*, size_t);

// DLL/SO/dylib handle wrapper with RAII
// Works on Windows (.dll), Linux (.so), and macOS (.dylib)
class DynamicLibrary {
public:
    DynamicLibrary() = default;
    ~DynamicLibrary();

    // Non-copyable
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // Movable
    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

    // Load a dynamic library from path
    bool load(const std::string& path);

    // Unload the library
    void unload();

    // Check if library is loaded
    bool is_loaded() const;

    // Get a function pointer by name
    template <typename Func> Func get_function(const std::string& name) const {
        return reinterpret_cast<Func>(get_symbol(name));
    }

    // Get last error message
    const std::string& get_error() const {
        return error_;
    }

private:
    void* get_symbol(const std::string& name) const;

#ifdef _WIN32
    HMODULE handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
    std::string error_;
};

// Compile a test file to a dynamic library (DLL/SO/dylib)
// Returns the path to the compiled library, or empty string on failure
struct CompileToSharedLibResult {
    bool success = false;
    std::string lib_path; // Path to .dll/.so/.dylib
    std::string error_message;
    int64_t compile_time_us = 0;
};

CompileToSharedLibResult compile_test_to_shared_lib(const std::string& test_file,
                                                    bool verbose = false, bool no_cache = false);

// Run a test from a shared library in-process
// Captures stdout/stderr and returns the result
InProcessTestResult run_test_in_process(const std::string& lib_path);

// Run a test from a shared library in-process with sub-phase profiling
// Records exec.* sub-phases: load_lib, get_symbol, capture_start, run, capture_stop, cleanup
InProcessTestResult run_test_in_process_profiled(const std::string& lib_path,
                                                 PhaseTimings* timings);

// Combined: compile and run test in-process
// Falls back to process execution if library loading fails
InProcessTestResult compile_and_run_test_in_process(const std::string& test_file,
                                                    bool verbose = false, bool no_cache = false);

// Combined: compile and run test in-process with full phase profiling
// Records both compilation phases and exec sub-phases
InProcessTestResult compile_and_run_test_in_process_profiled(const std::string& test_file,
                                                             PhaseTimings* timings,
                                                             bool verbose = false,
                                                             bool no_cache = false);

// Compile a fuzz target file to a shared library
// Similar to test compilation, but generates tml_fuzz_target entry point
CompileToSharedLibResult compile_fuzz_to_shared_lib(const std::string& fuzz_file,
                                                    bool verbose = false, bool no_cache = false);

// ============================================================================
// Suite-Based Test Compilation (Batched DLLs)
// ============================================================================

// Information about a single test within a suite
struct SuiteTestInfo {
    std::string file_path;       // Original file path
    std::string test_name;       // Test name (stem of file)
    std::string entry_func_name; // Mangled entry function name in the DLL
    int test_count = 1;          // Number of @test functions in file
};

// A suite groups multiple test files into a single DLL
struct TestSuite {
    std::string name;                 // Suite name (e.g., "compiler_tests_compiler")
    std::string group;                // Display group (e.g., "compiler")
    std::vector<SuiteTestInfo> tests; // Tests in this suite
    std::string dll_path;             // Path to compiled suite DLL (after compilation)
};

// Result of compiling a suite
struct SuiteCompileResult {
    bool success = false;
    std::string dll_path;
    std::string error_message;
    std::string failed_test; // Which test file caused the failure
    int64_t compile_time_us = 0;
};

// Result of running a single test within a suite
struct SuiteTestResult {
    bool success = false;
    int exit_code = 0;
    std::string output;
    std::string error;
    int64_t duration_us = 0;
};

// Group test files into suites based on directory structure
// Returns suites grouped by: compiler/tests/X, lib/X/tests
std::vector<TestSuite> group_tests_into_suites(const std::vector<std::string>& test_files);

// Compile a suite of test files into a single DLL with a test registry
// Each test file becomes a separate entry function: tml_test_<index>()
// The DLL exports: tml_suite_count() and tml_suite_run(int index)
SuiteCompileResult compile_test_suite(const TestSuite& suite, bool verbose = false,
                                      bool no_cache = false);

// Compile a suite with phase profiling
SuiteCompileResult compile_test_suite_profiled(const TestSuite& suite, PhaseTimings* timings,
                                               bool verbose = false, bool no_cache = false);

// Run a specific test within a loaded suite DLL
// Uses the test index to call the correct entry function
// verbose: Enable debug logging for test execution
// timeout_seconds: Timeout in seconds (0 = no timeout)
// test_name: Name of the test (for timeout messages)
SuiteTestResult run_suite_test(DynamicLibrary& lib, int test_index, bool verbose = false,
                               int timeout_seconds = 0, const std::string& test_name = "");

// Run a specific test with profiling
SuiteTestResult run_suite_test_profiled(DynamicLibrary& lib, int test_index, PhaseTimings* timings,
                                        bool verbose = false);

} // namespace tml::cli
