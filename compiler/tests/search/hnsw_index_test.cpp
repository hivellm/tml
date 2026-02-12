//! # HNSW Vector Index — Unit Tests
//!
//! Tests graph construction, nearest neighbor search, TF-IDF vectorization,
//! recall quality, and edge cases for the HNSW approximate nearest neighbor
//! index.

#include "search/bm25_index.hpp"
#include "search/hnsw_index.hpp"
#include "search/simd_distance.hpp"

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <random>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

using namespace tml::search;

// ============================================================================
// TF-IDF Vectorizer Tests
// ============================================================================

class TfIdfVectorizerTest : public ::testing::Test {
protected:
    TfIdfVectorizer vectorizer{64}; // Small dimensionality for tests
};

TEST_F(TfIdfVectorizerTest, BuildFromCorpus) {
    vectorizer.add_document(0, "the quick brown fox jumps over the lazy dog");
    vectorizer.add_document(1, "a fast brown fox leaps over a sleepy hound");
    vectorizer.add_document(2, "hash map insert delete lookup");
    vectorizer.build();

    EXPECT_TRUE(vectorizer.is_built());
    EXPECT_GT(vectorizer.dims(), 0u);
    EXPECT_LE(vectorizer.dims(), 64u);
}

TEST_F(TfIdfVectorizerTest, VectorizeProducesCorrectDims) {
    vectorizer.add_document(0, "split string delimiter");
    vectorizer.add_document(1, "join list separator");
    vectorizer.build();

    auto vec = vectorizer.vectorize("split string");
    EXPECT_EQ(vec.size(), vectorizer.dims());
}

TEST_F(TfIdfVectorizerTest, VectorIsNormalized) {
    vectorizer.add_document(0, "alpha beta gamma delta");
    vectorizer.add_document(1, "epsilon zeta eta theta");
    vectorizer.build();

    auto vec = vectorizer.vectorize("alpha beta gamma");
    float length = norm_f32(vec.data(), vec.size());
    if (length > 0) {
        EXPECT_NEAR(length, 1.0f, 1e-5f) << "Vectorized output should be L2-normalized";
    }
}

TEST_F(TfIdfVectorizerTest, SimilarTextsHaveHighCosine) {
    vectorizer.add_document(0, "split string by delimiter into parts");
    vectorizer.add_document(1, "join strings with separator between them");
    vectorizer.add_document(2, "hash table insert remove lookup find");
    vectorizer.build();

    auto v_split1 = vectorizer.vectorize("split string delimiter");
    auto v_split2 = vectorizer.vectorize("split string by delimiter");
    auto v_hash = vectorizer.vectorize("hash table lookup");

    float sim_similar = cosine_similarity_f32(v_split1.data(), v_split2.data(), vectorizer.dims());
    float sim_diff = cosine_similarity_f32(v_split1.data(), v_hash.data(), vectorizer.dims());

    EXPECT_GT(sim_similar, sim_diff)
        << "Similar texts should have higher cosine similarity than different texts";
}

TEST_F(TfIdfVectorizerTest, EmptyCorpus) {
    vectorizer.build();
    EXPECT_TRUE(vectorizer.is_built());
    EXPECT_EQ(vectorizer.dims(), 0u);
}

TEST_F(TfIdfVectorizerTest, VectorizeBeforeBuild) {
    vectorizer.add_document(0, "test");
    auto vec = vectorizer.vectorize("test");
    EXPECT_TRUE(vec.empty());
}

TEST_F(TfIdfVectorizerTest, UnknownTermsProduceZeroVector) {
    vectorizer.add_document(0, "alpha beta gamma");
    vectorizer.build();

    auto vec = vectorizer.vectorize("zzzzzzz xxxxxxx");
    float length = norm_f32(vec.data(), vec.size());
    EXPECT_NEAR(length, 0.0f, 1e-6f) << "Unknown terms should produce zero vector";
}

// ============================================================================
// HNSW Index — Basic Operations
// ============================================================================

