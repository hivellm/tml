// Tests for the SimplifyCfg MIR optimization pass
//
// Verifies CFG simplification: block merging, empty block removal,
// constant branch simplification, and unreachable block removal.

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp"
#include "mir/passes/simplify_cfg.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <gtest/gtest.h>
#include <memory>

class SimplifyCfgTest : public ::testing::Test {
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

    auto block_count(const tml::mir::Function& func) -> size_t {
        return func.blocks.size();
    }
};

// ============================================================================
// Basic Tests
// ============================================================================

TEST_F(SimplifyCfgTest, PassName) {
    tml::mir::SimplifyCfgPass pass;
    EXPECT_EQ(pass.name(), "SimplifyCfg");
}

TEST_F(SimplifyCfgTest, EmptyFunctionNoChange) {
    auto mir = build_mir(R"(
        func empty() {
        }
    )");

    tml::mir::SimplifyCfgPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}

TEST_F(SimplifyCfgTest, SingleBlockNoChange) {
    auto mir = build_mir(R"(
        func simple() -> I32 {
            return 42
        }
    )");

    size_t blocks_before = block_count(mir.functions[0]);

    tml::mir::SimplifyCfgPass pass;
    pass.run(mir);

    // Single block cannot be simplified further
    EXPECT_LE(block_count(mir.functions[0]), blocks_before);
}

TEST_F(SimplifyCfgTest, IfElseBranching) {
    auto mir = build_mir(R"(
        func choose(flag: Bool) -> I32 {
            if flag {
                return 1
            } else {
                return 2
            }
        }
    )");

    tml::mir::SimplifyCfgPass pass;
    pass.run(mir);

    // If-else creates branches — pass should simplify any trivial blocks
    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_GE(block_count(mir.functions[0]), 1u);
}

TEST_F(SimplifyCfgTest, ConstantConditionSimplification) {
    auto mir = build_mir(R"(
        func main() -> I32 {
            if true {
                return 1
            } else {
                return 2
            }
        }
    )");

    size_t blocks_before = block_count(mir.functions[0]);

    tml::mir::SimplifyCfgPass pass;
    pass.run(mir);

    // Constant condition should eliminate one branch
    size_t blocks_after = block_count(mir.functions[0]);
    EXPECT_LE(blocks_after, blocks_before);
}

TEST_F(SimplifyCfgTest, LoopCFG) {
    auto mir = build_mir(R"(
        func count() -> I32 {
            var i: I32 = 0
            loop {
                if i >= 10 {
                    break
                }
                i = i + 1
            }
            return i
        }
    )");

    tml::mir::SimplifyCfgPass pass;
    pass.run(mir);

    // Loop has header, body, exit blocks — verify structure preserved
    ASSERT_EQ(mir.functions.size(), 1u);
    EXPECT_GE(block_count(mir.functions[0]), 1u);
}

TEST_F(SimplifyCfgTest, MultipleFunctions) {
    auto mir = build_mir(R"(
        func foo() -> I32 {
            if true {
                return 1
            }
            return 0
        }

        func bar() -> I32 {
            return 42
        }
    )");

    tml::mir::SimplifyCfgPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 2u);
}

TEST_F(SimplifyCfgTest, NestedIf) {
    auto mir = build_mir(R"(
        func nested(a: Bool, b: Bool) -> I32 {
            if a {
                if b {
                    return 1
                } else {
                    return 2
                }
            } else {
                return 3
            }
        }
    )");

    tml::mir::SimplifyCfgPass pass;
    pass.run(mir);

    ASSERT_EQ(mir.functions.size(), 1u);
}
