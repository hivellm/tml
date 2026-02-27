//! # Test Result Cache — Unit Tests
//!
//! Tests for suite-level test result caching:
//! - Cache entry creation and access
//! - Load/save round-trip (JSON serialization)
//! - is_cached() validation logic
//! - Cache-passing-only rule (failing suites not cached)
//! - Compiler hash and flags hash computation
//! - Source hash computation

#include "testing/testing_coordinator.hpp"
#include "testing/testing_test_cache.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

namespace fs = std::filesystem;
using namespace tml::testing;

// ============================================================================
// SuiteCacheEntry defaults
// ============================================================================

TEST(TestCacheTest, EntryDefaults) {
    SuiteCacheEntry entry;
    EXPECT_TRUE(entry.source_hashes.empty());
    EXPECT_TRUE(entry.flags_hash.empty());
    EXPECT_FALSE(entry.all_passed);
    EXPECT_EQ(entry.test_count, 0);
    EXPECT_EQ(entry.passed_count, 0);
    EXPECT_EQ(entry.failed_count, 0);
    EXPECT_EQ(entry.duration_us, 0);
    EXPECT_TRUE(entry.exe_path.empty());
}

// ============================================================================
// Basic cache operations
// ============================================================================

TEST(TestCacheTest, UpdateAndGet) {
    TestResultCache cache;

    SuiteCacheEntry entry;
    entry.source_hashes = {"hash_a", "hash_b"};
    entry.flags_hash = "flags_123";
    entry.all_passed = true;
    entry.test_count = 5;
    entry.passed_count = 5;

    cache.update("suite_1", entry);

    EXPECT_EQ(cache.size(), 1);

    auto* got = cache.get("suite_1");
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->source_hashes.size(), 2);
    EXPECT_EQ(got->source_hashes[0], "hash_a");
    EXPECT_EQ(got->source_hashes[1], "hash_b");
    EXPECT_EQ(got->flags_hash, "flags_123");
    EXPECT_TRUE(got->all_passed);
    EXPECT_EQ(got->test_count, 5);
    EXPECT_EQ(got->passed_count, 5);
}

TEST(TestCacheTest, GetNotFound) {
    TestResultCache cache;
    EXPECT_EQ(cache.get("nonexistent"), nullptr);
}

TEST(TestCacheTest, Clear) {
    TestResultCache cache;
    cache.set_compiler_hash("abc123");

    SuiteCacheEntry entry;
    entry.all_passed = true;
    cache.update("s1", entry);
    cache.update("s2", entry);

    EXPECT_EQ(cache.size(), 2);
    cache.clear();
    EXPECT_EQ(cache.size(), 0);
    EXPECT_TRUE(cache.compiler_hash().empty());
}

// ============================================================================
// is_cached() logic
// ============================================================================

TEST(TestCacheTest, IsCachedAllMatch) {
    TestResultCache cache;
    cache.set_compiler_hash("compiler_v1");

    SuiteCacheEntry entry;
    entry.source_hashes = {"h1", "h2", "h3"};
    entry.flags_hash = "flags_abc";
    entry.all_passed = true;
    entry.test_count = 3;
    entry.passed_count = 3;
    cache.update("suite_x", entry);

    // Exact match → cached
    EXPECT_TRUE(cache.is_cached("suite_x", {"h1", "h2", "h3"}, "flags_abc"));
}

TEST(TestCacheTest, IsCachedFailedNotCached) {
    TestResultCache cache;

    SuiteCacheEntry entry;
    entry.source_hashes = {"h1"};
    entry.flags_hash = "f1";
    entry.all_passed = false; // Failed!
    entry.test_count = 1;
    entry.failed_count = 1;
    cache.update("suite_fail", entry);

    // Failed suite → NOT cached (Go's rule)
    EXPECT_FALSE(cache.is_cached("suite_fail", {"h1"}, "f1"));
}

TEST(TestCacheTest, IsCachedSourceChanged) {
    TestResultCache cache;

    SuiteCacheEntry entry;
    entry.source_hashes = {"old_hash"};
    entry.flags_hash = "f1";
    entry.all_passed = true;
    entry.test_count = 1;
    entry.passed_count = 1;
    cache.update("s1", entry);

    // Source changed → NOT cached
    EXPECT_FALSE(cache.is_cached("s1", {"new_hash"}, "f1"));
}

