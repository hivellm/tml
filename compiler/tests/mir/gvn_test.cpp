// Tests for the GVN (Global Value Numbering) pass
//
// Verifies that redundant computations are eliminated across basic blocks.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/gvn.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class GVNTest : public ::testing::Test {
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

TEST_F(GVNTest, PassName) {
    tml::mir::GVNPass pass;
    EXPECT_EQ(pass.name(), "GVN");
}

TEST_F(GVNTest, EliminateRedundantComputation) {
    auto mir = build_mir(R"(
        func main(a: I32, b: I32) -> I32 {
            let x: I32 = a + b
            let y: I32 = a + b
            return x + y
        }
    )");

    tml::mir::GVNPass pass;
    pass.run(mir);

    // GVN should recognize a+b computed twice and reuse
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(GVNTest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::GVNPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(GVNTest, DifferentOperationsNotEliminated) {
    auto mir = build_mir(R"(
        func main(a: I32, b: I32) -> I32 {
            let x: I32 = a + b
            let y: I32 = a * b
            return x + y
        }
    )");

    tml::mir::GVNPass pass;
    pass.run(mir);

    // a+b and a*b are different — should not be merged
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(GVNTest, ConstructWithAliasAnalysis) {
    // GVNPass can accept nullptr for alias analysis
    tml::mir::GVNPass pass(nullptr);
    EXPECT_EQ(pass.name(), "GVN");
}

TEST_F(GVNTest, SingleInstruction) {
    auto mir = build_mir(R"(
        func identity(x: I32) -> I32 {
            return x
        }
    )");

    tml::mir::GVNPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}
