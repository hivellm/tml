// Query Cache tests
//
// Tests for the thread-safe memoization cache.

#include "query/query_cache.hpp"
#include "query/query_fingerprint.hpp"
#include "query/query_key.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace tml::query;

class QueryCacheTest : public ::testing::Test {
protected:
    QueryCache cache_;

    QueryKey make_key(const std::string& path) {
        return ReadSourceKey{path};
    }

    Fingerprint make_fp(const std::string& s) {
        return fingerprint_string(s);
    }
};

// ============================================================================
// Insert + Lookup round-trip
// ============================================================================

TEST_F(QueryCacheTest, InsertAndLookup) {
    auto key = make_key("test.tml");
    ReadSourceResult result;
    result.source_code = "func main() {}";
    result.success = true;

    cache_.insert<ReadSourceResult>(key, result, make_fp("in"), make_fp("out"), {});

    auto cached = cache_.lookup<ReadSourceResult>(key);
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->source_code, "func main() {}");
    EXPECT_TRUE(cached->success);
}

TEST_F(QueryCacheTest, LookupMissingReturnsNullopt) {
    auto key = make_key("nonexistent.tml");
    auto cached = cache_.lookup<ReadSourceResult>(key);
    EXPECT_FALSE(cached.has_value());
}

// ============================================================================
// Contains
// ============================================================================

TEST_F(QueryCacheTest, ContainsAfterInsert) {
    auto key = make_key("test.tml");
    EXPECT_FALSE(cache_.contains(key));

    ReadSourceResult result;
    result.success = true;
    cache_.insert<ReadSourceResult>(key, result, make_fp("in"), make_fp("out"), {});

    EXPECT_TRUE(cache_.contains(key));
}

// ============================================================================
// Invalidate
// ============================================================================

TEST_F(QueryCacheTest, InvalidateRemovesEntry) {
    auto key = make_key("test.tml");
    ReadSourceResult result;
    result.success = true;
    cache_.insert<ReadSourceResult>(key, result, make_fp("in"), make_fp("out"), {});

    EXPECT_TRUE(cache_.contains(key));
    cache_.invalidate(key);
    EXPECT_FALSE(cache_.contains(key));
}

TEST_F(QueryCacheTest, InvalidateDependents) {
    auto key_src = make_key("src.tml");
    auto key_tok = TokenizeKey{"src.tml"};
    QueryKey tok_qk = key_tok;

    ReadSourceResult src_result;
    src_result.success = true;
    cache_.insert<ReadSourceResult>(key_src, src_result, make_fp("in1"), make_fp("out1"), {});

    TokenizeResult tok_result;
    tok_result.success = true;
    // Tokenize depends on ReadSource
    cache_.insert<TokenizeResult>(tok_qk, tok_result, make_fp("in2"), make_fp("out2"), {key_src});

    EXPECT_TRUE(cache_.contains(key_src));
    EXPECT_TRUE(cache_.contains(tok_qk));

    // Invalidating ReadSource should cascade to Tokenize
    cache_.invalidate_dependents(key_src);
    EXPECT_FALSE(cache_.contains(tok_qk));
}

// ============================================================================
// Clear
// ============================================================================

TEST_F(QueryCacheTest, ClearRemovesAll) {
    auto key1 = make_key("a.tml");
    auto key2 = make_key("b.tml");
    ReadSourceResult r;
    r.success = true;
    cache_.insert<ReadSourceResult>(key1, r, make_fp("in"), make_fp("out"), {});
    cache_.insert<ReadSourceResult>(key2, r, make_fp("in"), make_fp("out"), {});

    EXPECT_TRUE(cache_.contains(key1));
    EXPECT_TRUE(cache_.contains(key2));

    cache_.clear();
    EXPECT_FALSE(cache_.contains(key1));
    EXPECT_FALSE(cache_.contains(key2));
}

// ============================================================================
// Stats
// ============================================================================

TEST_F(QueryCacheTest, StatsTrackHitsAndMisses) {
    auto key = make_key("test.tml");
    ReadSourceResult result;
    result.success = true;
    cache_.insert<ReadSourceResult>(key, result, make_fp("in"), make_fp("out"), {});

    // Miss
    cache_.lookup<ReadSourceResult>(make_key("missing.tml"));
    // Hit
    cache_.lookup<ReadSourceResult>(key);

    auto stats = cache_.get_stats();
    EXPECT_EQ(stats.total_entries, 1u);
    EXPECT_GE(stats.hits, 1u);
    EXPECT_GE(stats.misses, 1u);
}

// ============================================================================
// Thread safety
// ============================================================================

TEST_F(QueryCacheTest, ConcurrentInserts) {
    constexpr int num_threads = 4;
    constexpr int inserts_per_thread = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < inserts_per_thread; ++i) {
                auto key = make_key("file_" + std::to_string(t) + "_" + std::to_string(i) + ".tml");
                ReadSourceResult result;
                result.source_code = "thread " + std::to_string(t);
                result.success = true;
                cache_.insert<ReadSourceResult>(key, result, make_fp("in"), make_fp("out"), {});
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto stats = cache_.get_stats();
    EXPECT_EQ(stats.total_entries, static_cast<size_t>(num_threads * inserts_per_thread));
}
