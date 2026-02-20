/**
 * @file search_engine.cpp
 * @brief TML Runtime - Search Engine FFI Layer
 *
 * Provides C API wrappers around the C++ BM25Index, HnswIndex,
 * and TfIdfVectorizer classes for use from TML via FFI.
 *
 * Uses opaque void* handles following the same pattern as crypto runtime.
 */

#include "search/bm25_index.hpp"
#include "search/hnsw_index.hpp"

#include <cstdlib>
#include <cstring>
#include <new>

#ifdef _WIN32
#define TML_EXPORT extern "C" __declspec(dllexport)
#else
#define TML_EXPORT extern "C" __attribute__((visibility("default")))
#endif

using namespace tml::search;

// F32 Distance Functions — removed in Phase 35 (migrated to pure TML in distance.tml)

// ============================================================================
// BM25 Index - Opaque Handle API
// ============================================================================

TML_EXPORT void* bm25_create() {
    auto* index = new (std::nothrow) BM25Index();
    return static_cast<void*>(index);
}

TML_EXPORT void bm25_destroy(void* handle) {
    if (handle) {
        delete static_cast<BM25Index*>(handle);
    }
}

TML_EXPORT void bm25_set_k1(void* handle, float k1) {
    if (handle) {
        static_cast<BM25Index*>(handle)->k1 = k1;
    }
}

TML_EXPORT void bm25_set_b(void* handle, float b) {
    if (handle) {
        static_cast<BM25Index*>(handle)->b = b;
    }
}

TML_EXPORT void bm25_set_name_boost(void* handle, float boost) {
    if (handle) {
        static_cast<BM25Index*>(handle)->name_boost = boost;
    }
}

TML_EXPORT void bm25_set_signature_boost(void* handle, float boost) {
    if (handle) {
        static_cast<BM25Index*>(handle)->signature_boost = boost;
    }
}

TML_EXPORT void bm25_set_doc_boost(void* handle, float boost) {
    if (handle) {
        static_cast<BM25Index*>(handle)->doc_boost = boost;
    }
}

TML_EXPORT void bm25_set_path_boost(void* handle, float boost) {
    if (handle) {
        static_cast<BM25Index*>(handle)->path_boost = boost;
    }
}

TML_EXPORT void bm25_add_document(void* handle, uint32_t doc_id, const char* name,
                                  const char* signature, const char* doc_text, const char* path) {
    if (!handle)
        return;
    auto* index = static_cast<BM25Index*>(handle);
    index->add_document(doc_id, name ? name : "", signature ? signature : "",
                        doc_text ? doc_text : "", path ? path : "");
}

TML_EXPORT void bm25_add_text(void* handle, uint32_t doc_id, const char* text) {
    if (!handle || !text)
        return;
    auto* index = static_cast<BM25Index*>(handle);
    // Use text as the doc_text field with empty name/sig/path
    index->add_document(doc_id, "", "", text, "");
}

TML_EXPORT void bm25_build(void* handle) {
    if (handle) {
        static_cast<BM25Index*>(handle)->build();
    }
}

// Search results are returned as packed arrays: [doc_id0, score0, doc_id1, score1, ...]
// The caller passes a pre-allocated buffer. Returns actual result count.
TML_EXPORT int32_t bm25_search(void* handle, const char* query, int32_t limit,
                               uint32_t* out_doc_ids, float* out_scores) {
    if (!handle || !query || limit <= 0)
        return 0;
    auto* index = static_cast<BM25Index*>(handle);
    auto results = index->search(query, static_cast<size_t>(limit));

    int32_t count = static_cast<int32_t>(results.size());
    for (int32_t i = 0; i < count; i++) {
        if (out_doc_ids)
            out_doc_ids[i] = results[i].doc_id;
        if (out_scores)
            out_scores[i] = results[i].score;
    }
    return count;
}

TML_EXPORT int32_t bm25_size(void* handle) {
    if (!handle)
        return 0;
    return static_cast<int32_t>(static_cast<BM25Index*>(handle)->size());
}

TML_EXPORT float bm25_idf(void* handle, const char* term) {
    if (!handle || !term)
        return 0.0f;
    return static_cast<BM25Index*>(handle)->idf(term);
}

