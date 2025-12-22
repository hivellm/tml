#include "tml/codegen/llvm_ir_gen.hpp"
#include "tml/types/checker.hpp"
#include "tml/parser/parser.hpp"
#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include <gtest/gtest.h>
#include <memory>
#include <iostream>

using namespace tml;
using namespace tml::codegen;
using namespace tml::types;
using namespace tml::parser;
using namespace tml::lexer;

class CodegenTest : public ::testing::Test {
protected:
    std::unique_ptr<Source> source_;

    auto generate(const std::string& code) -> std::string {
        try {
            std::cout << "\n=== Parsing ===" << std::endl;
            source_ = std::make_unique<Source>(Source::from_string(code));
            Lexer lexer(*source_);
            auto tokens = lexer.tokenize();
            std::cout << "Tokens: " << tokens.size() << std::endl;

            Parser parser(std::move(tokens));
            auto module_result = parser.parse_module("test");
            EXPECT_TRUE(is_ok(module_result));
            auto& module = std::get<Module>(module_result);
            std::cout << "Module parsed" << std::endl;

            std::cout << "\n=== Type Checking ===" << std::endl;
            TypeChecker checker;
            auto env_result = checker.check_module(module);
            EXPECT_TRUE(is_ok(env_result));
            auto& env = std::get<TypeEnv>(env_result);
            std::cout << "Type checking OK" << std::endl;

            std::cout << "\n=== Codegen ===" << std::endl;
            LLVMIRGen gen(env);
            auto ir_result = gen.generate(module);

            if (is_err(ir_result)) {
                auto& errors = std::get<std::vector<LLVMGenError>>(ir_result);
                std::cout << "Codegen errors:" << std::endl;
                for (const auto& err : errors) {
                    std::cout << "  - " << err.message << std::endl;
                }
                EXPECT_TRUE(false) << "Codegen failed";
                return "";
            }

            auto& ir = std::get<std::string>(ir_result);
            std::cout << "\n=== Generated LLVM IR ===" << std::endl;
            std::cout << ir << std::endl;
            std::cout << "=== End LLVM IR ===" << std::endl;

            return ir;
        } catch (const std::exception& e) {
            std::cout << "\n=== EXCEPTION ===" << std::endl;
            std::cout << "Exception: " << e.what() << std::endl;
            throw;
        }
    }
};

// ============================================================================
// Enum Constructor Tests
// ============================================================================

TEST_F(CodegenTest, EnumConstructorSimple) {
    std::string ir = generate(R"(
        type Result {
            Ok(I64),
            Err(I32),
        }

        func main() {
            let x: Result = Ok(42)
        }
    )");

    // Check that IR contains enum struct type declaration
    EXPECT_NE(ir.find("%struct.Result = type"), std::string::npos)
        << "IR should declare %struct.Result type";

    // Check that tag is set
    EXPECT_NE(ir.find("store i32 0"), std::string::npos)
        << "IR should store tag value 0 for Ok variant";

    // Check that enum value is created
    EXPECT_NE(ir.find("alloca %struct.Result"), std::string::npos)
        << "IR should allocate Result enum";
}

TEST_F(CodegenTest, EnumConstructorUnitVariant) {
    std::string ir = generate(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func main() {
            let x: Maybe[I64] = Nothing
        }
    )");

    std::cout << "\n=== Testing unit variant Nothing ===" << std::endl;

    // Check that tag is set for Nothing (tag = 1)
    EXPECT_NE(ir.find("store i32 1"), std::string::npos)
        << "IR should store tag value 1 for Nothing variant";
}

TEST_F(CodegenTest, EnumConstructorWithPrintln) {
    std::string ir = generate(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func main() {
            let x: Maybe[I64] = Just(42)
            println("Created enum")
        }
    )");

    // Verify that both enum construction and println are present
    EXPECT_NE(ir.find("%struct.Maybe = type"), std::string::npos);
    EXPECT_NE(ir.find("@puts"), std::string::npos);
}

TEST_F(CodegenTest, SimpleHelloWorld) {
    std::string ir = generate(R"(
        func main() {
            println("Hello")
        }
    )");

    // This should work - baseline test
    EXPECT_NE(ir.find("@puts"), std::string::npos);
}

TEST_F(CodegenTest, EnumConstructorWithVariable) {
    std::string ir = generate(R"(
        type Result[T, E] {
            Ok(T),
            Err(E),
        }

        func main() {
            let value: I64 = 123
            let r: Result[I64, I32] = Ok(value)
        }
    )");

    // Check that value is loaded and used in enum constructor
    EXPECT_NE(ir.find("load i64"), std::string::npos);
    EXPECT_NE(ir.find("store i64"), std::string::npos);
}

// ============================================================================
// Pattern Matching Tests
// ============================================================================

TEST_F(CodegenTest, WhenExpressionSimple) {
    std::string ir = generate(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func main() {
            let x: Maybe[I64] = Just(42)

            when x {
                Just(v) => println("has value"),
                Nothing => println("no value"),
            }
        }
    )");

    // Check for tag extraction (getelementptr to field 0)
    EXPECT_NE(ir.find("getelementptr inbounds %struct.Maybe"), std::string::npos)
        << "IR should extract tag from enum";

    // Check for tag comparison
    EXPECT_NE(ir.find("icmp eq i32"), std::string::npos)
        << "IR should compare tag values";

    // Check for conditional branches
    EXPECT_NE(ir.find("br i1"), std::string::npos)
        << "IR should have conditional branches";
}

TEST_F(CodegenTest, WhenExpressionPayloadBinding) {
    std::string ir = generate(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func get_value(m: Maybe[I64]) -> I64 {
            when m {
                Just(v) => v,
                Nothing => 0,
            }
        }

        func main() {
            let x: Maybe[I64] = Just(42)
            let result: I64 = get_value(x)
        }
    )");

    // Check for payload extraction (getelementptr to field 1)
    EXPECT_NE(ir.find("getelementptr inbounds %struct.Maybe, ptr"), std::string::npos)
        << "IR should extract payload from enum";

    // Check that we return the extracted value
    EXPECT_NE(ir.find("ret i64"), std::string::npos)
        << "Function should return i64 value";
}
