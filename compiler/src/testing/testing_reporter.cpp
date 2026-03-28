TML_MODULE("compiler")

//! # Test Reporter Implementation
//!
//! Provides TerminalReporter, JsonReporter, and JunitXmlReporter implementations
//! for flexible test output formatting.

#include "testing/testing_reporter.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace tml::testing {

// ============================================================================
// ANSI Color Support
// ============================================================================

struct Colors {
    bool enabled;
    explicit Colors(bool use_color) : enabled(use_color) {}

    const char* reset() const {
        return enabled ? "\033[0m" : "";
    }
    const char* bold() const {
        return enabled ? "\033[1m" : "";
    }
    const char* dim() const {
        return enabled ? "\033[2m" : "";
    }
    const char* red() const {
        return enabled ? "\033[31m" : "";
    }
    const char* green() const {
        return enabled ? "\033[32m" : "";
    }
    const char* yellow() const {
        return enabled ? "\033[33m" : "";
    }
    const char* cyan() const {
        return enabled ? "\033[36m" : "";
    }
};

static void enable_ansi_colors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

void TerminalReporter::enable_ansi_colors() {
    ::tml::testing::enable_ansi_colors();
}

// ============================================================================
// TerminalReporter Implementation
// ============================================================================

void TerminalReporter::on_run_start(const TestConfig& config) {
    profile_mode = config.verbose;
}

void TerminalReporter::on_run_end(const TestRunResult& result) {
    Colors c(use_color);

    TML_LOG_INFO("test", "");
    TML_LOG_INFO("test", c.bold() << "Test Results" << c.reset());

    print_summary_table(result);

    if (result.failed > 0 || result.crashed > 0 || result.compilation_errors > 0) {
        print_failure_details(result);
    }

    if (profile_mode) {
        print_profile_summary(result);
    }
}

void TerminalReporter::on_coverage_report(int covered, int total, const std::string& report_path) {
    Colors c(use_color);
    print_coverage_summary(covered, total, report_path);
}

void TerminalReporter::print_summary_table(const TestRunResult& result) {
    Colors c(use_color);

    int cached_count = 0;
    for (const auto& s : result.suites) {
        if (s.cached)
            ++cached_count;
    }

    TML_LOG_INFO("test", "  Suites:  " << result.suites.size()
                                       << (cached_count > 0
                                               ? " (" + std::to_string(cached_count) + " cached)"
                                               : ""));
    TML_LOG_INFO("test", "  Tests:   " << result.total_tests);
    TML_LOG_INFO("test", "  " << c.green() << "Passed:  " << result.passed << c.reset());

    if (result.failed > 0) {
        TML_LOG_INFO("test", "  " << c.red() << "Failed:  " << result.failed << c.reset());
    }
    if (result.crashed > 0) {
        TML_LOG_INFO("test", "  " << c.red() << "Crashed: " << result.crashed << c.reset());
    }
    if (result.timed_out > 0) {
        TML_LOG_INFO("test", "  " << c.red() << "Timeout: " << result.timed_out << c.reset());
    }
    if (result.compilation_errors > 0) {
        TML_LOG_INFO("test", "  " << c.yellow() << "Compile errors: " << result.compilation_errors
                                  << c.reset());
    }
    int skipped =
        result.total_tests - result.passed - result.failed - result.crashed - result.timed_out;
    if (skipped > 0) {
        TML_LOG_INFO("test", "  " << c.dim() << "Skipped: " << skipped << c.reset());
    }

    TML_LOG_INFO("test", "  Duration: " << (result.total_duration_us / 1000) << "ms");

    if (result.covered_functions_count > 0) {
        TML_LOG_INFO("test", "  " << c.cyan() << "Covered: " << result.covered_functions_count
                                  << " functions" << c.reset());
    }
}

