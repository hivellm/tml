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

#include "tester_internal.hpp"

namespace tml::cli::tester {

// ============================================================================
// Discover Benchmark Files
// ============================================================================

/// Discovers benchmark files (`*.bench.tml`) in the project.
std::vector<std::string> discover_bench_files(const std::string& root_dir) {
    std::vector<std::string> bench_files;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                auto filename = path.filename().string();
                std::string path_str = path.string();

                // Skip files in errors/ or pending/ directories
                if (path_str.find("\\errors\\") != std::string::npos ||
                    path_str.find("/errors/") != std::string::npos ||
                    path_str.find("\\pending\\") != std::string::npos ||
                    path_str.find("/pending/") != std::string::npos) {
                    continue;
                }

                // Include .bench.tml files
                if (filename.ends_with(".bench.tml")) {
                    bench_files.push_back(path_str);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error discovering benchmark files: " << e.what() << "\n";
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
        for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                auto filename = path.filename().string();
                std::string path_str = path.string();

                // Skip files in errors/ or pending/ directories
                if (path_str.find("\\errors\\") != std::string::npos ||
                    path_str.find("/errors/") != std::string::npos ||
                    path_str.find("\\pending\\") != std::string::npos ||
                    path_str.find("/pending/") != std::string::npos) {
                    continue;
                }

                // Include .test.tml files or .tml files in tests/ directory
                // But exclude .bench.tml files (those are for --bench)
                if (filename.ends_with(".bench.tml")) {
                    continue;
                }
                if (filename.ends_with(".test.tml") ||
                    (path.extension() == ".tml" &&
                     (path_str.find("\\tests\\") != std::string::npos ||
                      path_str.find("/tests/") != std::string::npos))) {
                    test_files.push_back(path_str);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error discovering test files: " << e.what() << "\n";
    }

    // Remove duplicates and sort
    std::sort(test_files.begin(), test_files.end());
    test_files.erase(std::unique(test_files.begin(), test_files.end()), test_files.end());

    return test_files;
}

} // namespace tml::cli::tester
