/**
 * @file string.c
 * @brief TML Runtime - String Functions
 *
 * Implements string manipulation functions for the TML language. These functions
 * provide the runtime support for TML's Str type operations.
 *
 * ## Components
 *
 * - **Basic operations**: length, equality, hashing
 * - **Manipulation**: concat, substring, slice, trim
 * - **Search**: contains, starts_with, ends_with
 * - **Case conversion**: to_upper, to_lower
 * - **Character operations**: char_at, char classification, conversion
 * - **StringBuilder**: Dynamic string building
 * - **Type conversion**: integer/float to string
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
 * @brief Concatenate 3 strings in a single allocation.
 * Optimized version for the common case of "a" + "b" + "c".
 */
const char* str_concat_3(const char* a, const char* b, const char* c) {
    if (!a)
        a = "";
    if (!b)
        b = "";
    if (!c)
        c = "";

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    size_t len_c = strlen(c);
    size_t total_len = len_a + len_b + len_c;

    uint64_t capacity = total_len * 2;
    if (capacity < 64)
        capacity = 64;

    char* result = alloc_dynamic_string(capacity);
    if (!result)
        return "";

    memcpy(result, a, len_a);
    memcpy(result + len_a, b, len_b);
    memcpy(result + len_a + len_b, c, len_c);
    result[total_len] = '\0';
    set_string_length(result, total_len);

    return result;
}

/**
 * @brief Concatenate 4 strings in a single allocation.
 */
const char* str_concat_4(const char* a, const char* b, const char* c, const char* d) {
    if (!a)
        a = "";
    if (!b)
        b = "";
    if (!c)
        c = "";
    if (!d)
        d = "";

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    size_t len_c = strlen(c);
    size_t len_d = strlen(d);
    size_t total_len = len_a + len_b + len_c + len_d;

    uint64_t capacity = total_len * 2;
    if (capacity < 64)
        capacity = 64;

    char* result = alloc_dynamic_string(capacity);
    if (!result)
        return "";

    char* p = result;
    memcpy(p, a, len_a);
    p += len_a;
    memcpy(p, b, len_b);
    p += len_b;
    memcpy(p, c, len_c);
    p += len_c;
    memcpy(p, d, len_d);
    result[total_len] = '\0';
    set_string_length(result, total_len);

    return result;
}

