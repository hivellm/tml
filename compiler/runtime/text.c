/**
 * @file text.c
 * @brief TML Runtime - Text Type Implementation
 *
 * Dynamic string type with heap allocation and automatic growth.
 * Implements Small String Optimization (SSO) for strings <= 23 bytes.
 *
 * ## Memory Layout
 *
 * Text uses a union for SSO:
 * - Heap mode: data pointer, length, capacity
 * - Inline mode: 23 bytes inline storage + length byte
 *
 * The flags byte indicates the mode:
 * - Bit 0: 0 = heap, 1 = inline
 */

#include "log.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// SSO threshold - strings up to this size are stored inline
#define TEXT_SSO_CAPACITY 23

// Flag bits
#define TEXT_FLAG_INLINE 0x01

/**
 * @brief Text structure with Small String Optimization (SSO)
 */
typedef struct {
    union {
        struct {
            uint8_t* data; // Heap-allocated buffer
            uint64_t len;  // Current length
            uint64_t cap;  // Allocated capacity
        } heap;
        struct {
            uint8_t data[TEXT_SSO_CAPACITY]; // Inline storage
            uint8_t len;                     // Inline length (max 23)
        } sso;
    };
    uint8_t flags; // Bit 0: is_inline
} TmlText;

// ============================================================================
// Internal Helpers
// ============================================================================

static inline int text_is_inline(const TmlText* t) {
    return (t->flags & TEXT_FLAG_INLINE) != 0;
}

static inline uint64_t text_len(const TmlText* t) {
    return text_is_inline(t) ? t->sso.len : t->heap.len;
}

static inline uint64_t text_cap(const TmlText* t) {
    return text_is_inline(t) ? TEXT_SSO_CAPACITY : t->heap.cap;
}

static inline const uint8_t* text_data(const TmlText* t) {
    return text_is_inline(t) ? t->sso.data : t->heap.data;
}

static inline uint8_t* text_data_mut(TmlText* t) {
    return text_is_inline(t) ? t->sso.data : t->heap.data;
}

/**
 * Calculate new capacity using growth strategy:
 * - Start at 32 bytes
 * - Double until 4KB
 * - Then grow by 50%
 */
static uint64_t grow_capacity(uint64_t current, uint64_t required) {
    uint64_t new_cap;
    if (current == 0) {
        new_cap = 32;
    } else if (current < 4096) {
        new_cap = current * 2;
    } else {
        new_cap = current + current / 2;
    }
    return new_cap >= required ? new_cap : required;
}

/**
 * Convert from inline to heap storage if needed
 */
static void text_ensure_heap(TmlText* t, uint64_t required_cap) {
    if (text_is_inline(t)) {
        // Currently inline, need to switch to heap
        if (required_cap <= TEXT_SSO_CAPACITY) {
            return; // Still fits inline
        }

        uint64_t len = t->sso.len;
        uint64_t new_cap = grow_capacity(TEXT_SSO_CAPACITY, required_cap);
        uint8_t* new_data = (uint8_t*)malloc(new_cap + 1); // +1 for null terminator
        if (!new_data) {
            RT_FATAL("text", "out of memory");
            exit(1);
        }

        // Copy inline data to heap
        memcpy(new_data, t->sso.data, len);
        new_data[len] = '\0';

        // Switch to heap mode
        t->heap.data = new_data;
        t->heap.len = len;
        t->heap.cap = new_cap;
        t->flags &= ~TEXT_FLAG_INLINE;
    } else {
        // Already heap, check if reallocation needed
        if (required_cap <= t->heap.cap) {
            return;
        }

        uint64_t new_cap = grow_capacity(t->heap.cap, required_cap);
        uint8_t* new_data = (uint8_t*)realloc(t->heap.data, new_cap + 1);
        if (!new_data) {
            RT_FATAL("text", "out of memory");
            exit(1);
        }

        t->heap.data = new_data;
        t->heap.cap = new_cap;
    }
}

// ============================================================================
// Constructors
// ============================================================================

/**
 * Create a new empty Text
 */
TML_EXPORT TmlText* tml_text_new(void) {
    TmlText* t = (TmlText*)calloc(1, sizeof(TmlText));
    if (!t) {
        RT_FATAL("text", "out of memory");
        exit(1);
    }
    t->flags = TEXT_FLAG_INLINE;
    t->sso.len = 0;
    return t;
}

