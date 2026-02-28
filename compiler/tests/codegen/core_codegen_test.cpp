// Tests for codegen core infrastructure
// Covers: core/llvm_types.cpp, core/types_resolve.cpp, core/target.cpp,
//         core/generate.cpp, core/generate_support.cpp, core/generate_cache.cpp,
//         core/generic.cpp, core/llvm_utils.cpp, core/runtime.cpp,
//         core/runtime_modules.cpp, core/class_codegen.cpp,
//         core/class_codegen_generic.cpp, core/class_codegen_virtual.cpp,
//         core/dyn.cpp, core/drop.cpp, core/debug_info.cpp,
//         core/optimization_passes.cpp

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

class CoreCodegenTest : public ::testing::Test {
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
// Type Layout Tests
// ============================================================================

TEST_F(CoreCodegenTest, I8TypeMapping) {
    std::string ir = generate(R"(
        func main(x: I8) -> I8 {
            return x
        }
    )");
    EXPECT_NE(ir.find("i8"), std::string::npos);
}

TEST_F(CoreCodegenTest, I16TypeMapping) {
    std::string ir = generate(R"(
        func main(x: I16) -> I16 {
            return x
        }
    )");
    EXPECT_NE(ir.find("i16"), std::string::npos);
}

TEST_F(CoreCodegenTest, I32TypeMapping) {
    std::string ir = generate(R"(
        func main(x: I32) -> I32 {
            return x
        }
    )");
    EXPECT_NE(ir.find("i32"), std::string::npos);
}

TEST_F(CoreCodegenTest, I64TypeMapping) {
    std::string ir = generate(R"(
        func main(x: I64) -> I64 {
            return x
        }
    )");
    EXPECT_NE(ir.find("i64"), std::string::npos);
}

TEST_F(CoreCodegenTest, F32TypeMapping) {
    std::string ir = generate(R"(
        func main(x: F32) -> F32 {
            return x
        }
    )");
    EXPECT_NE(ir.find("float"), std::string::npos);
}

TEST_F(CoreCodegenTest, F64TypeMapping) {
    std::string ir = generate(R"(
        func main(x: F64) -> F64 {
            return x
        }
    )");
    EXPECT_NE(ir.find("double"), std::string::npos);
}

TEST_F(CoreCodegenTest, BoolTypeMapping) {
    std::string ir = generate(R"(
        func main(x: Bool) -> Bool {
            return x
        }
    )");
    EXPECT_NE(ir.find("i1"), std::string::npos);
}

TEST_F(CoreCodegenTest, StructTypeLayout) {
    std::string ir = generate(R"(
        struct Pair {
            first: I32,
            second: I64,
        }

        func main() {
            let p = Pair { first: 1, second: 2 }
        }
    )");
    EXPECT_NE(ir.find("%struct.Pair = type"), std::string::npos);
    EXPECT_NE(ir.find("i32"), std::string::npos);
    EXPECT_NE(ir.find("i64"), std::string::npos);
}

// ============================================================================
// Generic Instantiation
// ============================================================================

TEST_F(CoreCodegenTest, GenericMonomorphization) {
    std::string ir = generate(R"(
        func id[T](x: T) -> T {
            return x
        }

        func main() -> I32 {
            let a = id[I32](42)
            return a
        }
    )");
    // Generic should be monomorphized — no T in final IR
    EXPECT_EQ(ir.find("TypeVar"), std::string::npos)
        << "IR should not contain unresolved type variables";
}

TEST_F(CoreCodegenTest, MultipleGenericInstantiations) {
    std::string ir = generate(R"(
        func wrap[T](x: T) -> T {
            return x
        }

        func main() -> I32 {
            let a = wrap[I32](42)
            let b = wrap[I64](100)
            return a
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Multiple monomorphizations should all succeed";
}

// ============================================================================
// Runtime Declarations
// ============================================================================

TEST_F(CoreCodegenTest, RuntimePrintDeclaration) {
    std::string ir = generate(R"(
        func main() {
            print("hello\n")
        }
    )");
    // Print should reference runtime functions
    EXPECT_NE(ir.find("declare"), std::string::npos)
        << "IR should contain runtime function declarations";
}

// ============================================================================
// Class/OOP Codegen
// ============================================================================

TEST_F(CoreCodegenTest, ClassWithMethods) {
    std::string ir = generate(R"(
        struct Animal {
            legs: I32,
        }

        impl Animal {
            func leg_count(self) -> I32 {
                return self.legs
            }

            func new(legs: I32) -> Animal {
                return Animal { legs: legs }
            }
        }

        func main() -> I32 {
            let dog = Animal.new(4)
            return dog.leg_count()
        }
    )");
    EXPECT_FALSE(ir.empty());
    EXPECT_NE(ir.find("define"), std::string::npos);
}

// ============================================================================
// Dynamic Dispatch (Behavior Objects)
// ============================================================================

TEST_F(CoreCodegenTest, BehaviorDefinition) {
    std::string ir = generate(R"(
        behavior Printable {
            func to_string(self) -> Str
        }

        struct Num {
            val: I32,
        }

        impl Printable for Num {
            func to_string(self) -> Str {
                return "num"
            }
        }

        func main() {
            let n = Num { val: 42 }
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Behavior and impl should generate valid IR";
}

// ============================================================================
// Drop Glue
// ============================================================================

TEST_F(CoreCodegenTest, StructWithDrop) {
    std::string ir = generate(R"(
        struct Resource {
            handle: I32,
        }

        func main() {
            let r = Resource { handle: 1 }
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Module-Level Codegen
// ============================================================================

TEST_F(CoreCodegenTest, MultipleTopLevelFunctions) {
    std::string ir = generate(R"(
        func foo() -> I32 { return 1 }
        func bar() -> I32 { return 2 }
        func baz() -> I32 { return 3 }
    )");
    // Should have 3 function definitions
    size_t define_count = 0;
    size_t pos = 0;
    while ((pos = ir.find("define", pos)) != std::string::npos) {
        define_count++;
        pos++;
    }
    EXPECT_GE(define_count, 3u);
}

TEST_F(CoreCodegenTest, EmptyModule) {
    std::string ir = generate(R"(
        func main() { }
    )");
    EXPECT_FALSE(ir.empty());
    EXPECT_NE(ir.find("define"), std::string::npos);
}

// ============================================================================
// Constants and Globals
// ============================================================================

TEST_F(CoreCodegenTest, StringConstant) {
    std::string ir = generate(R"(
        func main() {
            let s: Str = "hello"
        }
    )");
    EXPECT_FALSE(ir.empty());
}

TEST_F(CoreCodegenTest, BoolConstants) {
    std::string ir = generate(R"(
        func main() -> Bool {
            let t: Bool = true
            let f: Bool = false
            return t
        }
    )");
    EXPECT_FALSE(ir.empty());
}
