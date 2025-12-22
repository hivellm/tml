#pragma once

#include <string>
#include <vector>

namespace tml::cli {

// Test command options
struct TestOptions {
    std::vector<std::string> patterns;    // Test name patterns to filter
    bool nocapture = false;               // Show stdout/stderr during tests
    bool verbose = false;                 // Verbose output
    bool quiet = false;                   // Minimal output
    bool ignored = false;                 // Run only ignored tests
    bool bench = false;                   // Run benchmarks
    int test_threads = 0;                 // Parallel test threads (0 = auto)
    bool release = false;                 // Run in release mode
    std::string test_binary;              // Path to test binary (if provided)
};

// Parse test command arguments
TestOptions parse_test_args(int argc, char* argv[], int start_index);

// Run test command
// Discovers and runs all tests in *.test.tml files
int run_test(int argc, char* argv[], bool verbose);

// Test discovery: Find all *.test.tml files
std::vector<std::string> discover_test_files(const std::string& root_dir);

// Compile and run a test file
int compile_and_run_test(const std::string& test_file, const TestOptions& opts);

} // namespace tml::cli
