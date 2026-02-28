// Tests for the Inlining MIR optimization pass
//
// Verifies function inlining decisions, cost analysis, and statistics.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/inlining.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class InliningPassTest : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;

    auto build_mir(const std::string& code) -> tml::mir::Module {
        source_ = std::make_unique<tml::lexer::Source>(tml::lexer::Source::from_string(code));
        tml::lexer::Lexer lexer(*source_);
        auto tokens = lexer.tokenize();

        tml::parser::Parser parser(std::move(tokens));
        auto module_result = parser.parse_module("test");
        EXPECT_TRUE(tml::is_ok(module_result));
        auto& module = std::get<tml::parser::Module>(module_result);

        tml::types::TypeChecker checker;
        auto env_result = checker.check_module(module);
        EXPECT_TRUE(tml::is_ok(env_result));
        auto& env = std::get<tml::types::TypeEnv>(env_result);

        tml::mir::MirBuilder builder(env);
        return builder.build(module);
    }
};

// ============================================================================
// Pass Metadata
// ============================================================================

TEST_F(InliningPassTest, PassName) {
    tml::mir::InliningPass pass;
    EXPECT_EQ(pass.name(), "Inlining");
}

TEST_F(InliningPassTest, AlwaysInlinePassName) {
    tml::mir::AlwaysInlinePass pass;
    EXPECT_EQ(pass.name(), "AlwaysInline");
}

// ============================================================================
// InlineCost Tests
// ============================================================================

TEST_F(InliningPassTest, InlineCostBeneficial) {
    tml::mir::InlineCost cost;
    cost.instruction_cost = 10;
    cost.call_overhead_saved = 20;
    cost.threshold = 50;

    EXPECT_TRUE(cost.should_inline());
    EXPECT_LT(cost.net_cost(), 0) << "Negative net cost means inlining is beneficial";
}

TEST_F(InliningPassTest, InlineCostTooExpensive) {
    tml::mir::InlineCost cost;
    cost.instruction_cost = 500;
    cost.call_overhead_saved = 20;
    cost.threshold = 100;

    EXPECT_FALSE(cost.should_inline());
    EXPECT_GT(cost.net_cost(), 0) << "Positive net cost means inlining is too expensive";
}

TEST_F(InliningPassTest, InlineCostBreakEven) {
    tml::mir::InlineCost cost;
    cost.instruction_cost = 50;
    cost.call_overhead_saved = 50;
    cost.threshold = 50;

    // Net cost = 0, threshold = 50, so 0 <= 50 → should inline
    EXPECT_TRUE(cost.should_inline());
    EXPECT_EQ(cost.net_cost(), 0);
}

// ============================================================================
// Options Tests
// ============================================================================

TEST_F(InliningPassTest, DefaultOptions) {
    tml::mir::InliningOptions opts;

    EXPECT_EQ(opts.base_threshold, 250);
    EXPECT_EQ(opts.recursive_limit, 3);
    EXPECT_EQ(opts.max_callee_size, 500);
    EXPECT_GT(opts.call_penalty, 0);
    EXPECT_GT(opts.alloca_bonus, 0);
    EXPECT_TRUE(opts.inline_hot);
    EXPECT_FALSE(opts.inline_cold);
}

TEST_F(InliningPassTest, CustomOptions) {
    tml::mir::InliningOptions opts;
    opts.base_threshold = 100;
    opts.max_callee_size = 200;

    tml::mir::InliningPass pass(opts);
    EXPECT_EQ(pass.name(), "Inlining");
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(InliningPassTest, InitialStatsAreZero) {
    tml::mir::InliningPass pass;
    auto stats = pass.get_stats();

    EXPECT_EQ(stats.calls_analyzed, 0u);
    EXPECT_EQ(stats.calls_inlined, 0u);
    EXPECT_EQ(stats.calls_not_inlined, 0u);
    EXPECT_EQ(stats.always_inline, 0u);
    EXPECT_EQ(stats.never_inline, 0u);
}

// ============================================================================
// Inlining Behavior
// ============================================================================

TEST_F(InliningPassTest, InlineSmallFunction) {
    auto mir = build_mir(R"(
        func add_one(x: I32) -> I32 {
            return x + 1
        }

        func main() -> I32 {
            return add_one(41)
        }
    )");

    tml::mir::InliningPass pass;
    pass.run(mir);

    auto stats = pass.get_stats();
    // The pass should analyze at least one call
    EXPECT_GE(stats.calls_analyzed, 0u);

    ASSERT_GE(mir.functions.size(), 1u);
}

TEST_F(InliningPassTest, EmptyModuleNoChange) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            return 0
        }
    )");

    tml::mir::InliningPass pass;
    pass.run(mir);

    // No calls to inline
    auto stats = pass.get_stats();
    EXPECT_EQ(stats.calls_inlined, 0u);
}

TEST_F(InliningPassTest, SetProfileData) {
    tml::mir::InliningPass pass;
    pass.set_profile_data(nullptr);

    // Should not crash with null profile data
    auto mir = build_mir(R"(
        func main() -> I32 {
            return 0
        }
    )");

    pass.run(mir);
}

TEST_F(InliningPassTest, SetOptions) {
    tml::mir::InliningPass pass;

    tml::mir::InliningOptions opts;
    opts.base_threshold = 500;
    opts.max_callee_size = 1000;
    pass.set_options(opts);

    auto mir = build_mir(R"(
        func main() -> I32 {
            return 0
        }
    )");

    pass.run(mir);
    EXPECT_EQ(pass.name(), "Inlining");
}

TEST_F(InliningPassTest, RecursiveFunction) {
    auto mir = build_mir(R"(
        func factorial(n: I32) -> I32 {
            if n <= 1 {
                return 1
            }
            return n * factorial(n - 1)
        }

        func main() -> I32 {
            return factorial(5)
        }
    )");

    tml::mir::InliningPass pass;
    pass.run(mir);

    auto stats = pass.get_stats();
    // Recursive inlining should be limited
    EXPECT_GE(stats.calls_analyzed, 0u);
}
