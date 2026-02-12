//! # BM25 Text Index — Unit Tests
//!
//! Tests tokenization, indexing, scoring, and search accuracy of the BM25
//! full-text search index. Validates TF-IDF scoring, field boosting, stop
//! word filtering, and camelCase/snake_case splitting.

#include "search/bm25_index.hpp"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <string>
#include <unordered_set>
#include <vector>

using namespace tml::search;

// ============================================================================
// Tokenizer Tests
// ============================================================================

TEST(BM25TokenizerTest, BasicWhitespace) {
    auto tokens = BM25Index::tokenize("hello world foo");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "foo");
}

TEST(BM25TokenizerTest, CamelCaseSplitting) {
    auto tokens = BM25Index::tokenize("HashMap");
    // Should split into ["hash", "map"]
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0], "hash");
    EXPECT_EQ(tokens[1], "map");
}

TEST(BM25TokenizerTest, SnakeCaseSplitting) {
    auto tokens = BM25Index::tokenize("hash_map_insert");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "hash");
    EXPECT_EQ(tokens[1], "map");
    EXPECT_EQ(tokens[2], "insert");
}

TEST(BM25TokenizerTest, MixedCaseAndUnderscore) {
    auto tokens = BM25Index::tokenize("getHashMap_value");
    // "get" -> "get", "Hash" -> "hash", "Map" -> "map", "value" -> "value"
    ASSERT_GE(tokens.size(), 3u);
    // Check that "hash", "map", "value" are present
    std::unordered_set<std::string> token_set(tokens.begin(), tokens.end());
    EXPECT_TRUE(token_set.count("hash") > 0);
    EXPECT_TRUE(token_set.count("map") > 0);
    EXPECT_TRUE(token_set.count("value") > 0);
}

TEST(BM25TokenizerTest, Punctuation) {
    auto tokens = BM25Index::tokenize("split(s: Str, delim: Str) -> List[Str]");
    std::unordered_set<std::string> token_set(tokens.begin(), tokens.end());
    EXPECT_TRUE(token_set.count("split") > 0);
    EXPECT_TRUE(token_set.count("str") > 0);
    EXPECT_TRUE(token_set.count("delim") > 0);
    EXPECT_TRUE(token_set.count("list") > 0);
}

TEST(BM25TokenizerTest, Lowercasing) {
    auto tokens = BM25Index::tokenize("UPPERCASE MiXeD lowercase");
    for (const auto& t : tokens) {
        for (char c : t) {
            EXPECT_TRUE(std::islower(static_cast<unsigned char>(c)) ||
                        std::isdigit(static_cast<unsigned char>(c)))
                << "Token '" << t << "' contains uppercase char";
        }
    }
}

TEST(BM25TokenizerTest, StopWordFiltering) {
    auto tokens = BM25Index::tokenize("the function is a method");
    // "the", "is", "a" should be filtered; "function" is also a stop word
    // "method" should remain
    std::unordered_set<std::string> token_set(tokens.begin(), tokens.end());
    EXPECT_TRUE(token_set.count("the") == 0) << "'the' should be filtered";
    EXPECT_TRUE(token_set.count("is") == 0) << "'is' should be filtered";
    EXPECT_TRUE(token_set.count("method") > 0) << "'method' should remain";
}

TEST(BM25TokenizerTest, TMLKeywordFiltering) {
    auto tokens = BM25Index::tokenize("func let var pub split");
    std::unordered_set<std::string> token_set(tokens.begin(), tokens.end());
    EXPECT_TRUE(token_set.count("func") == 0) << "'func' should be filtered";
    EXPECT_TRUE(token_set.count("let") == 0) << "'let' should be filtered";
    EXPECT_TRUE(token_set.count("var") == 0) << "'var' should be filtered";
    EXPECT_TRUE(token_set.count("pub") == 0) << "'pub' should be filtered";
    EXPECT_TRUE(token_set.count("split") > 0) << "'split' should remain";
}

TEST(BM25TokenizerTest, ShortTokensFiltered) {
    auto tokens = BM25Index::tokenize("I a x ab foo");
    // Single-char tokens should be filtered (< 2 chars)
    std::unordered_set<std::string> token_set(tokens.begin(), tokens.end());
    EXPECT_TRUE(token_set.count("i") == 0);
    EXPECT_TRUE(token_set.count("x") == 0);
    EXPECT_TRUE(token_set.count("ab") > 0);
    EXPECT_TRUE(token_set.count("foo") > 0);
}

