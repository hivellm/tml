//! # SIMD-Optimized String Operations — Implementation
//!
//! Uses SSE4.2 (PCMPISTRI/CRC32) and SSE2 intrinsics for fast string
//! operations with runtime CPUID dispatch. All functions have scalar
//! fallbacks for non-x86 platforms.
//!
//! Dispatch strategy:
//!   - SSE4.2 available → use PCMPISTRI for search, CRC32C for hash
//!   - SSE2 available → use PCMPEQB + MOVMASK for search/case/trim
//!   - Neither → scalar fallback

#include "simd/simd_string.h"

#include "simd/simd_detect.hpp"
#include "simd/simd_utils.h"

#include <cstring>

#if TML_SSE2
#include <emmintrin.h> // SSE2
#endif

#if TML_SSE42
#include <nmmintrin.h> // SSE4.2 (PCMPISTRI, CRC32)
#endif

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

// --------------------------------------------------------------------------
// Scalar fallback implementations
// --------------------------------------------------------------------------

int scalar_str_find(const char* hay, int hay_len, const char* needle, int needle_len) {
    if (needle_len == 0)
        return 0;
    if (needle_len > hay_len)
        return -1;

    const int limit = hay_len - needle_len;
    const char first = needle[0];

    for (int i = 0; i <= limit; ++i) {
        if (hay[i] == first) {
            if (needle_len == 1 ||
                memcmp(hay + i + 1, needle + 1, static_cast<size_t>(needle_len - 1)) == 0) {
                return i;
            }
        }
    }
    return -1;
}

int scalar_str_rfind(const char* hay, int hay_len, const char* needle, int needle_len) {
    if (needle_len == 0)
        return hay_len;
    if (needle_len > hay_len)
        return -1;

    const char first = needle[0];

    for (int i = hay_len - needle_len; i >= 0; --i) {
        if (hay[i] == first) {
            if (needle_len == 1 ||
                memcmp(hay + i + 1, needle + 1, static_cast<size_t>(needle_len - 1)) == 0) {
                return i;
            }
        }
    }
    return -1;
}

void scalar_to_upper(const char* src, char* dst, int len) {
    for (int i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        if (c >= 'a' && c <= 'z') {
            dst[i] = static_cast<char>(c - 32);
        } else {
            dst[i] = static_cast<char>(c);
        }
    }
}

void scalar_to_lower(const char* src, char* dst, int len) {
    for (int i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        if (c >= 'A' && c <= 'Z') {
            dst[i] = static_cast<char>(c + 32);
        } else {
            dst[i] = static_cast<char>(c);
        }
    }
}

int scalar_trim_start(const char* s, int len) {
    for (int i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            return i;
        }
    }
    return len;
}

int scalar_trim_end(const char* s, int len) {
    for (int i = len - 1; i >= 0; --i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            return i + 1;
        }
    }
    return 0;
}

uint64_t scalar_str_hash(const char* s, int len) {
    // FNV-1a 64-bit
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (int i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(s[i]));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

// --------------------------------------------------------------------------
// SSE2 implementations
// --------------------------------------------------------------------------

#if TML_SSE2

int sse2_str_find(const char* hay, int hay_len, const char* needle, int needle_len) {
    if (needle_len == 0)
        return 0;
    if (needle_len > hay_len)
        return -1;

    const int limit = hay_len - needle_len;
    const __m128i first_byte = _mm_set1_epi8(needle[0]);
    int i = 0;

    // Process 16 bytes at a time: find candidate positions where first byte matches
    for (; i + 15 <= limit; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(hay + i));
        __m128i cmp = _mm_cmpeq_epi8(chunk, first_byte);
        int mask = _mm_movemask_epi8(cmp);

        while (mask != 0) {
            // Find lowest set bit = position of matching first byte
#ifdef _MSC_VER
            unsigned long bit;
            _BitScanForward(&bit, static_cast<unsigned long>(mask));
            int pos = static_cast<int>(bit);
#else
            int pos = __builtin_ctz(mask);
#endif
            int candidate = i + pos;
            if (candidate > limit)
                break;

            if (needle_len == 1 ||
                memcmp(hay + candidate + 1, needle + 1, static_cast<size_t>(needle_len - 1)) == 0) {
                return candidate;
            }
            mask &= mask - 1; // clear lowest set bit
        }
    }

    // Scalar tail
    for (; i <= limit; ++i) {
        if (hay[i] == needle[0]) {
            if (needle_len == 1 ||
                memcmp(hay + i + 1, needle + 1, static_cast<size_t>(needle_len - 1)) == 0) {
                return i;
            }
        }
    }
    return -1;
}

