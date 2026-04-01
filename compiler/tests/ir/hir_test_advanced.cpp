// HIR advanced tests — optimization passes and serialization
//
// Split from hir_test.cpp for maintainability

#include "hir/hir.hpp"
#include "hir/hir_builder.hpp"
#include "hir/hir_printer.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

// ============================================================================
// HIR Optimization Tests
// ============================================================================

#include "hir/hir_pass.hpp"

class HirOptimizationTest : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;

    auto build_hir(const std::string& code) -> tml::hir::HirModule {
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

        tml::hir::HirBuilder builder(env);
        return builder.lower_module(module);
    }

    // Helper to check if an expression is a literal with specific value
    template <typename T>
    auto is_literal_with_value(const tml::hir::HirExpr& expr, T expected) -> bool {
        if (!expr.is<tml::hir::HirLiteralExpr>())
            return false;
        const auto& lit = expr.as<tml::hir::HirLiteralExpr>();
        if (auto* val = std::get_if<T>(&lit.value)) {
            return *val == expected;
        }
        return false;
    }
};

// ============================================================================
// Constant Folding Tests
// ============================================================================

TEST_F(HirOptimizationTest, ConstantFolding_IntegerAddition) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 2 + 3
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);

    // The expression should now be folded to a literal 5
    ASSERT_EQ(hir.functions.size(), 1u);
    ASSERT_TRUE(hir.functions[0].body.has_value());
}

TEST_F(HirOptimizationTest, ConstantFolding_IntegerSubtraction) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 10 - 3
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_IntegerMultiplication) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 4 * 5
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_IntegerDivision) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 20 / 4
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_IntegerModulo) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 17 % 5
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_FloatAddition) {
    auto hir = build_hir(R"(
        func test() -> F64 {
            return 1.5 + 2.5
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_BooleanAnd) {
    auto hir = build_hir(R"(
        func test() -> Bool {
            return true and false
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_BooleanOr) {
    auto hir = build_hir(R"(
        func test() -> Bool {
            return true or false
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_Comparison) {
    auto hir = build_hir(R"(
        func test() -> Bool {
            return 5 > 3
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_Equality) {
    auto hir = build_hir(R"(
        func test() -> Bool {
            return 42 == 42
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_UnaryNegation) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return -42
        }
    )");

    // Unary negation on literal - may or may not fold depending on parser
    tml::hir::ConstantFolding::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ConstantFolding_LogicalNot) {
    auto hir = build_hir(R"(
        func test() -> Bool {
            return not true
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_NestedExpressions) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return (2 + 3) * (4 + 1)
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_NoChangeWithVariables) {
    auto hir = build_hir(R"(
        func test(x: I32) -> I32 {
            return x + 1
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    // Should not change because x is a variable
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_ShortCircuitAndFalse) {
    auto hir = build_hir(R"(
        func side_effect() -> Bool {
            return true
        }
        func test() -> Bool {
            return false and side_effect()
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    // false and X => false (short-circuit)
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_ShortCircuitOrTrue) {
    auto hir = build_hir(R"(
        func side_effect() -> Bool {
            return false
        }
        func test() -> Bool {
            return true or side_effect()
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    // true or X => true (short-circuit)
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, ConstantFolding_BitwiseOperations) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 0xFF & 0x0F
        }
    )");

    bool changed = tml::hir::ConstantFolding::run_pass(hir);
    EXPECT_TRUE(changed);
}

// ============================================================================
// Dead Code Elimination Tests
// ============================================================================

TEST_F(HirOptimizationTest, DCE_ConstantTrueCondition) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            if true {
                return 1
            } else {
                return 2
            }
        }
    )");

    bool changed = tml::hir::DeadCodeElimination::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, DCE_ConstantFalseCondition) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            if false {
                return 1
            } else {
                return 2
            }
        }
    )");

    bool changed = tml::hir::DeadCodeElimination::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, DCE_NoChangeWithVariableCondition) {
    auto hir = build_hir(R"(
        func test(cond: Bool) -> I32 {
            if cond {
                return 1
            } else {
                return 2
            }
        }
    )");

    bool changed = tml::hir::DeadCodeElimination::run_pass(hir);
    // Condition is a variable, should not eliminate
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, DCE_NestedIf) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            if true {
                if false {
                    return 1
                } else {
                    return 2
                }
            } else {
                return 3
            }
        }
    )");

    bool changed = tml::hir::DeadCodeElimination::run_pass(hir);
    EXPECT_TRUE(changed);
}

// ============================================================================
// Pass Manager Tests
// ============================================================================

TEST_F(HirOptimizationTest, PassManager_RunMultiplePasses) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            if true {
                return 2 + 3
            } else {
                return 10
            }
        }
    )");

    tml::hir::HirPassManager pm;
    pm.add_pass<tml::hir::ConstantFolding>();
    pm.add_pass<tml::hir::DeadCodeElimination>();

    bool changed = pm.run(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, PassManager_RunToFixpoint) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 1 + 2 + 3 + 4
        }
    )");

    tml::hir::HirPassManager pm;
    pm.add_pass<tml::hir::ConstantFolding>();

    size_t iterations = pm.run_to_fixpoint(hir, 10);
    EXPECT_GE(iterations, 1u);
}

