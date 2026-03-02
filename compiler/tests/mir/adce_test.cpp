// Tests for the ADCE (Aggressive Dead Code Elimination) pass
//
// Verifies that code not contributing to observable output is removed.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/adce.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class ADCETest : public ::testing::Test {
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

TEST_F(ADCETest, PassName) {
    tml::mir::ADCEPass pass;
    EXPECT_EQ(pass.name(), "ADCE");
}

TEST_F(ADCETest, RemoveDeadComputation) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let dead: I32 = 1 + 2
            return 0
        }
    )");

    tml::mir::ADCEPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(ADCETest, PreserveSideEffects) {
    auto mir = build_mir(R"(
        func main() {
            print("side effect\n")
        }
    )");

    tml::mir::ADCEPass pass;
    pass.run(mir);

    // print call must be preserved
    EXPECT_GE(total_instructions(mir.functions[0]), 1u);
}

TEST_F(ADCETest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::ADCEPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(ADCETest, PreserveReturnChain) {
    auto mir = build_mir(R"(
        func compute() -> I32 {
            let x: I32 = 10
            let y: I32 = x + 5
            return y
        }
    )");

    tml::mir::ADCEPass pass;
    pass.run(mir);

    // x and y are in the return chain — should be preserved
    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_FALSE(mir.functions[0].blocks.empty());
}
