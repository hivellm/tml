//! # Test Command Interface
//!
//! This header defines the test framework API and data structures.
//!
//! ## Test Types
//!
//! | Type              | Description                              |
//! |-------------------|------------------------------------------|
//! | `TestResult`      | Result of a single test file             |
//! | `TestGroup`       | Group of tests with statistics           |
//! | `BenchmarkResult` | Benchmark timing results                 |
//! | `FuzzResult`      | Fuzz testing results                     |
//! | `TestOptions`     | Command-line options for `tml test`      |
//!
//! ## Test Options
//!
//! Key options include:
//! - `--nocapture`: Show stdout/stderr during tests
//! - `--bench`: Run benchmarks instead of tests
//! - `--fuzz`: Run fuzz tests
//! - `--profile`: Show detailed phase timings
//! - `--suite-mode`: Bundle tests into DLLs (default: true)

#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace tml::cli {

// ANSI color codes for terminal output
namespace colors {
inline const char* reset = "\033[0m";
inline const char* bold = "\033[1m";
inline const char* dim = "\033[2m";
inline const char* red = "\033[31m";
inline const char* green = "\033[32m";
inline const char* yellow = "\033[33m";
inline const char* blue = "\033[34m";
inline const char* magenta = "\033[35m";
inline const char* cyan = "\033[36m";
inline const char* gray = "\033[90m";
inline const char* bg_red = "\033[41m";
inline const char* bg_green = "\033[42m";
} // namespace colors

// Test result for a single test file
struct TestResult {
    std::string file_path;
    std::string test_name;
    std::string group; // Directory group (e.g., "compiler", "runtime")
    bool passed = false;
    bool timeout = false;
    bool compilation_error = false; // True if the test failed to compile
    int exit_code = 0;
    int64_t duration_ms = 0; // Duration in milliseconds
    std::string error_message;
    int test_count = 1; // Number of @test functions in this file
};

// Test group summary
struct TestGroup {
    std::string name;
    std::vector<TestResult> results;
    int passed = 0;
    int failed = 0;
    int64_t total_duration_ms = 0;
};

// Benchmark result for a single benchmark
struct BenchmarkResult {
    std::string file_path;
    std::string bench_name;
    int64_t ns_per_iter = 0; // Nanoseconds per iteration
    int64_t iterations = 0;  // Number of iterations
    bool passed = true;
};

// Fuzz result for a single fuzz target
struct FuzzResult {
    std::string file_path;
    std::string fuzz_name;
    int64_t iterations = 0;    // Number of iterations run
    int64_t duration_ms = 0;   // Total fuzzing duration
    bool found_crash = false;  // True if a crash was found
    std::string crash_input;   // Input that caused crash (hex encoded)
    std::string crash_message; // Error message from crash
    bool passed = true;        // True if no crashes found
};

// Phase timing for profiling
struct PhaseTiming {
    std::string name;
    int64_t duration_us = 0; // Microseconds for precision
};

// Per-file leak information for the leak summary table
struct LeakFileInfo {
    std::string file_path;
    int32_t leak_count = 0;
    int64_t leak_bytes = 0;
};

// Aggregated leak statistics across all suites
struct LeakStats {
    int32_t total_leaks = 0;
    int64_t total_bytes = 0;
    std::vector<LeakFileInfo> files; // Per-file breakdown

    void add(const std::string& file, int32_t count, int64_t bytes) {
        // Merge into existing entry if same file
        for (auto& f : files) {
            if (f.file_path == file) {
                f.leak_count += count;
                f.leak_bytes += bytes;
                total_leaks += count;
                total_bytes += bytes;
                return;
            }
        }
        files.push_back({file, count, bytes});
        total_leaks += count;
        total_bytes += bytes;
    }
};

// Aggregated phase timings across all tests
struct ProfileStats {
    std::map<std::string, int64_t> total_us; // Total time per phase
    std::map<std::string, int64_t> max_us;   // Max time per phase
    std::map<std::string, int64_t> count;    // Number of measurements
    int64_t total_tests = 0;

    void add(const std::string& phase, int64_t us) {
        total_us[phase] += us;
        if (us > max_us[phase])
            max_us[phase] = us;
        count[phase]++;
    }
};

// Test command options
struct TestOptions {
    std::vector<std::string> patterns; // Test name patterns to filter
    bool nocapture = false;            // Show stdout/stderr during tests
    bool verbose = false;              // Verbose output
    bool quiet = false;                // Minimal output
    bool ignored = false;              // Run only ignored tests
    bool bench = false;                // Run benchmarks
    bool fuzz = false;                 // Run fuzz tests
    int fuzz_duration = 10;            // Fuzz duration in seconds (default: 10s)
    int fuzz_max_len = 4096;           // Maximum fuzz input length
    int test_threads = 0;              // Parallel test threads (0 = auto)
    bool release = false;              // Run in release mode
    std::string test_binary;           // Path to test binary (if provided)
    int timeout_seconds = 20;          // Test timeout in seconds (default: 20s)
    bool no_color = false;             // Disable colored output
    bool no_cache = false;             // Disable build cache
    std::string save_baseline;         // Save benchmark results to file (for --bench)
    std::string compare_baseline;      // Compare against baseline file (for --bench)
    bool coverage = false;             // Enable code coverage tracking (function-level)
    std::string coverage_output;       // Coverage output file (default: coverage.html)
    bool coverage_source = false;      // Enable LLVM source code coverage
    std::string coverage_source_dir;   // Directory to write coverage reports
    bool profile = false;              // Show detailed phase timings
    std::string log_path;              // Custom log file path (--log=<path>)
    bool suite_mode = false;           // Individual mode: one DLL per test file (default)
    std::string corpus_dir;            // Directory for fuzz corpus (inputs)
    std::string crashes_dir;           // Directory to save crash inputs
    bool check_leaks = true;           // Memory leak detection (enabled by default)
    bool fail_fast = true;             // Stop on first test failure (enabled by default)
    bool backtrace = true;             // Show backtrace on test failures (enabled by default)
    std::string backend = "llvm";      // Codegen backend ("llvm" or "cranelift")
    std::vector<std::string>
        features; // Feature flags (--feature network → defines FEATURE_NETWORK)
    std::vector<std::string> suite_filters; // Suite group filters (e.g., "core/str", "std/json")
    bool list_suites = false;               // Print discovered suite groups and exit
};

// Parse test command arguments
TestOptions parse_test_args(int argc, char* argv[], int start_index);

// Run test command
// Discovers and runs all tests in *.test.tml files
int run_test(int argc, char* argv[], bool verbose);

} // namespace tml::cli
