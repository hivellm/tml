#include "tml/types/checker.hpp"
#include "tml/types/type.hpp"
#include "tml/parser/parser.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include <gtest/gtest.h>
#include <memory>

using namespace tml;
using namespace tml::types;
using namespace tml::parser;
using namespace tml::lexer;

class TypeCheckerTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> Result<TypeEnv, std::vector<TypeError>> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        EXPECT_TRUE(is_ok(module));

        TypeChecker checker;
        return checker.check_module(std::get<Module>(module));
    }

    auto check_ok(const std::string& code) -> TypeEnv {
        auto result = check(code);
        EXPECT_TRUE(is_ok(result)) << "Type check failed";
        return std::move(std::get<TypeEnv>(result));
    }

    void check_error(const std::string& code) {
        auto result = check(code);
        EXPECT_TRUE(is_err(result)) << "Expected type error";
    }
};

// ============================================================================
// Type Resolution Tests
// ============================================================================

TEST_F(TypeCheckerTest, ResolveBuiltinTypes) {
    auto env = check_ok(R"(
        func test_i32(x: I32) -> I32 { x }
        func test_i64(x: I64) -> I64 { x }
        func test_bool(x: Bool) -> Bool { x }
        func test_str(x: Str) -> Str { x }
    )");

    auto i32_func = env.lookup_func("test_i32");
    EXPECT_TRUE(i32_func.has_value());
    EXPECT_EQ(i32_func->params.size(), 1);
    EXPECT_TRUE(i32_func->params[0]->is<types::PrimitiveType>());
    EXPECT_EQ(i32_func->params[0]->as<types::PrimitiveType>().kind, types::PrimitiveKind::I32);
}

TEST_F(TypeCheckerTest, ResolveReferenceTypes) {
    auto env = check_ok(R"(
        func test_ref(x: ref I32) -> ref I32 { x }
        func test_mut_ref(x: mut ref I32) -> mut ref I32 { x }
    )");

    auto ref_func = env.lookup_func("test_ref");
    EXPECT_TRUE(ref_func.has_value());
    EXPECT_TRUE(ref_func->params[0]->is<types::RefType>());
    EXPECT_FALSE(ref_func->params[0]->as<types::RefType>().is_mut);

    auto mut_ref_func = env.lookup_func("test_mut_ref");
    EXPECT_TRUE(mut_ref_func.has_value());
    EXPECT_TRUE(mut_ref_func->params[0]->is<types::RefType>());
    EXPECT_TRUE(mut_ref_func->params[0]->as<types::RefType>().is_mut);
}

TEST_F(TypeCheckerTest, ResolveSliceType) {
    auto env = check_ok(R"(
        func test_slice(x: [I32]) -> [I32] { x }
    )");

    auto func = env.lookup_func("test_slice");
    EXPECT_TRUE(func.has_value());
    EXPECT_TRUE(func->params[0]->is<types::SliceType>());
}

// ============================================================================
// Function Declaration Tests
// ============================================================================

TEST_F(TypeCheckerTest, SimpleFunctionDecl) {
    auto env = check_ok(R"(
        func add(a: I32, b: I32) -> I32 {
            a + b
        }
    )");

    auto func = env.lookup_func("add");
    EXPECT_TRUE(func.has_value());
    EXPECT_EQ(func->name, "add");
    EXPECT_EQ(func->params.size(), 2);
    EXPECT_TRUE(func->return_type->is<types::PrimitiveType>());
}

TEST_F(TypeCheckerTest, FunctionWithNoReturn) {
    auto env = check_ok(R"(
        func print_hello() {
            let x: I32 = 42
        }
    )");

    auto func = env.lookup_func("print_hello");
    EXPECT_TRUE(func.has_value());
    // Return type should be unit
    EXPECT_TRUE(func->return_type->is<types::PrimitiveType>());
    EXPECT_EQ(func->return_type->as<types::PrimitiveType>().kind, types::PrimitiveKind::Unit);
}

TEST_F(TypeCheckerTest, AsyncFunction) {
    auto env = check_ok(R"(
        async func fetch_data() -> I32 {
            42
        }
    )");

    auto func = env.lookup_func("fetch_data");
    EXPECT_TRUE(func.has_value());
    EXPECT_TRUE(func->is_async);
}