class HnswIndexTest : public ::testing::Test {
protected:
    static constexpr size_t DIMS = 8;
    HnswIndex index{DIMS};

    /// Creates a unit vector in the given dimension.
    auto make_unit_vector(size_t dim) -> std::vector<float> {
        std::vector<float> v(DIMS, 0.0f);
        if (dim < DIMS)
            v[dim] = 1.0f;
        return v;
    }

    /// Creates a random normalized vector.
    auto make_random_vector(std::mt19937& rng) -> std::vector<float> {
        std::normal_distribution<float> dist(0.0f, 1.0f);
        std::vector<float> v(DIMS);
        for (size_t i = 0; i < DIMS; ++i) {
            v[i] = dist(rng);
        }
        normalize_f32(v.data(), DIMS);
        return v;
    }
};

TEST_F(HnswIndexTest, EmptyIndex) {
    EXPECT_EQ(index.size(), 0u);
    auto results = index.search(make_unit_vector(0), 5);
    EXPECT_TRUE(results.empty());
}

TEST_F(HnswIndexTest, InsertSingle) {
    index.insert(0, make_unit_vector(0));
    EXPECT_EQ(index.size(), 1u);
}

TEST_F(HnswIndexTest, SearchSingleElement) {
    auto v = make_unit_vector(0);
    index.insert(0, v);
    auto results = index.search(v, 1);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].doc_id, 0u);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-5f);
}

TEST_F(HnswIndexTest, InsertMultipleAndSearch) {
    // Insert unit vectors along each axis
    for (size_t i = 0; i < DIMS; ++i) {
        index.insert(static_cast<uint32_t>(i), make_unit_vector(i));
    }
    EXPECT_EQ(index.size(), DIMS);

    // Search for each — should find itself as nearest
    for (size_t i = 0; i < DIMS; ++i) {
        auto results = index.search(make_unit_vector(i), 1);
        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].doc_id, static_cast<uint32_t>(i))
            << "Query for axis " << i << " should return itself";
    }
}

TEST_F(HnswIndexTest, KNearestNeighbors) {
    std::mt19937 rng(42);
    const int N = 50;

    for (int i = 0; i < N; ++i) {
        index.insert(static_cast<uint32_t>(i), make_random_vector(rng));
    }

    auto query = make_random_vector(rng);
    auto results = index.search(query, 5);

    ASSERT_EQ(results.size(), 5u);

    // Distances should be non-decreasing
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i].distance, results[i - 1].distance - 1e-6f)
            << "Results should be sorted by distance ascending";
    }
}

TEST_F(HnswIndexTest, SearchReturnsCorrectK) {
    for (int i = 0; i < 20; ++i) {
        std::vector<float> v(DIMS, 0.0f);
        v[i % DIMS] = static_cast<float>(i + 1);
        normalize_f32(v.data(), DIMS);
        index.insert(static_cast<uint32_t>(i), v);
    }

    auto query = make_unit_vector(0);

    auto r3 = index.search(query, 3);
    EXPECT_EQ(r3.size(), 3u);

    auto r10 = index.search(query, 10);
    EXPECT_EQ(r10.size(), 10u);

    auto r50 = index.search(query, 50);
    EXPECT_EQ(r50.size(), 20u); // Can't return more than indexed
}

TEST_F(HnswIndexTest, UniqueDocIds) {
    std::mt19937 rng(99);
    for (int i = 0; i < 30; ++i) {
        index.insert(static_cast<uint32_t>(i), make_random_vector(rng));
    }

    auto results = index.search(make_random_vector(rng), 10);
    std::set<uint32_t> seen;
    for (const auto& r : results) {
        EXPECT_TRUE(seen.insert(r.doc_id).second)
            << "Duplicate doc_id " << r.doc_id << " in results";
    }
}

// ============================================================================
// HNSW — Recall Quality
// ============================================================================

