//! # Search Engine Benchmarks
//!
//! Measures the performance of SIMD distance, BM25, and HNSW operations.
//! Uses GoogleTest with timing instrumentation to report throughput metrics
//! comparable to Rust benchmarks.
//!
//! Run with: tml_tests --gtest_filter="SearchBenchmark*"

#include "search/bm25_index.hpp"
#include "search/hnsw_index.hpp"
#include "search/simd_distance.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace tml::search;
using Clock = std::chrono::high_resolution_clock;

// ============================================================================
// SIMD Distance Benchmarks
// ============================================================================

TEST(SearchBenchmark, DotProduct_512dim_1M_ops) {
    const size_t DIMS = 512;
    const int ITERS = 1'000'000;

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    std::vector<float> a(DIMS), b(DIMS);
    for (size_t i = 0; i < DIMS; ++i) {
        a[i] = dist(rng);
        b[i] = dist(rng);
    }

    auto start = Clock::now();
    float sum = 0;
    for (int i = 0; i < ITERS; ++i) {
        sum += dot_product_f32(a.data(), b.data(), DIMS);
    }
    auto end = Clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double ns_per_op = static_cast<double>(ns) / ITERS;
    double ops_per_sec = 1e9 / ns_per_op;
    std::cerr << "\n[BENCH] dot_product_f32 (512-dim): " << std::fixed << std::setprecision(1)
              << ns_per_op << " ns/op, " << std::setprecision(0) << ops_per_sec / 1e6 << " Mops/s"
              << " (sum=" << sum << ")\n";

    EXPECT_LT(ns_per_op, 5000.0) << "dot_product should be < 5us per call";
}

TEST(SearchBenchmark, CosineSimilarity_512dim_1M_ops) {
    const size_t DIMS = 512;
    const int ITERS = 1'000'000;

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    std::vector<float> a(DIMS), b(DIMS);
    for (size_t i = 0; i < DIMS; ++i) {
        a[i] = dist(rng);
        b[i] = dist(rng);
    }

    auto start = Clock::now();
    float sum = 0;
    for (int i = 0; i < ITERS; ++i) {
        sum += cosine_similarity_f32(a.data(), b.data(), DIMS);
    }
    auto end = Clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double ns_per_op = static_cast<double>(ns) / ITERS;
    double ops_per_sec = 1e9 / ns_per_op;
    std::cerr << "\n[BENCH] cosine_similarity_f32 (512-dim): " << std::fixed << std::setprecision(1)
              << ns_per_op << " ns/op, " << std::setprecision(0) << ops_per_sec / 1e6 << " Mops/s"
              << " (sum=" << sum << ")\n";

    EXPECT_LT(ns_per_op, 10000.0) << "cosine_similarity should be < 10us per call";
}

// ============================================================================
// BM25 Benchmarks
// ============================================================================

TEST(SearchBenchmark, BM25_Index_1000docs) {
    BM25Index index;

    std::mt19937 rng(42);
    std::vector<std::string> words = {
        "split",   "join",   "hash",   "map",     "list",     "sort",    "filter", "reduce",
        "parse",   "format", "encode", "decode",  "compress", "encrypt", "sign",   "verify",
        "connect", "listen", "read",   "write",   "open",     "close",   "create", "delete",
        "update",  "insert", "remove", "find",    "search",   "compare", "equal",  "clone",
        "copy",    "move",   "swap",   "reverse", "string",   "integer", "float",  "boolean",
        "array",   "slice",  "vector", "queue",   "stack",    "tree",    "graph",  "node",
        "edge",    "path",
    };

    // Build 1000 documents with random word combinations
    for (int i = 0; i < 1000; ++i) {
        std::string name = words[rng() % words.size()];
        std::string sig = "pub func " + name + "(";
        std::string doc;
        for (int w = 0; w < 20; ++w) {
            if (w > 0)
                doc += " ";
            doc += words[rng() % words.size()];
        }
        std::string path = "mod::" + words[rng() % words.size()];
        index.add_document(static_cast<uint32_t>(i), name, sig, doc, path);
    }

    // Benchmark: index build time
    auto start = Clock::now();
    index.build();
    auto end = Clock::now();
    auto build_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cerr << "\n[BENCH] BM25 index build (1000 docs): " << build_us << " us\n";

    EXPECT_LT(build_us, 50000) << "BM25 index build should be < 50ms for 1000 docs";

    // Benchmark: search queries
    const int QUERIES = 10'000;
    start = Clock::now();
    for (int q = 0; q < QUERIES; ++q) {
        std::string query = words[q % words.size()] + " " + words[(q + 7) % words.size()];
        auto results = index.search(query, 10);
        (void)results;
    }
    end = Clock::now();
    auto query_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double us_per_query = static_cast<double>(query_us) / QUERIES;
    double qps = 1e6 / us_per_query;
    std::cerr << "[BENCH] BM25 search (1000 docs, 10k queries): " << std::fixed
              << std::setprecision(1) << us_per_query << " us/query, " << std::setprecision(0)
              << qps << " QPS\n";

    EXPECT_LT(us_per_query, 1000.0) << "BM25 search should be < 1ms per query for 1000 docs";
}

