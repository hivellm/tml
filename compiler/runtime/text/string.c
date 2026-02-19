/**
 * @file string.c
 * @brief TML Runtime - String Functions (Active Only)
 *
 * Implements string manipulation functions for the TML language. These functions
 * provide the runtime support for TML's Str type operations.
 *
 * ## Active Components
 *
 * - **Basic operations**: length, equality, hashing
 * - **Manipulation**: concat (optimized O(1) amortized), substring, slice, trim
 * - **Search**: contains, starts_with, ends_with
 * - **Case conversion**: to_upper, to_lower
 * - **Character access**: char_at
 * - **Type conversion**: integer/float to string, char to string
 *
 * ## Removed (migrated to pure TML in str.tml / char/methods.tml)
 *
 * - str_split, str_chars, str_split_whitespace, str_lines, str_join
 * - str_find, str_rfind, str_replace, str_replace_first, str_repeat
 * - str_parse_i32, str_parse_i64, str_parse_f64
 * - str_trim_start, str_trim_end, str_as_bytes
 * - str_concat_3, str_concat_4, str_concat (legacy)
 * - char_is_X (8 functions), char_to_X/char_from_X (6 functions)
 * - strbuilder_* (9 functions)
 *
 * ## String Optimization (v2.0)
 *
 * TML strings now use an optimized representation with capacity tracking:
 * - str_concat_opt: O(1) amortized concatenation when LHS has capacity
 * - Automatic capacity growth with exponential doubling
 * - Compatible with C string literals (detected by checking heap ownership)
 *
 * ## Memory Model
 *
 * - String literals: Static memory, no capacity header
 * - Dynamic strings: Heap-allocated with 16-byte header [capacity|length|data...]
 * - str_concat_opt: Returns heap string with extra capacity for future appends
 *
 * @see env_builtins_string.cpp for compiler builtin registration
 */

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INT64_MIN
#define INT64_MIN (-9223372036854775807LL - 1)
#endif

// ============================================================================
// Optimized String Implementation
// ============================================================================
//
// String layout for dynamic strings:
//   [capacity: 8 bytes][length: 8 bytes][data: variable][null terminator]
//
// The pointer returned points to 'data', so the header is at ptr - 16.
// String literals (from code) don't have this header.
//
// We detect dynamic strings by checking if they were allocated by us,
// using a magic marker in the high bits of capacity.

#define TML_STRING_MAGIC 0x544D4C5000000000ULL // "TML\x50" in hex
#define TML_STRING_MAGIC_MASK 0xFFFF000000000000ULL
#define TML_STRING_CAPACITY_MASK 0x0000FFFFFFFFFFFFULL

// Header for dynamic strings (16 bytes, placed BEFORE the string data)
typedef struct {
    uint64_t capacity_with_magic; // High 16 bits: magic, Low 48 bits: capacity
    uint64_t length;
} TmlStringHeader;

// Check if a string pointer is a dynamic TML string (has our header)
static inline int is_dynamic_string(const char* s) {
    if (!s)
        return 0;
    const TmlStringHeader* header = (const TmlStringHeader*)(s - sizeof(TmlStringHeader));
    // Check for magic marker (with some address sanity checks)
    return (header->capacity_with_magic & TML_STRING_MAGIC_MASK) == TML_STRING_MAGIC;
}

// Get capacity of a dynamic string
static inline uint64_t get_string_capacity(const char* s) {
    const TmlStringHeader* header = (const TmlStringHeader*)(s - sizeof(TmlStringHeader));
    return header->capacity_with_magic & TML_STRING_CAPACITY_MASK;
}

// Get length of a dynamic string (stored, not computed)
static inline uint64_t get_string_length(const char* s) {
    const TmlStringHeader* header = (const TmlStringHeader*)(s - sizeof(TmlStringHeader));
    return header->length;
}

// Set length of a dynamic string
static inline void set_string_length(char* s, uint64_t len) {
    TmlStringHeader* header = (TmlStringHeader*)(s - sizeof(TmlStringHeader));
    header->length = len;
}