int sse2_str_rfind(const char* hay, int hay_len, const char* needle, int needle_len) {
    if (needle_len == 0)
        return hay_len;
    if (needle_len > hay_len)
        return -1;

    const int limit = hay_len - needle_len;
    const __m128i first_byte = _mm_set1_epi8(needle[0]);

    // Process 16-byte chunks from end to start
    // Start at the highest 16-byte-aligned position <= limit
    int chunk_start = limit - 15;
    if (chunk_start < 0)
        chunk_start = 0;

    // Align chunk_start down to process from the right
    for (int cs = (limit > 15) ? (limit - 15) : 0; cs >= 0; cs -= 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(hay + cs));
        __m128i cmp = _mm_cmpeq_epi8(chunk, first_byte);
        int mask = _mm_movemask_epi8(cmp);

        // Mask out bits beyond limit
        int max_valid = limit - cs;
        if (max_valid < 15) {
            mask &= (1 << (max_valid + 1)) - 1;
        }

        // Check candidates from highest (rightmost) to lowest
        while (mask != 0) {
#ifdef _MSC_VER
            unsigned long bit;
            _BitScanReverse(&bit, static_cast<unsigned long>(mask));
            int pos = static_cast<int>(bit);
#else
            int pos = 31 - __builtin_clz(mask);
#endif
            int candidate = cs + pos;
            if (needle_len == 1 ||
                memcmp(hay + candidate + 1, needle + 1, static_cast<size_t>(needle_len - 1)) == 0) {
                return candidate;
            }
            mask &= ~(1 << pos); // clear highest bit
        }
    }

    // Scalar tail for bytes before the first chunk
    return -1;
}

void sse2_to_upper(const char* src, char* dst, int len) {
    int i = 0;
    const __m128i lower_a = _mm_set1_epi8('a');
    const __m128i diff = _mm_set1_epi8(32); // 'a' - 'A'

    for (; i + 16 <= len; i += 16) {
        __m128i chars = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));

        // Range check [a, z] using unsigned compare via sign-bit flip trick:
        // offset = chars - 'a' (wrapping), then check if offset < 26 (unsigned)
        // SSE2 has only signed compare, so flip sign bit on both sides
        __m128i offset = _mm_sub_epi8(chars, lower_a);
        __m128i bias = _mm_set1_epi8(static_cast<char>(0x80u));
        __m128i offset_biased = _mm_xor_si128(offset, bias);
        // 26 ^ 0x80 = threshold for is_lower: cmpgt(threshold, offset_biased) means offset < 26
        __m128i threshold = _mm_set1_epi8(static_cast<char>(static_cast<unsigned char>(26 ^ 0x80)));
        __m128i is_lower = _mm_cmpgt_epi8(threshold, offset_biased);

        // Subtract 32 from lowercase bytes only
        __m128i sub_mask = _mm_and_si128(is_lower, diff);
        __m128i result = _mm_sub_epi8(chars, sub_mask);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), result);
    }

    // Scalar tail
    for (; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        dst[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : static_cast<char>(c);
    }
}

void sse2_to_lower(const char* src, char* dst, int len) {
    int i = 0;
    const __m128i diff = _mm_set1_epi8(32);

    for (; i + 16 <= len; i += 16) {
        __m128i chars = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));

        // Range check [A, Z] using same bias trick
        __m128i upper_a = _mm_set1_epi8('A');
        __m128i offset = _mm_sub_epi8(chars, upper_a);
        __m128i bias = _mm_set1_epi8(static_cast<char>(0x80));
        __m128i offset_biased = _mm_xor_si128(offset, bias);
        __m128i limit_plus1 =
            _mm_set1_epi8(static_cast<char>(static_cast<unsigned char>(26 ^ 0x80)));
        __m128i is_upper = _mm_cmpgt_epi8(limit_plus1, offset_biased);

        // Add 32 to uppercase bytes only
        __m128i add_mask = _mm_and_si128(is_upper, diff);
        __m128i result = _mm_add_epi8(chars, add_mask);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), result);
    }

    // Scalar tail
    for (; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        dst[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c);
    }
}