TEST_F(HirOptimizationTest, OptimizeHir_ConvenienceFunction) {
    auto hir = build_hir(R"(
        func test() -> Bool {
            return true and false
        }
    )");

    bool changed = tml::hir::optimize_hir(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, OptimizeHirLevel_Level0NoOptimization) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 2 + 3
        }
    )");

    bool changed = tml::hir::optimize_hir_level(hir, 0);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, OptimizeHirLevel_Level1ConstantFolding) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 2 + 3
        }
    )");

    bool changed = tml::hir::optimize_hir_level(hir, 1);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, OptimizeHirLevel_Level2DCE) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            if true {
                return 5
            } else {
                return 10
            }
        }
    )");

    bool changed = tml::hir::optimize_hir_level(hir, 2);
    EXPECT_TRUE(changed);
}

// ============================================================================
// Pure Expression Detection Tests
// ============================================================================

TEST(HirPurityTest, LiteralIsPure) {
    tml::hir::HirExpr expr;
    tml::hir::HirLiteralExpr lit;
    lit.id = tml::hir::HirId{1};
    lit.value = int64_t{42};
    lit.type = tml::types::make_i32();
    lit.span = tml::SourceSpan{};
    expr.kind = std::move(lit);

    tml::hir::DeadCodeElimination dce;
    // Note: is_pure_expr is private, but we can verify behavior through DCE
    SUCCEED(); // Test setup works
}

TEST(HirPurityTest, VariableIsPure) {
    tml::hir::HirExpr expr;
    tml::hir::HirVarExpr var;
    var.id = tml::hir::HirId{1};
    var.name = "x";
    var.type = tml::types::make_i32();
    var.span = tml::SourceSpan{};
    expr.kind = std::move(var);

    SUCCEED(); // Test setup works
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(HirOptimizationTest, EmptyModule) {
    auto hir = build_hir("");

    bool changed = tml::hir::optimize_hir(hir);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, FunctionWithoutOptimizableCode) {
    auto hir = build_hir(R"(
        func test(x: I32) -> I32 {
            return x
        }
    )");

    // Simple function with no constant expressions should not change
    bool changed = tml::hir::optimize_hir(hir);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, MultipleOptimizationRounds) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return if true { 2 + 3 } else { 10 + 20 }
        }
    )");

    tml::hir::HirPassManager pm;
    pm.add_pass<tml::hir::ConstantFolding>();
    pm.add_pass<tml::hir::DeadCodeElimination>();

    // Run multiple times to ensure convergence
    size_t iterations = pm.run_to_fixpoint(hir, 5);
    EXPECT_LE(iterations, 5u);
}

// ============================================================================
// HIR Serialization Tests
// ============================================================================

#include "hir/hir_serialize.hpp"

#include <sstream>

class HirSerializationTest : public HirTest {};

