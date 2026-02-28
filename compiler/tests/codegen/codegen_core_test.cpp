// Tests for codegen core infrastructure:
// - Type mapping (types.cpp)
// - Generic instantiation (generic.cpp)
// - Drop glue (drop.cpp)
// - Dynamic dispatch (dyn.cpp)
// - Runtime declarations (runtime.cpp)
// - Debug info (debug_info.cpp)
// - Optimization passes (optimization_passes.cpp)

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

class CodegenCoreTest : public ::testing::Test {
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
// Type Mapping (types.cpp / types_resolve.cpp)
// ============================================================================

TEST_F(CodegenCoreTest, I32TypeMapping) {
    auto ir = generate(R"(
        func main() -> I32 {
            return 42
        }
    )");
    expect_ir_contains(ir, "i32", "IR should use i32 for I32 type");
}

TEST_F(CodegenCoreTest, I64TypeMapping) {
    auto ir = generate(R"(
        func main() -> I64 {
            return 42
        }
    )");
    expect_ir_contains(ir, "i64", "IR should use i64 for I64 type");
}

TEST_F(CodegenCoreTest, F64TypeMapping) {
    auto ir = generate(R"(
        func main() -> F64 {
            return 3.14
        }
    )");
    expect_ir_contains(ir, "double", "IR should use double for F64 type");
}

TEST_F(CodegenCoreTest, BoolTypeMapping) {
    auto ir = generate(R"(
        func main() -> Bool {
            return true
        }
    )");
    expect_ir_contains(ir, "i1", "IR should use i1 for Bool type");
}

TEST_F(CodegenCoreTest, StrTypeMapping) {
    auto ir = generate(R"(
        func main() -> Str {
            return "hello"
        }
    )");
    expect_ir_contains(ir, "ptr", "IR should use ptr for Str type");
}

TEST_F(CodegenCoreTest, StructTypeLayout) {
    auto ir = generate(R"(
        struct Pair {
            first: I32,
            second: I64,
        }

        func main() {
            let p: Pair = Pair { first: 1, second: 2 }
        }
    )");
    expect_ir_contains(ir, "%struct.Pair = type", "IR should define struct type");
}

// ============================================================================
// Generic Instantiation (generic.cpp)
// ============================================================================

TEST_F(CodegenCoreTest, GenericStruct) {
    auto ir = generate(R"(
        struct Box[T] {
            value: T,
        }

        func main() -> I32 {
            let b: Box[I32] = Box { value: 42 }
            return b.value
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Generic struct instantiation should work";
}

TEST_F(CodegenCoreTest, GenericFunction) {
    auto ir = generate(R"(
        func identity[T](x: T) -> T {
            return x
        }

        func main() -> I32 {
            return identity(42)
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Generic function instantiation should work";
}

TEST_F(CodegenCoreTest, GenericEnum) {
    auto ir = generate(R"(
        type Maybe[T] {
            Just(T),
            Nothing,
        }

        func main() {
            let x: Maybe[I32] = Just(42)
            let y: Maybe[I32] = Nothing
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Generic enum instantiation should work";
}

// ============================================================================
// Runtime Declarations (runtime.cpp)
// ============================================================================

TEST_F(CodegenCoreTest, RuntimePrintDecl) {
    auto ir = generate(R"(
        func main() {
            print("hello\n")
        }
    )");
    // Runtime should declare print function
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Target Configuration (target.cpp)
// ============================================================================

TEST_F(CodegenCoreTest, TargetTriple) {
    auto ir = generate(R"(
        func main() -> I32 {
            return 0
        }
    )");
    // IR should contain target triple
    expect_ir_contains(ir, "target", "IR should contain target specification");
}

// ============================================================================
// Multiple Modules
// ============================================================================

TEST_F(CodegenCoreTest, MultipleFunctions) {
    auto ir = generate(R"(
        func foo() -> I32 {
            return 1
        }

        func bar() -> I32 {
            return 2
        }

        func main() -> I32 {
            return foo() + bar()
        }
    )");
    expect_ir_contains(ir, "@foo", "IR should define foo");
    expect_ir_contains(ir, "@bar", "IR should define bar");
    expect_ir_contains(ir, "@main", "IR should define main");
}

TEST_F(CodegenCoreTest, StructWithMethods) {
    auto ir = generate(R"(
        struct Vec2 {
            x: F64,
            y: F64,
        }

        impl Vec2 {
            func new(x: F64, y: F64) -> Vec2 {
                return Vec2 { x: x, y: y }
            }

            func length_squared(self: ref Vec2) -> F64 {
                return self.x * self.x + self.y * self.y
            }
        }

        func main() -> F64 {
            let v: Vec2 = Vec2::new(3.0, 4.0)
            return v.length_squared()
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Struct with methods should generate valid IR";
}
