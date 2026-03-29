#pragma once

//! # SIMD-Optimized Math Array Operations
//!
//! Provides SIMD-accelerated sum and dot product for integer and floating-point
//! arrays. Uses AVX2 where available with SSE2 and scalar fallbacks.
//!
//! ## Functions
//!
//! | Function | Description |
//! |----------|-------------|
//! | `simd_sum_i32` | Sum of int32_t array elements |
//! | `simd_sum_f64` | Sum of double array elements |
//! | `simd_dot_f64` | Dot product of two double arrays (delegates to search) |
//!
//! ## Design
//!
//! Runtime CPUID dispatch:
//! - AVX2: 8 int32s or 4 doubles per cycle
//! - SSE2: 4 int32s or 2 doubles per cycle
//! - Scalar: portable fallback

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Sum all elements of an int32_t array.
/// AVX2: _mm256_add_epi32 with horizontal reduction.
/// SSE2 fallback: _mm_add_epi32.
/// Returns 0 for null pointer or n <= 0.
int32_t simd_sum_i32(const int32_t* data, int n);

/// Sum all elements of a double array.
/// AVX2: _mm256_add_pd with horizontal reduction.
/// SSE2 fallback: _mm_add_pd.
/// Returns 0.0 for null pointer or n <= 0.
double simd_sum_f64(const double* data, int n);

/// Dot product of two double arrays.
/// Delegates to tml::search::dot_product_f64 (simd_distance.cpp).
/// This avoids code duplication — the search module already has an optimized
/// AVX2+FMA implementation with SSE2 fallback.
/// Returns 0.0 for null pointers or n <= 0.
double simd_dot_f64(const double* a, const double* b, int n);

#ifdef __cplusplus
}
#endif
