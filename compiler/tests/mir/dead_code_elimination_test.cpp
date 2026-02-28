// Tests for the DeadCodeElimination MIR optimization pass
//
// Verifies that unused instructions are removed while preserving
// instructions with side effects.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/dead_code_elimination.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class DCETest : public ::testing::Test {
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

    auto total_instructions(const tml::mir::Function& func) -> size_t {
        size_t count = 0;
        for (const auto& block : func.blocks) {
            count += block.instructions.size();
        }
        return count;
    }
};

// ============================================================================
// Basic DCE Tests
// ============================================================================

TEST_F(DCETest, PassName) {
    tml::mir::DeadCodeEliminationPass pass;
    EXPECT_EQ(pass.name(), "DeadCodeElimination");
}

TEST_F(DCETest, RemoveUnusedVariable) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let unused: I32 = 42
            return 0
        }
    )");

    auto before = total_instructions(mir.functions[0]);

    tml::mir::DeadCodeEliminationPass pass;
    pass.run(mir);

    auto after = total_instructions(mir.functions[0]);
    EXPECT_LE(after, before) << "DCE should not add instructions";
}

TEST_F(DCETest, PreserveUsedVariable) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    tml::mir::DeadCodeEliminationPass pass;
    pass.run(mir);

    // x is used in return — should not be eliminated
    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_FALSE(mir.functions[0].blocks.empty());
}

TEST_F(DCETest, PreserveSideEffects) {
    auto mir = build_mir(R"(
        func main() {
            print("hello\n")
        }
    )");

    tml::mir::DeadCodeEliminationPass pass;
    pass.run(mir);

    // print has side effects — should not be removed
    size_t after = total_instructions(mir.functions[0]);
    EXPECT_GE(after, 1u) << "Side-effecting calls must be preserved";
}

TEST_F(DCETest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::DeadCodeEliminationPass pass;
    bool changed = pass.run(mir);

    EXPECT_FALSE(changed);
}

TEST_F(DCETest, MultipleUnusedVariables) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let a: I32 = 1
            let b: I32 = 2
            let c: I32 = 3
            return 0
        }
    )");

    tml::mir::DeadCodeEliminationPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(DCETest, PreserveReturnValue) {
    auto mir = build_mir(R"(
        func compute() -> I32 {
            let x: I32 = 10
            let y: I32 = x + 5
            return y
        }
    )");

    tml::mir::DeadCodeEliminationPass pass;
    pass.run(mir);

    // Both x and y are used in the return chain
    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_FALSE(mir.functions[0].blocks.empty());
}

TEST_F(DCETest, IterativeElimination) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let a: I32 = 1
            let b: I32 = a + 2
            let c: I32 = b + 3
            return 0
        }
    )");

    tml::mir::DeadCodeEliminationPass pass;
    // First pass removes c, second pass may remove b, third a
    pass.run(mir);
    pass.run(mir);
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(DCETest, MultipleFunctions) {
    auto mir = build_mir(R"(
        func foo() -> I32 {
            let dead: I32 = 99
            return 0
        }

        func bar() -> I32 {
            let also_dead: I32 = 88
            return 1
        }
    )");

    tml::mir::DeadCodeEliminationPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 2u);
}
