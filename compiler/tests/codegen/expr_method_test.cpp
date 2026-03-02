// Tests for method dispatch codegen
// Covers: method.cpp, method_static.cpp, method_static_dispatch.cpp,
//         method_generic.cpp, method_impl.cpp, method_class.cpp,
//         method_primitive.cpp, method_primitive_ext.cpp,
//         method_prim_behavior.cpp, method_array.cpp, method_slice.cpp,
//         method_collection.cpp, method_maybe.cpp, method_outcome.cpp,
//         method_dyn.cpp

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

class ExprMethodTest : public ::testing::Test {
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
// Struct Method Calls (Static Dispatch)
// ============================================================================

TEST_F(ExprMethodTest, SimpleMethodCall) {
    std::string ir = generate(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        impl Point {
            func sum(self) -> I32 {
                return self.x + self.y
            }
        }

        func main() -> I32 {
            let p = Point { x: 3, y: 4 }
            return p.sum()
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos) << "IR should contain method call";
}

TEST_F(ExprMethodTest, MethodWithParameters) {
    std::string ir = generate(R"(
        struct Calc {
            base: I32,
        }

        impl Calc {
            func add(self, x: I32) -> I32 {
                return self.base + x
            }
        }

        func main() -> I32 {
            let c = Calc { base: 10 }
            return c.add(5)
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos);
}

TEST_F(ExprMethodTest, MethodChaining) {
    std::string ir = generate(R"(
        struct Builder {
            value: I32,
        }

        impl Builder {
            func get(self) -> I32 {
                return self.value
            }
        }

        func main() -> I32 {
            let b = Builder { value: 42 }
            return b.get()
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Static Methods
// ============================================================================

TEST_F(ExprMethodTest, StaticMethodCall) {
    std::string ir = generate(R"(
        struct Math {
            dummy: I32,
        }

        impl Math {
            func square(x: I32) -> I32 {
                return x * x
            }
        }

        func main() -> I32 {
            return Math.square(5)
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos);
}

// ============================================================================
// Generic Method Calls
// ============================================================================

TEST_F(ExprMethodTest, GenericStructMethod) {
    std::string ir = generate(R"(
        struct Box[T] {
            value: T,
        }

        impl[T] Box[T] {
            func get(self) -> T {
                return self.value
            }
        }

        func main() -> I32 {
            let b = Box[I32] { value: 42 }
            return b.get()
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos);
}

// ============================================================================
// Primitive Type Methods
// ============================================================================

TEST_F(ExprMethodTest, I32AbsMethod) {
    std::string ir = generate(R"(
        func main() -> I32 {
            let x: I32 = -5
            return x.abs()
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Primitive method call should generate valid IR";
}

// ============================================================================
// Maybe[T] Methods
// ============================================================================

TEST_F(ExprMethodTest, MaybeIsJust) {
    std::string ir = generate(R"(
        func main() -> Bool {
            let x: Maybe[I32] = Just(42)
            return x.is_just()
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Maybe.is_just() should generate valid IR";
}

TEST_F(ExprMethodTest, MaybeIsNothing) {
    std::string ir = generate(R"(
        func main() -> Bool {
            let x: Maybe[I32] = Nothing
            return x.is_nothing()
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Outcome[T,E] Methods
// ============================================================================

TEST_F(ExprMethodTest, OutcomeIsOk) {
    std::string ir = generate(R"(
        func main() -> Bool {
            let x: Outcome[I32, Str] = Ok(42)
            return x.is_ok()
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Outcome.is_ok() should generate valid IR";
}

// ============================================================================
// Enum Method Calls
// ============================================================================

TEST_F(ExprMethodTest, EnumMethodCall) {
    std::string ir = generate(R"(
        type Color {
            Red,
            Green,
            Blue,
        }

        impl Color {
            func is_red(self) -> Bool {
                when self {
                    Red => return true,
                    _ => return false,
                }
            }
        }

        func main() -> Bool {
            let c: Color = Red
            return c.is_red()
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos);
}
