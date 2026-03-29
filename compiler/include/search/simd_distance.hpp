//! # SIMD-Optimized Distance Functions
//!
//! Provides vector distance and similarity functions for nearest-neighbor search.
//! Uses explicit AVX2+FMA intrinsics for maximum throughput with SSE2 and scalar
//! fallbacks selected at runtime via CPUID detection.
//!
//! ## Float (f32) Functions
//!
//! | Function | Description |
//! |----------|-------------|
//! | `dot_product_f32` | Inner product of two float vectors |
//! | `cosine_similarity_f32` | Cosine similarity (normalized dot product) |
//! | `euclidean_distance_f32` | L2 (Euclidean) distance |
//! | `l2_distance_squared_f32` | Squared L2 distance (avoids sqrt) |
//! | `normalize_f32` | L2-normalize a vector in place |
//! | `norm_f32` | L2 norm (magnitude) of a vector |
//!
//! ## Double (f64) Functions
//!
//! | Function | Description |
//! |----------|-------------|
//! | `dot_product_f64` | Inner product of two double vectors |
//! | `cosine_similarity_f64` | Cosine similarity for doubles |
//! | `euclidean_distance_f64` | L2 distance for doubles |
//! | `l2_distance_squared_f64` | Squared L2 distance for doubles |
//! | `normalize_f64` | L2-normalize a double vector in place |
//! | `norm_f64` | L2 norm of a double vector |
//!
//! ## Design
//!
//! All functions use runtime CPUID dispatch:
//! - AVX2+FMA: 8 floats or 4 doubles per cycle
//! - SSE2 fallback: 4 floats or 2 doubles per cycle
//! - Scalar fallback: portable, no SIMD
//!
//! All pointer parameters carry `__restrict` to enable further optimizations.

#pragma once

#include <cstddef>
#include <cstdint>

namespace tml::search {

// ============================================================================
// Float (f32) Distance Functions
// ============================================================================

/// Computes the dot product (inner product) of two float vectors.
/// AVX2+FMA: 8 floats/cycle. SSE2 fallback: 4 floats/cycle.
auto dot_product_f32(const float* __restrict a, const float* __restrict b, size_t dim) -> float;

/// Computes cosine similarity between two float vectors.
/// Uses three FMA accumulators (dot, norm_a, norm_b) in a single pass.
/// Returns value in [-1, 1].
auto cosine_similarity_f32(const float* __restrict a, const float* __restrict b, size_t dim)
    -> float;

/// Computes the Euclidean (L2) distance between two float vectors.
auto euclidean_distance_f32(const float* __restrict a, const float* __restrict b, size_t dim)
    -> float;

/// Computes squared Euclidean distance (avoids sqrt for comparisons).
/// AVX2: sub + fmadd for 8 floats/cycle.
auto l2_distance_squared_f32(const float* __restrict a, const float* __restrict b, size_t dim)
    -> float;

/// L2-normalizes a vector in place (makes it unit length).
/// AVX2: bulk scale with broadcast inverse.
void normalize_f32(float* __restrict vec, size_t dim);

/// Computes the L2 norm (magnitude) of a vector.
/// Uses self-dot-product via FMA.
auto norm_f32(const float* __restrict vec, size_t dim) -> float;

// ============================================================================
// Double (f64) Distance Functions
// ============================================================================

/// Computes the dot product of two double vectors.
/// AVX2+FMA: 4 doubles/cycle. SSE2 fallback: 2 doubles/cycle.
auto dot_product_f64(const double* __restrict a, const double* __restrict b, size_t dim) -> double;

/// Computes cosine similarity between two double vectors.
/// Three FMA accumulators in a single pass.
auto cosine_similarity_f64(const double* __restrict a, const double* __restrict b, size_t dim)
    -> double;

/// Computes the Euclidean (L2) distance between two double vectors.
auto euclidean_distance_f64(const double* __restrict a, const double* __restrict b, size_t dim)
    -> double;

/// Computes squared L2 distance for doubles.
auto l2_distance_squared_f64(const double* __restrict a, const double* __restrict b, size_t dim)
    -> double;

/// L2-normalizes a double vector in place.
void normalize_f64(double* __restrict vec, size_t dim);

/// Computes the L2 norm of a double vector.
auto norm_f64(const double* __restrict vec, size_t dim) -> double;

// ============================================================================
// Batch Distance (HNSW optimization)
// ============================================================================

/// Computes distances from a query to 4 candidate vectors simultaneously.
/// Interleaves loads to maximize throughput.
///
/// @param query Query vector
/// @param candidates Array of 4 pointers to candidate vectors
/// @param dim Number of dimensions
/// @param out_distances Output array of 4 distances (squared L2)
void batch_l2_squared_f32_x4(const float* __restrict query,
                             const float* const __restrict candidates[4], size_t dim,
                             float* __restrict out_distances);

} // namespace tml::search
