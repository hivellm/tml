// Tests for control flow codegen
// Covers: control/if.cpp, control/loop.cpp, control/when.cpp, control/return.cpp

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

class ControlFlowCodegenTest : public ::testing::Test {
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
// If/Else Codegen
// ============================================================================

TEST_F(ControlFlowCodegenTest, SimpleIf) {
    std::string ir = generate(R"(
        func main(flag: Bool) -> I32 {
            if flag {
                return 1
            }
            return 0
        }
    )");
    EXPECT_NE(ir.find("br"), std::string::npos) << "IR should contain branch instruction for if";
}

TEST_F(ControlFlowCodegenTest, IfElse) {
    std::string ir = generate(R"(
        func main(flag: Bool) -> I32 {
            if flag {
                return 1
            } else {
                return 0
            }
        }
    )");
    // Should have conditional branch
    EXPECT_NE(ir.find("br i1"), std::string::npos)
        << "IR should contain conditional branch for if/else";
}

TEST_F(ControlFlowCodegenTest, NestedIf) {
    std::string ir = generate(R"(
        func classify(x: I32) -> I32 {
            if x > 0 {
                if x > 100 {
                    return 2
                }
                return 1
            }
            return 0
        }
    )");
    EXPECT_FALSE(ir.empty());
    // Multiple branches expected
    size_t br_count = 0;
    size_t pos = 0;
    while ((pos = ir.find("br i1", pos)) != std::string::npos) {
        br_count++;
        pos++;
    }
    EXPECT_GE(br_count, 2u) << "Nested if should generate multiple conditional branches";
}

TEST_F(ControlFlowCodegenTest, IfWithExpression) {
    std::string ir = generate(R"(
        func max(a: I32, b: I32) -> I32 {
            if a > b {
                return a
            } else {
                return b
            }
        }
    )");
    EXPECT_NE(ir.find("icmp"), std::string::npos);
    EXPECT_NE(ir.find("br"), std::string::npos);
}

// ============================================================================
// Loop Codegen
// ============================================================================

TEST_F(ControlFlowCodegenTest, InfiniteLoopWithBreak) {
    std::string ir = generate(R"(
        func main() -> I32 {
            var i: I32 = 0
            loop {
                if i >= 10 {
                    break
                }
                i = i + 1
            }
            return i
        }
    )");
    EXPECT_NE(ir.find("br"), std::string::npos) << "Loop should generate back-edge branch";
}

TEST_F(ControlFlowCodegenTest, LoopWithContinue) {
    std::string ir = generate(R"(
        func main() -> I32 {
            var sum: I32 = 0
            var i: I32 = 0
            loop {
                if i >= 10 { break }
                i = i + 1
                if i % 2 == 0 { continue }
                sum = sum + i
            }
            return sum
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Loop with continue should generate valid IR";
}

TEST_F(ControlFlowCodegenTest, NestedLoops) {
    std::string ir = generate(R"(
        func main() -> I32 {
            var total: I32 = 0
            var i: I32 = 0
            loop {
                if i >= 3 { break }
                var j: I32 = 0
                loop {
                    if j >= 3 { break }
                    total = total + 1
                    j = j + 1
                }
                i = i + 1
            }
            return total
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// When (Match) Codegen
// ============================================================================

TEST_F(ControlFlowCodegenTest, WhenOnEnum) {
    std::string ir = generate(R"(
        type Color {
            Red,
            Green,
            Blue,
        }

        func to_int(c: Color) -> I32 {
            when c {
                Red => return 0,
                Green => return 1,
                Blue => return 2,
            }
        }
    )");
    EXPECT_FALSE(ir.empty()) << "When expression on enum should generate valid IR";
}

TEST_F(ControlFlowCodegenTest, WhenOnInteger) {
    std::string ir = generate(R"(
        func classify(x: I32) -> I32 {
            when x {
                0 => return 0,
                1 => return 1,
                _ => return 2,
            }
        }
    )");
    EXPECT_FALSE(ir.empty());
}

TEST_F(ControlFlowCodegenTest, WhenWithPayload) {
    std::string ir = generate(R"(
        type Shape {
            Circle(F64),
            Square(F64),
        }

        func area(s: Shape) -> F64 {
            when s {
                Circle(r) => return 3.14 * r * r,
                Square(side) => return side * side,
            }
        }
    )");
    EXPECT_FALSE(ir.empty()) << "When with payload extraction should generate valid IR";
}

// ============================================================================
// Return Codegen
// ============================================================================

TEST_F(ControlFlowCodegenTest, SimpleReturn) {
    std::string ir = generate(R"(
        func main() -> I32 {
            return 42
        }
    )");
    EXPECT_NE(ir.find("ret i32"), std::string::npos) << "IR should contain ret instruction";
}

TEST_F(ControlFlowCodegenTest, VoidReturn) {
    std::string ir = generate(R"(
        func main() {
            return
        }
    )");
    EXPECT_NE(ir.find("ret void"), std::string::npos) << "IR should contain void ret";
}

TEST_F(ControlFlowCodegenTest, MultipleReturns) {
    std::string ir = generate(R"(
        func abs(x: I32) -> I32 {
            if x >= 0 {
                return x
            }
            return 0 - x
        }
    )");
    // Count ret instructions
    size_t ret_count = 0;
    size_t pos = 0;
    while ((pos = ir.find("ret i32", pos)) != std::string::npos) {
        ret_count++;
        pos++;
    }
    EXPECT_GE(ret_count, 1u) << "Should have at least one return";
}

TEST_F(ControlFlowCodegenTest, EarlyReturn) {
    std::string ir = generate(R"(
        func check(x: I32) -> I32 {
            if x < 0 { return -1 }
            if x == 0 { return 0 }
            return 1
        }
    )");
    EXPECT_FALSE(ir.empty());
}
