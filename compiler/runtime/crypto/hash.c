/**
 * @file hash.c
 * @brief TML Runtime - Pure Hash Functions (no OpenSSL dependency)
 *
 * Contains FNV-1a, MurmurHash2, and hex conversion functions.
 * These are always linked — they have no external dependencies.
 */

#include <stdint.h>
#include <string.h>

// ============================================================================
// Platform export macro
// ============================================================================

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

extern void* mem_alloc(int64_t);

// Forward declaration for TmlBuffer (matching crypto_common.h)
typedef struct {
    uint8_t* data;
    int64_t length;
    int64_t capacity;
    int64_t read_pos;
} TmlBuffer;

// ============================================================================
// FNV-1a Hash
// ============================================================================

#define FNV32_OFFSET_BASIS 2166136261u
#define FNV32_PRIME 16777619u
#define FNV64_OFFSET_BASIS 14695981039346656037ull
#define FNV64_PRIME 1099511628211ull

TML_EXPORT uint32_t crypto_fnv1a32(const char* data) {
    uint32_t hash = FNV32_OFFSET_BASIS;
    if (data) {
        while (*data) {
            hash ^= (uint8_t)*data++;
            hash *= FNV32_PRIME;
        }
    }
    return hash;
}

TML_EXPORT uint32_t crypto_fnv1a32_bytes(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    uint32_t hash = FNV32_OFFSET_BASIS;
    if (buf && buf->data)
        for (int64_t i = 0; i < buf->length; i++) {
            hash ^= buf->data[i];
            hash *= FNV32_PRIME;
        }
    return hash;
}

TML_EXPORT uint64_t crypto_fnv1a64(const char* data) {
    uint64_t hash = FNV64_OFFSET_BASIS;
    if (data) {
        while (*data) {
            hash ^= (uint8_t)*data++;
            hash *= FNV64_PRIME;
        }
    }
    return hash;
}

TML_EXPORT uint64_t crypto_fnv1a64_bytes(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    uint64_t hash = FNV64_OFFSET_BASIS;
    if (buf && buf->data)
        for (int64_t i = 0; i < buf->length; i++) {
            hash ^= buf->data[i];
            hash *= FNV64_PRIME;
        }
    return hash;
}

// ============================================================================
// MurmurHash2
// ============================================================================

TML_EXPORT uint64_t crypto_murmur2_64(const char* data, uint64_t seed) {
    if (!data)
        return seed;
    const uint64_t m = 0xc6a4a7935bd1e995ull;
    const int r = 47;
    size_t len = strlen(data);
    uint64_t h = seed ^ (len * m);
    const uint64_t* data64 = (const uint64_t*)data;
    const uint64_t* end = data64 + (len / 8);
    while (data64 != end) {
        uint64_t k = *data64++;
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }
    const uint8_t* data8 = (const uint8_t*)data64;
    switch (len & 7) {
    case 7:
        h ^= (uint64_t)data8[6] << 48; /* fallthrough */
    case 6:
        h ^= (uint64_t)data8[5] << 40; /* fallthrough */
    case 5:
        h ^= (uint64_t)data8[4] << 32; /* fallthrough */
    case 4:
        h ^= (uint64_t)data8[3] << 24; /* fallthrough */
    case 3:
        h ^= (uint64_t)data8[2] << 16; /* fallthrough */
    case 2:
        h ^= (uint64_t)data8[1] << 8; /* fallthrough */
    case 1:
        h ^= (uint64_t)data8[0];
        h *= m;
    }
    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

TML_EXPORT uint64_t crypto_murmur2_64_bytes(void* handle, uint64_t seed) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || !buf->data || buf->length <= 0)
        return seed;
    const uint64_t m = 0xc6a4a7935bd1e995ull;
    const int r = 47;
    size_t len = (size_t)buf->length;
    uint64_t h = seed ^ (len * m);
    const uint64_t* data64 = (const uint64_t*)buf->data;
    const uint64_t* end = data64 + (len / 8);
    while (data64 != end) {
        uint64_t k = *data64++;
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }
    const uint8_t* data8 = (const uint8_t*)data64;
    switch (len & 7) {
    case 7:
        h ^= (uint64_t)data8[6] << 48; /* fallthrough */
    case 6:
        h ^= (uint64_t)data8[5] << 40; /* fallthrough */
    case 5:
        h ^= (uint64_t)data8[4] << 32; /* fallthrough */
    case 4:
        h ^= (uint64_t)data8[3] << 24; /* fallthrough */
    case 3:
        h ^= (uint64_t)data8[2] << 16; /* fallthrough */
    case 2:
        h ^= (uint64_t)data8[1] << 8; /* fallthrough */
    case 1:
        h ^= (uint64_t)data8[0];
        h *= m;
    }
    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}

TML_EXPORT uint32_t crypto_murmur2_32(const char* data, uint32_t seed) {
    if (!data)
        return seed;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    size_t len = strlen(data);
    uint32_t h = seed ^ (uint32_t)len;
    const uint8_t* data8 = (const uint8_t*)data;
    while (len >= 4) {
        uint32_t k = *(uint32_t*)data8;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data8 += 4;
        len -= 4;
    }
    switch (len) {
    case 3:
        h ^= data8[2] << 16; /* fallthrough */
    case 2:
        h ^= data8[1] << 8; /* fallthrough */
    case 1:
        h ^= data8[0];
        h *= m;
    }
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

TML_EXPORT uint32_t crypto_murmur2_32_bytes(void* handle, uint32_t seed) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || !buf->data || buf->length <= 0)
        return seed;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    size_t len = (size_t)buf->length;
    uint32_t h = seed ^ (uint32_t)len;
    const uint8_t* data8 = buf->data;
    while (len >= 4) {
        uint32_t k = *(uint32_t*)data8;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data8 += 4;
        len -= 4;
    }
    switch (len) {
    case 3:
        h ^= data8[2] << 16; /* fallthrough */
    case 2:
        h ^= data8[1] << 8; /* fallthrough */
    case 1:
        h ^= data8[0];
        h *= m;
    }
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

// ============================================================================
// Hex conversion
// ============================================================================

TML_EXPORT const char* crypto_u32_to_hex(uint32_t value) {
    char* hex = (char*)mem_alloc(9);
    if (!hex)
        return "";
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) {
        hex[i] = hex_chars[value & 0x0F];
        value >>= 4;
    }
    hex[8] = '\0';
    return hex;
}

TML_EXPORT const char* crypto_u64_to_hex(uint64_t value) {
    char* hex = (char*)mem_alloc(17);
    if (!hex)
        return "";
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 15; i >= 0; i--) {
        hex[i] = hex_chars[value & 0x0F];
        value >>= 4;
    }
    hex[16] = '\0';
    return hex;
}
