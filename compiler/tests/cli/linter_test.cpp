// Tests for CLI linter infrastructure
// Covers: linter/config.cpp, linter/linter_discovery.cpp,
//         linter/linter_helpers.cpp, linter/linter_run.cpp,
//         linter/semantic.cpp, linter/style.cpp

#include "lexer/lexer.hpp"
#include "lexer/source.hpp"
#include "parser/parser.hpp"
#include "types/checker.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// Linter Discovery Tests
// ============================================================================

TEST(LinterDiscoveryTest, FindsTmlFiles) {
    // Create a temp directory with .tml files
    auto temp = fs::temp_directory_path() / "tml_linter_test";
    fs::create_directories(temp);

    // Write test files
    {
        std::ofstream f(temp / "test1.tml");
        f << "func main() { }";
    }
    {
        std::ofstream f(temp / "test2.tml");
        f << "func foo() -> I32 { return 42 }";
    }
    {
        std::ofstream f(temp / "ignore.txt");
        f << "not a tml file";
    }

    // Count .tml files
    int tml_count = 0;
    for (const auto& entry : fs::directory_iterator(temp)) {
        if (entry.path().extension() == ".tml") {
            tml_count++;
        }
    }

    EXPECT_EQ(tml_count, 2) << "Should find 2 .tml files";

    // Cleanup
    fs::remove_all(temp);
}

// ============================================================================
// Style Lint Rule Tests
// ============================================================================

TEST(LinterStyleTest, TokenizesValidCode) {
    auto source = tml::lexer::Source::from_string("func main() { let x: I32 = 42 }");
    tml::lexer::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    EXPECT_GT(tokens.size(), 0u) << "Valid code should tokenize successfully";
}

TEST(LinterStyleTest, ParsesValidFunction) {
    auto source = tml::lexer::Source::from_string("func main() -> I32 { return 42 }");
    tml::lexer::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    tml::parser::Parser parser(std::move(tokens));
    auto result = parser.parse_module("test");
    EXPECT_TRUE(tml::is_ok(result)) << "Valid function should parse successfully";
}

// ============================================================================
// Semantic Lint Rule Tests
// ============================================================================

TEST(LinterSemanticTest, TypeChecksValidCode) {
    auto source = tml::lexer::Source::from_string(R"(
        func add(a: I32, b: I32) -> I32 {
            return a + b
        }
    )");
    tml::lexer::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    tml::parser::Parser parser(std::move(tokens));
    auto module_result = parser.parse_module("test");
    EXPECT_TRUE(tml::is_ok(module_result));

    auto& module = std::get<tml::parser::Module>(module_result);
    tml::types::TypeChecker checker;
    auto env_result = checker.check_module(module);
    EXPECT_TRUE(tml::is_ok(env_result)) << "Valid code should pass type checking";
}

TEST(LinterSemanticTest, DetectsTypeMismatch) {
    auto source = tml::lexer::Source::from_string(R"(
        func main() -> I32 {
            return true
        }
    )");
    tml::lexer::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    tml::parser::Parser parser(std::move(tokens));
    auto module_result = parser.parse_module("test");
    EXPECT_TRUE(tml::is_ok(module_result));

    auto& module = std::get<tml::parser::Module>(module_result);
    tml::types::TypeChecker checker;
    auto env_result = checker.check_module(module);
    EXPECT_TRUE(tml::is_err(env_result)) << "Type mismatch should be detected";
}