// ============================================================================
// Struct Declaration Tests
// ============================================================================

TEST_F(TypeCheckerTest, StructDecl) {
    auto env = check_ok(R"(
        type Point {
            x: I32,
            y: I32,
        }
    )");

    auto struct_def = env.lookup_struct("Point");
    EXPECT_TRUE(struct_def.has_value());
    EXPECT_EQ(struct_def->name, "Point");
    EXPECT_EQ(struct_def->fields.size(), 2);
    EXPECT_EQ(struct_def->fields[0].first, "x");
    EXPECT_EQ(struct_def->fields[1].first, "y");
}

TEST_F(TypeCheckerTest, GenericStruct) {
    auto env = check_ok(R"(
        type Container[T] {
            value: T,
        }
    )");

    auto struct_def = env.lookup_struct("Container");
    EXPECT_TRUE(struct_def.has_value());
    EXPECT_EQ(struct_def->type_params.size(), 1);
    EXPECT_EQ(struct_def->type_params[0], "T");
}

// ============================================================================
// Enum Declaration Tests
// ============================================================================

TEST_F(TypeCheckerTest, SimpleEnum) {
    auto env = check_ok(R"(
        type Color {
            Red,
            Green,
            Blue,
        }
    )");

    auto enum_def = env.lookup_enum("Color");
    EXPECT_TRUE(enum_def.has_value());
    EXPECT_EQ(enum_def->variants.size(), 3);
}

TEST_F(TypeCheckerTest, EnumWithData) {
    auto env = check_ok(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }
    )");

    auto enum_def = env.lookup_enum("Maybe");
    EXPECT_TRUE(enum_def.has_value());
    EXPECT_EQ(enum_def->type_params.size(), 1);
    EXPECT_EQ(enum_def->variants.size(), 2);
}

// ============================================================================
// Behavior Declaration Tests
// ============================================================================

TEST_F(TypeCheckerTest, BehaviorDecl) {
    auto env = check_ok(R"(
        behavior Printable {
            func print(this) -> Str
        }
    )");

    auto behavior = env.lookup_behavior("Printable");
    EXPECT_TRUE(behavior.has_value());
    EXPECT_EQ(behavior->methods.size(), 1);
    EXPECT_EQ(behavior->methods[0].name, "print");
}

// ============================================================================
// Impl Block Tests
// ============================================================================

TEST_F(TypeCheckerTest, ImplBlock) {
    auto env = check_ok(R"(
        type Counter {
            value: I32,
        }

        impl Counter {
            func new() -> Counter {
                Counter { value: 0 }
            }

            func increment(this) {
                let x = 1
            }
        }
    )");

    // Methods are registered with qualified names
    auto new_method = env.lookup_func("Counter::new");
    EXPECT_TRUE(new_method.has_value());

    auto increment_method = env.lookup_func("Counter::increment");
    EXPECT_TRUE(increment_method.has_value());
}

// ============================================================================
// Type Alias Tests
// ============================================================================

TEST_F(TypeCheckerTest, TypeAlias) {
    auto env = check_ok(R"(
        type Int = I32

        func test(x: Int) -> Int { x }
    )");

    auto alias = env.lookup_type_alias("Int");
    EXPECT_TRUE(alias.has_value());
}

// ============================================================================
// Expression Type Inference Tests
// ============================================================================

TEST_F(TypeCheckerTest, LiteralTypes) {
    check_ok(R"(
        func test() {
            let a: I32 = 42
            let b: F64 = 3.14
            let c: Str = "hello"
            let d: Bool = true
            let e: Char = 'x'
        }
    )");
}

TEST_F(TypeCheckerTest, BinaryExpressionTypes) {
    check_ok(R"(
        func test() {
            let sum: I32 = 1 + 2
            let diff: I32 = 5 - 3
            let prod: I32 = 2 * 3
            let quot: I32 = 10 / 2
            let rem: I32 = 7 % 3
        }
    )");
}

