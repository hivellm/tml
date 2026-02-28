// Tests for MIR control flow optimization passes:
// - BlockMerge
// - MergeReturns
// - JumpThreading
// - UnreachableCodeElimination
// - SimplifySelect

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/block_merge.hpp"
#include "mir/passes/jump_threading.hpp"
#include "mir/passes/merge_returns.hpp"
#include "mir/passes/simplify_select.hpp"
#include "mir/passes/unreachable_code_elimination.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class ControlFlowPassesTest : public ::testing::Test {
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

    auto block_count(const tml::mir::Function& func) -> size_t {
        return func.blocks.size();
    }
};

// ============================================================================
// BlockMerge
// ============================================================================

TEST_F(ControlFlowPassesTest, BlockMergePassName) {
    tml::mir::BlockMergePass pass;
    EXPECT_EQ(pass.name(), "BlockMerge");
}

TEST_F(ControlFlowPassesTest, BlockMergeEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::BlockMergePass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(ControlFlowPassesTest, BlockMergeSimpleChain) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 1
            let y: I32 = 2
            return x + y
        }
    )");

    size_t before = block_count(mir.functions[0]);
    tml::mir::BlockMergePass pass;
    pass.run(mir);

    EXPECT_LE(block_count(mir.functions[0]), before);
}

TEST_F(ControlFlowPassesTest, BlockMergeBranching) {
    auto mir = build_mir(R"(
        func choose(flag: Bool) -> I32 {
            if flag { return 1 } else { return 2 }
        }
    )");

    tml::mir::BlockMergePass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// MergeReturns
// ============================================================================

TEST_F(ControlFlowPassesTest, MergeReturnsPassName) {
    tml::mir::MergeReturnsPass pass;
    EXPECT_EQ(pass.name(), "MergeReturns");
}

TEST_F(ControlFlowPassesTest, MergeReturnsEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::MergeReturnsPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(ControlFlowPassesTest, MergeMultipleReturns) {
    auto mir = build_mir(R"(
        func abs(x: I32) -> I32 {
            if x >= 0 {
                return x
            } else {
                return 0 - x
            }
        }
    )");

    tml::mir::MergeReturnsPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// JumpThreading
// ============================================================================

TEST_F(ControlFlowPassesTest, JumpThreadingPassName) {
    tml::mir::JumpThreadingPass pass;
    EXPECT_EQ(pass.name(), "JumpThreading");
}

TEST_F(ControlFlowPassesTest, JumpThreadingEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::JumpThreadingPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(ControlFlowPassesTest, JumpThreadingConditional) {
    auto mir = build_mir(R"(
        func chain(a: Bool, b: Bool) -> I32 {
            if a {
                if b {
                    return 1
                }
                return 2
            }
            return 3
        }
    )");

    tml::mir::JumpThreadingPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// UnreachableCodeElimination
// ============================================================================

TEST_F(ControlFlowPassesTest, UnreachableCodePassName) {
    tml::mir::UnreachableCodeEliminationPass pass;
    EXPECT_EQ(pass.name(), "UnreachableCodeElimination");
}

TEST_F(ControlFlowPassesTest, UnreachableCodeEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::UnreachableCodeEliminationPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(ControlFlowPassesTest, RemoveUnreachableAfterReturn) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            return 42
        }
    )");

    tml::mir::UnreachableCodeEliminationPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// SimplifySelect
// ============================================================================

TEST_F(ControlFlowPassesTest, SimplifySelectPassName) {
    tml::mir::SimplifySelectPass pass;
    EXPECT_EQ(pass.name(), "SimplifySelect");
}

TEST_F(ControlFlowPassesTest, SimplifySelectEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::SimplifySelectPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}