/**
 * Internal: Create Text from data pointer and length
 */
static TmlText* text_from_bytes(const uint8_t* data, uint64_t len) {
    TmlText* t = (TmlText*)calloc(1, sizeof(TmlText));
    if (!t) {
        RT_FATAL("text", "out of memory");
        exit(1);
    }

    if (len <= TEXT_SSO_CAPACITY) {
        // Use SSO
        t->flags = TEXT_FLAG_INLINE;
        memcpy(t->sso.data, data, len);
        t->sso.len = (uint8_t)len;
    } else {
        // Use heap
        t->flags = 0;
        t->heap.cap = grow_capacity(0, len);
        t->heap.data = (uint8_t*)malloc(t->heap.cap + 1);
        if (!t->heap.data) {
            free(t);
            RT_FATAL("text", "out of memory");
            exit(1);
        }
        memcpy(t->heap.data, data, len);
        t->heap.data[len] = '\0';
        t->heap.len = len;
    }

    return t;
}

/**
 * Create Text from a null-terminated string
 */
TML_EXPORT TmlText* tml_text_from_str(const char* data) {
    if (!data)
        return tml_text_new();
    return text_from_bytes((const uint8_t*)data, strlen(data));
}

/**
 * Create Text with pre-allocated capacity
 */
TML_EXPORT TmlText* tml_text_with_capacity(uint64_t cap) {
    TmlText* t = (TmlText*)calloc(1, sizeof(TmlText));
    if (!t) {
        RT_FATAL("text", "out of memory");
        exit(1);
    }

    if (cap <= TEXT_SSO_CAPACITY) {
        t->flags = TEXT_FLAG_INLINE;
        t->sso.len = 0;
    } else {
        t->flags = 0;
        t->heap.cap = cap;
        t->heap.data = (uint8_t*)malloc(cap + 1);
        if (!t->heap.data) {
            free(t);
            RT_FATAL("text", "out of memory");
            exit(1);
        }
        t->heap.data[0] = '\0';
        t->heap.len = 0;
    }

    return t;
}

/**
 * Clone a Text (deep copy)
 */
TML_EXPORT TmlText* tml_text_clone(const TmlText* src) {
    if (!src)
        return tml_text_new();
    return text_from_bytes(text_data(src), text_len(src));
}

/**
 * Free a Text
 */
TML_EXPORT void tml_text_drop(TmlText* t) {
    if (!t)
        return;
    if (!text_is_inline(t) && t->heap.data) {
        free(t->heap.data);
    }
    free(t);
}

// ============================================================================
// Accessors
// ============================================================================

TML_EXPORT uint64_t tml_text_len(const TmlText* t) {
    return t ? text_len(t) : 0;
}

TML_EXPORT uint64_t tml_text_capacity(const TmlText* t) {
    return t ? text_cap(t) : 0;
}

TML_EXPORT int32_t tml_text_is_empty(const TmlText* t) {
    return !t || text_len(t) == 0;
}

TML_EXPORT const uint8_t* tml_text_data(const TmlText* t) {
    return t ? text_data(t) : (const uint8_t*)"";
}

TML_EXPORT int32_t tml_text_byte_at(const TmlText* t, uint64_t idx) {
    if (!t || idx >= text_len(t))
        return -1;
    return text_data(t)[idx];
}

// ============================================================================
// Modification
// ============================================================================

TML_EXPORT void tml_text_clear(TmlText* t) {
    if (!t)
        return;
    if (text_is_inline(t)) {
        t->sso.len = 0;
    } else {
        t->heap.len = 0;
        if (t->heap.data)
            t->heap.data[0] = '\0';
    }
}

// UNSAFE: Get raw data pointer (assumes heap mode)
TML_EXPORT uint8_t* tml_text_data_ptr(TmlText* t) {
    if (!t)
        return NULL;
    // For performance, assume heap mode (caller responsibility)
    return t->heap.data;
}

// UNSAFE: Set length directly (no bounds checking)
TML_EXPORT void tml_text_set_len(TmlText* t, uint64_t new_len) {
    if (!t)
        return;
    // For performance, assume heap mode (caller responsibility)
    t->heap.len = new_len;
    // Null terminate
    if (t->heap.data)
        t->heap.data[new_len] = '\0';
}

