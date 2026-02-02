//! # Test Cache Manager
//!
//! This module provides caching for test results based on file hashes.
//! When a test file hasn't changed, we can skip compilation and testing.
//!
//! ## Cache Structure
//!
//! The cache is stored as `.test-cache.json` with the following format:
//! ```json
//! {
//!   "version": 1,
//!   "tests": {
//!     "lib/core/tests/alloc.test.tml": {
//!       "sha512": "abc123...",
//!       "suite": "lib_core_tests",
//!       "last_updated": "2026-02-01T12:34:56Z",
//!       "test_functions": ["test_alloc_new", "test_alloc_free"],
//!       "last_result": "pass",
//!       "duration_ms": 123,
//!       "dependency_hashes": {
//!         "lib/core/src/alloc.tml": "def456..."
//!       }
//!     }
//!   }
//! }
//! ```
//!
//! ## Usage
//!
//! ```cpp
//! TestCacheManager cache;
//! cache.load(".test-cache.json");
//!
//! if (cache.is_valid(test_file)) {
//!     // Skip test, it hasn't changed
//!     auto result = cache.get_cached_result(test_file);
//! } else {
//!     // Run test normally
//!     auto result = run_test(test_file);
//!     cache.update(test_file, result);
//! }
//!
//! cache.save(".test-cache.json");
//! ```

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tml::cli {

// Result status for a cached test
enum class CachedTestStatus {
    Pass,
    Fail,
    Error,    // Compilation error
    Timeout,  // Test timed out
    Unknown   // Not yet run
};

// Information about a single cached test
struct CachedTestInfo {
    std::string file_path;             // Relative path from project root
    std::string sha512;                // SHA512 hash of the file content
    std::string suite;                 // Suite this test belongs to
    std::string last_updated;          // ISO 8601 timestamp
    std::vector<std::string> test_functions;  // @test functions in this file
    CachedTestStatus last_result = CachedTestStatus::Unknown;
    int64_t duration_ms = 0;           // Last run duration
    std::map<std::string, std::string> dependency_hashes;  // Hashes of dependencies
    bool coverage_enabled = false;     // Whether coverage was enabled
    bool profile_enabled = false;      // Whether profiling was enabled
};

// Cache validation result
struct CacheValidationResult {
    bool valid = false;
    std::string reason;  // Why the cache is invalid (if applicable)
};

// Test cache manager
class TestCacheManager {
public:
    TestCacheManager() = default;
    ~TestCacheManager() = default;

    // Load cache from file
    // Returns true if cache was loaded successfully
    bool load(const std::string& cache_file);

    // Save cache to file
    // Returns true if cache was saved successfully
    bool save(const std::string& cache_file) const;

    // Check if a test's cache is valid
    // Returns validation result with reason if invalid
    CacheValidationResult validate(const std::string& test_file) const;

    // Check if a test can be skipped (valid cache + passed last time)
    bool can_skip(const std::string& test_file) const;

    // Get cached result for a test
    std::optional<CachedTestInfo> get_cached_info(const std::string& test_file) const;

    // Update cache for a test
    void update(const std::string& test_file,
                const std::string& sha512,
                const std::string& suite,
                const std::vector<std::string>& test_functions,
                CachedTestStatus result,
                int64_t duration_ms,
                const std::map<std::string, std::string>& dependency_hashes = {},
                bool coverage_enabled = false,
                bool profile_enabled = false);

    // Remove a test from cache
    void remove(const std::string& test_file);

    // Clear all cache entries
    void clear();

    // Get all cached tests
    const std::map<std::string, CachedTestInfo>& get_all() const { return tests_; }

    // Get cache statistics
    struct CacheStats {
        size_t total_entries = 0;
        size_t valid_entries = 0;
        size_t passed_entries = 0;
        size_t failed_entries = 0;
    };
    CacheStats get_stats() const;

    // Compute SHA512 hash of a file
    static std::string compute_file_hash(const std::string& file_path);

    // Get current ISO 8601 timestamp
    static std::string current_timestamp();

    // Get cache version
    static int get_version() { return CACHE_VERSION; }

private:
    static constexpr int CACHE_VERSION = 1;

    std::map<std::string, CachedTestInfo> tests_;
    std::string cache_file_;

    // Normalize file path for consistent keys
    static std::string normalize_path(const std::string& path);

    // Convert result status to/from string
    static std::string status_to_string(CachedTestStatus status);
    static CachedTestStatus string_to_status(const std::string& str);
};

} // namespace tml::cli