// ============================================================================
// HNSW Index - Opaque Handle API
// ============================================================================

TML_EXPORT void* hnsw_create(int32_t dims) {
    if (dims <= 0)
        return nullptr;
    auto* index = new (std::nothrow) HnswIndex(static_cast<size_t>(dims));
    return static_cast<void*>(index);
}

TML_EXPORT void hnsw_destroy(void* handle) {
    if (handle) {
        delete static_cast<HnswIndex*>(handle);
    }
}

TML_EXPORT void hnsw_set_params(void* handle, int32_t m, int32_t ef_construction,
                                int32_t ef_search) {
    if (handle) {
        static_cast<HnswIndex*>(handle)->set_params(m, ef_construction, ef_search);
    }
}

TML_EXPORT void hnsw_insert(void* handle, uint32_t doc_id, const float* embedding) {
    if (!handle || !embedding)
        return;
    auto* index = static_cast<HnswIndex*>(handle);
    size_t dims = index->dims();
    std::vector<float> vec(embedding, embedding + dims);
    index->insert(doc_id, vec);
}

// Search: returns count, fills pre-allocated output buffers
TML_EXPORT int32_t hnsw_search(void* handle, const float* query, int32_t k, uint32_t* out_doc_ids,
                               float* out_distances) {
    if (!handle || !query || k <= 0)
        return 0;
    auto* index = static_cast<HnswIndex*>(handle);
    size_t dims = index->dims();
    std::vector<float> qvec(query, query + dims);
    auto results = index->search(qvec, static_cast<size_t>(k));

    int32_t count = static_cast<int32_t>(results.size());
    for (int32_t i = 0; i < count; i++) {
        if (out_doc_ids)
            out_doc_ids[i] = results[i].doc_id;
        if (out_distances)
            out_distances[i] = results[i].distance;
    }
    return count;
}

TML_EXPORT int32_t hnsw_size(void* handle) {
    if (!handle)
        return 0;
    return static_cast<int32_t>(static_cast<HnswIndex*>(handle)->size());
}

TML_EXPORT int32_t hnsw_dims(void* handle) {
    if (!handle)
        return 0;
    return static_cast<int32_t>(static_cast<HnswIndex*>(handle)->dims());
}

TML_EXPORT int32_t hnsw_max_layer(void* handle) {
    if (!handle)
        return -1;
    return static_cast<int32_t>(static_cast<HnswIndex*>(handle)->max_layer());
}

// ============================================================================
// TF-IDF Vectorizer - Opaque Handle API
// ============================================================================

TML_EXPORT void* tfidf_create(int32_t max_dims) {
    if (max_dims <= 0)
        max_dims = 512;
    auto* vec = new (std::nothrow) TfIdfVectorizer(static_cast<size_t>(max_dims));
    return static_cast<void*>(vec);
}

TML_EXPORT void tfidf_destroy(void* handle) {
    if (handle) {
        delete static_cast<TfIdfVectorizer*>(handle);
    }
}

TML_EXPORT void tfidf_add_document(void* handle, uint32_t doc_id, const char* text) {
    if (!handle || !text)
        return;
    static_cast<TfIdfVectorizer*>(handle)->add_document(doc_id, text);
}

TML_EXPORT void tfidf_build(void* handle) {
    if (handle) {
        static_cast<TfIdfVectorizer*>(handle)->build();
    }
}

// Vectorize text into a pre-allocated float buffer. Returns actual dims.
TML_EXPORT int32_t tfidf_vectorize(void* handle, const char* text, float* out_vec) {
    if (!handle || !text || !out_vec)
        return 0;
    auto* vectorizer = static_cast<TfIdfVectorizer*>(handle);
    auto vec = vectorizer->vectorize(text);
    int32_t dims = static_cast<int32_t>(vec.size());
    std::memcpy(out_vec, vec.data(), dims * sizeof(float));
    return dims;
}

TML_EXPORT int32_t tfidf_dims(void* handle) {
    if (!handle)
        return 0;
    return static_cast<int32_t>(static_cast<TfIdfVectorizer*>(handle)->dims());
}

TML_EXPORT int32_t tfidf_is_built(void* handle) {
    if (!handle)
        return 0;
    return static_cast<TfIdfVectorizer*>(handle)->is_built() ? 1 : 0;
}
