//! # Suite-Based Test Execution
//!
//! This file implements the optimized suite mode for running tests.
//!
//! ## How Suite Mode Works
//!
//! 1. Group test files by directory (compiler/tests/runtime → runtime suite)
//! 2. Compile all tests in a suite to a single DLL
//! 3. Load DLL once, run each test function in sequence
//! 4. Clean up and report results
//!
//! ## Performance Benefits
//!
//! ```text
//! Individual mode: 100 tests × (compile + load DLL) = slow
//! Suite mode:      5 suites × compile + 100 × run = fast
//! ```
//!
//! DLL loading is expensive (~20ms), so bundling tests reduces overhead.
//!
//! ## Suite Structure
//!
//! ```text
//! compiler_tests_runtime.dll
//!   ├─ tml_test_0()  // first test file
//!   ├─ tml_test_1()  // second test file
//!   └─ ...
//! ```

#include "tester_internal.hpp"

namespace tml::cli::tester {

// ============================================================================
// Suite-Based Test Execution
// ============================================================================

int run_tests_suite_mode(const std::vector<std::string>& test_files, const TestOptions& opts,
                         TestResultCollector& collector, const ColorOutput& c) {
    using Clock = std::chrono::high_resolution_clock;

    // Aggregate covered functions across all suites for library coverage analysis
    std::set<std::string> all_covered_functions;

    try {

        // Group test files into suites
        auto phase_start = Clock::now();
        auto suites = group_tests_into_suites(test_files);
        if (opts.profile) {
            collector.profile_stats.add(
                "group_suites",
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start)
                    .count());
        }

        if (!opts.quiet) {
            std::cout << c.dim() << " Grouped into " << suites.size() << " test suite"
                      << (suites.size() != 1 ? "s" : "") << c.reset() << "\n";
            std::cout << std::flush;
            std::cerr << std::flush;
        }

        // Compile all suites IN PARALLEL
        std::vector<std::pair<TestSuite, DynamicLibrary>> loaded_suites;

        // Determine number of compilation threads
        unsigned int hw_threads = std::thread::hardware_concurrency();
        unsigned int num_compile_threads = (hw_threads > 0) ? std::max(1u, hw_threads) : 4;

        // Structure to hold compilation results
        struct CompileJob {
            size_t index;
            TestSuite suite;
            SuiteCompileResult result;
            bool compiled = false;
        };

        std::vector<CompileJob> jobs;
        jobs.reserve(suites.size());
        for (size_t i = 0; i < suites.size(); ++i) {
            jobs.push_back({i, std::move(suites[i]), {}, false});
        }

        // Thread-safe job queue
        std::atomic<size_t> next_job{0};
        std::mutex output_mutex;

        // Compile suites in parallel
        auto compile_worker = [&]() {
            while (true) {
                size_t job_idx = next_job.fetch_add(1);
                if (job_idx >= jobs.size())
                    break;

                auto& job = jobs[job_idx];

                if (!opts.quiet && opts.verbose) {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    std::cout << c.dim() << " Compiling suite: " << job.suite.name << " ("
                              << job.suite.tests.size() << " tests)" << c.reset() << "\n";
                    std::cout << std::flush;
                }

                job.result = compile_test_suite(job.suite, opts.verbose, opts.no_cache);
                job.compiled = true;
            }
        };

        // Launch compilation threads
        if (!opts.quiet) {
            std::cout << c.dim() << " Compiling " << jobs.size() << " suites with "
                      << num_compile_threads << " threads..." << c.reset() << "\n";
            std::cout << std::flush;
        }

        phase_start = Clock::now();
        std::vector<std::thread> compile_threads;
        for (unsigned int t = 0; t < std::min(num_compile_threads, (unsigned int)jobs.size());
             ++t) {
            compile_threads.emplace_back(compile_worker);
        }
        for (auto& t : compile_threads) {
            t.join();
        }

        if (opts.profile) {
            collector.profile_stats.add(
                "parallel_compile",
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start)
                    .count());
        }

