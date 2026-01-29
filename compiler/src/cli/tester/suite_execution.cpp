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
        }

        // Compile all suites
        std::vector<std::pair<TestSuite, DynamicLibrary>> loaded_suites;

        for (auto& suite : suites) {
            if (!opts.quiet && opts.verbose) {
                std::cout << c.dim() << " Compiling suite: " << suite.name << " ("
                          << suite.tests.size() << " tests)" << c.reset() << "\n";
            }
            std::cout << std::flush;
            std::cerr << std::flush;

            phase_start = Clock::now();
            if (opts.verbose) {
                std::cerr << "[DEBUG] Starting compile_test_suite for: " << suite.name << " ("
                          << suite.tests.size() << " tests)\n"
                          << std::flush;
            }
            auto compile_result = compile_test_suite(suite, opts.verbose, opts.no_cache);
            if (opts.verbose) {
                std::cerr << "[DEBUG] Finished compile_test_suite for: " << suite.name << "\n"
                          << std::flush;
            }
            if (opts.profile) {
                collector.profile_stats.add("suite_compile",
                                            std::chrono::duration_cast<std::chrono::microseconds>(
                                                Clock::now() - phase_start)
                                                .count());
            }

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

        // Print library coverage analysis after all suites complete
        if (CompilerOptions::coverage && !all_covered_functions.empty()) {
            print_library_coverage_report(all_covered_functions, c);

            // Write HTML report with proper library coverage data
            if (!CompilerOptions::coverage_output.empty()) {
                write_library_coverage_html(all_covered_functions,
                                            CompilerOptions::coverage_output);
            }
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
