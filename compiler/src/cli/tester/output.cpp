TML_MODULE("test")

//! # Test Output Formatting
//!
//! This file implements test result formatting in Go/Rust style.
//!
//! ## Output Format
//!
//! ```text
//! running 363 test files
//!
//! test compiler_tests::borrow_library ... ok (1ms)
//! test compiler_tests::closure_capture ... ok (0ms)
//! test lib/broken::broken ... FAILED
//!
//! failures:
//!     lib/broken::broken.test.tml: assertion failed
//!
//! test result: ok. 3632 passed; 0 failed; 363 files; finished in 0.62s
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
// Print Results in Go/Rust Style
// ============================================================================

/// Prints test results in Go/Rust style with colored output.
void print_results_vitest_style(const std::vector<TestResult>& results, const TestOptions& opts,
                                int64_t total_duration_ms) {
    ColorOutput c(!opts.no_color);

    // Group results by directory
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

    // Collect failures for summary at end
    std::vector<std::pair<std::string, std::string>> failures; // (qualified name, error)

    // Print per-test results (verbose) or per-group summary (non-verbose)
    if (opts.verbose) {
        // Rust-style: "test group::name ... ok"
        for (const auto& group_name : group_names) {
            const auto& group = groups[group_name];
            for (const auto& result : group.results) {
                std::string qualified = group.name + "::" + result.test_name;
                std::ostringstream oss;
                oss << "test " << qualified << " ... ";
                if (result.passed) {
                    oss << c.green() << "ok" << c.reset();
                } else {
                    oss << c.red() << "FAILED" << c.reset();
                    failures.push_back({qualified, result.error_message});
                }
                oss << " " << c.dim() << "(" << format_duration(result.duration_ms) << ")"
                    << c.reset();
                TML_LOG_INFO("test", oss.str());
            }
        }
    } else {
        // Go-style: "ok  group  0.24s" or "FAIL  group  0.05s"
        for (const auto& group_name : group_names) {
            const auto& group = groups[group_name];
            bool all_passed = (group.failed == 0);

            // Collect failures even in non-verbose
            if (!all_passed) {
                for (const auto& result : group.results) {
                    if (!result.passed) {
                        std::string qualified = group.name + "::" + result.test_name;
                        failures.push_back({qualified, result.error_message});
                    }
                }
            }

            int group_test_count = 0;
            for (const auto& r : group.results) {
                group_test_count += r.test_count;
            }

            std::ostringstream oss;
            if (all_passed) {
                oss << c.green() << "ok" << c.reset();
            } else {
                oss << c.red() << "FAIL" << c.reset();
            }
            oss << "  " << c.bold() << group.name << c.reset() << "  " << c.dim()
                << group_test_count << " test" << (group_test_count != 1 ? "s" : "") << "  "
                << format_duration(group.total_duration_ms) << c.reset();
            TML_LOG_INFO("test", oss.str());
        }
    }

    // Print failures section (like Rust)
    if (!failures.empty()) {
        TML_LOG_INFO("test", "");
        TML_LOG_INFO("test", c.red() << c.bold() << "failures:" << c.reset());
        for (const auto& [name, err] : failures) {
            TML_LOG_INFO("test", "    " << name << ": " << err);
        }
        TML_LOG_INFO("test", "");
    }

    // Count totals
    int tests_passed = 0;
    int tests_failed = 0;
    for (const auto& result : results) {
        if (result.passed) {
            tests_passed += result.test_count;
        } else {
            tests_failed += result.test_count;
        }
    }

    // Rust-style summary: "test result: ok. 3632 passed; 0 failed; 363 files; finished in 0.62s"
    {
        std::ostringstream summary;
        summary << c.bold() << "test result: " << c.reset();
        if (tests_failed == 0) {
            summary << c.green() << c.bold() << "ok" << c.reset() << ". ";
        } else {
            summary << c.red() << c.bold() << "FAILED" << c.reset() << ". ";
        }
        summary << tests_passed << " passed; " << tests_failed << " failed; " << results.size()
                << " file" << (results.size() != 1 ? "s" : "") << "; finished in "
                << format_duration(total_duration_ms);
        TML_LOG_INFO("test", summary.str());
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
            TML_LOG_INFO("test", c.yellow()
                                     << "Bottleneck: " << c.reset() << c.bold() << slowest
                                     << c.reset() << " is using " << std::fixed
                                     << std::setprecision(1) << slowest_pct << "% of total time");

            // Give specific recommendations based on phase
            if (slowest == "clang_compile") {
                TML_LOG_INFO("test", c.dim()
                                         << "  -> Consider: Enable build cache, use -O0 for tests"
                                         << c.reset());
            } else if (slowest == "link") {
                TML_LOG_INFO("test",
                             c.dim() << "  -> Consider: Enable LTO cache, fewer deps" << c.reset());
            } else if (slowest == "type_check") {
                TML_LOG_INFO("test", c.dim() << "  -> Consider: Smaller test files, less imports"
                                             << c.reset());
            } else if (slowest == "codegen") {
                TML_LOG_INFO("test",
                             c.dim() << "  -> Consider: Simpler code, fewer generics" << c.reset());
            }
        }
    }
}