        // Process compilation results and load DLLs (sequential for stability)
        for (auto& job : jobs) {
            auto& suite = job.suite;
            auto& compile_result = job.result;

            if (!compile_result.success) {
                // Report compilation error but continue with other suites
                TestResult error_result;
                error_result.file_path = compile_result.failed_test;
                error_result.test_name = fs::path(compile_result.failed_test).stem().string();
                error_result.group = suite.group;
                error_result.passed = false;
                error_result.compilation_error = true;
                error_result.exit_code = EXIT_COMPILATION_ERROR;
                error_result.error_message = "COMPILATION FAILED\n" + compile_result.error_message;

                collector.add(std::move(error_result));

                // Log the error immediately so user sees it
                std::cerr << c.red() << c.bold() << "COMPILATION FAILED: " << c.reset()
                          << error_result.test_name << "\n";
                if (!compile_result.error_message.empty()) {
                    std::cerr << c.dim() << compile_result.error_message << c.reset() << "\n";
                }

                continue; // Continue with other suites instead of stopping
            }

            suite.dll_path = compile_result.dll_path;

            // Load the suite DLL
            phase_start = Clock::now();
            DynamicLibrary lib;
            bool load_ok = lib.load(suite.dll_path);
            if (opts.profile) {
                collector.profile_stats.add("suite_load",
                                            std::chrono::duration_cast<std::chrono::microseconds>(
                                                Clock::now() - phase_start)
                                                .count());
            }
            if (!load_ok) {
                TestResult error_result;
                error_result.file_path = suite.tests.empty() ? "" : suite.tests[0].file_path;
                error_result.test_name = suite.name;
                error_result.group = suite.group;
                error_result.passed = false;
                error_result.error_message = "Failed to load suite DLL: " + lib.get_error();

                collector.add(std::move(error_result));

                // Log the error immediately
                std::cerr << c.red() << c.bold() << "DLL LOAD FAILED: " << c.reset() << suite.name
                          << "\n";
                std::cerr << c.dim() << lib.get_error() << c.reset() << "\n";

                continue; // Continue with other suites instead of stopping
            }

            loaded_suites.push_back({std::move(suite), std::move(lib)});
        }

        // Run all tests from loaded suites
        // Each suite runs in isolation - if one crashes, others continue
        for (auto& [suite, lib] : loaded_suites) {
            if (opts.verbose) {
                std::cerr << "[DEBUG] Running tests from suite: " << suite.name << " ("
                          << suite.tests.size() << " test files)\n"
                          << std::flush;
            }

            for (size_t i = 0; i < suite.tests.size(); ++i) {
                const auto& test_info = suite.tests[i];

                if (opts.verbose) {
                    std::cerr << "[DEBUG] Starting test " << (i + 1) << "/" << suite.tests.size()
                              << ": " << test_info.test_name << " (" << test_info.test_count
                              << " sub-tests)\n"
                              << std::flush;
                }

                TestResult result;
                result.file_path = test_info.file_path;
                result.test_name = test_info.test_name;
                result.group = suite.group;
                result.test_count = test_info.test_count;

                phase_start = Clock::now();

                // Flush all output before running test (helps with crash debugging)
                std::cout << std::flush;
                std::cerr << std::flush;
                std::fflush(stdout);
                std::fflush(stderr);

                if (opts.verbose) {
                    std::cerr << "[DEBUG] Calling run_suite_test for index " << i << "...\n"
                              << std::flush;
                }

                auto run_result = run_suite_test(lib, static_cast<int>(i), opts.verbose);
                int run_exit_code = run_result.exit_code;
                bool run_success = run_result.success;
                if (opts.verbose) {
                    std::cerr << "[DEBUG] run_suite_test returned: exit_code=" << run_exit_code
                              << ", success=" << (run_success ? "true" : "false") << "\n"
                              << std::flush;
                }
                auto run_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                           Clock::now() - phase_start)
                                           .count();

                if (opts.profile) {
                    collector.profile_stats.add("test_run", run_duration_us);
                    collector.profile_stats.total_tests++;
                }

                result.duration_ms = run_duration_us / 1000;
                result.passed = run_success;
                result.exit_code = run_exit_code;

                if (!result.passed) {
                    result.error_message = "Exit code: " + std::to_string(result.exit_code);
                    // Include detailed error message from run_suite_test
                    if (!run_result.error.empty()) {
                        result.error_message += "\n" + run_result.error;
                    }
                    // Include captured output which may contain panic/crash info
                    if (!run_result.output.empty()) {
                        result.error_message += "\n" + run_result.output;
                    }
                }

                collector.add(std::move(result));
            }

