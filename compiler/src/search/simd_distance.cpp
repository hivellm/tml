TML_MODULE("tools")

//! # SIMD-Optimized Distance Functions — Implementation
//!
//! Uses explicit AVX2+FMA intrinsics for maximum throughput with runtime
//! CPUID dispatch. SSE2 and scalar fallbacks for older CPUs.
//!
//! AVX2 path: 8 floats (f32) or 4 doubles (f64) per cycle
//! SSE2 path: 4 floats (f32) or 2 doubles (f64) per cycle
//! Scalar path: 1 element per cycle (portable fallback)

#include "search/simd_distance.hpp"

#include "simd/simd_detect.hpp"

#include <cmath>

#ifdef _MSC_VER
#include <immintrin.h>
#else
#include <immintrin.h>
#endif

namespace tml::search {

// ============================================================================
// Internal: Horizontal sum helpers
// ============================================================================

namespace detail {

#if defined(__AVX2__) || defined(_MSC_VER)

// Horizontal sum of 8 floats in a __m256
static inline auto hsum_ps_avx(__m256 v) -> float {
    // hi = [v4,v5,v6,v7], lo = [v0,v1,v2,v3]
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 sum4 = _mm_add_ps(lo, hi);     // [v0+v4, v1+v5, v2+v6, v3+v7]
    __m128 shuf = _mm_movehdup_ps(sum4);  // [v1+v5, v1+v5, v3+v7, v3+v7]
    __m128 sum2 = _mm_add_ss(sum4, shuf); // [v0+v4+v1+v5, ...]
    shuf = _mm_movehl_ps(shuf, sum2);     // [v2+v6+v3+v7, ...]
    __m128 sum1 = _mm_add_ss(sum2, shuf);
    return _mm_cvtss_f32(sum1);
}

// Horizontal sum of 4 doubles in a __m256d
static inline auto hsum_pd_avx(__m256d v) -> double {
    __m128d hi = _mm256_extractf128_pd(v, 1);
    __m128d lo = _mm256_castpd256_pd128(v);
    __m128d sum2 = _mm_add_pd(lo, hi);
    __m128d hi64 = _mm_unpackhi_pd(sum2, sum2);
    __m128d sum1 = _mm_add_sd(sum2, hi64);
    return _mm_cvtsd_f64(sum1);
}

#endif

// Horizontal sum of 4 floats in a __m128
static inline auto hsum_ps_sse(__m128 v) -> float {
    __m128 shuf = _mm_movehdup_ps(v);  // [v1, v1, v3, v3]
    __m128 sum2 = _mm_add_ps(v, shuf); // [v0+v1, _, v2+v3, _]
    shuf = _mm_movehl_ps(shuf, sum2);  // [v2+v3, ...]
    __m128 sum1 = _mm_add_ss(sum2, shuf);
    return _mm_cvtss_f32(sum1);
}

// Horizontal sum of 2 doubles in a __m128d
static inline auto hsum_pd_sse(__m128d v) -> double {
    __m128d hi = _mm_unpackhi_pd(v, v);
    __m128d sum = _mm_add_sd(v, hi);
    return _mm_cvtsd_f64(sum);
}

} // namespace detail

// ============================================================================
// dot_product_f32
// ============================================================================

auto dot_product_f32(const float* __restrict a, const float* __restrict b, size_t dim) -> float {
    float sum = 0.0f;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256 acc = _mm256_setzero_ps();
        for (; i + 7 < dim; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            acc = _mm256_fmadd_ps(va, vb, acc);
        }
        sum += detail::hsum_ps_avx(acc);
    } else
#endif
    {
        // SSE2 path — always available on x86-64
        __m128 acc = _mm_setzero_ps();
        for (; i + 3 < dim; i += 4) {
            __m128 va = _mm_loadu_ps(a + i);
            __m128 vb = _mm_loadu_ps(b + i);
            acc = _mm_add_ps(acc, _mm_mul_ps(va, vb));
        }
        sum += detail::hsum_ps_sse(acc);
    }

