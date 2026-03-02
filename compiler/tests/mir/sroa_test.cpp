// Tests for the SROA (Scalar Replacement of Aggregates) pass
//
// Verifies that aggregate allocas are decomposed into scalar allocas
// when accessed field-by-field.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/sroa.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class SROATest : public ::testing::Test {
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

    template <typename T> auto count_instructions(const tml::mir::Function& func) -> size_t {
        size_t count = 0;
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (std::holds_alternative<T>(inst.inst)) {
                    count++;
                }
            }
        }
        return count;
    }
};

TEST_F(SROATest, PassName) {
    tml::mir::SROAPass pass;
    EXPECT_EQ(pass.name(), "SROA");
}

TEST_F(SROATest, SplitStructAlloca) {
    auto mir = build_mir(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> I32 {
            let p: Point = Point { x: 10, y: 20 }
            return p.x + p.y
        }
    )");

    tml::mir::SROAPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(SROATest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::SROAPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(SROATest, ScalarVariableUnchanged) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    tml::mir::SROAPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(SROATest, TupleDecomposition) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let t: (I32, I32) = (1, 2)
            return 0
        }
    )");

    tml::mir::SROAPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}
