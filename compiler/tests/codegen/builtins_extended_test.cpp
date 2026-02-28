// Tests for extended builtin codegen:
// - Intrinsics, collections, memory, math, IO, async, atomic, sync, time, assert

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

class BuiltinsExtendedTest : public ::testing::Test {
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
// Memory Builtins (mem.cpp)
// ============================================================================

TEST_F(BuiltinsExtendedTest, MemAlloc) {
    auto ir = generate(R"(
        func main() {
            let p: Ptr = mem_alloc(64)
            mem_free(p)
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Math Builtins (math.cpp)
// ============================================================================

TEST_F(BuiltinsExtendedTest, MathSqrt) {
    auto ir = generate(R"(
        func main() -> F64 {
            return float_sqrt(25.0)
        }
    )");
    EXPECT_FALSE(ir.empty());
}

TEST_F(BuiltinsExtendedTest, MathAbs) {
    auto ir = generate(R"(
        func main() -> F64 {
            return float_abs(-3.14)
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// String Builtins (string.cpp)
// ============================================================================

TEST_F(BuiltinsExtendedTest, StringLen) {
    auto ir = generate(R"(
        func main() -> I64 {
            return str_len("hello")
        }
    )");
    EXPECT_FALSE(ir.empty());
}

TEST_F(BuiltinsExtendedTest, StringEq) {
    auto ir = generate(R"(
        func main() -> Bool {
            return str_eq("hello", "hello")
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// IO Builtins (io.cpp)
// ============================================================================

TEST_F(BuiltinsExtendedTest, PrintBuiltin) {
    auto ir = generate(R"(
        func main() {
            print("test output\n")
        }
    )");
    expect_ir_contains(ir, "call", "IR should contain print call");
}

// ============================================================================
// Assert Builtins (assert.cpp)
// ============================================================================

TEST_F(BuiltinsExtendedTest, AssertEq) {
    auto ir = generate(R"(
        func main() {
            assert_eq(1, 1)
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Multiple builtins in one function
// ============================================================================

TEST_F(BuiltinsExtendedTest, MixedBuiltins) {
    auto ir = generate(R"(
        func main() -> I32 {
            print("start\n")
            let x: I32 = 42
            return x
        }
    )");
    EXPECT_FALSE(ir.empty());
}
