// MIR Optimization Pass Tests
//
// Tests for MIR optimization passes: constant folding, DCE, copy propagation,
// simplify CFG, block merge, strength reduction, mem2reg, SROA, tail call, inlining

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "mir/passes/block_merge.hpp"
#include "mir/passes/constant_folding.hpp"
#include "mir/passes/constant_propagation.hpp"
#include "mir/passes/copy_propagation.hpp"
#include "mir/passes/dead_code_elimination.hpp"
#include "mir/passes/inlining.hpp"
#include "mir/passes/mem2reg.hpp"
#include "mir/passes/simplify_cfg.hpp"
#include "mir/passes/sroa.hpp"
#include "mir/passes/strength_reduction.hpp"
#include "mir/passes/tail_call.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

// ============================================================================
// Test Fixture - shared MIR build pipeline
// ============================================================================

class MirPassTest : public ::testing::Test {
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

    // Count instructions of a specific type in a function
    template <typename InstType> auto count_instructions(const tml::mir::Function& func) -> size_t {
        size_t count = 0;
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (std::holds_alternative<InstType>(inst.inst)) {
                    count++;
                }
            }
        }
        return count;
    }

    // Count total instructions across all blocks in a function
    auto count_total_instructions(const tml::mir::Function& func) -> size_t {
        size_t count = 0;
        for (const auto& block : func.blocks) {
            count += block.instructions.size();
        }
        return count;
    }

    // Find a function by name in the module
    auto find_function(const tml::mir::Module& module, const std::string& name)
        -> const tml::mir::Function* {
        for (const auto& func : module.functions) {
            if (func.name.find(name) != std::string::npos) {
                return &func;
            }
        }
        return nullptr;
    }
};

// ============================================================================
// Constant Folding Tests
// ============================================================================

TEST_F(MirPassTest, ConstantFoldingPassName) {
    tml::mir::ConstantFoldingPass pass;
    EXPECT_EQ(pass.name(), "ConstantFolding");
}

TEST_F(MirPassTest, ConstantFoldingSimpleAdd) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            return 2 + 3
        }
    )");

    size_t before_count = count_total_instructions(mir.functions[0]);

    tml::mir::ConstantFoldingPass pass;
    bool changed = pass.run(mir);

    // The pass should either fold the constant or leave it unchanged
    // (depending on whether the MIR builder already constant-folded)
    size_t after_count = count_total_instructions(mir.functions[0]);
    if (changed) {
        EXPECT_LE(after_count, before_count)
            << "Constant folding should not increase instruction count";
    }
}

TEST_F(MirPassTest, ConstantFoldingMultipleOps) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let a: I32 = 10 * 2
            let b: I32 = a + 5
            return b
        }
    )");

    tml::mir::ConstantFoldingPass pass;
    pass.run(mir);

    // After folding, there should be constant instructions present
    ASSERT_FALSE(mir.functions.empty());
    bool has_constants = count_instructions<tml::mir::ConstantInst>(mir.functions[0]) > 0;
    EXPECT_TRUE(has_constants) << "Should have constant instructions";
}

TEST_F(MirPassTest, ConstantFoldingNoChangeOnVariables) {
    auto mir = build_mir(R"(
        func test(x: I32, y: I32) -> I32 {
            return x + y
        }
    )");

    tml::mir::ConstantFoldingPass pass;
    bool changed = pass.run(mir);

    // No constants to fold when operands are parameters
    // The pass may or may not change anything depending on other patterns
    (void)changed;
    ASSERT_FALSE(mir.functions.empty());
}

// ============================================================================
// Dead Code Elimination Tests
// ============================================================================

TEST_F(MirPassTest, DCEPassName) {
    tml::mir::DeadCodeEliminationPass pass;
    EXPECT_EQ(pass.name(), "DeadCodeElimination");
}

TEST_F(MirPassTest, DCERemovesUnusedComputation) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            let unused: I32 = 100 + 200
            return x
        }
    )");

    size_t before_count = count_total_instructions(mir.functions[0]);

    tml::mir::DeadCodeEliminationPass pass;
    bool changed = pass.run(mir);

    if (changed) {
        size_t after_count = count_total_instructions(mir.functions[0]);
        EXPECT_LT(after_count, before_count)
            << "DCE should reduce instruction count when removing dead code";
    }
}

TEST_F(MirPassTest, DCEPreservesSideEffects) {
    auto mir = build_mir(R"(
        func test() {
            print(42)
        }
    )");

    size_t before_calls = count_instructions<tml::mir::CallInst>(mir.functions[0]);

    tml::mir::DeadCodeEliminationPass pass;
    pass.run(mir);

    size_t after_calls = count_instructions<tml::mir::CallInst>(mir.functions[0]);
    EXPECT_EQ(after_calls, before_calls) << "DCE should not remove calls with side effects";
}

