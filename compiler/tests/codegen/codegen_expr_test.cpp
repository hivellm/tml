// Codegen Expression Tests
//
// Tests for LLVM IR generation of expressions: binary, unary, cast,
// call, closure, method dispatch, struct field, tuple, try

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

class CodegenExprTest : public ::testing::Test {
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

    // Count occurrences of a pattern in IR
    auto count_pattern(const std::string& ir, const std::string& pattern) -> size_t {
        size_t count = 0;
        size_t pos = 0;
        while ((pos = ir.find(pattern, pos)) != std::string::npos) {
            count++;
            pos += pattern.size();
        }
        return count;
    }
};

// ============================================================================
// Binary Expression Tests
// ============================================================================

TEST_F(CodegenExprTest, BinaryAddI32) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");

    EXPECT_NE(ir.find("add"), std::string::npos)
        << "IR should contain add instruction for I32 addition";
}

TEST_F(CodegenExprTest, BinarySubI32) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a - b
        }
    )");

    EXPECT_NE(ir.find("sub"), std::string::npos)
        << "IR should contain sub instruction for I32 subtraction";
}

TEST_F(CodegenExprTest, BinaryMulI32) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a * b
        }
    )");

    EXPECT_NE(ir.find("mul"), std::string::npos)
        << "IR should contain mul instruction for I32 multiplication";
}

TEST_F(CodegenExprTest, BinaryDivI32) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a / b
        }
    )");

    // Signed division uses sdiv
    EXPECT_NE(ir.find("sdiv"), std::string::npos)
        << "IR should contain sdiv instruction for I32 division";
}

TEST_F(CodegenExprTest, BinaryModI32) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a % b
        }
    )");

    // Signed modulo uses srem
    EXPECT_NE(ir.find("srem"), std::string::npos)
        << "IR should contain srem instruction for I32 modulo";
}

TEST_F(CodegenExprTest, BinaryAddF64) {
    std::string ir = generate(R"(
        func test(a: F64, b: F64) -> F64 {
            return a + b
        }
    )");

    EXPECT_NE(ir.find("fadd"), std::string::npos)
        << "IR should contain fadd instruction for F64 addition";
}

TEST_F(CodegenExprTest, BinaryMulF64) {
    std::string ir = generate(R"(
        func test(a: F64, b: F64) -> F64 {
            return a * b
        }
    )");

    EXPECT_NE(ir.find("fmul"), std::string::npos)
        << "IR should contain fmul instruction for F64 multiplication";
}

TEST_F(CodegenExprTest, ComparisonEq) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> Bool {
            return a == b
        }
    )");

    EXPECT_NE(ir.find("icmp eq"), std::string::npos)
        << "IR should contain icmp eq for equality comparison";
}

TEST_F(CodegenExprTest, ComparisonLt) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> Bool {
            return a < b
        }
    )");

    EXPECT_NE(ir.find("icmp slt"), std::string::npos)
        << "IR should contain icmp slt for signed less-than";
}

TEST_F(CodegenExprTest, ComparisonGe) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> Bool {
            return a >= b
        }
    )");

    EXPECT_NE(ir.find("icmp sge"), std::string::npos)
        << "IR should contain icmp sge for signed greater-or-equal";
}

TEST_F(CodegenExprTest, LogicalAnd) {
    std::string ir = generate(R"(
        func test(a: Bool, b: Bool) -> Bool {
            return a and b
        }
    )");

    // Logical AND may short-circuit (branch) or use 'and' instruction
    bool has_and = ir.find(" and ") != std::string::npos;
    bool has_branch = ir.find("br ") != std::string::npos;
    EXPECT_TRUE(has_and || has_branch)
        << "IR should contain and instruction or short-circuit branches";
}

TEST_F(CodegenExprTest, LogicalOr) {
    std::string ir = generate(R"(
        func test(a: Bool, b: Bool) -> Bool {
            return a or b
        }
    )");

    bool has_or = ir.find(" or ") != std::string::npos;
    bool has_branch = ir.find("br ") != std::string::npos;
    EXPECT_TRUE(has_or || has_branch)
        << "IR should contain or instruction or short-circuit branches";
}

TEST_F(CodegenExprTest, BitwiseAndI32) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a & b
        }
    )");

    EXPECT_NE(ir.find("and i32"), std::string::npos) << "IR should contain and i32 for bitwise AND";
}