    // Scalar tail
    for (; i < dim; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// ============================================================================
// cosine_similarity_f32
// ============================================================================

auto cosine_similarity_f32(const float* __restrict a, const float* __restrict b, size_t dim)
    -> float {
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256 acc_dot = _mm256_setzero_ps();
        __m256 acc_na = _mm256_setzero_ps();
        __m256 acc_nb = _mm256_setzero_ps();

        for (; i + 7 < dim; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            acc_dot = _mm256_fmadd_ps(va, vb, acc_dot);
            acc_na = _mm256_fmadd_ps(va, va, acc_na);
            acc_nb = _mm256_fmadd_ps(vb, vb, acc_nb);
        }

        dot += detail::hsum_ps_avx(acc_dot);
        norm_a += detail::hsum_ps_avx(acc_na);
        norm_b += detail::hsum_ps_avx(acc_nb);
    } else
#endif
    {
        __m128 acc_dot = _mm_setzero_ps();
        __m128 acc_na = _mm_setzero_ps();
        __m128 acc_nb = _mm_setzero_ps();

        for (; i + 3 < dim; i += 4) {
            __m128 va = _mm_loadu_ps(a + i);
            __m128 vb = _mm_loadu_ps(b + i);
            acc_dot = _mm_add_ps(acc_dot, _mm_mul_ps(va, vb));
            acc_na = _mm_add_ps(acc_na, _mm_mul_ps(va, va));
            acc_nb = _mm_add_ps(acc_nb, _mm_mul_ps(vb, vb));
        }

        dot += detail::hsum_ps_sse(acc_dot);
        norm_a += detail::hsum_ps_sse(acc_na);
        norm_b += detail::hsum_ps_sse(acc_nb);
    }

    // Scalar tail
    for (; i < dim; ++i) {
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

// ============================================================================
// l2_distance_squared_f32
// ============================================================================

auto l2_distance_squared_f32(const float* __restrict a, const float* __restrict b, size_t dim)
    -> float {
    float sum = 0.0f;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256 acc = _mm256_setzero_ps();
        for (; i + 7 < dim; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 diff = _mm256_sub_ps(va, vb);
            acc = _mm256_fmadd_ps(diff, diff, acc);
        }
        sum += detail::hsum_ps_avx(acc);
    } else
#endif
    {
        __m128 acc = _mm_setzero_ps();
        for (; i + 3 < dim; i += 4) {
            __m128 va = _mm_loadu_ps(a + i);
            __m128 vb = _mm_loadu_ps(b + i);
            __m128 diff = _mm_sub_ps(va, vb);
            acc = _mm_add_ps(acc, _mm_mul_ps(diff, diff));
        }
        sum += detail::hsum_ps_sse(acc);
    }

    // Scalar tail
    for (; i < dim; ++i) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

// ============================================================================
// euclidean_distance_f32
// ============================================================================

auto euclidean_distance_f32(const float* __restrict a, const float* __restrict b, size_t dim)
    -> float {
    return std::sqrt(l2_distance_squared_f32(a, b, dim));
}

// ============================================================================
// normalize_f32
// ============================================================================

void normalize_f32(float* __restrict vec, size_t dim) {
    float magnitude = norm_f32(vec, dim);
    if (magnitude < 1e-12f) {
        return;
    }
    float inv = 1.0f / magnitude;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2()) {
        __m256 vinv = _mm256_set1_ps(inv);
        for (; i + 7 < dim; i += 8) {
            __m256 v = _mm256_loadu_ps(vec + i);
            v = _mm256_mul_ps(v, vinv);
            _mm256_storeu_ps(vec + i, v);
        }
    } else
#endif
    {
        __m128 vinv = _mm_set1_ps(inv);
        for (; i + 3 < dim; i += 4) {
            __m128 v = _mm_loadu_ps(vec + i);
            v = _mm_mul_ps(v, vinv);
            _mm_storeu_ps(vec + i, v);
        }
    }

    // Scalar tail
    for (; i < dim; ++i) {
        vec[i] *= inv;
    }
}

// ============================================================================
// norm_f32
// ============================================================================

auto norm_f32(const float* __restrict vec, size_t dim) -> float {
    float sum = 0.0f;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256 acc = _mm256_setzero_ps();
        for (; i + 7 < dim; i += 8) {
            __m256 v = _mm256_loadu_ps(vec + i);
            acc = _mm256_fmadd_ps(v, v, acc);
        }
        sum += detail::hsum_ps_avx(acc);
    } else
#endif
    {
        __m128 acc = _mm_setzero_ps();
        for (; i + 3 < dim; i += 4) {
            __m128 v = _mm_loadu_ps(vec + i);
            acc = _mm_add_ps(acc, _mm_mul_ps(v, v));
        }
        sum += detail::hsum_ps_sse(acc);
    }

    // Scalar tail
    for (; i < dim; ++i) {
        sum += vec[i] * vec[i];
    }
    return std::sqrt(sum);
}

// ============================================================================
// dot_product_f64
// ============================================================================

auto dot_product_f64(const double* __restrict a, const double* __restrict b, size_t dim) -> double {
    double sum = 0.0;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256d acc = _mm256_setzero_pd();
        for (; i + 3 < dim; i += 4) {
            __m256d va = _mm256_loadu_pd(a + i);
            __m256d vb = _mm256_loadu_pd(b + i);
            acc = _mm256_fmadd_pd(va, vb, acc);
        }
        sum += detail::hsum_pd_avx(acc);
    } else
#endif
    {
        __m128d acc = _mm_setzero_pd();
        for (; i + 1 < dim; i += 2) {
            __m128d va = _mm_loadu_pd(a + i);
            __m128d vb = _mm_loadu_pd(b + i);
            acc = _mm_add_pd(acc, _mm_mul_pd(va, vb));
        }
        sum += detail::hsum_pd_sse(acc);
    }

