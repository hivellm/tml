#pragma once

//! # SIMD Utility Macros and Alignment Helpers
//!
//! Portable definitions for SIMD feature detection at compile time,
//! forced inlining, alignment, and aligned memory allocation.
//!
//! Compile-time macros (TML_SSE2, TML_AVX2, etc.) reflect what the
//! compiler was told to target. Runtime detection (simd_detect.hpp)
//! tells you what the CPU actually supports.

#include <cstddef>
#include <cstdlib>

// ============================================================================
// Compile-time ISA detection macros
// ============================================================================

// x86-64 guarantees SSE2
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#define TML_SSE2 1
#else
#define TML_SSE2 0
#endif

#if defined(__SSE4_2__)
#define TML_SSE42 1
#else
#define TML_SSE42 0
#endif

#if defined(__AVX2__)
#define TML_AVX2 1
#else
#define TML_AVX2 0
#endif

#if defined(__AVX512F__)
#define TML_AVX512 1
#else
#define TML_AVX512 0
#endif

#if defined(__AES__)
#define TML_AESNI 1
#else
#define TML_AESNI 0
#endif

#if defined(__POPCNT__)
#define TML_POPCNT 1
#else
#define TML_POPCNT 0
#endif

#if defined(__FMA__)
#define TML_FMA 1
#else
#define TML_FMA 0
#endif

#if defined(__BMI__)
#define TML_BMI1 1
#else
#define TML_BMI1 0
#endif

#if defined(__BMI2__)
#define TML_BMI2 1
#else
#define TML_BMI2 0
#endif

// ARM NEON
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define TML_NEON 1
#else
#define TML_NEON 0
#endif

// ============================================================================
// Forced inlining
// ============================================================================

#ifdef _MSC_VER
#define SIMD_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define SIMD_INLINE inline __attribute__((always_inline))
#else
#define SIMD_INLINE inline
#endif

// ============================================================================
// Alignment
// ============================================================================

#define SIMD_ALIGN(N) alignas(N)

// Standard SIMD alignment values
#define SIMD_ALIGN_SSE SIMD_ALIGN(16)
#define SIMD_ALIGN_AVX SIMD_ALIGN(32)
#define SIMD_ALIGN_AVX512 SIMD_ALIGN(64)

// ============================================================================
// Aligned memory allocation
// ============================================================================

namespace tml::simd {

// Allocate `size` bytes with `alignment`-byte alignment.
// Returns nullptr on failure. Free with simd_aligned_free().
inline auto simd_aligned_alloc(size_t size, size_t alignment) -> void* {
#ifdef _MSC_VER
    return _aligned_malloc(size, alignment);
#else
    // C11 aligned_alloc requires size to be a multiple of alignment
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    return aligned_alloc(alignment, aligned_size);
#endif
}

inline void simd_aligned_free(void* ptr) {
#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

} // namespace tml::simd

// ============================================================================
// Target-specific includes (when compiling with appropriate flags)
// ============================================================================

#if TML_SSE2
#include <emmintrin.h> // SSE2
#endif

#if TML_SSE42
#include <nmmintrin.h> // SSE4.2
#endif

#if TML_AVX2
#include <immintrin.h> // AVX, AVX2, FMA
#endif

#if TML_AVX512
#include <immintrin.h> // AVX-512
#endif

#if TML_NEON
#include <arm_neon.h>
#endif
