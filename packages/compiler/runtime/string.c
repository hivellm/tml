// TML Runtime - String Functions
// Matches: env_builtins_string.cpp

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Static buffer for string operations
static char str_buffer[4096];

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
// String conversion utilities
// ============================================================================

// i64_to_str(n: I64) -> Str
// Convert integer to string for string interpolation
static char i64_buffer[32];
const char* i64_to_str(int64_t n) {
    snprintf(i64_buffer, sizeof(i64_buffer), "%lld", (long long)n);
    return i64_buffer;
}

// f64_to_str(n: F64) -> Str
// Convert float to string for string interpolation
static char f64_buffer[64];
const char* f64_to_str(double n) {
    snprintf(f64_buffer, sizeof(f64_buffer), "%g", n);
    return f64_buffer;
}