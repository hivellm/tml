//! # Test Output Formatting
//!
//! This file implements test result formatting in Vitest/Jest style.
//!
//! ## Output Format
//!
//! ```text
//!  + compiler_tests (542 tests) 201ms
//!  + lib/core (272 tests) 0ms
//!  x lib/broken (1 test) 5ms
//!    └─ broken.test.tml: assertion failed
//!
//!  Tests       906 passed (906 tests, 102 files)
//!  Duration    292ms
//!
//!  All tests passed!
//! ```
//!
//! ## Profile Stats Output
//!
//! When `--profile` is used, shows timing breakdown by phase:
//! - Lexing, Parsing, Type Checking, Borrow Checking
//! - MIR Generation, LLVM Codegen, Object Compilation

#include "log/log.hpp"
#include "tester_internal.hpp"

#include <sstream>

namespace tml::cli::tester {

// ============================================================================
// Print Results in Vitest Style
// ============================================================================

/// Prints test results in Vitest/Jest style with colored output.
void print_results_vitest_style(const std::vector<TestResult>& results, const TestOptions& opts,
                                int64_t total_duration_ms) {
    ColorOutput c(!opts.no_color);

    // Group results by directory and count individual tests
    std::map<std::string, TestGroup> groups;
    int total_test_count = 0;

    for (const auto& result : results) {
        auto& group = groups[result.group];
        if (group.name.empty()) {
            group.name = result.group;
        }
        group.results.push_back(result);
        group.total_duration_ms += result.duration_ms;
        total_test_count += result.test_count;
        if (result.passed) {
            group.passed += result.test_count;
        } else {
            group.failed += result.test_count;
        }
    }

    // Sort groups by name
    std::vector<std::string> group_names;
    for (const auto& [name, _] : groups) {
        group_names.push_back(name);
    }
    std::sort(group_names.begin(), group_names.end());

    // Print each group
    for (const auto& group_name : group_names) {
        const auto& group = groups[group_name];

        // Group header with icon
        bool all_passed = (group.failed == 0);
        const char* icon = all_passed ? "+" : "x";
        const char* icon_color = all_passed ? c.green() : c.red();

        // Count tests in this group
        int group_test_count = 0;
        for (const auto& r : group.results) {
            group_test_count += r.test_count;
        }

        TML_LOG_INFO("test", icon_color << icon << c.reset() << " " << c.bold() << group.name
                                        << c.reset() << " " << c.gray() << "(" << group_test_count
                                        << " test" << (group_test_count != 1 ? "s" : "") << ")"
                                        << c.reset() << " " << c.dim()
                                        << format_duration(group.total_duration_ms) << c.reset());

        // Print individual tests in group (only if verbose or there are failures)
        if (opts.verbose || group.failed > 0) {
            for (const auto& result : group.results) {
                const char* test_icon = result.passed ? "+" : "x";
                const char* test_color = result.passed ? c.green() : c.red();

                std::ostringstream oss;
                oss << "   " << test_color << test_icon << c.reset() << " " << result.test_name;

                if (!result.passed) {
                    oss << " " << c.red() << "[" << result.error_message << "]" << c.reset();
                }

                if (opts.verbose) {
                    oss << " " << c.dim() << format_duration(result.duration_ms) << c.reset();
                }

                TML_LOG_INFO("test", oss.str());
            }
        }
    }

    // Count totals (files and individual tests)
    int files_passed = 0;
    int files_failed = 0;
    int tests_passed = 0;
    int tests_failed = 0;
    for (const auto& result : results) {
        if (result.passed) {
            files_passed++;
            tests_passed += result.test_count;
        } else {
            files_failed++;
            tests_failed += result.test_count;
        }
    }
    (void)files_passed; // May be used for detailed file-level output later
    (void)files_failed;

    // Print summary box
    {
        std::ostringstream summary;
        summary << c.bold() << "Tests       " << c.reset();
        if (tests_failed > 0) {
            summary << c.red() << c.bold() << tests_failed << " failed" << c.reset() << " | ";
        }
        summary << c.green() << c.bold() << tests_passed << " passed" << c.reset() << " "
                << c.gray() << "(" << total_test_count << " tests, " << results.size() << " file"
                << (results.size() != 1 ? "s" : "") << ")" << c.reset();
        TML_LOG_INFO("test", summary.str());
    }

    TML_LOG_INFO("test",
                 c.bold() << "Duration    " << c.reset() << format_duration(total_duration_ms));

    // Print final status line
    if (tests_failed == 0) {
        TML_LOG_INFO("test", c.green() << c.bold() << "All tests passed!" << c.reset());
    } else {
        TML_LOG_INFO("test", c.red() << c.bold() << "Some tests failed." << c.reset());
    }
}

// ============================================================================
// Print Profile Statistics
// ============================================================================

void print_profile_stats(const ProfileStats& stats, const TestOptions& opts) {
    ColorOutput c(!opts.no_color);

    TML_LOG_INFO("test", c.cyan() << c.bold() << "Phase Profiling" << c.reset() << " " << c.dim()
                                  << "(" << stats.total_tests << " tests)" << c.reset());
    TML_LOG_INFO("test", c.dim() << std::string(60, '-') << c.reset());

    // Calculate total time
    int64_t total_us = 0;
    for (const auto& [_, us] : stats.total_us) {
        total_us += us;
    }

    // Order phases by total time (descending)
    std::vector<std::pair<std::string, int64_t>> phases(stats.total_us.begin(),
                                                        stats.total_us.end());
    std::sort(phases.begin(), phases.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Print each phase
    for (const auto& [phase, us] : phases) {
        double pct = total_us > 0 ? (100.0 * us / total_us) : 0.0;
        int64_t avg_us = stats.count.at(phase) > 0 ? us / stats.count.at(phase) : 0;
        int64_t max_us = stats.max_us.at(phase);

        // Color code by percentage
        const char* pct_color = c.gray();
        if (pct > 30.0)
            pct_color = c.red();
        else if (pct > 15.0)
            pct_color = c.yellow();
        else if (pct > 5.0)
            pct_color = c.green();

        // Format phase name (pad to 15 chars)
        std::string phase_name = phase;
        if (phase_name.size() < 15)
            phase_name += std::string(15 - phase_name.size(), ' ');

        // Format times
        auto format_us = [](int64_t us) -> std::string {
            if (us < 1000)
                return std::to_string(us) + " us";
            else if (us < 1000000)
                return std::to_string(us / 1000) + " ms";
            else
                return std::to_string(us / 1000000) + "." + std::to_string((us / 100000) % 10) +
                       " s";
        };

        std::ostringstream phase_line;
        phase_line << c.bold() << phase_name << c.reset() << "  " << pct_color << std::fixed
                   << std::setprecision(1) << std::setw(5) << pct << "%" << c.reset() << "  "
                   << c.dim() << "total: " << c.reset() << std::setw(8) << format_us(us) << "  "
                   << c.dim() << "avg: " << c.reset() << std::setw(8) << format_us(avg_us) << "  "
                   << c.dim() << "max: " << c.reset() << std::setw(8) << format_us(max_us);
        TML_LOG_INFO("test", phase_line.str());
    }

    TML_LOG_INFO("test", c.dim() << std::string(60, '-') << c.reset());
    TML_LOG_INFO("test", c.bold() << "Total          " << c.reset() << "         "
                                  << (total_us < 1000000
                                          ? std::to_string(total_us / 1000) + " ms"
                                          : std::to_string(total_us / 1000000) + "." +
                                                std::to_string((total_us / 100000) % 10) + " s"));

    // Recommendations
    if (!phases.empty()) {
        const auto& [slowest, slowest_us] = phases[0];
        double slowest_pct = total_us > 0 ? (100.0 * slowest_us / total_us) : 0.0;

        if (slowest_pct > 30.0) {
            TML_LOG_INFO("test", c.yellow() << "Bottleneck: " << c.reset() << c.bold() << slowest
                                            << c.reset() << " is using " << std::fixed
                                            << std::setprecision(1) << slowest_pct << "% of total time");

            // Give specific recommendations based on phase
            if (slowest == "clang_compile") {
                TML_LOG_INFO("test",
                             c.dim() << "  -> Consider: Enable build cache, use -O0 for tests" << c.reset());
            } else if (slowest == "link") {
                TML_LOG_INFO("test",
                             c.dim() << "  -> Consider: Enable LTO cache, fewer deps" << c.reset());
            } else if (slowest == "type_check") {
                TML_LOG_INFO("test",
                             c.dim() << "  -> Consider: Smaller test files, less imports" << c.reset());
            } else if (slowest == "codegen") {
                TML_LOG_INFO("test",
                             c.dim() << "  -> Consider: Simpler code, fewer generics" << c.reset());
            }
        }
    }
}

} // namespace tml::cli::tester
