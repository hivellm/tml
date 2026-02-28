// Tests for MIR loop optimization passes:
// - LICM (Loop Invariant Code Motion)
// - LoopOpts
// - LoopRotate
// - LoopUnroll
// - InfiniteLoopCheck

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/infinite_loop_check.hpp"
#include "mir/passes/licm.hpp"
#include "mir/passes/loop_opts.hpp"
#include "mir/passes/loop_rotate.hpp"
#include "mir/passes/loop_unroll.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class LoopPassesTest : public ::testing::Test {
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
// LICM
// ============================================================================

TEST_F(LoopPassesTest, LICMPassName) {
    tml::mir::LICMPass pass;
    EXPECT_EQ(pass.name(), "LICM");
}

TEST_F(LoopPassesTest, LICMEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::LICMPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(LoopPassesTest, LICMWithLoop) {
    auto mir = build_mir(R"(
        func sum() -> I32 {
            var total: I32 = 0
            var i: I32 = 0
            loop {
                if i >= 10 { break }
                total = total + i
                i = i + 1
            }
            return total
        }
    )");

    tml::mir::LICMPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// LoopOpts
// ============================================================================

TEST_F(LoopPassesTest, AdvancedLoopOptPassName) {
    tml::mir::AdvancedLoopOptPass pass;
    EXPECT_EQ(pass.name(), "AdvancedLoopOpt");
}

TEST_F(LoopPassesTest, AdvancedLoopOptEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::AdvancedLoopOptPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(LoopPassesTest, AdvancedLoopOptSimpleLoop) {
    auto mir = build_mir(R"(
        func count() -> I32 {
            var i: I32 = 0
            loop {
                if i >= 5 { break }
                i = i + 1
            }
            return i
        }
    )");

    tml::mir::AdvancedLoopOptPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// LoopRotate
// ============================================================================

TEST_F(LoopPassesTest, LoopRotatePassName) {
    tml::mir::LoopRotatePass pass;
    EXPECT_EQ(pass.name(), "LoopRotate");
}

TEST_F(LoopPassesTest, LoopRotateEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::LoopRotatePass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// LoopUnroll
// ============================================================================

TEST_F(LoopPassesTest, LoopUnrollPassName) {
    tml::mir::LoopUnrollPass pass;
    EXPECT_EQ(pass.name(), "LoopUnroll");
}

TEST_F(LoopPassesTest, LoopUnrollEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::LoopUnrollPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// InfiniteLoopCheck
// ============================================================================

TEST_F(LoopPassesTest, InfiniteLoopCheckPassName) {
    tml::mir::InfiniteLoopCheckPass pass;
    EXPECT_EQ(pass.name(), "InfiniteLoopCheck");
}

TEST_F(LoopPassesTest, InfiniteLoopCheckEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::InfiniteLoopCheckPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(LoopPassesTest, InfiniteLoopCheckWithBreak) {
    auto mir = build_mir(R"(
        func bounded() -> I32 {
            var i: I32 = 0
            loop {
                if i >= 10 { break }
                i = i + 1
            }
            return i
        }
    )");

    tml::mir::InfiniteLoopCheckPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}
