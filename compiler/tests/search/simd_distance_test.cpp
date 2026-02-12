//! # SIMD Distance Functions — Unit Tests
//!
//! Tests correctness of vector distance and similarity functions.
//! Validates dot product, cosine similarity, euclidean distance,
//! normalization, and edge cases (zero vectors, identical vectors, etc.).

#include "search/simd_distance.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <vector>

using namespace tml::search;

// ============================================================================
// Dot Product
// ============================================================================

TEST(SimdDistanceTest, DotProductIdentical) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f};
    float result = dot_product_f32(a.data(), a.data(), a.size());
    // 1*1 + 2*2 + 3*3 + 4*4 = 1 + 4 + 9 + 16 = 30
    EXPECT_FLOAT_EQ(result, 30.0f);
}

TEST(SimdDistanceTest, DotProductOrthogonal) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f, 0.0f};
    EXPECT_FLOAT_EQ(dot_product_f32(a.data(), b.data(), a.size()), 0.0f);
}

TEST(SimdDistanceTest, DotProductOpposite) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {-1.0f, -2.0f, -3.0f};
    EXPECT_FLOAT_EQ(dot_product_f32(a.data(), b.data(), a.size()), -14.0f);
}

TEST(SimdDistanceTest, DotProductZeroVector) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {0.0f, 0.0f, 0.0f};
    EXPECT_FLOAT_EQ(dot_product_f32(a.data(), b.data(), a.size()), 0.0f);
}

TEST(SimdDistanceTest, DotProductSingleElement) {
    std::vector<float> a = {5.0f};
    std::vector<float> b = {3.0f};
    EXPECT_FLOAT_EQ(dot_product_f32(a.data(), b.data(), 1), 15.0f);
}

TEST(SimdDistanceTest, DotProductEmpty) {
    float a = 0, b = 0;
    EXPECT_FLOAT_EQ(dot_product_f32(&a, &b, 0), 0.0f);
}

TEST(SimdDistanceTest, DotProductLargeVector) {
    const size_t N = 1024;
    std::vector<float> a(N, 1.0f);
    std::vector<float> b(N, 2.0f);
    EXPECT_FLOAT_EQ(dot_product_f32(a.data(), b.data(), N), 2048.0f);
}

// ============================================================================
// Cosine Similarity
// ============================================================================

TEST(SimdDistanceTest, CosineSimilarityIdentical) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    float result = cosine_similarity_f32(a.data(), a.data(), a.size());
    EXPECT_NEAR(result, 1.0f, 1e-6f);
}

TEST(SimdDistanceTest, CosineSimilarityOrthogonal) {
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f};
    EXPECT_NEAR(cosine_similarity_f32(a.data(), b.data(), a.size()), 0.0f, 1e-6f);
}

TEST(SimdDistanceTest, CosineSimilarityOpposite) {
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {-1.0f, 0.0f};
    EXPECT_NEAR(cosine_similarity_f32(a.data(), b.data(), a.size()), -1.0f, 1e-6f);
}

TEST(SimdDistanceTest, CosineSimilarityScaleInvariant) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {10.0f, 20.0f, 30.0f};
    // Cosine similarity should be 1.0 for parallel vectors regardless of magnitude
    EXPECT_NEAR(cosine_similarity_f32(a.data(), b.data(), a.size()), 1.0f, 1e-6f);
}

TEST(SimdDistanceTest, CosineSimilarityZeroVector) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {0.0f, 0.0f, 0.0f};
    // Should handle gracefully (return 0)
    EXPECT_NEAR(cosine_similarity_f32(a.data(), b.data(), a.size()), 0.0f, 1e-6f);
}

TEST(SimdDistanceTest, CosineSimilarityKnownAngle) {
    // 45 degrees: cos(pi/4) = sqrt(2)/2 ≈ 0.7071
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {1.0f, 1.0f};
    float expected = 1.0f / std::sqrt(2.0f);
    EXPECT_NEAR(cosine_similarity_f32(a.data(), b.data(), a.size()), expected, 1e-5f);
}

// ============================================================================
// Euclidean Distance
// ============================================================================

TEST(SimdDistanceTest, EuclideanDistanceIdentical) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(euclidean_distance_f32(a.data(), a.data(), a.size()), 0.0f);
}

TEST(SimdDistanceTest, EuclideanDistanceUnitVectors) {
    std::vector<float> a = {0.0f, 0.0f};
    std::vector<float> b = {3.0f, 4.0f};
    // sqrt(9 + 16) = 5
    EXPECT_FLOAT_EQ(euclidean_distance_f32(a.data(), b.data(), a.size()), 5.0f);
}

TEST(SimdDistanceTest, EuclideanDistanceSingleDim) {
    std::vector<float> a = {0.0f};
    std::vector<float> b = {7.0f};
    EXPECT_FLOAT_EQ(euclidean_distance_f32(a.data(), b.data(), 1), 7.0f);
}

TEST(SimdDistanceTest, EuclideanDistanceSymmetric) {
    std::vector<float> a = {1.0f, 5.0f, 9.0f};
    std::vector<float> b = {4.0f, 2.0f, 6.0f};
    float d_ab = euclidean_distance_f32(a.data(), b.data(), a.size());
    float d_ba = euclidean_distance_f32(b.data(), a.data(), a.size());
    EXPECT_FLOAT_EQ(d_ab, d_ba);
}

