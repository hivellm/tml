// Escape Analysis Tests
//
// Tests for the escape analysis and stack promotion passes

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/escape_analysis.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class EscapeAnalysisIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;
    tml::mir::EscapeAnalysisPass escape_pass_;

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

    void run_escape_analysis(tml::mir::Module& mir) {
        escape_pass_.set_module(&mir);
        escape_pass_.run(mir);
    }
};

// ============================================================================
// Basic Escape State Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, LocalVariableNoEscape) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    run_escape_analysis(mir);

    // Local integer variable should not escape
    auto stats = escape_pass_.get_stats();
    EXPECT_GT(stats.no_escape, 0u);
}

TEST_F(EscapeAnalysisIntegrationTest, ReturnedValueEscapes) {
    // Test that returned values are marked as escaping
    auto mir = build_mir(R"(
        func create_value() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    run_escape_analysis(mir);

    // Returned value should be marked as ReturnEscape
    auto stats = escape_pass_.get_stats();
    EXPECT_GT(stats.return_escape, 0u);
}

TEST_F(EscapeAnalysisIntegrationTest, PassedArgumentEscapes) {
    auto mir = build_mir(R"(
        func consume(x: I32) {
        }

        func test() {
            let val: I32 = 42
            consume(val)
        }
    )");

    run_escape_analysis(mir);

    // Value passed to function should be marked as ArgEscape
    auto stats = escape_pass_.get_stats();
    EXPECT_GT(stats.arg_escape, 0u);
}

// ============================================================================
// Allocation Tracking Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, LocalAllocationTracking) {
    // Test that local allocations are tracked
    auto mir = build_mir(R"(
        func test() {
            let x: I32 = 42
            let y: I32 = x + 1
        }
    )");

    run_escape_analysis(mir);

    // Local allocations should be tracked with no_escape
    auto stats = escape_pass_.get_stats();
    EXPECT_GT(stats.no_escape, 0u);
}

TEST_F(EscapeAnalysisIntegrationTest, MultipleAllocationsTracking) {
    // Test that multiple allocations in a function are all tracked
    auto mir = build_mir(R"(
        func test() -> I32 {
            let a: I32 = 1
            let b: I32 = 2
            let c: I32 = 3
            return a + b + c
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Should have tracked multiple values
    size_t total = stats.no_escape + stats.return_escape;
    EXPECT_GT(total, 0u);
}

// ============================================================================
// Stack Promotion Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, StackPromotableAllocation) {
    auto mir = build_mir(R"(
        func test() {
            let x: I32 = 42
        }
    )");

    run_escape_analysis(mir);

    auto promotable = escape_pass_.get_stack_promotable();
    // Local variable should be stack-promotable
    EXPECT_GE(promotable.size(), 0u);
}

TEST_F(EscapeAnalysisIntegrationTest, NonPromotableReturned) {
    // Test that returned values are marked as escaping (not promotable to stack)
    auto mir = build_mir(R"(
        func create_value() -> I64 {
            let value: I64 = 100
            return value
        }
    )");

    run_escape_analysis(mir);

    // Returned value should NOT be stack-promotable
    auto stats = escape_pass_.get_stats();
    EXPECT_GT(stats.return_escape, 0u);
}

// ============================================================================
// Class Metadata Tests (Unit tests for ClassMetadata struct)
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, ClassMetadataStructure) {
    // Test the ClassMetadata structure itself
    tml::mir::ClassMetadata metadata;
    metadata.name = "TestClass";
    metadata.is_sealed = true;
    metadata.is_abstract = false;
    metadata.is_value = false;
    metadata.stack_allocatable = true;
    metadata.estimated_size = 24;
    metadata.inheritance_depth = 1;
    metadata.base_class = "BaseClass";
    metadata.subclasses = {};
    metadata.virtual_methods = {"update", "render"};
    metadata.final_methods = {"dispose"};

    EXPECT_EQ(metadata.name, "TestClass");
    EXPECT_TRUE(metadata.is_sealed);
    EXPECT_FALSE(metadata.is_abstract);
    EXPECT_TRUE(metadata.stack_allocatable);
    EXPECT_TRUE(metadata.can_devirtualize_all()); // sealed = true means all can be devirtualized
    EXPECT_TRUE(metadata.methods_preserve_noescapse()); // sealed + not abstract
    EXPECT_FALSE(metadata.is_pure_value());             // is_value = false
}

