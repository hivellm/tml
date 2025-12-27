#include "tml/borrow/checker.hpp"
#include "tml/parser/parser.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include <gtest/gtest.h>
#include <memory>

using namespace tml;
using namespace tml::borrow;
using namespace tml::parser;
using namespace tml::lexer;

class BorrowCheckerTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto check(const std::string& code) -> Result<bool, std::vector<BorrowError>> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module = parser.parse_module("test");
        EXPECT_TRUE(is_ok(module)) << "Parse failed";

        BorrowChecker checker;
        return checker.check_module(std::get<parser::Module>(module));
    }

    void check_ok(const std::string& code) {
        auto result = check(code);
        EXPECT_TRUE(is_ok(result)) << "Borrow check failed";
    }

    void check_error(const std::string& code, const std::string& expected_msg = "") {
        auto result = check(code);
        EXPECT_TRUE(is_err(result)) << "Expected borrow error";
        if (!expected_msg.empty() && is_err(result)) {
            const auto& errors = std::get<std::vector<BorrowError>>(result);
            bool found = false;
            for (const auto& e : errors) {
                if (e.message.find(expected_msg) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Expected error containing: " << expected_msg;
        }
    }
};

// ============================================================================
// Basic Ownership Tests
// ============================================================================

TEST_F(BorrowCheckerTest, SimpleVariable) {
    check_ok(R"(
        func test() {
            let x: I32 = 42
            let y: I32 = x
        }
    )");
}

TEST_F(BorrowCheckerTest, MutableVariable) {
    check_ok(R"(
        func test() {
            let mut x: I32 = 42
            x = 10
        }
    )");
}

TEST_F(BorrowCheckerTest, ImmutableAssignmentError) {
    check_error(R"(
        func test() {
            let x: I32 = 42
            x = 10
        }
    )", "not mutable");
}

// ============================================================================
// Borrow Tests
// ============================================================================

TEST_F(BorrowCheckerTest, SharedBorrow) {
    check_ok(R"(
        func test() {
            let x: I32 = 42
            let r: ref I32 = ref x
        }
    )");
}

TEST_F(BorrowCheckerTest, MultipleSharedBorrows) {
    check_ok(R"(
        func test() {
            let x: I32 = 42
            let r1: ref I32 = ref x
            let r2: ref I32 = ref x
        }
    )");
}

TEST_F(BorrowCheckerTest, MutableBorrow) {
    check_ok(R"(
        func test() {
            let mut x: I32 = 42
            let r: mut ref I32 = mut ref x
        }
    )");
}

TEST_F(BorrowCheckerTest, MutableBorrowOfImmutableError) {
    check_error(R"(
        func test() {
            let x: I32 = 42
            let r: mut ref I32 = mut ref x
        }
    )", "not declared as mutable");
}

TEST_F(BorrowCheckerTest, DoubleMutableBorrowError) {
    check_error(R"(
        func test() {
            let mut x: I32 = 42
            let r1: mut ref I32 = mut ref x
            let r2: mut ref I32 = mut ref x
        }
    )", "more than once");
}

TEST_F(BorrowCheckerTest, MixedBorrowError) {
    check_error(R"(
        func test() {
            let mut x: I32 = 42
            let r1: ref I32 = ref x
            let r2: mut ref I32 = mut ref x
        }
    )", "also borrowed as immutable");
}

TEST_F(BorrowCheckerTest, MixedBorrowErrorReverse) {
    check_error(R"(
        func test() {
            let mut x: I32 = 42
            let r1: mut ref I32 = mut ref x
            let r2: ref I32 = ref x
        }
    )", "also borrowed as mutable");
}

// ============================================================================
// Scope Tests
// ============================================================================

TEST_F(BorrowCheckerTest, BorrowInNestedScope) {
    check_ok(R"(
        func test() {
            let mut x: I32 = 42
            {
                let r: mut ref I32 = mut ref x
            }
            let r2: mut ref I32 = mut ref x
        }
    )");
}

TEST_F(BorrowCheckerTest, VariableShadowing) {
    check_ok(R"(
        func test() {
            let x: I32 = 1
            {
                let x: I32 = 2
            }
            let y: I32 = x
        }
    )");
}

// ============================================================================
// Control Flow Tests
// ============================================================================

TEST_F(BorrowCheckerTest, IfExpression) {
    check_ok(R"(
        func test(cond: Bool) {
            let x: I32 = 42
            if cond {
                let r: ref I32 = ref x
            }
            let y: I32 = x
        }
    )");
}

TEST_F(BorrowCheckerTest, LoopExpression) {
    check_ok(R"(
        func test() {
            let mut x: I32 = 0
            loop {
                x = x + 1
                if x > 10 {
                    break
                }
            }
        }
    )");
}

TEST_F(BorrowCheckerTest, ForExpression) {
    check_ok(R"(
        func test(items: [I32]) {
            for item in items {
                let x: I32 = item
            }
        }
    )");
}

TEST_F(BorrowCheckerTest, WhenExpression) {
    check_ok(R"(
        func test(x: I32) {
            when x {
                0 => 1,
                _ => 2,
            }
        }
    )");
}

// ============================================================================
// Function Parameter Tests
// ============================================================================

TEST_F(BorrowCheckerTest, FunctionWithParams) {
    check_ok(R"(
        func add(a: I32, b: I32) -> I32 {
            a + b
        }
    )");
}

TEST_F(BorrowCheckerTest, FunctionWithMutableParam) {
    check_ok(R"(
        func increment(mut x: I32) -> I32 {
            x = x + 1
            x
        }
    )");
}

TEST_F(BorrowCheckerTest, FunctionWithRefParam) {
    check_ok(R"(
        func get_value(x: ref I32) -> I32 {
            42
        }
    )");
}

TEST_F(BorrowCheckerTest, MethodWithThis) {
    check_ok(R"(
        type Counter {
            value: I32,
        }

        impl Counter {
            func get(this) -> I32 {
                42
            }
        }
    )");
}

// ============================================================================
// Closure Tests
// ============================================================================

TEST_F(BorrowCheckerTest, SimpleClosure) {
    check_ok(R"(
        func test() {
            let f: (I32) -> I32 = do(x: I32) x + 1
        }
    )");
}

TEST_F(BorrowCheckerTest, ClosureWithCapture) {
    check_ok(R"(
        func test() {
            let y: I32 = 10
            let f: (I32) -> I32 = do(x: I32) x + y
        }
    )");
}

// ============================================================================
// Struct Tests
// ============================================================================

TEST_F(BorrowCheckerTest, StructCreation) {
    check_ok(R"(
        type Point {
            x: I32,
            y: I32,
        }

        func test() {
            let p: Point = Point { x: 1, y: 2 }
        }
    )");
}

// ============================================================================
// Array and Tuple Tests
// ============================================================================

TEST_F(BorrowCheckerTest, ArrayCreation) {
    check_ok(R"(
        func test() {
            let arr: [I32] = [1, 2, 3]
        }
    )");
}

TEST_F(BorrowCheckerTest, TupleCreation) {
    check_ok(R"(
        func test() {
            let t: (I32, I32, I32) = (1, 2, 3)
        }
    )");
}

// ============================================================================
// Complex Programs
// ============================================================================

TEST_F(BorrowCheckerTest, CompleteProgram) {
    check_ok(R"(
        type Point {
            x: I32,
            y: I32,
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
            let p: Point = Point::new(10, 20)
            let d: I32 = p.distance()
        }
    )");
}

TEST_F(BorrowCheckerTest, NestedFunctions) {
    check_ok(R"(
        func outer() -> I32 {
            let x: I32 = 10
            let result: I32 = inner(x)
            result
        }

        func inner(x: I32) -> I32 {
            x * 2
        }
    )");
}
