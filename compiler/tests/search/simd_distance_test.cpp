//! # SIMD Distance Functions — Unit Tests
//!
//! Tests correctness of vector distance and similarity functions for both
//! f32 and f64 types, plus batch distance operations.
//! Validates dot product, cosine similarity, euclidean distance,
//! normalization, batch operations, and edge cases.

#include "search/simd_distance.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <vector>

using namespace tml::search;

// ============================================================================
// Float (f32) — Dot Product
// ============================================================================

TEST(SimdDistanceTest, DotProductIdentical) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f};
    float result = dot_product_f32(a.data(), a.data(), a.size());
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
// Float (f32) — Cosine Similarity
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
    EXPECT_NEAR(cosine_similarity_f32(a.data(), b.data(), a.size()), 1.0f, 1e-6f);
}

TEST(SimdDistanceTest, CosineSimilarityZeroVector) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {0.0f, 0.0f, 0.0f};
    EXPECT_NEAR(cosine_similarity_f32(a.data(), b.data(), a.size()), 0.0f, 1e-6f);
}

TEST(SimdDistanceTest, CosineSimilarityKnownAngle) {
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {1.0f, 1.0f};
    float expected = 1.0f / std::sqrt(2.0f);
    EXPECT_NEAR(cosine_similarity_f32(a.data(), b.data(), a.size()), expected, 1e-5f);
}

// ============================================================================
// Float (f32) — Euclidean Distance
// ============================================================================

TEST(SimdDistanceTest, EuclideanDistanceIdentical) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(euclidean_distance_f32(a.data(), a.data(), a.size()), 0.0f);
}

TEST(SimdDistanceTest, EuclideanDistanceUnitVectors) {
    std::vector<float> a = {0.0f, 0.0f};
    std::vector<float> b = {3.0f, 4.0f};
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
// Float (f32) — L2 Distance Squared
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
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {3.0f, 4.0f};
    float d_qa = l2_distance_squared_f32(q.data(), a.data(), 2);
    float d_qb = l2_distance_squared_f32(q.data(), b.data(), 2);
    EXPECT_LT(d_qa, d_qb);
}

// ============================================================================
// Float (f32) — Normalization
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
// Float (f32) — Norm
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
// Float (f32) — High-Dimensional Vectors
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
    EXPECT_NEAR(sim, 0.0f, 0.15f);

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

// ============================================================================
// Double (f64) — Dot Product
// ============================================================================

TEST(SimdDistanceF64Test, DotProductIdentical) {
    std::vector<double> a = {1.0, 2.0, 3.0, 4.0};
    double result = dot_product_f64(a.data(), a.data(), a.size());
    EXPECT_DOUBLE_EQ(result, 30.0);
}

TEST(SimdDistanceF64Test, DotProductOrthogonal) {
    std::vector<double> a = {1.0, 0.0, 0.0};
    std::vector<double> b = {0.0, 1.0, 0.0};
    EXPECT_DOUBLE_EQ(dot_product_f64(a.data(), b.data(), a.size()), 0.0);
}

TEST(SimdDistanceF64Test, DotProductLarge) {
    const size_t N = 1024;
    std::vector<double> a(N, 1.0);
    std::vector<double> b(N, 2.0);
    EXPECT_DOUBLE_EQ(dot_product_f64(a.data(), b.data(), N), 2048.0);
}

TEST(SimdDistanceF64Test, DotProductOddLength) {
    // 7 elements — not aligned to AVX2 (4 doubles)
    std::vector<double> a = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    std::vector<double> b = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    EXPECT_DOUBLE_EQ(dot_product_f64(a.data(), b.data(), a.size()), 28.0);
}

// ============================================================================
// Double (f64) — Cosine Similarity
// ============================================================================

TEST(SimdDistanceF64Test, CosineSimilarityIdentical) {
    std::vector<double> a = {1.0, 2.0, 3.0};
    double result = cosine_similarity_f64(a.data(), a.data(), a.size());
    EXPECT_NEAR(result, 1.0, 1e-12);
}

TEST(SimdDistanceF64Test, CosineSimilarityOrthogonal) {
    std::vector<double> a = {1.0, 0.0};
    std::vector<double> b = {0.0, 1.0};
    EXPECT_NEAR(cosine_similarity_f64(a.data(), b.data(), a.size()), 0.0, 1e-12);
}

TEST(SimdDistanceF64Test, CosineSimilarityZeroVector) {
    std::vector<double> a = {1.0, 2.0, 3.0};
    std::vector<double> b = {0.0, 0.0, 0.0};
    EXPECT_NEAR(cosine_similarity_f64(a.data(), b.data(), a.size()), 0.0, 1e-12);
}

// ============================================================================
// Double (f64) — Euclidean Distance & L2 Squared
// ============================================================================

TEST(SimdDistanceF64Test, EuclideanDistance345) {
    std::vector<double> a = {0.0, 0.0};
    std::vector<double> b = {3.0, 4.0};
    EXPECT_DOUBLE_EQ(euclidean_distance_f64(a.data(), b.data(), 2), 5.0);
}

TEST(SimdDistanceF64Test, L2SquaredConsistentWithEuclidean) {
    std::vector<double> a = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> b = {5.0, 6.0, 7.0, 8.0};
    double l2sq = l2_distance_squared_f64(a.data(), b.data(), a.size());
    double l2 = euclidean_distance_f64(a.data(), b.data(), a.size());
    EXPECT_NEAR(l2sq, l2 * l2, 1e-10);
}

// ============================================================================
// Double (f64) — Normalization & Norm
// ============================================================================