int sse2_trim_start(const char* s, int len) {
    int i = 0;
    const __m128i ws_space = _mm_set1_epi8(' ');
    const __m128i ws_tab = _mm_set1_epi8('\t');
    const __m128i ws_cr = _mm_set1_epi8('\r');
    const __m128i ws_lf = _mm_set1_epi8('\n');

    for (; i + 16 <= len; i += 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + i));
        __m128i is_ws = _mm_or_si128(
            _mm_or_si128(_mm_cmpeq_epi8(chunk, ws_space), _mm_cmpeq_epi8(chunk, ws_tab)),
            _mm_or_si128(_mm_cmpeq_epi8(chunk, ws_cr), _mm_cmpeq_epi8(chunk, ws_lf)));
        int mask = _mm_movemask_epi8(is_ws);
        if (mask != 0xFFFF) {
            // Found a non-whitespace byte
            // First zero bit = first non-ws byte
            int non_ws_mask = ~mask & 0xFFFF;
#ifdef _MSC_VER
            unsigned long bit;
            _BitScanForward(&bit, static_cast<unsigned long>(non_ws_mask));
            return i + static_cast<int>(bit);
#else
            return i + __builtin_ctz(non_ws_mask);
#endif
        }
    }

    // Scalar tail
    for (; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            return i;
        }
    }
    return len;
}

int sse2_trim_end(const char* s, int len) {
    int i = len;

    // Process 16-byte chunks from end
    while (i >= 16) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + i - 16));
        const __m128i ws_space = _mm_set1_epi8(' ');
        const __m128i ws_tab = _mm_set1_epi8('\t');
        const __m128i ws_cr = _mm_set1_epi8('\r');
        const __m128i ws_lf = _mm_set1_epi8('\n');

        __m128i is_ws = _mm_or_si128(
            _mm_or_si128(_mm_cmpeq_epi8(chunk, ws_space), _mm_cmpeq_epi8(chunk, ws_tab)),
            _mm_or_si128(_mm_cmpeq_epi8(chunk, ws_cr), _mm_cmpeq_epi8(chunk, ws_lf)));
        int mask = _mm_movemask_epi8(is_ws);
        if (mask != 0xFFFF) {
            // Found a non-whitespace byte
            // Highest zero bit = last non-ws byte in chunk
            int non_ws_mask = ~mask & 0xFFFF;
#ifdef _MSC_VER
            unsigned long bit;
            _BitScanReverse(&bit, static_cast<unsigned long>(non_ws_mask));
            return (i - 16) + static_cast<int>(bit) + 1;
#else
            int highest = 31 - __builtin_clz(non_ws_mask);
            return (i - 16) + highest + 1;
#endif
        }
        i -= 16;
    }

    // Scalar tail for remaining bytes
    for (int j = i - 1; j >= 0; --j) {
        unsigned char c = static_cast<unsigned char>(s[j]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            return j + 1;
        }
    }
    return 0;
}

#endif // TML_SSE2

// --------------------------------------------------------------------------
// SSE4.2 implementations
// --------------------------------------------------------------------------

#if TML_SSE42

int sse42_str_find(const char* hay, int hay_len, const char* needle, int needle_len) {
    if (needle_len == 0)
        return 0;
    if (needle_len > hay_len)
        return -1;

    // SSE4.2 PCMPISTRI can handle needle up to 16 bytes
    if (needle_len <= 16) {
        const int limit = hay_len - needle_len;
        // Load needle into XMM register (implicit length from needle_len)
        __m128i needle_xmm = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needle));

        // _SIDD_CMP_EQUAL_ORDERED: substring search
        // _SIDD_UBYTE_OPS: unsigned byte comparison
        const int mode = _SIDD_CMP_EQUAL_ORDERED | _SIDD_UBYTE_OPS;

        int i = 0;
        while (i <= limit) {
            // How many valid bytes remain in haystack from position i
            int remaining = hay_len - i;
            if (remaining > 16)
                remaining = 16;

            __m128i hay_xmm = _mm_loadu_si128(reinterpret_cast<const __m128i*>(hay + i));

            // PCMPISTRI: returns index of first match in hay_xmm
            // Uses implicit string lengths (stops at null or 16 bytes)
            // But we have explicit lengths, so use PCMPESTRI instead
            int idx = _mm_cmpestri(needle_xmm, needle_len, hay_xmm, remaining, mode);

            if (idx < 16 && (i + idx) <= limit) {
                // Potential match — for short needles PCMPESTRI confirms it
                // For needles > 1 byte, we should verify the full match
                // PCMPESTRI with EQUAL_ORDERED does full substring match
                // so idx is valid if needle fits within the remaining window
                if (i + idx + needle_len <= hay_len) {
                    // Verify with memcmp for safety (PCMPESTRI handles it
                    // correctly for needle <= 16, but let's be safe)
                    if (memcmp(hay + i + idx, needle, static_cast<size_t>(needle_len)) == 0) {
                        return i + idx;
                    }
                }
                // PCMPISTRI found a partial match, advance past it
                i += idx + 1;
            } else {
                // No match in this 16-byte window
                i += 16 - needle_len + 1;
            }
        }
        return -1;
    }

    // For needles > 16 bytes, fall through to SSE2 first-byte filter
    return sse2_str_find(hay, hay_len, needle, needle_len);
}

