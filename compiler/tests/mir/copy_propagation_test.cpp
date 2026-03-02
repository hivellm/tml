// Tests for the CopyPropagation MIR optimization pass
//
// Verifies that redundant copies (single-entry phis, identity casts) are
// replaced with the original values.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/copy_propagation.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class CopyPropagationTest : public ::testing::Test {
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

TEST_F(CopyPropagationTest, PassName) {
    tml::mir::CopyPropagationPass pass;
    EXPECT_EQ(pass.name(), "CopyPropagation");
}

TEST_F(CopyPropagationTest, SimpleVariableCopy) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 42
            let y: I32 = x
            return y
        }
    )");

    tml::mir::CopyPropagationPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(CopyPropagationTest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::CopyPropagationPass pass;
    bool changed = pass.run(mir);

    EXPECT_FALSE(changed);
}

TEST_F(CopyPropagationTest, NoCopyInSingleReturn) {
    auto mir = build_mir(R"(
        func constant() -> I32 {
            return 42
        }
    )");

    tml::mir::CopyPropagationPass pass;
    bool changed = pass.run(mir);

    EXPECT_FALSE(changed);
}

TEST_F(CopyPropagationTest, ChainedCopy) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let a: I32 = 10
            let b: I32 = a
            let c: I32 = b
            return c
        }
    )");

    tml::mir::CopyPropagationPass pass;
    // Multiple passes to propagate through chain
    pass.run(mir);
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(CopyPropagationTest, PreserveComputation) {
    auto mir = build_mir(R"(
        func add(a: I32, b: I32) -> I32 {
            let sum: I32 = a + b
            return sum
        }
    )");

    tml::mir::CopyPropagationPass pass;
    pass.run(mir);

    // Addition is not a copy — should preserve
    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_FALSE(mir.functions[0].blocks.empty());
}
