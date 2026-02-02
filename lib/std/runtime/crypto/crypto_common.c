/**
 * TML Crypto Runtime - Common Utilities
 *
 * Platform-independent utilities and buffer management.
 */

#include "crypto_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Buffer Management
// ============================================================================

TmlBuffer* tml_buffer_create(size_t size) {
    TmlBuffer* buf = (TmlBuffer*)malloc(sizeof(TmlBuffer));
    if (!buf) return NULL;

    buf->data = (uint8_t*)malloc(size > 0 ? size : 1);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->len = size;
    buf->capacity = size > 0 ? size : 1;
    memset(buf->data, 0, buf->capacity);
    return buf;
}

TmlBuffer* tml_buffer_from_data(const uint8_t* data, size_t len) {
    TmlBuffer* buf = tml_buffer_create(len);
    if (!buf) return NULL;

    if (data && len > 0) {
        memcpy(buf->data, data, len);
    }
    return buf;
}

TmlBuffer* tml_buffer_from_string(const char* str) {
    if (!str) return tml_buffer_create(0);
    size_t len = strlen(str);
    return tml_buffer_from_data((const uint8_t*)str, len);
}

void tml_buffer_destroy(TmlBuffer* buf) {
    if (buf) {
        if (buf->data) {
            // Secure zero before freeing
            volatile uint8_t* p = buf->data;
            for (size_t i = 0; i < buf->capacity; i++) {
                p[i] = 0;
            }
            free(buf->data);
        }
        free(buf);
    }
}

uint8_t* tml_buffer_data(TmlBuffer* buf) {
    return buf ? buf->data : NULL;
}

size_t tml_buffer_len(TmlBuffer* buf) {
    return buf ? buf->len : 0;
}

