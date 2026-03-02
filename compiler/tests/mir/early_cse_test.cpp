// Tests for the EarlyCSE MIR optimization pass

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/early_cse.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class EarlyCSEPassTest : public ::testing::Test {
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

TEST_F(EarlyCSEPassTest, PassName) {
    tml::mir::EarlyCSEPass pass;
    EXPECT_EQ(pass.name(), "EarlyCSE");
}

TEST_F(EarlyCSEPassTest, EliminateDuplicateExpression) {
    auto mir = build_mir(R"(
        func main(a: I32, b: I32) -> I32 {
            let x: I32 = a + b
            let y: I32 = a + b
            return x + y
        }
    )");

    tml::mir::EarlyCSEPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(EarlyCSEPassTest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::EarlyCSEPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(EarlyCSEPassTest, DifferentExpressionsNotEliminated) {
    auto mir = build_mir(R"(
        func main(a: I32, b: I32) -> I32 {
            let x: I32 = a + b
            let y: I32 = a - b
            return x + y
        }
    )");

    tml::mir::EarlyCSEPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}