TEST(BM25TokenizerTest, EmptyInput) {
    auto tokens = BM25Index::tokenize("");
    EXPECT_TRUE(tokens.empty());
}

TEST(BM25TokenizerTest, OnlyStopWords) {
    auto tokens = BM25Index::tokenize("the a an is are");
    EXPECT_TRUE(tokens.empty());
}

// ============================================================================
// Index Building & Search
// ============================================================================

class BM25IndexTest : public ::testing::Test {
protected:
    BM25Index index;

    void SetUp() override {
        // Build a small documentation corpus
        index.add_document(0, "split", "pub func split(this, delimiter: Str) -> List[Str]",
                           "Splits the string by the given delimiter and returns a list",
                           "core::str");
        index.add_document(1, "join", "pub func join(this, separator: Str) -> Str",
                           "Joins a list of strings with a separator", "core::str");
        index.add_document(2, "HashMap", "pub type HashMap[K, V]",
                           "A hash table mapping keys to values with O(1) average lookup",
                           "std::collections");
        index.add_document(3, "contains", "pub func contains(this, needle: Str) -> Bool",
                           "Returns true if the string contains the given substring", "core::str");
        index.add_document(4, "fnv1a64", "pub func fnv1a64(data: Str) -> Hash64",
                           "Computes the FNV-1a 64-bit hash of a string", "std::hash");
        index.add_document(5, "Maybe", "pub type Maybe[T]",
                           "Represents an optional value that may or may not be present", "core");
        index.add_document(6, "parse",
                           "pub func parse(input: Str) -> Outcome[JsonValue, JsonError]",
                           "Parses a JSON string into a JsonValue", "std::json");
        index.add_document(7, "sort", "pub func sort(this) -> List[T]",
                           "Sorts the list in ascending order using the default comparison",
                           "core::slice");
        index.add_document(8, "to_upper", "pub func to_upper(this) -> Str",
                           "Converts all characters in the string to uppercase", "core::str");
        index.add_document(9, "filter", "pub func filter(this, pred: func(T) -> Bool) -> List[T]",
                           "Returns a new list containing only elements that satisfy the predicate",
                           "core::iter");
        index.build();
    }
};

TEST_F(BM25IndexTest, SearchByExactName) {
    auto results = index.search("split", 10);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].doc_id, 0u) << "Exact name match 'split' should rank first";
    EXPECT_GT(results[0].score, 0.0f);
}

TEST_F(BM25IndexTest, SearchByPartialName) {
    auto results = index.search("HashMap", 10);
    ASSERT_FALSE(results.empty());
    // "HashMap" tokenizes to "hash" + "map" — should find doc_id 2
    bool found = false;
    for (const auto& r : results) {
        if (r.doc_id == 2) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "'HashMap' search should find doc_id 2";
}

TEST_F(BM25IndexTest, SearchByDocText) {
    auto results = index.search("optional value", 10);
    ASSERT_FALSE(results.empty());
    // "optional" appears in Maybe's doc text
    bool found_maybe = false;
    for (const auto& r : results) {
        if (r.doc_id == 5) {
            found_maybe = true;
            break;
        }
    }
    EXPECT_TRUE(found_maybe) << "'optional value' should find Maybe type";
}

TEST_F(BM25IndexTest, SearchBySignature) {
    auto results = index.search("JsonValue", 10);
    ASSERT_FALSE(results.empty());
    bool found_parse = false;
    for (const auto& r : results) {
        if (r.doc_id == 6) {
            found_parse = true;
            break;
        }
    }
    EXPECT_TRUE(found_parse) << "'JsonValue' should find parse function";
}

TEST_F(BM25IndexTest, NameBoostingRanksHigher) {
    // "sort" is both a name match (doc 7) and might appear in doc text
    auto results = index.search("sort", 10);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].doc_id, 7u)
        << "Name match for 'sort' should rank higher than doc text match";
}

TEST_F(BM25IndexTest, NoResults) {
    auto results = index.search("xyznonexistent", 10);
    EXPECT_TRUE(results.empty());
}

TEST_F(BM25IndexTest, LimitRespected) {
    auto results = index.search("str", 3);
    EXPECT_LE(results.size(), 3u);
}