TML_EXPORT void tml_text_push(TmlText* t, int32_t c) {
    if (!t)
        return;
    uint64_t len = text_len(t);
    text_ensure_heap(t, len + 1);

    uint8_t* data = text_data_mut(t);
    data[len] = (uint8_t)c;

    if (text_is_inline(t)) {
        t->sso.len = (uint8_t)(len + 1);
    } else {
        t->heap.len = len + 1;
        t->heap.data[len + 1] = '\0';
    }
}

// Internal helper for pushing raw bytes with known length
static void text_push_bytes(TmlText* t, const uint8_t* data, uint64_t data_len) {
    if (!t || !data || data_len == 0)
        return;

    uint64_t len = text_len(t);
    text_ensure_heap(t, len + data_len);

    uint8_t* dst = text_data_mut(t);
    memcpy(dst + len, data, data_len);

    if (text_is_inline(t)) {
        t->sso.len = (uint8_t)(len + data_len);
    } else {
        t->heap.len = len + data_len;
        t->heap.data[len + data_len] = '\0';
    }
}

TML_EXPORT void tml_text_push_str(TmlText* t, const char* data) {
    if (!t || !data)
        return;
    text_push_bytes(t, (const uint8_t*)data, strlen(data));
}

/**
 * @brief Push string with known length (avoids strlen overhead)
 */
TML_EXPORT void tml_text_push_str_len(TmlText* t, const char* data, uint64_t data_len) {
    if (!t || !data || data_len == 0)
        return;
    text_push_bytes(t, (const uint8_t*)data, data_len);
}

// Lookup table for fast 2-digit conversion (00-99)
static const char text_digit_pairs[201] = "00010203040506070809"
                                          "10111213141516171819"
                                          "20212223242526272829"
                                          "30313233343536373839"
                                          "40414243444546474849"
                                          "50515253545556575859"
                                          "60616263646566676869"
                                          "70717273747576777879"
                                          "80818283848586878889"
                                          "90919293949596979899";

/**
 * @brief Push integer directly without intermediate string allocation.
 * Uses the same lookup table algorithm as fast_i64_to_str.
 */
TML_EXPORT void tml_text_push_i64(TmlText* t, int64_t n) {
    if (!t)
        return;

    // Convert to temp buffer using fast algorithm
    char buf[21]; // Max 20 digits + sign
    char* end = buf + 21;
    char* p = end;
    int neg = 0;

    if (n < 0) {
        neg = 1;
        // Handle INT64_MIN specially
        if (n == (-9223372036854775807LL - 1)) {
            text_push_bytes(t, (const uint8_t*)"-9223372036854775808", 20);
            return;
        }
        n = -n;
    }

    if (n == 0) {
        text_push_bytes(t, (const uint8_t*)"0", 1);
        return;
    }

    // Process 2 digits at a time
    while (n >= 100) {
        int idx = (int)((n % 100) * 2);
        n /= 100;
        *--p = text_digit_pairs[idx + 1];
        *--p = text_digit_pairs[idx];
    }

    // Handle remaining 1-2 digits
    if (n >= 10) {
        int idx = (int)(n * 2);
        *--p = text_digit_pairs[idx + 1];
        *--p = text_digit_pairs[idx];
    } else {
        *--p = (char)('0' + n);
    }

    if (neg) {
        *--p = '-';
    }

    text_push_bytes(t, (const uint8_t*)p, (uint64_t)(end - p));
}

/**
 * @brief Push formatted message: prefix + integer + suffix
 * Combines three operations into one for efficiency.
 */
TML_EXPORT void tml_text_push_formatted(TmlText* t, const char* prefix, uint64_t prefix_len,
                                        int64_t n, const char* suffix, uint64_t suffix_len) {
    if (!t)
        return;

    // Reserve space for all parts to minimize reallocations
    // Max int length is 20 digits + sign = 21
    text_ensure_heap(t, text_len(t) + prefix_len + 21 + suffix_len);

    if (prefix && prefix_len > 0) {
        text_push_bytes(t, (const uint8_t*)prefix, prefix_len);
    }

    // Push the integer
    tml_text_push_i64(t, n);

    if (suffix && suffix_len > 0) {
        text_push_bytes(t, (const uint8_t*)suffix, suffix_len);
    }
}

