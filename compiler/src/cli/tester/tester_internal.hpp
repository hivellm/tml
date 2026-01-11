//! # Tester Internal Interface
//!
//! This header defines internal types and utilities for the test framework.
//!
//! ## Components
//!
//! | Type                  | Description                          |
//! |-----------------------|--------------------------------------|
//! | `TestResultCollector` | Thread-safe result aggregation       |
//! | `ColorOutput`         | ANSI color output wrapper            |
//!
//! ## Test Execution Pipeline
//!
//! ```text
//! discover tests → group into suites → compile DLLs → run → report
//! ```

#pragma once

// Internal header for test command implementation
// Contains shared utilities, types, and helpers

#include "cli/commands/cmd_build.hpp"
#include "cli/commands/cmd_test.hpp"
#include "cli/tester/test_runner.hpp"
#include "cli/utils.hpp"
#include "common.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#undef min
#undef max
#endif

namespace fs = std::filesystem;

namespace tml::cli::tester {

// ============================================================================
// Color Output Helper
// ============================================================================

// Color helper that respects no_color option
struct ColorOutput {
    bool enabled;

    ColorOutput(bool use_color) : enabled(use_color) {}

    const char* reset() const {
        return enabled ? colors::reset : "";
    }
    const char* bold() const {
        return enabled ? colors::bold : "";
    }
    const char* dim() const {
        return enabled ? colors::dim : "";
    }
    const char* red() const {
        return enabled ? colors::red : "";
    }
    const char* green() const {
        return enabled ? colors::green : "";
    }
    const char* yellow() const {
        return enabled ? colors::yellow : "";
    }
    const char* blue() const {
        return enabled ? colors::blue : "";
    }
    const char* cyan() const {
        return enabled ? colors::cyan : "";
    }
    const char* gray() const {
        return enabled ? colors::gray : "";
    }
    const char* magenta() const {
        return enabled ? colors::magenta : "";
    }
};

// ============================================================================
// Thread-safe Result Collector
// ============================================================================

struct TestResultCollector {
    std::mutex mutex;
    std::vector<TestResult> results;
    std::atomic<bool> compilation_error_occurred{false};
    TestResult first_compilation_error;
    ProfileStats profile_stats;

    void add(TestResult result);
    void add_timings(const PhaseTimings& timings);
    bool has_compilation_error() const;
};

// ============================================================================
// Helper Functions
// ============================================================================

// Enable ANSI colors on Windows
void enable_ansi_colors();

// Format duration in human-readable form
std::string format_duration(int64_t ms);

// Extract group name from file path
std::string extract_group_name(const std::string& file_path);

// Count @test functions in a file
int count_tests_in_file(const std::string& file_path);

// ============================================================================
// Discovery Functions
// ============================================================================

// Discover test files (*.test.tml) in a directory
std::vector<std::string> discover_test_files(const std::string& root_dir);

// Discover benchmark files (*.bench.tml) in a directory
std::vector<std::string> discover_bench_files(const std::string& root_dir);

// ============================================================================
// Execution Functions
// ============================================================================

// Run test using in-process execution (DLL loading)
TestResult compile_and_run_test_inprocess(const std::string& test_file, const TestOptions& opts);

// Run test with result collection
TestResult compile_and_run_test_with_result(const std::string& test_file, const TestOptions& opts);

// Run test with profiling
TestResult compile_and_run_test_profiled(const std::string& test_file, const TestOptions& opts,
                                         PhaseTimings* timings);

// Thread worker for parallel test execution
void test_worker(const std::vector<std::string>& test_files, std::atomic<size_t>& current_index,
                 TestResultCollector& collector, const TestOptions& opts);

// Warm-up worker for parallel DLL compilation (no execution)
void warmup_worker(const std::vector<std::string>& test_files, std::atomic<size_t>& current_index,
                   std::atomic<bool>& has_error, const TestOptions& opts);

// ============================================================================
// Output Functions
// ============================================================================

// Print test results in Vitest style
void print_results_vitest_style(const std::vector<TestResult>& results, const TestOptions& opts,
                                int64_t total_duration_ms);

// Print profile statistics
void print_profile_stats(const ProfileStats& stats, const TestOptions& opts);

// ============================================================================
// Benchmark Functions
// ============================================================================

// Parse benchmark output to extract timing results
std::vector<BenchmarkResult> parse_bench_output(const std::string& output,
                                                const std::string& file_path);

// Save benchmark results to JSON file
void save_benchmark_baseline(const std::string& filename,
                             const std::vector<BenchmarkResult>& results);

// Load benchmark baseline from JSON file
std::map<std::string, int64_t> load_benchmark_baseline(const std::string& filename);

// Run benchmarks and display results
int run_benchmarks(const TestOptions& opts, const ColorOutput& c);

// ============================================================================
// Suite-Based Execution Functions
// ============================================================================

// Run tests using suite-based DLL compilation (fewer DLLs, faster loading)
// Returns exit code (0 = all passed, 1 = failures)
int run_tests_suite_mode(const std::vector<std::string>& test_files, const TestOptions& opts,
                         TestResultCollector& collector, const ColorOutput& c);

// ============================================================================
// Fuzz Functions
// ============================================================================

// Discover fuzz files (*.fuzz.tml) in a directory
std::vector<std::string> discover_fuzz_files(const std::string& root_dir);

// Generate random bytes for fuzzing
std::vector<uint8_t> generate_fuzz_input(size_t max_len);

// Mutate existing fuzz input
std::vector<uint8_t> mutate_fuzz_input(const std::vector<uint8_t>& input, size_t max_len);

// Convert bytes to hex string for reporting
std::string bytes_to_hex(const std::vector<uint8_t>& bytes);

// Convert hex string back to bytes
std::vector<uint8_t> hex_to_bytes(const std::string& hex);

// Run fuzz tests and display results
int run_fuzz_tests(const TestOptions& opts, const ColorOutput& c);

} // namespace tml::cli::tester
