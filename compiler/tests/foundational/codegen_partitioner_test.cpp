// Codegen Partitioner tests
//
// Tests for CGU (Codegen Unit) partitioning of MIR modules.

#include "codegen/codegen_partitioner.hpp"
#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <set>

using namespace tml::codegen;

class CodegenPartitionerTest : public ::testing::Test {
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
// assign_cgu() — deterministic assignment
// ============================================================================

TEST_F(CodegenPartitionerTest, AssignCguDeterministic) {
    int cgu1 = CodegenPartitioner::assign_cgu("my_function", 4);
    int cgu2 = CodegenPartitioner::assign_cgu("my_function", 4);
    EXPECT_EQ(cgu1, cgu2);
}

TEST_F(CodegenPartitionerTest, AssignCguInRange) {
    for (int n = 1; n <= 8; ++n) {
        int cgu = CodegenPartitioner::assign_cgu("test_func", n);
        EXPECT_GE(cgu, 0);
        EXPECT_LT(cgu, n);
    }
}

TEST_F(CodegenPartitionerTest, AssignCguDistributes) {
    // With enough different function names, multiple CGUs should be used
    std::set<int> seen;
    for (int i = 0; i < 100; ++i) {
        int cgu = CodegenPartitioner::assign_cgu("func_" + std::to_string(i), 4);
        seen.insert(cgu);
    }
    // With 100 different names and 4 CGUs, we should hit all 4
    EXPECT_EQ(seen.size(), 4u);
}

// ============================================================================
// partition() — single function (monolithic path)
// ============================================================================

TEST_F(CodegenPartitionerTest, SingleFunctionMonolithic) {
    auto mir = build_mir(R"(
        func main() {
            let x: I32 = 42
        }
    )");

    PartitionOptions opts;
    opts.num_cgus = 4;
    CodegenPartitioner partitioner(opts);

    auto result = partitioner.partition(mir);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.cgus.size(), 1u);
    EXPECT_EQ(result.cgus[0].cgu_index, 0);
    EXPECT_EQ(result.cgus[0].function_names.size(), 1u);
    EXPECT_EQ(result.cgus[0].function_names[0], "main");
}

// ============================================================================
// partition() — multiple functions
// ============================================================================

TEST_F(CodegenPartitionerTest, MultipleFunctionsPartition) {
    auto mir = build_mir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
        func sub(a: I32, b: I32) -> I32 {
            return a - b
        }
        func mul(a: I32, b: I32) -> I32 {
            return a * b
        }
        func main() {
            let x: I32 = add(1, 2)
        }
    )");

    PartitionOptions opts;
    opts.num_cgus = 16;
    CodegenPartitioner partitioner(opts);

    auto result = partitioner.partition(mir);
    EXPECT_TRUE(result.success);
    // With 4 functions, CGUs are capped at min(16, 4) = up to 4
    EXPECT_GE(result.cgus.size(), 1u);
    EXPECT_LE(result.cgus.size(), 4u);
}

// ============================================================================
// partition() — caps at function count
// ============================================================================

TEST_F(CodegenPartitionerTest, CapsAtFunctionCount) {
    auto mir = build_mir(R"(
        func foo() {}
        func bar() {}
    )");

    PartitionOptions opts;
    opts.num_cgus = 100; // Way more than functions
    CodegenPartitioner partitioner(opts);

    auto result = partitioner.partition(mir);
    EXPECT_TRUE(result.success);
    // Only 2 functions, so at most 2 CGUs
    EXPECT_LE(result.cgus.size(), 2u);
}

// ============================================================================
// partition() — valid IR in each CGU
// ============================================================================

TEST_F(CodegenPartitionerTest, CguContainsValidIr) {
    auto mir = build_mir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
        func main() {
            let x: I32 = add(1, 2)
        }
    )");

    PartitionOptions opts;
    opts.num_cgus = 4;
    CodegenPartitioner partitioner(opts);

    auto result = partitioner.partition(mir);
    EXPECT_TRUE(result.success);

    for (const auto& cgu : result.cgus) {
        EXPECT_FALSE(cgu.llvm_ir.empty());
        // Every CGU should have at least one define or declare
        bool has_define = cgu.llvm_ir.find("define") != std::string::npos;
        bool has_declare = cgu.llvm_ir.find("declare") != std::string::npos;
        EXPECT_TRUE(has_define || has_declare);
    }
}

// ============================================================================
// partition() — fingerprints
// ============================================================================

TEST_F(CodegenPartitionerTest, CguFingerprintsNonEmpty) {
    auto mir = build_mir(R"(
        func foo() {}
        func bar() {}
        func main() {}
    )");

    PartitionOptions opts;
    opts.num_cgus = 4;
    CodegenPartitioner partitioner(opts);

    auto result = partitioner.partition(mir);
    EXPECT_TRUE(result.success);

    for (const auto& cgu : result.cgus) {
        EXPECT_FALSE(cgu.fingerprint.empty());
    }
}

// ============================================================================
// partition() — all functions appear exactly once
// ============================================================================

TEST_F(CodegenPartitionerTest, AllFunctionsAppearOnce) {
    auto mir = build_mir(R"(
        func alpha() {}
        func beta() {}
        func gamma() {}
        func main() {}
    )");

    PartitionOptions opts;
    opts.num_cgus = 4;
    CodegenPartitioner partitioner(opts);

    auto result = partitioner.partition(mir);
    EXPECT_TRUE(result.success);

    std::vector<std::string> all_names;
    for (const auto& cgu : result.cgus) {
        for (const auto& name : cgu.function_names) {
            all_names.push_back(name);
        }
    }

    // Sort for comparison
    std::sort(all_names.begin(), all_names.end());
    auto it = std::unique(all_names.begin(), all_names.end());

    // No duplicates
    EXPECT_EQ(it, all_names.end());

    // All functions present
    EXPECT_EQ(all_names.size(), mir.functions.size());
}

// ============================================================================
// partition() — empty module
// ============================================================================

TEST_F(CodegenPartitionerTest, EmptyModuleSucceeds) {
    tml::mir::Module mir;
    mir.name = "empty";

    PartitionOptions opts;
    opts.num_cgus = 4;
    CodegenPartitioner partitioner(opts);

    auto result = partitioner.partition(mir);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.cgus.empty());
}
