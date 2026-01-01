// Test command - output formatting

#include "tester_internal.hpp"

namespace tml::cli::tester {

// ============================================================================
// Print Results in Vitest Style
// ============================================================================

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

    // Print header
    std::cout << "\n";

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

        std::cout << " " << icon_color << icon << c.reset() << " " << c.bold() << group.name
                  << c.reset() << " " << c.gray() << "(" << group_test_count << " test"
                  << (group_test_count != 1 ? "s" : "") << ")" << c.reset() << " " << c.dim()
                  << format_duration(group.total_duration_ms) << c.reset() << "\n";

        // Print individual tests in group (only if verbose or there are failures)
        if (opts.verbose || group.failed > 0) {
            for (const auto& result : group.results) {
                const char* test_icon = result.passed ? "+" : "x";
                const char* test_color = result.passed ? c.green() : c.red();

                std::cout << "   " << test_color << test_icon << c.reset() << " "
                          << result.test_name;

                if (!result.passed) {
                    std::cout << " " << c.red() << "[" << result.error_message << "]" << c.reset();
                }

                if (opts.verbose) {
                    std::cout << " " << c.dim() << format_duration(result.duration_ms) << c.reset();
                }

                std::cout << "\n";
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

    // Print summary box
    std::cout << "\n";
    std::cout << " " << c.bold() << "Tests       " << c.reset();
    if (tests_failed > 0) {
        std::cout << c.red() << c.bold() << tests_failed << " failed" << c.reset() << " | ";
    }
    std::cout << c.green() << c.bold() << tests_passed << " passed" << c.reset() << " " << c.gray()
              << "(" << total_test_count << " tests, " << results.size() << " file"
              << (results.size() != 1 ? "s" : "") << ")" << c.reset() << "\n";

    std::cout << " " << c.bold() << "Duration    " << c.reset()
              << format_duration(total_duration_ms) << "\n";

    // Print final status line
    std::cout << "\n";
    if (tests_failed == 0) {
        std::cout << " " << c.green() << c.bold() << "All tests passed!" << c.reset() << "\n";
    } else {
        std::cout << " " << c.red() << c.bold() << "Some tests failed." << c.reset() << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// Print Profile Statistics
// ============================================================================

void print_profile_stats(const ProfileStats& stats, const TestOptions& opts) {
    ColorOutput c(!opts.no_color);

    std::cout << "\n";
    std::cout << " " << c.cyan() << c.bold() << "Phase Profiling" << c.reset() << " " << c.dim()
              << "(" << stats.total_tests << " tests)" << c.reset() << "\n";
    std::cout << " " << c.dim() << std::string(60, '-') << c.reset() << "\n";

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

        std::cout << " " << c.bold() << phase_name << c.reset() << "  " << pct_color << std::fixed
                  << std::setprecision(1) << std::setw(5) << pct << "%" << c.reset() << "  "
                  << c.dim() << "total: " << c.reset() << std::setw(8) << format_us(us) << "  "
                  << c.dim() << "avg: " << c.reset() << std::setw(8) << format_us(avg_us) << "  "
                  << c.dim() << "max: " << c.reset() << std::setw(8) << format_us(max_us) << "\n";
    }

    std::cout << " " << c.dim() << std::string(60, '-') << c.reset() << "\n";
    std::cout << " " << c.bold() << "Total          " << c.reset() << "         "
              << (total_us < 1000000 ? std::to_string(total_us / 1000) + " ms"
                                     : std::to_string(total_us / 1000000) + "." +
                                           std::to_string((total_us / 100000) % 10) + " s")
              << "\n";
    std::cout << "\n";

    // Recommendations
    if (!phases.empty()) {
        const auto& [slowest, slowest_us] = phases[0];
        double slowest_pct = total_us > 0 ? (100.0 * slowest_us / total_us) : 0.0;

        if (slowest_pct > 30.0) {
            std::cout << " " << c.yellow() << "Bottleneck: " << c.reset() << c.bold() << slowest
                      << c.reset() << " is using " << std::fixed << std::setprecision(1)
                      << slowest_pct << "% of total time\n";

            // Give specific recommendations based on phase
            if (slowest == "clang_compile") {
                std::cout << " " << c.dim()
                          << "  -> Consider: Enable build cache, use -O0 for tests" << c.reset()
                          << "\n";
            } else if (slowest == "link") {
                std::cout << " " << c.dim() << "  -> Consider: Enable LTO cache, fewer deps"
                          << c.reset() << "\n";
            } else if (slowest == "type_check") {
                std::cout << " " << c.dim() << "  -> Consider: Smaller test files, less imports"
                          << c.reset() << "\n";
            } else if (slowest == "codegen") {
                std::cout << " " << c.dim() << "  -> Consider: Simpler code, fewer generics"
                          << c.reset() << "\n";
            }
            std::cout << "\n";
        }
    }
}

} // namespace tml::cli::tester
