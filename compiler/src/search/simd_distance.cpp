//! # SIMD-Optimized Distance Functions — Implementation
//!
//! All loops are written for auto-vectorization. The compiler will generate
//! SSE2/AVX2 (x86), NEON (ARM), or scalar code depending on the target.
//! No explicit intrinsics are used.

#include "search/simd_distance.hpp"

#include <cmath>

namespace tml::search {

auto dot_product_f32(const float* a, const float* b, size_t dim) -> float {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

auto cosine_similarity_f32(const float* a, const float* b, size_t dim) -> float {
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t i = 0; i < dim; ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denom < 1e-12f) {
        return 0.0f;
    }
    return dot / denom;
}

auto euclidean_distance_f32(const float* a, const float* b, size_t dim) -> float {
    return std::sqrt(l2_distance_squared_f32(a, b, dim));
}

auto l2_distance_squared_f32(const float* a, const float* b, size_t dim) -> float {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

void normalize_f32(float* vec, size_t dim) {
    float magnitude = norm_f32(vec, dim);
    if (magnitude < 1e-12f) {
        return;
    }
    float inv = 1.0f / magnitude;
    for (size_t i = 0; i < dim; ++i) {
        vec[i] *= inv;
    }
}

auto norm_f32(const float* vec, size_t dim) -> float {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        sum += vec[i] * vec[i];
    }
    return std::sqrt(sum);
}

} // namespace tml::search