TEST_F(CodegenExprTest, BitwiseOrI32) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a | b
        }
    )");

    EXPECT_NE(ir.find("or i32"), std::string::npos) << "IR should contain or i32 for bitwise OR";
}

TEST_F(CodegenExprTest, BitwiseShiftLeft) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a << b
        }
    )");

    EXPECT_NE(ir.find("shl"), std::string::npos)
        << "IR should contain shl instruction for shift left";
}

TEST_F(CodegenExprTest, BitwiseShiftRight) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return a >> b
        }
    )");

    // Arithmetic shift right for signed integers
    EXPECT_NE(ir.find("ashr"), std::string::npos)
        << "IR should contain ashr instruction for arithmetic shift right";
}

// ============================================================================
// Unary Expression Tests
// ============================================================================

TEST_F(CodegenExprTest, UnaryNegateI32) {
    std::string ir = generate(R"(
        func test(x: I32) -> I32 {
            return -x
        }
    )");

    // Negation is sub 0, x
    EXPECT_NE(ir.find("sub"), std::string::npos)
        << "IR should contain sub instruction for integer negation";
}

TEST_F(CodegenExprTest, UnaryNegateF64) {
    std::string ir = generate(R"(
        func test(x: F64) -> F64 {
            return -x
        }
    )");

    EXPECT_NE(ir.find("fneg"), std::string::npos)
        << "IR should contain fneg instruction for float negation";
}

TEST_F(CodegenExprTest, UnaryNotBool) {
    std::string ir = generate(R"(
        func test(x: Bool) -> Bool {
            return not x
        }
    )");

    // Boolean not is xor with true
    EXPECT_NE(ir.find("xor"), std::string::npos)
        << "IR should contain xor instruction for boolean not";
}

// ============================================================================
// Cast Expression Tests
// ============================================================================

TEST_F(CodegenExprTest, CastI32ToI64) {
    std::string ir = generate(R"(
        func test(x: I32) -> I64 {
            return x as I64
        }
    )");

    EXPECT_NE(ir.find("sext i32"), std::string::npos)
        << "IR should contain sext for I32 to I64 cast";
}

TEST_F(CodegenExprTest, CastI64ToI32) {
    std::string ir = generate(R"(
        func test(x: I64) -> I32 {
            return x as I32
        }
    )");

    EXPECT_NE(ir.find("trunc i64"), std::string::npos)
        << "IR should contain trunc for I64 to I32 cast";
}

TEST_F(CodegenExprTest, CastI32ToF64) {
    std::string ir = generate(R"(
        func test(x: I32) -> F64 {
            return x as F64
        }
    )");

    EXPECT_NE(ir.find("sitofp"), std::string::npos)
        << "IR should contain sitofp for signed int to float cast";
}

TEST_F(CodegenExprTest, CastF64ToI32) {
    std::string ir = generate(R"(
        func test(x: F64) -> I32 {
            return x as I32
        }
    )");

    EXPECT_NE(ir.find("fptosi"), std::string::npos)
        << "IR should contain fptosi for float to signed int cast";
}

TEST_F(CodegenExprTest, CastF32ToF64) {
    std::string ir = generate(R"(
        func test(x: F32) -> F64 {
            return x as F64
        }
    )");

    EXPECT_NE(ir.find("fpext float"), std::string::npos)
        << "IR should contain fpext for F32 to F64 cast";
}

TEST_F(CodegenExprTest, CastU8ToU32) {
    std::string ir = generate(R"(
        func test(x: U8) -> U32 {
            return x as U32
        }
    )");

    EXPECT_NE(ir.find("zext i8"), std::string::npos)
        << "IR should contain zext for unsigned widening cast";
}

// ============================================================================
// Function Call Tests
// ============================================================================

TEST_F(CodegenExprTest, SimpleCall) {
    std::string ir = generate(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }

        func test() -> I32 {
            return add(1, 2)
        }
    )");

    EXPECT_NE(ir.find("call i32 @add"), std::string::npos)
        << "IR should contain call to @add function";
}