TML_EXPORT void tml_text_reserve(TmlText* t, uint64_t additional) {
    if (!t)
        return;
    text_ensure_heap(t, text_len(t) + additional);
}

/**
 * @brief Push log-style message: s1 + n1 + s2 + n2 + s3 + n3 + s4
 * Combines 7 operations into 1 FFI call for efficiency.
 */
TML_EXPORT void tml_text_push_log(TmlText* t, const char* s1, uint64_t s1_len, int64_t n1,
                                  const char* s2, uint64_t s2_len, int64_t n2, const char* s3,
                                  uint64_t s3_len, int64_t n3, const char* s4, uint64_t s4_len) {
    if (!t)
        return;

    // Reserve space for all parts (3 ints max 21 chars each)
    text_ensure_heap(t, text_len(t) + s1_len + s2_len + s3_len + s4_len + 63);

    if (s1 && s1_len > 0)
        text_push_bytes(t, (const uint8_t*)s1, s1_len);
    tml_text_push_i64(t, n1);
    if (s2 && s2_len > 0)
        text_push_bytes(t, (const uint8_t*)s2, s2_len);
    tml_text_push_i64(t, n2);
    if (s3 && s3_len > 0)
        text_push_bytes(t, (const uint8_t*)s3, s3_len);
    tml_text_push_i64(t, n3);
    if (s4 && s4_len > 0)
        text_push_bytes(t, (const uint8_t*)s4, s4_len);
}

/**
 * @brief Push path-style pattern: s1 + n1 + s2 + n2 + s3
 * Combines 5 operations into 1 FFI call for efficiency.
 * Common for file path building like: "/path/module" + 42 + "/file" + 123 + ".txt"
 */
TML_EXPORT void tml_text_push_path(TmlText* t, const char* s1, uint64_t s1_len, int64_t n1,
                                   const char* s2, uint64_t s2_len, int64_t n2, const char* s3,
                                   uint64_t s3_len) {
    if (!t)
        return;

    // Reserve space for all parts (2 ints max 21 chars each)
    text_ensure_heap(t, text_len(t) + s1_len + s2_len + s3_len + 42);

    if (s1 && s1_len > 0)
        text_push_bytes(t, (const uint8_t*)s1, s1_len);
    tml_text_push_i64(t, n1);
    if (s2 && s2_len > 0)
        text_push_bytes(t, (const uint8_t*)s2, s2_len);
    tml_text_push_i64(t, n2);
    if (s3 && s3_len > 0)
        text_push_bytes(t, (const uint8_t*)s3, s3_len);
}

/**
 * @brief Fill with N copies of the same character.
 * Much faster than calling push() N times due to reduced FFI overhead.
 */
TML_EXPORT void tml_text_fill_char(TmlText* t, int32_t c, uint64_t count) {
    if (!t || count == 0)
        return;

    uint64_t len = text_len(t);
    text_ensure_heap(t, len + count);

    uint8_t* data = text_data_mut(t);
    memset(data + len, (uint8_t)c, count);

    if (text_is_inline(t)) {
        t->sso.len = (uint8_t)(len + count);
    } else {
        t->heap.len = len + count;
        t->heap.data[len + count] = '\0';
    }
}

/**
 * @brief UNSAFE push_i64 - assumes heap mode with sufficient capacity.
 * Called from inline fast path where checks are already done.
 * Returns the number of bytes written.
 */
TML_EXPORT int64_t tml_text_push_i64_unsafe(TmlText* t, int64_t n) {
    // Direct access - no null check, assumes heap mode
    uint8_t* data = t->heap.data;
    uint64_t len = t->heap.len;

    // Convert to temp buffer using fast algorithm
    char buf[21]; // Max 20 digits + sign
    char* end = buf + 21;
    char* p = end;
    int neg = 0;

    if (n < 0) {
        neg = 1;
        // Handle INT64_MIN specially
        if (n == (-9223372036854775807LL - 1)) {
            memcpy(data + len, "-9223372036854775808", 20);
            t->heap.len = len + 20;
            return 20;
        }
        n = -n;
    }

    if (n == 0) {
        data[len] = '0';
        t->heap.len = len + 1;
        return 1;
    }

    // Process 2 digits at a time
    while (n >= 100) {
        int idx = (int)((n % 100) * 2);
        n /= 100;
        *--p = text_digit_pairs[idx + 1];
        *--p = text_digit_pairs[idx];
    }

    // Handle remaining 1-2 digits
    if (n >= 10) {
        int idx = (int)(n * 2);
        *--p = text_digit_pairs[idx + 1];
        *--p = text_digit_pairs[idx];
    } else {
        *--p = (char)('0' + n);
    }

    if (neg) {
        *--p = '-';
    }

    uint64_t digits_len = (uint64_t)(end - p);
    memcpy(data + len, p, digits_len);
    t->heap.len = len + digits_len;
    return (int64_t)digits_len;
}

