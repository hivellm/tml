// Query System tests
//
// Tests for the full QueryContext pipeline (providers, caching, convenience methods).

#include "query/query_context.hpp"
#include "query/query_key.hpp"

#include <gtest/gtest.h>

using namespace tml::query;

class QuerySystemTest : public ::testing::Test {
protected:
    QueryOptions make_opts() {
        QueryOptions opts;
        opts.incremental = false; // Don't try to load disk cache in tests
        return opts;
    }
};

// ============================================================================
// Construction
// ============================================================================

TEST_F(QuerySystemTest, DefaultConstruction) {
    QueryContext ctx(make_opts());
    auto stats = ctx.cache_stats();
    EXPECT_EQ(stats.total_entries, 0u);
    EXPECT_EQ(stats.hits, 0u);
    EXPECT_EQ(stats.misses, 0u);
}

// ============================================================================
// Cache management
// ============================================================================

TEST_F(QuerySystemTest, ClearCache) {
    QueryContext ctx(make_opts());

    // Manually insert something into the cache
    auto key = QueryKey{ReadSourceKey{"dummy.tml"}};
    ReadSourceResult result;
    result.success = true;
    result.source_code = "test";
    ctx.cache().insert<ReadSourceResult>(key, result, fingerprint_string("in"),
                                         fingerprint_string("out"), {});

    EXPECT_EQ(ctx.cache_stats().total_entries, 1u);
    ctx.clear_cache();
    EXPECT_EQ(ctx.cache_stats().total_entries, 0u);
}

// ============================================================================
// Incremental mode
// ============================================================================

TEST_F(QuerySystemTest, IncrementalNotActiveByDefault) {
    QueryOptions opts = make_opts();
    opts.incremental = false;
    QueryContext ctx(opts);
    EXPECT_FALSE(ctx.incremental_active());
}

// ============================================================================
// Options access
// ============================================================================

TEST_F(QuerySystemTest, OptionsPreserved) {
    QueryOptions opts = make_opts();
    opts.verbose = true;
    opts.optimization_level = 2;
    opts.target_triple = "x86_64-pc-windows-msvc";

    QueryContext ctx(opts);
    EXPECT_TRUE(ctx.options().verbose);
    EXPECT_EQ(ctx.options().optimization_level, 2);
    EXPECT_EQ(ctx.options().target_triple, "x86_64-pc-windows-msvc");
}
