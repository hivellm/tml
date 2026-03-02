// Tests for LLVM expression codegen:
// - Binary/unary operations, casts, calls, method dispatch
// - Struct/tuple/enum construction, field access
// - Closures, try expressions, collections

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace tml;
using namespace tml::codegen;
using namespace tml::types;
using namespace tml::parser;
using namespace tml::lexer;

class ExprCodegenTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto generate(const std::string& code) -> std::string {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto module_result = parser.parse_module("test");
        EXPECT_TRUE(is_ok(module_result));
        auto& module = std::get<parser::Module>(module_result);
        TypeChecker checker;
        auto env_result = checker.check_module(module);
        EXPECT_TRUE(is_ok(env_result));
        auto& env = std::get<TypeEnv>(env_result);
        LLVMIRGen gen(env);
        auto ir_result = gen.generate(module);
        if (is_err(ir_result)) {
            auto& errors = std::get<std::vector<LLVMGenError>>(ir_result);
            for (const auto& err : errors) {
                ADD_FAILURE() << "Codegen error: " << err.message;
            }
            return "";
        }
        return std::get<std::string>(ir_result);
    }

    void expect_ir_contains(const std::string& ir, const std::string& pattern,
                            const std::string& msg) {
        EXPECT_NE(ir.find(pattern), std::string::npos) << msg;
    }
};

// ============================================================================
// Binary Operations (binary.cpp / binary_ops.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, IntegerAddition) {
    auto ir = generate(R"(
        func main() -> I32 {
            return 2 + 3
        }
    )");
    expect_ir_contains(ir, "add", "IR should contain add instruction");
}

TEST_F(ExprCodegenTest, IntegerSubtraction) {
    auto ir = generate(R"(
        func main() -> I32 {
            return 10 - 3
        }
    )");
    expect_ir_contains(ir, "sub", "IR should contain sub instruction");
}

TEST_F(ExprCodegenTest, IntegerMultiplication) {
    auto ir = generate(R"(
        func main() -> I32 {
            return 4 * 5
        }
    )");
    expect_ir_contains(ir, "mul", "IR should contain mul instruction");
}

TEST_F(ExprCodegenTest, IntegerDivision) {
    auto ir = generate(R"(
        func main() -> I32 {
            return 10 / 2
        }
    )");
    // Division may be sdiv or udiv
    EXPECT_TRUE(ir.find("div") != std::string::npos) << "IR should contain division";
}

TEST_F(ExprCodegenTest, ComparisonLessThan) {
    auto ir = generate(R"(
        func main() -> Bool {
            return 1 < 2
        }
    )");
    expect_ir_contains(ir, "icmp", "IR should contain integer comparison");
}

TEST_F(ExprCodegenTest, ComparisonEqual) {
    auto ir = generate(R"(
        func main() -> Bool {
            return 42 == 42
        }
    )");
    expect_ir_contains(ir, "icmp eq", "IR should contain equality comparison");
}

TEST_F(ExprCodegenTest, LogicalAnd) {
    auto ir = generate(R"(
        func main() -> Bool {
            return true and false
        }
    )");
    // Logical and may be short-circuit (branch) or bitwise and
    EXPECT_FALSE(ir.empty());
}

TEST_F(ExprCodegenTest, LogicalOr) {
    auto ir = generate(R"(
        func main() -> Bool {
            return true or false
        }
    )");
    EXPECT_FALSE(ir.empty());
}

TEST_F(ExprCodegenTest, BitwiseAnd) {
    auto ir = generate(R"(
        func main() -> I32 {
            let a: I32 = 0xFF
            let b: I32 = 0x0F
            return a & b
        }
    )");
    expect_ir_contains(ir, "and", "IR should contain bitwise AND");
}

TEST_F(ExprCodegenTest, BitwiseOr) {
    auto ir = generate(R"(
        func main() -> I32 {
            let a: I32 = 0xF0
            let b: I32 = 0x0F
            return a | b
        }
    )");
    expect_ir_contains(ir, "or", "IR should contain bitwise OR");
}

TEST_F(ExprCodegenTest, ShiftLeft) {
    auto ir = generate(R"(
        func main() -> I32 {
            return 1 << 3
        }
    )");
    expect_ir_contains(ir, "shl", "IR should contain left shift");
}

// ============================================================================
// Unary Operations (unary.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, UnaryNegation) {
    auto ir = generate(R"(
        func main() -> I32 {
            return -42
        }
    )");
    EXPECT_FALSE(ir.empty());
}