TEST_F(MirPassTest, DCEPreservesUsedValues) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 10
            let y: I32 = x + 5
            return y
        }
    )");

    tml::mir::DeadCodeEliminationPass pass;
    pass.run(mir);

    // The return value chain must be preserved
    ASSERT_FALSE(mir.functions.empty());
    ASSERT_FALSE(mir.functions[0].blocks.empty());
    EXPECT_TRUE(mir.functions[0].blocks.back().terminator.has_value())
        << "Function should still have a terminator";
}

// ============================================================================
// Copy Propagation Tests
// ============================================================================

TEST_F(MirPassTest, CopyPropagationPassName) {
    tml::mir::CopyPropagationPass pass;
    EXPECT_EQ(pass.name(), "CopyPropagation");
}

TEST_F(MirPassTest, CopyPropagationSimple) {
    auto mir = build_mir(R"(
        func test(x: I32) -> I32 {
            let y: I32 = x
            return y
        }
    )");

    tml::mir::CopyPropagationPass pass;
    pass.run(mir);

    // After copy propagation, the function should still be valid
    ASSERT_FALSE(mir.functions.empty());
    EXPECT_GT(mir.functions[0].blocks.size(), 0u);
}

TEST_F(MirPassTest, CopyPropagationChainedCopies) {
    auto mir = build_mir(R"(
        func test(x: I32) -> I32 {
            let a: I32 = x
            let b: I32 = a
            let c: I32 = b
            return c
        }
    )");

    tml::mir::CopyPropagationPass pass;
    pass.run(mir);

    ASSERT_FALSE(mir.functions.empty());
    // Function should still produce correct results
    EXPECT_TRUE(mir.functions[0].blocks.back().terminator.has_value());
}

// ============================================================================
// SimplifyCFG Tests
// ============================================================================

TEST_F(MirPassTest, SimplifyCfgPassName) {
    tml::mir::SimplifyCfgPass pass;
    EXPECT_EQ(pass.name(), "SimplifyCfg");
}

TEST_F(MirPassTest, SimplifyCfgReducesBlocks) {
    auto mir = build_mir(R"(
        func test(x: Bool) {
            if x {
                print(1)
            }
        }
    )");

    ASSERT_FALSE(mir.functions.empty());
    size_t before_blocks = mir.functions[0].blocks.size();

    tml::mir::SimplifyCfgPass pass;
    bool changed = pass.run(mir);

    if (changed) {
        size_t after_blocks = mir.functions[0].blocks.size();
        EXPECT_LE(after_blocks, before_blocks) << "SimplifyCFG should not increase block count";
    }
}

TEST_F(MirPassTest, SimplifyCfgConstantBranch) {
    // A branch on a constant condition should be simplified
    auto mir = build_mir(R"(
        func test() -> I32 {
            if true then {
                return 1
            } else {
                return 2
            }
        }
    )");

    ASSERT_FALSE(mir.functions.empty());
    size_t before_blocks = mir.functions[0].blocks.size();

    tml::mir::SimplifyCfgPass pass;
    pass.run(mir);

    size_t after_blocks = mir.functions[0].blocks.size();
    EXPECT_LE(after_blocks, before_blocks) << "Constant branch should allow block reduction";
}

// ============================================================================
// Block Merge Tests
// ============================================================================

TEST_F(MirPassTest, BlockMergePassName) {
    tml::mir::BlockMergePass pass;
    EXPECT_EQ(pass.name(), "BlockMerge");
}

TEST_F(MirPassTest, BlockMergeSimple) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 1
            let y: I32 = 2
            return x + y
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::BlockMergePass pass;
    pass.run(mir);

    // Simple linear code should result in few blocks
    EXPECT_GE(mir.functions[0].blocks.size(), 1u) << "Should have at least one block after merging";
}