TEST(SimdDistanceTest, EuclideanDistanceTriangleInequality) {
    std::vector<float> a = {0.0f, 0.0f};
    std::vector<float> b = {1.0f, 0.0f};
    std::vector<float> c = {0.0f, 1.0f};
    float d_ab = euclidean_distance_f32(a.data(), b.data(), 2);
    float d_bc = euclidean_distance_f32(b.data(), c.data(), 2);
    float d_ac = euclidean_distance_f32(a.data(), c.data(), 2);
    EXPECT_LE(d_ac, d_ab + d_bc + 1e-6f);
}

// ============================================================================
// L2 Distance Squared
// ============================================================================

TEST(SimdDistanceTest, L2SquaredConsistentWithEuclidean) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> b = {5.0f, 6.0f, 7.0f, 8.0f};
    float l2sq = l2_distance_squared_f32(a.data(), b.data(), a.size());
    float l2 = euclidean_distance_f32(a.data(), b.data(), a.size());
    EXPECT_NEAR(l2sq, l2 * l2, 1e-5f);
}

TEST(SimdDistanceTest, L2SquaredPreservesOrdering) {
    std::vector<float> q = {0.0f, 0.0f};
    std::vector<float> a = {1.0f, 0.0f}; // distance = 1
    std::vector<float> b = {3.0f, 4.0f}; // distance = 5
    float d_qa = l2_distance_squared_f32(q.data(), a.data(), 2);
    float d_qb = l2_distance_squared_f32(q.data(), b.data(), 2);
    // Same ordering as euclidean distance
    EXPECT_LT(d_qa, d_qb);
}

// ============================================================================
// Normalization
// ============================================================================

TEST(SimdDistanceTest, NormalizeUnitLength) {
    std::vector<float> v = {3.0f, 4.0f};
    normalize_f32(v.data(), v.size());
    float length = norm_f32(v.data(), v.size());
    EXPECT_NEAR(length, 1.0f, 1e-6f);
}

TEST(SimdDistanceTest, NormalizeDirection) {
    std::vector<float> v = {3.0f, 4.0f};
    normalize_f32(v.data(), v.size());
    // Direction should be preserved: 3/5, 4/5
    EXPECT_NEAR(v[0], 0.6f, 1e-6f);
    EXPECT_NEAR(v[1], 0.8f, 1e-6f);
}

TEST(SimdDistanceTest, NormalizeAlreadyUnit) {
    std::vector<float> v = {1.0f, 0.0f, 0.0f};
    normalize_f32(v.data(), v.size());
    EXPECT_NEAR(v[0], 1.0f, 1e-6f);
    EXPECT_NEAR(v[1], 0.0f, 1e-6f);
    EXPECT_NEAR(v[2], 0.0f, 1e-6f);
}

TEST(SimdDistanceTest, NormalizeZeroVector) {
    std::vector<float> v = {0.0f, 0.0f, 0.0f};
    normalize_f32(v.data(), v.size());
    // Should remain zero (no division by zero crash)
    EXPECT_FLOAT_EQ(v[0], 0.0f);
    EXPECT_FLOAT_EQ(v[1], 0.0f);
    EXPECT_FLOAT_EQ(v[2], 0.0f);
}

TEST(SimdDistanceTest, NormalizeThenDotProductEqualsCosine) {
    std::vector<float> a = {1.0f, 3.0f, 5.0f, 7.0f};
    std::vector<float> b = {2.0f, 4.0f, 6.0f, 8.0f};

    float cos_sim = cosine_similarity_f32(a.data(), b.data(), a.size());

    normalize_f32(a.data(), a.size());
    normalize_f32(b.data(), b.size());
    float dot_after = dot_product_f32(a.data(), b.data(), a.size());

    EXPECT_NEAR(cos_sim, dot_after, 1e-5f);
}

// ============================================================================
// Norm
// ============================================================================

TEST(SimdDistanceTest, NormBasic) {
    std::vector<float> v = {3.0f, 4.0f};
    EXPECT_FLOAT_EQ(norm_f32(v.data(), v.size()), 5.0f);
}

TEST(SimdDistanceTest, NormZero) {
    std::vector<float> v = {0.0f, 0.0f};
    EXPECT_FLOAT_EQ(norm_f32(v.data(), v.size()), 0.0f);
}

TEST(SimdDistanceTest, NormUnitVector) {
    std::vector<float> v = {0.0f, 0.0f, 1.0f};
    EXPECT_FLOAT_EQ(norm_f32(v.data(), v.size()), 1.0f);
}

// ============================================================================
// High-Dimensional Vectors (realistic for HNSW usage)
// ============================================================================

TEST(SimdDistanceTest, HighDimensionalCosine) {
    const size_t N = 512;
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    std::vector<float> a(N), b(N);
    for (size_t i = 0; i < N; ++i) {
        a[i] = dist(rng);
        b[i] = dist(rng);
    }

    float sim = cosine_similarity_f32(a.data(), b.data(), N);
    // Random high-dim vectors should have cosine similarity near 0
    EXPECT_NEAR(sim, 0.0f, 0.15f);

    // Self-similarity should be 1
    float self_sim = cosine_similarity_f32(a.data(), a.data(), N);
    EXPECT_NEAR(self_sim, 1.0f, 1e-5f);
}

TEST(SimdDistanceTest, HighDimensionalNormalize) {
    const size_t N = 1024;
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-10.0f, 10.0f);

    std::vector<float> v(N);
    for (size_t i = 0; i < N; ++i) {
        v[i] = dist(rng);
    }

    normalize_f32(v.data(), N);
    float length = norm_f32(v.data(), N);
    EXPECT_NEAR(length, 1.0f, 1e-5f);
}