TEST_F(ExprCodegenTest, LogicalNot) {
    auto ir = generate(R"(
        func main() -> Bool {
            return not true
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Cast Operations (cast.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, IntToFloat) {
    auto ir = generate(R"(
        func main() -> F64 {
            let x: I32 = 42
            return x as F64
        }
    )");
    expect_ir_contains(ir, "sitofp", "IR should contain signed int to float cast");
}

TEST_F(ExprCodegenTest, FloatToInt) {
    auto ir = generate(R"(
        func main() -> I32 {
            let x: F64 = 3.14
            return x as I32
        }
    )");
    expect_ir_contains(ir, "fptosi", "IR should contain float to signed int cast");
}

TEST_F(ExprCodegenTest, IntWidening) {
    auto ir = generate(R"(
        func main() -> I64 {
            let x: I32 = 42
            return x as I64
        }
    )");
    expect_ir_contains(ir, "sext", "IR should contain sign extension");
}

// ============================================================================
// Function Calls (call.cpp / call_user.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, SimpleCallUser) {
    auto ir = generate(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }

        func main() -> I32 {
            return add(1, 2)
        }
    )");
    expect_ir_contains(ir, "call", "IR should contain function call");
}

TEST_F(ExprCodegenTest, CallWithNoArgs) {
    auto ir = generate(R"(
        func constant() -> I32 {
            return 42
        }

        func main() -> I32 {
            return constant()
        }
    )");
    expect_ir_contains(ir, "call", "IR should contain function call");
}

// ============================================================================
// Struct Operations (struct.cpp / struct_field.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, StructConstruction) {
    auto ir = generate(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        func main() {
            let p: Point = Point { x: 10, y: 20 }
        }
    )");
    expect_ir_contains(ir, "%struct.Point", "IR should declare Point struct");
}

TEST_F(ExprCodegenTest, StructFieldAccess) {
    auto ir = generate(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> I32 {
            let p: Point = Point { x: 10, y: 20 }
            return p.x
        }
    )");
    expect_ir_contains(ir, "getelementptr", "IR should access struct field via GEP");
}

// ============================================================================
// Tuple Operations (tuple.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, TupleConstruction) {
    auto ir = generate(R"(
        func main() -> I32 {
            let t: (I32, I32) = (1, 2)
            return 0
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Enum Operations
// ============================================================================

TEST_F(ExprCodegenTest, EnumConstruction) {
    auto ir = generate(R"(
        type Color {
            Red,
            Green,
            Blue,
        }

        func main() {
            let c: Color = Green
        }
    )");
    expect_ir_contains(ir, "store i32", "IR should store enum tag");
}

// ============================================================================
// Collection Expressions (collections.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, ArrayLiteral) {
    auto ir = generate(R"(
        func main() {
            let arr: [I32; 3] = [1, 2, 3]
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Print Expression (print.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, PrintString) {
    auto ir = generate(R"(
        func main() {
            print("hello world\n")
        }
    )");
    expect_ir_contains(ir, "call", "IR should contain print call");
}

TEST_F(ExprCodegenTest, PrintInteger) {
    auto ir = generate(R"(
        func main() {
            print("{}\n", 42)
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Control Flow (if.cpp / loop.cpp / when.cpp / return.cpp)
// ============================================================================

TEST_F(ExprCodegenTest, IfExpression) {
    auto ir = generate(R"(
        func abs(x: I32) -> I32 {
            if x >= 0 {
                return x
            } else {
                return 0 - x
            }
        }
    )");
    expect_ir_contains(ir, "br", "IR should contain branch instruction");
    expect_ir_contains(ir, "icmp", "IR should contain comparison for condition");
}

TEST_F(ExprCodegenTest, LoopExpression) {
    auto ir = generate(R"(
        func count() -> I32 {
            var i: I32 = 0
            loop {
                if i >= 10 { break }
                i = i + 1
            }
            return i
        }
    )");
    expect_ir_contains(ir, "br", "IR should contain loop branches");
}

TEST_F(ExprCodegenTest, WhenExpression) {
    auto ir = generate(R"(
        type Shape {
            Circle(F64),
            Square(F64),
        }

        func describe(s: Shape) -> I32 {
            when s {
                Circle(r) -> return 1,
                Square(side) -> return 2,
            }
        }
    )");
    EXPECT_FALSE(ir.empty());
}

TEST_F(ExprCodegenTest, ReturnExpression) {
    auto ir = generate(R"(
        func early(x: I32) -> I32 {
            if x > 0 {
                return x
            }
            return 0
        }
    )");
    expect_ir_contains(ir, "ret", "IR should contain return instruction");
}

// ============================================================================
// Method Calls
// ============================================================================

TEST_F(ExprCodegenTest, StaticMethodCall) {
    auto ir = generate(R"(
        struct Counter {
            value: I32,
        }

        impl Counter {
            func new() -> Counter {
                return Counter { value: 0 }
            }
        }

        func main() {
            let c: Counter = Counter::new()
        }
    )");
    EXPECT_FALSE(ir.empty());
}
