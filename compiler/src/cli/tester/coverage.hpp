//! # LLVM Coverage Collector
//!
//! This header defines the coverage collection and reporting API.
//!
//! ## Workflow
//!
//! ```text
//! Run tests with -fprofile-instr-generate -fcoverage-mapping
//!              |
//!              v
//! Generates .profraw files (raw profile data)
//!              |
//!              v
//! llvm-profdata merge -> .profdata (merged profile)
//!              |
//!              v
//! llvm-cov report/show -> Coverage reports (HTML, LCOV, console)
//! ```
//!
//! ## Report Types
//!
//! | Type    | Description                              |
//! |---------|------------------------------------------|
//! | Console | Summary table with percentages           |
//! | HTML    | Interactive line-by-line annotations     |
//! | LCOV    | Standard format for CI integration       |

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli::tester {

/**
 * Coverage summary statistics
 */
struct CoverageSummary {
    int total_lines = 0;
    int covered_lines = 0;
    int total_functions = 0;
    int covered_functions = 0;
    int total_branches = 0;
    int covered_branches = 0;

    double line_percent() const {
        return total_lines > 0 ? (100.0 * covered_lines / total_lines) : 0.0;
    }

    double function_percent() const {
        return total_functions > 0 ? (100.0 * covered_functions / total_functions) : 0.0;
    }

    double branch_percent() const {
        return total_branches > 0 ? (100.0 * covered_branches / total_branches) : 0.0;
    }
};

/**
 * Per-file coverage data
 */
struct FileCoverage {
    std::string file_path;
    int total_lines = 0;
    int covered_lines = 0;
    int total_functions = 0;
    int covered_functions = 0;
    int total_branches = 0;
    int covered_branches = 0;

    double line_percent() const {
        return total_lines > 0 ? (100.0 * covered_lines / total_lines) : 0.0;
    }

    double function_percent() const {
        return total_functions > 0 ? (100.0 * covered_functions / total_functions) : 0.0;
    }

    double branch_percent() const {
        return total_branches > 0 ? (100.0 * covered_branches / total_branches) : 0.0;
    }
};

/**
 * Per-function coverage data
 */
struct FunctionCoverage {
    std::string name;       // Function name
    int64_t call_count = 0; // Number of times called
    bool covered() const {
        return call_count > 0;
    }
};

/**
 * Coverage report result
 */
struct CoverageReport {
    bool success = false;
    std::string error_message;
    CoverageSummary summary;
    std::vector<FileCoverage> files;
    std::vector<std::string> uncovered_files;     // Files with 0% coverage
    std::vector<FunctionCoverage> functions;      // Function-level coverage
    std::vector<std::string> uncovered_functions; // Functions with 0 calls
};

/**
 * LLVM Coverage Collector
 *
 * Handles collection of profile data from instrumented test runs
 * and generates coverage reports using llvm-profdata and llvm-cov.
 */
class CoverageCollector {
public:
    CoverageCollector();
    ~CoverageCollector();

    /**
     * Initialize the coverage collector
     * Verifies that llvm-profdata and llvm-cov are available
     *
     * @return true if coverage tools are available
     */
    bool initialize();

    /**
     * Get the last error message
     */
    const std::string& get_last_error() const {
        return error_message_;
    }

    /**
     * Set the output directory for profraw files
     */
    void set_profraw_dir(const fs::path& dir);

    /**
     * Get the profile output file path for a test
     * Sets LLVM_PROFILE_FILE environment variable pattern
     *
     * @param test_name Name of the test (used in filename)
     * @return Path pattern for LLVM_PROFILE_FILE
     */
    std::string get_profile_env(const std::string& test_name);

    /**
     * Collect profraw files after test execution
     * Scans profraw_dir for .profraw files
     */
    void collect_profraw_files();

    /**
     * Merge collected profraw files into a single profdata file
     *
     * @param output_profdata Output path for merged .profdata file
     * @return true on success
     */
    bool merge_profiles(const fs::path& output_profdata);

    /**
     * Generate coverage report
     *
     * @param binary Path to the instrumented binary
     * @param profdata Path to merged .profdata file
     * @param source_dirs Directories containing source files to include
     * @return Coverage report
     */
    CoverageReport generate_report(const fs::path& binary, const fs::path& profdata,
                                   const std::vector<fs::path>& source_dirs);

    /**
     * Generate HTML coverage report
     *
     * @param binary Path to the instrumented binary
     * @param profdata Path to merged .profdata file
     * @param output_dir Output directory for HTML files
     * @param source_dirs Directories containing source files to include
     * @return true on success
     */
    bool generate_html_report(const fs::path& binary, const fs::path& profdata,
                              const fs::path& output_dir, const std::vector<fs::path>& source_dirs);

    /**
     * Generate LCOV format coverage report
     *
     * @param binary Path to the instrumented binary
     * @param profdata Path to merged .profdata file
     * @param output_lcov Output path for .lcov file
     * @param source_dirs Directories containing source files to include
     * @return true on success
     */
    bool generate_lcov_report(const fs::path& binary, const fs::path& profdata,
                              const fs::path& output_lcov,
                              const std::vector<fs::path>& source_dirs);

    /**
     * Generate function-level coverage report from profdata
     * Uses llvm-profdata show --all-functions to extract function call counts
     *
     * @param profdata Path to merged .profdata file
     * @return Coverage report with function-level data
     */
    CoverageReport generate_function_report(const fs::path& profdata);

    /**
     * Print console coverage summary (Vitest-style)
     */
    void print_console_report(const CoverageReport& report);

    /**
     * Print function-level coverage report
     */
    void print_function_report(const CoverageReport& report);

    /**
     * Get paths to llvm tools
     */
    const std::string& get_profdata_path() const {
        return profdata_path_;
    }
    const std::string& get_cov_path() const {
        return cov_path_;
    }

private:
    std::string profdata_path_; // Path to llvm-profdata
    std::string cov_path_;      // Path to llvm-cov
    std::string error_message_;
    fs::path profraw_dir_;
    std::vector<fs::path> profraw_files_;
};

} // namespace tml::cli::tester