// Allocate a new dynamic string with given capacity (minimum 32 bytes)
static char* alloc_dynamic_string(uint64_t capacity) {
    if (capacity < 32)
        capacity = 32;

    // Allocate header + data + null terminator
    void* mem = malloc(sizeof(TmlStringHeader) + capacity + 1);
    if (!mem)
        return NULL;

    TmlStringHeader* header = (TmlStringHeader*)mem;
    header->capacity_with_magic = TML_STRING_MAGIC | (capacity & TML_STRING_CAPACITY_MASK);
    header->length = 0;

    char* data = (char*)mem + sizeof(TmlStringHeader);
    data[0] = '\0';
    return data;
}

// Free a dynamic string (only if it's actually dynamic)
void str_free(const char* s) {
    if (s && is_dynamic_string(s)) {
        void* mem = (void*)(s - sizeof(TmlStringHeader));
        free(mem);
    }
}

/**
 * @brief Optimized string concatenation with O(1) amortized complexity.
 *
 * If 'a' is a dynamic string with enough capacity, appends 'b' in place.
 * Otherwise, allocates a new dynamic string with extra capacity for future growth.
 *
 * @param a Left string (may be modified in place if dynamic with capacity)
 * @param b Right string to append
 * @return Concatenated string (may be same pointer as 'a' or new allocation)
 */
const char* str_concat_opt(const char* a, const char* b) {
    if (!a)
        a = "";
    if (!b)
        b = "";

    size_t len_a = is_dynamic_string(a) ? get_string_length(a) : strlen(a);
    size_t len_b = strlen(b);
    size_t total_len = len_a + len_b;

    // Fast path: 'a' is dynamic and has enough capacity
    if (is_dynamic_string(a)) {
        uint64_t cap = get_string_capacity(a);
        if (total_len < cap) {
            // Append in place - O(len_b) only!
            char* a_mut = (char*)a;
            memcpy(a_mut + len_a, b, len_b);
            a_mut[total_len] = '\0';
            set_string_length(a_mut, total_len);
            return a;
        }
    }

    // Slow path: allocate new string with 2x capacity for future growth
    uint64_t new_capacity = total_len * 2;
    if (new_capacity < 64)
        new_capacity = 64;

    char* result = alloc_dynamic_string(new_capacity);
    if (!result) {
        // Fallback to simple allocation
        result = (char*)malloc(total_len + 1);
        if (!result)
            return "";
        memcpy(result, a, len_a);
        memcpy(result + len_a, b, len_b);
        result[total_len] = '\0';
        return result;
    }

    memcpy(result, a, len_a);
    memcpy(result + len_a, b, len_b);
    result[total_len] = '\0';
    set_string_length(result, total_len);

    return result;
}

/**
 * @brief Concatenate multiple strings in a single allocation.
 *
 * This is much more efficient than chained str_concat_opt calls because:
 * 1. Single allocation of final size (no temporary allocations)
 * 2. Single pass through all strings (no recopying)
 *
 * @param strings Array of string pointers
 * @param count Number of strings
 * @return Concatenated string (caller owns, but uses dynamic string header)
 */
const char* str_concat_n(const char** strings, int64_t count) {
    if (count <= 0) {
        // Return empty dynamic string
        return alloc_dynamic_string(32);
    }

    // First pass: calculate total length
    size_t total_len = 0;
    for (int64_t i = 0; i < count; i++) {
        if (strings[i]) {
            total_len += strlen(strings[i]);
        }
    }

    // Allocate with 2x capacity for potential future appends
    uint64_t capacity = total_len * 2;
    if (capacity < 64)
        capacity = 64;

    char* result = alloc_dynamic_string(capacity);
    if (!result) {
        // Fallback
        result = (char*)malloc(total_len + 1);
        if (!result)
            return "";
    }

    // Second pass: copy all strings
    char* p = result;
    for (int64_t i = 0; i < count; i++) {
        if (strings[i]) {
            size_t len = strlen(strings[i]);
            memcpy(p, strings[i], len);
            p += len;
        }
    }
    *p = '\0';

    if (is_dynamic_string(result)) {
        set_string_length(result, total_len);
    }

    return result;
}

