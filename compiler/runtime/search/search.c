/**
 * @file search.c
 * @brief TML Runtime - Search Engine Functions
 *
 * Provides BM25 text search and HNSW vector nearest-neighbor search as
 * C functions callable from TML via FFI.
 *
 * ## Components
 *
 * - **BM25 Index**: Full-text search with TF-IDF scoring
 * - **HNSW Index**: Approximate nearest neighbor vector search
 * - **Vector Distance**: Dot product, cosine similarity, euclidean distance
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// Vector Distance Functions
// ============================================================================

/**
 * @brief Dot product of two float arrays.
 * Auto-vectorizable loop (SIMD-friendly).
 */
TML_EXPORT double search_dot_product(const double* a, const double* b, int64_t dim) {
    double sum = 0.0;
    for (int64_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Cosine similarity between two vectors.
 * Returns value in [-1, 1].
 */
TML_EXPORT double search_cosine_similarity(const double* a, const double* b, int64_t dim) {
    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;

    for (int64_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    double denom = sqrt(norm_a) * sqrt(norm_b);
    if (denom < 1e-12) {
        return 0.0;
    }
    return dot / denom;
}

/**
 * @brief Euclidean (L2) distance between two vectors.
 */
TML_EXPORT double search_euclidean_distance(const double* a, const double* b, int64_t dim) {
    double sum = 0.0;
    for (int64_t i = 0; i < dim; i++) {
        double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}

/**
 * @brief L2 norm (magnitude) of a vector.
 */
TML_EXPORT double search_norm(const double* v, int64_t dim) {
    double sum = 0.0;
    for (int64_t i = 0; i < dim; i++) {
        sum += v[i] * v[i];
    }
    return sqrt(sum);
}

/**
 * @brief Normalize a vector in place (makes it unit length).
 */
TML_EXPORT void search_normalize(double* v, int64_t dim) {
    double mag = search_norm(v, dim);
    if (mag < 1e-12)
        return;
    double inv = 1.0 / mag;
    for (int64_t i = 0; i < dim; i++) {
        v[i] *= inv;
    }
}
