//! # Test Runner Implementation
//!
//! This file contains the main `run_test()` function that orchestrates
//! test discovery, execution, and reporting.
//!
//! ## Test Flow
//!
//! ```text
//! run_test()
//!   ├─ parse_test_args()     → TestOptions
//!   ├─ discover_test_files() → List of *.test.tml files
//!   ├─ Filter by pattern(s)
//!   ├─ Execution:
//!   │     ├─ Suite mode:   run_tests_suite_mode() (parallel DLL compilation)
//!   │     ├─ Profile mode: Sequential with timing collection
//!   │     └─ Normal mode:  Parallel worker threads
//!   └─ print_results_vitest_style()
//! ```
//!
//! ## Threading Model
//!
//! By default, tests run in parallel using N/2 threads where N is the
//! hardware thread count. This can be overridden with `--test-threads=N`.

#include "cli/builder/builder_internal.hpp"
#include "coverage.hpp"
#include "tester_internal.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Global unhandled exception filter for crash logging
static LONG WINAPI global_crash_filter(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;

    const char* name = "UNKNOWN";
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        name = "ACCESS_VIOLATION";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        name = "STACK_OVERFLOW";
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        name = "INTEGER_DIVIDE_BY_ZERO";
        break;
    }

    // Write directly to stderr handle for reliability
    char msg[256];
    int len = snprintf(msg, sizeof(msg),
                       "\n[FATAL CRASH] Exception 0x%08lX (%s)\n"
                       "Test crashed before exception could be caught.\n",
                       (unsigned long)code, name);

    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD written;
    WriteFile(hErr, msg, (DWORD)len, &written, NULL);
    FlushFileBuffers(hErr);

    return EXCEPTION_EXECUTE_HANDLER;
}

static void install_global_crash_handler() {
    SetUnhandledExceptionFilter(global_crash_filter);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
}
#endif

