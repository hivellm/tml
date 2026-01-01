// Test command - suite-based test execution
// Compiles multiple test files into single DLLs per suite for faster loading

#include "tester_internal.hpp"

namespace tml::cli::tester {

// ============================================================================
// Suite-Based Test Execution
// ============================================================================

int run_tests_suite_mode(const std::vector<std::string>& test_files, const TestOptions& opts,
                         TestResultCollector& collector, const ColorOutput& c) {
    using Clock = std::chrono::high_resolution_clock;

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
            std::cout << c.dim() << " Compiling suite: " << suite.name << " (" << suite.tests.size()
                      << " tests)" << c.reset() << "\n";
        }

        phase_start = Clock::now();
        auto compile_result = compile_test_suite(suite, opts.verbose, opts.no_cache);
        if (opts.profile) {
            collector.profile_stats.add(
                "suite_compile",
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start)
                    .count());
        }

        if (!compile_result.success) {
            // Report compilation error
            TestResult error_result;
            error_result.file_path = compile_result.failed_test;
            error_result.test_name = fs::path(compile_result.failed_test).stem().string();
            error_result.group = suite.group;
            error_result.passed = false;
            error_result.compilation_error = true;
            error_result.exit_code = EXIT_COMPILATION_ERROR;
            error_result.error_message = "COMPILATION FAILED\n" + compile_result.error_message;

            collector.add(std::move(error_result));
            return 1; // Stop on compilation error
        }

        suite.dll_path = compile_result.dll_path;

        // Load the suite DLL
        phase_start = Clock::now();
        DynamicLibrary lib;
        bool load_ok = lib.load(suite.dll_path);
        if (opts.profile) {
            collector.profile_stats.add(
                "suite_load",
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start)
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
            return 1;
        }

        loaded_suites.push_back({std::move(suite), std::move(lib)});
    }

    // Run all tests from loaded suites
    for (auto& [suite, lib] : loaded_suites) {
        for (size_t i = 0; i < suite.tests.size(); ++i) {
            const auto& test_info = suite.tests[i];

            TestResult result;
            result.file_path = test_info.file_path;
            result.test_name = test_info.test_name;
            result.group = suite.group;
            result.test_count = test_info.test_count;

            phase_start = Clock::now();
            auto run_result = run_suite_test(lib, static_cast<int>(i));
            auto run_duration_us =
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start)
                    .count();

            if (opts.profile) {
                collector.profile_stats.add("test_run", run_duration_us);
                collector.profile_stats.total_tests++;
            }

            result.duration_ms = run_duration_us / 1000;
            result.passed = run_result.success;
            result.exit_code = run_result.exit_code;

            if (!result.passed) {
                result.error_message = "Exit code: " + std::to_string(result.exit_code);
                if (!run_result.error.empty()) {
                    result.error_message += "\n" + run_result.error;
                }
                if (!run_result.output.empty()) {
                    result.error_message += "\n" + run_result.output;
                }
            }

            collector.add(std::move(result));
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

    return 0;
}

} // namespace tml::cli::tester