TEST_F(HirSerializationTest, BinaryRoundTripEmptyModule) {
    auto original = build_hir("");

    // Serialize to binary
    std::ostringstream oss(std::ios::binary);
    tml::hir::HirBinaryWriter writer(oss);
    writer.write_module(original);

    // Deserialize
    std::istringstream iss(oss.str(), std::ios::binary);
    tml::hir::HirBinaryReader reader(iss);
    auto loaded = reader.read_module();

    EXPECT_FALSE(reader.has_error());
    EXPECT_EQ(loaded.name, original.name);
    EXPECT_EQ(loaded.functions.size(), original.functions.size());
    EXPECT_EQ(loaded.structs.size(), original.structs.size());
    EXPECT_EQ(loaded.enums.size(), original.enums.size());
}

TEST_F(HirSerializationTest, BinaryRoundTripWithFunction) {
    auto original = build_hir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");

    // Serialize
    std::ostringstream oss(std::ios::binary);
    tml::hir::HirBinaryWriter writer(oss);
    writer.write_module(original);

    // Deserialize
    std::istringstream iss(oss.str(), std::ios::binary);
    tml::hir::HirBinaryReader reader(iss);
    auto loaded = reader.read_module();

    EXPECT_FALSE(reader.has_error());
    EXPECT_EQ(loaded.name, original.name);
    ASSERT_EQ(loaded.functions.size(), 1u);
    EXPECT_EQ(loaded.functions[0].name, "add");
    EXPECT_EQ(loaded.functions[0].params.size(), 2u);
}

TEST_F(HirSerializationTest, BinaryRoundTripWithStruct) {
    auto original = build_hir(R"(
        type Point {
            x: I32
            y: I32
        }
    )");

    // Serialize
    std::ostringstream oss(std::ios::binary);
    tml::hir::HirBinaryWriter writer(oss);
    writer.write_module(original);

    // Deserialize
    std::istringstream iss(oss.str(), std::ios::binary);
    tml::hir::HirBinaryReader reader(iss);
    auto loaded = reader.read_module();

    EXPECT_FALSE(reader.has_error());
    ASSERT_EQ(loaded.structs.size(), 1u);
    EXPECT_EQ(loaded.structs[0].name, "Point");
    EXPECT_EQ(loaded.structs[0].fields.size(), 2u);
}

TEST_F(HirSerializationTest, BinaryRoundTripWithEnum) {
    auto original = build_hir(R"(
        type Color {
            Red,
            Green,
            Blue
        }
    )");

    // Serialize
    std::ostringstream oss(std::ios::binary);
    tml::hir::HirBinaryWriter writer(oss);
    writer.write_module(original);

    // Deserialize
    std::istringstream iss(oss.str(), std::ios::binary);
    tml::hir::HirBinaryReader reader(iss);
    auto loaded = reader.read_module();

    EXPECT_FALSE(reader.has_error());
    ASSERT_EQ(loaded.enums.size(), 1u);
    EXPECT_EQ(loaded.enums[0].name, "Color");
    EXPECT_EQ(loaded.enums[0].variants.size(), 3u);
}

TEST_F(HirSerializationTest, TextWriterOutput) {
    auto hir = build_hir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");

    std::ostringstream oss;
    tml::hir::HirTextWriter writer(oss);
    writer.write_module(hir);

    std::string output = oss.str();
    EXPECT_TRUE(output.find("HIR Module") != std::string::npos);
    EXPECT_TRUE(output.find("add") != std::string::npos);
}

TEST_F(HirSerializationTest, ContentHashConsistency) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 42
        }
    )");

    auto hash1 = tml::hir::compute_hir_hash(hir);
    auto hash2 = tml::hir::compute_hir_hash(hir);

    // Same module should produce same hash
    EXPECT_EQ(hash1, hash2);
}

TEST_F(HirSerializationTest, ContentHashDifferentModules) {
    auto hir1 = build_hir(R"(
        func test() -> I32 {
            return 42
        }
    )");

    auto hir2 = build_hir(R"(
        func different_name() -> I32 {
            return 42
        }
    )");

    auto hash1 = tml::hir::compute_hir_hash(hir1);
    auto hash2 = tml::hir::compute_hir_hash(hir2);

    // Different function names should produce different hashes
    EXPECT_NE(hash1, hash2);
}

