//! # Test Coordinator (v3 — fully independent)
//!
//! Central orchestrator for the TML test system. Drives the complete pipeline:
//! discovery → grouping → compilation → execution → aggregation.
//!
//! ## Architecture
//!
//! ```text
//! TestConfig → run_tests()
//!   ├─ testing::discover_tests()           [independent discovery]
//!   ├─ testing::group_into_suites()        [independent grouping, max_per_suite=1]
//!   ├─ testing::compile_suites_parallel()  [QueryContext pipeline]
//!   ├─ Process::launch()                   [cross-platform subprocess]
//!   ├─ parse_json_event()                  [NDJSON protocol]
//!   ├─ testing::print_coverage_report()    [independent coverage]
//!   └─ → TestRunResult
//! ```
//!
//! Zero dependency on cli/tester/. All infrastructure is self-contained
//! within the testing/ module.
//!
//! ## Usage
//!
//! ```cpp
//! TestConfig config;
//! config.root_dir = ".";
//! config.suite_filters = {"core/str"};
//!
//! auto result = run_tests(config);
//! // result.passed, result.failed, result.suites[0].tests[0].name, etc.
//! ```

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tml::testing {

// ============================================================================
// Configuration
// ============================================================================

/// Configuration for the test coordinator.
/// Maps from the CLI TestOptions struct to a clean, self-contained config.
struct TestConfig {
    std::vector<std::string> patterns;      // Test file path filters
    std::vector<std::string> suite_filters; // Suite group filters (e.g. "core/str")
    int max_per_suite = 8;                  // Tests per suite (1 = individual mode)
    int compile_threads = 0;                // Parallel compile workers (0 = auto)
    int exec_concurrent = 0;                // Max concurrent subprocesses (0 = auto)
    int timeout_seconds = 20;               // Per-suite subprocess timeout
    bool no_cache = false;                  // Force recompile everything
    bool verbose = false;
    bool coverage = false;
    bool fail_fast = true;    // Stop on first failure
    bool list_suites = false; // Print suites and exit
    std::string root_dir;     // Discovery root (empty = cwd)
};

// ============================================================================
// Results
// ============================================================================

/// Result of a single test within a suite.
struct CoordinatorTestResult {
    int index = 0;
    std::string name;
    std::string file;
    bool passed = false;
    int exit_code = 0;
    std::string error;
    int64_t duration_us = 0;
};

/// Result of executing one suite (compilation + execution).
struct SuiteRunResult {
    std::string name;
    std::string group;
    int test_count = 0;
    int passed = 0;
    int failed = 0;
    int crashed = 0;
    bool compile_ok = true;
    bool cached = false; // True if result came from cache
    std::string compile_error;
    int64_t compile_time_us = 0;
    int64_t exec_time_us = 0;
    std::vector<CoordinatorTestResult> tests;
};

/// Aggregate result of a full test run.
struct TestRunResult {
    int total_tests = 0;
    int passed = 0;
    int failed = 0;
    int crashed = 0;
    int timed_out = 0;
    int skipped = 0;
    int compilation_errors = 0;
    int64_t total_duration_us = 0;
    int covered_functions_count = 0; // Covered library functions (when --coverage)
    std::vector<SuiteRunResult> suites;
};

// ============================================================================
// Entry point
// ============================================================================

/// Run all tests matching the configuration.
/// Returns structured results — the caller is responsible for formatting/output.
TestRunResult run_tests(const TestConfig& config);

} // namespace tml::testing
