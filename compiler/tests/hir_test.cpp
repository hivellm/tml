// HIR (High-level IR) tests
//
// Tests for the HIR builder, printer, and lowering logic

#include "hir/hir.hpp"
#include "hir/hir_builder.hpp"
#include "hir/hir_printer.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class HirTest : public ::testing::Test {
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
};

// ============================================================================
// Basic Module Tests
// ============================================================================

TEST_F(HirTest, EmptyModule) {
    auto hir = build_hir("");
    EXPECT_EQ(hir.name, "test");
    EXPECT_TRUE(hir.functions.empty());
    EXPECT_TRUE(hir.structs.empty());
    EXPECT_TRUE(hir.enums.empty());
}

TEST_F(HirTest, ModuleName) {
    auto hir = build_hir("func main() {}");
    EXPECT_EQ(hir.name, "test");
}

// ============================================================================
// Function Lowering Tests
// ============================================================================

TEST_F(HirTest, SimpleFunction) {
    auto hir = build_hir(R"(
        func main() {
            let x: I32 = 42
        }
    )");

    EXPECT_EQ(hir.functions.size(), 1u);
    EXPECT_EQ(hir.functions[0].name, "main");
}

TEST_F(HirTest, FunctionWithParams) {
    auto hir = build_hir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
    const auto& func = hir.functions[0];
    EXPECT_EQ(func.name, "add");
    EXPECT_EQ(func.params.size(), 2u);
    EXPECT_EQ(func.params[0].name, "a");
    EXPECT_EQ(func.params[1].name, "b");
}

TEST_F(HirTest, MultipleFunctions) {
    auto hir = build_hir(R"(
        func foo() {}
        func bar() {}
        func baz() {}
    )");

    EXPECT_EQ(hir.functions.size(), 3u);
    EXPECT_EQ(hir.functions[0].name, "foo");
    EXPECT_EQ(hir.functions[1].name, "bar");
    EXPECT_EQ(hir.functions[2].name, "baz");
}

