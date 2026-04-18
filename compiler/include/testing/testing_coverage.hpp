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

/// Write the coverage summary JSON and append to coverage_history.jsonl.
///
/// The HTML output that the deleted `write_coverage_html` used to
/// produce is now emitted by `tml coverage --format=html` (see the
/// `lib/coverage/` pure-TML reporter). This function writes only the
/// JSON summary (which regression detection depends on) and the
/// history log.
///
/// The JSON path is derived from `output_path` by replacing its
/// extension with `.json`, so callers still holding the historical
/// `coverage.html` path string get the same behaviour.
void write_coverage_json_summary(const std::set<std::string>& covered_functions,
                                 const std::string& output_path,
                                 const CoverageStats& stats = {});

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

// ============================================================================
// Coverage Cache — per-module caching of coverage data
// ============================================================================

/// Load cached coverage data for modules whose source files haven't changed.
/// Returns a set of function names that were covered in previous runs and whose
/// source modules are still unchanged (by content hash).
/// Cache file: build/coverage/coverage_cache.json
std::set<std::string> load_coverage_cache();

/// Save coverage data to the cache file after a successful coverage run.
/// Stores per-module: content hash of source files, list of covered functions.
/// Only updates modules that were actually tested in this run.
void save_coverage_cache(const std::set<std::string>& covered_functions);

} // namespace tml::testing