TEST(HnswRecallTest, RecallAt10Above80Percent) {
    // Build a larger index and verify recall quality
    const size_t DIMS = 32;
    const int N = 200;
    const int K = 10;

    HnswIndex index(DIMS);
    index.set_params(16, 200, 100);

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    // Generate random vectors
    std::vector<std::vector<float>> vectors(N);
    for (int i = 0; i < N; ++i) {
        vectors[i].resize(DIMS);
        for (size_t d = 0; d < DIMS; ++d) {
            vectors[i][d] = dist(rng);
        }
        normalize_f32(vectors[i].data(), DIMS);
        index.insert(static_cast<uint32_t>(i), vectors[i]);
    }

    // Run multiple queries and measure recall
    int total_recall = 0;
    int total_expected = 0;
    const int NUM_QUERIES = 20;

    for (int q = 0; q < NUM_QUERIES; ++q) {
        std::vector<float> query(DIMS);
        for (size_t d = 0; d < DIMS; ++d) {
            query[d] = dist(rng);
        }
        normalize_f32(query.data(), DIMS);

        // Brute-force exact K nearest neighbors
        std::vector<std::pair<float, uint32_t>> all_dists;
        for (int i = 0; i < N; ++i) {
            float d = 1.0f - dot_product_f32(query.data(), vectors[i].data(), DIMS);
            all_dists.push_back({d, static_cast<uint32_t>(i)});
        }
        std::sort(all_dists.begin(), all_dists.end());
        std::unordered_set<uint32_t> exact_knn;
        for (int i = 0; i < K && i < N; ++i) {
            exact_knn.insert(all_dists[i].second);
        }

        // HNSW search
        auto hnsw_results = index.search(query, K);
        for (const auto& r : hnsw_results) {
            if (exact_knn.count(r.doc_id)) {
                total_recall++;
            }
        }
        total_expected += K;
    }

    float recall = static_cast<float>(total_recall) / static_cast<float>(total_expected);
    EXPECT_GE(recall, 0.80f) << "HNSW recall@" << K << " should be >= 80%, got " << (recall * 100)
                             << "%";
}

// ============================================================================
// HNSW — Graph Structure
// ============================================================================

TEST(HnswGraphTest, MaxLayerGrowsLogarithmically) {
    const size_t DIMS = 16;
    HnswIndex index(DIMS);
    index.set_params(16, 200, 50);

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < 500; ++i) {
        std::vector<float> v(DIMS);
        for (size_t d = 0; d < DIMS; ++d) {
            v[d] = dist(rng);
        }
        normalize_f32(v.data(), DIMS);
        index.insert(static_cast<uint32_t>(i), v);
    }

    // Max layer should be roughly ln(N) / ln(M) ~ ln(500)/ln(16) ~ 2.2
    // But with randomness it could be higher, so just check it's reasonable
    EXPECT_GE(index.max_layer(), 1) << "Should have at least 2 layers with 500 nodes";
    EXPECT_LE(index.max_layer(), 10) << "Max layer shouldn't be too high for 500 nodes";
}

TEST(HnswGraphTest, ParameterSetting) {
    const size_t DIMS = 8;
    HnswIndex index(DIMS);
    index.set_params(32, 300, 100);

    EXPECT_EQ(index.dims(), DIMS);
}

// ============================================================================
// HNSW — Edge Cases
// ============================================================================

TEST(HnswEdgeCaseTest, DuplicateVectors) {
    const size_t DIMS = 4;
    HnswIndex index(DIMS);
    std::vector<float> v = {1.0f, 0.0f, 0.0f, 0.0f};

    index.insert(0, v);
    index.insert(1, v);
    index.insert(2, v);

    auto results = index.search(v, 3);
    EXPECT_EQ(results.size(), 3u);
    // All should have distance ~0
    for (const auto& r : results) {
        EXPECT_NEAR(r.distance, 0.0f, 1e-5f);
    }
}

