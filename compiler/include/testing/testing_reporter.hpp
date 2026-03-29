//! # Test Reporter Interface and Implementations
//!
//! Abstraction for test output formatting, supporting multiple backends:
//! - Terminal (colored vitest-style output)
//! - JSON (NDJSON format for CI/parsing)
//! - JUnit XML (for Jenkins, GitHub Actions)
//!
//! Inspired by Catch2's reporter architecture.

#pragma once

#include "testing_coordinator.hpp"

#include <memory>
#include <string>
#include <vector>

namespace tml::testing {

// ============================================================================
// Reporter Interface
// ============================================================================

/// Base interface for test result reporting.
/// Reporters are called AFTER the full test run completes.
/// Methods are optional (no-op default implementations).
struct ITestReporter {
    virtual ~ITestReporter() = default;

    /// Called when test run starts (before any tests are compiled/executed).
    virtual void on_run_start(const TestConfig& /*config*/) {}

    /// Called when a test suite finishes compilation.
    virtual void on_suite_compile_done(const SuiteRunResult& /*suite*/) {}

    /// Called when a test suite finishes execution.
    virtual void on_suite_exec_done(const SuiteRunResult& /*suite*/) {}

    /// Called when the entire test run completes.
    /// This is the primary callback — reporters typically output here.
    virtual void on_run_end(const TestRunResult& /*result*/) {}

    /// Called when coverage data is available.
    virtual void on_coverage_report(int /*covered*/, int /*total*/,
                                    const std::string& /*report_path*/) {}

    /// Flush any buffered output.
    virtual void flush() {}
};

// ============================================================================
// MultiReporter — Broadcasts to all children
// ============================================================================

/// Aggregates multiple reporters.
/// All method calls are forwarded to each registered reporter in order.
struct MultiReporter : ITestReporter {
    std::vector<std::unique_ptr<ITestReporter>> reporters;

    void add(std::unique_ptr<ITestReporter> reporter) {
        if (reporter) {
            reporters.push_back(std::move(reporter));
        }
    }

    void on_run_start(const TestConfig& config) override {
        for (auto& r : reporters) {
            r->on_run_start(config);
        }
    }

    void on_suite_compile_done(const SuiteRunResult& suite) override {
        for (auto& r : reporters) {
            r->on_suite_compile_done(suite);
        }
    }

    void on_suite_exec_done(const SuiteRunResult& suite) override {
        for (auto& r : reporters) {
            r->on_suite_exec_done(suite);
        }
    }

    void on_run_end(const TestRunResult& result) override {
        for (auto& r : reporters) {
            r->on_run_end(result);
        }
    }

    void on_coverage_report(int covered, int total, const std::string& report_path) override {
        for (auto& r : reporters) {
            r->on_coverage_report(covered, total, report_path);
        }
    }

    void flush() override {
        for (auto& r : reporters) {
            r->flush();
        }
    }
};

// ============================================================================
// TerminalReporter — Colored vitest-style output
// ============================================================================

/// Terminal-based reporter with optional ANSI coloring.
/// Outputs:
/// - Summary table (suites, tests, passed, failed, crashed, compile errors, duration)
/// - Failure details (compile errors, test failures)
/// - Per-suite timing breakdown (when verbose/profile is enabled)
/// - Coverage summary (when coverage is enabled)
struct TerminalReporter : ITestReporter {
    bool use_color = true;
    bool profile_mode = false;

    explicit TerminalReporter(bool use_color = true) : use_color(use_color) {
        enable_ansi_colors();
    }

    void on_run_start(const TestConfig& config) override;
    void on_run_end(const TestRunResult& result) override;
    void on_coverage_report(int covered, int total, const std::string& report_path) override;

private:
    void print_summary_table(const TestRunResult& result) const;
    void print_failure_details(const TestRunResult& result) const;
    void print_profile_summary(const TestRunResult& result) const;
    void print_coverage_summary(int covered, int total, const std::string& report_path) const;
    static void enable_ansi_colors();
};

// ============================================================================
// JsonReporter — NDJSON format for CI/parsing
// ============================================================================

/// JSON reporter writing one JSON object per suite (NDJSON format).
/// Each line is a complete JSON object:
/// ```json
/// {"suite":"core/str","passed":21,"failed":0,"compile_ok":true,"tests":[...]}
/// ```
struct JsonReporter : ITestReporter {
    std::string output_path;

    explicit JsonReporter(const std::string& path) : output_path(path) {}

    void on_run_end(const TestRunResult& result) override;

private:
    static std::string escape_json_string(const std::string& s);
};

// ============================================================================
// JunitXmlReporter — Jenkins/GitHub Actions compatible XML
// ============================================================================

/// JUnit-compatible XML reporter for Jenkins, GitHub Actions, and other CI systems.
/// Generates standard `<testsuites>/<testsuite>/<testcase>` structure.
struct JunitXmlReporter : ITestReporter {
    std::string output_path;

    explicit JunitXmlReporter(const std::string& path) : output_path(path) {}

    void on_run_end(const TestRunResult& result) override;

private:
    static std::string escape_xml_string(const std::string& s);
};

// ============================================================================
// Factory Functions
// ============================================================================

/// Create a terminal reporter with auto-detected color support.
std::unique_ptr<ITestReporter> make_terminal_reporter(bool use_color = true);

/// Create a JSON reporter writing to the specified path.
std::unique_ptr<ITestReporter> make_json_reporter(const std::string& output_path);

/// Create a JUnit XML reporter writing to the specified path.
std::unique_ptr<ITestReporter> make_junit_reporter(const std::string& output_path);

} // namespace tml::testing