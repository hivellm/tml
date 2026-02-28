// Tests for binary and unary expression codegen
// Covers: binary.cpp, binary_ops.cpp, unary.cpp, cast.cpp

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

class ExprBinaryTest : public ::testing::Test {
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
};

// ============================================================================
// Integer Arithmetic
// ============================================================================

TEST_F(ExprBinaryTest, IntegerAddition) {
    std::string ir = generate(R"(
        func main() -> I32 {
            return 1 + 2
        }
    )");
    EXPECT_NE(ir.find("add"), std::string::npos) << "IR should contain add instruction";
}

TEST_F(ExprBinaryTest, IntegerSubtraction) {
    std::string ir = generate(R"(
        func main() -> I32 {
            return 10 - 3
        }
    )");
    EXPECT_NE(ir.find("sub"), std::string::npos) << "IR should contain sub instruction";
}

TEST_F(ExprBinaryTest, IntegerMultiplication) {
    std::string ir = generate(R"(
        func main() -> I32 {
            return 4 * 5
        }
    )");
    EXPECT_NE(ir.find("mul"), std::string::npos) << "IR should contain mul instruction";
}

TEST_F(ExprBinaryTest, IntegerDivision) {
    std::string ir = generate(R"(
        func main() -> I32 {
            return 20 / 4
        }
    )");
    EXPECT_NE(ir.find("div"), std::string::npos) << "IR should contain div instruction";
}

TEST_F(ExprBinaryTest, IntegerModulo) {
    std::string ir = generate(R"(
        func main() -> I32 {
            return 17 % 5
        }
    )");
    // srem for signed, urem for unsigned
    bool has_rem = ir.find("rem") != std::string::npos;
    EXPECT_TRUE(has_rem) << "IR should contain rem instruction";
}

// ============================================================================
// Comparison Operations
// ============================================================================

TEST_F(ExprBinaryTest, IntegerEqual) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return 1 == 1
        }
    )");
    EXPECT_NE(ir.find("icmp"), std::string::npos) << "IR should contain icmp instruction";
}

TEST_F(ExprBinaryTest, IntegerNotEqual) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return 1 != 2
        }
    )");
    EXPECT_NE(ir.find("icmp"), std::string::npos);
}

TEST_F(ExprBinaryTest, IntegerLessThan) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return 1 < 2
        }
    )");
    EXPECT_NE(ir.find("icmp"), std::string::npos);
}

TEST_F(ExprBinaryTest, IntegerGreaterThan) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return 2 > 1
        }
    )");
    EXPECT_NE(ir.find("icmp"), std::string::npos);
}

TEST_F(ExprBinaryTest, IntegerLessEqual) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return 1 <= 2
        }
    )");
    EXPECT_NE(ir.find("icmp"), std::string::npos);
}

TEST_F(ExprBinaryTest, IntegerGreaterEqual) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return 2 >= 1
        }
    )");
    EXPECT_NE(ir.find("icmp"), std::string::npos);
}

// ============================================================================
// Logical Operations
// ============================================================================

TEST_F(ExprBinaryTest, LogicalAnd) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return true and false
        }
    )");
    // Short-circuit AND generates branches or 'and' instruction
    EXPECT_FALSE(ir.empty());
}

TEST_F(ExprBinaryTest, LogicalOr) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return true or false
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Unary Operations
// ============================================================================

TEST_F(ExprBinaryTest, UnaryNegation) {
    std::string ir = generate(R"(
        func negate(x: I32) -> I32 {
            return -x
        }
    )");
    // Negation is typically sub 0, x or mul -1, x
    bool has_sub = ir.find("sub") != std::string::npos;
    bool has_neg = ir.find("mul") != std::string::npos || has_sub;
    EXPECT_TRUE(has_neg) << "IR should contain negation";
}

