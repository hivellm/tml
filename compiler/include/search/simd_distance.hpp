//! # SIMD-Optimized Distance Functions
//!
//! Provides vector distance and similarity functions for nearest-neighbor search.
//! Uses auto-vectorizable loops following TML's portable SIMD philosophy —
//! the compiler auto-vectorizes to SSE2/AVX2 without explicit intrinsics.
//!
//! ## Functions
//!
//! | Function | Description |
//! |----------|-------------|
//! | `dot_product_f32` | Inner product of two float vectors |
//! | `cosine_similarity_f32` | Cosine similarity (normalized dot product) |
//! | `euclidean_distance_f32` | L2 (Euclidean) distance |
//! | `l2_distance_squared_f32` | Squared L2 distance (avoids sqrt) |
//! | `normalize_f32` | L2-normalize a vector in place |
//!
//! ## Design
//!
//! All functions operate on raw `float*` arrays with explicit dimension parameter.
//! Loops are written to be trivially auto-vectorizable by MSVC, GCC, and Clang.
//! No platform-specific intrinsics are used.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tml::search {

/// Computes the dot product (inner product) of two float vectors.
///
/// @param a First vector
/// @param b Second vector
/// @param dim Number of dimensions
/// @return Sum of element-wise products
auto dot_product_f32(const float* a, const float* b, size_t dim) -> float;

/// Computes cosine similarity between two float vectors.
///
/// Returns value in [-1, 1] where 1 means identical direction,
/// 0 means orthogonal, -1 means opposite direction.
///
/// @param a First vector
/// @param b Second vector
/// @param dim Number of dimensions
/// @return Cosine similarity score
auto cosine_similarity_f32(const float* a, const float* b, size_t dim) -> float;

/// Computes the Euclidean (L2) distance between two float vectors.
///
/// @param a First vector
/// @param b Second vector
/// @param dim Number of dimensions
/// @return L2 distance (always >= 0)
auto euclidean_distance_f32(const float* a, const float* b, size_t dim) -> float;

/// Computes squared Euclidean distance (avoids sqrt for comparisons).
///
/// When comparing distances, using squared distance avoids the sqrt and
/// preserves ordering. Use this for HNSW neighbor selection.
///
/// @param a First vector
/// @param b Second vector
/// @param dim Number of dimensions
/// @return Squared L2 distance (always >= 0)
auto l2_distance_squared_f32(const float* a, const float* b, size_t dim) -> float;

/// L2-normalizes a vector in place (makes it unit length).
///
/// After normalization, dot_product equals cosine_similarity.
/// If the vector has zero magnitude, it is left unchanged.
///
/// @param vec Vector to normalize (modified in place)
/// @param dim Number of dimensions
void normalize_f32(float* vec, size_t dim);

/// Computes the L2 norm (magnitude) of a vector.
///
/// @param vec Input vector
/// @param dim Number of dimensions
/// @return L2 norm (always >= 0)
auto norm_f32(const float* vec, size_t dim) -> float;

} // namespace tml::search