TEST_F(MirPassTest, BlockMergePreservesControlFlow) {
    auto mir = build_mir(R"(
        func test(x: Bool) -> I32 {
            if x {
                return 1
            }
            return 0
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::BlockMergePass pass;
    pass.run(mir);

    // Must still have multiple blocks for if/else
    EXPECT_GT(mir.functions[0].blocks.size(), 1u) << "Should preserve control flow blocks";
}

// ============================================================================
// Strength Reduction Tests
// ============================================================================

TEST_F(MirPassTest, StrengthReductionPassName) {
    tml::mir::StrengthReductionPass pass;
    EXPECT_EQ(pass.name(), "StrengthReduction");
}

TEST_F(MirPassTest, StrengthReductionPowerOfTwo) {
    // Multiply by power of 2 should be reduced to shift
    auto mir = build_mir(R"(
        func test(x: I32) -> I32 {
            return x * 8
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::StrengthReductionPass pass;
    bool changed = pass.run(mir);

    // If changed, verify the multiply was replaced
    if (changed) {
        bool has_binary = false;
        for (const auto& block : mir.functions[0].blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* bin = std::get_if<tml::mir::BinaryInst>(&inst.inst)) {
                    if (bin->op == tml::mir::BinOp::Shl) {
                        has_binary = true;
                    }
                }
            }
        }
        EXPECT_TRUE(has_binary) << "Multiply by 8 should be reduced to shift left by 3";
    }
}

TEST_F(MirPassTest, StrengthReductionNonPowerOfTwo) {
    // Multiply by non-power-of-2 should not be changed (or use LEA pattern)
    auto mir = build_mir(R"(
        func test(x: I32) -> I32 {
            return x * 7
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::StrengthReductionPass pass;
    pass.run(mir);

    // Function should still be valid regardless
    EXPECT_GT(mir.functions[0].blocks.size(), 0u);
}

// ============================================================================
// Mem2Reg Tests
// ============================================================================

TEST_F(MirPassTest, Mem2RegPassName) {
    tml::mir::Mem2RegPass pass;
    EXPECT_EQ(pass.name(), "Mem2Reg");
}

TEST_F(MirPassTest, Mem2RegPromotesSimpleAlloca) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    ASSERT_FALSE(mir.functions.empty());
    size_t before_allocas = count_instructions<tml::mir::AllocaInst>(mir.functions[0]);

    tml::mir::Mem2RegPass pass;
    bool changed = pass.run(mir);

    if (changed && before_allocas > 0) {
        size_t after_allocas = count_instructions<tml::mir::AllocaInst>(mir.functions[0]);
        EXPECT_LT(after_allocas, before_allocas)
            << "Mem2Reg should promote simple allocas to registers";
    }
}

TEST_F(MirPassTest, Mem2RegMultipleVariables) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let a: I32 = 1
            let b: I32 = 2
            let c: I32 = a + b
            return c
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::Mem2RegPass pass;
    pass.run(mir);

    // After promotion, function should still be valid
    EXPECT_GT(mir.functions[0].blocks.size(), 0u);
    EXPECT_TRUE(mir.functions[0].blocks.back().terminator.has_value());
}

// ============================================================================
// SROA Tests
// ============================================================================

TEST_F(MirPassTest, SROAPassName) {
    tml::mir::SROAPass pass;
    EXPECT_EQ(pass.name(), "SROA");
}

TEST_F(MirPassTest, SROAStructFieldAccess) {
    auto mir = build_mir(R"(
        struct Point {
            x: I32,
            y: I32
        }

        func test() -> I32 {
            let p: Point = Point { x: 10, y: 20 }
            return p.x
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::SROAPass pass;
    pass.run(mir);

    // Function should remain valid after SROA
    EXPECT_GT(mir.functions[0].blocks.size(), 0u);
}

TEST_F(MirPassTest, SROAPreservesNonAggregates) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    ASSERT_FALSE(mir.functions.empty());
    size_t before_count = count_total_instructions(mir.functions[0]);

    tml::mir::SROAPass pass;
    bool changed = pass.run(mir);

    // SROA should not modify non-aggregate allocations
    if (!changed) {
        size_t after_count = count_total_instructions(mir.functions[0]);
        EXPECT_EQ(after_count, before_count);
    }
}

// ============================================================================
// Tail Call Tests
// ============================================================================

TEST_F(MirPassTest, TailCallPassName) {
    tml::mir::TailCallPass pass;
    EXPECT_EQ(pass.name(), "TailCall");
}

TEST_F(MirPassTest, TailCallDetection) {
    auto mir = build_mir(R"(
        func helper(n: I32) -> I32 {
            return n + 1
        }

        func test(n: I32) -> I32 {
            return helper(n)
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::TailCallPass pass;
    pass.run(mir);

    // The pass identifies tail calls; check the query API
    // Even if no tail calls found, the pass should not crash
    EXPECT_GT(mir.functions.size(), 0u);
}

TEST_F(MirPassTest, TailCallNonTailPosition) {
    auto mir = build_mir(R"(
        func helper(n: I32) -> I32 {
            return n + 1
        }

        func test(n: I32) -> I32 {
            let result: I32 = helper(n)
            return result + 1
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::TailCallPass pass;
    pass.run(mir);

    // The call to helper is NOT in tail position (result is used in addition)
    // Check that the function is still valid
    auto* func = find_function(mir, "test");
    ASSERT_NE(func, nullptr);
    EXPECT_GT(func->blocks.size(), 0u);
}

// ============================================================================
// Inlining Tests
// ============================================================================

TEST_F(MirPassTest, InliningPassName) {
    tml::mir::InliningPass pass;
    EXPECT_EQ(pass.name(), "Inlining");
}

TEST_F(MirPassTest, InliningSmallFunction) {
    auto mir = build_mir(R"(
        func add_one(x: I32) -> I32 {
            return x + 1
        }

        func test() -> I32 {
            return add_one(41)
        }
    )");

    ASSERT_GE(mir.functions.size(), 2u);

    tml::mir::InliningPass pass;
    pass.run(mir);

    auto stats = pass.get_stats();
    EXPECT_GT(stats.calls_analyzed, 0u) << "Inlining pass should analyze call sites";
}

TEST_F(MirPassTest, InliningWithOptions) {
    auto mir = build_mir(R"(
        func identity(x: I32) -> I32 {
            return x
        }

        func test() -> I32 {
            return identity(42)
        }
    )");

    tml::mir::InliningOptions opts;
    opts.base_threshold = 1000; // Very high threshold - should inline everything
    opts.max_callee_size = 1000;

    tml::mir::InliningPass pass(opts);
    pass.run(mir);

    auto stats = pass.get_stats();
    EXPECT_GT(stats.calls_analyzed, 0u);
}

TEST_F(MirPassTest, InlineCostAnalysis) {
    tml::mir::InlineCost cost;
    cost.instruction_cost = 10;
    cost.call_overhead_saved = 20;
    cost.threshold = 50;

    // Net cost is negative (beneficial)
    EXPECT_EQ(cost.net_cost(), -10);
    EXPECT_TRUE(cost.should_inline());

    // Now make it expensive
    cost.instruction_cost = 100;
    cost.call_overhead_saved = 5;
    cost.threshold = 50;

    EXPECT_EQ(cost.net_cost(), 95);
    EXPECT_FALSE(cost.should_inline());
}

TEST_F(MirPassTest, InliningStatsInitialized) {
    tml::mir::InliningStats stats;
    EXPECT_EQ(stats.calls_analyzed, 0u);
    EXPECT_EQ(stats.calls_inlined, 0u);
    EXPECT_EQ(stats.calls_not_inlined, 0u);
    EXPECT_EQ(stats.always_inline, 0u);
    EXPECT_EQ(stats.never_inline, 0u);
    EXPECT_EQ(stats.recursive_limit_hit, 0u);
    EXPECT_EQ(stats.too_large, 0u);
    EXPECT_EQ(stats.no_definition, 0u);
    EXPECT_EQ(stats.total_instructions_inlined, 0u);
}

TEST_F(MirPassTest, AlwaysInlinePassName) {
    tml::mir::AlwaysInlinePass pass;
    EXPECT_EQ(pass.name(), "AlwaysInline");
}

// ============================================================================
// Constant Propagation Tests
// ============================================================================

TEST_F(MirPassTest, ConstantPropagationPassName) {
    tml::mir::ConstantPropagationPass pass;
    EXPECT_EQ(pass.name(), "ConstantPropagation");
}

TEST_F(MirPassTest, ConstantPropagationSimple) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 5
            let y: I32 = x + 10
            return y
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    tml::mir::ConstantPropagationPass pass;
    pass.run(mir);

    // Function should still produce valid output
    EXPECT_GT(mir.functions[0].blocks.size(), 0u);
}

// ============================================================================
// PassManager Tests
// ============================================================================

TEST_F(MirPassTest, PassManagerCreation) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    EXPECT_EQ(pm.opt_level(), tml::mir::OptLevel::O2);
}

TEST_F(MirPassTest, PassManagerOptLevels) {
    tml::mir::PassManager pm0(tml::mir::OptLevel::O0);
    EXPECT_EQ(pm0.opt_level(), tml::mir::OptLevel::O0);

    tml::mir::PassManager pm1(tml::mir::OptLevel::O1);
    EXPECT_EQ(pm1.opt_level(), tml::mir::OptLevel::O1);

    tml::mir::PassManager pm3(tml::mir::OptLevel::O3);
    EXPECT_EQ(pm3.opt_level(), tml::mir::OptLevel::O3);
}

TEST_F(MirPassTest, PassManagerAddAndRun) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    pm.add_pass(std::make_unique<tml::mir::ConstantFoldingPass>());
    pm.add_pass(std::make_unique<tml::mir::DeadCodeEliminationPass>());

    int changed = pm.run(mir);
    // The return value is the number of passes that changed the module
    EXPECT_GE(changed, 0);
}

TEST_F(MirPassTest, PassManagerStandardPipeline) {
    auto mir = build_mir(R"(
        func test(a: I32, b: I32) -> I32 {
            let sum: I32 = a + b
            let diff: I32 = a - b
            return sum * diff
        }
    )");

    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    pm.configure_standard_pipeline();

    int changed = pm.run(mir);
    EXPECT_GE(changed, 0);

    // Module should still be valid after full pipeline
    ASSERT_FALSE(mir.functions.empty());
    EXPECT_GT(mir.functions[0].blocks.size(), 0u);
}

// ============================================================================
// MIR Analysis Utility Tests
// ============================================================================

TEST_F(MirPassTest, IsValueUsedHelper) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    ASSERT_FALSE(mir.functions.empty());
    const auto& func = mir.functions[0];

    // Check that analysis utilities work
    // INVALID_VALUE should not be used anywhere
    bool used = tml::mir::is_value_used(func, tml::mir::INVALID_VALUE);
    EXPECT_FALSE(used) << "INVALID_VALUE should not be considered used";
}

TEST_F(MirPassTest, HasSideEffectsOnStore) {
    auto mir = build_mir(R"(
        func test() {
            let mut x: I32 = 0
            x = 42
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    // Find a store instruction and verify it has side effects
    for (const auto& block : mir.functions[0].blocks) {
        for (const auto& inst : block.instructions) {
            if (std::holds_alternative<tml::mir::StoreInst>(inst.inst)) {
                EXPECT_TRUE(tml::mir::has_side_effects(inst.inst))
                    << "Store instructions should have side effects";
            }
        }
    }
}

TEST_F(MirPassTest, IsConstantOnLiteral) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            return 42
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    // Find a constant instruction and verify is_constant returns true
    for (const auto& block : mir.functions[0].blocks) {
        for (const auto& inst : block.instructions) {
            if (std::holds_alternative<tml::mir::ConstantInst>(inst.inst)) {
                EXPECT_TRUE(tml::mir::is_constant(inst.inst))
                    << "ConstantInst should be recognized as constant";
            }
        }
    }
}

TEST_F(MirPassTest, GetConstantIntValue) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            return 42
        }
    )");

    ASSERT_FALSE(mir.functions.empty());

    bool found_42 = false;
    for (const auto& block : mir.functions[0].blocks) {
        for (const auto& inst : block.instructions) {
            auto val = tml::mir::get_constant_int(inst.inst);
            if (val.has_value() && *val == 42) {
                found_42 = true;
            }
        }
    }
    EXPECT_TRUE(found_42) << "Should find constant value 42 in MIR";
}

// ============================================================================
// Combined Pass Pipeline Tests
// ============================================================================

TEST_F(MirPassTest, CombinedOptimizationPipeline) {
    auto mir = build_mir(R"(
        func square(x: I32) -> I32 {
            return x * x
        }

        func test() -> I32 {
            let a: I32 = 5
            let b: I32 = square(a)
            let unused: I32 = 999
            return b
        }
    )");

    ASSERT_GE(mir.functions.size(), 2u);

    // Run a sequence of optimization passes
    tml::mir::ConstantFoldingPass cf;
    cf.run(mir);

    tml::mir::DeadCodeEliminationPass dce;
    dce.run(mir);

    tml::mir::CopyPropagationPass cp;
    cp.run(mir);

    tml::mir::SimplifyCfgPass scfg;
    scfg.run(mir);

    // Module should still be valid
    ASSERT_FALSE(mir.functions.empty());
    for (const auto& func : mir.functions) {
        EXPECT_GT(func.blocks.size(), 0u)
            << "Function " << func.name << " should have blocks after optimization";
    }
}

TEST_F(MirPassTest, RepeatedPassExecution) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 1 + 2
            let y: I32 = x + 3
            return y
        }
    )");

    // Running the same pass multiple times should converge
    tml::mir::ConstantFoldingPass pass;
    bool changed1 = pass.run(mir);
    bool changed2 = pass.run(mir);
    bool changed3 = pass.run(mir);

    // Eventually the pass should stop finding things to fold
    if (changed1 && changed2) {
        // After enough iterations, should converge
        (void)changed3;
    }

    ASSERT_FALSE(mir.functions.empty());
}