TEST_F(ExprBinaryTest, LogicalNot) {
    std::string ir = generate(R"(
        func invert(x: Bool) -> Bool {
            return not x
        }
    )");
    // Logical not is xor with true or icmp eq false
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Bitwise Operations
// ============================================================================

TEST_F(ExprBinaryTest, BitwiseAnd) {
    std::string ir = generate(R"(
        func main(a: I32, b: I32) -> I32 {
            return a & b
        }
    )");
    EXPECT_NE(ir.find("and"), std::string::npos) << "IR should contain bitwise and";
}

TEST_F(ExprBinaryTest, BitwiseOr) {
    std::string ir = generate(R"(
        func main(a: I32, b: I32) -> I32 {
            return a | b
        }
    )");
    EXPECT_NE(ir.find("or"), std::string::npos) << "IR should contain bitwise or";
}

TEST_F(ExprBinaryTest, BitwiseXor) {
    std::string ir = generate(R"(
        func main(a: I32, b: I32) -> I32 {
            return a ^ b
        }
    )");
    EXPECT_NE(ir.find("xor"), std::string::npos) << "IR should contain bitwise xor";
}

TEST_F(ExprBinaryTest, LeftShift) {
    std::string ir = generate(R"(
        func main(a: I32, b: I32) -> I32 {
            return a << b
        }
    )");
    EXPECT_NE(ir.find("shl"), std::string::npos) << "IR should contain shl instruction";
}

TEST_F(ExprBinaryTest, RightShift) {
    std::string ir = generate(R"(
        func main(a: I32, b: I32) -> I32 {
            return a >> b
        }
    )");
    // ashr for signed, lshr for unsigned
    bool has_shift = ir.find("shr") != std::string::npos;
    EXPECT_TRUE(has_shift) << "IR should contain shr instruction";
}

// ============================================================================
// Float Operations
// ============================================================================

TEST_F(ExprBinaryTest, FloatAddition) {
    std::string ir = generate(R"(
        func main() -> F64 {
            return 1.0 + 2.0
        }
    )");
    EXPECT_NE(ir.find("fadd"), std::string::npos) << "IR should contain fadd instruction";
}

TEST_F(ExprBinaryTest, FloatMultiplication) {
    std::string ir = generate(R"(
        func main() -> F64 {
            return 3.0 * 4.0
        }
    )");
    EXPECT_NE(ir.find("fmul"), std::string::npos) << "IR should contain fmul instruction";
}

TEST_F(ExprBinaryTest, FloatComparison) {
    std::string ir = generate(R"(
        func main() -> Bool {
            return 1.0 < 2.0
        }
    )");
    EXPECT_NE(ir.find("fcmp"), std::string::npos) << "IR should contain fcmp instruction";
}

// ============================================================================
// Cast Operations
// ============================================================================

TEST_F(ExprBinaryTest, CastI32ToI64) {
    std::string ir = generate(R"(
        func main() -> I64 {
            let x: I32 = 42
            return x as I64
        }
    )");
    EXPECT_NE(ir.find("sext"), std::string::npos)
        << "IR should contain sign extension for I32 -> I64";
}

TEST_F(ExprBinaryTest, CastI64ToI32) {
    std::string ir = generate(R"(
        func main() -> I32 {
            let x: I64 = 42
            return x as I32
        }
    )");
    EXPECT_NE(ir.find("trunc"), std::string::npos) << "IR should contain truncation for I64 -> I32";
}

TEST_F(ExprBinaryTest, CastI32ToF64) {
    std::string ir = generate(R"(
        func main() -> F64 {
            let x: I32 = 42
            return x as F64
        }
    )");
    EXPECT_NE(ir.find("sitofp"), std::string::npos) << "IR should contain sitofp for I32 -> F64";
}

TEST_F(ExprBinaryTest, CastF64ToI32) {
    std::string ir = generate(R"(
        func main() -> I32 {
            let x: F64 = 42.0
            return x as I32
        }
    )");
    EXPECT_NE(ir.find("fptosi"), std::string::npos) << "IR should contain fptosi for F64 -> I32";
}

// ============================================================================
// Mixed Expression Tests
// ============================================================================

TEST_F(ExprBinaryTest, ChainedArithmetic) {
    std::string ir = generate(R"(
        func main(a: I32, b: I32, c: I32) -> I32 {
            return a + b * c
        }
    )");
    EXPECT_NE(ir.find("mul"), std::string::npos);
    EXPECT_NE(ir.find("add"), std::string::npos);
}

TEST_F(ExprBinaryTest, ComparisonWithArithmetic) {
    std::string ir = generate(R"(
        func main(x: I32) -> Bool {
            return x + 1 > 10
        }
    )");
    EXPECT_NE(ir.find("add"), std::string::npos);
    EXPECT_NE(ir.find("icmp"), std::string::npos);
}