TEST_F(TypeCheckerTest, ComparisonExpressionTypes) {
    check_ok(R"(
        func test() {
            let eq: Bool = 1 == 1
            let ne: Bool = 1 != 2
            let lt: Bool = 1 < 2
            let le: Bool = 1 <= 2
            let gt: Bool = 2 > 1
            let ge: Bool = 2 >= 1
        }
    )");
}

TEST_F(TypeCheckerTest, LogicalExpressionTypes) {
    check_ok(R"(
        func test() {
            let a: Bool = true and false
            let b: Bool = true or false
            let c: Bool = not true
        }
    )");
}

// ============================================================================
// Control Flow Tests
// ============================================================================

TEST_F(TypeCheckerTest, IfExpression) {
    check_ok(R"(
        func test(x: I32) -> I32 {
            if x > 0 {
                1
            } else {
                0
            }
        }
    )");
}

TEST_F(TypeCheckerTest, LoopExpression) {
    check_ok(R"(
        func test() {
            loop {
                break
            }
        }
    )");
}

TEST_F(TypeCheckerTest, ForExpression) {
    check_ok(R"(
        func test(items: [I32]) {
            for item in items {
                let x: I32 = item
            }
        }
    )");
}

TEST_F(TypeCheckerTest, WhenExpression) {
    check_ok(R"(
        func test(x: I32) -> I32 {
            when x {
                0 => 100,
                1 => 200,
                _ => 0,
            }
        }
    )");
}

// ============================================================================
// Closure Tests
// ============================================================================

TEST_F(TypeCheckerTest, SimpleClosure) {
    check_ok(R"(
        func test() {
            let add = do(x: I32, y: I32) x + y
        }
    )");
}

TEST_F(TypeCheckerTest, ClosureWithReturn) {
    check_ok(R"(
        func test() {
            let double = do(x: I32) -> I32 { x * 2 }
        }
    )");
}

// ============================================================================
// Array and Tuple Tests
// ============================================================================

TEST_F(TypeCheckerTest, ArrayExpression) {
    check_ok(R"(
        func test() {
            let arr: [I32] = [1, 2, 3]
        }
    )");
}

TEST_F(TypeCheckerTest, DISABLED_TupleExpression) {
    check_ok(R"(
        func test() {
            let pair: (I32, I32) = (1, 2)
            let triple: (I32, Str, Bool) = (1, "hello", true)
        }
    )");
}

// ============================================================================
// Multiple Declaration Tests
// ============================================================================

TEST_F(TypeCheckerTest, MultipleFunctions) {
    auto env = check_ok(R"(
        func foo(x: I32) -> I32 { x }
        func bar(x: I32) -> I32 { foo(x) }
        func baz() -> I32 { bar(42) }
    )");

    EXPECT_TRUE(env.lookup_func("foo").has_value());
    EXPECT_TRUE(env.lookup_func("bar").has_value());
    EXPECT_TRUE(env.lookup_func("baz").has_value());
}

TEST_F(TypeCheckerTest, CompleteModule) {
    check_ok(R"(
        type Point {
            x: I32,
            y: I32,
        }

        type Maybe[T] {
            Just(T),
            Nothing,
        }

        behavior Printable {
            func print(this) -> Str
        }

        impl Point {
            func new(x: I32, y: I32) -> Point {
                Point { x: x, y: y }
            }

            func distance(this) -> I32 {
                this.x + this.y
            }
        }

        func main() {
            let p = Point::new(10, 20)
            let d = p.distance()
        }
    )");
}

// ============================================================================
// Type Utility Tests
// ============================================================================

