//! # Test Runner Internal Helpers
//!
//! Shared utilities used across the split test_runner_*.cpp files.
//! This is an internal header — not part of the public test runner API.

#pragma once

#include "cli/builder/builder_internal.hpp"
#include "cli/builder/object_compiler.hpp"
#include "cli/commands/cmd_build.hpp"
#include "cli/tester/tester_internal.hpp"
#include "codegen/codegen_backend.hpp"
#include "hir/hir_builder.hpp"
#include "mir/hir_mir_builder.hpp"
#include "preprocessor/preprocessor.hpp"
#include "test_runner.hpp"
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

// Bring build helpers into scope
using namespace build;

// ============================================================================
// Thread Count Calculation
// ============================================================================

/// Calculate thread count for internal compilation parallelism.
/// Returns 2-6 threads based on hardware cores, capped to avoid oversubscription
/// when multiple suites compile in parallel (each suite calls this independently).
unsigned int calc_codegen_threads(unsigned int task_count);

// ============================================================================
// C Runtime Logger Bridge
// ============================================================================

/// Callback that routes C runtime log messages through the C++ Logger.
/// Set via rt_log_set_callback() when loading test DLLs.
void rt_log_bridge_callback(int level, const char* module, const char* message);

// ============================================================================
// Windows Crash Handler
// ============================================================================

#ifdef _WIN32
// Thread-local storage for crash info
extern thread_local char g_crash_msg[256];
extern thread_local bool g_crash_occurred;

const char* get_exception_name(DWORD code);
LONG WINAPI crash_filter(EXCEPTION_POINTERS* info);

/// SEH wrapper for calling test functions.
/// Can't be in same function as C++ try/catch, so it's a separate function.
/// Returns: function result, or -2 on crash.
int call_test_with_seh(TestMainFunc func);

/// SEH wrapper for calling tml_run_test_with_catch(test_func).
/// Catches STATUS_BAD_STACK and other fatal crashes that the VEH handler
/// cannot recover from (e.g., when longjmp corrupts the stack).
/// Returns: run_with_catch result, or -2 on crash.
using TmlRunTestWithCatchFn = int32_t (*)(TestMainFunc);
int call_run_with_catch_seh(TmlRunTestWithCatchFn run_with_catch, TestMainFunc test_func);
#endif

// ============================================================================
// Output Capture Helper
// ============================================================================

/// RAII class to capture stdout/stderr to a temp file during test execution,
/// then restore original file descriptors and read captured output.
class OutputCapture {
public:
    OutputCapture() : capturing_(false), temp_file_path_("") {}

    ~OutputCapture() {
        stop();
        cleanup();
    }

    // Non-copyable, non-movable
    OutputCapture(const OutputCapture&) = delete;
    OutputCapture& operator=(const OutputCapture&) = delete;

    bool start();
    std::string stop();

    const std::string& get_output() const {
        return captured_output_;
    }

private:
    void cleanup();

    bool capturing_;
    fs::path temp_file_path_;
    std::string captured_output_;
    int saved_stdout_ = -1;
    int saved_stderr_ = -1;
};

// ============================================================================
// Slow Task Thresholds
// ============================================================================

/// Ratio threshold: a task is "slow" if it takes SLOW_TASK_THRESHOLD× the median.
constexpr double SLOW_TASK_THRESHOLD = 5.0;

/// Minimum absolute time (microseconds) before we flag something as slow.
constexpr int64_t MIN_SLOW_THRESHOLD_US = 45000000; // 45 seconds

} // namespace tml::cli