TEST_F(BM25IndexTest, ScoresDescending) {
    auto results = index.search("string delimiter", 10);
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i - 1].score, results[i].score)
            << "Results should be sorted by score descending";
    }
}

TEST_F(BM25IndexTest, AllScoresPositive) {
    auto results = index.search("hash", 10);
    for (const auto& r : results) {
        EXPECT_GT(r.score, 0.0f) << "All returned results should have positive score";
    }
}

TEST_F(BM25IndexTest, EmptyQueryNoResults) {
    auto results = index.search("", 10);
    EXPECT_TRUE(results.empty());
}

TEST_F(BM25IndexTest, MultiTermQueryBoost) {
    // "string split" should boost the split doc since both terms match
    auto results_single = index.search("split", 10);
    auto results_multi = index.search("string split delimiter", 10);
    ASSERT_FALSE(results_single.empty());
    ASSERT_FALSE(results_multi.empty());
    // Both should rank doc 0 first
    EXPECT_EQ(results_single[0].doc_id, 0u);
    EXPECT_EQ(results_multi[0].doc_id, 0u);
}

// ============================================================================
// IDF Scoring
// ============================================================================

TEST_F(BM25IndexTest, IDFRareTerm) {
    // "fnv1a64" appears in only 1 document — should have high IDF
    float idf_rare = index.idf("fnv1a64");
    // "str" appears in multiple docs — should have lower IDF
    float idf_common = index.idf("str");
    if (idf_rare > 0 && idf_common > 0) {
        EXPECT_GT(idf_rare, idf_common)
            << "Rare term 'fnv1a64' should have higher IDF than common 'str'";
    }
}

TEST_F(BM25IndexTest, IDFUnknownTerm) {
    EXPECT_FLOAT_EQ(index.idf("zzzznonexistent"), 0.0f);
}

// ============================================================================
// Index Properties
// ============================================================================

TEST_F(BM25IndexTest, SizeCorrect) {
    EXPECT_EQ(index.size(), 10u);
}

TEST_F(BM25IndexTest, VocabularyNonEmpty) {
    EXPECT_FALSE(index.vocabulary().empty());
}

TEST_F(BM25IndexTest, VocabularyContainsExpectedTerms) {
    const auto& vocab = index.vocabulary();
    EXPECT_TRUE(vocab.count("split") > 0);
    EXPECT_TRUE(vocab.count("hash") > 0);
    EXPECT_TRUE(vocab.count("json") > 0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(BM25EdgeCaseTest, EmptyIndex) {
    BM25Index empty_index;
    empty_index.build();
    auto results = empty_index.search("anything", 10);
    EXPECT_TRUE(results.empty());
}

TEST(BM25EdgeCaseTest, SingleDocument) {
    BM25Index single;
    single.add_document(0, "foo", "func foo()", "does stuff", "mod");
    single.build();
    auto results = single.search("foo", 10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].doc_id, 0u);
}

TEST(BM25EdgeCaseTest, SearchBeforeBuild) {
    BM25Index unbuild;
    unbuild.add_document(0, "test", "test", "test", "test");
    // Should handle gracefully (not crash)
    auto results = unbuild.search("test", 10);
    EXPECT_TRUE(results.empty());
}

TEST(BM25EdgeCaseTest, DuplicateDocuments) {
    BM25Index idx;
    idx.add_document(0, "split", "func split()", "splits string", "core::str");
    idx.add_document(1, "split", "func split()", "splits string", "core::str");
    idx.build();
    auto results = idx.search("split", 10);
    EXPECT_EQ(results.size(), 2u);
}

TEST(BM25EdgeCaseTest, EmptyFields) {
    BM25Index idx;
    idx.add_document(0, "", "", "", "");
    idx.add_document(1, "test", "", "", "");
    idx.build();
    auto results = idx.search("test", 10);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].doc_id, 1u);
}

TEST(BM25EdgeCaseTest, ParameterTuning) {
    BM25Index idx;
    idx.k1 = 2.0f;
    idx.b = 0.5f;
    idx.name_boost = 5.0f;
    idx.add_document(0, "test", "func test()", "testing stuff", "mod");
    idx.add_document(1, "other", "func other()", "other testing", "mod");
    idx.build();
    auto results = idx.search("test", 10);
    ASSERT_FALSE(results.empty());
    // Name match should still rank first with increased boost
    EXPECT_EQ(results[0].doc_id, 0u);
}
