// Test command - main run_test function

#include "tester_internal.hpp"

namespace tml::cli {

// Using tester namespace for internal functions
using namespace tester;

// ============================================================================
// Parse Test Arguments
// ============================================================================

TestOptions parse_test_args(int argc, char* argv[], int start_index) {
    TestOptions opts;

    for (int i = start_index; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--nocapture") {
            opts.nocapture = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
        } else if (arg == "--ignored") {
            opts.ignored = true;
        } else if (arg == "--bench") {
            opts.bench = true;
        } else if (arg == "--release") {
            opts.release = true;
        } else if (arg == "--no-color") {
            opts.no_color = true;
        } else if (arg == "--no-cache") {
            opts.no_cache = true;
        } else if (arg.starts_with("--save-baseline=")) {
            opts.save_baseline = arg.substr(16);
        } else if (arg.starts_with("--compare=")) {
            opts.compare_baseline = arg.substr(10);
        } else if (arg == "--coverage") {
            opts.coverage = true;
        } else if (arg.starts_with("--coverage-output=")) {
            opts.coverage_output = arg.substr(18);
            opts.coverage = true; // Implicitly enable coverage
        } else if (arg == "--profile") {
            opts.profile = true;
            opts.test_threads = 1; // Force single-threaded for accurate profiling
        } else if (arg.starts_with("--test-threads=")) {
            opts.test_threads = std::stoi(arg.substr(15));
        } else if (arg.starts_with("--timeout=")) {
            opts.timeout_seconds = std::stoi(arg.substr(10));
        } else if (arg.starts_with("--group=")) {
            opts.patterns.push_back(arg.substr(8));
        } else if (arg.starts_with("--suite=")) {
            opts.patterns.push_back(arg.substr(8));
        } else if (!arg.starts_with("--")) {
            opts.patterns.push_back(arg);
        }
    }

    return opts;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int run_test(int argc, char* argv[], bool verbose) {
    // Enable ANSI colors on Windows
    enable_ansi_colors();

    TestOptions opts = parse_test_args(argc, argv, 2);
    opts.verbose = opts.verbose || verbose;

    // Important: Don't propagate verbose to compiler debug output
    // Test --verbose only controls test runner output format
    tml::CompilerOptions::verbose = false;

    ColorOutput c(!opts.no_color);

    // If --bench flag is set, run benchmarks instead of tests
    if (opts.bench) {
        return run_benchmarks(opts, c);
    }

    std::string cwd = fs::current_path().string();
    std::vector<std::string> test_files = discover_test_files(cwd);

    if (test_files.empty()) {
        if (!opts.quiet) {
            std::cout << c.yellow() << "No test files found" << c.reset()
                      << " (looking for *.test.tml)\n";
        }
        return 0;
    }

    // Filter test files by pattern
    if (!opts.patterns.empty()) {
        std::vector<std::string> filtered;
        for (const auto& file : test_files) {
            for (const auto& pattern : opts.patterns) {
                if (file.find(pattern) != std::string::npos) {
                    filtered.push_back(file);
                    break;
                }
            }
        }
        test_files = filtered;
    }

    if (test_files.empty()) {
        if (!opts.quiet) {
            std::cout << c.yellow() << "No tests matched the specified pattern(s)" << c.reset()
                      << "\n";
        }
        return 0;
    }

    // Print header
    if (!opts.quiet) {
        std::cout << "\n " << c.cyan() << c.bold() << "TML" << c.reset() << " " << c.dim()
                  << "v0.1.0" << c.reset() << "\n";
        std::cout << "\n " << c.dim() << "Running " << test_files.size() << " test file"
                  << (test_files.size() != 1 ? "s" : "") << "..." << c.reset() << "\n";
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    TestResultCollector collector;

    // Determine number of threads
    // Default to half of available cores for better resource usage
    unsigned int num_threads = opts.test_threads;
    if (num_threads == 0) {
        unsigned int hw_threads = std::thread::hardware_concurrency();
        if (hw_threads == 0) {
            num_threads = 4; // Fallback
        } else {
            num_threads = std::max(1u, hw_threads / 2);
        }
    }

    // Single-threaded mode for verbose/nocapture/profile or if only 1 test
    if (opts.verbose || opts.nocapture || opts.profile || test_files.size() == 1 ||
        num_threads == 1) {
        for (const auto& file : test_files) {
            TestResult result;
            if (opts.profile) {
                PhaseTimings timings;
                result = compile_and_run_test_profiled(file, opts, &timings);
                collector.add_timings(timings);
            } else {
                result = compile_and_run_test_with_result(file, opts);
            }
            bool is_compilation_error = result.compilation_error;
            collector.add(std::move(result));

            // Stop immediately if this was a compilation error
            if (is_compilation_error) {
                break;
            }
        }
    } else {
        // Parallel execution
        std::atomic<size_t> current_index{0};
        std::vector<std::thread> threads;

        // Limit threads to number of tests
        num_threads = std::min(num_threads, static_cast<unsigned int>(test_files.size()));

        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back(test_worker, std::ref(test_files), std::ref(current_index),
                                 std::ref(collector), std::ref(opts));
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    int64_t total_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Check if we stopped due to a compilation error
    if (collector.has_compilation_error()) {
        const auto& err = collector.first_compilation_error;

        // Print a clear, visible error message
        std::cerr << "\n";
        std::cerr
            << c.red() << c.bold()
            << "==============================================================================="
            << c.reset() << "\n";
        std::cerr << c.red() << c.bold() << "COMPILATION ERROR - Test run aborted" << c.reset()
                  << "\n";
        std::cerr
            << c.red() << c.bold()
            << "==============================================================================="
            << c.reset() << "\n";
        std::cerr << "\n";
        std::cerr << c.bold() << "File: " << c.reset() << err.file_path << "\n";
        std::cerr << "\n";
        std::cerr << c.red() << err.error_message << c.reset() << "\n";
        std::cerr << "\n";
        std::cerr << c.yellow() << "Fix the compilation error above before running tests again."
                  << c.reset() << "\n";
        std::cerr << "\n";

        return 1; // Fail immediately
    }

    // Print results
    if (!opts.quiet) {
        print_results_vitest_style(collector.results, opts, total_duration_ms);

        // Print profiling stats if enabled
        if (opts.profile && collector.profile_stats.total_tests > 0) {
            print_profile_stats(collector.profile_stats, opts);
        }
    }

    // Count failures
    int failed = 0;
    for (const auto& result : collector.results) {
        if (!result.passed) {
            failed++;
        }
    }

    return failed > 0 ? 1 : 0;
}

} // namespace tml::cli
