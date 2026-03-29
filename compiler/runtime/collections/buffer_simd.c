/**
 * @file buffer_simd.c
 * @brief SIMD-optimized buffer operations for TML Buffer type.
 *
 * Provides:
 *   buf_last_index_of_simd  — SSE2 reverse byte scan
 *   buf_bswap16             — bswap16 bulk in-place
 *   buf_bswap32             — bswap32 bulk in-place
 *   buf_bswap64             — bswap64 bulk in-place
 *
 * Each function has a scalar fallback for platforms without SSE2.
 * Called via @extern("c") from lib/std/src/collections/buffer.tml.
 */

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

/* ─── bswap portability ──────────────────────────────────────────────────── */

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#include <stdlib.h>
#define bswap16(x) _byteswap_ushort(x)
#define bswap32(x) _byteswap_ulong(x)
#define bswap64(x) _byteswap_uint64(x)
#else
#define bswap16(x) __builtin_bswap16(x)
#define bswap32(x) __builtin_bswap32(x)
#define bswap64(x) __builtin_bswap64(x)
#endif

/* ─── SSE2 header (only when available) ─────────────────────────────────── */

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#define TML_HAS_SSE2 1
#include <emmintrin.h> /* SSE2 */
#else
#define TML_HAS_SSE2 0
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * buf_last_index_of_simd
 *
 * Searches backwards from position `start` (inclusive) for the first
 * occurrence of byte `value` in `data[0..len)`.
 * Returns the index, or -1 if not found.
 *
 * SSE2 path: processes 16 bytes at a time using PCMPEQB + PMOVMSKB + BSR.
 * Scalar fallback: simple reverse linear scan.
 * ═══════════════════════════════════════════════════════════════════════════ */

TML_EXPORT int64_t buf_last_index_of_simd(const uint8_t* data, int64_t len, int32_t value,
                                          int64_t start) {
    if (!data || len <= 0)
        return -1;

    int64_t s = start;
    if (s < 0 || s >= len)
        s = len - 1;

    uint8_t target = (uint8_t)(value & 0xFF);

#if TML_HAS_SSE2
    /* Process 16-byte chunks from the end */
    const __m128i needle = _mm_set1_epi8((char)target);

    /* Align the scan to a 16-byte boundary going backwards */
    int64_t block_end = s;

    while (block_end >= 15) {
        /* Load 16 bytes ending at block_end */
        int64_t block_start = block_end - 15;
        const __m128i chunk = _mm_loadu_si128((const __m128i*)(data + block_start));
        const __m128i cmp = _mm_cmpeq_epi8(chunk, needle);
        int mask = _mm_movemask_epi8(cmp);

        if (mask) {
            /* Find the highest set bit — that is the rightmost match in
               the chunk, which is the last occurrence overall because we
               scan from the end. */
#if defined(_MSC_VER) && !defined(__clang__)
            unsigned long bit;
            _BitScanReverse(&bit, (unsigned long)mask);
            return block_start + (int64_t)bit;
#else
            /* __builtin_clz operates on unsigned int (at least 32-bit).
               We want the position of the most significant set bit. */
            int bit = 31 - __builtin_clz((unsigned int)mask);
            return block_start + (int64_t)bit;
#endif
        }

        block_end = block_start - 1;
    }

    /* Scalar tail for the remaining < 16 bytes */
    int64_t i = block_end;
    while (i >= 0) {
        if (data[i] == target)
            return i;
        i--;
    }
    return -1;

#else /* Scalar fallback */
    int64_t i = s;
    while (i >= 0) {
        if (data[i] == target)
            return i;
        i--;
    }
    return -1;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * buf_bswap16
 *
 * Swaps every pair of bytes in data[0..len) in place.
 * Uses __builtin_bswap16 / _byteswap_ushort per 2-byte word.
 * len must be a multiple of 2; any trailing odd byte is left untouched.
 * ═══════════════════════════════════════════════════════════════════════════ */

TML_EXPORT void buf_bswap16(uint8_t* data, int64_t len) {
    if (!data || len < 2)
        return;

    int64_t i = 0;
    /* Process 2 bytes at a time using bswap16 */
    for (; i + 1 < len; i += 2) {
        uint16_t word;
        memcpy(&word, data + i, 2);
        word = bswap16(word);
        memcpy(data + i, &word, 2);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * buf_bswap32
 *
 * Reverses byte order within every 4-byte word in data[0..len) in place.
 * Uses __builtin_bswap32 / _byteswap_ulong.
 * Any trailing bytes (len % 4 != 0) are left untouched.
 * ═══════════════════════════════════════════════════════════════════════════ */

TML_EXPORT void buf_bswap32(uint8_t* data, int64_t len) {
    if (!data || len < 4)
        return;

    int64_t i = 0;
    for (; i + 3 < len; i += 4) {
        uint32_t word;
        memcpy(&word, data + i, 4);
        word = bswap32(word);
        memcpy(data + i, &word, 4);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * buf_bswap64
 *
 * Reverses byte order within every 8-byte word in data[0..len) in place.
 * Uses __builtin_bswap64 / _byteswap_uint64.
 * Any trailing bytes (len % 8 != 0) are left untouched.
 * ═══════════════════════════════════════════════════════════════════════════ */

TML_EXPORT void buf_bswap64(uint8_t* data, int64_t len) {
    if (!data || len < 8)
        return;

    int64_t i = 0;
    for (; i + 7 < len; i += 8) {
        uint64_t word;
        memcpy(&word, data + i, 8);
        word = bswap64(word);
        memcpy(data + i, &word, 8);
    }
}