            // Collect coverage data before unloading DLL
            if (CompilerOptions::coverage) {
                // Get coverage functions from the DLL
                using PrintCoverageFunc = void (*)();
                using WriteCoverageHtmlFunc = void (*)(const char*);
                using TmlSetOutputSuppressed = void (*)(int32_t);
                using GetFuncCountFunc = int32_t (*)();
                using GetFuncNameFunc = const char* (*)(int32_t);
                using GetFuncHitsFunc = int32_t (*)(int32_t);

                auto print_coverage =
                    lib.get_function<PrintCoverageFunc>("tml_print_coverage_report");
                auto write_html = lib.get_function<WriteCoverageHtmlFunc>("write_coverage_html");
                auto set_output_suppressed =
                    lib.get_function<TmlSetOutputSuppressed>("tml_set_output_suppressed");
                auto get_func_count = lib.get_function<GetFuncCountFunc>("tml_get_func_count");
                auto get_func_name = lib.get_function<GetFuncNameFunc>("tml_get_func_name");
                auto get_func_hits = lib.get_function<GetFuncHitsFunc>("tml_get_func_hits");

                if (opts.verbose) {
                    std::cerr << "[DEBUG] Coverage enabled, looking for functions...\n";
                    std::cerr << "[DEBUG] print_coverage: "
                              << (print_coverage ? "found" : "NOT FOUND") << "\n";
                    std::cerr << "[DEBUG] write_html: " << (write_html ? "found" : "NOT FOUND")
                              << "\n";
                }

                // Collect covered function names for library analysis
                if (get_func_count && get_func_name && get_func_hits) {
                    int32_t count = get_func_count();
                    for (int32_t i = 0; i < count; i++) {
                        const char* name = get_func_name(i);
                        int32_t hits = get_func_hits(i);
                        if (name && hits > 0) {
                            all_covered_functions.insert(name);
                        }
                    }
                }

                // Ensure output is not suppressed when printing coverage report
                if (set_output_suppressed) {
                    set_output_suppressed(0);
                }
                // Flush before printing
                std::cout << std::flush;
                std::cerr << std::flush;
                std::fflush(stdout);
                std::fflush(stderr);

                // In verbose mode, print the full coverage report to console
                if (print_coverage && opts.verbose) {
                    print_coverage();
                    std::fflush(stdout);
                    std::fflush(stderr);
                }

                // NOTE: We don't call write_html here anymore - the C runtime's
                // HTML generator only shows called functions, not library coverage.
                // The proper HTML report is generated after all suites complete
                // using write_library_coverage_html().
            }

            // Clean up suite DLL
            lib.unload();
            try {
                fs::remove(suite.dll_path);
#ifdef _WIN32
                fs::path lib_file = suite.dll_path;
                lib_file.replace_extension(".lib");
                if (fs::exists(lib_file)) {
                    fs::remove(lib_file);
                }
#endif
            } catch (...) {
                // Ignore cleanup errors
            }
        }

        // Collect test run statistics from the results
        TestRunStats test_stats;
        test_stats.total_files = static_cast<int>(test_files.size());

        // Aggregate stats by suite
        std::map<std::string, SuiteStats> suite_map;
        for (const auto& result : collector.results) {
            auto& ss = suite_map[result.group];
            ss.name = result.group;
            ss.test_count += result.test_count;
            ss.duration_ms += result.duration_ms;
            test_stats.total_tests += result.test_count;
            test_stats.total_duration_ms += result.duration_ms;
        }

        // Convert map to vector
        for (auto& [name, ss] : suite_map) {
            test_stats.suites.push_back(std::move(ss));
        }

        // Sort suites by test count (descending)
        std::sort(
            test_stats.suites.begin(), test_stats.suites.end(),
            [](const SuiteStats& a, const SuiteStats& b) { return a.test_count > b.test_count; });

        // Print library coverage analysis after all suites complete
        // Skip coverage report if a filter is active (incomplete coverage data)
        if (CompilerOptions::coverage && opts.patterns.empty()) {
            // Generate report even if no functions were tracked (shows 0% coverage)
            print_library_coverage_report(all_covered_functions, c, test_stats);

            // Write HTML report with proper library coverage data
            if (!CompilerOptions::coverage_output.empty()) {
                write_library_coverage_html(all_covered_functions, CompilerOptions::coverage_output,
                                            test_stats);
            }
        } else if (CompilerOptions::coverage && !opts.patterns.empty()) {
            std::cout << c.dim() << " [Coverage report skipped - filter active]" << c.reset()
                      << "\n";
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n"
                  << c.red() << c.bold()
                  << "[FATAL] Exception in run_tests_suite_mode: " << e.what() << c.reset() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n"
                  << c.red() << c.bold() << "[FATAL] Unknown exception in run_tests_suite_mode"
                  << c.reset() << "\n";
        return 1;
    }
}

} // namespace tml::cli::tester