TEST(TestCacheTest, IsCachedFlagsChanged) {
    TestResultCache cache;

    SuiteCacheEntry entry;
    entry.source_hashes = {"h1"};
    entry.flags_hash = "old_flags";
    entry.all_passed = true;
    entry.test_count = 1;
    entry.passed_count = 1;
    cache.update("s1", entry);

    // Flags changed → NOT cached
    EXPECT_FALSE(cache.is_cached("s1", {"h1"}, "new_flags"));
}

TEST(TestCacheTest, IsCachedFileCountChanged) {
    TestResultCache cache;

    SuiteCacheEntry entry;
    entry.source_hashes = {"h1", "h2"};
    entry.flags_hash = "f1";
    entry.all_passed = true;
    cache.update("s1", entry);

    // Different number of files → NOT cached
    EXPECT_FALSE(cache.is_cached("s1", {"h1", "h2", "h3"}, "f1"));
    EXPECT_FALSE(cache.is_cached("s1", {"h1"}, "f1"));
}

TEST(TestCacheTest, IsCachedSuiteNotFound) {
    TestResultCache cache;
    EXPECT_FALSE(cache.is_cached("nonexistent", {"h1"}, "f1"));
}

// ============================================================================
// Save/Load round-trip
// ============================================================================

TEST(TestCacheTest, SaveLoadRoundTrip) {
    // Use a temp file
    auto temp_dir = fs::temp_directory_path();
    auto cache_file = temp_dir / "tml_test_cache_test.json";

    // Create cache with data
    {
        TestResultCache cache;
        cache.set_compiler_hash("compiler_deadbeef");

        SuiteCacheEntry e1;
        e1.source_hashes = {"abc123", "def456"};
        e1.flags_hash = "flags_001";
        e1.all_passed = true;
        e1.test_count = 8;
        e1.passed_count = 8;
        e1.failed_count = 0;
        e1.duration_us = 12345;
        e1.exe_path = "/tmp/suite1.exe";
        cache.update("lib_core_tests_1", e1);

        SuiteCacheEntry e2;
        e2.source_hashes = {"xyz789"};
        e2.flags_hash = "flags_001";
        e2.all_passed = false;
        e2.test_count = 3;
        e2.passed_count = 2;
        e2.failed_count = 1;
        e2.duration_us = 5000;
        cache.update("lib_std_tests_1", e2);

        EXPECT_TRUE(cache.save(cache_file.string()));
    }

    // Load into fresh cache
    {
        TestResultCache cache;
        EXPECT_TRUE(cache.load(cache_file.string()));
        EXPECT_EQ(cache.compiler_hash(), "compiler_deadbeef");
        EXPECT_EQ(cache.size(), 2);

        auto* e1 = cache.get("lib_core_tests_1");
        ASSERT_NE(e1, nullptr);
        EXPECT_EQ(e1->source_hashes.size(), 2);
        EXPECT_EQ(e1->source_hashes[0], "abc123");
        EXPECT_EQ(e1->source_hashes[1], "def456");
        EXPECT_EQ(e1->flags_hash, "flags_001");
        EXPECT_TRUE(e1->all_passed);
        EXPECT_EQ(e1->test_count, 8);
        EXPECT_EQ(e1->passed_count, 8);
        EXPECT_EQ(e1->failed_count, 0);
        EXPECT_EQ(e1->duration_us, 12345);
        EXPECT_EQ(e1->exe_path, "/tmp/suite1.exe");

        auto* e2 = cache.get("lib_std_tests_1");
        ASSERT_NE(e2, nullptr);
        EXPECT_FALSE(e2->all_passed);
        EXPECT_EQ(e2->passed_count, 2);
        EXPECT_EQ(e2->failed_count, 1);

        // Verify is_cached works after load
        EXPECT_TRUE(cache.is_cached("lib_core_tests_1", {"abc123", "def456"}, "flags_001"));
        EXPECT_FALSE(cache.is_cached("lib_std_tests_1", {"xyz789"}, "flags_001")); // failed
    }

    // Clean up
    std::error_code ec;
    fs::remove(cache_file, ec);
}

