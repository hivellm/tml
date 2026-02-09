// Query Fingerprint tests
//
// Tests for the 128-bit fingerprint system used by incremental compilation.

#include "query/query_fingerprint.hpp"

#include <gtest/gtest.h>

using namespace tml::query;

// ============================================================================
// fingerprint_string()
// ============================================================================

TEST(QueryFingerprint, StringProducesNonZero) {
    auto fp = fingerprint_string("hello world");
    EXPECT_FALSE(fp.is_zero());
}

TEST(QueryFingerprint, SameInputSameFingerprint) {
    auto fp1 = fingerprint_string("test input");
    auto fp2 = fingerprint_string("test input");
    EXPECT_EQ(fp1, fp2);
}

TEST(QueryFingerprint, DifferentInputDifferentFingerprint) {
    auto fp1 = fingerprint_string("input A");
    auto fp2 = fingerprint_string("input B");
    EXPECT_NE(fp1, fp2);
}

TEST(QueryFingerprint, EmptyStringIsZero) {
    auto fp = fingerprint_string("");
    // CRC32C of empty data is 0, so the fingerprint is zero
    EXPECT_TRUE(fp.is_zero());
}

// ============================================================================
// fingerprint_bytes()
// ============================================================================

TEST(QueryFingerprint, BytesMatchesString) {
    std::string s = "hello";
    auto fp_str = fingerprint_string(s);
    auto fp_bytes = fingerprint_bytes(s.data(), s.size());
    EXPECT_EQ(fp_str, fp_bytes);
}

// ============================================================================
// fingerprint_combine()
// ============================================================================

TEST(QueryFingerprint, CombineIsOrderDependent) {
    auto a = fingerprint_string("alpha");
    auto b = fingerprint_string("beta");

    auto ab = fingerprint_combine(a, b);
    auto ba = fingerprint_combine(b, a);
    EXPECT_NE(ab, ba);
}

TEST(QueryFingerprint, CombineProducesNonZero) {
    auto a = fingerprint_string("x");
    auto b = fingerprint_string("y");
    auto combined = fingerprint_combine(a, b);
    EXPECT_FALSE(combined.is_zero());
}

TEST(QueryFingerprint, CombineDifferentFromInputs) {
    auto a = fingerprint_string("x");
    auto b = fingerprint_string("y");
    auto combined = fingerprint_combine(a, b);
    EXPECT_NE(combined, a);
    EXPECT_NE(combined, b);
}

// ============================================================================
// Fingerprint struct
// ============================================================================

TEST(QueryFingerprint, DefaultIsZero) {
    Fingerprint fp{};
    EXPECT_TRUE(fp.is_zero());
    EXPECT_EQ(fp.high, 0u);
    EXPECT_EQ(fp.low, 0u);
}

TEST(QueryFingerprint, ToHexReturns32Chars) {
    auto fp = fingerprint_string("test");
    auto hex = fp.to_hex();
    EXPECT_EQ(hex.size(), 32u);

    // Should only contain hex characters
    for (char c : hex) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST(QueryFingerprint, ZeroToHexIsAllZeros) {
    Fingerprint fp{};
    auto hex = fp.to_hex();
    EXPECT_EQ(hex.size(), 32u);
    EXPECT_EQ(hex, "00000000000000000000000000000000");
}

TEST(QueryFingerprint, EqualityOperator) {
    auto fp1 = fingerprint_string("same");
    auto fp2 = fingerprint_string("same");
    auto fp3 = fingerprint_string("different");

    EXPECT_EQ(fp1, fp2);
    EXPECT_NE(fp1, fp3);
}