/**
 * @brief Static buffer for legacy string operations. NOT THREAD SAFE.
 *
 * Used by: str_substring, str_slice, str_to_upper, str_to_lower, str_trim
 */
static char str_buffer[4 * 1024 * 1024]; // 4MB buffer

// str_len(s: Str) -> I32
int32_t str_len(const char* s) {
    if (!s)
        return 0;
    return (int32_t)strlen(s);
}

// str_eq(a: Str, b: Str) -> Bool
int32_t str_eq(const char* a, const char* b) {
    if (!a && !b)
        return 1;
    if (!a || !b)
        return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

// str_hash(s: Str) -> I32
int32_t str_hash(const char* s) {
    if (!s)
        return 0;
    uint32_t hash = 5381;
    int c;
    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return (int32_t)hash;
}

// str_substring(s: Str, start: I32, len: I32) -> Str
const char* str_substring(const char* s, int32_t start, int32_t len) {
    if (!s)
        return "";
    int32_t slen = (int32_t)strlen(s);
    if (start < 0 || start >= slen || len <= 0)
        return "";
    if (start + len > slen)
        len = slen - start;
    if (len >= (int32_t)sizeof(str_buffer))
        len = sizeof(str_buffer) - 1;
    memcpy(str_buffer, s + start, len);
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_slice(s: Str, start: I64, end: I64) -> Str (exclusive end)
const char* str_slice(const char* s, int64_t start, int64_t end) {
    if (!s)
        return "";
    int64_t slen = (int64_t)strlen(s);
    if (start < 0)
        start = 0;
    if (end > slen)
        end = slen;
    if (start >= end)
        return "";
    int64_t len = end - start;
    if (len >= (int64_t)sizeof(str_buffer))
        len = sizeof(str_buffer) - 1;
    memcpy(str_buffer, s + start, len);
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_contains(haystack: Str, needle: Str) -> Bool
int32_t str_contains(const char* haystack, const char* needle) {
    if (!haystack || !needle)
        return 0;
    return strstr(haystack, needle) != NULL ? 1 : 0;
}

// str_starts_with(s: Str, prefix: Str) -> Bool
int32_t str_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix)
        return 0;
    size_t prefix_len = strlen(prefix);
    return strncmp(s, prefix, prefix_len) == 0 ? 1 : 0;
}

// str_ends_with(s: Str, suffix: Str) -> Bool
int32_t str_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix)
        return 0;
    size_t s_len = strlen(s);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > s_len)
        return 0;
    return strcmp(s + s_len - suffix_len, suffix) == 0 ? 1 : 0;
}

