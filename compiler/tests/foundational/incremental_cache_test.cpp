// Incremental Cache tests
//
// Tests for disk persistence of fingerprints and dependency edges.

#include "query/query_fingerprint.hpp"
#include "query/query_incr.hpp"
#include "query/query_key.hpp"

#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;
using namespace tml::query;

class IncrementalCacheTest : public ::testing::Test {
protected:
    fs::path temp_dir_;

    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / "tml_incr_cache_test";
        fs::create_directories(temp_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(temp_dir_, ec);
    }
};

// ============================================================================
// IncrCacheWriter
// ============================================================================

TEST_F(IncrementalCacheTest, WriterRecordAndCount) {
    IncrCacheWriter writer;
    EXPECT_EQ(writer.entry_count(), 0u);

    auto key = QueryKey{ReadSourceKey{"test.tml"}};
    auto in_fp = fingerprint_string("input");
    auto out_fp = fingerprint_string("output");

    writer.record(key, in_fp, out_fp, {});
    EXPECT_EQ(writer.entry_count(), 1u);
}

TEST_F(IncrementalCacheTest, WriteProducesFile) {
    IncrCacheWriter writer;

    auto key = QueryKey{ReadSourceKey{"test.tml"}};
    writer.record(key, fingerprint_string("in"), fingerprint_string("out"), {});

    auto cache_file = temp_dir_ / "incr.bin";
    bool ok = writer.write(cache_file, 42);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(fs::exists(cache_file));
    EXPECT_GT(fs::file_size(cache_file), 0u);
}

// ============================================================================
// PrevSessionCache load
// ============================================================================

TEST_F(IncrementalCacheTest, LoadNonexistentReturnsFalse) {
    PrevSessionCache prev;
    bool ok = prev.load(temp_dir_ / "nonexistent.bin");
    EXPECT_FALSE(ok);
}

TEST_F(IncrementalCacheTest, RoundTrip) {
    auto key = QueryKey{ReadSourceKey{"hello.tml"}};
    auto in_fp = fingerprint_string("source code");
    auto out_fp = fingerprint_string("tokens");

    // Write
    IncrCacheWriter writer;
    writer.record(key, in_fp, out_fp, {});
    auto cache_file = temp_dir_ / "incr.bin";
    ASSERT_TRUE(writer.write(cache_file, 100));

    // Load
    PrevSessionCache prev;
    ASSERT_TRUE(prev.load(cache_file));
    EXPECT_EQ(prev.options_hash(), 100u);
    EXPECT_EQ(prev.entry_count(), 1u);

    // Lookup
    const auto* entry = prev.lookup(key);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->input_fingerprint, in_fp);
    EXPECT_EQ(entry->output_fingerprint, out_fp);
}

TEST_F(IncrementalCacheTest, LookupMissingReturnsNull) {
    IncrCacheWriter writer;
    auto key = QueryKey{ReadSourceKey{"a.tml"}};
    writer.record(key, fingerprint_string("in"), fingerprint_string("out"), {});

    auto cache_file = temp_dir_ / "incr.bin";
    ASSERT_TRUE(writer.write(cache_file, 0));

    PrevSessionCache prev;
    ASSERT_TRUE(prev.load(cache_file));

    auto missing_key = QueryKey{ReadSourceKey{"missing.tml"}};
    EXPECT_EQ(prev.lookup(missing_key), nullptr);
}

// ============================================================================
// Free functions
// ============================================================================

TEST_F(IncrementalCacheTest, ComputeOptionsHashChanges) {
    auto hash1 = compute_options_hash(0, false, "x86_64-pc-windows-msvc", {}, false);
    auto hash2 = compute_options_hash(2, false, "x86_64-pc-windows-msvc", {}, false);
    auto hash3 = compute_options_hash(0, true, "x86_64-pc-windows-msvc", {}, false);

    // Different optimization → different hash
    EXPECT_NE(hash1, hash2);
    // Different debug_info → different hash
    EXPECT_NE(hash1, hash3);
}

TEST_F(IncrementalCacheTest, CompilerBuildHashNonZero) {
    auto hash = compiler_build_hash();
    EXPECT_NE(hash, 0u);
}