void TerminalReporter::print_failure_details(const TestRunResult& result) {
    Colors c(use_color);

    TML_LOG_INFO("test", "");
    TML_LOG_INFO("test", c.bold() << "Failure Details" << c.reset());

    for (const auto& suite : result.suites) {
        if (!suite.compile_ok) {
            TML_LOG_ERROR("test", c.red() << "COMPILE ERROR" << c.reset() << " " << suite.name
                                          << ": " << suite.compile_error);
        }
        for (const auto& [file, err] : suite.per_file_compile_errors) {
            TML_LOG_ERROR("test", c.yellow() << "  COMPILE SKIP" << c.reset() << " " << file << ": "
                                             << err);
        }
        for (const auto& test : suite.tests) {
            if (!test.passed) {
                TML_LOG_ERROR("test", c.red()
                                          << "FAIL" << c.reset() << " " << suite.group << "/"
                                          << test.name << " (" << test.file << ")"
                                          << (test.exit_code != 0
                                                  ? " [exit " + std::to_string(test.exit_code) + "]"
                                                  : ""));
                if (!test.error.empty()) {
                    std::istringstream err_stream(test.error);
                    std::string err_line;
                    int line_count = 0;
                    while (std::getline(err_stream, err_line) && line_count < 20) {
                        if (!err_line.empty() && err_line.back() == '\r')
                            err_line.pop_back();
                        if (!err_line.empty()) {
                            TML_LOG_ERROR("test", "    " << err_line);
                            ++line_count;
                        }
                    }
                    if (line_count >= 20) {
                        TML_LOG_ERROR("test", "    " << c.dim() << "... (truncated)" << c.reset());
                    }
                }
            }
        }
    }
}

void TerminalReporter::print_profile_summary(const TestRunResult& result) {
    Colors c(use_color);

    // Collect suite timings
    struct SuiteTiming {
        std::string name;
        int64_t compile_us;
        int64_t exec_us;
    };
    std::vector<SuiteTiming> timings;

    for (const auto& suite : result.suites) {
        timings.push_back({suite.name, suite.compile_time_us, suite.exec_time_us});
    }

    // Sort by slowest (compile + exec) total time
    std::sort(timings.begin(), timings.end(), [](const SuiteTiming& a, const SuiteTiming& b) {
        return (a.compile_us + a.exec_us) > (b.compile_us + b.exec_us);
    });

    if (!timings.empty()) {
        TML_LOG_INFO("test", "");
        TML_LOG_INFO("test", c.bold() << "Suite Timings (slowest first)" << c.reset());
        for (const auto& t : timings) {
            std::ostringstream oss;
            oss << "  " << std::left << std::setw(40) << t.name
                << "  compile=" << std::to_string(t.compile_us / 1000)
                << "ms  exec=" << std::to_string(t.exec_us / 1000) << "ms";
            TML_LOG_INFO("test", oss.str());
        }
    }
}

void TerminalReporter::print_coverage_summary(int covered, int total,
                                              const std::string& report_path) {
    Colors c(use_color);

    if (total > 0) {
        int percentage = (covered * 100) / total;
        TML_LOG_INFO("test", "  " << c.cyan() << "Coverage: " << covered << "/" << total << " ("
                                  << percentage << "%)" << c.reset());
    }
    if (!report_path.empty()) {
        TML_LOG_INFO("test", c.dim() << "  Coverage report: " << c.reset() << report_path);
    }
}

// ============================================================================
// JsonReporter Implementation
// ============================================================================

std::string JsonReporter::escape_json_string(const std::string& s) const {
    std::string result;
    for (char c : s) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                // Escape control characters
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(c));
                result += oss.str();
            } else {
                result += c;
            }
        }
    }
    return result;
}

void JsonReporter::on_run_end(const TestRunResult& result) {
    std::ofstream file(output_path);
    if (!file.is_open()) {
        TML_LOG_ERROR("test", "Failed to open JSON report file: " << output_path);
        return;
    }

    for (const auto& suite : result.suites) {
        std::ostringstream oss;
        oss << "{\"suite\":\"" << escape_json_string(suite.name) << "\""
            << ",\"group\":\"" << escape_json_string(suite.group) << "\""
            << ",\"passed\":" << suite.passed << ",\"failed\":" << suite.failed
            << ",\"crashed\":" << suite.crashed
            << ",\"compile_ok\":" << (suite.compile_ok ? "true" : "false")
            << ",\"cached\":" << (suite.cached ? "true" : "false");

        if (!suite.compile_error.empty()) {
            oss << ",\"compile_error\":\"" << escape_json_string(suite.compile_error) << "\"";
        }

        // Detailed test results
        oss << ",\"tests\":[";
        bool first = true;
        for (const auto& test : suite.tests) {
            if (!first)
                oss << ",";
            oss << "{\"name\":\"" << escape_json_string(test.name) << "\""
                << ",\"passed\":" << (test.passed ? "true" : "false")
                << ",\"exit_code\":" << test.exit_code << ",\"duration_us\":" << test.duration_us;
            if (!test.error.empty()) {
                oss << ",\"error\":\"" << escape_json_string(test.error) << "\"";
            }
            oss << "}";
            first = false;
        }
        oss << "]";

        oss << ",\"compile_time_us\":" << suite.compile_time_us
            << ",\"exec_time_us\":" << suite.exec_time_us << "}";

        file << oss.str() << "\n";
    }

    file.close();
    TML_LOG_INFO("test", "JSON report written to: " << output_path);
}

