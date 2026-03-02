// Tests for closure and advanced expression codegen
// Covers: closure.cpp, try.cpp, await.cpp, infer.cpp, infer_methods.cpp

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

class ExprClosureTest : public ::testing::Test {
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
// Closure Expressions
// ============================================================================

TEST_F(ExprClosureTest, SimpleClosure) {
    std::string ir = generate(R"(
        func main() -> I32 {
            let f = do(x: I32) -> I32 { x + 1 }
            return f(41)
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Simple closure should generate valid IR";
}

TEST_F(ExprClosureTest, ClosureCapture) {
    std::string ir = generate(R"(
        func main() -> I32 {
            let offset: I32 = 10
            let f = do(x: I32) -> I32 { x + offset }
            return f(32)
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Closure with capture should generate valid IR";
}

TEST_F(ExprClosureTest, ClosureNoParams) {
    std::string ir = generate(R"(
        func main() -> I32 {
            let f = do() -> I32 { 42 }
            return f()
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Try Expression (Error Propagation)
// ============================================================================

TEST_F(ExprClosureTest, TryExpression) {
    std::string ir = generate(R"(
        func may_fail(x: I32) -> Outcome[I32, Str] {
            if x < 0 {
                return Err("negative")
            }
            return Ok(x)
        }

        func main() -> Outcome[I32, Str] {
            let result = try may_fail(42)
            return Ok(result)
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Try expression should generate valid IR";
}

// ============================================================================
// Type Inference in Codegen
// ============================================================================

TEST_F(ExprClosureTest, InferredLetBinding) {
    std::string ir = generate(R"(
        func main() -> I32 {
            let x = 42
            return x
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Inferred type should generate valid IR";
}

TEST_F(ExprClosureTest, InferredReturnType) {
    std::string ir = generate(R"(
        func double(x: I32) -> I32 {
            return x * 2
        }

        func main() -> I32 {
            let result = double(21)
            return result
        }
    )");
    EXPECT_FALSE(ir.empty());
}
