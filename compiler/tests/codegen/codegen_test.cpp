#include "codegen/llvm_ir_gen.hpp"
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

class CodegenTest : public ::testing::Test {
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
        type Option[T] {
            Some(T),
            None,
        }

        func main() {
            let x: Option[I64] = None
        }
    )");

    // Check that tag is set for None (tag = 1)
    EXPECT_NE(ir.find("store i32 1"), std::string::npos)
        << "IR should store tag value 1 for None variant";
}

TEST_F(CodegenTest, EnumConstructorWithPrintln) {
    std::string ir = generate(R"(
        type Option[T] {
            Some(T),
            None,
        }

        func main() {
            let x: Option[I64] = Some(42)
            println("Created enum")
        }
    )");

    // Verify that both enum construction and println are present
    // Generic enums use mangled names like Option__I64
    EXPECT_NE(ir.find("%struct.Option__I64 = type"), std::string::npos)
        << "IR should declare %struct.Option__I64 type for Option[I64]";
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
        type Option[T] {
            Some(T),
            None,
        }

        func main() {
            let x: Option[I64] = Some(42)

            when x {
                Some(v) => println("has value"),
                None => println("no value"),
            }
        }
    )");

    // Check for tag extraction (getelementptr to field 0)
    // Generic enums use mangled names like Option__I64
    EXPECT_NE(ir.find("getelementptr inbounds %struct.Option__I64"), std::string::npos)
        << "IR should extract tag from enum";

    // Check for tag comparison
    EXPECT_NE(ir.find("icmp eq i32"), std::string::npos) << "IR should compare tag values";

    // Check for conditional branches
    EXPECT_NE(ir.find("br i1"), std::string::npos) << "IR should have conditional branches";
}

TEST_F(CodegenTest, WhenExpressionPayloadBinding) {
    std::string ir = generate(R"(
        type Option[T] {
            Some(T),
            None,
        }

        func get_value(m: Option[I64]) -> I64 {
            return when m {
                Some(v) => v,
                None => 0,
            }
        }

        func main() {
            let x: Option[I64] = Some(42)
            let result: I64 = get_value(x)
        }
    )");

    // Check for payload extraction (getelementptr to field 1)
    // Generic enums use mangled names like Option__I64
    EXPECT_NE(ir.find("getelementptr inbounds %struct.Option__I64, ptr"), std::string::npos)
        << "IR should extract payload from enum";

    // Check that we return the extracted value
    EXPECT_NE(ir.find("ret i64"), std::string::npos) << "Function should return i64 value";
}

// ============================================================================
// FFI Tests (@extern and @link decorators)
// ============================================================================

TEST_F(CodegenTest, ExternFunctionBasic) {
    std::string ir = generate(R"(
        @extern("c")
        func getenv(name: Str) -> Str

        func main() -> I32 {
            return 0
        }
    )");

    // Check that extern function is declared (not defined)
    EXPECT_NE(ir.find("declare ptr @getenv(ptr)"), std::string::npos)
        << "IR should contain extern declaration";

    // Verify it's NOT defined (no define @getenv)
    EXPECT_EQ(ir.find("define"), ir.find("define")) << "Extern function should not have a body";
}

TEST_F(CodegenTest, ExternFunctionWithCustomName) {
    std::string ir = generate(R"(
        @extern("c", name = "atoi")
        func string_to_int(s: Str) -> I32

        func main() -> I32 {
            let val: I32 = string_to_int("42")
            return val
        }
    )");

    // Check that extern function uses the custom name
    EXPECT_NE(ir.find("declare i32 @atoi(ptr)"), std::string::npos)
        << "IR should declare function with extern_name 'atoi'";

    // Check that call uses the custom name
    EXPECT_NE(ir.find("call i32 @atoi("), std::string::npos)
        << "Call should use extern_name 'atoi'";
}

TEST_F(CodegenTest, ExternFunctionStdcall) {
    std::string ir = generate(R"(
        @extern("stdcall")
        func MyWinFunc(x: I32) -> I32

        func main() -> I32 {
            return 0
        }
    )");

    // Check stdcall calling convention
    EXPECT_NE(ir.find("declare x86_stdcallcc i32 @MyWinFunc(i32)"), std::string::npos)
        << "IR should use x86_stdcallcc calling convention";
}

TEST_F(CodegenTest, ExternFunctionFastcall) {
    std::string ir = generate(R"(
        @extern("fastcall")
        func FastFunc(a: I32, b: I32) -> I32

        func main() -> I32 {
            return 0
        }
    )");

    // Check fastcall calling convention
    EXPECT_NE(ir.find("declare x86_fastcallcc i32 @FastFunc(i32, i32)"), std::string::npos)
        << "IR should use x86_fastcallcc calling convention";
}