// ============================================================================
// Search Methods
// ============================================================================

TML_EXPORT int64_t tml_text_index_of(const TmlText* t, const char* needle) {
    if (!t || !needle)
        return -1;

    uint64_t needle_len = strlen(needle);
    if (needle_len == 0)
        return -1;

    uint64_t len = text_len(t);
    if (needle_len > len)
        return -1;

    const uint8_t* data = text_data(t);
    for (uint64_t i = 0; i <= len - needle_len; i++) {
        if (memcmp(data + i, needle, needle_len) == 0) {
            return (int64_t)i;
        }
    }
    return -1;
}

TML_EXPORT int64_t tml_text_last_index_of(const TmlText* t, const char* needle) {
    if (!t || !needle)
        return -1;

    uint64_t needle_len = strlen(needle);
    if (needle_len == 0)
        return -1;

    uint64_t len = text_len(t);
    if (needle_len > len)
        return -1;

    const uint8_t* data = text_data(t);
    for (int64_t i = (int64_t)(len - needle_len); i >= 0; i--) {
        if (memcmp(data + i, needle, needle_len) == 0) {
            return i;
        }
    }
    return -1;
}

TML_EXPORT int32_t tml_text_starts_with(const TmlText* t, const char* prefix) {
    if (!t || !prefix)
        return 0;
    uint64_t prefix_len = strlen(prefix);
    if (prefix_len == 0)
        return 1;
    if (prefix_len > text_len(t))
        return 0;
    return memcmp(text_data(t), prefix, prefix_len) == 0;
}

TML_EXPORT int32_t tml_text_ends_with(const TmlText* t, const char* suffix) {
    if (!t || !suffix)
        return 0;
    uint64_t suffix_len = strlen(suffix);
    if (suffix_len == 0)
        return 1;
    uint64_t len = text_len(t);
    if (suffix_len > len)
        return 0;
    return memcmp(text_data(t) + len - suffix_len, suffix, suffix_len) == 0;
}

TML_EXPORT int32_t tml_text_contains(const TmlText* t, const char* needle) {
    return tml_text_index_of(t, needle) >= 0;
}

// ============================================================================
// Transformation Methods (return new Text)
// ============================================================================

TML_EXPORT TmlText* tml_text_to_upper(const TmlText* t) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    TmlText* result = tml_text_with_capacity(len);
    const uint8_t* src = text_data(t);

    for (uint64_t i = 0; i < len; i++) {
        tml_text_push(result, (uint8_t)toupper(src[i]));
    }
    return result;
}

TML_EXPORT TmlText* tml_text_to_lower(const TmlText* t) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    TmlText* result = tml_text_with_capacity(len);
    const uint8_t* src = text_data(t);

    for (uint64_t i = 0; i < len; i++) {
        tml_text_push(result, (uint8_t)tolower(src[i]));
    }
    return result;
}

TML_EXPORT TmlText* tml_text_trim(const TmlText* t) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    if (len == 0)
        return tml_text_new();

    const uint8_t* data = text_data(t);
    uint64_t start = 0;
    uint64_t end = len;

    while (start < len && isspace(data[start]))
        start++;
    while (end > start && isspace(data[end - 1]))
        end--;

    return text_from_bytes(data + start, end - start);
}

TML_EXPORT TmlText* tml_text_trim_start(const TmlText* t) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    if (len == 0)
        return tml_text_new();

    const uint8_t* data = text_data(t);
    uint64_t start = 0;

    while (start < len && isspace(data[start]))
        start++;

    return text_from_bytes(data + start, len - start);
}

