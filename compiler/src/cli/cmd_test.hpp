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

// Test result for a single test
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

// Phase timing for profiling
struct PhaseTiming {
    std::string name;
    int64_t duration_us = 0; // Microseconds for precision
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
    int test_threads = 0;              // Parallel test threads (0 = auto)
    bool release = false;              // Run in release mode
    std::string test_binary;           // Path to test binary (if provided)
    int timeout_seconds = 20;          // Test timeout in seconds (default: 20s)
    bool no_color = false;             // Disable colored output
    bool no_cache = false;             // Disable build cache
    std::string save_baseline;         // Save benchmark results to file (for --bench)
    std::string compare_baseline;      // Compare against baseline file (for --bench)
    bool coverage = false;             // Enable code coverage tracking
    std::string coverage_output;       // Coverage output file (default: coverage.html)
    bool profile = false;              // Show detailed phase timings
};

// Parse test command arguments
TestOptions parse_test_args(int argc, char* argv[], int start_index);

// Run test command
// Discovers and runs all tests in *.test.tml files
int run_test(int argc, char* argv[], bool verbose);

} // namespace tml::cli