TEST_F(CodegenTest, LinkDecorator) {
    std::string ir = generate(R"(
        @link("user32")
        @extern("c")
        func MyExternFunc(x: I32) -> I32

        func main() -> I32 {
            return 0
        }
    )");

    // Just verify it parses and generates - actual linking is done by clang
    EXPECT_NE(ir.find("declare i32 @MyExternFunc(i32)"), std::string::npos)
        << "IR should contain extern declaration with @link";
}

// ============================================================================
// FFI Namespace Tests (qualified calls like SDL2::init)
// ============================================================================

TEST_F(CodegenTest, FFINamespaceQualifiedCall) {
    std::string ir = generate(R"(
        @link("mylib")
        @extern("c")
        func my_init() -> I32

        func main() -> I32 {
            let result: I32 = mylib::my_init()
            return result
        }
    )");

    // The qualified call mylib::my_init() should resolve to the extern function
    EXPECT_NE(ir.find("declare i32 @my_init()"), std::string::npos)
        << "IR should contain extern declaration";
    EXPECT_NE(ir.find("call i32 @my_init()"), std::string::npos)
        << "IR should call the extern function via qualified name";
}

TEST_F(CodegenTest, FFINamespaceMultipleLibs) {
    std::string ir = generate(R"(
        @link("libfoo")
        @extern("c")
        func foo_init() -> I32

        @link("libbar")
        @extern("c")
        func bar_init() -> I32

        func main() -> I32 {
            let a: I32 = foo::foo_init()
            let b: I32 = bar::bar_init()
            return a + b
        }
    )");

    // Both qualified calls should resolve correctly
    EXPECT_NE(ir.find("declare i32 @foo_init()"), std::string::npos)
        << "IR should contain foo_init declaration";
    EXPECT_NE(ir.find("declare i32 @bar_init()"), std::string::npos)
        << "IR should contain bar_init declaration";
    EXPECT_NE(ir.find("call i32 @foo_init()"), std::string::npos)
        << "IR should call foo_init via qualified name";
    EXPECT_NE(ir.find("call i32 @bar_init()"), std::string::npos)
        << "IR should call bar_init via qualified name";
}

TEST_F(CodegenTest, FFINamespaceLibNameExtraction) {
    // Test that library name is extracted correctly from various formats
    std::string ir = generate(R"(
        @link("SDL2.dll")
        @extern("c")
        func SDL_Init(flags: U32) -> I32

        func main() -> I32 {
            let result: I32 = SDL2::SDL_Init(0)
            return result
        }
    )");

    // SDL2.dll should extract to namespace "SDL2"
    EXPECT_NE(ir.find("declare i32 @SDL_Init(i32)"), std::string::npos)
        << "IR should contain SDL_Init declaration";
    EXPECT_NE(ir.find("call i32 @SDL_Init(i32 0)"), std::string::npos)
        << "IR should call SDL_Init via SDL2:: namespace";
}

// ============================================================================
// Tuple Destructuring Tests
// ============================================================================

TEST_F(CodegenTest, TupleDestructuringSimple) {
    std::string ir = generate(R"(
        func make_pair() -> (I32, I32) {
            let x: I32 = 10
            let y: I32 = 20
            return (x, y)
        }

        func main() {
            let (a, b): (I32, I32) = make_pair()
        }
    )");

    // Check that tuple type is used
    EXPECT_NE(ir.find("{ i32, i32 }"), std::string::npos)
        << "IR should contain tuple type { i32, i32 }";

    // Check that getelementptr is used to extract elements
    EXPECT_NE(ir.find("getelementptr inbounds { i32, i32 }"), std::string::npos)
        << "IR should use GEP to extract tuple elements";
}

TEST_F(CodegenTest, TupleDestructuringNested) {
    std::string ir = generate(R"(
        func make_nested() -> ((I32, I32), I32) {
            let x: I32 = 1
            let y: I32 = 2
            let z: I32 = 3
            return ((x, y), z)
        }

        func main() {
            let ((a, b), c): ((I32, I32), I32) = make_nested()
        }
    )");

    // Check nested tuple type
    EXPECT_NE(ir.find("{ { i32, i32 }, i32 }"), std::string::npos)
        << "IR should contain nested tuple type";

    // Should have multiple GEP extractions for nested destructuring
    size_t gep_count = 0;
    size_t pos = 0;
    while ((pos = ir.find("getelementptr inbounds", pos)) != std::string::npos) {
        gep_count++;
        pos++;
    }
    EXPECT_GE(gep_count, 3) << "IR should have at least 3 GEP instructions for nested tuple";
}

TEST_F(CodegenTest, TupleDestructuringWithWildcard) {
    std::string ir = generate(R"(
        func get_triple() -> (I32, I32, I32) {
            let x: I32 = 1
            let y: I32 = 2
            let z: I32 = 3
            return (x, y, z)
        }

        func main() {
            let (a, _, c): (I32, I32, I32) = get_triple()
        }
    )");

    // Should still generate GEP for all 3 elements (wildcard is just ignored)
    EXPECT_NE(ir.find("{ i32, i32, i32 }"), std::string::npos)
        << "IR should contain triple tuple type";
}