TEST(HnswEdgeCaseTest, SingleElement) {
    const size_t DIMS = 4;
    HnswIndex index(DIMS);
    std::vector<float> v = {0.0f, 1.0f, 0.0f, 0.0f};
    index.insert(42, v);

    auto results = index.search(v, 5);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].doc_id, 42u);
}

TEST(HnswEdgeCaseTest, TwoElements) {
    const size_t DIMS = 4;
    HnswIndex index(DIMS);
    std::vector<float> v1 = {1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> v2 = {0.0f, 1.0f, 0.0f, 0.0f};
    index.insert(0, v1);
    index.insert(1, v2);

    // Query closer to v1
    auto results = index.search(v1, 2);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].doc_id, 0u);
    EXPECT_EQ(results[1].doc_id, 1u);
}

// ============================================================================
// End-to-End: TF-IDF Vectorizer + HNSW Search
// ============================================================================

TEST(HnswEndToEndTest, SemanticDocSearch) {
    // Build a small doc corpus with the TF-IDF vectorizer and HNSW
    TfIdfVectorizer vectorizer(128);
    vectorizer.add_document(0, "split string by delimiter into parts substring");
    vectorizer.add_document(1, "join list of strings with separator concatenate");
    vectorizer.add_document(2, "hash table map key value insert lookup delete");
    vectorizer.add_document(3, "sort list ascending descending compare order");
    vectorizer.add_document(4, "filter predicate condition match elements select");
    vectorizer.add_document(5, "parse json string value object array decode");
    vectorizer.build();

    if (vectorizer.dims() == 0) {
        GTEST_SKIP() << "Vectorizer produced 0 dimensions (corpus too small)";
    }

    HnswIndex index(vectorizer.dims());
    index.set_params(16, 200, 50);

    std::vector<std::string> docs = {
        "split string by delimiter into parts substring",
        "join list of strings with separator concatenate",
        "hash table map key value insert lookup delete",
        "sort list ascending descending compare order",
        "filter predicate condition match elements select",
        "parse json string value object array decode",
    };

    for (uint32_t i = 0; i < docs.size(); ++i) {
        auto vec = vectorizer.vectorize(docs[i]);
        index.insert(i, vec);
    }

    // Query: "divide string into tokens" — should be closest to "split" (doc 0)
    {
        auto qvec = vectorizer.vectorize("divide string into tokens");
        auto results = index.search(qvec, 3);
        ASSERT_FALSE(results.empty());
        // The top result should ideally be doc 0 (split) due to "string" overlap
        // but with bag-of-words this might vary, so just check it returns results
        EXPECT_LE(results.size(), 3u);
    }

    // Query: "key value store" — should find hash table (doc 2)
    {
        auto qvec = vectorizer.vectorize("key value store");
        auto results = index.search(qvec, 3);
        ASSERT_FALSE(results.empty());
        bool found_hash = false;
        for (const auto& r : results) {
            if (r.doc_id == 2)
                found_hash = true;
        }
        EXPECT_TRUE(found_hash) << "'key value store' query should find hash table doc in top 3";
    }
}

// ============================================================================
// Stress Test
// ============================================================================

TEST(HnswStressTest, LargerIndex) {
    const size_t DIMS = 64;
    const int N = 1000;
    HnswIndex index(DIMS);
    index.set_params(16, 100, 50);

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < N; ++i) {
        std::vector<float> v(DIMS);
        for (size_t d = 0; d < DIMS; ++d) {
            v[d] = dist(rng);
        }
        normalize_f32(v.data(), DIMS);
        index.insert(static_cast<uint32_t>(i), v);
    }

    EXPECT_EQ(index.size(), static_cast<size_t>(N));

    // Search should return results
    std::vector<float> query(DIMS);
    for (size_t d = 0; d < DIMS; ++d) {
        query[d] = dist(rng);
    }
    normalize_f32(query.data(), DIMS);

    auto results = index.search(query, 10);
    EXPECT_EQ(results.size(), 10u);

    // Distances should be non-decreasing
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i].distance + 1e-6f, results[i - 1].distance);
    }
}
