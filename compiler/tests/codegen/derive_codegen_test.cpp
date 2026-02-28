// Tests for derive macro codegen:
// @derive(PartialEq), @derive(Debug), @derive(Duplicate), @derive(Hash),
// @derive(Default), @derive(Display), @derive(PartialOrd),
// @derive(Serialize), @derive(Deserialize), @derive(FromStr), @derive(Reflect)

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

class DeriveCodegenTest : public ::testing::Test {
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
// @derive(PartialEq)
// ============================================================================

TEST_F(DeriveCodegenTest, PartialEqStruct) {
    auto ir = generate(R"(
        @derive(PartialEq)
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> Bool {
            let a: Point = Point { x: 1, y: 2 }
            let b: Point = Point { x: 1, y: 2 }
            return a == b
        }
    )");
    EXPECT_FALSE(ir.empty()) << "PartialEq derive should generate valid IR";
}

TEST_F(DeriveCodegenTest, PartialEqEnum) {
    auto ir = generate(R"(
        @derive(PartialEq)
        type Color {
            Red,
            Green,
            Blue,
        }

        func main() -> Bool {
            return Red == Green
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// @derive(PartialOrd)
// ============================================================================

TEST_F(DeriveCodegenTest, PartialOrdStruct) {
    auto ir = generate(R"(
        @derive(PartialEq, PartialOrd)
        struct Score {
            value: I32,
        }

        func main() -> Bool {
            let a: Score = Score { value: 10 }
            let b: Score = Score { value: 20 }
            return a < b
        }
    )");
    EXPECT_FALSE(ir.empty()) << "PartialOrd derive should generate valid IR";
}

// ============================================================================
// @derive(Debug)
// ============================================================================

TEST_F(DeriveCodegenTest, DebugStruct) {
    auto ir = generate(R"(
        @derive(Debug)
        struct Point {
            x: I32,
            y: I32,
        }

        func main() {
            let p: Point = Point { x: 1, y: 2 }
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Debug derive should generate valid IR";
}

// ============================================================================
// @derive(Display)
// ============================================================================

TEST_F(DeriveCodegenTest, DisplayStruct) {
    auto ir = generate(R"(
        @derive(Display)
        struct Name {
            value: Str,
        }

        func main() {
            let n: Name = Name { value: "Alice" }
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Display derive should generate valid IR";
}

// ============================================================================
// @derive(Duplicate)
// ============================================================================

TEST_F(DeriveCodegenTest, DuplicateStruct) {
    auto ir = generate(R"(
        @derive(Duplicate)
        struct Point {
            x: I32,
            y: I32,
        }

        func main() {
            let p: Point = Point { x: 1, y: 2 }
            let q: Point = p.duplicate()
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Duplicate derive should generate valid IR";
}

// ============================================================================
// @derive(Hash)
// ============================================================================

TEST_F(DeriveCodegenTest, HashStruct) {
    auto ir = generate(R"(
        @derive(Hash)
        struct Id {
            value: I64,
        }

        func main() {
            let id: Id = Id { value: 42 }
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Hash derive should generate valid IR";
}

// ============================================================================
// @derive(Default)
// ============================================================================

TEST_F(DeriveCodegenTest, DefaultStruct) {
    auto ir = generate(R"(
        @derive(Default)
        struct Config {
            width: I32,
            height: I32,
        }

        func main() {
            let c: Config = Config::default()
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Default derive should generate valid IR";
}

// ============================================================================
// Declaration Codegen (struct, enum, func, impl)
// ============================================================================

TEST_F(DeriveCodegenTest, StructDeclaration) {
    auto ir = generate(R"(
        struct Rect {
            width: I32,
            height: I32,
        }

        func main() {
            let r: Rect = Rect { width: 100, height: 50 }
        }
    )");
    expect_ir_contains(ir, "%struct.Rect", "IR should declare Rect struct type");
}

TEST_F(DeriveCodegenTest, EnumDeclaration) {
    auto ir = generate(R"(
        type Shape {
            Circle(F64),
            Square(F64),
            Triangle(F64, F64),
        }

        func main() {
            let s: Shape = Circle(5.0)
        }
    )");
    expect_ir_contains(ir, "%struct.Shape", "IR should declare Shape enum type");
}

TEST_F(DeriveCodegenTest, FunctionDeclaration) {
    auto ir = generate(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");
    expect_ir_contains(ir, "define", "IR should contain function definition");
    expect_ir_contains(ir, "@add", "IR should define add function");
}

TEST_F(DeriveCodegenTest, ImplBlock) {
    auto ir = generate(R"(
        struct Counter {
            value: I32,
        }

        impl Counter {
            func new() -> Counter {
                return Counter { value: 0 }
            }

            func get(self: ref Counter) -> I32 {
                return self.value
            }
        }

        func main() -> I32 {
            let c: Counter = Counter::new()
            return c.get()
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Impl block methods should generate valid IR";
}
