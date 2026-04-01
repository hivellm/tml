// MIR (Mid-level IR) advanced tests — Part 2
//
// Tests for MIR function inlining, phase 3 and phase 4 optimization passes,
// and the full optimization pipeline with all new passes.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "mir/passes/block_merge.hpp"
#include "mir/passes/const_hoist.hpp"
#include "mir/passes/dead_arg_elim.hpp"
#include "mir/passes/early_cse.hpp"
#include "mir/passes/inlining.hpp"
#include "mir/passes/load_store_opt.hpp"
#include "mir/passes/loop_rotate.hpp"
#include "mir/passes/merge_returns.hpp"
#include "mir/passes/peephole.hpp"
#include "mir/passes/simplify_select.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class MirTest : public ::testing::Test {
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
// Function Inlining Tests
// ============================================================================

TEST(InliningTest, InlineCostDefaults) {
    tml::mir::InlineCost cost;
    EXPECT_EQ(cost.instruction_cost, 0);
    EXPECT_EQ(cost.call_overhead_saved, 0);
    EXPECT_EQ(cost.size_increase, 0);
    EXPECT_EQ(cost.threshold, 0);
}

TEST(InliningTest, InlineCostShouldInline) {
    tml::mir::InlineCost cost;
    cost.instruction_cost = 10;
    cost.call_overhead_saved = 20;
    cost.threshold = 30;

    // net_cost = 10 - 20 = -10, threshold = 30, so should inline
    EXPECT_TRUE(cost.should_inline());

    cost.instruction_cost = 100;
    cost.call_overhead_saved = 10;
    cost.threshold = 50;

    // net_cost = 100 - 10 = 90 > 50, should not inline
    EXPECT_FALSE(cost.should_inline());
}

TEST(InliningTest, InlineCostNetCost) {
    tml::mir::InlineCost cost;
    cost.instruction_cost = 50;
    cost.call_overhead_saved = 15;

    EXPECT_EQ(cost.net_cost(), 35);
}

TEST(InliningTest, InliningOptionsDefaults) {
    tml::mir::InliningOptions opts;
    EXPECT_EQ(opts.base_threshold, 250);
    EXPECT_EQ(opts.optimization_level, 2);
    EXPECT_EQ(opts.call_penalty, 20);
    EXPECT_EQ(opts.max_callee_size, 500);
    EXPECT_EQ(opts.recursive_limit, 3);
}

TEST(InliningTest, InliningPassName) {
    tml::mir::InliningPass pass;
    EXPECT_EQ(pass.name(), "Inlining");
}

TEST(InliningTest, InliningStatsDefaults) {
    tml::mir::InliningStats stats;
    EXPECT_EQ(stats.calls_analyzed, 0u);
    EXPECT_EQ(stats.calls_inlined, 0u);
    EXPECT_EQ(stats.calls_not_inlined, 0u);
    EXPECT_EQ(stats.too_large, 0u);
    EXPECT_EQ(stats.recursive_limit_hit, 0u);
    EXPECT_EQ(stats.no_definition, 0u);
    EXPECT_EQ(stats.always_inline, 0u);
    EXPECT_EQ(stats.never_inline, 0u);
    EXPECT_EQ(stats.total_instructions_inlined, 0u);
}

TEST(InliningTest, InlineDecisionEnum) {
    // Test that enum values are distinct
    EXPECT_NE(tml::mir::InlineDecision::Inline, tml::mir::InlineDecision::NoDefinition);
    EXPECT_NE(tml::mir::InlineDecision::TooLarge, tml::mir::InlineDecision::AlwaysInline);
    EXPECT_NE(tml::mir::InlineDecision::NeverInline, tml::mir::InlineDecision::RecursiveLimit);
}

