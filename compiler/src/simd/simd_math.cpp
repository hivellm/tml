//! # SIMD-Optimized Math Array Operations — Implementation
//!
//! Uses AVX2 and SSE2 intrinsics for fast array sum and dot product
//! with runtime CPUID dispatch. All functions have scalar fallbacks.
//!
//! Dispatch strategy:
//!   - AVX2 available → use 256-bit operations (8 i32 or 4 f64 per cycle)
//!   - SSE2 available → use 128-bit operations (4 i32 or 2 f64 per cycle)
//!   - Neither → scalar fallback

#include "simd/simd_math.h"

#include "search/simd_distance.hpp"
#include "simd/simd_detect.hpp"
#include "simd/simd_utils.h"

#include <cstring>

#if TML_SSE2
#include <emmintrin.h> // SSE2
#endif

#if TML_AVX2
#include <immintrin.h> // AVX2
#endif

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// --------------------------------------------------------------------------
// Scalar fallback implementations
// --------------------------------------------------------------------------

int32_t scalar_sum_i32(const int32_t* data, int n) {
    int32_t sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

double scalar_sum_f64(const double* data, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

// --------------------------------------------------------------------------
// SSE2 implementations
// --------------------------------------------------------------------------

#if TML_SSE2

int32_t sse2_sum_i32(const int32_t* data, int n) {
    int i = 0;
    __m128i acc = _mm_setzero_si128();

    // Process 4 int32s per iteration
    for (; i + 3 < n; i += 4) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        acc = _mm_add_epi32(acc, v);
    }

    // Horizontal sum of 4 int32s in __m128i
    // acc = [a0, a1, a2, a3]
    // Shuffle to get [a2, a3, a0, a1], add → [a0+a2, a1+a3, ...]
    __m128i hi64 = _mm_shuffle_epi32(acc, _MM_SHUFFLE(1, 0, 3, 2));
    __m128i sum2 = _mm_add_epi32(acc, hi64);
    // Shuffle to get [a1+a3, a0+a2, ...], add → [a0+a1+a2+a3, ...]
    __m128i hi32 = _mm_shuffle_epi32(sum2, _MM_SHUFFLE(2, 3, 0, 1));
    __m128i sum1 = _mm_add_epi32(sum2, hi32);
    int32_t sum = _mm_cvtsi128_si32(sum1);

    // Scalar tail
    for (; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

double sse2_sum_f64(const double* data, int n) {
    int i = 0;
    __m128d acc = _mm_setzero_pd();

    // Process 2 doubles per iteration
    for (; i + 1 < n; i += 2) {
        __m128d v = _mm_loadu_pd(data + i);
        acc = _mm_add_pd(acc, v);
    }

    // Horizontal sum of 2 doubles in __m128d
    __m128d hi = _mm_unpackhi_pd(acc, acc);
    __m128d sum_vec = _mm_add_sd(acc, hi);
    double sum = _mm_cvtsd_f64(sum_vec);

    // Scalar tail
    for (; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

#endif // TML_SSE2

// --------------------------------------------------------------------------
// AVX2 implementations
// --------------------------------------------------------------------------

#if TML_AVX2

int32_t avx2_sum_i32(const int32_t* data, int n) {
    int i = 0;
    __m256i acc = _mm256_setzero_si256();

    // Process 8 int32s per iteration
    for (; i + 7 < n; i += 8) {
        __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
        acc = _mm256_add_epi32(acc, v);
    }

    // Horizontal reduction: 256-bit → 128-bit → scalar
    __m128i hi = _mm256_extracti128_si256(acc, 1);
    __m128i lo = _mm256_castsi256_si128(acc);
    __m128i sum4 = _mm_add_epi32(lo, hi);

    // Reduce 4 → 1
    __m128i hi64 = _mm_shuffle_epi32(sum4, _MM_SHUFFLE(1, 0, 3, 2));
    __m128i sum2 = _mm_add_epi32(sum4, hi64);
    __m128i hi32 = _mm_shuffle_epi32(sum2, _MM_SHUFFLE(2, 3, 0, 1));
    __m128i sum1 = _mm_add_epi32(sum2, hi32);
    int32_t sum = _mm_cvtsi128_si32(sum1);

    // Process remaining 4-element chunks with SSE2
    for (; i + 3 < n; i += 4) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        __m128i s4 = _mm_add_epi32(_mm_set1_epi32(sum), v);
        // Re-reduce
        __m128i h64 = _mm_shuffle_epi32(s4, _MM_SHUFFLE(1, 0, 3, 2));
        __m128i s2 = _mm_add_epi32(s4, h64);
        __m128i h32 = _mm_shuffle_epi32(s2, _MM_SHUFFLE(2, 3, 0, 1));
        __m128i s1 = _mm_add_epi32(s2, h32);
        sum = _mm_cvtsi128_si32(s1);
    }

    // Scalar tail
    for (; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

double avx2_sum_f64(const double* data, int n) {
    int i = 0;
    __m256d acc = _mm256_setzero_pd();

    // Process 4 doubles per iteration
    for (; i + 3 < n; i += 4) {
        __m256d v = _mm256_loadu_pd(data + i);
        acc = _mm256_add_pd(acc, v);
    }

    // Horizontal reduction: 256-bit → 128-bit → scalar
    __m128d hi = _mm256_extractf128_pd(acc, 1);
    __m128d lo = _mm256_castpd256_pd128(acc);
    __m128d sum2 = _mm_add_pd(lo, hi);
    __m128d hi64 = _mm_unpackhi_pd(sum2, sum2);
    __m128d sum1 = _mm_add_sd(sum2, hi64);
    double sum = _mm_cvtsd_f64(sum1);

    // Process remaining 2-element chunks with SSE2
    for (; i + 1 < n; i += 2) {
        __m128d v = _mm_loadu_pd(data + i);
        __m128d s2 = _mm_add_pd(_mm_set1_pd(sum), v);
        __m128d h = _mm_unpackhi_pd(s2, s2);
        __m128d s1 = _mm_add_sd(s2, h);
        sum = _mm_cvtsd_f64(s1);
    }

    // Scalar tail
    for (; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

#endif // TML_AVX2

} // anonymous namespace

// ============================================================================
// Public API — runtime dispatch
// ============================================================================

extern "C" {

int32_t simd_sum_i32(const int32_t* data, int n) {
    if (data == nullptr || n <= 0)
        return 0;

#if TML_AVX2
    if (tml::simd::has_avx2()) {
        return avx2_sum_i32(data, n);
    }
#endif
#if TML_SSE2
    if (tml::simd::has_sse2()) {
        return sse2_sum_i32(data, n);
    }
#endif
    return scalar_sum_i32(data, n);
}

double simd_sum_f64(const double* data, int n) {
    if (data == nullptr || n <= 0)
        return 0.0;

#if TML_AVX2
    if (tml::simd::has_avx2()) {
        return avx2_sum_f64(data, n);
    }
#endif
#if TML_SSE2
    if (tml::simd::has_sse2()) {
        return sse2_sum_f64(data, n);
    }
#endif
    return scalar_sum_f64(data, n);
}

double simd_dot_f64(const double* a, const double* b, int n) {
    if (a == nullptr || b == nullptr || n <= 0)
        return 0.0;

    // Delegate to the existing optimized implementation in simd_distance.cpp
    return tml::search::dot_product_f64(a, b, static_cast<size_t>(n));
}

} // extern "C"
