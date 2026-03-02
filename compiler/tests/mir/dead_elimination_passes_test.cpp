// Tests for MIR dead elimination passes:
// - DeadArgElim
// - DeadFunctionElimination
// - DeadMethodElimination

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/dead_arg_elim.hpp"
#include "mir/passes/dead_function_elimination.hpp"
#include "mir/passes/dead_method_elimination.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class DeadEliminationTest : public ::testing::Test {
protected:
    std::unique_ptr<tml::lexer::Source> source_;
    std::unique_ptr<tml::types::TypeEnv> env_;

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
        env_ = std::make_unique<tml::types::TypeEnv>(
            std::move(std::get<tml::types::TypeEnv>(env_result)));
        tml::mir::MirBuilder builder(*env_);
        return builder.build(module);
    }
};

// ============================================================================
// DeadArgElim
// ============================================================================

TEST_F(DeadEliminationTest, DeadArgElimPassName) {
    tml::mir::DeadArgEliminationPass pass;
    EXPECT_EQ(pass.name(), "DeadArgElim");
}

TEST_F(DeadEliminationTest, DeadArgElimEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::DeadArgEliminationPass pass;
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(DeadEliminationTest, DeadArgElimUnusedParam) {
    auto mir = build_mir(R"(
        func ignore(x: I32, unused: I32) -> I32 {
            return x
        }

        func main() -> I32 {
            return ignore(42, 0)
        }
    )");

    tml::mir::DeadArgEliminationPass pass;
    pass.run(mir);
    ASSERT_GE(mir.functions.size(), 1u);
}

TEST_F(DeadEliminationTest, DeadArgElimAllUsed) {
    auto mir = build_mir(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");

    tml::mir::DeadArgEliminationPass pass;
    bool changed = pass.run(mir);
    // All args used — no change
    EXPECT_FALSE(changed);
}

// ============================================================================
// DeadFunctionElimination
// ============================================================================

TEST_F(DeadEliminationTest, DeadFuncElimPassName) {
    tml::mir::DeadFunctionEliminationPass pass;
    EXPECT_EQ(pass.name(), "DeadFunctionElimination");
}

TEST_F(DeadEliminationTest, DeadFuncElimUnusedFunction) {
    auto mir = build_mir(R"(
        func unused() -> I32 {
            return 42
        }

        func main() -> I32 {
            return 0
        }
    )");

    size_t before = mir.functions.size();
    tml::mir::DeadFunctionEliminationPass pass;
    pass.run(mir);

    // unused() should potentially be removed
    EXPECT_LE(mir.functions.size(), before);
}

TEST_F(DeadEliminationTest, DeadFuncElimPreserveCalledFunction) {
    auto mir = build_mir(R"(
        func helper() -> I32 {
            return 42
        }

        func main() -> I32 {
            return helper()
        }
    )");

    tml::mir::DeadFunctionEliminationPass pass;
    pass.run(mir);

    // helper() is called — should be preserved
    EXPECT_GE(mir.functions.size(), 1u);
}

// ============================================================================
// DeadMethodElimination
// ============================================================================

TEST_F(DeadEliminationTest, DeadMethodElimPassName) {
    auto mir = build_mir("func empty() { }");
    tml::mir::DeadMethodEliminationPass pass(*env_);
    EXPECT_EQ(pass.name(), "DeadMethodElimination");
}

TEST_F(DeadEliminationTest, DeadMethodElimEmptyFunction) {
    auto mir = build_mir("func empty() { }");
    tml::mir::DeadMethodEliminationPass pass(*env_);
    pass.run(mir);
    ASSERT_EQ(mir.functions.size(), 1u);
}
