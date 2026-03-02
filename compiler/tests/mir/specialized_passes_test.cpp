// Tests for MIR specialized optimization passes:
// - TailCall, RVO, ConstructorFusion, DestructorHoist, BatchDestruction
// - MatchSimplify, AsyncLowering, Sinking, IPO, BuilderOpt, PGO, Vectorization
// - Peephole, Reassociate, CSE, Narrowing, ConstHoist, InstSimplify
// - AliasAnalysis, LoadStoreOpt, BoundsCheckElimination, MemoryLeakCheck

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/alias_analysis.hpp"
#include "mir/passes/async_lowering.hpp"
#include "mir/passes/batch_destruction.hpp"
#include "mir/passes/bounds_check_elimination.hpp"
#include "mir/passes/builder_opt.hpp"
#include "mir/passes/common_subexpression_elimination.hpp"
#include "mir/passes/const_hoist.hpp"
#include "mir/passes/constant_folding.hpp"
#include "mir/passes/constructor_fusion.hpp"
#include "mir/passes/dead_code_elimination.hpp"
#include "mir/passes/destructor_hoist.hpp"
#include "mir/passes/inst_simplify.hpp"
#include "mir/passes/ipo.hpp"
#include "mir/passes/load_store_opt.hpp"
#include "mir/passes/match_simplify.hpp"
#include "mir/passes/memory_leak_check.hpp"
#include "mir/passes/narrowing.hpp"
#include "mir/passes/peephole.hpp"
#include "mir/passes/pgo.hpp"
#include "mir/passes/reassociate.hpp"
#include "mir/passes/rvo.hpp"
#include "mir/passes/sinking.hpp"
#include "mir/passes/tail_call.hpp"
#include "mir/passes/vectorization.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class SpecializedPassesTest : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;
    std::unique_ptr<tml::types::TypeEnv> env_;

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
        env_ = std::make_unique<tml::types::TypeEnv>(
            std::move(std::get<tml::types::TypeEnv>(env_result)));
        tml::mir::MirBuilder builder(*env_);
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
// TailCall
// ============================================================================

TEST_F(SpecializedPassesTest, TailCallPassName) {
    tml::mir::TailCallPass pass;
    EXPECT_EQ(pass.name(), "TailCall");
}