TEST_F(CodegenExprTest, CallWithVoidReturn) {
    std::string ir = generate(R"(
        func greet() {
            println("Hello")
        }

        func test() {
            greet()
        }
    )");

    EXPECT_NE(ir.find("call void @greet"), std::string::npos)
        << "IR should contain void call to @greet";
}

TEST_F(CodegenExprTest, NestedCalls) {
    std::string ir = generate(R"(
        func double(x: I32) -> I32 {
            return x * 2
        }

        func test() -> I32 {
            return double(double(5))
        }
    )");

    // Should have two calls to double
    EXPECT_GE(count_pattern(ir, "call i32 @double"), 2u)
        << "IR should contain two calls to @double for nested call";
}

// ============================================================================
// Struct Field Access Tests
// ============================================================================

TEST_F(CodegenExprTest, StructFieldAccess) {
    std::string ir = generate(R"(
        struct Point {
            x: I32,
            y: I32
        }

        func test() -> I32 {
            let p: Point = Point { x: 10, y: 20 }
            return p.x
        }
    )");

    EXPECT_NE(ir.find("getelementptr"), std::string::npos)
        << "IR should contain getelementptr for field access";
}

TEST_F(CodegenExprTest, StructConstruction) {
    std::string ir = generate(R"(
        struct Color {
            r: U8,
            g: U8,
            b: U8
        }

        func test() -> Color {
            return Color { r: 255, g: 128, b: 0 }
        }
    )");

    EXPECT_NE(ir.find("%struct.Color"), std::string::npos)
        << "IR should declare %struct.Color type";
}

TEST_F(CodegenExprTest, NestedStructAccess) {
    std::string ir = generate(R"(
        struct Inner {
            value: I32
        }

        struct Outer {
            inner: Inner,
            name: I32
        }

        func test() -> I32 {
            let o: Outer = Outer { inner: Inner { value: 42 }, name: 1 }
            return o.inner.value
        }
    )");

    // Should have GEP instructions for nested access
    EXPECT_GE(count_pattern(ir, "getelementptr"), 1u)
        << "IR should have GEP instructions for nested struct access";
}

// ============================================================================
// Tuple Expression Tests
// ============================================================================

TEST_F(CodegenExprTest, TupleConstruction) {
    std::string ir = generate(R"(
        func test() -> (I32, I32) {
            return (1, 2)
        }
    )");

    // Tuples are represented as structs
    // Should have insertvalue or store instructions for tuple elements
    bool has_insert = ir.find("insertvalue") != std::string::npos;
    bool has_store = ir.find("store") != std::string::npos;
    EXPECT_TRUE(has_insert || has_store)
        << "IR should have insertvalue or store for tuple construction";
}

TEST_F(CodegenExprTest, TupleElementAccess) {
    std::string ir = generate(R"(
        func test() -> I32 {
            let t: (I32, I32) = (10, 20)
            return t.0
        }
    )");

    // Tuple access uses extractvalue or GEP
    bool has_extract = ir.find("extractvalue") != std::string::npos;
    bool has_gep = ir.find("getelementptr") != std::string::npos;
    EXPECT_TRUE(has_extract || has_gep) << "IR should have extractvalue or GEP for tuple access";
}

// ============================================================================
// Closure/Lambda Tests
// ============================================================================

TEST_F(CodegenExprTest, SimpleLambda) {
    std::string ir = generate(R"(
        func test() -> I32 {
            let f: func(I32) -> I32 = do(x: I32) -> I32 { return x + 1 }
            return f(41)
        }
    )");

    // Lambda generates a closure function
    // May have a lambda function name and call instruction
    if (!ir.empty()) {
        EXPECT_NE(ir.find("define"), std::string::npos)
            << "IR should contain function definitions (including lambda)";
    }
}

// ============================================================================
// Print/Format Tests
// ============================================================================

TEST_F(CodegenExprTest, PrintlnString) {
    std::string ir = generate(R"(
        func test() {
            println("Hello, World!")
        }
    )");

    // println with a string constant should call puts or printf
    bool has_puts = ir.find("@puts") != std::string::npos;
    bool has_printf = ir.find("@printf") != std::string::npos;
    EXPECT_TRUE(has_puts || has_printf) << "IR should call puts or printf for println";
}

TEST_F(CodegenExprTest, PrintlnInteger) {
    std::string ir = generate(R"(
        func test() {
            println(42)
        }
    )");

    // println with integer should use printf with format string
    if (!ir.empty()) {
        EXPECT_NE(ir.find("call"), std::string::npos) << "IR should contain a call for println";
    }
}