TML_EXPORT TmlText* tml_text_trim_end(const TmlText* t) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    if (len == 0)
        return tml_text_new();

    const uint8_t* data = text_data(t);
    uint64_t end = len;

    while (end > 0 && isspace(data[end - 1]))
        end--;

    return text_from_bytes(data, end);
}

TML_EXPORT TmlText* tml_text_substring(const TmlText* t, uint64_t start, uint64_t end) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    if (start >= len)
        return tml_text_new();
    if (end > len)
        end = len;
    if (start >= end)
        return tml_text_new();

    return text_from_bytes(text_data(t) + start, end - start);
}

TML_EXPORT TmlText* tml_text_repeat(const TmlText* t, uint64_t count) {
    if (!t || count == 0)
        return tml_text_new();

    uint64_t len = text_len(t);
    if (len == 0)
        return tml_text_new();

    TmlText* result = tml_text_with_capacity(len * count);
    const uint8_t* data = text_data(t);

    for (uint64_t i = 0; i < count; i++) {
        text_push_bytes(result, data, len);
    }
    return result;
}

TML_EXPORT TmlText* tml_text_replace(const TmlText* t, const char* search,
                                     const char* replacement) {
    if (!t || !search)
        return tml_text_clone(t);

    uint64_t search_len = strlen(search);
    if (search_len == 0)
        return tml_text_clone(t);

    uint64_t len = text_len(t);
    const uint8_t* data = text_data(t);

    // Find first occurrence
    int64_t pos = tml_text_index_of(t, search);
    if (pos < 0)
        return tml_text_clone(t);

    TmlText* result = tml_text_with_capacity(len);

    // Copy before match
    text_push_bytes(result, data, (uint64_t)pos);
    // Copy replacement
    if (replacement) {
        uint64_t replacement_len = strlen(replacement);
        if (replacement_len > 0) {
            text_push_bytes(result, (const uint8_t*)replacement, replacement_len);
        }
    }
    // Copy after match
    uint64_t after = (uint64_t)pos + search_len;
    if (after < len) {
        text_push_bytes(result, data + after, len - after);
    }

    return result;
}

TML_EXPORT TmlText* tml_text_replace_all(const TmlText* t, const char* search,
                                         const char* replacement) {
    if (!t || !search)
        return tml_text_clone(t);

    uint64_t search_len = strlen(search);
    if (search_len == 0)
        return tml_text_clone(t);

    uint64_t replacement_len = replacement ? strlen(replacement) : 0;

    uint64_t len = text_len(t);
    const uint8_t* data = text_data(t);
    TmlText* result = tml_text_with_capacity(len);

    uint64_t i = 0;
    while (i < len) {
        // Check for match at current position
        if (i + search_len <= len && memcmp(data + i, search, search_len) == 0) {
            // Found match - append replacement
            if (replacement_len > 0) {
                text_push_bytes(result, (const uint8_t*)replacement, replacement_len);
            }
            i += search_len;
        } else {
            // No match - append single character
            tml_text_push(result, data[i]);
            i++;
        }
    }

    return result;
}

TML_EXPORT TmlText* tml_text_reverse(const TmlText* t) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    if (len == 0)
        return tml_text_new();

    TmlText* result = tml_text_with_capacity(len);
    const uint8_t* data = text_data(t);

    for (int64_t i = (int64_t)len - 1; i >= 0; i--) {
        tml_text_push(result, data[i]);
    }
    return result;
}

TML_EXPORT TmlText* tml_text_pad_start(const TmlText* t, uint64_t target_len, int32_t pad_char) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    if (len >= target_len)
        return tml_text_clone(t);

    TmlText* result = tml_text_with_capacity(target_len);
    uint64_t pad_count = target_len - len;

    for (uint64_t i = 0; i < pad_count; i++) {
        tml_text_push(result, pad_char);
    }
    text_push_bytes(result, text_data(t), len);

    return result;
}

TML_EXPORT TmlText* tml_text_pad_end(const TmlText* t, uint64_t target_len, int32_t pad_char) {
    if (!t)
        return tml_text_new();

    uint64_t len = text_len(t);
    if (len >= target_len)
        return tml_text_clone(t);

    TmlText* result = tml_text_with_capacity(target_len);
    text_push_bytes(result, text_data(t), len);

    uint64_t pad_count = target_len - len;
    for (uint64_t i = 0; i < pad_count; i++) {
        tml_text_push(result, pad_char);
    }

    return result;
}