TEST(SimdDistanceF64Test, NormBasic) {
    std::vector<double> v = {3.0, 4.0};
    EXPECT_DOUBLE_EQ(norm_f64(v.data(), v.size()), 5.0);
}

TEST(SimdDistanceF64Test, NormalizeUnitLength) {
    std::vector<double> v = {3.0, 4.0};
    normalize_f64(v.data(), v.size());
    double length = norm_f64(v.data(), v.size());
    EXPECT_NEAR(length, 1.0, 1e-12);
}

TEST(SimdDistanceF64Test, NormalizeDirection) {
    std::vector<double> v = {3.0, 4.0};
    normalize_f64(v.data(), v.size());
    EXPECT_NEAR(v[0], 0.6, 1e-12);
    EXPECT_NEAR(v[1], 0.8, 1e-12);
}

TEST(SimdDistanceF64Test, NormalizeZeroVector) {
    std::vector<double> v = {0.0, 0.0, 0.0};
    normalize_f64(v.data(), v.size());
    EXPECT_DOUBLE_EQ(v[0], 0.0);
    EXPECT_DOUBLE_EQ(v[1], 0.0);
    EXPECT_DOUBLE_EQ(v[2], 0.0);
}

TEST(SimdDistanceF64Test, HighDimensionalNormalize) {
    const size_t N = 1024;
    std::mt19937 rng(456);
    std::uniform_real_distribution<double> dist(-10.0, 10.0);

    std::vector<double> v(N);
    for (size_t i = 0; i < N; ++i) {
        v[i] = dist(rng);
    }

    normalize_f64(v.data(), N);
    double length = norm_f64(v.data(), N);
    EXPECT_NEAR(length, 1.0, 1e-10);
}

// ============================================================================
// Batch Distance (f32 x4)
// ============================================================================

TEST(SimdDistanceBatchTest, BatchL2Squared_Basic) {
    const size_t DIM = 4;
    std::vector<float> query = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> c0 = {1.0f, 2.0f, 3.0f, 4.0f}; // same = 0
    std::vector<float> c1 = {2.0f, 3.0f, 4.0f, 5.0f}; // diff 1 each = 4
    std::vector<float> c2 = {0.0f, 0.0f, 0.0f, 0.0f}; // diff = 1+4+9+16=30
    std::vector<float> c3 = {5.0f, 6.0f, 7.0f, 8.0f}; // diff 4 each = 64

    const float* candidates[4] = {c0.data(), c1.data(), c2.data(), c3.data()};
    float out[4] = {};
    batch_l2_squared_f32_x4(query.data(), candidates, DIM, out);

    EXPECT_NEAR(out[0], 0.0f, 1e-6f);
    EXPECT_NEAR(out[1], 4.0f, 1e-6f);
    EXPECT_NEAR(out[2], 30.0f, 1e-6f);
    EXPECT_NEAR(out[3], 64.0f, 1e-6f);
}

TEST(SimdDistanceBatchTest, BatchL2Squared_MatchesSingle) {
    const size_t DIM = 512;
    std::mt19937 rng(99);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    std::vector<float> query(DIM);
    std::vector<float> c0(DIM), c1(DIM), c2(DIM), c3(DIM);
    for (size_t i = 0; i < DIM; ++i) {
        query[i] = dist(rng);
        c0[i] = dist(rng);
        c1[i] = dist(rng);
        c2[i] = dist(rng);
        c3[i] = dist(rng);
    }

    // Single-shot reference
    float ref0 = l2_distance_squared_f32(query.data(), c0.data(), DIM);
    float ref1 = l2_distance_squared_f32(query.data(), c1.data(), DIM);
    float ref2 = l2_distance_squared_f32(query.data(), c2.data(), DIM);
    float ref3 = l2_distance_squared_f32(query.data(), c3.data(), DIM);

    // Batch
    const float* candidates[4] = {c0.data(), c1.data(), c2.data(), c3.data()};
    float out[4] = {};
    batch_l2_squared_f32_x4(query.data(), candidates, DIM, out);

    EXPECT_NEAR(out[0], ref0, 1e-3f);
    EXPECT_NEAR(out[1], ref1, 1e-3f);
    EXPECT_NEAR(out[2], ref2, 1e-3f);
    EXPECT_NEAR(out[3], ref3, 1e-3f);
}

TEST(SimdDistanceBatchTest, BatchL2Squared_OddDimension) {
    const size_t DIM = 7; // not multiple of 8 — exercises scalar tail
    std::vector<float> query = {1, 2, 3, 4, 5, 6, 7};
    std::vector<float> c0 = {1, 2, 3, 4, 5, 6, 7};
    std::vector<float> c1 = {0, 0, 0, 0, 0, 0, 0};
    std::vector<float> c2 = {7, 6, 5, 4, 3, 2, 1};
    std::vector<float> c3 = {2, 2, 2, 2, 2, 2, 2};

    const float* candidates[4] = {c0.data(), c1.data(), c2.data(), c3.data()};
    float out[4] = {};
    batch_l2_squared_f32_x4(query.data(), candidates, DIM, out);

    float ref0 = l2_distance_squared_f32(query.data(), c0.data(), DIM);
    float ref1 = l2_distance_squared_f32(query.data(), c1.data(), DIM);
    float ref2 = l2_distance_squared_f32(query.data(), c2.data(), DIM);
    float ref3 = l2_distance_squared_f32(query.data(), c3.data(), DIM);

    EXPECT_NEAR(out[0], ref0, 1e-6f);
    EXPECT_NEAR(out[1], ref1, 1e-6f);
    EXPECT_NEAR(out[2], ref2, 1e-6f);
    EXPECT_NEAR(out[3], ref3, 1e-6f);
}