    // Scalar tail
    for (; i < dim; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

// ============================================================================
// cosine_similarity_f64
// ============================================================================

auto cosine_similarity_f64(const double* __restrict a, const double* __restrict b, size_t dim)
    -> double {
    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256d acc_dot = _mm256_setzero_pd();
        __m256d acc_na = _mm256_setzero_pd();
        __m256d acc_nb = _mm256_setzero_pd();

        for (; i + 3 < dim; i += 4) {
            __m256d va = _mm256_loadu_pd(a + i);
            __m256d vb = _mm256_loadu_pd(b + i);
            acc_dot = _mm256_fmadd_pd(va, vb, acc_dot);
            acc_na = _mm256_fmadd_pd(va, va, acc_na);
            acc_nb = _mm256_fmadd_pd(vb, vb, acc_nb);
        }

        dot += detail::hsum_pd_avx(acc_dot);
        norm_a += detail::hsum_pd_avx(acc_na);
        norm_b += detail::hsum_pd_avx(acc_nb);
    } else
#endif
    {
        __m128d acc_dot = _mm_setzero_pd();
        __m128d acc_na = _mm_setzero_pd();
        __m128d acc_nb = _mm_setzero_pd();

        for (; i + 1 < dim; i += 2) {
            __m128d va = _mm_loadu_pd(a + i);
            __m128d vb = _mm_loadu_pd(b + i);
            acc_dot = _mm_add_pd(acc_dot, _mm_mul_pd(va, vb));
            acc_na = _mm_add_pd(acc_na, _mm_mul_pd(va, va));
            acc_nb = _mm_add_pd(acc_nb, _mm_mul_pd(vb, vb));
        }

        dot += detail::hsum_pd_sse(acc_dot);
        norm_a += detail::hsum_pd_sse(acc_na);
        norm_b += detail::hsum_pd_sse(acc_nb);
    }

    // Scalar tail
    for (; i < dim; ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    double denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denom < 1e-24) {
        return 0.0;
    }
    return dot / denom;
}

// ============================================================================
// l2_distance_squared_f64
// ============================================================================

auto l2_distance_squared_f64(const double* __restrict a, const double* __restrict b, size_t dim)
    -> double {
    double sum = 0.0;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256d acc = _mm256_setzero_pd();
        for (; i + 3 < dim; i += 4) {
            __m256d va = _mm256_loadu_pd(a + i);
            __m256d vb = _mm256_loadu_pd(b + i);
            __m256d diff = _mm256_sub_pd(va, vb);
            acc = _mm256_fmadd_pd(diff, diff, acc);
        }
        sum += detail::hsum_pd_avx(acc);
    } else
#endif
    {
        __m128d acc = _mm_setzero_pd();
        for (; i + 1 < dim; i += 2) {
            __m128d va = _mm_loadu_pd(a + i);
            __m128d vb = _mm_loadu_pd(b + i);
            __m128d diff = _mm_sub_pd(va, vb);
            acc = _mm_add_pd(acc, _mm_mul_pd(diff, diff));
        }
        sum += detail::hsum_pd_sse(acc);
    }