// ============================================================================
// Comparison
// ============================================================================

TML_EXPORT int32_t tml_text_compare(const TmlText* a, const TmlText* b) {
    if (!a && !b)
        return 0;
    if (!a)
        return -1;
    if (!b)
        return 1;

    uint64_t len_a = text_len(a);
    uint64_t len_b = text_len(b);
    uint64_t min_len = len_a < len_b ? len_a : len_b;

    int cmp = memcmp(text_data(a), text_data(b), min_len);
    if (cmp != 0)
        return cmp;

    if (len_a < len_b)
        return -1;
    if (len_a > len_b)
        return 1;
    return 0;
}

TML_EXPORT int32_t tml_text_equals(const TmlText* a, const TmlText* b) {
    if (!a && !b)
        return 1;
    if (!a || !b)
        return 0;

    uint64_t len_a = text_len(a);
    uint64_t len_b = text_len(b);
    if (len_a != len_b)
        return 0;

    return memcmp(text_data(a), text_data(b), len_a) == 0;
}

// ============================================================================
// Concatenation
// ============================================================================

TML_EXPORT TmlText* tml_text_concat(const TmlText* a, const TmlText* b) {
    uint64_t len_a = a ? text_len(a) : 0;
    uint64_t len_b = b ? text_len(b) : 0;

    TmlText* result = tml_text_with_capacity(len_a + len_b);

    if (a && len_a > 0) {
        text_push_bytes(result, text_data(a), len_a);
    }
    if (b && len_b > 0) {
        text_push_bytes(result, text_data(b), len_b);
    }

    return result;
}

TML_EXPORT TmlText* tml_text_concat_str(const TmlText* t, const char* s) {
    uint64_t len_t = t ? text_len(t) : 0;
    uint64_t s_len = s ? strlen(s) : 0;

    TmlText* result = tml_text_with_capacity(len_t + s_len);

    if (t && len_t > 0) {
        text_push_bytes(result, text_data(t), len_t);
    }
    if (s && s_len > 0) {
        text_push_bytes(result, (const uint8_t*)s, s_len);
    }

    return result;
}

// ============================================================================
// Conversion to C string (null-terminated)
// ============================================================================

TML_EXPORT const char* tml_text_as_cstr(const TmlText* t) {
    if (!t)
        return "";

    // For heap mode, data is already null-terminated
    if (!text_is_inline(t)) {
        return (const char*)t->heap.data;
    }

    // For SSO mode, we need a static buffer (not ideal but works for simple cases)
    // In practice, callers should use tml_text_data + tml_text_len for safe handling
    static char sso_buffer[TEXT_SSO_CAPACITY + 1];
    uint64_t len = t->sso.len;
    memcpy(sso_buffer, t->sso.data, len);
    sso_buffer[len] = '\0';
    return sso_buffer;
}

// ============================================================================
// Number Formatting
// ============================================================================

TML_EXPORT TmlText* tml_text_from_i64(int64_t value) {
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    return text_from_bytes((const uint8_t*)buffer, (uint64_t)len);
}

TML_EXPORT TmlText* tml_text_from_u64(uint64_t value) {
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
    return text_from_bytes((const uint8_t*)buffer, (uint64_t)len);
}

TML_EXPORT TmlText* tml_text_from_f64(double value, int32_t precision) {
    char buffer[64];
    int len;
    if (precision < 0) {
        len = snprintf(buffer, sizeof(buffer), "%g", value);
    } else {
        len = snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    }
    return text_from_bytes((const uint8_t*)buffer, (uint64_t)len);
}

TML_EXPORT TmlText* tml_text_from_bool(int32_t value) {
    return value ? text_from_bytes((const uint8_t*)"true", 4)
                 : text_from_bytes((const uint8_t*)"false", 5);
}

// ============================================================================
// Print Functions
// ============================================================================

TML_EXPORT void tml_text_print(const TmlText* t) {
    if (!t)
        return;
    uint64_t len = text_len(t);
    if (len > 0) {
        fwrite(text_data(t), 1, len, stdout);
    }
}

TML_EXPORT void tml_text_println(const TmlText* t) {
    tml_text_print(t);
    printf("\n");
}
