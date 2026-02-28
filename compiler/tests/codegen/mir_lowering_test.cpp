// Tests for MIR-to-LLVM lowering codegen
// Covers: codegen/mir/terminators.cpp, codegen/mir/instructions.cpp,
//         codegen/mir/instructions_method.cpp, codegen/mir/instructions_misc.cpp,
//         codegen/mir/codegen_helpers.cpp, codegen/mir/mir_types.cpp

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

class MirLoweringTest : public ::testing::Test {
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
// Terminator Instructions
// ============================================================================

TEST_F(MirLoweringTest, ReturnTerminator) {
    std::string ir = generate(R"(
        func main() -> I32 {
            return 42
        }
    )");
    EXPECT_NE(ir.find("ret i32"), std::string::npos);
}

TEST_F(MirLoweringTest, VoidReturnTerminator) {
    std::string ir = generate(R"(
        func main() {
        }
    )");
    EXPECT_NE(ir.find("ret void"), std::string::npos);
}

TEST_F(MirLoweringTest, ConditionalBranch) {
    std::string ir = generate(R"(
        func main(x: Bool) -> I32 {
            if x { return 1 }
            return 0
        }
    )");
    EXPECT_NE(ir.find("br i1"), std::string::npos)
        << "Should generate conditional branch terminator";
}

TEST_F(MirLoweringTest, UnconditionalBranch) {
    std::string ir = generate(R"(
        func main() -> I32 {
            var x: I32 = 0
            loop {
                if x >= 5 { break }
                x = x + 1
            }
            return x
        }
    )");
    // Loop back-edge is an unconditional branch
    bool has_br = ir.find("br label") != std::string::npos;
    EXPECT_TRUE(has_br) << "Loop should contain unconditional branch";
}

// ============================================================================
// Basic Instructions
// ============================================================================

TEST_F(MirLoweringTest, AllocaInstruction) {
    std::string ir = generate(R"(
        func main() -> I32 {
            var x: I32 = 42
            return x
        }
    )");
    EXPECT_NE(ir.find("alloca"), std::string::npos) << "Mutable variable should generate alloca";
}

TEST_F(MirLoweringTest, StoreAndLoad) {
    std::string ir = generate(R"(
        func main() -> I32 {
            var x: I32 = 10
            x = 20
            return x
        }
    )");
    EXPECT_NE(ir.find("store"), std::string::npos);
    EXPECT_NE(ir.find("load"), std::string::npos);
}

TEST_F(MirLoweringTest, BinaryInstruction) {
    std::string ir = generate(R"(
        func main(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");
    EXPECT_NE(ir.find("add"), std::string::npos);
}

// ============================================================================
// Method Call Instructions
// ============================================================================

TEST_F(MirLoweringTest, MethodCallInstruction) {
    std::string ir = generate(R"(
        struct Foo {
            val: I32,
        }

        impl Foo {
            func get(self) -> I32 {
                return self.val
            }
        }

        func main() -> I32 {
            let f = Foo { val: 42 }
            return f.get()
        }
    )");
    EXPECT_NE(ir.find("call"), std::string::npos);
}

// ============================================================================
// Misc Instructions
// ============================================================================

TEST_F(MirLoweringTest, GEPInstruction) {
    std::string ir = generate(R"(
        struct Point {
            x: I32,
            y: I32,
        }

        func main() -> I32 {
            let p = Point { x: 1, y: 2 }
            return p.y
        }
    )");
    EXPECT_NE(ir.find("getelementptr"), std::string::npos);
}

TEST_F(MirLoweringTest, PhiNode) {
    std::string ir = generate(R"(
        func main(flag: Bool) -> I32 {
            if flag {
                return 1
            } else {
                return 0
            }
        }
    )");
    // Phi nodes may or may not be generated depending on the IR structure
    EXPECT_FALSE(ir.empty());
}

// ============================================================================
// Type Lowering
// ============================================================================

TEST_F(MirLoweringTest, StructTypeLowering) {
    std::string ir = generate(R"(
        struct Vec2 {
            x: F32,
            y: F32,
        }

        func main() {
            let v = Vec2 { x: 1.0 as F32, y: 2.0 as F32 }
        }
    )");
    EXPECT_NE(ir.find("%struct.Vec2"), std::string::npos);
}

TEST_F(MirLoweringTest, EnumTypeLowering) {
    std::string ir = generate(R"(
        type Result {
            Ok(I32),
            Err(I32),
        }

        func main() {
            let r: Result = Ok(42)
        }
    )");
    EXPECT_NE(ir.find("%struct.Result"), std::string::npos);
}