// str_to_upper(s: Str) -> Str
const char* str_to_upper(const char* s) {
    if (!s)
        return "";
    size_t len = strlen(s);
    if (len >= sizeof(str_buffer))
        len = sizeof(str_buffer) - 1;
    for (size_t i = 0; i < len; i++) {
        str_buffer[i] = (char)toupper((unsigned char)s[i]);
    }
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_to_lower(s: Str) -> Str
const char* str_to_lower(const char* s) {
    if (!s)
        return "";
    size_t len = strlen(s);
    if (len >= sizeof(str_buffer))
        len = sizeof(str_buffer) - 1;
    for (size_t i = 0; i < len; i++) {
        str_buffer[i] = (char)tolower((unsigned char)s[i]);
    }
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_trim(s: Str) -> Str
const char* str_trim(const char* s) {
    if (!s)
        return "";
    while (isspace((unsigned char)*s))
        s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        len--;
    if (len >= sizeof(str_buffer))
        len = sizeof(str_buffer) - 1;
    memcpy(str_buffer, s, len);
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_char_at(s: Str, index: I32) -> Char
int32_t str_char_at(const char* s, int32_t index) {
    if (!s || index < 0 || index >= (int32_t)strlen(s))
        return 0;
    return (int32_t)(unsigned char)s[index];
}

// ============================================================================
// String conversion utilities (Optimized with lookup table)
// ============================================================================

// Lookup table for fast 2-digit conversion (00-99)
static const char digit_pairs[201] = "00010203040506070809"
                                     "10111213141516171819"
                                     "20212223242526272829"
                                     "30313233343536373839"
                                     "40414243444546474849"
                                     "50515253545556575859"
                                     "60616263646566676869"
                                     "70717273747576777879"
                                     "80818283848586878889"
                                     "90919293949596979899";

// Fast integer to string conversion using lookup table
// Processes 2 digits at a time, ~10-20x faster than snprintf
static inline char* fast_i64_to_str(int64_t n, char* buf) {
    // Handle negative numbers
    if (n < 0) {
        *buf++ = '-';
        // Handle INT64_MIN specially (can't negate it)
        if (n == INT64_MIN) {
            memcpy(buf, "9223372036854775808", 19);
            return buf + 19;
        }
        n = -n;
    }

    // Handle 0 specially
    if (n == 0) {
        *buf++ = '0';
        return buf;
    }

    // Write digits in reverse order to temp buffer
    char temp[20];
    char* p = temp + 20;

    // Process 2 digits at a time using lookup table
    while (n >= 100) {
        int idx = (int)((n % 100) * 2);
        n /= 100;
        *--p = digit_pairs[idx + 1];
        *--p = digit_pairs[idx];
    }

    // Handle remaining 1-2 digits
    if (n >= 10) {
        int idx = (int)(n * 2);
        *--p = digit_pairs[idx + 1];
        *--p = digit_pairs[idx];
    } else {
        *--p = (char)('0' + n);
    }

    // Copy to output buffer
    size_t len = (size_t)(temp + 20 - p);
    memcpy(buf, p, len);
    return buf + len;
}

// i64_to_str(n: I64) -> Str
// Convert integer to string for string interpolation
// NOTE: Returns a newly allocated string, caller must free or use before next allocation
const char* i64_to_str(int64_t n) {
    char* buffer = (char*)malloc(32);
    if (!buffer)
        return "";
    char* end = fast_i64_to_str(n, buffer);
    *end = '\0';
    return buffer;
}

// f64_to_str(n: F64) -> Str
// Convert float to string for string interpolation
// NOTE: Returns a newly allocated string, caller must free or use before next allocation
const char* f64_to_str(double n) {
    char* buffer = (char*)malloc(64);
    if (!buffer)
        return "";
    snprintf(buffer, 64, "%g", n);
    return buffer;
}

// ============================================================================
// Type to_string methods (for Display behavior)
// ============================================================================

// i32_to_string(n: I32) -> Str
// Returns a newly allocated string to avoid static buffer issues
const char* i32_to_string(int32_t n) {
    char* buffer = (char*)malloc(16);
    if (!buffer)
        return "";
    char* end = fast_i64_to_str((int64_t)n, buffer);
    *end = '\0';
    return buffer;
}

// i64_to_string(n: I64) -> Str
// Returns a newly allocated string to avoid static buffer issues
const char* i64_to_string(int64_t n) {
    char* buffer = (char*)malloc(32);
    if (!buffer)
        return "";
    char* end = fast_i64_to_str(n, buffer);
    *end = '\0';
    return buffer;
}

// bool_to_string(b: Bool) -> Str
const char* bool_to_string(int b) {
    return b ? "true" : "false";
}

// char_to_string(c: U8) -> Str
// Converts a single ASCII character (byte) to a 1-character string
// Returns a newly allocated string to avoid static buffer issues
const char* char_to_string(uint8_t c) {
    char* buffer = (char*)malloc(2);
    if (!buffer)
        return "";
    buffer[0] = (char)c;
    buffer[1] = '\0';
    return buffer;
}

// str_as_bytes(s: Str) -> ptr (returns pointer to the string's byte data)
// Used by Str::as_bytes() in str.tml via lowlevel block
const void* str_as_bytes(const char* s) {
    return s ? s : "";
}