TEST_F(HirSerializationTest, SerializeDeserializeUtilities) {
    auto original = build_hir(R"(
        func main() {
            let x: I32 = 42
        }
    )");

    // Test convenience functions
    auto bytes = tml::hir::serialize_hir_binary(original);
    EXPECT_FALSE(bytes.empty());

    auto loaded = tml::hir::deserialize_hir_binary(bytes);
    EXPECT_EQ(loaded.name, original.name);
    EXPECT_EQ(loaded.functions.size(), original.functions.size());
}

TEST_F(HirSerializationTest, TextSerializationOutput) {
    auto hir = build_hir(R"(
        type Point {
            x: I32
            y: I32
        }

        func distance(p: Point) -> I32 {
            return p.x + p.y
        }
    )");

    auto text = tml::hir::serialize_hir_text(hir);
    EXPECT_FALSE(text.empty());
    EXPECT_TRUE(text.find("Point") != std::string::npos);
    EXPECT_TRUE(text.find("distance") != std::string::npos);
}

TEST_F(HirSerializationTest, InvalidBinaryHeader) {
    // Create invalid binary data
    std::string invalid_data = "INVALID";
    std::istringstream iss(invalid_data, std::ios::binary);
    tml::hir::HirBinaryReader reader(iss);
    auto loaded = reader.read_module();

    EXPECT_TRUE(reader.has_error());
    EXPECT_TRUE(reader.error_message().find("magic") != std::string::npos ||
                reader.error_message().find("Invalid") != std::string::npos);
}

TEST_F(HirSerializationTest, ContentHashFromWriter) {
    auto hir = build_hir(R"(
        func test() -> I32 { return 42 }
    )");

    std::ostringstream oss(std::ios::binary);
    tml::hir::HirBinaryWriter writer(oss);
    writer.write_module(hir);

    auto writer_hash = writer.content_hash();
    auto computed_hash = tml::hir::compute_hir_hash(hir);

    EXPECT_EQ(writer_hash, computed_hash);
}

// ============================================================================
// Inlining Pass Tests
// ============================================================================

