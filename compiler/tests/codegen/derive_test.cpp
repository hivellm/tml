// Tests for derive codegen
// Covers: derive/partial_eq.cpp, derive/partial_ord.cpp, derive/debug.cpp,
//         derive/display.cpp, derive/duplicate.cpp, derive/hash.cpp,
//         derive/default.cpp, derive/serialize.cpp, derive/deserialize.cpp,
//         derive/fromstr.cpp, derive/reflect.cpp

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
};

// ============================================================================
// @derive(PartialEq)
// ============================================================================

TEST_F(DeriveCodegenTest, DerivePartialEqStruct) {
    std::string ir = generate(R"(
        @derive(PartialEq)
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> Bool {
            let a = Point { x: 1, y: 2 }
            let b = Point { x: 1, y: 2 }
            return a == b
        }
    )");
    EXPECT_FALSE(ir.empty()) << "PartialEq derive should generate valid comparison IR";
}

TEST_F(DeriveCodegenTest, DerivePartialEqEnum) {
    std::string ir = generate(R"(
        @derive(PartialEq)
        type Color {
            Red,
            Green,
            Blue,
        }

        func main() -> Bool {
            let a: Color = Red
            let b: Color = Red
            return a == b
        }
    )");
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// @derive(PartialOrd)
// ============================================================================

TEST_F(DeriveCodegenTest, DerivePartialOrdStruct) {
    std::string ir = generate(R"(
        @derive(PartialEq, PartialOrd)
        struct Score {
            value: I32,
        }

        func main() -> Bool {
            let a = Score { value: 10 }
            let b = Score { value: 20 }
            return a < b
        }
    )");
    EXPECT_FALSE(ir.empty()) << "PartialOrd derive should generate comparison IR";
}

// ============================================================================
// @derive(Debug)
// ============================================================================

TEST_F(DeriveCodegenTest, DeriveDebugStruct) {
    std::string ir = generate(R"(
        @derive(Debug)
        struct Point {
            x: I32,
            y: I32,
        }

        func main() {
            let p = Point { x: 1, y: 2 }
            print("{:?}\n", p)
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Debug derive should generate format IR";
}

// ============================================================================
// @derive(Duplicate)
// ============================================================================

TEST_F(DeriveCodegenTest, DeriveDuplicateStruct) {
    std::string ir = generate(R"(
        @derive(Duplicate)
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> I32 {
            let a = Point { x: 1, y: 2 }
            let b = a.duplicate()
            return b.x
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Duplicate derive should generate copy IR";
}

// ============================================================================
// @derive(Hash)
// ============================================================================

TEST_F(DeriveCodegenTest, DeriveHashStruct) {
    std::string ir = generate(R"(
        @derive(Hash)
        struct Key {
            id: I32,
            name: Str,
        }

        func main() {
            let k = Key { id: 1, name: "test" }
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Hash derive should generate valid IR";
}

// ============================================================================
// @derive(Default)
// ============================================================================

TEST_F(DeriveCodegenTest, DeriveDefaultStruct) {
    std::string ir = generate(R"(
        @derive(Default)
        struct Config {
            width: I32,
            height: I32,
            enabled: Bool,
        }

        func main() -> I32 {
            let c = Config.default()
            return c.width
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Default derive should generate zero-initialization IR";
}

// ============================================================================
// @derive(Display)
// ============================================================================

TEST_F(DeriveCodegenTest, DeriveDisplayStruct) {
    std::string ir = generate(R"(
        @derive(Display)
        struct Greeting {
            name: Str,
        }

        func main() {
            let g = Greeting { name: "world" }
            print("{}\n", g)
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Display derive should generate format IR";
}

// ============================================================================
// Multiple Derives
// ============================================================================

TEST_F(DeriveCodegenTest, MultipleDerives) {
    std::string ir = generate(R"(
        @derive(PartialEq, Debug, Duplicate)
        struct Item {
            id: I32,
        }

        func main() -> Bool {
            let a = Item { id: 1 }
            let b = a.duplicate()
            return a == b
        }
    )");
    EXPECT_FALSE(ir.empty()) << "Multiple derives should all generate valid IR";
}