namespace tml::cli {

// Using tester namespace for internal functions
using namespace tester;

// ============================================================================
// Parse Test Arguments
// ============================================================================

/// Parses test command-line arguments into a TestOptions struct.
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
        } else if (arg == "--fuzz") {
            opts.fuzz = true;
        } else if (arg.starts_with("--fuzz-duration=")) {
            opts.fuzz_duration = std::stoi(arg.substr(16));
            opts.fuzz = true;
        } else if (arg.starts_with("--fuzz-max-len=")) {
            opts.fuzz_max_len = std::stoi(arg.substr(15));
        } else if (arg.starts_with("--corpus=")) {
            opts.corpus_dir = arg.substr(9);
        } else if (arg.starts_with("--crashes=")) {
            opts.crashes_dir = arg.substr(10);
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
        } else if (arg == "--coverage-source") {
            opts.coverage_source = true;
        } else if (arg.starts_with("--coverage-source-dir=")) {
            opts.coverage_source_dir = arg.substr(22);
            opts.coverage_source = true; // Implicitly enable source coverage
        } else if (arg == "--check-leaks") {
            opts.check_leaks = true;
        } else if (arg == "--no-check-leaks") {
            opts.check_leaks = false;
        } else if (arg == "--profile") {
            opts.profile = true;
            opts.test_threads = 1; // Force single-threaded for accurate profiling
        } else if (arg == "--suite") {
            opts.suite_mode = true; // Use suite-based DLL compilation (default)
        } else if (arg == "--no-suite") {
            opts.suite_mode = false; // Disable suite mode (one DLL per test file)
        } else if (arg == "--fail-fast" || arg == "-x") {
            opts.fail_fast = true; // Stop on first test failure
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
#ifdef _WIN32
    // Install global crash handler to log crashes that escape SEH
    install_global_crash_handler();
#endif

    // Enable ANSI colors on Windows
    enable_ansi_colors();

    TestOptions opts = parse_test_args(argc, argv, 2);
    opts.verbose = opts.verbose || verbose;

    // Important: Don't propagate verbose to compiler debug output
    // Test --verbose only controls test runner output format
    tml::CompilerOptions::verbose = false;

    // Set coverage options (global flag for runtime linking + output path)
    tml::CompilerOptions::coverage = opts.coverage;
    if (!opts.coverage_output.empty()) {
        tml::CompilerOptions::coverage_output = opts.coverage_output;
    } else if (opts.coverage) {
        // Default coverage output to build/coverage directory to keep project root clean
        fs::path coverage_dir = fs::path("build") / "coverage";
        fs::create_directories(coverage_dir);
        tml::CompilerOptions::coverage_output = (coverage_dir / "coverage.html").string();
    }

    // Set LLVM source coverage options
    tml::CompilerOptions::coverage_source = opts.coverage_source;
    if (!opts.coverage_source_dir.empty()) {
        tml::CompilerOptions::coverage_source_dir = opts.coverage_source_dir;
    }

    // Set memory leak checking option
    tml::CompilerOptions::check_leaks = opts.check_leaks;

    ColorOutput c(!opts.no_color);

    // If --bench flag is set, run benchmarks instead of tests
    if (opts.bench) {
        return run_benchmarks(opts, c);
    }

    // If --fuzz flag is set, run fuzz tests instead
    if (opts.fuzz) {
        return run_fuzz_tests(opts, c);
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

    // Initialize LLVM source coverage collector if enabled
    std::unique_ptr<tester::CoverageCollector> coverage_collector;
    if (opts.coverage_source) {
        coverage_collector = std::make_unique<tester::CoverageCollector>();
        if (!coverage_collector->initialize()) {
            std::cerr << c.red() << "Coverage Error: " << c.reset()
                      << coverage_collector->get_last_error() << "\n";
            return 1;
        }

        // Set up profraw output directory
        fs::path coverage_dir = fs::path(tml::CompilerOptions::coverage_source_dir);
        fs::path profraw_dir = coverage_dir / "profraw";
        coverage_collector->set_profraw_dir(profraw_dir);

        // Set environment variable for LLVM profile output
        // Use %p for process ID to handle parallel tests
        std::string profile_path = coverage_collector->get_profile_env("test");
#ifdef _WIN32
        _putenv_s("LLVM_PROFILE_FILE", profile_path.c_str());
#else
        setenv("LLVM_PROFILE_FILE", profile_path.c_str(), 1);
#endif

        if (!opts.quiet) {
            std::cout << c.dim() << " Source coverage enabled (output: " << coverage_dir.string()
                      << ")" << c.reset() << "\n";
        }
    }

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

    // Helper to process coverage after tests complete
    auto process_coverage = [&]() {
        if (!coverage_collector)
            return;

        fs::path coverage_dir = fs::path(tml::CompilerOptions::coverage_source_dir);
        fs::path profdata = coverage_dir / "coverage.profdata";

        coverage_collector->collect_profraw_files();

        if (!opts.quiet) {
            std::cout << c.dim() << "\n Generating coverage report..." << c.reset() << std::flush;
        }

        if (coverage_collector->merge_profiles(profdata)) {
            if (!opts.quiet) {
                std::cout << "\r" << std::string(40, ' ') << "\r" << std::flush;
            }

            // Generate function-level coverage report from profdata
            // (Line-level coverage requires coverage mapping data which we don't generate yet)
            auto report = coverage_collector->generate_function_report(profdata);

            if (!opts.quiet && report.success) {
                coverage_collector->print_function_report(report);
            } else if (!opts.quiet && !report.success) {
                std::cout << c.yellow() << "Warning: " << c.reset() << report.error_message << "\n";
            }
        } else if (!opts.quiet) {
            std::cout << "\n"
                      << c.red() << "Coverage Error: " << c.reset()
                      << coverage_collector->get_last_error() << "\n";
        }
    };

    // Suite mode: compile multiple test files into single DLLs per suite
    // This is now the default behavior as internal linkage prevents duplicate symbols
    if (opts.suite_mode) {
        run_tests_suite_mode(test_files, opts, collector, c);

        // Note: We no longer abort on compilation errors - they are recorded and
        // reported at the end, allowing other suites to continue running

        auto end_time = std::chrono::high_resolution_clock::now();
        int64_t total_duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        if (!opts.quiet) {
            print_results_vitest_style(collector.results, opts, total_duration_ms);

            // Print profiling stats if enabled
            if (opts.profile && collector.profile_stats.total_tests > 0) {
                print_profile_stats(collector.profile_stats, opts);
            }

            // Print TML runtime coverage summary
            if (opts.coverage && !CompilerOptions::coverage_output.empty()) {
                std::cout << "\n " << c.dim() << "Coverage report: " << c.reset()
                          << CompilerOptions::coverage_output << "\n";
            }
        }

        // Process LLVM source coverage
        process_coverage();

        int failed = 0;
        for (const auto& result : collector.results) {
            if (!result.passed) {
                failed++;
            }
        }

        return failed > 0 ? 1 : 0;
    }

    // Profile mode: parallel warm-up, then sequential profiled execution
    if (opts.profile && test_files.size() > 1 && num_threads > 1) {
        // Phase 1: Parallel warm-up (compile all DLLs to populate cache)
        if (!opts.quiet) {
            std::cout << c.dim() << " Warming up cache..." << c.reset() << std::flush;
        }

        std::atomic<size_t> warmup_index{0};
        std::atomic<bool> warmup_error{false};
        std::vector<std::thread> warmup_threads;

        unsigned int warmup_threads_count =
            std::min(num_threads, static_cast<unsigned int>(test_files.size()));
        for (unsigned int i = 0; i < warmup_threads_count; ++i) {
            warmup_threads.emplace_back(warmup_worker, std::ref(test_files), std::ref(warmup_index),
                                        std::ref(warmup_error), std::ref(opts));
        }

        for (auto& thread : warmup_threads) {
            thread.join();
        }

        if (!opts.quiet) {
            std::cout << "\r" << std::string(30, ' ') << "\r" << std::flush;
        }

        // Phase 2: Sequential profiled execution (with warm cache)
        for (const auto& file : test_files) {
            PhaseTimings timings;
            TestResult result = compile_and_run_test_profiled(file, opts, &timings);
            collector.add_timings(timings);
            collector.add(std::move(result));
            // Continue running other tests even if this one failed to compile
        }
    }
    // Single-threaded mode for verbose/nocapture or if only 1 test
    else if (opts.verbose || opts.nocapture || opts.profile || test_files.size() == 1 ||
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
            collector.add(std::move(result));
            // Continue running other tests even if this one failed to compile
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

    // Note: Compilation errors are now recorded as failed tests and printed in the
    // normal results output. We no longer abort the test run early.

    // Print results
    if (!opts.quiet) {
        print_results_vitest_style(collector.results, opts, total_duration_ms);

        // Print profiling stats if enabled
        if (opts.profile && collector.profile_stats.total_tests > 0) {
            print_profile_stats(collector.profile_stats, opts);
        }
    }

    // Process coverage
    process_coverage();

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