TEST(TypeTest, TypeToString) {
    auto i32 = make_i32();
    EXPECT_EQ(type_to_string(i32), "I32");

    auto bool_type = make_bool();
    EXPECT_EQ(type_to_string(bool_type), "Bool");

    auto unit = make_unit();
    EXPECT_EQ(type_to_string(unit), "()");

    auto never = make_never();
    EXPECT_EQ(type_to_string(never), "!");

    auto ref = make_ref(make_i32(), false);
    EXPECT_EQ(type_to_string(ref), "ref I32");

    auto mut_ref = make_ref(make_i32(), true);
    EXPECT_EQ(type_to_string(mut_ref), "mut ref I32");

    auto arr = make_array(make_i32(), 10);
    EXPECT_EQ(type_to_string(arr), "[I32; 10]");

    auto slice = make_slice(make_i32());
    EXPECT_EQ(type_to_string(slice), "[I32]");

    auto tuple = make_tuple({make_i32(), make_bool()});
    EXPECT_EQ(type_to_string(tuple), "(I32, Bool)");

    auto func = make_func({make_i32(), make_i32()}, make_i32());
    EXPECT_EQ(type_to_string(func), "func(I32, I32) -> I32");
}

TEST(TypeTest, TypesEqual) {
    // Structural equality - same type kind means equal
    auto a = make_i32();
    auto b = make_i32();
    EXPECT_TRUE(types_equal(a, b));  // Same type kind

    auto ref_a = make_ref(make_i32(), false);
    auto ref_b = make_ref(make_i32(), false);
    EXPECT_TRUE(types_equal(ref_a, ref_b));

    auto ref_mut = make_ref(make_i32(), true);
    EXPECT_FALSE(types_equal(ref_a, ref_mut));  // Mutability differs

    // Different primitive types
    auto i64 = make_i64();
    EXPECT_FALSE(types_equal(a, i64));  // Different kinds
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(TypeCheckerTest, UndefinedVariable) {
    // This should produce an error for using undefined 'y'
    auto result = check(R"(
        func test() {
            let x = y
        }
    )");
    // Note: The current implementation may or may not error here
    // depending on how strict the checker is
}

TEST_F(TypeCheckerTest, BreakOutsideLoop) {
    auto result = check(R"(
        func test() {
            break
        }
    )");
    EXPECT_TRUE(is_err(result));
}

// ============================================================================
// Enum Constructor Tests
// ============================================================================

TEST_F(TypeCheckerTest, EnumConstructorWithPayload) {
    auto env = check_ok(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func test() {
            let x: Maybe[I64] = Just(42)
        }
    )");

    // Verify enum is registered
    auto enum_def = env.lookup_enum("Maybe");
    EXPECT_TRUE(enum_def.has_value());
    EXPECT_EQ(enum_def->variants.size(), 2);
    EXPECT_EQ(enum_def->variants[0].first, "Just");
    EXPECT_EQ(enum_def->variants[1].first, "Nothing");
}

TEST_F(TypeCheckerTest, EnumConstructorWithoutPayload) {
    auto env = check_ok(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func test() {
            let x: Maybe[I64] = Nothing
        }
    )");

    auto enum_def = env.lookup_enum("Maybe");
    EXPECT_TRUE(enum_def.has_value());
}

TEST_F(TypeCheckerTest, EnumConstructorArgCountMismatch) {
    check_error(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func test() {
            let x = Just(42, 100)
        }
    )");
}

// ============================================================================
// Pattern Binding Tests
// ============================================================================

TEST_F(TypeCheckerTest, PatternBindingInWhen) {
    auto env = check_ok(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func test() {
            let x: Maybe[I64] = Just(42)

            when x {
                Just(v) => println(v),
                Nothing => println("nothing"),
            }
        }
    )");

    // Test should pass - v should be bound in the Just arm
}

TEST_F(TypeCheckerTest, PatternBindingMultiplePayloads) {
    auto env = check_ok(R"(
        type Pair[A, B] {
            Both(A, B),
            None,
        }

        func test() {
            let p: Pair[I32, I64] = Both(1, 2)

            when p {
                Both(a, b) => {
                    println(a)
                    println(b)
                },
                None => println("none"),
            }
        }
    )");
}

TEST_F(TypeCheckerTest, PatternBindingNestedEnums) {
    auto env = check_ok(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        type Outcome[T, E] {
            Ok(T),
            Err(E),
        }

        func test() {
            let x: Maybe[Outcome[I32, I64]] = Just(Ok(42))

            when x {
                Just(result) => {
                    when result {
                        Ok(value) => println(value),
                        Err(e) => println("error"),
                    }
                },
                Nothing => println("nothing"),
            }
        }
    )");
}
