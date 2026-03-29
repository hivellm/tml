#pragma once

//! # SIMD-Optimized String Operations
//!
//! Fast string search, case conversion, trimming, hashing, and comparison
//! using SSE4.2 (PCMPISTRI) and SSE2 intrinsics with scalar fallbacks.
//!
//! All functions use runtime CPUID dispatch via simd_detect.hpp.
//! Thread-safe, no global state.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 3.1 String Search
// ============================================================================

/// Find first occurrence of needle in haystack.
/// Returns byte offset of match, or -1 if not found.
/// SSE4.2: PCMPISTRI with _SIDD_CMP_EQUAL_ORDERED for needle <= 16 bytes.
/// SSE2 fallback: PCMPEQB on first byte + memcmp confirmation.
int simd_str_find(const char* hay, int hay_len, const char* needle, int needle_len);

/// Find last occurrence of needle in haystack (reverse search).
/// Returns byte offset of last match, or -1 if not found.
int simd_str_rfind(const char* hay, int hay_len, const char* needle, int needle_len);

/// Check if haystack contains needle.
/// Returns 1 if found, 0 if not.
int simd_str_contains(const char* hay, int hay_len, const char* needle, int needle_len);

// ============================================================================
// 3.2 Case Conversion
// ============================================================================

/// Convert ASCII characters to uppercase in-place.
/// SSE2: range check [a-z], subtract 32 via masked AND.
/// Non-ASCII bytes are passed through unchanged.
void simd_to_upper(const char* src, char* dst, int len);

/// Convert ASCII characters to lowercase in-place.
/// SSE2: range check [A-Z], add 32 via masked OR.
/// Non-ASCII bytes are passed through unchanged.
void simd_to_lower(const char* src, char* dst, int len);

// ============================================================================
// 3.3 Whitespace Trim
// ============================================================================

/// Return index of first non-whitespace byte (scanning forward).
/// Whitespace: ' ', '\t', '\r', '\n'.
/// SSE2: PCMPEQB for each ws char, OR masks, MOVMASK + tzcnt.
/// Returns len if string is all whitespace.
int simd_trim_start(const char* s, int len);

/// Return index one past the last non-whitespace byte (scanning backward).
/// Returns 0 if string is all whitespace.
int simd_trim_end(const char* s, int len);

// ============================================================================
// 3.4 String Hashing
// ============================================================================

/// Hash a string using hardware CRC32C (SSE4.2) or FNV-1a fallback.
/// SSE4.2: _mm_crc32_u64 processing 8 bytes/cycle.
/// Fallback: FNV-1a (64-bit).
uint64_t simd_str_hash(const char* s, int len);

// ============================================================================
// 3.5 String Compare
// ============================================================================

/// Compare two strings for equality.
/// Fast path: length check + memcmp (which uses SIMD internally on most platforms).
/// Returns 1 if equal, 0 if not.
int simd_str_eq(const char* a, int a_len, const char* b, int b_len);

#ifdef __cplusplus
}
#endif