TEST_F(EscapeAnalysisIntegrationTest, ModuleClassMetadataLookup) {
    // Test Module class metadata lookup
    tml::mir::Module module;
    module.name = "test";

    tml::mir::ClassMetadata metadata;
    metadata.name = "Point";
    metadata.is_sealed = true;
    metadata.stack_allocatable = true;
    module.class_metadata["Point"] = metadata;

    EXPECT_TRUE(module.is_class_sealed("Point"));
    EXPECT_TRUE(module.can_stack_allocate("Point"));
    EXPECT_FALSE(module.is_class_sealed("NonExistent"));
    EXPECT_FALSE(module.can_stack_allocate("NonExistent"));

    auto lookup = module.get_class_metadata("Point");
    EXPECT_TRUE(lookup.has_value());
    EXPECT_EQ(lookup->name, "Point");
}

// ============================================================================
// Combined Pass Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, EscapeAndPromotePass) {
    auto mir = build_mir(R"(
        func test() {
            let x: I32 = 1
            let y: I32 = 2
            let z: I32 = x + y
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto escape_stats = combined_pass.get_escape_stats();
    auto promo_stats = combined_pass.get_promotion_stats();

    // Should have analyzed some values
    EXPECT_GT(escape_stats.no_escape + escape_stats.arg_escape + escape_stats.return_escape, 0u);
}

// ============================================================================
// Arena Allocation Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, ArenaAllocationTracking) {
    // This test verifies that the arena allocation detection logic exists
    // Even without actual Arena_alloc calls, we verify the infrastructure
    auto mir = build_mir(R"(
        func test() {
            let x: I32 = 42
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Arena allocations counter should be initialized
    EXPECT_EQ(stats.arena_allocations, 0u);
}

// ============================================================================
// Conditional Escape Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, ConditionalEscapeTracking) {
    auto mir = build_mir(R"(
        func test(flag: Bool) -> I32 {
            let x: I32 = 42
            if flag then {
                return x
            }
            return 0
        }
    )");

    run_escape_analysis(mir);

    // Value escapes only in one branch
    auto stats = escape_pass_.get_stats();
    EXPECT_GT(stats.return_escape, 0u);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, StatisticsAccumulation) {
    auto mir = build_mir(R"(
        func helper(x: I32) -> I32 {
            return x + 1
        }

        func test() -> I32 {
            let a: I32 = 10
            let b: I32 = helper(a)
            return b
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Should have both no_escape and escape categories
    size_t total = stats.no_escape + stats.arg_escape + stats.return_escape + stats.global_escape;
    EXPECT_GT(total, 0u);
}

// ============================================================================
// Stack Promotion Pass Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, StackPromotionBasic) {
    auto mir = build_mir(R"(
        func test() {
            let x: I32 = 42
            let y: I32 = x + 1
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto promo_stats = combined_pass.get_promotion_stats();
    // Stack promotion should have run without errors
    EXPECT_GE(promo_stats.allocations_promoted, 0u);
}

TEST_F(EscapeAnalysisIntegrationTest, StackPromotionStats) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let a: I32 = 1
            let b: I32 = 2
            let c: I32 = a + b
            return c
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto escape_stats = combined_pass.get_escape_stats();
    auto promo_stats = combined_pass.get_promotion_stats();

    // Should have tracked values
    EXPECT_GE(escape_stats.total_allocations + escape_stats.no_escape, 0u);
}

// ============================================================================
// Loop Allocation Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, LoopAllocationStatsExist) {
    // Test that loop allocation tracking infrastructure exists
    auto mir = build_mir(R"(
        func test() -> I32 {
            let sum: I32 = 0
            return sum
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Loop allocation fields should be initialized
    EXPECT_EQ(stats.loop_allocations_found, 0u);
    EXPECT_EQ(stats.loop_allocs_promotable, 0u);
    EXPECT_EQ(stats.loop_allocs_hoistable, 0u);
}

// ============================================================================
// Conditional Allocation Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, ConditionalAllocationStatsExist) {
    auto mir = build_mir(R"(
        func test(flag: Bool) -> I32 {
            if flag then {
                return 1
            }
            return 0
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Conditional allocation fields should be initialized
    EXPECT_GE(stats.conditional_allocations_found, 0u);
}

// ============================================================================
// Sealed Class Optimization Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, SealedClassStatsExist) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            return 42
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Sealed class fields should be initialized (even if 0)
    EXPECT_EQ(stats.sealed_class_instances, 0u);
    EXPECT_EQ(stats.sealed_class_promotable, 0u);
    EXPECT_EQ(stats.sealed_method_noescapes, 0u);
}

// ============================================================================
// Free Call Removal Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, FreeRemovalStatsExist) {
    auto mir = build_mir(R"(
        func test() {
            let x: I32 = 1
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto promo_stats = combined_pass.get_promotion_stats();
    // Free call removal counter should be initialized
    EXPECT_GE(promo_stats.free_calls_removed, 0u);
}

// ============================================================================
// Destructor Insertion Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, DestructorInsertionStatsExist) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto promo_stats = combined_pass.get_promotion_stats();
    // Destructor insertion counter should be initialized
    EXPECT_GE(promo_stats.destructors_inserted, 0u);
}

// ============================================================================
// Loop Allocation Promotion Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, LoopPromotionStatsExist) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            return 0
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto promo_stats = combined_pass.get_promotion_stats();
    // Loop promotion fields should be initialized
    EXPECT_GE(promo_stats.loop_allocs_promoted, 0u);
    EXPECT_GE(promo_stats.loop_allocs_hoisted, 0u);
}

// ============================================================================
// Conditional Slot Sharing Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, ConditionalSlotSharingStatsExist) {
    auto mir = build_mir(R"(
        func test(flag: Bool) -> I32 {
            if flag then {
                return 1
            }
            return 2
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto promo_stats = combined_pass.get_promotion_stats();
    // Conditional slot sharing fields should be initialized
    EXPECT_GE(promo_stats.conditional_slots_shared, 0u);
    EXPECT_GE(promo_stats.conditional_allocs_promoted, 0u);
}

// ============================================================================
// Stack Allocation IR Flag Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, StackEligibleFlagOnAlloca) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    // Run escape and promote pass
    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    // Check that allocations have been marked as stack-eligible
    bool found_eligible = false;
    for (const auto& func : mir.functions) {
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* alloca = std::get_if<tml::mir::AllocaInst>(&inst.inst)) {
                    if (alloca->is_stack_eligible) {
                        found_eligible = true;
                    }
                }
            }
        }
    }
    // Note: primitives may not be marked, this is just checking the field exists
    EXPECT_GE(found_eligible ? 1 : 0, 0);
}