    // Scalar tail
    for (; i < dim; ++i) {
        double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

// ============================================================================
// euclidean_distance_f64
// ============================================================================

auto euclidean_distance_f64(const double* __restrict a, const double* __restrict b, size_t dim)
    -> double {
    return std::sqrt(l2_distance_squared_f64(a, b, dim));
}

// ============================================================================
// normalize_f64
// ============================================================================

void normalize_f64(double* __restrict vec, size_t dim) {
    double magnitude = norm_f64(vec, dim);
    if (magnitude < 1e-24) {
        return;
    }
    double inv = 1.0 / magnitude;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2()) {
        __m256d vinv = _mm256_set1_pd(inv);
        for (; i + 3 < dim; i += 4) {
            __m256d v = _mm256_loadu_pd(vec + i);
            v = _mm256_mul_pd(v, vinv);
            _mm256_storeu_pd(vec + i, v);
        }
    } else
#endif
    {
        __m128d vinv = _mm_set1_pd(inv);
        for (; i + 1 < dim; i += 2) {
            __m128d v = _mm_loadu_pd(vec + i);
            v = _mm_mul_pd(v, vinv);
            _mm_storeu_pd(vec + i, v);
        }
    }

    // Scalar tail
    for (; i < dim; ++i) {
        vec[i] *= inv;
    }
}

// ============================================================================
// norm_f64
// ============================================================================

auto norm_f64(const double* __restrict vec, size_t dim) -> double {
    double sum = 0.0;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256d acc = _mm256_setzero_pd();
        for (; i + 3 < dim; i += 4) {
            __m256d v = _mm256_loadu_pd(vec + i);
            acc = _mm256_fmadd_pd(v, v, acc);
        }
        sum += detail::hsum_pd_avx(acc);
    } else
#endif
    {
        __m128d acc = _mm_setzero_pd();
        for (; i + 1 < dim; i += 2) {
            __m128d v = _mm_loadu_pd(vec + i);
            acc = _mm_add_pd(acc, _mm_mul_pd(v, v));
        }
        sum += detail::hsum_pd_sse(acc);
    }

    // Scalar tail
    for (; i < dim; ++i) {
        sum += vec[i] * vec[i];
    }
    return std::sqrt(sum);
}

// ============================================================================
// batch_l2_squared_f32_x4
// ============================================================================