TEST_F(SpecializedPassesTest, TailCallEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::TailCallPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(SpecializedPassesTest, TailCallRecursive) {
    auto mir = build_mir(R"(
        func count_down(n: I32) -> I32 {
            if n <= 0 {
                return 0
            }
            return count_down(n - 1)
        }
    )");

    tml::mir::TailCallPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// RVO
// ============================================================================

TEST_F(SpecializedPassesTest, RVOPassName) {
    tml::mir::RvoPass pass;
    EXPECT_EQ(pass.name(), "RVO");
}

TEST_F(SpecializedPassesTest, RVOEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::RvoPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// ConstructorFusion
// ============================================================================

TEST_F(SpecializedPassesTest, ConstructorFusionPassName) {
    auto mir = build_mir("func empty() { }");
    tml::mir::ConstructorFusionPass pass(*env_);
    EXPECT_EQ(pass.name(), "ConstructorFusion");
}

TEST_F(SpecializedPassesTest, ConstructorFusionEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::ConstructorFusionPass pass(*env_);
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// DestructorHoist
// ============================================================================

TEST_F(SpecializedPassesTest, DestructorHoistPassName) {
    auto mir = build_mir("func empty() { }");
    tml::mir::DestructorHoistPass pass(*env_);
    EXPECT_EQ(pass.name(), "DestructorHoist");
}

TEST_F(SpecializedPassesTest, DestructorHoistEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::DestructorHoistPass pass(*env_);
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// BatchDestruction
// ============================================================================

TEST_F(SpecializedPassesTest, BatchDestructionPassName) {
    auto mir = build_mir("func empty() { }");
    tml::mir::BatchDestructionPass pass(*env_);
    EXPECT_EQ(pass.name(), "BatchDestruction");
}

TEST_F(SpecializedPassesTest, BatchDestructionEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::BatchDestructionPass pass(*env_);
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// MatchSimplify
// ============================================================================

TEST_F(SpecializedPassesTest, MatchSimplifyPassName) {
    tml::mir::MatchSimplifyPass pass;
    EXPECT_EQ(pass.name(), "MatchSimplify");
}

TEST_F(SpecializedPassesTest, MatchSimplifyEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::MatchSimplifyPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// AsyncLowering
// ============================================================================

TEST_F(SpecializedPassesTest, AsyncLoweringPassName) {
    tml::mir::AsyncLoweringPass pass;
    EXPECT_EQ(pass.name(), "async-lowering");
}

TEST_F(SpecializedPassesTest, AsyncLoweringEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::AsyncLoweringPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// Sinking
// ============================================================================

TEST_F(SpecializedPassesTest, SinkingPassName) {
    tml::mir::SinkingPass pass;
    EXPECT_EQ(pass.name(), "Sinking");
}

TEST_F(SpecializedPassesTest, SinkingEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::SinkingPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// IPO
// ============================================================================

TEST_F(SpecializedPassesTest, IPOPassName) {
    tml::mir::IpoPass pass;
    EXPECT_EQ(pass.name(), "IPO");
}

TEST_F(SpecializedPassesTest, IPOEmptyModule) {
    auto mir = build_mir("func main() { }");
    tml::mir::IpoPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// BuilderOpt
// ============================================================================

TEST_F(SpecializedPassesTest, BuilderOptPassName) {
    tml::mir::BuilderOptPass pass;
    EXPECT_EQ(pass.name(), "BuilderOpt");
}

TEST_F(SpecializedPassesTest, BuilderOptEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::BuilderOptPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// PGO
// ============================================================================

TEST_F(SpecializedPassesTest, PGOPassName) {
    tml::mir::ProfileData profile;
    tml::mir::PgoPass pass(profile);
    EXPECT_EQ(pass.name(), "PGO");
}

TEST_F(SpecializedPassesTest, PGOEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::ProfileData profile;
    tml::mir::PgoPass pass(profile);
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// Vectorization
// ============================================================================

TEST_F(SpecializedPassesTest, VectorizationPassName) {
    tml::mir::VectorizationPass pass;
    EXPECT_EQ(pass.name(), "Vectorization");
}

TEST_F(SpecializedPassesTest, VectorizationEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::VectorizationPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// Peephole
// ============================================================================

TEST_F(SpecializedPassesTest, PeepholePassName) {
    tml::mir::PeepholePass pass;
    EXPECT_EQ(pass.name(), "Peephole");
}

TEST_F(SpecializedPassesTest, PeepholeEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::PeepholePass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(SpecializedPassesTest, PeepholeSimpleArithmetic) {
    auto mir = build_mir(R"(
        func compute() -> I32 {
            let a: I32 = 5
            return a + 0
        }
    )");

    tml::mir::PeepholePass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// Reassociate
// ============================================================================

TEST_F(SpecializedPassesTest, ReassociatePassName) {
    tml::mir::ReassociatePass pass;
    EXPECT_EQ(pass.name(), "Reassociate");
}

TEST_F(SpecializedPassesTest, ReassociateEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::ReassociatePass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// CSE
// ============================================================================

TEST_F(SpecializedPassesTest, CSEPassName) {
    tml::mir::CommonSubexpressionEliminationPass pass;
    EXPECT_EQ(pass.name(), "CommonSubexpressionElimination");
}

TEST_F(SpecializedPassesTest, CSEEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::CommonSubexpressionEliminationPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

TEST_F(SpecializedPassesTest, CSEDuplicateExpression) {
    auto mir = build_mir(R"(
        func main(a: I32, b: I32) -> I32 {
            let x: I32 = a + b
            let y: I32 = a + b
            return x + y
        }
    )");

    tml::mir::CommonSubexpressionEliminationPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

// ============================================================================
// Narrowing
// ============================================================================

TEST_F(SpecializedPassesTest, NarrowingPassName) {
    tml::mir::NarrowingPass pass;
    EXPECT_EQ(pass.name(), "Narrowing");
}

TEST_F(SpecializedPassesTest, NarrowingEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::NarrowingPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// ConstHoist
// ============================================================================

TEST_F(SpecializedPassesTest, ConstHoistPassName) {
    tml::mir::ConstantHoistPass pass;
    EXPECT_EQ(pass.name(), "ConstHoist");
}

TEST_F(SpecializedPassesTest, ConstHoistEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::ConstantHoistPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// InstSimplify
// ============================================================================

TEST_F(SpecializedPassesTest, InstSimplifyPassName) {
    tml::mir::InstSimplifyPass pass;
    EXPECT_EQ(pass.name(), "InstSimplify");
}

TEST_F(SpecializedPassesTest, InstSimplifyEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::InstSimplifyPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// AliasAnalysis
// ============================================================================

TEST_F(SpecializedPassesTest, AliasAnalysisPassName) {
    tml::mir::AliasAnalysisPass pass;
    EXPECT_EQ(pass.name(), "AliasAnalysis");
}

TEST_F(SpecializedPassesTest, AliasAnalysisEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::AliasAnalysisPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// LoadStoreOpt
// ============================================================================

TEST_F(SpecializedPassesTest, LoadStoreOptPassName) {
    tml::mir::LoadStoreOptPass pass;
    EXPECT_EQ(pass.name(), "LoadStoreOpt");
}

TEST_F(SpecializedPassesTest, LoadStoreOptEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::LoadStoreOptPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// BoundsCheckElimination
// ============================================================================

TEST_F(SpecializedPassesTest, BoundsCheckElimPassName) {
    tml::mir::BoundsCheckEliminationPass pass;
    EXPECT_EQ(pass.name(), "BoundsCheckElimination");
}

TEST_F(SpecializedPassesTest, BoundsCheckElimEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::BoundsCheckEliminationPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// MemoryLeakCheck
// ============================================================================

TEST_F(SpecializedPassesTest, MemoryLeakCheckPassName) {
    tml::mir::MemoryLeakCheckPass pass;
    EXPECT_EQ(pass.name(), "memory-leak-check");
}

TEST_F(SpecializedPassesTest, MemoryLeakCheckEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::MemoryLeakCheckPass pass;
    bool changed = pass.run(mir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// PassManager Integration
// ============================================================================

TEST_F(SpecializedPassesTest, PassManagerBasic) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    pm.configure_standard_pipeline();

    auto mir = build_mir(R"(
        func main() -> I32 {
            let x: I32 = 2 + 3
            return x
        }
    )");

    int passes_changed = pm.run(mir);
    EXPECT_GE(passes_changed, 0);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(SpecializedPassesTest, PassManagerO0NoOptimization) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O0);
    pm.configure_standard_pipeline();

    EXPECT_EQ(pm.opt_level(), tml::mir::OptLevel::O0);
}

TEST_F(SpecializedPassesTest, PassManagerAddCustomPass) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    pm.add_pass(std::make_unique<tml::mir::ConstantFoldingPass>());
    pm.add_pass(std::make_unique<tml::mir::DeadCodeEliminationPass>());

    auto mir = build_mir(R"(
        func main() -> I32 {
            return 1 + 2
        }
    )");

    int passes_changed = pm.run(mir);
    EXPECT_GE(passes_changed, 0);
}

TEST_F(SpecializedPassesTest, PassManagerWithPipelineDir) {
    tml::mir::PassManager pm(tml::mir::OptLevel::O2);
    // Setting empty dir should be fine
    pm.set_pipeline_dir("", "");
    pm.configure_standard_pipeline();

    auto mir = build_mir("func main() { }");
    pm.run(mir);
}
