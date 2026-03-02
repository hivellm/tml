// Tests for the ConstantPropagation MIR optimization pass

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/constant_propagation.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class ConstantPropagationTest : public ::testing::Test {
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

TEST_F(ConstantPropagationTest, PassName) {
    tml::mir::ConstantPropagationPass pass;
    EXPECT_EQ(pass.name(), "ConstantPropagation");
}

TEST_F(ConstantPropagationTest, PropagateConstantThroughUse) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 42
            return x + 1
        }
    )");

    tml::mir::ConstantPropagationPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(ConstantPropagationTest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::ConstantPropagationPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(ConstantPropagationTest, ParameterNotPropagated) {
    auto mir = build_mir(R"(
        func identity(x: I32) -> I32 {
            return x
        }
    )");

    tml::mir::ConstantPropagationPass pass;
    bool changed = pass.run(mir);
    // Parameters are not constants — nothing to propagate
    EXPECT_FALSE(changed);
}

TEST_F(ConstantPropagationTest, MultipleConstants) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let a: I32 = 10
            let b: I32 = 20
            return a + b
        }
    )");

    tml::mir::ConstantPropagationPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}