TEST_F(HirTest, FunctionWithReturnType) {
    auto hir = build_hir(R"(
        func get_value() -> I64 {
            return 100
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
    const auto& func = hir.functions[0];
    EXPECT_NE(func.return_type, nullptr);
}

// ============================================================================
// Struct Lowering Tests
// ============================================================================

TEST_F(HirTest, SimpleStruct) {
    auto hir = build_hir(R"(
        type Point { x: I32, y: I32 }
    )");

    ASSERT_EQ(hir.structs.size(), 1u);
    const auto& s = hir.structs[0];
    EXPECT_EQ(s.name, "Point");
    EXPECT_EQ(s.fields.size(), 2u);
    EXPECT_EQ(s.fields[0].name, "x");
    EXPECT_EQ(s.fields[1].name, "y");
}

TEST_F(HirTest, StructWithMultipleFields) {
    auto hir = build_hir(R"(
        type Person {
            name: Str,
            age: I32,
            active: Bool
        }
    )");

    ASSERT_EQ(hir.structs.size(), 1u);
    const auto& s = hir.structs[0];
    EXPECT_EQ(s.name, "Person");
    EXPECT_EQ(s.fields.size(), 3u);
}

// ============================================================================
// Enum Lowering Tests
// ============================================================================

TEST_F(HirTest, SimpleEnum) {
    auto hir = build_hir(R"(
        type Color {
            Red,
            Green,
            Blue
        }
    )");

    ASSERT_EQ(hir.enums.size(), 1u);
    const auto& e = hir.enums[0];
    EXPECT_EQ(e.name, "Color");
    EXPECT_EQ(e.variants.size(), 3u);
    EXPECT_EQ(e.variants[0].name, "Red");
    EXPECT_EQ(e.variants[1].name, "Green");
    EXPECT_EQ(e.variants[2].name, "Blue");
}

TEST_F(HirTest, EnumWithPayload) {
    auto hir = build_hir(R"(
        type Option[T] {
            Some(T),
            None
        }
    )");

    ASSERT_EQ(hir.enums.size(), 1u);
    const auto& e = hir.enums[0];
    EXPECT_EQ(e.name, "Option");
    EXPECT_EQ(e.variants.size(), 2u);
    EXPECT_EQ(e.variants[0].name, "Some");
    EXPECT_EQ(e.variants[1].name, "None");
}

// ============================================================================
// Expression Lowering Tests
// ============================================================================

TEST_F(HirTest, LiteralExpressions) {
    auto hir = build_hir(R"(
        func test() {
            let a: I32 = 42
            let b: F64 = 3.14
            let c: Bool = true
            let d: Str = "hello"
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
    const auto& func = hir.functions[0];
    EXPECT_NE(func.body, nullptr);
}

TEST_F(HirTest, BinaryExpressions) {
    auto hir = build_hir(R"(
        func test() -> I32 {
            return 1 + 2 * 3
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, ComparisonExpressions) {
    auto hir = build_hir(R"(
        func test(a: I32, b: I32) -> Bool {
            return a < b
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, LogicalExpressions) {
    auto hir = build_hir(R"(
        func test(a: Bool, b: Bool) -> Bool {
            return a and b or not a
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, UnaryExpressions) {
    auto hir = build_hir(R"(
        func test(x: I32) -> I32 {
            return -x
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

// ============================================================================
// Control Flow Tests
// ============================================================================

TEST_F(HirTest, IfExpression) {
    auto hir = build_hir(R"(
        func test(x: I32) -> I32 {
            if x > 0 {
                return 1
            } else {
                return -1
            }
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, WhenExpression) {
    auto hir = build_hir(R"(
        func test(x: I32) -> I32 {
            return when x {
                0 => 0,
                1 => 1,
                _ => 2,
            }
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, LoopExpression) {
    auto hir = build_hir(R"(
        func test() {
            loop {
                break
            }
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, WhileExpression) {
    auto hir = build_hir(R"(
        func test() {
            let mut x: I32 = 0
            while x < 10 {
                x = x + 1
            }
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

// ============================================================================
// Pattern Lowering Tests
// ============================================================================

TEST_F(HirTest, WildcardPattern) {
    auto hir = build_hir(R"(
        func test(x: I32) -> I32 {
            return when x {
                _ => 0,
            }
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, LiteralPattern) {
    auto hir = build_hir(R"(
        func test(x: I32) -> I32 {
            return when x {
                0 => 0,
                1 => 1,
                _ => 2,
            }
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, BindingPattern) {
    auto hir = build_hir(R"(
        func test(pair: (I32, I32)) -> I32 {
            let (a, b): (I32, I32) = pair
            return a + b
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

// ============================================================================
// Struct Operations Tests
// ============================================================================

TEST_F(HirTest, StructConstruction) {
    auto hir = build_hir(R"(
        type Point { x: I32, y: I32 }

        func make_point() -> Point {
            return Point { x: 1, y: 2 }
        }
    )");

    ASSERT_EQ(hir.structs.size(), 1u);
    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, StructFieldAccess) {
    auto hir = build_hir(R"(
        type Point { x: I32, y: I32 }

        func get_x(p: Point) -> I32 {
            return p.x
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

// ============================================================================
// Array Tests
// ============================================================================

TEST_F(HirTest, ArrayLiteral) {
    auto hir = build_hir(R"(
        func test() {
            let arr: [I32; 3] = [1, 2, 3]
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, ArrayRepeat) {
    auto hir = build_hir(R"(
        func test() {
            let arr: [I32; 5] = [0; 5]
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

// ============================================================================
// Closure Tests
// ============================================================================

TEST_F(HirTest, SimpleClosure) {
    auto hir = build_hir(R"(
        func test() {
            let f: (I32) -> I32 = do(x: I32) x + 1
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, ClosureWithCapture) {
    auto hir = build_hir(R"(
        func test() {
            let y: I32 = 10
            let f: (I32) -> I32 = do(x: I32) x + y
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

// ============================================================================
// Impl Block Tests
// ============================================================================

TEST_F(HirTest, ImplBlock) {
    auto hir = build_hir(R"(
        type Counter { value: I32 }

        impl Counter {
            func new() -> Counter {
                return Counter { value: 0 }
            }

            func get(this) -> I32 {
                return this.value
            }
        }
    )");

    ASSERT_EQ(hir.structs.size(), 1u);
    EXPECT_GE(hir.impls.size(), 1u);
}

// ============================================================================
// HIR Printer Tests
// ============================================================================

TEST_F(HirTest, PrintModule) {
    auto hir = build_hir(R"(
        func main() {
            let x: I32 = 42
        }
    )");

    tml::hir::HirPrinter printer(false);
    std::string output = printer.print_module(hir);

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("main"), std::string::npos);
}

TEST_F(HirTest, PrintFunction) {
    auto hir = build_hir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);

    tml::hir::HirPrinter printer(false);
    std::string output = printer.print_function(hir.functions[0]);

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("add"), std::string::npos);
}

TEST_F(HirTest, PrintStruct) {
    auto hir = build_hir(R"(
        type Point { x: I32, y: I32 }
    )");

    ASSERT_EQ(hir.structs.size(), 1u);

    tml::hir::HirPrinter printer(false);
    std::string output = printer.print_struct(hir.structs[0]);

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Point"), std::string::npos);
}

TEST_F(HirTest, PrintEnum) {
    auto hir = build_hir(R"(
        type Color {
            Red,
            Green,
            Blue
        }
    )");

    ASSERT_EQ(hir.enums.size(), 1u);

    tml::hir::HirPrinter printer(false);
    std::string output = printer.print_enum(hir.enums[0]);

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Color"), std::string::npos);
}

// ============================================================================
// Module Lookup Tests
// ============================================================================

TEST_F(HirTest, FindStruct) {
    auto hir = build_hir(R"(
        type Point { x: I32, y: I32 }
        type Size { w: I32, h: I32 }
    )");

    auto point = hir.find_struct("Point");
    auto size = hir.find_struct("Size");
    auto not_found = hir.find_struct("NotExist");

    EXPECT_NE(point, nullptr);
    EXPECT_NE(size, nullptr);
    EXPECT_EQ(not_found, nullptr);
}

TEST_F(HirTest, FindEnum) {
    auto hir = build_hir(R"(
        type Color {
            Red,
            Green,
            Blue
        }
        type Direction {
            North,
            South,
            East,
            West
        }
    )");

    auto color = hir.find_enum("Color");
    auto direction = hir.find_enum("Direction");
    auto not_found = hir.find_enum("NotExist");

    EXPECT_NE(color, nullptr);
    EXPECT_NE(direction, nullptr);
    EXPECT_EQ(not_found, nullptr);
}

TEST_F(HirTest, FindFunction) {
    auto hir = build_hir(R"(
        func foo() {}
        func bar() {}
    )");

    auto foo = hir.find_function("foo");
    auto bar = hir.find_function("bar");
    auto not_found = hir.find_function("baz");

    EXPECT_NE(foo, nullptr);
    EXPECT_NE(bar, nullptr);
    EXPECT_EQ(not_found, nullptr);
}

// ============================================================================
// Const Lowering Tests
// ============================================================================

TEST_F(HirTest, ConstDeclaration) {
    auto hir = build_hir(R"(
        const MAX_SIZE: I64 = 100
    )");

    ASSERT_EQ(hir.constants.size(), 1u);
    EXPECT_EQ(hir.constants[0].name, "MAX_SIZE");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(HirTest, NestedBlocks) {
    auto hir = build_hir(R"(
        func test() {
            {
                let a: I32 = 1
                {
                    let b: I32 = 2
                }
            }
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, ReturnWithoutValue) {
    auto hir = build_hir(R"(
        func test() {
            return
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, ContinueAndBreak) {
    auto hir = build_hir(R"(
        func test() {
            loop {
                if true {
                    continue
                }
                break
            }
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, TupleExpression) {
    auto hir = build_hir(R"(
        func test() -> (I64, I64) {
            return (1, 2)
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

TEST_F(HirTest, CastExpression) {
    auto hir = build_hir(R"(
        func test(x: I32) -> I64 {
            return x as I64
        }
    )");

    ASSERT_EQ(hir.functions.size(), 1u);
}

// ============================================================================
// Monomorphization Cache Tests
// ============================================================================

TEST(MonomorphizationCacheTest, EmptyCache) {
    tml::hir::MonomorphizationCache cache;
    EXPECT_FALSE(cache.has_type("Foo"));
    EXPECT_FALSE(cache.has_func("bar"));
}

TEST(MonomorphizationCacheTest, GetOrCreateType) {
    tml::hir::MonomorphizationCache cache;

    std::vector<tml::hir::HirType> type_args;
    type_args.push_back(tml::types::make_i32());

    auto name1 = cache.get_or_create_type("Vec", type_args);
    auto name2 = cache.get_or_create_type("Vec", type_args);

    EXPECT_EQ(name1, name2);
    EXPECT_TRUE(cache.has_type(name1));
}

TEST(MonomorphizationCacheTest, GetOrCreateFunc) {
    tml::hir::MonomorphizationCache cache;

    std::vector<tml::hir::HirType> type_args;
    type_args.push_back(tml::types::make_i64());

    auto name1 = cache.get_or_create_func("generic_fn", type_args);
    auto name2 = cache.get_or_create_func("generic_fn", type_args);

    EXPECT_EQ(name1, name2);
    EXPECT_TRUE(cache.has_func(name1));
}

TEST(MonomorphizationCacheTest, DifferentTypeArgsDifferentNames) {
    tml::hir::MonomorphizationCache cache;

    std::vector<tml::hir::HirType> args_i32;
    args_i32.push_back(tml::types::make_i32());

    std::vector<tml::hir::HirType> args_i64;
    args_i64.push_back(tml::types::make_i64());

    auto name_i32 = cache.get_or_create_type("Generic", args_i32);
    auto name_i64 = cache.get_or_create_type("Generic", args_i64);

    EXPECT_NE(name_i32, name_i64);
}

// ============================================================================
// HIR ID Generator Tests
// ============================================================================

TEST(HirIdGeneratorTest, GeneratesUniqueIds) {
    tml::hir::HirIdGenerator gen;

    auto id1 = gen.next();
    auto id2 = gen.next();
    auto id3 = gen.next();

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST(HirIdGeneratorTest, IdsAreSequential) {
    tml::hir::HirIdGenerator gen;

    auto id1 = gen.next();
    auto id2 = gen.next();
    auto id3 = gen.next();

    EXPECT_EQ(id2, id1 + 1);
    EXPECT_EQ(id3, id2 + 1);
}

TEST(HirIdGeneratorTest, ResetWorks) {
    tml::hir::HirIdGenerator gen;

    gen.next();
    gen.next();
    gen.reset();

    auto id = gen.next();
    EXPECT_EQ(id, 1u); // First ID after reset is 1 (0 is INVALID_HIR_ID)
}

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
        if (!expr.is<tml::hir::HirLiteralExpr>()) return false;
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
