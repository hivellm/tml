TML_MODULE("test")

//! # Test File Discovery
//!
//! This file implements test and benchmark file discovery for `tml test`.
//!
//! ## Discovery Rules
//!
//! | File Pattern     | Included By       | Description                    |
//! |------------------|-------------------|--------------------------------|
//! | `*.test.tml`     | `tml test`        | Unit test files                |
//! | `tests/*.tml`    | `tml test`        | Test directory files           |
//! | `*.error.tml`    | `tml test`        | Diagnostic tests (expect errors)|
//! | `*.bench.tml`    | `tml test --bench`| Benchmark files                |
//!
//! ## Excluded Directories
//!
//! - `pending/`: Tests for unimplemented features
//! - `.sandbox/`: Scratch space for temporary experiments (never included in discovery)
//!
//! ## Caching
//!
//! Test file discovery is cached for 1 hour in `build/debug/.test-cache`
//! to speed up repeated test runs.

#include "log/log.hpp"
#include "tester_internal.hpp"

#include <fstream>
#include <regex>

namespace tml::cli::tester {

// ============================================================================
// Discover Benchmark Files
// ============================================================================

/// Discovers benchmark files (`*.bench.tml`) in the project.
std::vector<std::string> discover_bench_files(const std::string& root_dir) {
    std::vector<std::string> bench_files;

    try {
        auto options = fs::directory_options::skip_permission_denied;
        for (auto it = fs::recursive_directory_iterator(root_dir, options);
             it != fs::recursive_directory_iterator();) {
            try {
                const auto& entry = *it;
                if (entry.is_directory()) {
                    std::string dirname;
                    try {
                        dirname = entry.path().filename().string();
                    } catch (...) {
                        it.disable_recursion_pending();
                        ++it;
                        continue;
                    }
                    if (dirname == ".git" || dirname == "node_modules" || dirname == "build" ||
                        dirname == "gcc" || dirname == "llvm-project" || dirname == ".hg" ||
                        dirname == ".sandbox") {
                        it.disable_recursion_pending();
                        ++it;
                        continue;
                    }
                }
                if (entry.is_regular_file()) {
                    auto path = entry.path();
                    auto filename = path.filename().string();
                    std::string path_str = path.string();

                    // Skip files in errors/ or pending/ directories
                    if (path_str.find("\\errors\\") != std::string::npos ||
                        path_str.find("/errors/") != std::string::npos ||
                        path_str.find("\\pending\\") != std::string::npos ||
                        path_str.find("/pending/") != std::string::npos) {
                        ++it;
                        continue;
                    }

                    // Include .bench.tml files
                    if (filename.ends_with(".bench.tml")) {
                        bench_files.push_back(path_str);
                    }
                }
                ++it;
            } catch (const std::exception&) {
                ++it;
            }
        }
    } catch (const std::exception& e) {
        TML_LOG_ERROR("test", "Error discovering benchmark files: " << e.what());
    }

    // Sort by name
    std::sort(bench_files.begin(), bench_files.end());
    return bench_files;
}

// ============================================================================
// Discover Test Files
// ============================================================================

std::vector<std::string> discover_test_files(const std::string& root_dir) {
    // Always scan filesystem to detect new files
    // The file list cache caused issues with incremental testing (new files not detected)
    std::vector<std::string> test_files;

    try {
        auto options = fs::directory_options::skip_permission_denied;
        for (auto it = fs::recursive_directory_iterator(root_dir, options);
             it != fs::recursive_directory_iterator();) {
            try {
                const auto& entry = *it;
                auto path = entry.path();

                // Skip known irrelevant directories to avoid Unicode conversion errors
                // in submodules like src/llvm-project or src/gcc
                if (entry.is_directory()) {
                    std::string dirname;
                    try {
                        dirname = path.filename().string();
                    } catch (...) {
                        it.disable_recursion_pending();
                        ++it;
                        continue;
                    }
                    if (dirname == ".git" || dirname == "node_modules" || dirname == "build" ||
                        dirname == "gcc" || dirname == "llvm-project" || dirname == ".hg" ||
                        dirname == ".sandbox") {
                        it.disable_recursion_pending();
                        ++it;
                        continue;
                    }
                }

                if (entry.is_regular_file()) {
                    auto filename = path.filename().string();
                    std::string path_str = path.string();

                    // Skip files in errors/ or pending/ directories
                    if (path_str.find("\\errors\\") != std::string::npos ||
                        path_str.find("/errors/") != std::string::npos ||
                        path_str.find("\\pending\\") != std::string::npos ||
                        path_str.find("/pending/") != std::string::npos) {
                        ++it;
                        continue;
                    }

                    // Include .test.tml files or .tml files in tests/ directory
                    // But exclude .bench.tml and .error.tml files (separate modes)
                    if (filename.ends_with(".bench.tml") || filename.ends_with(".error.tml")) {
                        ++it;
                        continue;
                    }
                    if (filename.ends_with(".test.tml") ||
                        (path.extension() == ".tml" &&
                         (path_str.find("\\tests\\") != std::string::npos ||
                          path_str.find("/tests/") != std::string::npos))) {
                        test_files.push_back(path_str);
                    }
                }
                ++it;
            } catch (const std::exception&) {
                // Skip entries with Unicode conversion errors or other issues
                ++it;
            }
        }
    } catch (const std::exception& e) {
        TML_LOG_ERROR("test", "Error discovering test files: " << e.what());
    }

    // Remove duplicates and sort
    std::sort(test_files.begin(), test_files.end());
    test_files.erase(std::unique(test_files.begin(), test_files.end()), test_files.end());

    return test_files;
}

// ============================================================================
// Discover Diagnostic Test Files
// ============================================================================

std::vector<std::string> discover_diagnostic_files(const std::string& root_dir) {
    std::vector<std::string> diag_files;

    try {
        auto options = fs::directory_options::skip_permission_denied;
        for (auto it = fs::recursive_directory_iterator(root_dir, options);
             it != fs::recursive_directory_iterator();) {
            try {
                const auto& entry = *it;
                auto path = entry.path();

                if (entry.is_directory()) {
                    std::string dirname;
                    try {
                        dirname = path.filename().string();
                    } catch (...) {
                        it.disable_recursion_pending();
                        ++it;
                        continue;
                    }
                    if (dirname == ".git" || dirname == "node_modules" || dirname == "build" ||
                        dirname == "gcc" || dirname == "llvm-project" || dirname == ".hg" ||
                        dirname == ".sandbox") {
                        it.disable_recursion_pending();
                        ++it;
                        continue;
                    }
                }

                if (entry.is_regular_file()) {
                    auto filename = path.filename().string();
                    std::string path_str = path.string();

                    // Skip files in pending/ directories
                    if (path_str.find("\\pending\\") != std::string::npos ||
                        path_str.find("/pending/") != std::string::npos) {
                        ++it;
                        continue;
                    }

                    // Include .error.tml files
                    if (filename.ends_with(".error.tml")) {
                        diag_files.push_back(path_str);
                    }
                }
                ++it;
            } catch (const std::exception&) {
                ++it;
            }
        }
    } catch (const std::exception& e) {
        TML_LOG_ERROR("test", "Error discovering diagnostic files: " << e.what());
    }

    std::sort(diag_files.begin(), diag_files.end());
    return diag_files;
}

// ============================================================================
// Parse Diagnostic Expectations
// ============================================================================

std::vector<DiagnosticExpectation> parse_diagnostic_expectations(const std::string& file_path) {
    std::vector<DiagnosticExpectation> expectations;

    std::ifstream file(file_path);
    if (!file.is_open()) {
        return expectations;
    }

    std::string line;
    int line_num = 0;
    // Regex: // @expect-error CODE [optional message]
    // CODE is 1 letter + 3 digits (e.g., T001, B005, P003)
    std::regex directive_pattern(R"(//\s*@expect-error\s+([A-Z]\d{3})\s*(.*))");

    while (std::getline(file, line)) {
        ++line_num;
        std::smatch match;
        if (std::regex_search(line, match, directive_pattern)) {
            DiagnosticExpectation exp;
            exp.error_code = match[1].str();
            exp.message_pattern = match[2].str();
            // Trim trailing whitespace from message pattern
            while (!exp.message_pattern.empty() &&
                   (exp.message_pattern.back() == ' ' || exp.message_pattern.back() == '\t' ||
                    exp.message_pattern.back() == '\r')) {
                exp.message_pattern.pop_back();
            }
            exp.line_number = line_num;
            expectations.push_back(std::move(exp));
        }
    }

    return expectations;
}

} // namespace tml::cli::tester
