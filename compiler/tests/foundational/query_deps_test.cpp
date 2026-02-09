// Query Dependency Tracker tests
//
// Tests for dependency tracking between queries and cycle detection.

#include "query/query_deps.hpp"
#include "query/query_key.hpp"

#include <gtest/gtest.h>

using namespace tml::query;

class QueryDepsTest : public ::testing::Test {
protected:
    DependencyTracker tracker_;

    QueryKey make_key(const std::string& path) {
        return ReadSourceKey{path};
    }

    QueryKey make_parse_key(const std::string& path, const std::string& module) {
        return ParseModuleKey{path, module};
    }
};

// ============================================================================
// Stack management
// ============================================================================

TEST_F(QueryDepsTest, InitialDepthIsZero) {
    EXPECT_EQ(tracker_.depth(), 0u);
}

TEST_F(QueryDepsTest, PushIncreasesDepth) {
    tracker_.push_active(make_key("a.tml"));
    EXPECT_EQ(tracker_.depth(), 1u);

    tracker_.push_active(make_key("b.tml"));
    EXPECT_EQ(tracker_.depth(), 2u);
}

TEST_F(QueryDepsTest, PopDecreasesDepth) {
    tracker_.push_active(make_key("a.tml"));
    tracker_.push_active(make_key("b.tml"));
    EXPECT_EQ(tracker_.depth(), 2u);

    tracker_.pop_active();
    EXPECT_EQ(tracker_.depth(), 1u);

    tracker_.pop_active();
    EXPECT_EQ(tracker_.depth(), 0u);
}

// ============================================================================
// Dependency recording
// ============================================================================

TEST_F(QueryDepsTest, RecordDependency) {
    auto key_a = make_key("a.tml");
    auto key_b = make_key("b.tml");

    tracker_.push_active(key_a);
    tracker_.record_dependency(key_b);

    auto deps = tracker_.current_dependencies();
    EXPECT_EQ(deps.size(), 1u);
    EXPECT_EQ(deps[0], key_b);

    tracker_.pop_active();
}

TEST_F(QueryDepsTest, MultipleDependencies) {
    auto key_a = make_key("a.tml");
    auto key_b = make_key("b.tml");
    auto key_c = make_key("c.tml");

    tracker_.push_active(key_a);
    tracker_.record_dependency(key_b);
    tracker_.record_dependency(key_c);

    auto deps = tracker_.current_dependencies();
    EXPECT_EQ(deps.size(), 2u);

    tracker_.pop_active();
}

TEST_F(QueryDepsTest, NoDepsWhenNoneRecorded) {
    tracker_.push_active(make_key("a.tml"));
    auto deps = tracker_.current_dependencies();
    EXPECT_TRUE(deps.empty());
    tracker_.pop_active();
}

// ============================================================================
// Cycle detection
// ============================================================================

TEST_F(QueryDepsTest, NoCycleForNewKey) {
    auto key_a = make_key("a.tml");
    auto key_b = make_key("b.tml");

    tracker_.push_active(key_a);
    auto cycle = tracker_.detect_cycle(key_b);
    EXPECT_FALSE(cycle.has_value());
    tracker_.pop_active();
}

TEST_F(QueryDepsTest, DetectsSelfCycle) {
    auto key_a = make_key("a.tml");

    tracker_.push_active(key_a);
    auto cycle = tracker_.detect_cycle(key_a);
    EXPECT_TRUE(cycle.has_value());
    EXPECT_GE(cycle->size(), 1u);
    tracker_.pop_active();
}

TEST_F(QueryDepsTest, DetectsIndirectCycle) {
    auto key_a = make_key("a.tml");
    auto key_b = make_key("b.tml");

    tracker_.push_active(key_a);
    tracker_.push_active(key_b);

    // Trying to execute key_a again while key_a -> key_b is on stack
    auto cycle = tracker_.detect_cycle(key_a);
    EXPECT_TRUE(cycle.has_value());

    tracker_.pop_active();
    tracker_.pop_active();
}

// ============================================================================
// Clear
// ============================================================================

TEST_F(QueryDepsTest, ClearResetsState) {
    tracker_.push_active(make_key("a.tml"));
    tracker_.push_active(make_key("b.tml"));
    EXPECT_EQ(tracker_.depth(), 2u);

    tracker_.clear();
    EXPECT_EQ(tracker_.depth(), 0u);
}