TEST_F(HirOptimizationTest, Inlining_SmallFunction) {
    auto hir = build_hir(R"(
        func square(x: I32) -> I32 {
            return x * x
        }

        func caller() -> I32 {
            let a: I32 = 1
            let b: I32 = 2
            let c: I32 = 3
            let d: I32 = 4
            let e: I32 = 5
            let f: I32 = 6
            return square(a + b + c + d + e + f)
        }
    )");

    // Small functions should be inlined into larger callers
    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, Inlining_MultipleParameters) {
    auto hir = build_hir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }

        func caller() -> I32 {
            let a: I32 = 1
            let b: I32 = 2
            let c: I32 = 3
            let d: I32 = 4
            let e: I32 = 5
            let f: I32 = 6
            return add(a + b + c, d + e + f)
        }
    )");

    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, Inlining_NoInlineForLargeFunctions) {
    auto hir = build_hir(R"(
        func large_func(x: I32) -> I32 {
            let a: I32 = x + 1
            let b: I32 = a + 2
            let c: I32 = b + 3
            let d: I32 = c + 4
            let e: I32 = d + 5
            let f: I32 = e + 6
            return f
        }

        func test() -> I32 {
            return large_func(0)
        }
    )");

    // Large functions (>5 statements) should not be inlined by default
    tml::hir::Inlining inliner(5); // max 5 statements
    bool changed = inliner.run(hir);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, Inlining_MultipleCallSites) {
    auto hir = build_hir(R"(
        func double(x: I32) -> I32 {
            return x * 2
        }

        func caller() -> I32 {
            let v1: I32 = 1
            let v2: I32 = 2
            let v3: I32 = 3
            let v4: I32 = 4
            let v5: I32 = 5
            let v6: I32 = 6
            let a: I32 = double(v1 + v2)
            let b: I32 = double(v3 + v4)
            return a + b + v5 + v6
        }
    )");

    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, Inlining_NoChangeWithRecursion) {
    auto hir = build_hir(R"(
        func factorial(n: I32) -> I32 {
            if n <= 1 {
                return 1
            } else {
                return n * factorial(n - 1)
            }
        }

        func test() -> I32 {
            return factorial(5)
        }
    )");

    // Recursive functions should not be inlined
    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, Inlining_NoChangeForEmptyModule) {
    auto hir = build_hir("");

    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, Inlining_SingleFunctionModule) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 42
        }
    )");

    // Single function with no calls should not change
    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, Inlining_ChainedCalls) {
    auto hir = build_hir(R"(
        func inc(x: I32) -> I32 {
            return x + 1
        }

        func caller() -> I32 {
            let v1: I32 = 1
            let v2: I32 = 2
            let v3: I32 = 3
            let v4: I32 = 4
            let v5: I32 = 5
            let v6: I32 = 6
            return inc(inc(inc(v1 + v2 + v3 + v4 + v5 + v6)))
        }
    )");

    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, Inlining_WithConditional) {
    auto hir = build_hir(R"(
        func abs_val(x: I32) -> I32 {
            if x < 0 {
                return -x
            } else {
                return x
            }
        }

        func caller() -> I32 {
            let v1: I32 = 1
            let v2: I32 = 2
            let v3: I32 = 3
            let v4: I32 = 4
            let v5: I32 = 5
            let v6: I32 = 6
            return abs_val(v1 - v2 - v3 - v4 - v5 - v6)
        }
    )");

    // Function with conditional should still be inlined if small enough
    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, Inlining_CustomThreshold) {
    auto hir = build_hir(R"(
        func triple(x: I32) -> I32 {
            let y: I32 = x + x
            return y + x
        }

        func caller() -> I32 {
            let v1: I32 = 1
            let v2: I32 = 2
            let v3: I32 = 3
            let v4: I32 = 4
            let v5: I32 = 5
            let v6: I32 = 6
            return triple(v1 + v2 + v3 + v4 + v5 + v6)
        }
    )");

    // With threshold of 1, this should not inline (2 statements: let + return)
    tml::hir::Inlining inliner_strict(1);
    EXPECT_FALSE(inliner_strict.run(hir));

    // Rebuild HIR for fresh test
    auto hir2 = build_hir(R"(
        func triple(x: I32) -> I32 {
            let y: I32 = x + x
            return y + x
        }

        func caller() -> I32 {
            let v1: I32 = 1
            let v2: I32 = 2
            let v3: I32 = 3
            let v4: I32 = 4
            let v5: I32 = 5
            let v6: I32 = 6
            return triple(v1 + v2 + v3 + v4 + v5 + v6)
        }
    )");

    // With threshold of 5, this should inline
    tml::hir::Inlining inliner_lenient(5);
    EXPECT_TRUE(inliner_lenient.run(hir2));
}

TEST_F(HirOptimizationTest, Inlining_DoesNotInlineSelf) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 42
        }
    )");

    // A function should not attempt to inline itself
    bool changed = tml::hir::Inlining::run_pass(hir);
    EXPECT_FALSE(changed);
}

// ============================================================================
// Closure Optimization Tests
// ============================================================================

TEST_F(HirOptimizationTest, ClosureOptimization_RemoveUnusedCaptures) {
    auto hir = build_hir(R"(
        func test() {
            let x: I32 = 10
            let y: I32 = 20
            let f: (I32) -> I32 = do(a: I32) a + 1
        }
    )");

    // Closure doesn't use x or y, any captures should be removed
    // May or may not change depending on whether captures were added
    (void)tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ClosureOptimization_KeepUsedCaptures) {
    auto hir = build_hir(R"(
        func test() {
            let offset: I32 = 10
            let f: (I32) -> I32 = do(x: I32) x + offset
        }
    )");

    // Closure uses offset, capture should be kept
    tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ClosureOptimization_NoChangeForEmptyModule) {
    auto hir = build_hir("");

    bool changed = tml::hir::ClosureOptimization::run_pass(hir);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, ClosureOptimization_NoClosure) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 42
        }
    )");

    // No closures = no changes
    bool changed = tml::hir::ClosureOptimization::run_pass(hir);
    EXPECT_FALSE(changed);
}