/**
 * @brief Static buffer for legacy string operations. NOT THREAD SAFE.
 *
 * This is kept for backward compatibility with existing functions.
 * New code should use str_concat_opt which is O(1) amortized.
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

// str_concat(a: Str, b: Str) -> Str
const char* str_concat(const char* a, const char* b) {
    if (!a)
        a = "";
    if (!b)
        b = "";
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a + len_b >= sizeof(str_buffer)) {
        len_b = sizeof(str_buffer) - len_a - 1;
    }
    memcpy(str_buffer, a, len_a);
    memcpy(str_buffer + len_a, b, len_b);
    str_buffer[len_a + len_b] = '\0';
    return str_buffer;
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
// Char Operations (Unicode)
// ============================================================================
// Note: For now, these operate on ASCII. Full Unicode support requires UTF-8 decoding.

// char_is_alphabetic(c: Char) -> Bool
int32_t char_is_alphabetic(int32_t c) {
    return isalpha(c) ? 1 : 0;
}

// char_is_numeric(c: Char) -> Bool
int32_t char_is_numeric(int32_t c) {
    return isdigit(c) ? 1 : 0;
}

// char_is_alphanumeric(c: Char) -> Bool
int32_t char_is_alphanumeric(int32_t c) {
    return isalnum(c) ? 1 : 0;
}

// char_is_whitespace(c: Char) -> Bool
int32_t char_is_whitespace(int32_t c) {
    return isspace(c) ? 1 : 0;
}

// char_is_uppercase(c: Char) -> Bool
int32_t char_is_uppercase(int32_t c) {
    return isupper(c) ? 1 : 0;
}

// char_is_lowercase(c: Char) -> Bool
int32_t char_is_lowercase(int32_t c) {
    return islower(c) ? 1 : 0;
}

// char_is_ascii(c: Char) -> Bool
int32_t char_is_ascii(int32_t c) {
    return (c >= 0 && c <= 127) ? 1 : 0;
}

// char_is_control(c: Char) -> Bool
int32_t char_is_control(int32_t c) {
    return iscntrl(c) ? 1 : 0;
}

// char_to_uppercase(c: Char) -> Char
int32_t char_to_uppercase(int32_t c) {
    return (int32_t)toupper(c);
}

// char_to_lowercase(c: Char) -> Char
int32_t char_to_lowercase(int32_t c) {
    return (int32_t)tolower(c);
}

// char_to_digit(c: Char, radix: I32) -> I32
// Returns -1 if not a valid digit in the given radix
int32_t char_to_digit(int32_t c, int32_t radix) {
    if (radix < 2 || radix > 36)
        return -1;
    int32_t digit;
    if (c >= '0' && c <= '9') {
        digit = c - '0';
    } else if (c >= 'a' && c <= 'z') {
        digit = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'Z') {
        digit = c - 'A' + 10;
    } else {
        return -1;
    }
    return (digit < radix) ? digit : -1;
}

// char_from_digit(digit: I32, radix: I32) -> Char
// Returns 0 if not a valid digit in the given radix
int32_t char_from_digit(int32_t digit, int32_t radix) {
    if (radix < 2 || radix > 36)
        return 0;
    if (digit < 0 || digit >= radix)
        return 0;
    if (digit < 10)
        return '0' + digit;
    return 'a' + digit - 10;
}

// char_code(c: Char) -> I32
// Returns the Unicode code point (same as the char value for ASCII)
int32_t char_code(int32_t c) {
    return c;
}

// char_from_code(code: I32) -> Char
// Creates a character from a Unicode code point
int32_t char_from_code(int32_t code) {
    return code;
}

// ============================================================================
// StringBuilder - Mutable String Operations
// ============================================================================
// StringBuilder is a dynamically-allocated mutable string.
// Structure: { capacity: i64, length: i64, data: char* }

typedef struct {
    int64_t capacity;
    int64_t length;
    char* data;
} StringBuilder;

// strbuilder_create(capacity: I64) -> *StringBuilder
void* strbuilder_create(int64_t capacity) {
    if (capacity < 16)
        capacity = 16;
    StringBuilder* sb = (StringBuilder*)malloc(sizeof(StringBuilder));
    if (!sb)
        return NULL;
    sb->capacity = capacity;
    sb->length = 0;
    sb->data = (char*)malloc((size_t)capacity);
    if (!sb->data) {
        free(sb);
        return NULL;
    }
    sb->data[0] = '\0';
    return sb;
}

// strbuilder_destroy(sb: *StringBuilder) -> Unit
void strbuilder_destroy(void* ptr) {
    StringBuilder* sb = (StringBuilder*)ptr;
    if (sb) {
        free(sb->data);
        free(sb);
    }
}

// Internal: ensure capacity
static void strbuilder_ensure_capacity(StringBuilder* sb, int64_t needed) {
    if (sb->length + needed + 1 > sb->capacity) {
        int64_t new_cap = sb->capacity * 2;
        while (new_cap < sb->length + needed + 1) {
            new_cap *= 2;
        }
        char* new_data = (char*)realloc(sb->data, (size_t)new_cap);
        if (new_data) {
            sb->data = new_data;
            sb->capacity = new_cap;
        }
    }
}

// strbuilder_push(sb: *StringBuilder, c: Char) -> Unit
void strbuilder_push(void* ptr, int32_t c) {
    StringBuilder* sb = (StringBuilder*)ptr;
    if (!sb)
        return;
    strbuilder_ensure_capacity(sb, 1);
    sb->data[sb->length++] = (char)c;
    sb->data[sb->length] = '\0';
}

// strbuilder_push_str(sb: *StringBuilder, s: Str) -> Unit
void strbuilder_push_str(void* ptr, const char* s) {
    StringBuilder* sb = (StringBuilder*)ptr;
    if (!sb || !s)
        return;
    int64_t len = (int64_t)strlen(s);
    strbuilder_ensure_capacity(sb, len);
    memcpy(sb->data + sb->length, s, (size_t)len);
    sb->length += len;
    sb->data[sb->length] = '\0';
}

// strbuilder_len(sb: *StringBuilder) -> I64
int64_t strbuilder_len(void* ptr) {
    StringBuilder* sb = (StringBuilder*)ptr;
    return sb ? sb->length : 0;
}

// strbuilder_capacity(sb: *StringBuilder) -> I64
int64_t strbuilder_capacity(void* ptr) {
    StringBuilder* sb = (StringBuilder*)ptr;
    return sb ? sb->capacity : 0;
}

// strbuilder_clear(sb: *StringBuilder) -> Unit
void strbuilder_clear(void* ptr) {
    StringBuilder* sb = (StringBuilder*)ptr;
    if (sb) {
        sb->length = 0;
        sb->data[0] = '\0';
    }
}

// strbuilder_to_str(sb: *StringBuilder) -> Str
// Returns a copy of the internal string (caller must free)
const char* strbuilder_to_str(void* ptr) {
    StringBuilder* sb = (StringBuilder*)ptr;
    if (!sb)
        return "";
    char* result = (char*)malloc((size_t)(sb->length + 1));
    if (!result)
        return "";
    memcpy(result, sb->data, (size_t)sb->length);
    result[sb->length] = '\0';
    return result;
}

// strbuilder_as_str(sb: *StringBuilder) -> Str
// Returns a reference to the internal string (do not free, valid until modified)
const char* strbuilder_as_str(void* ptr) {
    StringBuilder* sb = (StringBuilder*)ptr;
    return sb ? sb->data : "";
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

// str_find(s: Str, pattern: Str) -> I64 (returns -1 if not found)
int64_t str_find(const char* s, const char* pattern) {
    if (!s || !pattern)
        return -1;
    const char* found = strstr(s, pattern);
    if (!found)
        return -1;
    return (int64_t)(found - s);
}

// str_rfind(s: Str, pattern: Str) -> I64 (returns -1 if not found)
int64_t str_rfind(const char* s, const char* pattern) {
    if (!s || !pattern)
        return -1;
    size_t s_len = strlen(s);
    size_t p_len = strlen(pattern);
    if (p_len > s_len)
        return -1;

    // Search backwards
    for (size_t i = s_len - p_len + 1; i > 0; i--) {
        if (strncmp(s + i - 1, pattern, p_len) == 0) {
            return (int64_t)(i - 1);
        }
    }
    return -1;
}

// str_trim_start(s: Str) -> Str
const char* str_trim_start(const char* s) {
    if (!s)
        return "";
    while (isspace((unsigned char)*s))
        s++;
    size_t len = strlen(s);
    if (len >= sizeof(str_buffer))
        len = sizeof(str_buffer) - 1;
    memcpy(str_buffer, s, len);
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_trim_end(s: Str) -> Str
const char* str_trim_end(const char* s) {
    if (!s)
        return "";
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        len--;
    if (len >= sizeof(str_buffer))
        len = sizeof(str_buffer) - 1;
    memcpy(str_buffer, s, len);
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_parse_i64(s: Str) -> I64 (returns 0 on parse failure)
int64_t str_parse_i64(const char* s) {
    if (!s)
        return 0;
    char* endptr;
    int64_t result = strtoll(s, &endptr, 10);
    return result;
}

// str_replace(s: Str, from: Str, to: Str) -> Str
const char* str_replace(const char* s, const char* from, const char* to) {
    if (!s)
        return "";
    if (!from || !*from) {
        size_t len = strlen(s);
        if (len >= sizeof(str_buffer))
            len = sizeof(str_buffer) - 1;
        memcpy(str_buffer, s, len);
        str_buffer[len] = '\0';
        return str_buffer;
    }
    if (!to)
        to = "";

    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    size_t result_len = 0;
    const char* p = s;

    // First pass: calculate result length
    while (*p) {
        if (strncmp(p, from, from_len) == 0) {
            result_len += to_len;
            p += from_len;
        } else {
            result_len++;
            p++;
        }
    }

    if (result_len >= sizeof(str_buffer)) {
        // Truncate if too long
        result_len = sizeof(str_buffer) - 1;
    }

    // Second pass: build result
    p = s;
    char* out = str_buffer;
    char* end = str_buffer + result_len;

    while (*p && out < end) {
        if (strncmp(p, from, from_len) == 0) {
            size_t copy_len = to_len;
            if (out + copy_len > end)
                copy_len = end - out;
            memcpy(out, to, copy_len);
            out += copy_len;
            p += from_len;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    return str_buffer;
}

// str_split(s: Str, delimiter: Str) -> ptr (returns list of strings)
// Note: Returns a TmlList* containing string pointers
// Forward declaration of list functions from essential.c
typedef struct TmlList TmlList;
extern TmlList* list_create(int64_t initial_capacity);
extern void list_push(TmlList* list, int64_t value);

void* str_split(const char* s, const char* delimiter) {
    TmlList* result = list_create(4);
    if (!s || !delimiter || !*delimiter) {
        if (s) {
            list_push(result, (int64_t)s);
        }
        return result;
    }

    size_t delim_len = strlen(delimiter);
    const char* start = s;
    const char* found;

    while ((found = strstr(start, delimiter)) != NULL) {
        // Allocate and copy substring
        size_t part_len = found - start;
        char* part = (char*)malloc(part_len + 1);
        if (part) {
            memcpy(part, start, part_len);
            part[part_len] = '\0';
            list_push(result, (int64_t)part);
        }
        start = found + delim_len;
    }

    // Add remaining part
    if (*start) {
        size_t part_len = strlen(start);
        char* part = (char*)malloc(part_len + 1);
        if (part) {
            memcpy(part, start, part_len);
            part[part_len] = '\0';
            list_push(result, (int64_t)part);
        }
    }

    return result;
}

// str_chars(s: Str) -> ptr (returns list of character codes)
void* str_chars(const char* s) {
    TmlList* result = list_create(s ? strlen(s) : 0);
    if (!s)
        return result;

    while (*s) {
        list_push(result, (int64_t)(unsigned char)*s);
        s++;
    }
    return result;
}

// str_split_whitespace(s: Str) -> ptr (returns list of strings, splits on whitespace)
void* str_split_whitespace(const char* s) {
    TmlList* result = list_create(4);
    if (!s)
        return result;

    while (*s) {
        // Skip leading whitespace
        while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
            s++;
        if (!*s)
            break;
        // Find end of word
        const char* start = s;
        while (*s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
            s++;
        size_t len = s - start;
        char* part = (char*)malloc(len + 1);
        if (part) {
            memcpy(part, start, len);
            part[len] = '\0';
            list_push(result, (int64_t)part);
        }
    }
    return result;
}

// str_lines(s: Str) -> ptr (returns list of lines, handles \n and \r\n)
void* str_lines(const char* s) {
    TmlList* result = list_create(4);
    if (!s)
        return result;

    const char* start = s;
    while (*s) {
        if (*s == '\n') {
            size_t len = s - start;
            // Strip \r before \n
            if (len > 0 && start[len - 1] == '\r')
                len--;
            char* part = (char*)malloc(len + 1);
            if (part) {
                memcpy(part, start, len);
                part[len] = '\0';
                list_push(result, (int64_t)part);
            }
            s++;
            start = s;
        } else {
            s++;
        }
    }
    // Add remaining part (last line without newline)
    if (start != s || *start) {
        size_t len = s - start;
        if (len > 0) {
            char* part = (char*)malloc(len + 1);
            if (part) {
                memcpy(part, start, len);
                part[len] = '\0';
                list_push(result, (int64_t)part);
            }
        }
    }
    return result;
}

// str_replace_first(s: Str, from: Str, to: Str) -> Str
static char str_buffer2[16384];
const char* str_replace_first(const char* s, const char* from, const char* to) {
    if (!s)
        return "";
    if (!from || !*from) {
        size_t len = strlen(s);
        if (len >= sizeof(str_buffer2))
            len = sizeof(str_buffer2) - 1;
        memcpy(str_buffer2, s, len);
        str_buffer2[len] = '\0';
        return str_buffer2;
    }
    if (!to)
        to = "";

    const char* found = strstr(s, from);
    if (!found) {
        size_t len = strlen(s);
        if (len >= sizeof(str_buffer2))
            len = sizeof(str_buffer2) - 1;
        memcpy(str_buffer2, s, len);
        str_buffer2[len] = '\0';
        return str_buffer2;
    }

    size_t prefix_len = found - s;
    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    size_t suffix_len = strlen(found + from_len);
    size_t total = prefix_len + to_len + suffix_len;
    if (total >= sizeof(str_buffer2))
        total = sizeof(str_buffer2) - 1;

    char* out = str_buffer2;
    if (prefix_len > 0) {
        memcpy(out, s, prefix_len);
        out += prefix_len;
    }
    size_t copy_to = to_len;
    if (out + copy_to > str_buffer2 + sizeof(str_buffer2) - 1)
        copy_to = str_buffer2 + sizeof(str_buffer2) - 1 - out;
    memcpy(out, to, copy_to);
    out += copy_to;
    size_t copy_suffix = suffix_len;
    if (out + copy_suffix > str_buffer2 + sizeof(str_buffer2) - 1)
        copy_suffix = str_buffer2 + sizeof(str_buffer2) - 1 - out;
    memcpy(out, found + from_len, copy_suffix);
    out += copy_suffix;
    *out = '\0';
    return str_buffer2;
}

// str_repeat(s: Str, n: I32) -> Str
static char str_repeat_buffer[16384];
const char* str_repeat(const char* s, int32_t n) {
    if (!s || n <= 0) {
        str_repeat_buffer[0] = '\0';
        return str_repeat_buffer;
    }
    size_t len = strlen(s);
    size_t total = len * (size_t)n;
    if (total >= sizeof(str_repeat_buffer))
        total = sizeof(str_repeat_buffer) - 1;

    char* out = str_repeat_buffer;
    for (int32_t i = 0;
         i < n && (size_t)(out - str_repeat_buffer) + len < sizeof(str_repeat_buffer); i++) {
        memcpy(out, s, len);
        out += len;
    }
    *out = '\0';
    return str_repeat_buffer;
}

// str_parse_i32(s: Str) -> I32 (returns 0 on parse failure)
int32_t str_parse_i32(const char* s) {
    if (!s)
        return 0;
    char* endptr;
    long result = strtol(s, &endptr, 10);
    return (int32_t)result;
}

// str_parse_f64(s: Str) -> F64 (returns 0.0 on parse failure)
double str_parse_f64(const char* s) {
    if (!s)
        return 0.0;
    char* endptr;
    double result = strtod(s, &endptr);
    return result;
}

// str_join(parts: List[Str], separator: Str) -> Str
extern int64_t list_len(TmlList* list);
extern int64_t list_get(TmlList* list, int64_t index);

static char str_join_buffer[16384];
const char* str_join(TmlList* parts, const char* separator) {
    if (!parts) {
        str_join_buffer[0] = '\0';
        return str_join_buffer;
    }
    int64_t count = list_len(parts);
    if (count == 0) {
        str_join_buffer[0] = '\0';
        return str_join_buffer;
    }

    size_t sep_len = separator ? strlen(separator) : 0;
    char* out = str_join_buffer;
    char* end = str_join_buffer + sizeof(str_join_buffer) - 1;

    for (int64_t i = 0; i < count && out < end; i++) {
        if (i > 0 && sep_len > 0 && out + sep_len < end) {
            memcpy(out, separator, sep_len);
            out += sep_len;
        }
        const char* part = (const char*)list_get(parts, i);
        if (part) {
            size_t part_len = strlen(part);
            if (out + part_len > end)
                part_len = end - out;
            memcpy(out, part, part_len);
            out += part_len;
        }
    }
    *out = '\0';
    return str_join_buffer;
}

// str_as_bytes(s: Str) -> ptr (returns pointer to the string's byte data)
const void* str_as_bytes(const char* s) {
    return s ? s : "";
}