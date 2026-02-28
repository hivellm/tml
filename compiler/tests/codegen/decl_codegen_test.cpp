// Tests for declaration codegen
// Covers: decl/llvm_struct_decl.cpp, decl/enum.cpp, decl/func.cpp, decl/impl.cpp

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

class DeclCodegenTest : public ::testing::Test {
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
// Struct Type Declarations
// ============================================================================

TEST_F(DeclCodegenTest, SimpleStructDecl) {
    std::string ir = generate(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        func main() {
            let p = Point { x: 0, y: 0 }
        }
    )");
    EXPECT_NE(ir.find("%struct.Point = type"), std::string::npos)
        << "IR should declare struct type";
}

TEST_F(DeclCodegenTest, StructWithVariousTypes) {
    std::string ir = generate(R"(
        struct Record {
            id: I64,
            active: Bool,
            score: F64,
            count: I32,
        }

        func main() {
            let r = Record { id: 1, active: true, score: 3.14, count: 0 }
        }
    )");
    EXPECT_NE(ir.find("%struct.Record = type"), std::string::npos);
}

TEST_F(DeclCodegenTest, EmptyStruct) {
    std::string ir = generate(R"(
        struct Unit {}

        func main() {
            let u = Unit {}
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Enum Type Declarations
// ============================================================================

TEST_F(DeclCodegenTest, SimpleEnumDecl) {
    std::string ir = generate(R"(
        type Direction {
            North,
            South,
            East,
            West,
        }

        func main() {
            let d: Direction = North
        }
    )");
    EXPECT_NE(ir.find("%struct.Direction"), std::string::npos)
        << "IR should declare enum as struct type with tag";
}

TEST_F(DeclCodegenTest, EnumWithPayload) {
    std::string ir = generate(R"(
        type Shape {
            Circle(F64),
            Rectangle(F64, F64),
            Point,
        }

        func main() {
            let s: Shape = Circle(5.0)
        }
    )");
    EXPECT_NE(ir.find("%struct.Shape"), std::string::npos);
}

TEST_F(DeclCodegenTest, GenericEnum) {
    std::string ir = generate(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func main() {
            let x: Maybe[I32] = Just(42)
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Generic enum should generate valid monomorphized IR";
}

// ============================================================================
// Function Declarations
// ============================================================================

TEST_F(DeclCodegenTest, SimpleFuncDecl) {
    std::string ir = generate(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");
    EXPECT_NE(ir.find("define"), std::string::npos) << "IR should contain function definition";
    EXPECT_NE(ir.find("i32"), std::string::npos) << "IR should contain i32 parameter types";
}

TEST_F(DeclCodegenTest, VoidFuncDecl) {
    std::string ir = generate(R"(
        func noop() {
        }
    )");
    EXPECT_NE(ir.find("define"), std::string::npos);
    EXPECT_NE(ir.find("void"), std::string::npos);
}

TEST_F(DeclCodegenTest, FuncWithManyParams) {
    std::string ir = generate(R"(
        func multi(a: I32, b: I64, c: F64, d: Bool) -> I32 {
            return a
        }
    )");
    EXPECT_NE(ir.find("define"), std::string::npos);
}

TEST_F(DeclCodegenTest, GenericFuncDecl) {
    std::string ir = generate(R"(
        func first[T](a: T, b: T) -> T {
            return a
        }

        func main() -> I32 {
            return first[I32](1, 2)
        }
    )");
    EXPECT_NE(ir.find("define"), std::string::npos);
}

// ============================================================================
// Impl Block Declarations
// ============================================================================

TEST_F(DeclCodegenTest, ImplBlock) {
    std::string ir = generate(R"(
        struct Counter {
            val: I32,
        }

        impl Counter {
            func value(self) -> I32 {
                return self.val
            }

            func zero() -> Counter {
                return Counter { val: 0 }
            }
        }

        func main() -> I32 {
            let c = Counter.zero()
            return c.value()
        }
    )");
    EXPECT_NE(ir.find("define"), std::string::npos);
    // Both instance and static methods should be defined
    EXPECT_NE(ir.find("call"), std::string::npos);
}

TEST_F(DeclCodegenTest, MultipleImpls) {
    std::string ir = generate(R"(
        struct Num {
            v: I32,
        }

        impl Num {
            func get(self) -> I32 {
                return self.v
            }
        }

        func main() -> I32 {
            let n = Num { v: 42 }
            return n.get()
        }
    )");
    EXPECT_FALSE(ir.empty());
}