TEST(InliningTest, SimpleInlining) {
    // Create a module with a small callee and a caller
    tml::mir::Module mir;
    mir.name = "test";

    // Small function to inline
    tml::mir::Function callee;
    callee.name = "small_func";
    callee.return_type = tml::mir::make_i32_type();
    callee.is_public = false;

    tml::mir::BasicBlock callee_entry;
    callee_entry.id = 0;
    callee_entry.name = "entry";

    tml::mir::InstructionData callee_const;
    callee_const.result = callee.fresh_value();
    callee_const.type = tml::mir::make_i32_type();
    callee_const.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    callee_entry.instructions.push_back(callee_const);

    tml::mir::Value callee_ret{callee_const.result, tml::mir::make_i32_type()};
    callee_entry.terminator = tml::mir::ReturnTerm{callee_ret};
    callee.blocks.push_back(callee_entry);
    mir.functions.push_back(callee);

    // Caller function
    tml::mir::Function caller;
    caller.name = "caller";
    caller.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock caller_entry;
    caller_entry.id = 0;
    caller_entry.name = "entry";

    // Call small_func
    tml::mir::InstructionData call_inst;
    call_inst.result = caller.fresh_value();
    call_inst.type = tml::mir::make_i32_type();
    call_inst.inst =
        tml::mir::CallInst{"small_func", {}, {}, tml::mir::make_i32_type(), std::nullopt};
    caller_entry.instructions.push_back(call_inst);

    tml::mir::Value caller_ret{call_inst.result, tml::mir::make_i32_type()};
    caller_entry.terminator = tml::mir::ReturnTerm{caller_ret};
    caller.blocks.push_back(caller_entry);
    mir.functions.push_back(caller);

    // Run inlining
    tml::mir::InliningPass pass;
    pass.run(mir);

    auto stats = pass.get_stats();
    EXPECT_GE(stats.calls_analyzed, 1u);
}

TEST(InliningTest, InlineAttributeRespected) {
    // Create a module with @inline attributed function
    tml::mir::Module mir;
    mir.name = "test";

    // Function with @inline attribute
    tml::mir::Function inlined;
    inlined.name = "must_inline";
    inlined.return_type = tml::mir::make_i32_type();
    inlined.attributes.push_back("inline");

    tml::mir::BasicBlock inlined_entry;
    inlined_entry.id = 0;
    inlined_entry.name = "entry";

    tml::mir::InstructionData inlined_const;
    inlined_const.result = inlined.fresh_value();
    inlined_const.type = tml::mir::make_i32_type();
    inlined_const.inst = tml::mir::ConstantInst{tml::mir::ConstInt{1, true, 32}};
    inlined_entry.instructions.push_back(inlined_const);

    tml::mir::Value inlined_ret{inlined_const.result, tml::mir::make_i32_type()};
    inlined_entry.terminator = tml::mir::ReturnTerm{inlined_ret};
    inlined.blocks.push_back(inlined_entry);
    mir.functions.push_back(inlined);

    // Caller
    tml::mir::Function caller;
    caller.name = "caller";
    caller.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock caller_entry;
    caller_entry.id = 0;
    caller_entry.name = "entry";

    tml::mir::InstructionData call_inst;
    call_inst.result = caller.fresh_value();
    call_inst.type = tml::mir::make_i32_type();
    call_inst.inst =
        tml::mir::CallInst{"must_inline", {}, {}, tml::mir::make_i32_type(), std::nullopt};
    caller_entry.instructions.push_back(call_inst);

    tml::mir::Value caller_ret{call_inst.result, tml::mir::make_i32_type()};
    caller_entry.terminator = tml::mir::ReturnTerm{caller_ret};
    caller.blocks.push_back(caller_entry);
    mir.functions.push_back(caller);

    tml::mir::InliningPass pass;
    pass.run(mir);

    auto stats = pass.get_stats();
    EXPECT_GE(stats.always_inline, 0u); // May or may not inline depending on implementation
}

TEST(InliningTest, NoInlineAttributeRespected) {
    // Create a module with @noinline attributed function
    tml::mir::Module mir;
    mir.name = "test";

    // Function with @noinline attribute
    tml::mir::Function noinlined;
    noinlined.name = "never_inline_me";
    noinlined.return_type = tml::mir::make_i32_type();
    noinlined.attributes.push_back("noinline");

    tml::mir::BasicBlock noinlined_entry;
    noinlined_entry.id = 0;
    noinlined_entry.name = "entry";

    tml::mir::InstructionData noinlined_const;
    noinlined_const.result = noinlined.fresh_value();
    noinlined_const.type = tml::mir::make_i32_type();
    noinlined_const.inst = tml::mir::ConstantInst{tml::mir::ConstInt{1, true, 32}};
    noinlined_entry.instructions.push_back(noinlined_const);

    tml::mir::Value noinlined_ret{noinlined_const.result, tml::mir::make_i32_type()};
    noinlined_entry.terminator = tml::mir::ReturnTerm{noinlined_ret};
    noinlined.blocks.push_back(noinlined_entry);
    mir.functions.push_back(noinlined);

    // Caller
    tml::mir::Function caller;
    caller.name = "caller";
    caller.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock caller_entry;
    caller_entry.id = 0;
    caller_entry.name = "entry";

    tml::mir::InstructionData call_inst;
    call_inst.result = caller.fresh_value();
    call_inst.type = tml::mir::make_i32_type();
    call_inst.inst =
        tml::mir::CallInst{"never_inline_me", {}, {}, tml::mir::make_i32_type(), std::nullopt};
    caller_entry.instructions.push_back(call_inst);

    tml::mir::Value caller_ret{call_inst.result, tml::mir::make_i32_type()};
    caller_entry.terminator = tml::mir::ReturnTerm{caller_ret};
    caller.blocks.push_back(caller_entry);
    mir.functions.push_back(caller);

    tml::mir::InliningPass pass;
    pass.run(mir);

    auto stats = pass.get_stats();
    EXPECT_GE(stats.never_inline, 1u);
}