void tml_buffer_resize(TmlBuffer* buf, size_t new_len) {
    if (!buf) return;

    if (new_len > buf->capacity) {
        size_t new_capacity = new_len * 2;
        uint8_t* new_data = (uint8_t*)realloc(buf->data, new_capacity);
        if (!new_data) return;
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
    buf->len = new_len;
}

void tml_buffer_append(TmlBuffer* buf, const uint8_t* data, size_t len) {
    if (!buf || !data || len == 0) return;

    size_t old_len = buf->len;
    tml_buffer_resize(buf, old_len + len);
    memcpy(buf->data + old_len, data, len);
}

TmlBuffer* tml_buffer_slice(TmlBuffer* buf, size_t offset, size_t len) {
    if (!buf || offset >= buf->len) return tml_buffer_create(0);

    size_t actual_len = (offset + len > buf->len) ? (buf->len - offset) : len;
    return tml_buffer_from_data(buf->data + offset, actual_len);
}

TmlBuffer* tml_buffer_concat(TmlBuffer* a, TmlBuffer* b) {
    size_t total = (a ? a->len : 0) + (b ? b->len : 0);
    TmlBuffer* result = tml_buffer_create(total);
    if (!result) return NULL;

    size_t offset = 0;
    if (a && a->len > 0) {
        memcpy(result->data, a->data, a->len);
        offset = a->len;
    }
    if (b && b->len > 0) {
        memcpy(result->data + offset, b->data, b->len);
    }
    return result;
}

TmlBuffer* crypto_concat_buffers3(TmlBuffer* a, TmlBuffer* b, TmlBuffer* c) {
    size_t total = (a ? a->len : 0) + (b ? b->len : 0) + (c ? c->len : 0);
    TmlBuffer* result = tml_buffer_create(total);
    if (!result) return NULL;

    size_t offset = 0;
    if (a && a->len > 0) {
        memcpy(result->data, a->data, a->len);
        offset += a->len;
    }
    if (b && b->len > 0) {
        memcpy(result->data + offset, b->data, b->len);
        offset += b->len;
    }
    if (c && c->len > 0) {
        memcpy(result->data + offset, c->data, c->len);
    }
    return result;
}

TmlBuffer* crypto_buffer_slice(TmlBuffer* buf, int64_t offset, int64_t len) {
    return tml_buffer_slice(buf, (size_t)offset, (size_t)len);
}

// ============================================================================
// Hex Encoding/Decoding
// ============================================================================

static const char HEX_CHARS[] = "0123456789abcdef";

char* crypto_bytes_to_hex(TmlBuffer* data) {
    if (!data || data->len == 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    size_t hex_len = data->len * 2 + 1;
    char* hex = (char*)malloc(hex_len);
    if (!hex) return NULL;

    for (size_t i = 0; i < data->len; i++) {
        hex[i * 2] = HEX_CHARS[(data->data[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = HEX_CHARS[data->data[i] & 0x0F];
    }
    hex[data->len * 2] = '\0';
    return hex;
}

static int hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

TmlBuffer* crypto_hex_to_bytes(const char* hex) {
    if (!hex) return NULL;

    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0) return NULL;  // Invalid hex string

    size_t byte_len = hex_len / 2;
    TmlBuffer* buf = tml_buffer_create(byte_len);
    if (!buf) return NULL;

    for (size_t i = 0; i < byte_len; i++) {
        int high = hex_char_to_int(hex[i * 2]);
        int low = hex_char_to_int(hex[i * 2 + 1]);
        if (high < 0 || low < 0) {
            tml_buffer_destroy(buf);
            return NULL;
        }
        buf->data[i] = (uint8_t)((high << 4) | low);
    }
    return buf;
}

// ============================================================================
// Base64 Encoding/Decoding
// ============================================================================

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char BASE64URL_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int base64_char_to_int(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    if (c == '=') return 0;  // Padding
    return -1;
}

char* crypto_bytes_to_base64(TmlBuffer* data) {
    if (!data || data->len == 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    size_t output_len = 4 * ((data->len + 2) / 3) + 1;
    char* output = (char*)malloc(output_len);
    if (!output) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < data->len; i += 3, j += 4) {
        uint32_t a = data->data[i];
        uint32_t b = (i + 1 < data->len) ? data->data[i + 1] : 0;
        uint32_t c = (i + 2 < data->len) ? data->data[i + 2] : 0;

        uint32_t triple = (a << 16) | (b << 8) | c;

        output[j] = BASE64_CHARS[(triple >> 18) & 0x3F];
        output[j + 1] = BASE64_CHARS[(triple >> 12) & 0x3F];
        output[j + 2] = (i + 1 < data->len) ? BASE64_CHARS[(triple >> 6) & 0x3F] : '=';
        output[j + 3] = (i + 2 < data->len) ? BASE64_CHARS[triple & 0x3F] : '=';
    }
    output[j] = '\0';
    return output;
}

TmlBuffer* crypto_base64_to_bytes(const char* b64) {
    if (!b64) return NULL;

    size_t input_len = strlen(b64);
    if (input_len == 0) return tml_buffer_create(0);
    if (input_len % 4 != 0) return NULL;  // Invalid base64

    size_t padding = 0;
    if (input_len > 0 && b64[input_len - 1] == '=') padding++;
    if (input_len > 1 && b64[input_len - 2] == '=') padding++;

    size_t output_len = (input_len / 4) * 3 - padding;
    TmlBuffer* buf = tml_buffer_create(output_len);
    if (!buf) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < input_len; i += 4) {
        int a = base64_char_to_int(b64[i]);
        int b = base64_char_to_int(b64[i + 1]);
        int c = base64_char_to_int(b64[i + 2]);
        int d = base64_char_to_int(b64[i + 3]);

        if (a < 0 || b < 0 || c < 0 || d < 0) {
            tml_buffer_destroy(buf);
            return NULL;
        }

        uint32_t triple = (a << 18) | (b << 12) | (c << 6) | d;

        if (j < output_len) buf->data[j++] = (triple >> 16) & 0xFF;
        if (j < output_len) buf->data[j++] = (triple >> 8) & 0xFF;
        if (j < output_len) buf->data[j++] = triple & 0xFF;
    }
    return buf;
}

char* crypto_bytes_to_base64url(TmlBuffer* data) {
    char* b64 = crypto_bytes_to_base64(data);
    if (!b64) return NULL;

    // Convert + to -, / to _, remove padding
    for (char* p = b64; *p; p++) {
        if (*p == '+') *p = '-';
        else if (*p == '/') *p = '_';
    }

    // Remove trailing '='
    size_t len = strlen(b64);
    while (len > 0 && b64[len - 1] == '=') {
        b64[--len] = '\0';
    }

    return b64;
}

TmlBuffer* crypto_base64url_to_bytes(const char* b64url) {
    if (!b64url) return NULL;

    size_t input_len = strlen(b64url);

    // Calculate padding needed
    size_t padding = (4 - (input_len % 4)) % 4;
    size_t padded_len = input_len + padding;

    char* padded = (char*)malloc(padded_len + 1);
    if (!padded) return NULL;

    // Copy and convert - to +, _ to /
    for (size_t i = 0; i < input_len; i++) {
        char c = b64url[i];
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
        padded[i] = c;
    }

    // Add padding
    for (size_t i = input_len; i < padded_len; i++) {
        padded[i] = '=';
    }
    padded[padded_len] = '\0';

    TmlBuffer* result = crypto_base64_to_bytes(padded);
    free(padded);
    return result;
}

// ============================================================================
// String Utilities
// ============================================================================

TmlBuffer* crypto_str_to_bytes(const char* str) {
    return tml_buffer_from_string(str);
}

char* crypto_bytes_to_str(TmlBuffer* data) {
    if (!data || data->len == 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }

    char* str = (char*)malloc(data->len + 1);
    if (!str) return NULL;

    memcpy(str, data->data, data->len);
    str[data->len] = '\0';
    return str;
}

// ============================================================================
// Timing-Safe Comparison
// ============================================================================

bool crypto_timing_safe_equal(TmlBuffer* a, TmlBuffer* b) {
    if (!a || !b) return false;
    if (a->len != b->len) return false;

    volatile uint8_t result = 0;
    for (size_t i = 0; i < a->len; i++) {
        result |= a->data[i] ^ b->data[i];
    }
    return result == 0;
}

bool crypto_timing_safe_equal_str(const char* a, const char* b) {
    if (!a || !b) return false;

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    if (len_a != len_b) return false;

    volatile uint8_t result = 0;
    for (size_t i = 0; i < len_a; i++) {
        result |= (uint8_t)a[i] ^ (uint8_t)b[i];
    }
    return result == 0;
}

// ============================================================================
// JWK Utilities
// ============================================================================

char* crypto_jwk_extract_k(const char* jwk) {
    // Simple JSON parsing for "k" field
    // Format: {..., "k": "base64url_value", ...}
    if (!jwk) return NULL;

    const char* k_start = strstr(jwk, "\"k\"");
    if (!k_start) return NULL;

    k_start = strchr(k_start + 3, '"');
    if (!k_start) return NULL;
    k_start++;  // Skip opening quote

    const char* k_end = strchr(k_start, '"');
    if (!k_end) return NULL;

    size_t len = k_end - k_start;
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, k_start, len);
    result[len] = '\0';
    return result;
}

// ============================================================================
// UUID Generation (platform-independent formatting)
// ============================================================================

char* format_uuid(const uint8_t* bytes) {
    // UUID format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // where y is 8, 9, a, or b
    char* uuid = (char*)malloc(37);
    if (!uuid) return NULL;

    snprintf(uuid, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        (bytes[6] & 0x0F) | 0x40,  // Version 4
        bytes[7],
        (bytes[8] & 0x3F) | 0x80,  // Variant 1
        bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

    return uuid;
}

// ============================================================================
// List Management for Algorithm Lists
// ============================================================================

TmlList* tml_list_create(size_t initial_capacity) {
    TmlList* list = (TmlList*)malloc(sizeof(TmlList));
    if (!list) return NULL;

    list->items = (char**)malloc(sizeof(char*) * initial_capacity);
    if (!list->items) {
        free(list);
        return NULL;
    }

    list->len = 0;
    list->capacity = initial_capacity;
    return list;
}

void tml_list_destroy(TmlList* list) {
    if (list) {
        for (size_t i = 0; i < list->len; i++) {
            free(list->items[i]);
        }
        free(list->items);
        free(list);
    }
}

void tml_list_push(TmlList* list, const char* item) {
    if (!list || !item) return;

    if (list->len >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        char** new_items = (char**)realloc(list->items, sizeof(char*) * new_capacity);
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->len] = strdup(item);
    list->len++;
}