void batch_l2_squared_f32_x4(const float* __restrict query,
                             const float* const __restrict candidates[4], size_t dim,
                             float* __restrict out_distances) {
    float sum0 = 0.0f, sum1 = 0.0f, sum2 = 0.0f, sum3 = 0.0f;
    size_t i = 0;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (tml::simd::has_avx2() && tml::simd::has_fma()) {
        __m256 acc0 = _mm256_setzero_ps();
        __m256 acc1 = _mm256_setzero_ps();
        __m256 acc2 = _mm256_setzero_ps();
        __m256 acc3 = _mm256_setzero_ps();

        for (; i + 7 < dim; i += 8) {
            __m256 vq = _mm256_loadu_ps(query + i);

            __m256 vc0 = _mm256_loadu_ps(candidates[0] + i);
            __m256 d0 = _mm256_sub_ps(vq, vc0);
            acc0 = _mm256_fmadd_ps(d0, d0, acc0);

            __m256 vc1 = _mm256_loadu_ps(candidates[1] + i);
            __m256 d1 = _mm256_sub_ps(vq, vc1);
            acc1 = _mm256_fmadd_ps(d1, d1, acc1);

            __m256 vc2 = _mm256_loadu_ps(candidates[2] + i);
            __m256 d2 = _mm256_sub_ps(vq, vc2);
            acc2 = _mm256_fmadd_ps(d2, d2, acc2);

            __m256 vc3 = _mm256_loadu_ps(candidates[3] + i);
            __m256 d3 = _mm256_sub_ps(vq, vc3);
            acc3 = _mm256_fmadd_ps(d3, d3, acc3);
        }

        sum0 += detail::hsum_ps_avx(acc0);
        sum1 += detail::hsum_ps_avx(acc1);
        sum2 += detail::hsum_ps_avx(acc2);
        sum3 += detail::hsum_ps_avx(acc3);
    } else
#endif
    {
        __m128 acc0 = _mm_setzero_ps();
        __m128 acc1 = _mm_setzero_ps();
        __m128 acc2 = _mm_setzero_ps();
        __m128 acc3 = _mm_setzero_ps();

        for (; i + 3 < dim; i += 4) {
            __m128 vq = _mm_loadu_ps(query + i);

            __m128 vc0 = _mm_loadu_ps(candidates[0] + i);
            __m128 d0 = _mm_sub_ps(vq, vc0);
            acc0 = _mm_add_ps(acc0, _mm_mul_ps(d0, d0));

            __m128 vc1 = _mm_loadu_ps(candidates[1] + i);
            __m128 d1 = _mm_sub_ps(vq, vc1);
            acc1 = _mm_add_ps(acc1, _mm_mul_ps(d1, d1));

            __m128 vc2 = _mm_loadu_ps(candidates[2] + i);
            __m128 d2 = _mm_sub_ps(vq, vc2);
            acc2 = _mm_add_ps(acc2, _mm_mul_ps(d2, d2));

            __m128 vc3 = _mm_loadu_ps(candidates[3] + i);
            __m128 d3 = _mm_sub_ps(vq, vc3);
            acc3 = _mm_add_ps(acc3, _mm_mul_ps(d3, d3));
        }

        sum0 += detail::hsum_ps_sse(acc0);
        sum1 += detail::hsum_ps_sse(acc1);
        sum2 += detail::hsum_ps_sse(acc2);
        sum3 += detail::hsum_ps_sse(acc3);
    }

    // Scalar tail
    for (; i < dim; ++i) {
        float dq = query[i];
        float d0 = dq - candidates[0][i];
        sum0 += d0 * d0;
        float d1 = dq - candidates[1][i];
        sum1 += d1 * d1;
        float d2 = dq - candidates[2][i];
        sum2 += d2 * d2;
        float d3 = dq - candidates[3][i];
        sum3 += d3 * d3;
    }

    out_distances[0] = sum0;
    out_distances[1] = sum1;
    out_distances[2] = sum2;
    out_distances[3] = sum3;
}

} // namespace tml::search
