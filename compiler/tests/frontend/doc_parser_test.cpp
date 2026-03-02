// Tests for the documentation parser
// Covers: doc/doc_parser.cpp, doc/extractor.cpp, doc/signature.cpp, doc/generators.cpp

#include "doc/doc_parser.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace tml::doc;

// ============================================================================
// Basic Doc Parsing
// ============================================================================

TEST(DocParserTest, EmptyDoc) {
    auto result = parse_doc_comment("");
    EXPECT_TRUE(result.summary.empty());
    EXPECT_TRUE(result.params.empty());
}

TEST(DocParserTest, SummaryOnly) {
    auto result = parse_doc_comment("A simple function that adds two numbers.");
    EXPECT_FALSE(result.summary.empty());
    EXPECT_NE(result.summary.find("adds two numbers"), std::string::npos);
}

TEST(DocParserTest, SummaryAndBody) {
    auto result = parse_doc_comment("Computes the factorial of n.\n\n"
                                    "Uses an iterative approach for efficiency.");
    EXPECT_FALSE(result.summary.empty());
    EXPECT_NE(result.summary.find("factorial"), std::string::npos);
}

// ============================================================================
// Parameter Tags
// ============================================================================

TEST(DocParserTest, SingleParam) {
    auto result = parse_doc_comment("@param x The input value");
    EXPECT_EQ(result.params.size(), 1u);
    if (!result.params.empty()) {
        EXPECT_EQ(result.params[0].name, "x");
        EXPECT_NE(result.params[0].description.find("input value"), std::string::npos);
    }
}

TEST(DocParserTest, MultipleParams) {
    auto result = parse_doc_comment("Add two numbers.\n"
                                    "@param a First operand\n"
                                    "@param b Second operand");
    EXPECT_EQ(result.params.size(), 2u);
}

// ============================================================================
// Returns Tag
// ============================================================================

TEST(DocParserTest, ReturnsTag) {
    auto result = parse_doc_comment("@returns The sum of a and b");
    EXPECT_TRUE(result.returns.has_value());
    if (result.returns) {
        EXPECT_NE(result.returns->description.find("sum"), std::string::npos);
    }
}

// ============================================================================
// Throws Tag
// ============================================================================

TEST(DocParserTest, ThrowsTag) {
    auto result = parse_doc_comment("@throws ValueError If input is negative");
    EXPECT_EQ(result.throws.size(), 1u);
}

// ============================================================================
// Example Tag
// ============================================================================

TEST(DocParserTest, ExampleCodeBlock) {
    auto result = parse_doc_comment("Adds numbers.\n\n"
                                    "```tml\n"
                                    "let x = add(1, 2)\n"
                                    "```");
    EXPECT_GE(result.examples.size(), 1u);
}

// ============================================================================
// See Also
// ============================================================================

TEST(DocParserTest, SeeAlsoTag) {
    auto result = parse_doc_comment("@see subtract\n@see multiply");
    EXPECT_EQ(result.see_also.size(), 2u);
}

// ============================================================================
// Since/Deprecated
// ============================================================================

TEST(DocParserTest, SinceTag) {
    auto result = parse_doc_comment("@since 1.0");
    EXPECT_TRUE(result.since.has_value());
    if (result.since) {
        EXPECT_EQ(*result.since, "1.0");
    }
}

TEST(DocParserTest, DeprecatedTag) {
    auto result = parse_doc_comment("@deprecated Use new_func instead");
    EXPECT_TRUE(result.deprecated.has_value());
}

// ============================================================================
// Complex Documentation
// ============================================================================

TEST(DocParserTest, FullDocComment) {
    auto result = parse_doc_comment("Sorts a slice in ascending order.\n\n"
                                    "Uses an optimized quicksort algorithm.\n\n"
                                    "@param slice The slice to sort\n"
                                    "@returns The sorted slice\n"
                                    "@throws OutOfMemory If allocation fails\n"
                                    "@see sort_desc\n"
                                    "@since 1.0\n"
                                    "\n"
                                    "```tml\n"
                                    "let sorted = sort([3, 1, 2])\n"
                                    "```");

    EXPECT_FALSE(result.summary.empty());
    EXPECT_EQ(result.params.size(), 1u);
    EXPECT_TRUE(result.returns.has_value());
    EXPECT_EQ(result.throws.size(), 1u);
    EXPECT_EQ(result.see_also.size(), 1u);
    EXPECT_TRUE(result.since.has_value());
    EXPECT_GE(result.examples.size(), 1u);
}
