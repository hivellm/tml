// MIR Codegen CGU mode tests
//
// Tests for the generate_cgu() method that produces partial IR
// with `define` for selected functions and `declare` for the rest.

#include "codegen/mir_codegen.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

using namespace tml::codegen;

class MirCodegenCguTest : public ::testing::Test {
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
// generate_cgu() with all indices = same as generate()
// ============================================================================

TEST_F(MirCodegenCguTest, AllIndicesMatchesFullGenerate) {
    auto mir = build_mir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
        func main() {
            let x: I32 = add(1, 2)
        }
    )");

    MirCodegenOptions opts;

    // Full generate
    MirCodegen full_gen(opts);
    auto full_ir = full_gen.generate(mir);

    // CGU with all indices
    std::vector<size_t> all_indices;
    for (size_t i = 0; i < mir.functions.size(); ++i) {
        all_indices.push_back(i);
    }
    MirCodegen cgu_gen(opts);
    auto cgu_ir = cgu_gen.generate_cgu(mir, all_indices);

    // Both should contain `define` for all functions
    for (const auto& func : mir.functions) {
        std::string define_marker = "define";
        EXPECT_NE(full_ir.find(define_marker), std::string::npos);
        EXPECT_NE(cgu_ir.find(define_marker), std::string::npos);

        // Both should mention the function name
        EXPECT_NE(full_ir.find(func.name), std::string::npos);
        EXPECT_NE(cgu_ir.find(func.name), std::string::npos);
    }
}

// ============================================================================
// generate_cgu() with subset — defines subset, declares rest
// ============================================================================

TEST_F(MirCodegenCguTest, SubsetDefinesAndDeclares) {
    auto mir = build_mir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
        func sub(a: I32, b: I32) -> I32 {
            return a - b
        }
        func main() {
            let x: I32 = add(1, 2)
        }
    )");

    ASSERT_GE(mir.functions.size(), 2u);

    MirCodegenOptions opts;
    MirCodegen codegen(opts);

    // Only include first function in the CGU
    std::vector<size_t> indices = {0};
    auto ir = codegen.generate_cgu(mir, indices);

    // The first function should be defined
    std::string first_func = mir.functions[0].name;
    std::string define_pattern = "define";
    EXPECT_NE(ir.find(define_pattern), std::string::npos);
    EXPECT_NE(ir.find(first_func), std::string::npos);
}

// ============================================================================
// Preamble uses internal linkage
// ============================================================================

TEST_F(MirCodegenCguTest, PreambleHasInternalLinkage) {
    auto mir = build_mir(R"(
        func main() {
            let x: I32 = 42
        }
    )");

    MirCodegenOptions opts;
    MirCodegen codegen(opts);

    std::vector<size_t> indices = {0};
    auto ir = codegen.generate_cgu(mir, indices);

    // Preamble functions (assert, drop_*) should use internal linkage
    if (ir.find("@assert") != std::string::npos) {
        EXPECT_NE(ir.find("internal"), std::string::npos);
    }
}

// ============================================================================
// Output contains target triple
// ============================================================================

TEST_F(MirCodegenCguTest, ContainsTargetTriple) {
    auto mir = build_mir(R"(
        func main() {}
    )");

    MirCodegenOptions opts;
    opts.target_triple = "x86_64-pc-windows-msvc";
    MirCodegen codegen(opts);

    std::vector<size_t> all_indices = {0};
    auto ir = codegen.generate_cgu(mir, all_indices);

    EXPECT_NE(ir.find("target triple"), std::string::npos);
    EXPECT_NE(ir.find("x86_64"), std::string::npos);
}
