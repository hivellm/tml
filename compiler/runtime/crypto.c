/**
 * @file crypto.c
 * @brief TML Runtime - Cryptographic Functions Implementation
 *
 * Cross-platform cryptographically secure random number generation using:
 * - Windows: BCryptGenRandom (CNG)
 * - Linux: getrandom() syscall
 * - macOS: arc4random_buf
 * - Other Unix: /dev/urandom
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__)
#define TML_EXPORT __attribute__((visibility("default")))
#include <stdlib.h>  // arc4random_buf
#elif defined(__linux__)
#define TML_EXPORT __attribute__((visibility("default")))
#include <sys/random.h>
#else
#define TML_EXPORT __attribute__((visibility("default")))
#include <fcntl.h>
#include <unistd.h>
#endif

// ============================================================================
// Internal: Platform-specific random generation
// ============================================================================

static int fill_random_bytes(uint8_t* buffer, size_t size) {
    if (size == 0) return 0;

#ifdef _WIN32
    NTSTATUS status = BCryptGenRandom(NULL, buffer, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BCRYPT_SUCCESS(status) ? 0 : -1;
#elif defined(__APPLE__)
    arc4random_buf(buffer, size);
    return 0;
#elif defined(__linux__)
    ssize_t result = getrandom(buffer, size, 0);
    return (result == (ssize_t)size) ? 0 : -1;
#else
    // Fallback: /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;
    ssize_t bytes_read = read(fd, buffer, size);
    close(fd);
    return (bytes_read == (ssize_t)size) ? 0 : -1;
#endif
}

// ============================================================================
// Buffer structure (matching TML's std::collections::Buffer)
// ============================================================================

typedef struct {
    uint8_t* data;
    int64_t length;
    int64_t capacity;
    int64_t read_pos;
} TmlBuffer;

static TmlBuffer* create_buffer(int64_t capacity) {
    TmlBuffer* buf = (TmlBuffer*)malloc(sizeof(TmlBuffer));
    if (!buf) return NULL;
    buf->data = (uint8_t*)malloc(capacity > 0 ? capacity : 1);
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    buf->length = 0;
    buf->capacity = capacity > 0 ? capacity : 1;
    buf->read_pos = 0;
    return buf;
}

// ============================================================================
// Public API: Random byte generation
// ============================================================================

TML_EXPORT void* crypto_random_bytes(int64_t size) {
    if (size <= 0) {
        return create_buffer(0);
    }
    TmlBuffer* buf = create_buffer(size);
    if (!buf) return NULL;

    if (fill_random_bytes(buf->data, size) != 0) {
        free(buf->data);
        free(buf);
        return NULL;
    }
    buf->length = size;
    return buf;
}

TML_EXPORT void crypto_random_fill(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || buf->length <= 0) return;
    fill_random_bytes(buf->data, buf->length);
}

TML_EXPORT void crypto_random_fill_range(void* handle, int64_t offset, int64_t size) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || offset < 0 || size <= 0) return;
    if (offset + size > buf->length) return;
    fill_random_bytes(buf->data + offset, size);
}

// ============================================================================
// Public API: Random integer generation
// ============================================================================

TML_EXPORT int64_t crypto_random_int(int64_t min, int64_t max) {
    if (min >= max) return min;

    uint64_t range = (uint64_t)(max - min);
    uint64_t random_value;

    // Use rejection sampling for unbiased distribution
    uint64_t bucket_size = UINT64_MAX / range;
    uint64_t limit = bucket_size * range;

    do {
        fill_random_bytes((uint8_t*)&random_value, sizeof(random_value));
    } while (random_value >= limit);

    return min + (int64_t)(random_value / bucket_size);
}

TML_EXPORT uint8_t crypto_random_u8(void) {
    uint8_t value;
    fill_random_bytes(&value, sizeof(value));
    return value;
}

TML_EXPORT uint16_t crypto_random_u16(void) {
    uint16_t value;
    fill_random_bytes((uint8_t*)&value, sizeof(value));
    return value;
}

TML_EXPORT uint32_t crypto_random_u32(void) {
    uint32_t value;
    fill_random_bytes((uint8_t*)&value, sizeof(value));
    return value;
}

TML_EXPORT uint64_t crypto_random_u64(void) {
    uint64_t value;
    fill_random_bytes((uint8_t*)&value, sizeof(value));
    return value;
}

TML_EXPORT int32_t crypto_random_i32(void) {
    int32_t value;
    fill_random_bytes((uint8_t*)&value, sizeof(value));
    return value;
}

TML_EXPORT int64_t crypto_random_i64(void) {
    int64_t value;
    fill_random_bytes((uint8_t*)&value, sizeof(value));
    return value;
}

TML_EXPORT float crypto_random_f32(void) {
    uint32_t random_bits;
    fill_random_bytes((uint8_t*)&random_bits, sizeof(random_bits));
    // Convert to float in [0, 1) by dividing by 2^32
    return (float)(random_bits >> 8) / 16777216.0f;  // 2^24
}

TML_EXPORT double crypto_random_f64(void) {
    uint64_t random_bits;
    fill_random_bytes((uint8_t*)&random_bits, sizeof(random_bits));
    // Convert to double in [0, 1) by dividing by 2^53
    return (double)(random_bits >> 11) / 9007199254740992.0;  // 2^53
}

// ============================================================================
// Public API: UUID generation
// ============================================================================

TML_EXPORT const char* crypto_random_uuid(void) {
    uint8_t bytes[16];
    fill_random_bytes(bytes, 16);

    // Set version to 4 (random)
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    // Set variant to RFC 4122
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    char* uuid = (char*)malloc(37);
    if (!uuid) return "";

    static const char hex[] = "0123456789abcdef";
    char* p = uuid;

    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            *p++ = '-';
        }
        *p++ = hex[bytes[i] >> 4];
        *p++ = hex[bytes[i] & 0x0F];
    }
    *p = '\0';

    return uuid;
}

// ============================================================================
// Public API: Timing-safe comparison
// ============================================================================

TML_EXPORT int32_t crypto_timing_safe_equal(void* a_handle, void* b_handle) {
    TmlBuffer* a = (TmlBuffer*)a_handle;
    TmlBuffer* b = (TmlBuffer*)b_handle;

    if (!a || !b || a->length != b->length) return 0;

    volatile uint8_t result = 0;
    for (int64_t i = 0; i < a->length; i++) {
        result |= a->data[i] ^ b->data[i];
    }
    return result == 0 ? 1 : 0;
}

TML_EXPORT int32_t crypto_timing_safe_equal_str(const char* a, const char* b) {
    if (!a || !b) return 0;

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    if (len_a != len_b) return 0;

    volatile uint8_t result = 0;
    for (size_t i = 0; i < len_a; i++) {
        result |= (uint8_t)a[i] ^ (uint8_t)b[i];
    }
    return result == 0 ? 1 : 0;
}

// ============================================================================
// Public API: Prime number operations (stubs - full impl requires bignum)
// ============================================================================

TML_EXPORT void* crypto_generate_prime(int64_t bits) {
    // Stub: Full implementation requires bignum library
    // For now, return NULL to indicate failure
    (void)bits;
    return NULL;
}

TML_EXPORT void* crypto_generate_safe_prime(int64_t bits) {
    // Stub: Full implementation requires bignum library
    (void)bits;
    return NULL;
}

TML_EXPORT int32_t crypto_check_prime(void* handle) {
    // Stub: Full implementation requires bignum library
    (void)handle;
    return 0;
}

TML_EXPORT int32_t crypto_check_prime_rounds(void* handle, int64_t rounds) {
    // Stub: Full implementation requires bignum library
    (void)handle;
    (void)rounds;
    return 0;
}
