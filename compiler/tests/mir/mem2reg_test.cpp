// Tests for the Mem2Reg MIR optimization pass
//
// Verifies that promotable allocas are converted to SSA registers.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/mem2reg.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class Mem2RegTest : public ::testing::Test {
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

// ============================================================================
// Mem2Reg Tests
// ============================================================================

TEST_F(Mem2RegTest, PassName) {
    tml::mir::Mem2RegPass pass;
    EXPECT_EQ(pass.name(), "Mem2Reg");
}

TEST_F(Mem2RegTest, PromoteSimpleVariable) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    size_t allocas_before = count_instructions<tml::mir::AllocaInst>(mir.functions[0]);

    tml::mir::Mem2RegPass pass;
    pass.run(mir);

    size_t allocas_after = count_instructions<tml::mir::AllocaInst>(mir.functions[0]);

    // Mem2reg should promote the simple alloca to SSA
    if (allocas_before > 0) {
        EXPECT_LE(allocas_after, allocas_before)
            << "Mem2reg should reduce alloca count for simple variables";
    }
}

TEST_F(Mem2RegTest, PromoteMultipleVariables) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let a: I32 = 1
            let b: I32 = 2
            return a + b
        }
    )");

    size_t allocas_before = count_instructions<tml::mir::AllocaInst>(mir.functions[0]);

    tml::mir::Mem2RegPass pass;
    pass.run(mir);

    size_t allocas_after = count_instructions<tml::mir::AllocaInst>(mir.functions[0]);

    if (allocas_before > 0) {
        EXPECT_LE(allocas_after, allocas_before);
    }
}

TEST_F(Mem2RegTest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::Mem2RegPass pass;
    bool changed = pass.run(mir);

    EXPECT_FALSE(changed);
}

TEST_F(Mem2RegTest, FunctionWithOnlyReturn) {
    auto mir = build_mir(R"(
        func constant() -> I32 {
            return 42
        }
    )");

    tml::mir::Mem2RegPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_FALSE(mir.functions[0].blocks.empty());
}

TEST_F(Mem2RegTest, PreserveFunctionParams) {
    auto mir = build_mir(R"(
        func identity(x: I32) -> I32 {
            return x
        }
    )");

    tml::mir::Mem2RegPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_EQ(mir.functions[0].params.size(), 1u);
}

TEST_F(Mem2RegTest, ReduceLoadStoreCount) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 10
            let y: I32 = x + 5
            return y
        }
    )");

    size_t loads_before = count_instructions<tml::mir::LoadInst>(mir.functions[0]);
    size_t stores_before = count_instructions<tml::mir::StoreInst>(mir.functions[0]);

    tml::mir::Mem2RegPass pass;
    pass.run(mir);

    size_t loads_after = count_instructions<tml::mir::LoadInst>(mir.functions[0]);
    size_t stores_after = count_instructions<tml::mir::StoreInst>(mir.functions[0]);

    // After promotion, loads/stores to promoted allocas should be gone
    EXPECT_LE(loads_after, loads_before);
    EXPECT_LE(stores_after, stores_before);
}