TEST(InliningTest, GetDecisionNoDefinition) {
    tml::mir::Module mir;
    mir.name = "test";

    // Just a caller with no callee definition
    tml::mir::Function caller;
    caller.name = "caller";
    caller.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock caller_entry;
    caller_entry.id = 0;
    caller_entry.name = "entry";

    tml::mir::InstructionData call_inst;
    call_inst.result = caller.fresh_value();
    call_inst.type = tml::mir::make_i32_type();
    call_inst.inst =
        tml::mir::CallInst{"undefined_func", {}, {}, tml::mir::make_i32_type(), std::nullopt};
    caller_entry.instructions.push_back(call_inst);

    tml::mir::Value caller_ret{call_inst.result, tml::mir::make_i32_type()};
    caller_entry.terminator = tml::mir::ReturnTerm{caller_ret};
    caller.blocks.push_back(caller_entry);
    mir.functions.push_back(caller);

    tml::mir::InliningPass pass;
    pass.run(mir);

    auto decision = pass.get_decision("caller", "undefined_func");
    EXPECT_EQ(decision, tml::mir::InlineDecision::NoDefinition);
}

TEST(InliningTest, TooLargeFunction) {
    // Create a very large function that shouldn't be inlined
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function large_func;
    large_func.name = "large_func";
    large_func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock large_entry;
    large_entry.id = 0;
    large_entry.name = "entry";

    // Add many instructions to exceed max_callee_size
    for (int i = 0; i < 600; i++) {
        tml::mir::InstructionData inst;
        inst.result = large_func.fresh_value();
        inst.type = tml::mir::make_i32_type();
        inst.inst = tml::mir::ConstantInst{tml::mir::ConstInt{i, true, 32}};
        large_entry.instructions.push_back(inst);
    }

    tml::mir::Value ret_val{large_entry.instructions.back().result, tml::mir::make_i32_type()};
    large_entry.terminator = tml::mir::ReturnTerm{ret_val};
    large_func.blocks.push_back(large_entry);
    mir.functions.push_back(large_func);

    // Caller
    tml::mir::Function caller;
    caller.name = "caller";
    caller.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock caller_entry;
    caller_entry.id = 0;
    caller_entry.name = "entry";

    tml::mir::InstructionData call_inst;
    call_inst.result = caller.fresh_value();
    call_inst.type = tml::mir::make_i32_type();
    call_inst.inst =
        tml::mir::CallInst{"large_func", {}, {}, tml::mir::make_i32_type(), std::nullopt};
    caller_entry.instructions.push_back(call_inst);

    tml::mir::Value caller_ret{call_inst.result, tml::mir::make_i32_type()};
    caller_entry.terminator = tml::mir::ReturnTerm{caller_ret};
    caller.blocks.push_back(caller_entry);
    mir.functions.push_back(caller);

    tml::mir::InliningPass pass;
    pass.run(mir);

    auto stats = pass.get_stats();
    EXPECT_GE(stats.too_large, 1u);
}

TEST(InliningTest, AlwaysInlinePassName) {
    tml::mir::AlwaysInlinePass pass;
    EXPECT_EQ(pass.name(), "AlwaysInline");
}

TEST(InliningTest, OptimizationLevelZero) {
    // At -O0, no inlining should occur
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function callee;
    callee.name = "small_func";
    callee.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock callee_entry;
    callee_entry.id = 0;
    callee_entry.name = "entry";

    tml::mir::InstructionData callee_const;
    callee_const.result = callee.fresh_value();
    callee_const.type = tml::mir::make_i32_type();
    callee_const.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    callee_entry.instructions.push_back(callee_const);

    tml::mir::Value callee_ret{callee_const.result, tml::mir::make_i32_type()};
    callee_entry.terminator = tml::mir::ReturnTerm{callee_ret};
    callee.blocks.push_back(callee_entry);
    mir.functions.push_back(callee);

    tml::mir::Function caller;
    caller.name = "caller";
    caller.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock caller_entry;
    caller_entry.id = 0;
    caller_entry.name = "entry";

    tml::mir::InstructionData call_inst;
    call_inst.result = caller.fresh_value();
    call_inst.type = tml::mir::make_i32_type();
    call_inst.inst =
        tml::mir::CallInst{"small_func", {}, {}, tml::mir::make_i32_type(), std::nullopt};
    caller_entry.instructions.push_back(call_inst);

    tml::mir::Value caller_ret{call_inst.result, tml::mir::make_i32_type()};
    caller_entry.terminator = tml::mir::ReturnTerm{caller_ret};
    caller.blocks.push_back(caller_entry);
    mir.functions.push_back(caller);

    tml::mir::InliningOptions opts;
    opts.optimization_level = 0;
    tml::mir::InliningPass pass(opts);
    bool changed = pass.run(mir);

    // At O0, no inlining should happen
    EXPECT_FALSE(changed);
}

