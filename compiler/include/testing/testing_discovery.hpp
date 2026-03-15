//! # Independent Test Discovery
//!
//! Discovers test files and groups them into suites without any dependency
//! on the old cli/tester/ system. Part of the v3 independent test system.
//!
//! ## Discovery Rules
//!
//! - Recursively scans for `*.test.tml` files and `.tml` files in `tests/` dirs
//! - Skips: `pending/`, `errors/`, `.sandbox/`, `build/`, `.git/`, submodules
//! - Groups by directory structure: `lib/core/tests/str/` → group `"core/str"`

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tml::testing {

/// Information about a single discovered test file.
struct TestFileInfo {
    std::string file_path; ///< Full path to the .test.tml file
    std::string test_name; ///< Stem without .test.tml (e.g., "split")
    std::string group;     ///< Module group (e.g., "core/str", "compiler/compiler")
    int test_count = 1;    ///< Number of @test functions in the file
};

/// A suite groups one or more test files for compilation + execution.
/// With max_per_suite=1 (default), each file becomes its own suite.
struct Suite {
    std::string name;                ///< Unique identifier (e.g., "core_str_split")
    std::string group;               ///< Display group (e.g., "core/str")
    std::vector<TestFileInfo> tests; ///< Tests in this suite
};

/// Recursively discover *.test.tml files under root_dir.
/// Also discovers .tml files inside tests/ directories.
/// Skips: pending/, errors/, .sandbox/, build/, .git/, submodules.
std::vector<TestFileInfo> discover_tests(const std::string& root_dir);

/// Group discovered test files into suites by directory structure.
/// @param max_per_suite Tests per suite (default 1; safe to increase now that
///        the struct alias shortcut checks field layout stability)
std::vector<Suite> group_into_suites(const std::vector<TestFileInfo>& tests,
                                     size_t max_per_suite = 1);

} // namespace tml::testing
