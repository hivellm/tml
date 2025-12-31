#pragma once

#include <cstdint>
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

// Combined: compile and run test in-process
// Falls back to process execution if library loading fails
InProcessTestResult compile_and_run_test_in_process(const std::string& test_file,
                                                    bool verbose = false, bool no_cache = false);

// Compile a fuzz target file to a shared library
// Similar to test compilation, but generates tml_fuzz_target entry point
CompileToSharedLibResult compile_fuzz_to_shared_lib(const std::string& fuzz_file,
                                                    bool verbose = false, bool no_cache = false);

} // namespace tml::cli