// ============================================================================
// HNSW Benchmarks
// ============================================================================

TEST(SearchBenchmark, HNSW_Build_1000vectors_64dim) {
    const size_t DIMS = 64;
    const int N = 1000;

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    // Generate vectors
    std::vector<std::vector<float>> vectors(N);
    for (int i = 0; i < N; ++i) {
        vectors[i].resize(DIMS);
        for (size_t d = 0; d < DIMS; ++d) {
            vectors[i][d] = dist(rng);
        }
        normalize_f32(vectors[i].data(), DIMS);
    }

    // Benchmark: index build time
    HnswIndex index(DIMS);
    index.set_params(16, 200, 50);

    auto start = Clock::now();
    for (int i = 0; i < N; ++i) {
        index.insert(static_cast<uint32_t>(i), vectors[i]);
    }
    auto end = Clock::now();
    auto build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cerr << "\n[BENCH] HNSW build (1000 vectors, 64-dim, M=16): " << build_ms << " ms\n";

    EXPECT_LT(build_ms, 5000) << "HNSW build should be < 5s for 1000 64-dim vectors";

    // Benchmark: search queries
    const int QUERIES = 1000;
    start = Clock::now();
    for (int q = 0; q < QUERIES; ++q) {
        std::vector<float> query(DIMS);
        for (size_t d = 0; d < DIMS; ++d) {
            query[d] = dist(rng);
        }
        normalize_f32(query.data(), DIMS);
        auto results = index.search(query, 10);
        (void)results;
    }
    end = Clock::now();
    auto query_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double us_per_query = static_cast<double>(query_us) / QUERIES;
    double qps = 1e6 / us_per_query;
    std::cerr << "[BENCH] HNSW search (1000 vectors, 64-dim, k=10): " << std::fixed
              << std::setprecision(1) << us_per_query << " us/query, " << std::setprecision(0)
              << qps << " QPS\n";

    EXPECT_LT(us_per_query, 10000.0) << "HNSW search should be < 10ms per query";
}

TEST(SearchBenchmark, HNSW_Build_5000vectors_128dim) {
    const size_t DIMS = 128;
    const int N = 5000;

    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    std::vector<std::vector<float>> vectors(N);
    for (int i = 0; i < N; ++i) {
        vectors[i].resize(DIMS);
        for (size_t d = 0; d < DIMS; ++d) {
            vectors[i][d] = dist(rng);
        }
        normalize_f32(vectors[i].data(), DIMS);
    }

    HnswIndex index(DIMS);
    index.set_params(16, 100, 50); // Reduced efConstruction for speed

    auto start = Clock::now();
    for (int i = 0; i < N; ++i) {
        index.insert(static_cast<uint32_t>(i), vectors[i]);
    }
    auto end = Clock::now();
    auto build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cerr << "\n[BENCH] HNSW build (5000 vectors, 128-dim, M=16): " << build_ms << " ms\n";

    // Benchmark: recall + latency
    int correct = 0;
    int total = 0;
    const int QUERIES = 100;

    auto query_start = Clock::now();
    for (int q = 0; q < QUERIES; ++q) {
        std::vector<float> query(DIMS);
        for (size_t d = 0; d < DIMS; ++d) {
            query[d] = dist(rng);
        }
        normalize_f32(query.data(), DIMS);

        // Brute force top-10
        std::vector<std::pair<float, int>> brute;
        for (int i = 0; i < N; ++i) {
            float d2 = 1.0f - dot_product_f32(query.data(), vectors[i].data(), DIMS);
            brute.push_back({d2, i});
        }
        std::sort(brute.begin(), brute.end());
        std::set<int> exact_top10;
        for (int i = 0; i < 10; ++i)
            exact_top10.insert(brute[i].second);

        auto hnsw_results = index.search(query, 10);
        for (const auto& r : hnsw_results) {
            if (exact_top10.count(r.doc_id))
                correct++;
        }
        total += 10;
    }
    auto query_end = Clock::now();
    auto total_query_us =
        std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_start).count();

    float recall = static_cast<float>(correct) / total;
    double us_per_q = static_cast<double>(total_query_us) / QUERIES;

    std::cerr << "[BENCH] HNSW search (5000 vectors, 128-dim, k=10): " << std::fixed
              << std::setprecision(1) << us_per_q << " us/query"
              << ", recall@10=" << std::setprecision(2) << (recall * 100) << "%\n";

    EXPECT_GE(recall, 0.75f) << "Recall@10 should be >= 75%";
}

