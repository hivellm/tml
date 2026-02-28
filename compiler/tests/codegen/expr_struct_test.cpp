// Tests for struct, tuple, and collection expression codegen
// Covers: struct.cpp, struct_field.cpp, tuple.cpp, collections.cpp, llvm_struct_expr.cpp

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

class ExprStructTest : public ::testing::Test {
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
// Struct Construction
// ============================================================================

TEST_F(ExprStructTest, SimpleStructConstruction) {
    std::string ir = generate(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        func main() {
            let p = Point { x: 10, y: 20 }
        }
    )");
    EXPECT_NE(ir.find("%struct.Point"), std::string::npos) << "IR should declare Point struct type";
    EXPECT_NE(ir.find("alloca %struct.Point"), std::string::npos)
        << "IR should allocate Point struct";
}

TEST_F(ExprStructTest, StructFieldAccess) {
    std::string ir = generate(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> I32 {
            let p = Point { x: 10, y: 20 }
            return p.x
        }
    )");
    EXPECT_NE(ir.find("getelementptr"), std::string::npos) << "IR should use GEP for field access";
}

TEST_F(ExprStructTest, NestedStructAccess) {
    std::string ir = generate(R"(
        struct Inner {
            value: I32,
        }

        struct Outer {
            inner: Inner,
        }

        func main() -> I32 {
            let o = Outer { inner: Inner { value: 42 } }
            return o.inner.value
        }
    )");
    EXPECT_NE(ir.find("getelementptr"), std::string::npos)
        << "IR should use GEP for nested field access";
}

TEST_F(ExprStructTest, StructWithMultipleTypes) {
    std::string ir = generate(R"(
        struct Mixed {
            flag: Bool,
            count: I64,
            ratio: F64,
        }

        func main() {
            let m = Mixed { flag: true, count: 100, ratio: 3.14 }
        }
    )");
    EXPECT_NE(ir.find("%struct.Mixed"), std::string::npos);
}

// ============================================================================
// Tuple Operations
// ============================================================================

TEST_F(ExprStructTest, TupleConstruction) {
    std::string ir = generate(R"(
        func main() {
            let t: (I32, Bool) = (42, true)
        }
    )");
    EXPECT_FALSE(ir.empty()) << "IR generation should succeed for tuples";
}

TEST_F(ExprStructTest, TupleAccess) {
    std::string ir = generate(R"(
        func main() -> I32 {
            let t: (I32, Bool) = (42, true)
            return t.0
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Tuple element access should generate valid IR";
}

// ============================================================================
// Struct with Methods
// ============================================================================

TEST_F(ExprStructTest, StructWithImplMethod) {
    std::string ir = generate(R"(
        struct Counter {
            value: I32,
        }

        impl Counter {
            func get(self) -> I32 {
                return self.value
            }
        }

        func main() -> I32 {
            let c = Counter { value: 5 }
            return c.get()
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos) << "IR should contain call to method";
}

// ============================================================================
// Generic Structs
// ============================================================================

TEST_F(ExprStructTest, GenericStructConstruction) {
    std::string ir = generate(R"(
        struct Pair[T] {
            first: T,
            second: T,
        }

        func main() {
            let p = Pair[I32] { first: 1, second: 2 }
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Generic struct construction should generate valid IR";
}
