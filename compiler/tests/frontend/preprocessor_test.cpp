// Tests for the TML preprocessor
// Covers: preprocessor/preprocessor.cpp

#include "preprocessor/preprocessor.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace tml::preprocessor;

class PreprocessorTest : public ::testing::Test {
protected:
    Preprocessor pp_;
};

// ============================================================================
// Basic Directives
// ============================================================================

TEST_F(PreprocessorTest, NoDirectivesPassthrough) {
    std::string input = "func main() { }";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("func main"), std::string::npos)
        << "Code without directives should pass through unchanged";
}

TEST_F(PreprocessorTest, DefineSymbol) {
    pp_.define("DEBUG");
    std::string input = R"(
#ifdef DEBUG
func debug_func() { }
#endif
func main() { }
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("debug_func"), std::string::npos)
        << "Defined symbol should include guarded code";
}

TEST_F(PreprocessorTest, UndefinedSymbolExcludes) {
    std::string input = R"(
#ifdef FEATURE_X
func feature_x() { }
#endif
func main() { }
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_EQ(result.output.find("feature_x"), std::string::npos)
        << "Undefined symbol should exclude guarded code";
}

TEST_F(PreprocessorTest, IfndefDirective) {
    std::string input = R"(
#ifndef PRODUCTION
func debug_only() { }
#endif
func main() { }
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("debug_only"), std::string::npos)
        << "ifndef with undefined symbol should include code";
}

TEST_F(PreprocessorTest, IfndefWithDefinedSymbol) {
    pp_.define("PRODUCTION");
    std::string input = R"(
#ifndef PRODUCTION
func debug_only() { }
#endif
func main() { }
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_EQ(result.output.find("debug_only"), std::string::npos)
        << "ifndef with defined symbol should exclude code";
}

// ============================================================================
// If/Elif/Else
// ============================================================================

TEST_F(PreprocessorTest, IfElseDirective) {
    pp_.define("WINDOWS");
    std::string input = R"(
#if WINDOWS
func platform() -> I32 { return 1 }
#else
func platform() -> I32 { return 2 }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("return 1"), std::string::npos);
    EXPECT_EQ(result.output.find("return 2"), std::string::npos);
}

TEST_F(PreprocessorTest, ElseBranch) {
    std::string input = R"(
#if WINDOWS
func platform() -> I32 { return 1 }
#else
func platform() -> I32 { return 2 }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_EQ(result.output.find("return 1"), std::string::npos);
    EXPECT_NE(result.output.find("return 2"), std::string::npos);
}

TEST_F(PreprocessorTest, ElifDirective) {
    pp_.define("LINUX");
    std::string input = R"(
#if WINDOWS
func os() -> I32 { return 1 }
#elif LINUX
func os() -> I32 { return 2 }
#else
func os() -> I32 { return 3 }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("return 2"), std::string::npos);
}

// ============================================================================
// Logical Operators
// ============================================================================

TEST_F(PreprocessorTest, LogicalAnd) {
    pp_.define("DEBUG");
    pp_.define("WINDOWS");
    std::string input = R"(
#if DEBUG && WINDOWS
func debug_win() { }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("debug_win"), std::string::npos);
}

TEST_F(PreprocessorTest, LogicalOr) {
    pp_.define("LINUX");
    std::string input = R"(
#if WINDOWS || LINUX
func unix_like() { }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("unix_like"), std::string::npos);
}

TEST_F(PreprocessorTest, LogicalNot) {
    std::string input = R"(
#if !RELEASE
func debug_mode() { }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("debug_mode"), std::string::npos);
}

// ============================================================================
// Defined() Operator
// ============================================================================

TEST_F(PreprocessorTest, DefinedOperator) {
    pp_.define("FEATURE_A");
    std::string input = R"(
#if defined(FEATURE_A)
func feature_a() { }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("feature_a"), std::string::npos);
}

// ============================================================================
// Error/Warning Directives
// ============================================================================

TEST_F(PreprocessorTest, ErrorDirective) {
    std::string input = R"(
#error "compilation stopped"
)";
    auto result = pp_.process(input);
    EXPECT_FALSE(result.errors().empty()) << "#error should produce a compilation error";
}

// ============================================================================
// Nested Conditionals
// ============================================================================

TEST_F(PreprocessorTest, NestedConditionals) {
    pp_.define("A");
    pp_.define("B");
    std::string input = R"(
#ifdef A
    #ifdef B
func both() { }
    #endif
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("both"), std::string::npos);
}

// ============================================================================
// Predefined Symbols
// ============================================================================

TEST_F(PreprocessorTest, PredefinedWindowsSymbol) {
    pp_.set_target_os(TargetOS::Windows);
    std::string input = R"(
#ifdef WINDOWS
func win() { }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("win"), std::string::npos);
}

TEST_F(PreprocessorTest, PredefinedX86_64Symbol) {
    pp_.set_target_arch(TargetArch::X86_64);
    std::string input = R"(
#ifdef X86_64
func x64() { }
#endif
)";
    auto result = pp_.process(input);
    EXPECT_TRUE(result.errors().empty());
    EXPECT_NE(result.output.find("x64"), std::string::npos);
}