uint64_t sse42_str_hash(const char* s, int len) {
    // CRC32C using SSE4.2 intrinsics
    uint64_t crc = 0xFFFFFFFFFFFFFFFFULL;
    int i = 0;

    // Process 8 bytes at a time
    for (; i + 8 <= len; i += 8) {
        uint64_t word;
        memcpy(&word, s + i, 8);
        crc = _mm_crc32_u64(crc, word);
    }

    // Process 4 bytes
    if (i + 4 <= len) {
        uint32_t word;
        memcpy(&word, s + i, 4);
        crc = _mm_crc32_u32(static_cast<uint32_t>(crc), word);
        i += 4;
    }

    // Process remaining bytes
    for (; i < len; ++i) {
        crc = _mm_crc32_u8(static_cast<uint32_t>(crc), static_cast<uint8_t>(s[i]));
    }

    return crc;
}

#endif // TML_SSE42

} // anonymous namespace

// ============================================================================
// Public API — runtime dispatch
// ============================================================================

extern "C" {

int simd_str_find(const char* hay, int hay_len, const char* needle, int needle_len) {
    if (hay == nullptr || hay_len <= 0)
        return (needle_len == 0) ? 0 : -1;
    if (needle == nullptr || needle_len <= 0)
        return 0;

#if TML_SSE42
    if (tml::simd::has_sse42()) {
        return sse42_str_find(hay, hay_len, needle, needle_len);
    }
#endif
#if TML_SSE2
    if (tml::simd::has_sse2()) {
        return sse2_str_find(hay, hay_len, needle, needle_len);
    }
#endif
    return scalar_str_find(hay, hay_len, needle, needle_len);
}

int simd_str_rfind(const char* hay, int hay_len, const char* needle, int needle_len) {
    if (hay == nullptr || hay_len <= 0)
        return (needle_len == 0) ? 0 : -1;
    if (needle == nullptr || needle_len <= 0)
        return hay_len;

#if TML_SSE2
    if (tml::simd::has_sse2()) {
        return sse2_str_rfind(hay, hay_len, needle, needle_len);
    }
#endif
    return scalar_str_rfind(hay, hay_len, needle, needle_len);
}

int simd_str_contains(const char* hay, int hay_len, const char* needle, int needle_len) {
    return simd_str_find(hay, hay_len, needle, needle_len) >= 0 ? 1 : 0;
}

void simd_to_upper(const char* src, char* dst, int len) {
    if (src == nullptr || dst == nullptr || len <= 0)
        return;

#if TML_SSE2
    if (tml::simd::has_sse2()) {
        sse2_to_upper(src, dst, len);
        return;
    }
#endif
    scalar_to_upper(src, dst, len);
}

void simd_to_lower(const char* src, char* dst, int len) {
    if (src == nullptr || dst == nullptr || len <= 0)
        return;

#if TML_SSE2
    if (tml::simd::has_sse2()) {
        sse2_to_lower(src, dst, len);
        return;
    }
#endif
    scalar_to_lower(src, dst, len);
}

int simd_trim_start(const char* s, int len) {
    if (s == nullptr || len <= 0)
        return 0;

#if TML_SSE2
    if (tml::simd::has_sse2()) {
        return sse2_trim_start(s, len);
    }
#endif
    return scalar_trim_start(s, len);
}

int simd_trim_end(const char* s, int len) {
    if (s == nullptr || len <= 0)
        return 0;

#if TML_SSE2
    if (tml::simd::has_sse2()) {
        return sse2_trim_end(s, len);
    }
#endif
    return scalar_trim_end(s, len);
}

uint64_t simd_str_hash(const char* s, int len) {
    if (s == nullptr || len <= 0)
        return 0;

#if TML_SSE42
    if (tml::simd::has_sse42()) {
        return sse42_str_hash(s, len);
    }
#endif
    return scalar_str_hash(s, len);
}

int simd_str_eq(const char* a, int a_len, const char* b, int b_len) {
    if (a_len != b_len)
        return 0;
    if (a_len == 0)
        return 1;
    if (a == b)
        return 1; // pointer identity
    if (a == nullptr || b == nullptr)
        return 0;
    return memcmp(a, b, static_cast<size_t>(a_len)) == 0 ? 1 : 0;
}

} // extern "C"