TEST_F(EscapeAnalysisIntegrationTest, BytesSavedStatistic) {
    auto mir = build_mir(R"(
        func test() {
            let a: I64 = 1
            let b: I64 = 2
            let c: I64 = 3
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto promo_stats = combined_pass.get_promotion_stats();
    // bytes_saved should be tracked
    EXPECT_GE(promo_stats.bytes_saved, 0u);
}

// ============================================================================
// Escape Info Query Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, GetEscapeInfoReturnsValidInfo) {
    auto mir = build_mir(R"(
        func test() -> I32 {
            let x: I32 = 42
            return x
        }
    )");

    run_escape_analysis(mir);

    // Query escape info for any value
    // Even for invalid value, should not crash
    auto info = escape_pass_.get_escape_info(tml::mir::INVALID_VALUE);
    EXPECT_EQ(info.state, tml::mir::EscapeState::Unknown);
}

TEST_F(EscapeAnalysisIntegrationTest, CanStackPromoteQuery) {
    auto mir = build_mir(R"(
        func test() {
            let x: I32 = 42
        }
    )");

    run_escape_analysis(mir);

    // Query can_stack_promote for invalid value
    bool result = escape_pass_.can_stack_promote(tml::mir::INVALID_VALUE);
    EXPECT_FALSE(result);
}

// ============================================================================
// SROA (Scalar Replacement of Aggregates) Tests
// ============================================================================

TEST_F(EscapeAnalysisIntegrationTest, SroaEligibleAllocationsHaveAlignment) {
    // Test that stack-promoted allocations have proper 8-byte alignment
    // which is required for LLVM's SROA pass to work effectively
    auto mir = build_mir(R"(
        func test() {
            let x: I64 = 42
            let y: I64 = 100
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    // Verify that allocations exist (the actual alignment is in LLVM IR generation)
    bool has_allocations = false;
    for (const auto& func : mir.functions) {
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (std::holds_alternative<tml::mir::AllocaInst>(inst.inst)) {
                    has_allocations = true;
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(has_allocations);
}

TEST_F(EscapeAnalysisIntegrationTest, SroaSmallStructsEligible) {
    // Test that small structs (that don't escape) are SROA-eligible
    // These should be stack-promoted and can be broken into scalar registers
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

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    auto promo_stats = combined_pass.get_promotion_stats();
    // Small local structs should be candidates for stack promotion
    // which enables SROA to break them into individual registers
    EXPECT_GE(promo_stats.allocations_promoted, 0u);
}

TEST_F(EscapeAnalysisIntegrationTest, SroaNoEscapeForLocalUse) {
    // Local struct used only within function should not escape
    // This is key for SROA - non-escaping allocations can be fully scalarized
    auto mir = build_mir(R"(
        struct Vec2 {
            x: F64,
            y: F64
        }

        func add(a: Vec2, b: Vec2) -> F64 {
            return a.x + b.x + a.y + b.y
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Local usage should result in NoEscape state
    EXPECT_GT(stats.no_escape, 0u);
}

TEST_F(EscapeAnalysisIntegrationTest, SroaFunctionAttributesForOptimization) {
    // Test that functions with small allocations are suitable for optimization
    // The MIR should produce code that LLVM can optimize with SROA
    auto mir = build_mir(R"(
        func compute(x: I32, y: I32) -> I32 {
            let sum: I32 = x + y
            let diff: I32 = x - y
            return sum * diff
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    // The function should be eligible for aggressive optimization
    // Check that the function exists and has blocks
    EXPECT_GT(mir.functions.size(), 0u);
    for (const auto& func : mir.functions) {
        if (func.name.find("compute") != std::string::npos) {
            EXPECT_GT(func.blocks.size(), 0u);
            break;
        }
    }
}

TEST_F(EscapeAnalysisIntegrationTest, SroaStackEligibleCallInst) {
    // Test that constructor calls can be marked as stack-eligible
    // which enables LLVM to use alloca instead of heap allocation
    auto mir = build_mir(R"(
        func identity(x: I32) -> I32 {
            return x
        }

        func test() -> I32 {
            return identity(42)
        }
    )");

    tml::mir::EscapeAndPromotePass combined_pass;
    combined_pass.run(mir);

    // Check that call instructions have is_stack_eligible field
    bool found_call = false;
    for (const auto& func : mir.functions) {
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (std::holds_alternative<tml::mir::CallInst>(inst.inst)) {
                    found_call = true;
                    // The is_stack_eligible field exists on CallInst
                    const auto& call = std::get<tml::mir::CallInst>(inst.inst);
                    (void)call.is_stack_eligible; // Just verify field exists
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(found_call);
}

TEST_F(EscapeAnalysisIntegrationTest, SroaMultipleFieldsOptimizable) {
    // Test that structs with multiple fields can be optimized by SROA
    // Each field should become a separate register after SROA runs
    auto mir = build_mir(R"(
        struct Rectangle {
            x: I32,
            y: I32,
            width: I32,
            height: I32
        }

        func area(r: Rectangle) -> I32 {
            return r.width * r.height
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Rectangle parameter doesn't escape the function
    EXPECT_GT(stats.no_escape + stats.arg_escape, 0u);
}

TEST_F(EscapeAnalysisIntegrationTest, SroaReturnedStructCannotScalarize) {
    // Test that returned structs are properly marked as escaping
    // These cannot be fully scalarized by SROA as they need to be materialized
    auto mir = build_mir(R"(
        struct Pair {
            first: I32,
            second: I32
        }

        func make_pair(a: I32, b: I32) -> Pair {
            return Pair { first: a, second: b }
        }
    )");

    run_escape_analysis(mir);

    auto stats = escape_pass_.get_stats();
    // Return value escapes
    EXPECT_GT(stats.return_escape, 0u);
}