TEST(TestCacheTest, LoadNonexistent) {
    TestResultCache cache;
    EXPECT_FALSE(cache.load("/nonexistent/path/to/cache.json"));
}

TEST(TestCacheTest, LoadInvalidJson) {
    auto temp_dir = fs::temp_directory_path();
    auto cache_file = temp_dir / "tml_test_cache_bad.json";

    {
        std::ofstream ofs(cache_file);
        ofs << "not json at all";
    }

    TestResultCache cache;
    EXPECT_FALSE(cache.load(cache_file.string()));

    std::error_code ec;
    fs::remove(cache_file, ec);
}

// ============================================================================
// Flags hash computation
// ============================================================================

TEST(TestCacheTest, FlagsHashDeterministic) {
    TestConfig config;
    config.coverage = false;
    config.max_per_suite = 8;

    auto hash1 = TestResultCache::compute_flags_hash(config);
    auto hash2 = TestResultCache::compute_flags_hash(config);
    EXPECT_EQ(hash1, hash2);
    EXPECT_FALSE(hash1.empty());
}

TEST(TestCacheTest, FlagsHashChangesCoverage) {
    TestConfig c1;
    c1.coverage = false;
    c1.max_per_suite = 8;

    TestConfig c2;
    c2.coverage = true;
    c2.max_per_suite = 8;

    EXPECT_NE(TestResultCache::compute_flags_hash(c1), TestResultCache::compute_flags_hash(c2));
}

TEST(TestCacheTest, FlagsHashChangesMaxPerSuite) {
    TestConfig c1;
    c1.coverage = false;
    c1.max_per_suite = 8;

    TestConfig c2;
    c2.coverage = false;
    c2.max_per_suite = 1;

    EXPECT_NE(TestResultCache::compute_flags_hash(c1), TestResultCache::compute_flags_hash(c2));
}

// ============================================================================
// Source hash computation
// ============================================================================

TEST(TestCacheTest, SourceHashesSorted) {
    // Create two temp files
    auto temp_dir = fs::temp_directory_path();
    auto f1 = temp_dir / "tml_cache_test_a.tml";
    auto f2 = temp_dir / "tml_cache_test_b.tml";

    {
        std::ofstream(f1) << "// file A content";
        std::ofstream(f2) << "// file B content";
    }

    auto hashes_ab = TestResultCache::compute_source_hashes({f1.string(), f2.string()});
    auto hashes_ba = TestResultCache::compute_source_hashes({f2.string(), f1.string()});

    // Should produce same sorted result regardless of input order
    EXPECT_EQ(hashes_ab, hashes_ba);
    EXPECT_EQ(hashes_ab.size(), 2);

    // Clean up
    std::error_code ec;
    fs::remove(f1, ec);
    fs::remove(f2, ec);
}

TEST(TestCacheTest, SourceHashesChangeOnContentChange) {
    auto temp_dir = fs::temp_directory_path();
    auto f1 = temp_dir / "tml_cache_test_change.tml";

    { std::ofstream(f1) << "// original content"; }
    auto h1 = TestResultCache::compute_source_hashes({f1.string()});

    { std::ofstream(f1) << "// modified content!"; }
    auto h2 = TestResultCache::compute_source_hashes({f1.string()});

    EXPECT_NE(h1, h2);

    std::error_code ec;
    fs::remove(f1, ec);
}

// ============================================================================
// Compiler hash
// ============================================================================

TEST(TestCacheTest, CompilerHashNotEmpty) {
    auto hash = TestResultCache::compute_compiler_hash();
    // Should return some non-empty string (even if we can't verify exact value)
    EXPECT_FALSE(hash.empty());
}

TEST(TestCacheTest, CompilerHashDeterministic) {
    auto h1 = TestResultCache::compute_compiler_hash();
    auto h2 = TestResultCache::compute_compiler_hash();
    EXPECT_EQ(h1, h2);
}