// ============================================================================
// End-to-End: TF-IDF + HNSW Pipeline
// ============================================================================

TEST(SearchBenchmark, EndToEnd_TfIdf_HNSW_500docs) {
    // Simulate a realistic documentation corpus
    std::vector<std::string> docs;
    for (int i = 0; i < 500; ++i) {
        std::string doc = "function_" + std::to_string(i) + " ";
        doc += "implements a method that processes data with parameters ";
        doc += "returns output value type integer string boolean ";
        if (i % 3 == 0)
            doc += "hash map table insert lookup delete find ";
        if (i % 5 == 0)
            doc += "sort compare order ascending descending ";
        if (i % 7 == 0)
            doc += "parse json xml format encode decode serialize ";
        if (i % 11 == 0)
            doc += "network socket connect listen accept read write ";
        docs.push_back(doc);
    }

    // Build TF-IDF vectorizer
    TfIdfVectorizer vectorizer(256);
    auto vec_start = Clock::now();
    for (uint32_t i = 0; i < docs.size(); ++i) {
        vectorizer.add_document(i, docs[i]);
    }
    vectorizer.build();
    auto vec_end = Clock::now();
    auto vec_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(vec_end - vec_start).count();

    std::cerr << "\n[BENCH] TF-IDF vectorizer build (500 docs, " << vectorizer.dims()
              << "-dim): " << vec_ms << " ms\n";

    // Build HNSW index
    HnswIndex hnsw(vectorizer.dims());
    hnsw.set_params(16, 100, 50);

    auto hnsw_start = Clock::now();
    for (uint32_t i = 0; i < docs.size(); ++i) {
        auto vec = vectorizer.vectorize(docs[i]);
        hnsw.insert(i, vec);
    }
    auto hnsw_end = Clock::now();
    auto hnsw_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(hnsw_end - hnsw_start).count();

    std::cerr << "[BENCH] HNSW insert (500 vectors, " << vectorizer.dims() << "-dim): " << hnsw_ms
              << " ms\n";

    // Benchmark queries
    std::vector<std::string> queries = {
        "hash table lookup",      "sort ascending order",  "parse json format",
        "network socket connect", "process data function",
    };

    auto q_start = Clock::now();
    for (int rep = 0; rep < 1000; ++rep) {
        for (const auto& q : queries) {
            auto qvec = vectorizer.vectorize(q);
            auto results = hnsw.search(qvec, 10);
            (void)results;
        }
    }
    auto q_end = Clock::now();
    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(q_end - q_start).count();

    double us_per_q = static_cast<double>(total_us) / (1000 * queries.size());
    std::cerr << "[BENCH] End-to-end query (vectorize + HNSW search, 500 docs): " << std::fixed
              << std::setprecision(1) << us_per_q << " us/query\n";

    EXPECT_LT(us_per_q, 5000.0) << "End-to-end query should be < 5ms";

    // Total pipeline time
    auto total_ms = vec_ms + hnsw_ms;
    std::cerr << "[BENCH] Total pipeline build (500 docs): " << total_ms << " ms\n";
    EXPECT_LT(total_ms, 5000) << "Full pipeline should build in < 5s for 500 docs";
}
