// Codegen Backend Abstraction tests
//
// Tests for the CodegenBackend behavior and LLVMCodegenBackend implementation.

#include "codegen/codegen_backend.hpp"
#include "codegen/llvm/llvm_codegen_backend.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace tml::codegen;

class CodegenBackendTest : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;

    auto build_mir(const std::string& code) -> tml::mir::Module {
        source_ = std::make_unique<tml::lexer::Source>(tml::lexer::Source::from_string(code));
        tml::lexer::Lexer lexer(*source_);
        auto tokens = lexer.tokenize();

        tml::parser::Parser parser(std::move(tokens));
        auto module_result = parser.parse_module("test");
        EXPECT_TRUE(tml::is_ok(module_result));
        auto& module = std::get<tml::parser::Module>(module_result);

        tml::types::TypeChecker checker;
        auto env_result = checker.check_module(module);
        EXPECT_TRUE(tml::is_ok(env_result));
        auto& env = std::get<tml::types::TypeEnv>(env_result);

        tml::mir::MirBuilder builder(env);
        return builder.build(module);
    }
};

// ============================================================================
// Factory
// ============================================================================

TEST_F(CodegenBackendTest, CreateLLVM) {
    auto backend = create_backend(BackendType::LLVM);
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->name(), "llvm");
}

TEST_F(CodegenBackendTest, CreateCraneliftThrows) {
    EXPECT_THROW(create_backend(BackendType::Cranelift), std::runtime_error);
}

TEST_F(CodegenBackendTest, DefaultBackendIsLLVM) {
    EXPECT_EQ(default_backend_type(), BackendType::LLVM);
}

// ============================================================================
// LLVMCodegenBackend capabilities
// ============================================================================

TEST_F(CodegenBackendTest, LLVMCapabilities) {
    LLVMCodegenBackend backend;
    auto caps = backend.capabilities();

    EXPECT_TRUE(caps.supports_mir);
    EXPECT_TRUE(caps.supports_ast);
    EXPECT_TRUE(caps.supports_generics);
    EXPECT_TRUE(caps.supports_debug_info);
    EXPECT_TRUE(caps.supports_coverage);
    EXPECT_TRUE(caps.supports_cgu);
    EXPECT_EQ(caps.max_optimization_level, 3);
}

// ============================================================================
// generate_ir (MIR → IR text, no compilation)
// ============================================================================

TEST_F(CodegenBackendTest, GenerateIrFromMir) {
    auto mir = build_mir(R"(
func compute() -> I32 {
    return 42
}
)");

    LLVMCodegenBackend backend;
    CodegenOptions opts;
    auto ir = backend.generate_ir(mir, opts);

    EXPECT_FALSE(ir.empty());
    EXPECT_NE(ir.find("define"), std::string::npos);
    // MIR codegen should produce ret with 42
    EXPECT_NE(ir.find("42"), std::string::npos);
}

// ============================================================================
// compile_mir (MIR → object file)
// ============================================================================

TEST_F(CodegenBackendTest, CompileMir) {
    auto mir = build_mir(R"(
func identity(x: I32) -> I32 {
    return x
}
)");

    LLVMCodegenBackend backend;
    CodegenOptions opts;
    auto result = backend.compile_mir(mir, opts);

    EXPECT_TRUE(result.success) << result.error_message;
    EXPECT_FALSE(result.llvm_ir.empty());
    EXPECT_TRUE(std::filesystem::exists(result.object_file));
}

// ============================================================================
// compile_mir_cgu (subset of functions → object file)
// ============================================================================

TEST_F(CodegenBackendTest, CompileMirCgu) {
    auto mir = build_mir(R"(
func add(a: I32, b: I32) -> I32 {
    return a + b
}

func main() -> I32 {
    return add(1, 2)
}
)");

    ASSERT_GE(mir.functions.size(), 2u);

    LLVMCodegenBackend backend;
    CodegenOptions opts;

    // Compile only the first function
    auto result = backend.compile_mir_cgu(mir, {0}, opts);
    EXPECT_TRUE(result.success) << result.error_message;
    EXPECT_FALSE(result.llvm_ir.empty());
    EXPECT_TRUE(std::filesystem::exists(result.object_file));
}