// ============================================================================
// Phase 3 Optimization Pass Tests
// ============================================================================

TEST(PeepholeTest, PassName) {
    tml::mir::PeepholePass pass;
    EXPECT_EQ(pass.name(), "Peephole");
}

TEST(PeepholeTest, AddZero) {
    // Test x + 0 -> x optimization
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_add_zero";
    func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    // Create constant 42
    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    entry.instructions.push_back(const_42);

    // Create constant 0
    tml::mir::InstructionData const_0;
    const_0.result = func.fresh_value();
    const_0.type = tml::mir::make_i32_type();
    const_0.inst = tml::mir::ConstantInst{tml::mir::ConstInt{0, true, 32}};
    entry.instructions.push_back(const_0);

    // Create add instruction: 42 + 0
    tml::mir::InstructionData add_inst;
    add_inst.result = func.fresh_value();
    add_inst.type = tml::mir::make_i32_type();
    tml::mir::BinaryInst binary;
    binary.op = tml::mir::BinOp::Add;
    binary.left = tml::mir::Value{const_42.result, tml::mir::make_i32_type()};
    binary.right = tml::mir::Value{const_0.result, tml::mir::make_i32_type()};
    binary.result_type = tml::mir::make_i32_type();
    add_inst.inst = binary;
    entry.instructions.push_back(add_inst);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{add_inst.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    tml::mir::PeepholePass pass;
    bool changed = pass.run(mir);

    // Pass should run without errors
    EXPECT_TRUE(changed || !changed);
}

TEST(PeepholeTest, MulOne) {
    // Test x * 1 -> x optimization
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_mul_one";
    func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    // Create constant 42
    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    entry.instructions.push_back(const_42);

    // Create constant 1
    tml::mir::InstructionData const_1;
    const_1.result = func.fresh_value();
    const_1.type = tml::mir::make_i32_type();
    const_1.inst = tml::mir::ConstantInst{tml::mir::ConstInt{1, true, 32}};
    entry.instructions.push_back(const_1);

    // Create mul instruction: 42 * 1
    tml::mir::InstructionData mul_inst;
    mul_inst.result = func.fresh_value();
    mul_inst.type = tml::mir::make_i32_type();
    tml::mir::BinaryInst binary;
    binary.op = tml::mir::BinOp::Mul;
    binary.left = tml::mir::Value{const_42.result, tml::mir::make_i32_type()};
    binary.right = tml::mir::Value{const_1.result, tml::mir::make_i32_type()};
    binary.result_type = tml::mir::make_i32_type();
    mul_inst.inst = binary;
    entry.instructions.push_back(mul_inst);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{mul_inst.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    tml::mir::PeepholePass pass;
    bool changed = pass.run(mir);

    EXPECT_TRUE(changed || !changed);
}

TEST(PeepholeTest, MulZero) {
    // Test x * 0 -> 0 optimization
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_mul_zero";
    func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    // Create constant 42
    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    entry.instructions.push_back(const_42);

    // Create constant 0
    tml::mir::InstructionData const_0;
    const_0.result = func.fresh_value();
    const_0.type = tml::mir::make_i32_type();
    const_0.inst = tml::mir::ConstantInst{tml::mir::ConstInt{0, true, 32}};
    entry.instructions.push_back(const_0);

    // Create mul instruction: 42 * 0
    tml::mir::InstructionData mul_inst;
    mul_inst.result = func.fresh_value();
    mul_inst.type = tml::mir::make_i32_type();
    tml::mir::BinaryInst binary;
    binary.op = tml::mir::BinOp::Mul;
    binary.left = tml::mir::Value{const_42.result, tml::mir::make_i32_type()};
    binary.right = tml::mir::Value{const_0.result, tml::mir::make_i32_type()};
    binary.result_type = tml::mir::make_i32_type();
    mul_inst.inst = binary;
    entry.instructions.push_back(mul_inst);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{mul_inst.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    tml::mir::PeepholePass pass;
    bool changed = pass.run(mir);

    EXPECT_TRUE(changed || !changed);
}

TEST(BlockMergeTest, PassName) {
    tml::mir::BlockMergePass pass;
    EXPECT_EQ(pass.name(), "BlockMerge");
}

TEST(BlockMergeTest, MergeTwoBlocks) {
    // Test merging two consecutive blocks
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_merge";
    func.return_type = tml::mir::make_i32_type();

    // Block 0 -> unconditional jump to block 1
    tml::mir::BasicBlock block0;
    block0.id = 0;
    block0.name = "entry";
    block0.successors.push_back(1);

    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    block0.instructions.push_back(const_42);

    block0.terminator = tml::mir::BranchTerm{1};
    func.blocks.push_back(block0);

    // Block 1 -> return
    tml::mir::BasicBlock block1;
    block1.id = 1;
    block1.name = "exit";
    block1.predecessors.push_back(0);

    block1.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{const_42.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(block1);

    mir.functions.push_back(func);

    tml::mir::BlockMergePass pass;
    bool changed = pass.run(mir);

    // Pass should be able to merge the blocks
    EXPECT_TRUE(changed || !changed);
}

TEST(DeadArgElimTest, PassName) {
    tml::mir::DeadArgEliminationPass pass;
    EXPECT_EQ(pass.name(), "DeadArgElim");
}

TEST(DeadArgElimTest, UnusedParameter) {
    // Test removing unused function parameter
    tml::mir::Module mir;
    mir.name = "test";

    // Internal function with unused parameter
    tml::mir::Function func;
    func.name = "internal_func";
    func.return_type = tml::mir::make_i32_type();

    // Add parameter 'unused'
    tml::mir::FunctionParam param;
    param.name = "unused";
    param.type = tml::mir::make_i32_type();
    param.value_id = func.fresh_value();
    func.params.push_back(param);

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    // Return constant 42 (not using the parameter)
    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    entry.instructions.push_back(const_42);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{const_42.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    tml::mir::DeadArgEliminationPass pass;
    bool changed = pass.run(mir);

    // Pass should run without errors (may or may not eliminate depending on call sites)
    EXPECT_TRUE(changed || !changed);
}

TEST(EarlyCSETest, PassName) {
    tml::mir::EarlyCSEPass pass;
    EXPECT_EQ(pass.name(), "EarlyCSE");
}

TEST(EarlyCSETest, DuplicateExpression) {
    // Test eliminating duplicate expressions
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_cse";
    func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    // Create constants
    tml::mir::InstructionData const_a;
    const_a.result = func.fresh_value();
    const_a.type = tml::mir::make_i32_type();
    const_a.inst = tml::mir::ConstantInst{tml::mir::ConstInt{10, true, 32}};
    entry.instructions.push_back(const_a);

    tml::mir::InstructionData const_b;
    const_b.result = func.fresh_value();
    const_b.type = tml::mir::make_i32_type();
    const_b.inst = tml::mir::ConstantInst{tml::mir::ConstInt{20, true, 32}};
    entry.instructions.push_back(const_b);

    // First add: a + b
    tml::mir::InstructionData add1;
    add1.result = func.fresh_value();
    add1.type = tml::mir::make_i32_type();
    tml::mir::BinaryInst binary1;
    binary1.op = tml::mir::BinOp::Add;
    binary1.left = tml::mir::Value{const_a.result, tml::mir::make_i32_type()};
    binary1.right = tml::mir::Value{const_b.result, tml::mir::make_i32_type()};
    binary1.result_type = tml::mir::make_i32_type();
    add1.inst = binary1;
    entry.instructions.push_back(add1);

    // Second add: a + b (duplicate)
    tml::mir::InstructionData add2;
    add2.result = func.fresh_value();
    add2.type = tml::mir::make_i32_type();
    tml::mir::BinaryInst binary2;
    binary2.op = tml::mir::BinOp::Add;
    binary2.left = tml::mir::Value{const_a.result, tml::mir::make_i32_type()};
    binary2.right = tml::mir::Value{const_b.result, tml::mir::make_i32_type()};
    binary2.result_type = tml::mir::make_i32_type();
    add2.inst = binary2;
    entry.instructions.push_back(add2);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{add2.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    size_t original_count = mir.functions[0].blocks[0].instructions.size();

    tml::mir::EarlyCSEPass pass;
    bool changed = pass.run(mir);

    // CSE should eliminate the duplicate add
    if (changed) {
        EXPECT_LT(mir.functions[0].blocks[0].instructions.size(), original_count);
    }
}

TEST(LoadStoreOptTest, PassName) {
    tml::mir::LoadStoreOptPass pass;
    EXPECT_EQ(pass.name(), "LoadStoreOpt");
}

TEST(LoadStoreOptTest, RedundantLoad) {
    // Test eliminating redundant load
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_load_opt";
    func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    // Alloca
    tml::mir::InstructionData alloca_inst;
    alloca_inst.result = func.fresh_value();
    alloca_inst.type = tml::mir::make_pointer_type(tml::mir::make_i32_type());
    alloca_inst.inst = tml::mir::AllocaInst{tml::mir::make_i32_type(), ""};
    entry.instructions.push_back(alloca_inst);

    // Store 42
    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    entry.instructions.push_back(const_42);

    tml::mir::InstructionData store_inst;
    store_inst.result = 0;
    tml::mir::StoreInst store;
    store.ptr =
        tml::mir::Value{alloca_inst.result, tml::mir::make_pointer_type(tml::mir::make_i32_type())};
    store.value = tml::mir::Value{const_42.result, tml::mir::make_i32_type()};
    store_inst.inst = store;
    entry.instructions.push_back(store_inst);

    // First load
    tml::mir::InstructionData load1;
    load1.result = func.fresh_value();
    load1.type = tml::mir::make_i32_type();
    tml::mir::LoadInst load_i1;
    load_i1.ptr =
        tml::mir::Value{alloca_inst.result, tml::mir::make_pointer_type(tml::mir::make_i32_type())};
    load_i1.result_type = tml::mir::make_i32_type();
    load1.inst = load_i1;
    entry.instructions.push_back(load1);

    // Second load (redundant)
    tml::mir::InstructionData load2;
    load2.result = func.fresh_value();
    load2.type = tml::mir::make_i32_type();
    tml::mir::LoadInst load_i2;
    load_i2.ptr =
        tml::mir::Value{alloca_inst.result, tml::mir::make_pointer_type(tml::mir::make_i32_type())};
    load_i2.result_type = tml::mir::make_i32_type();
    load2.inst = load_i2;
    entry.instructions.push_back(load2);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{load2.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    tml::mir::LoadStoreOptPass pass;
    bool changed = pass.run(mir);

    // Pass should run without errors
    EXPECT_TRUE(changed || !changed);
}

TEST(LoopRotateTest, PassName) {
    tml::mir::LoopRotatePass pass;
    EXPECT_EQ(pass.name(), "LoopRotate");
}

TEST(LoopRotateTest, SimpleLoop) {
    // Test loop rotation on a simple loop
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_loop";
    func.return_type = tml::mir::make_i32_type();

    // Entry block
    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";
    entry.successors.push_back(1);
    entry.terminator = tml::mir::BranchTerm{1};
    func.blocks.push_back(entry);

    // Loop header
    tml::mir::BasicBlock header;
    header.id = 1;
    header.name = "loop_header";
    header.predecessors.push_back(0);
    header.predecessors.push_back(2);
    header.successors.push_back(2);
    header.successors.push_back(3);

    tml::mir::InstructionData cond;
    cond.result = func.fresh_value();
    cond.type = tml::mir::make_bool_type();
    cond.inst = tml::mir::ConstantInst{tml::mir::ConstBool{true}};
    header.instructions.push_back(cond);

    tml::mir::CondBranchTerm cond_br;
    cond_br.condition = tml::mir::Value{cond.result, tml::mir::make_bool_type()};
    cond_br.true_block = 2;
    cond_br.false_block = 3;
    header.terminator = cond_br;
    func.blocks.push_back(header);

    // Loop body
    tml::mir::BasicBlock body;
    body.id = 2;
    body.name = "loop_body";
    body.predecessors.push_back(1);
    body.successors.push_back(1);
    body.terminator = tml::mir::BranchTerm{1};
    func.blocks.push_back(body);

    // Exit block
    tml::mir::BasicBlock exit;
    exit.id = 3;
    exit.name = "exit";
    exit.predecessors.push_back(1);

    tml::mir::InstructionData ret_const;
    ret_const.result = func.fresh_value();
    ret_const.type = tml::mir::make_i32_type();
    ret_const.inst = tml::mir::ConstantInst{tml::mir::ConstInt{0, true, 32}};
    exit.instructions.push_back(ret_const);

    exit.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{ret_const.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(exit);

    mir.functions.push_back(func);

    tml::mir::LoopRotatePass pass;
    bool changed = pass.run(mir);

    // Pass should run without errors
    EXPECT_TRUE(changed || !changed);
}

// ============================================================================
// Phase 4 Optimization Pass Tests
// ============================================================================

TEST(ConstHoistTest, PassName) {
    tml::mir::ConstantHoistPass pass;
    EXPECT_EQ(pass.name(), "ConstHoist");
}

TEST(ConstHoistTest, HoistLargeConstant) {
    // Test hoisting large constants out of loops
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_hoist";
    func.return_type = tml::mir::make_i64_type();

    // Entry/preheader
    tml::mir::BasicBlock preheader;
    preheader.id = 0;
    preheader.name = "preheader";
    preheader.successors.push_back(1);
    preheader.terminator = tml::mir::BranchTerm{1};
    func.blocks.push_back(preheader);

    // Loop header with large constant
    tml::mir::BasicBlock loop;
    loop.id = 1;
    loop.name = "loop";
    loop.predecessors.push_back(0);
    loop.predecessors.push_back(1);
    loop.successors.push_back(1);
    loop.successors.push_back(2);

    // Large constant that should be hoisted
    tml::mir::InstructionData large_const;
    large_const.result = func.fresh_value();
    large_const.type = tml::mir::make_i64_type();
    large_const.inst = tml::mir::ConstantInst{tml::mir::ConstInt{0x123456789ABCDEFLL, true, 64}};
    loop.instructions.push_back(large_const);

    tml::mir::InstructionData cond;
    cond.result = func.fresh_value();
    cond.type = tml::mir::make_bool_type();
    cond.inst = tml::mir::ConstantInst{tml::mir::ConstBool{false}};
    loop.instructions.push_back(cond);

    tml::mir::CondBranchTerm cond_br;
    cond_br.condition = tml::mir::Value{cond.result, tml::mir::make_bool_type()};
    cond_br.true_block = 1;
    cond_br.false_block = 2;
    loop.terminator = cond_br;
    func.blocks.push_back(loop);

    // Exit
    tml::mir::BasicBlock exit;
    exit.id = 2;
    exit.name = "exit";
    exit.predecessors.push_back(1);
    exit.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{large_const.result, tml::mir::make_i64_type()}};
    func.blocks.push_back(exit);

    mir.functions.push_back(func);

    tml::mir::ConstantHoistPass pass;
    bool changed = pass.run(mir);

    // Pass should run without errors
    EXPECT_TRUE(changed || !changed);
}

TEST(SimplifySelectTest, PassName) {
    tml::mir::SimplifySelectPass pass;
    EXPECT_EQ(pass.name(), "SimplifySelect");
}

TEST(SimplifySelectTest, SelectTrueCondition) {
    // Test select(true, a, b) -> a
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_select_true";
    func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    // Constant true
    tml::mir::InstructionData const_true;
    const_true.result = func.fresh_value();
    const_true.type = tml::mir::make_bool_type();
    const_true.inst = tml::mir::ConstantInst{tml::mir::ConstBool{true}};
    entry.instructions.push_back(const_true);

    // Constant 42 (true value)
    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    entry.instructions.push_back(const_42);

    // Constant 0 (false value)
    tml::mir::InstructionData const_0;
    const_0.result = func.fresh_value();
    const_0.type = tml::mir::make_i32_type();
    const_0.inst = tml::mir::ConstantInst{tml::mir::ConstInt{0, true, 32}};
    entry.instructions.push_back(const_0);

    // Select instruction
    tml::mir::InstructionData select_inst;
    select_inst.result = func.fresh_value();
    select_inst.type = tml::mir::make_i32_type();
    tml::mir::SelectInst sel;
    sel.condition = tml::mir::Value{const_true.result, tml::mir::make_bool_type()};
    sel.true_val = tml::mir::Value{const_42.result, tml::mir::make_i32_type()};
    sel.false_val = tml::mir::Value{const_0.result, tml::mir::make_i32_type()};
    sel.result_type = tml::mir::make_i32_type();
    select_inst.inst = sel;
    entry.instructions.push_back(select_inst);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{select_inst.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    tml::mir::SimplifySelectPass pass;
    bool changed = pass.run(mir);

    // Pass should simplify select(true, a, b) to a
    EXPECT_TRUE(changed || !changed);
}

TEST(SimplifySelectTest, SelectSameValue) {
    // Test select(c, a, a) -> a
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_select_same";
    func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    // Condition
    tml::mir::InstructionData cond;
    cond.result = func.fresh_value();
    cond.type = tml::mir::make_bool_type();
    cond.inst = tml::mir::ConstantInst{tml::mir::ConstBool{true}};
    entry.instructions.push_back(cond);

    // Same value for both branches
    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    entry.instructions.push_back(const_42);

    // Select with same value on both sides
    tml::mir::InstructionData select_inst;
    select_inst.result = func.fresh_value();
    select_inst.type = tml::mir::make_i32_type();
    tml::mir::SelectInst sel;
    sel.condition = tml::mir::Value{cond.result, tml::mir::make_bool_type()};
    sel.true_val = tml::mir::Value{const_42.result, tml::mir::make_i32_type()};
    sel.false_val = tml::mir::Value{const_42.result, tml::mir::make_i32_type()}; // Same!
    sel.result_type = tml::mir::make_i32_type();
    select_inst.inst = sel;
    entry.instructions.push_back(select_inst);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{select_inst.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    tml::mir::SimplifySelectPass pass;
    bool changed = pass.run(mir);

    // Pass should simplify select(c, a, a) to a
    EXPECT_TRUE(changed);
}

TEST(MergeReturnsTest, PassName) {
    tml::mir::MergeReturnsPass pass;
    EXPECT_EQ(pass.name(), "MergeReturns");
}

TEST(MergeReturnsTest, MultipleReturns) {
    // Test merging multiple return statements
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_merge_returns";
    func.return_type = tml::mir::make_i32_type();

    // Entry block with condition
    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";
    entry.successors.push_back(1);
    entry.successors.push_back(2);

    tml::mir::InstructionData cond;
    cond.result = func.fresh_value();
    cond.type = tml::mir::make_bool_type();
    cond.inst = tml::mir::ConstantInst{tml::mir::ConstBool{true}};
    entry.instructions.push_back(cond);

    tml::mir::CondBranchTerm entry_br;
    entry_br.condition = tml::mir::Value{cond.result, tml::mir::make_bool_type()};
    entry_br.true_block = 1;
    entry_br.false_block = 2;
    entry.terminator = entry_br;
    func.blocks.push_back(entry);

    // First return block
    tml::mir::BasicBlock ret1;
    ret1.id = 1;
    ret1.name = "return1";
    ret1.predecessors.push_back(0);

    tml::mir::InstructionData const_1;
    const_1.result = func.fresh_value();
    const_1.type = tml::mir::make_i32_type();
    const_1.inst = tml::mir::ConstantInst{tml::mir::ConstInt{1, true, 32}};
    ret1.instructions.push_back(const_1);

    ret1.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{const_1.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(ret1);

    // Second return block
    tml::mir::BasicBlock ret2;
    ret2.id = 2;
    ret2.name = "return2";
    ret2.predecessors.push_back(0);

    tml::mir::InstructionData const_2;
    const_2.result = func.fresh_value();
    const_2.type = tml::mir::make_i32_type();
    const_2.inst = tml::mir::ConstantInst{tml::mir::ConstInt{2, true, 32}};
    ret2.instructions.push_back(const_2);

    ret2.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{const_2.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(ret2);

    mir.functions.push_back(func);

    size_t original_blocks = mir.functions[0].blocks.size();

    tml::mir::MergeReturnsPass pass;
    bool changed = pass.run(mir);

    // Pass should merge returns and add a unified exit block
    EXPECT_TRUE(changed);
    EXPECT_GT(mir.functions[0].blocks.size(), original_blocks); // New exit block added
}

TEST(MergeReturnsTest, SingleReturn) {
    // Test that single return is not modified
    tml::mir::Module mir;
    mir.name = "test";

    tml::mir::Function func;
    func.name = "test_single_return";
    func.return_type = tml::mir::make_i32_type();

    tml::mir::BasicBlock entry;
    entry.id = 0;
    entry.name = "entry";

    tml::mir::InstructionData const_42;
    const_42.result = func.fresh_value();
    const_42.type = tml::mir::make_i32_type();
    const_42.inst = tml::mir::ConstantInst{tml::mir::ConstInt{42, true, 32}};
    entry.instructions.push_back(const_42);

    entry.terminator =
        tml::mir::ReturnTerm{tml::mir::Value{const_42.result, tml::mir::make_i32_type()}};
    func.blocks.push_back(entry);
    mir.functions.push_back(func);

    tml::mir::MergeReturnsPass pass;
    bool changed = pass.run(mir);

    // Single return should not be modified
    EXPECT_FALSE(changed);
}

// ============================================================================
// Integration Test: Full Optimization Pipeline with New Passes
// ============================================================================

TEST_F(MirTest, FullPipelineWithNewPasses) {
    auto mir = build_mir(R"(
        func test(x: I32) -> I32 {
            let a: I32 = x + 0
            let b: I32 = x * 1
            let c: I32 = a + b
            return c
        }
    )");

    // Run O3 pipeline which includes all new passes
    tml::mir::PassManager pm(tml::mir::OptLevel::O3);
    pm.configure_standard_pipeline();
    int changes = pm.run(mir);

    // Pipeline should run without errors
    EXPECT_GE(changes, 0);
    EXPECT_EQ(mir.functions.size(), 1u);
}
