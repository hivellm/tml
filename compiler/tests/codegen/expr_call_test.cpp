// Tests for function call expression codegen
// Covers: call.cpp, call_user.cpp, call_generic_struct.cpp, print.cpp

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

class ExprCallTest : public ::testing::Test {
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
// Direct Function Calls
// ============================================================================

TEST_F(ExprCallTest, SimpleCall) {
    std::string ir = generate(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }

        func main() -> I32 {
            return add(1, 2)
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos) << "IR should contain call instruction";
    EXPECT_NE(ir.find("@test_add"), std::string::npos) << "IR should reference the add function";
}

TEST_F(ExprCallTest, VoidFunctionCall) {
    std::string ir = generate(R"(
        func do_nothing() {
        }

        func main() {
            do_nothing()
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos) << "IR should call do_nothing";
}

TEST_F(ExprCallTest, NestedCalls) {
    std::string ir = generate(R"(
        func square(x: I32) -> I32 {
            return x * x
        }

        func add(a: I32, b: I32) -> I32 {
            return a + b
        }

        func main() -> I32 {
            return add(square(3), square(4))
        }
    )");
    EXPECT_NE(ir.find("@test_square"), std::string::npos);
    EXPECT_NE(ir.find("@test_add"), std::string::npos);
}

TEST_F(ExprCallTest, MultipleParameters) {
    std::string ir = generate(R"(
        func sum3(a: I32, b: I32, c: I32) -> I32 {
            return a + b + c
        }

        func main() -> I32 {
            return sum3(1, 2, 3)
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos);
}

// ============================================================================
// Print Expression
// ============================================================================

TEST_F(ExprCallTest, PrintStringLiteral) {
    std::string ir = generate(R"(
        func main() {
            print("hello\n")
        }
    )");
    // Print should generate a call to tml_print_str or similar runtime function
    EXPECT_NE(ir.find("call"), std::string::npos) << "IR should contain call for print";
}

TEST_F(ExprCallTest, PrintInteger) {
    std::string ir = generate(R"(
        func main() {
            print("{}\n", 42)
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos);
}

// ============================================================================
// Generic Function Calls
// ============================================================================

TEST_F(ExprCallTest, GenericFunctionCall) {
    std::string ir = generate(R"(
        func identity[T](x: T) -> T {
            return x
        }

        func main() -> I32 {
            return identity[I32](42)
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos)
        << "IR should contain call to monomorphized generic";
}

// ============================================================================
// Recursive Calls
// ============================================================================

TEST_F(ExprCallTest, RecursiveCall) {
    std::string ir = generate(R"(
        func factorial(n: I32) -> I32 {
            if n <= 1 {
                return 1
            }
            return n * factorial(n - 1)
        }
    )");
    EXPECT_NE(ir.find("@test_factorial"), std::string::npos)
        << "IR should contain self-referencing call";
}