// ============================================================================
// JunitXmlReporter Implementation
// ============================================================================

std::string JunitXmlReporter::escape_xml_string(const std::string& s) const {
    std::string result;
    for (char c : s) {
        switch (c) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&apos;";
            break;
        default:
            result += c;
        }
    }
    return result;
}

void JunitXmlReporter::on_run_end(const TestRunResult& result) {
    std::ofstream file(output_path);
    if (!file.is_open()) {
        TML_LOG_ERROR("test", "Failed to open JUnit XML report file: " << output_path);
        return;
    }

    // XML header
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

    // Root element
    auto total_duration_sec = result.total_duration_us / 1000000.0;
    file << "<testsuites name=\"TML Test Results\" tests=\"" << result.total_tests
         << "\" failures=\"" << result.failed << "\" errors=\"" << result.crashed << "\" skipped=\""
         << result.skipped << "\" time=\"" << std::fixed << std::setprecision(3)
         << total_duration_sec << "\">\n";

    // Test suites
    for (const auto& suite : result.suites) {
        auto suite_duration_sec = (suite.compile_time_us + suite.exec_time_us) / 1000000.0;

        file << "  <testsuite name=\"" << escape_xml_string(suite.name) << "\""
             << " package=\"" << escape_xml_string(suite.group) << "\""
             << " tests=\"" << suite.test_count << "\""
             << " failures=\"" << suite.failed << "\""
             << " errors=\"" << suite.crashed << "\""
             << " skipped=\"" << (suite.test_count - suite.passed - suite.failed - suite.crashed)
             << "\""
             << " time=\"" << std::fixed << std::setprecision(3) << suite_duration_sec << "\">\n";

        // Compile error
        if (!suite.compile_ok && !suite.compile_error.empty()) {
            file << "    <error message=\"Compilation failed\" type=\"CompilationError\">\n";
            file << "      " << escape_xml_string(suite.compile_error) << "\n";
            file << "    </error>\n";
        }

        // Individual tests
        for (const auto& test : suite.tests) {
            auto test_duration_sec = test.duration_us / 1000000.0;
            file << "    <testcase name=\"" << escape_xml_string(test.name) << "\""
                 << " file=\"" << escape_xml_string(test.file) << "\""
                 << " time=\"" << std::fixed << std::setprecision(3) << test_duration_sec << "\"";

            if (test.passed) {
                file << " />\n";
            } else {
                file << ">\n";
                if (test.exit_code != 0) {
                    file << "      <failure message=\"Test failed with exit code " << test.exit_code
                         << "\">";
                    if (!test.error.empty()) {
                        file << escape_xml_string(test.error);
                    }
                    file << "</failure>\n";
                } else if (!test.error.empty()) {
                    file << "      <failure message=\"Test assertion failed\">"
                         << escape_xml_string(test.error) << "</failure>\n";
                }
                file << "    </testcase>\n";
            }
        }

        file << "  </testsuite>\n";
    }

    file << "</testsuites>\n";
    file.close();

    TML_LOG_INFO("test", "JUnit XML report written to: " << output_path);
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<ITestReporter> make_terminal_reporter(bool use_color) {
    return std::make_unique<TerminalReporter>(use_color);
}

std::unique_ptr<ITestReporter> make_json_reporter(const std::string& output_path) {
    return std::make_unique<JsonReporter>(output_path);
}

std::unique_ptr<ITestReporter> make_junit_reporter(const std::string& output_path) {
    return std::make_unique<JunitXmlReporter>(output_path);
}

} // namespace tml::testing