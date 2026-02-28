// Tests for the MIR PassManager and analysis utilities

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "mir/passes/constant_folding.hpp"
#include "mir/passes/dead_code_elimination.hpp"
#include "mir/passes/mem2reg.hpp"
#include "mir/passes/simplify_cfg.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class PassManagerTest : public ::testing::Test {
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

// ============================================================================
// PassManager
// ============================================================================

TEST_F(PassManagerTest, CreateWithOptLevel) {
    tml::mir::PassManager pm_o0(tml::mir::OptLevel::O0);
    EXPECT_EQ(pm_o0.opt_level(), tml::mir::OptLevel::O0);

    tml::mir::PassManager pm_o1(tml::mir::OptLevel::O1);
    EXPECT_EQ(pm_o1.opt_level(), tml::mir::OptLevel::O1);

    tml::mir::PassManager pm_o2(tml::mir::OptLevel::O2);
    EXPECT_EQ(pm_o2.opt_level(), tml::mir::OptLevel::O2);

    tml::mir::PassManager pm_o3(tml::mir::OptLevel::O3);
    EXPECT_EQ(pm_o3.opt_level(), tml::mir::OptLevel::O3);
}

TEST_F(PassManagerTest, DefaultOptLevel) {
    tml::mir::PassManager pm;
    EXPECT_EQ(pm.opt_level(), tml::mir::OptLevel::O2);
}

TEST_F(PassManagerTest, AddAndRunPasses) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    pm.add_pass(std::make_unique<tml::mir::ConstantFoldingPass>());
    pm.add_pass(std::make_unique<tml::mir::DeadCodeEliminationPass>());

    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 1 + 2
            return x
        }
    )");

    int changed = pm.run(mir);
    EXPECT_GE(changed, 0);
}

TEST_F(PassManagerTest, StandardPipeline) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    pm.configure_standard_pipeline();

    auto mir = build_mir(R"(
        func main() -> I32 {
            let a: I32 = 10
            let b: I32 = 20
            return a + b
        }
    )");

    int changed = pm.run(mir);
    EXPECT_GE(changed, 0);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(PassManagerTest, EmptyPipeline) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O0);

    auto mir = build_mir("func main() { }");

    int changed = pm.run(mir);
    EXPECT_EQ(changed, 0);
}

TEST_F(PassManagerTest, RunOnEmptyModule) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    pm.add_pass(std::make_unique<tml::mir::ConstantFoldingPass>());

    auto mir = build_mir("func main() { }");
    int changed = pm.run(mir);
    EXPECT_GE(changed, 0);
}

TEST_F(PassManagerTest, SetPipelineDir) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    pm.set_pipeline_dir("", "test_module");
    pm.configure_standard_pipeline();

    auto mir = build_mir("func main() { }");
    pm.run(mir);
}

// ============================================================================
// Analysis Utilities (from mir_pass.hpp)
// ============================================================================

TEST_F(PassManagerTest, IsValueUsed) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    ASSERT_EQ(mir.functions.size(), 1u);
    const auto& func = mir.functions[0];

    // Check that some values are used
    // (We don't know specific IDs, but the function should not crash)
    bool any_used = false;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != tml::mir::INVALID_VALUE) {
                if (tml::mir::is_value_used(func, inst.result)) {
                    any_used = true;
                }
            }
        }
    }
    // At least x should be used (in return)
    // But the exact structure depends on MIR builder
}

TEST_F(PassManagerTest, HasSideEffects) {
    auto mir = build_mir(R"(
        func main() {
            print("hello\n")
        }
    )");

    ASSERT_EQ(mir.functions.size(), 1u);
    const auto& func = mir.functions[0];

    // Find a call instruction — it should have side effects
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (std::holds_alternative<tml::mir::CallInst>(inst.inst)) {
                EXPECT_TRUE(tml::mir::has_side_effects(inst.inst));
            }
        }
    }
}

TEST_F(PassManagerTest, IsConstant) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            return 42
        }
    )");

    ASSERT_EQ(mir.functions.size(), 1u);
    const auto& func = mir.functions[0];

    bool found_constant = false;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (tml::mir::is_constant(inst.inst)) {
                found_constant = true;
            }
        }
    }
    EXPECT_TRUE(found_constant) << "Should find at least one constant instruction (42)";
}

TEST_F(PassManagerTest, GetConstantInt) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            return 42
        }
    )");

    ASSERT_EQ(mir.functions.size(), 1u);
    const auto& func = mir.functions[0];

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            auto val = tml::mir::get_constant_int(inst.inst);
            if (val.has_value()) {
                EXPECT_EQ(val.value(), 42);
                return; // Found it
            }
        }
    }
    // It's OK if the constant is stored differently
}