// ============================================================================
// Print Leak Statistics
// ============================================================================

void print_leak_stats(const LeakStats& stats, const TestOptions& opts) {
    ColorOutput c(!opts.no_color);

    if (stats.total_leaks == 0) {
        TML_LOG_INFO("test", c.green() << c.bold() << "Memory Leaks" << c.reset() << " " << c.dim()
                                       << "none detected" << c.reset());
        return;
    }

    TML_LOG_INFO("test", c.red() << c.bold() << "Memory Leaks" << c.reset() << " " << c.dim() << "("
                                 << stats.total_leaks << " leak"
                                 << (stats.total_leaks != 1 ? "s" : "") << ", " << stats.total_bytes
                                 << " bytes)" << c.reset());
    TML_LOG_INFO("test", c.dim() << std::string(72, '-') << c.reset());

    // Sort files by leak bytes (descending)
    auto sorted = stats.files;
    std::sort(sorted.begin(), sorted.end(), [](const LeakFileInfo& a, const LeakFileInfo& b) {
        return a.leak_bytes > b.leak_bytes;
    });

    // Format helper for bytes
    auto format_bytes = [](int64_t b) -> std::string {
        if (b < 1024)
            return std::to_string(b) + " B";
        else if (b < 1024 * 1024)
            return std::to_string(b / 1024) + "." + std::to_string((b % 1024) * 10 / 1024) + " KB";
        else
            return std::to_string(b / (1024 * 1024)) + "." +
                   std::to_string((b % (1024 * 1024)) * 10 / (1024 * 1024)) + " MB";
    };

    for (const auto& file : sorted) {
        // Shorten file path for display (show last 2-3 components)
        std::string display_path = file.file_path;
        size_t sep = display_path.rfind('/');
        if (sep == std::string::npos)
            sep = display_path.rfind('\\');
        if (sep != std::string::npos) {
            size_t sep2 = display_path.rfind('/', sep - 1);
            if (sep2 == std::string::npos)
                sep2 = display_path.rfind('\\', sep - 1);
            if (sep2 != std::string::npos) {
                size_t sep3 = display_path.rfind('/', sep2 - 1);
                if (sep3 == std::string::npos)
                    sep3 = display_path.rfind('\\', sep2 - 1);
                if (sep3 != std::string::npos)
                    display_path = display_path.substr(sep3 + 1);
            }
        }

        // Pad file path
        if (display_path.size() < 45)
            display_path += std::string(45 - display_path.size(), ' ');

        std::ostringstream line;
        line << c.red() << display_path << c.reset() << "  " << c.bold() << std::setw(3)
             << file.leak_count << c.reset() << " leak" << (file.leak_count != 1 ? "s" : " ")
             << "  " << c.dim() << std::setw(8) << format_bytes(file.leak_bytes) << c.reset();
        TML_LOG_INFO("test", line.str());
    }

    TML_LOG_INFO("test", c.dim() << std::string(72, '-') << c.reset());

    std::ostringstream total_line;
    total_line << c.bold() << c.red() << "Total" << c.reset() << std::string(40, ' ') << "  "
               << c.bold() << std::setw(3) << stats.total_leaks << c.reset() << " leak"
               << (stats.total_leaks != 1 ? "s" : " ") << "  " << c.dim() << std::setw(8)
               << format_bytes(stats.total_bytes) << c.reset();
    TML_LOG_INFO("test", total_line.str());
}

} // namespace tml::cli::tester