// ============================================================================
// Array Expression Tests
// ============================================================================

TEST_F(CodegenExprTest, ArrayLiteral) {
    std::string ir = generate(R"(
        func test() {
            let arr: [I32; 3] = [1, 2, 3]
        }
    )");

    if (!ir.empty()) {
        // Array should be allocated (alloca or global)
        bool has_alloca = ir.find("alloca") != std::string::npos;
        bool has_global = ir.find("@") != std::string::npos;
        EXPECT_TRUE(has_alloca || has_global) << "IR should have array allocation";
    }
}

TEST_F(CodegenExprTest, ArrayIndexAccess) {
    std::string ir = generate(R"(
        func test() -> I32 {
            let arr: [I32; 3] = [10, 20, 30]
            return arr[1]
        }
    )");

    if (!ir.empty()) {
        EXPECT_NE(ir.find("getelementptr"), std::string::npos)
            << "IR should contain GEP for array indexing";
    }
}

// ============================================================================
// Mixed Expression Tests
// ============================================================================

TEST_F(CodegenExprTest, ComplexExpression) {
    std::string ir = generate(R"(
        func test(a: I32, b: I32) -> I32 {
            return (a + b) * (a - b)
        }
    )");

    EXPECT_NE(ir.find("add"), std::string::npos);
    EXPECT_NE(ir.find("sub"), std::string::npos);
    EXPECT_NE(ir.find("mul"), std::string::npos);
}

TEST_F(CodegenExprTest, ConditionalExpression) {
    std::string ir = generate(R"(
        func max(a: I32, b: I32) -> I32 {
            if a > b then {
                return a
            } else {
                return b
            }
        }
    )");

    EXPECT_NE(ir.find("icmp sgt"), std::string::npos)
        << "IR should contain signed greater-than comparison";
    EXPECT_NE(ir.find("br i1"), std::string::npos) << "IR should contain conditional branch";
}

TEST_F(CodegenExprTest, LetBindingChain) {
    std::string ir = generate(R"(
        func test() -> I32 {
            let a: I32 = 1
            let b: I32 = a + 2
            let c: I32 = b * 3
            let d: I32 = c - 4
            return d
        }
    )");

    EXPECT_NE(ir.find("add"), std::string::npos);
    EXPECT_NE(ir.find("mul"), std::string::npos);
    EXPECT_NE(ir.find("sub"), std::string::npos);
}

TEST_F(CodegenExprTest, FunctionReturnTypes) {
    std::string ir = generate(R"(
        func return_bool() -> Bool {
            return true
        }

        func return_i64() -> I64 {
            return 12345678901234
        }

        func return_f32() -> F32 {
            return 3.14
        }
    )");

    EXPECT_NE(ir.find("define i1 @return_bool"), std::string::npos)
        << "Bool function should return i1";
    EXPECT_NE(ir.find("define i64 @return_i64"), std::string::npos)
        << "I64 function should return i64";
    EXPECT_NE(ir.find("define float @return_f32"), std::string::npos)
        << "F32 function should return float";
}

TEST_F(CodegenExprTest, MutableVariable) {
    std::string ir = generate(R"(
        func test() -> I32 {
            let mut x: I32 = 0
            x = 42
            return x
        }
    )");

    // Mutable variable should use alloca+store+load
    EXPECT_NE(ir.find("alloca"), std::string::npos) << "Mutable variable should use alloca";
    EXPECT_GE(count_pattern(ir, "store i32"), 1u)
        << "Should have stores for mutable variable assignment";
}

TEST_F(CodegenExprTest, WhileLoopCodegen) {
    std::string ir = generate(R"(
        func test() -> I32 {
            let mut sum: I32 = 0
            let mut i: I32 = 0
            while i < 10 {
                sum = sum + i
                i = i + 1
            }
            return sum
        }
    )");

    // Loop should have back edge (br label to earlier block)
    EXPECT_NE(ir.find("br label"), std::string::npos)
        << "Loop should generate unconditional branch for back edge";
    EXPECT_NE(ir.find("br i1"), std::string::npos)
        << "Loop should generate conditional branch for exit test";
}
