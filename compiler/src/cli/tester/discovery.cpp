//! # Test File Discovery
//!
//! This file implements test and benchmark file discovery for `tml test`.
//!
//! ## Discovery Rules
//!
//! | File Pattern     | Included By       | Description           |
//! |------------------|-------------------|-----------------------|
//! | `*.test.tml`     | `tml test`        | Unit test files       |
//! | `tests/*.tml`    | `tml test`        | Test directory files  |
//! | `*.bench.tml`    | `tml test --bench`| Benchmark files       |
//!
//! ## Excluded Directories
//!
//! - `errors/`: Expected compilation error tests
//! - `pending/`: Tests for unimplemented features
//!
//! ## Caching
//!
//! Test file discovery is cached for 1 hour in `build/debug/.test-cache`
//! to speed up repeated test runs.

#include "log/log.hpp"
#include "tester_internal.hpp"

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
                        dirname == "gcc" || dirname == "llvm-project" || dirname == ".hg") {
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
                        dirname == "gcc" || dirname == "llvm-project" || dirname == ".hg") {
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
                    // But exclude .bench.tml files (those are for --bench)
                    if (filename.ends_with(".bench.tml")) {
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

} // namespace tml::cli::tester