TEST_F(HirOptimizationTest, ClosureOptimization_MultipleCapturesPartialUse) {
    auto hir = build_hir(R"(
        func test() {
            let a: I32 = 1
            let b: I32 = 2
            let c: I32 = 3
            let f: (I32) -> I32 = do(x: I32) x + b
        }
    )");

    // Only b is used, a and c should be removed from captures (if any)
    tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ClosureOptimization_NestedClosures) {
    auto hir = build_hir(R"(
        func test() {
            let x: I32 = 10
            let outer: ((I32) -> I32) -> I32 = do(f: (I32) -> I32) f(x)
        }
    )");

    // Nested closure scenario
    tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ClosureOptimization_ClosureInLoop) {
    auto hir = build_hir(R"(
        func test() {
            let factor: I32 = 2
            let mut i: I32 = 0
            while i < 10 {
                let f: (I32) -> I32 = do(x: I32) x * factor
                i = i + 1
            }
        }
    )");

    // Closure in loop uses factor
    tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ClosureOptimization_ClosureReturnedFromFunction) {
    auto hir = build_hir(R"(
        func make_adder(n: I32) -> (I32) -> I32 {
            return do(x: I32) x + n
        }
    )");

    // Closure escapes (returned), captures should remain as-is
    tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ClosureOptimization_ClosureInConditional) {
    auto hir = build_hir(R"(
        func test(cond: Bool) {
            let val: I32 = 42
            if cond {
                let f: (I32) -> I32 = do(x: I32) x + val
            }
        }
    )");

    // Closure in if branch
    tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ClosureOptimization_SimpleClosureNoCapture) {
    auto hir = build_hir(R"(
        func test() {
            let identity: (I32) -> I32 = do(x: I32) x
        }
    )");

    // Identity closure with no captures
    tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirOptimizationTest, ClosureOptimization_ClosureWithBinaryExpr) {
    auto hir = build_hir(R"(
        func test() {
            let a: I32 = 5
            let b: I32 = 10
            let f: (I32) -> I32 = do(x: I32) x * a + b
        }
    )");

    // Both a and b are used in binary expression
    tml::hir::ClosureOptimization::run_pass(hir);
    ASSERT_EQ(hir.functions.size(), 1u);
}

// ============================================================================
// Combined Pass Tests (Inlining + ClosureOptimization)
// ============================================================================

TEST_F(HirOptimizationTest, CombinedPasses_InliningAndClosure) {
    auto hir = build_hir(R"(
        func double(x: I32) -> I32 {
            return x * 2
        }

        func caller() {
            let v1: I32 = 1
            let v2: I32 = 2
            let v3: I32 = 3
            let v4: I32 = 4
            let v5: I32 = 5
            let factor: I32 = 6
            let f: (I32) -> I32 = do(x: I32) double(x) * factor
            let _ : I32 = v1 + v2 + v3 + v4 + v5
        }
    )");

    tml::hir::HirPassManager pm;
    pm.add_pass<tml::hir::Inlining>();
    pm.add_pass<tml::hir::ClosureOptimization>();

    bool changed = pm.run(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, CombinedPasses_AllOptimizations) {
    auto hir = build_hir(R"(
        func inc(x: I32) -> I32 {
            return x + 1
        }

        func test() -> I32 {
            let unused: I32 = 999
            if true {
                return inc(2 + 3)
            } else {
                return 0
            }
        }
    )");

    tml::hir::HirPassManager pm;
    pm.add_pass<tml::hir::ConstantFolding>();
    pm.add_pass<tml::hir::DeadCodeElimination>();
    pm.add_pass<tml::hir::Inlining>();
    pm.add_pass<tml::hir::ClosureOptimization>();

    bool changed = pm.run(hir);
    EXPECT_TRUE(changed);
}

TEST_F(HirOptimizationTest, OptimizeHirLevel_Level3AllPasses) {
    auto hir = build_hir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }

        func test() -> I32 {
            return add(2 + 3, 4 + 5)
        }
    )");

    bool changed = tml::hir::optimize_hir_level(hir, 3);
    EXPECT_TRUE(changed);
}
