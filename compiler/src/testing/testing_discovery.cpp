TML_MODULE("test")

//! # Independent Test Discovery
//!
//! Reimplements test file discovery and suite grouping from scratch,
//! with zero dependency on the old test system. Part of the v3 independent test system.

#include "testing/testing_discovery.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

namespace tml::testing {

// ============================================================================
// Helpers
// ============================================================================

/// Extract a module group from a file path.
///
/// Examples:
///   lib/core/tests/str/split.test.tml  → "core/str"
///   lib/std/tests/json/parse.test.tml  → "std/json"
///   compiler/tests/compiler/foo.tml    → "compiler/compiler"
///   compiler/tests/runtime/bar.tml     → "compiler/runtime"
static std::string extract_group(const fs::path& file_path) {
    auto norm = file_path.lexically_normal();
    std::vector<std::string> parts;
    for (const auto& p : norm) {
        parts.push_back(p.string());
    }

    // Find "tests" directory in path
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i] != "tests")
            continue;

        // Pattern: lib/<lib>/tests/<module>/...
        if (i >= 1 && (parts[i - 1] == "core" || parts[i - 1] == "std" || parts[i - 1] == "test")) {
            // Go one more back to check for "lib"
            if (i >= 2 && parts[i - 2] == "lib") {
                std::string lib_name = parts[i - 1]; // core, std, test
                // Module is directory after tests/
                if (i + 1 < parts.size() - 1) { // -1 to exclude filename
                    return lib_name + "/" + parts[i + 1];
                }
                return lib_name;
            }
        }

        // Pattern: compiler/tests/<module>/...
        if (i >= 1 && parts[i - 1] == "compiler") {
            if (i + 1 < parts.size() - 1) {
                return "compiler/" + parts[i + 1];
            }
            return "compiler";
        }
    }

    return "other";
}

/// Extract the test name stem from a file path.
/// "split.test.tml" → "split", "foo.tml" → "foo"
static std::string extract_test_name(const fs::path& file_path) {
    auto filename = file_path.filename().string();

    // Remove .test.tml suffix
    if (filename.size() > 9 && filename.ends_with(".test.tml")) {
        return filename.substr(0, filename.size() - 9);
    }
    // Remove .tml suffix
    if (filename.size() > 4 && filename.ends_with(".tml")) {
        return filename.substr(0, filename.size() - 4);
    }
    return filename;
}

/// Build a suite key from the group (replace / with _).
static std::string group_to_suite_key(const std::string& group) {
    std::string key = group;
    std::replace(key.begin(), key.end(), '/', '_');
    return key;
}

// ============================================================================
// Discovery
// ============================================================================

std::vector<TestFileInfo> discover_tests(const std::string& root_dir) {
    std::vector<TestFileInfo> results;

    try {
        auto options = fs::directory_options::skip_permission_denied;
        for (auto it = fs::recursive_directory_iterator(root_dir, options);
             it != fs::recursive_directory_iterator();) {
            try {
                const auto& entry = *it;
                auto path = entry.path();

                // Skip irrelevant directories
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
                        dirname == ".sandbox" || dirname == "third_party") {
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

                    // Skip bench and error test files
                    if (filename.ends_with(".bench.tml") || filename.ends_with(".error.tml")) {
                        ++it;
                        continue;
                    }

                    // Include .test.tml files or .tml files in tests/ directories
                    bool is_test = filename.ends_with(".test.tml") ||
                                   (filename.ends_with(".tml") &&
                                    (path_str.find("\\tests\\") != std::string::npos ||
                                     path_str.find("/tests/") != std::string::npos));

                    if (is_test) {
                        TestFileInfo info;
                        info.file_path = path_str;
                        info.test_name = extract_test_name(path);
                        info.group = extract_group(path);
                        // Defer counting @test directives — reading every file during
                        // discovery is expensive. Set to 1; actual count happens at compile time.
                        info.test_count = 1;
                        results.push_back(std::move(info));
                    }
                }
                ++it;
            } catch (const std::exception&) {
                ++it;
            }
        }
    } catch (const std::exception& e) {
        TML_LOG_ERROR("test", "Error discovering test files: " << e.what());
    }

    // Sort by path for deterministic ordering
    std::sort(results.begin(), results.end(), [](const TestFileInfo& a, const TestFileInfo& b) {
        return a.file_path < b.file_path;
    });

    // Remove duplicates by file_path
    results.erase(std::unique(results.begin(), results.end(),
                              [](const TestFileInfo& a, const TestFileInfo& b) {
                                  return a.file_path == b.file_path;
                              }),
                  results.end());

    return results;
}

// ============================================================================
// Suite Grouping
// ============================================================================

std::vector<Suite> group_into_suites(const std::vector<TestFileInfo>& tests, size_t max_per_suite) {

    // Group by suite key (derived from group)
    std::map<std::string, std::vector<const TestFileInfo*>> groups;
    for (const auto& t : tests) {
        std::string key = group_to_suite_key(t.group);
        groups[key].push_back(&t);
    }

    std::vector<Suite> suites;
    for (auto& [key, file_ptrs] : groups) {
        // Sort within group for deterministic ordering
        std::sort(file_ptrs.begin(), file_ptrs.end(),
                  [](const TestFileInfo* a, const TestFileInfo* b) {
                      return a->file_path < b->file_path;
                  });

        // phase41a: compiler/compiler tests aggregate like every other group.
        // The old forced-per-file exception (impl blocks on primitive types
        // producing colliding external symbols) is obsolete: test compiles set
        // force_internal_linkage unconditionally (query_core.cpp), and any
        // residual duplicate-symbol link failure triggers the coordinator's
        // per-file fallback for that suite.
        size_t effective_max = max_per_suite;

        // Split into chunks of max_per_suite
        size_t chunk_count = (file_ptrs.size() + effective_max - 1) / effective_max;

        for (size_t chunk = 0; chunk < chunk_count; ++chunk) {
            Suite suite;
            // Derive group from first file in chunk
            size_t start_idx = chunk * effective_max;
            size_t end_idx = std::min(start_idx + effective_max, file_ptrs.size());

            suite.group = file_ptrs[start_idx]->group;

            // Build suite name
            if (effective_max == 1) {
                // Individual mode: use group_testname for uniqueness
                suite.name = key + "_" + file_ptrs[start_idx]->test_name;
            } else if (chunk_count > 1) {
                suite.name = key + "_" + std::to_string(chunk + 1);
            } else {
                suite.name = key;
            }

            for (size_t i = start_idx; i < end_idx; ++i) {
                suite.tests.push_back(*file_ptrs[i]);
            }

            suites.push_back(std::move(suite));
        }
    }

    // Sort suites by name
    std::sort(suites.begin(), suites.end(),
              [](const Suite& a, const Suite& b) { return a.name < b.name; });

    return suites;
}

} // namespace tml::testing
