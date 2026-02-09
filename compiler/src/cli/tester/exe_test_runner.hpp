//! # EXE-Based Test Runner Interface (v2)
//!
//! This header defines the EXE-based test execution system.
//! Instead of compiling tests to DLLs and loading them in-process,
//! tests are compiled to executables and run as subprocesses.
//!
//! ## Architecture (Go-style)
//!
//! ```text
//! test.tml → LLVM IR → .obj ─┐
//!                              ├─ link → suite.exe → subprocess execution
//! dispatcher_main.ll → .obj ──┘
//! ```
//!
//! ## Key Types
//!
//! - `ExeCompileResult`: Result of compiling a suite to an EXE
//! - `SubprocessTestResult`: Result of running a test via subprocess
//!
//! ## Function Signatures
//!
//! - `compile_test_suite_exe()`: Compile a TestSuite to an EXE
//! - `run_test_subprocess()`: Execute a test via subprocess
//! - `generate_dispatcher_ir()`: Generate dispatcher main() LLVM IR
//! - `run_tests_exe_mode()`: Top-level orchestration

#pragma once

#include "cli/commands/cmd_test.hpp"
#include "cli/tester/test_runner.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tml::cli {

// Result of compiling a test suite to an executable
struct ExeCompileResult {
    bool success = false;
    std::string exe_path;
    std::string error_message;
    std::string failed_test;
    int64_t compile_time_us = 0;
};

// Result of running a single test via subprocess
struct SubprocessTestResult {
    bool success = false;
    int exit_code = 0;
    std::string stdout_output;
    std::string stderr_output;
    int64_t duration_us = 0;
    bool timed_out = false;
};

// Compile a test suite to an EXE (adapts compile_test_suite for EXE output)
ExeCompileResult compile_test_suite_exe(const TestSuite& suite, bool verbose = false,
                                        bool no_cache = false);

// Run a single test from a compiled suite EXE via subprocess
// The EXE is invoked with --test-index=N to run a specific test
SubprocessTestResult run_test_subprocess(const std::string& exe_path, int test_index,
                                         int timeout_seconds = 60,
                                         const std::string& test_name = "");

// Generate LLVM IR for a dispatcher main() that routes --test-index=N to tml_test_N()
std::string generate_dispatcher_ir(int total_tests, const std::string& module_name);

} // namespace tml::cli

namespace tml::cli::tester {

// Forward declarations (defined in tester_internal.hpp)
struct TestResultCollector;
struct ColorOutput;

// Run tests using EXE-based subprocess execution (Go-style)
// Returns exit code (0 = all passed, 1 = failures)
int run_tests_exe_mode(const std::vector<std::string>& test_files, const TestOptions& opts,
                       TestResultCollector& collector, const ColorOutput& c);

} // namespace tml::cli::tester
