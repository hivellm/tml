//! # Independent Coverage Reports
//!
//! Scans library source files, extracts function definitions,
//! compares against runtime coverage data, and generates reports.
//! Zero dependency on cli/tester/. Part of the v3 independent test system.

#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace tml::testing {

/// Per-suite summary for coverage reports.
struct SuiteInfo {
    std::string name;
    int test_count = 0;
    int64_t duration_ms = 0;
};

/// Statistics for coverage report context.
struct CoverageStats {
    int total_tests = 0;
    int total_files = 0;
    int failed_count = 0;
    int compilation_error_count = 0;
    int64_t total_duration_ms = 0;
    bool no_cache = false;
    std::vector<SuiteInfo> suites;
};

/// Print library coverage analysis to console.
/// Scans lib/core, lib/std, lib/test for .tml files, extracts function defs,
/// compares against covered_functions from runtime instrumentation.
void print_coverage_report(const std::set<std::string>& covered_functions, bool use_color,
                           const CoverageStats& stats = {});

/// Write coverage report as HTML + JSON files.
/// JSON is written alongside the HTML with .json extension.
void write_coverage_html(const std::set<std::string>& covered_functions,
                         const std::string& output_path, const CoverageStats& stats = {});

/// Previous coverage data read from existing JSON report.
struct PreviousCoverage {
    int covered = 0;
    int total = 0;
    double percent = 0.0;
    bool valid = false;
};

/// Read previous coverage stats from the JSON file alongside the HTML report.
/// Used for regression detection — prevents overwriting a higher-coverage report.
PreviousCoverage get_previous_coverage_from_json(const std::string& html_path);

} // namespace tml::testing
