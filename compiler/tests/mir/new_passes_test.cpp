// Tests for MIR passes added after initial test suite:
// - RemoveUnneededDrops
// - DestinationPropagation
// - NormalizeArrayLen

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/destination_propagation.hpp"
#include "mir/passes/normalize_array_len.hpp"
#include "mir/passes/remove_unneeded_drops.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class NewPassesTest : public ::testing::Test {
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
// RemoveUnneededDrops
// ============================================================================

TEST_F(NewPassesTest, RemoveUnneededDropsPassName) {
    tml::mir::RemoveUnneededDropsPass pass;
    EXPECT_EQ(pass.name(), "RemoveUnneededDrops");
}

TEST_F(NewPassesTest, RemoveUnneededDropsEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::RemoveUnneededDropsPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(NewPassesTest, RemoveUnneededDropsPrimitiveTypes) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 42
            let y: Bool = true
            return x
        }
    )");

    tml::mir::RemoveUnneededDropsPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(NewPassesTest, RemoveUnneededDropsMultipleFunctions) {
    auto mir = build_mir(R"(
        func foo() -> I32 {
            let a: I32 = 1
            return a
        }
        func bar() -> I32 {
            let b: I32 = 2
            return b
        }
    )");

    tml::mir::RemoveUnneededDropsPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 2u);
}

// ============================================================================
// DestinationPropagation
// ============================================================================

TEST_F(NewPassesTest, DestinationPropagationPassName) {
    tml::mir::DestinationPropagationPass pass;
    EXPECT_EQ(pass.name(), "DestinationPropagation");
}

TEST_F(NewPassesTest, DestinationPropagationEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::DestinationPropagationPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(NewPassesTest, DestinationPropagationSimpleCopy) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 42
            let y: I32 = x
            return y
        }
    )");

    size_t before = total_instructions(mir.functions[0]);
    tml::mir::DestinationPropagationPass pass;
    pass.run(mir);

    // Pass may or may not optimize, but should not crash
    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_LE(total_instructions(mir.functions[0]), before);
}

TEST_F(NewPassesTest, DestinationPropagationNoChange) {
    auto mir = build_mir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");

    tml::mir::DestinationPropagationPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// NormalizeArrayLen
// ============================================================================

TEST_F(NewPassesTest, NormalizeArrayLenPassName) {
    tml::mir::NormalizeArrayLenPass pass;
    EXPECT_EQ(pass.name(), "NormalizeArrayLen");
}

TEST_F(NewPassesTest, NormalizeArrayLenEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::NormalizeArrayLenPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(NewPassesTest, NormalizeArrayLenNoArrays) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            return 42
        }
    )");

    tml::mir::NormalizeArrayLenPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}
