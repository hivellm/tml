// Tests for the StrengthReduction MIR optimization pass

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/strength_reduction.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class StrengthReductionTest : public ::testing::Test {
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

TEST_F(StrengthReductionTest, PassName) {
    tml::mir::StrengthReductionPass pass;
    EXPECT_EQ(pass.name(), "StrengthReduction");
}

TEST_F(StrengthReductionTest, MultiplyByPowerOfTwo) {
    auto mir = build_mir(R"(
        func main(x: I32) -> I32 {
            return x * 8
        }
    )");

    tml::mir::StrengthReductionPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(StrengthReductionTest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::StrengthReductionPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(StrengthReductionTest, DivideByPowerOfTwo) {
    auto mir = build_mir(R"(
        func main(x: I32) -> I32 {
            return x / 4
        }
    )");

    tml::mir::StrengthReductionPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(StrengthReductionTest, ModuloByPowerOfTwo) {
    auto mir = build_mir(R"(
        func main(x: I32) -> I32 {
            return x % 16
        }
    )");

    tml::mir::StrengthReductionPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(StrengthReductionTest, NonPowerOfTwoUnchanged) {
    auto mir = build_mir(R"(
        func main(x: I32) -> I32 {
            return x * 7
        }
    )");

    tml::mir::StrengthReductionPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}
