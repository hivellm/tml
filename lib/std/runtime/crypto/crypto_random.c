/**
 * TML Crypto Runtime - Random Number Generation
 *
 * Platform-specific CSPRNG implementations:
 * - Windows: BCryptGenRandom (CNG)
 * - Linux: getrandom() syscall
 * - macOS: SecRandomCopyBytes
 * - Other Unix: /dev/urandom
 */

#include "crypto_internal.h"
#include <string.h>

// ============================================================================
// Windows Implementation (BCrypt)
// ============================================================================

#ifdef TML_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

bool random_bytes(uint8_t* buffer, size_t len) {
    if (!buffer || len == 0) return true;

    NTSTATUS status = BCryptGenRandom(
        NULL,                   // Use default provider
        buffer,
        (ULONG)len,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );

    return BCRYPT_SUCCESS(status);
}

#endif // TML_PLATFORM_WINDOWS

// ============================================================================
// macOS Implementation (Security Framework)
// ============================================================================

#ifdef TML_PLATFORM_MACOS

#include <Security/Security.h>

bool random_bytes(uint8_t* buffer, size_t len) {
    if (!buffer || len == 0) return true;

    int result = SecRandomCopyBytes(kSecRandomDefault, len, buffer);
    return result == errSecSuccess;
}

#endif // TML_PLATFORM_MACOS

// ============================================================================
// Linux Implementation (getrandom syscall)
// ============================================================================

#ifdef TML_PLATFORM_LINUX

#include <sys/random.h>
#include <errno.h>

bool random_bytes(uint8_t* buffer, size_t len) {
    if (!buffer || len == 0) return true;

    size_t total = 0;
    while (total < len) {
        ssize_t result = getrandom(buffer + total, len - total, 0);
        if (result < 0) {
            if (errno == EINTR) continue;  // Interrupted, retry
            return false;
        }
        total += (size_t)result;
    }
    return true;
}

#endif // TML_PLATFORM_LINUX

// ============================================================================
// BSD/Unix Implementation (/dev/urandom)
// ============================================================================

#if defined(TML_PLATFORM_BSD) || defined(TML_PLATFORM_UNIX)

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

bool random_bytes(uint8_t* buffer, size_t len) {
    if (!buffer || len == 0) return true;

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return false;

    size_t total = 0;
    while (total < len) {
        ssize_t result = read(fd, buffer + total, len - total);
        if (result < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return false;
        }
        if (result == 0) {  // EOF (shouldn't happen with urandom)
            close(fd);
            return false;
        }
        total += (size_t)result;
    }

    close(fd);
    return true;
}

#endif // TML_PLATFORM_BSD || TML_PLATFORM_UNIX

// ============================================================================
// Public API Implementation
// ============================================================================

TmlBuffer* crypto_random_bytes(int64_t size) {
    if (size <= 0) return tml_buffer_create(0);

    TmlBuffer* buf = tml_buffer_create((size_t)size);
    if (!buf) return NULL;

    if (!random_bytes(buf->data, buf->len)) {
        tml_buffer_destroy(buf);
        return NULL;
    }

    return buf;
}

void crypto_random_fill(TmlBuffer* buf) {
    if (!buf || buf->len == 0) return;
    random_bytes(buf->data, buf->len);
}

void crypto_random_fill_range(TmlBuffer* buf, int64_t offset, int64_t size) {
    if (!buf || offset < 0 || size <= 0) return;
    if ((size_t)(offset + size) > buf->len) return;

    random_bytes(buf->data + offset, (size_t)size);
}

int64_t crypto_random_int(int64_t min, int64_t max) {
    if (min >= max) return min;

    uint64_t range = (uint64_t)(max - min);

    // Use rejection sampling for uniform distribution
    uint64_t threshold = (UINT64_MAX - range + 1) % range;

    uint64_t r;
    do {
        uint8_t bytes[8];
        if (!random_bytes(bytes, 8)) return min;

        r = ((uint64_t)bytes[0] << 56) |
            ((uint64_t)bytes[1] << 48) |
            ((uint64_t)bytes[2] << 40) |
            ((uint64_t)bytes[3] << 32) |
            ((uint64_t)bytes[4] << 24) |
            ((uint64_t)bytes[5] << 16) |
            ((uint64_t)bytes[6] << 8) |
            ((uint64_t)bytes[7]);
    } while (r < threshold);

    return min + (int64_t)(r % range);
}

uint8_t crypto_random_u8(void) {
    uint8_t value;
    random_bytes(&value, 1);
    return value;
}

uint16_t crypto_random_u16(void) {
    uint8_t bytes[2];
    random_bytes(bytes, 2);
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

uint32_t crypto_random_u32(void) {
    uint8_t bytes[4];
    random_bytes(bytes, 4);
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

uint64_t crypto_random_u64(void) {
    uint8_t bytes[8];
    random_bytes(bytes, 8);
    return ((uint64_t)bytes[0]) |
           ((uint64_t)bytes[1] << 8) |
           ((uint64_t)bytes[2] << 16) |
           ((uint64_t)bytes[3] << 24) |
           ((uint64_t)bytes[4] << 32) |
           ((uint64_t)bytes[5] << 40) |
           ((uint64_t)bytes[6] << 48) |
           ((uint64_t)bytes[7] << 56);
}

int32_t crypto_random_i32(void) {
    return (int32_t)crypto_random_u32();
}

int64_t crypto_random_i64(void) {
    return (int64_t)crypto_random_u64();
}

float crypto_random_f32(void) {
    // Generate random 23-bit mantissa for [0, 1)
    uint32_t bits = crypto_random_u32() >> 9;  // Use upper 23 bits
    return (float)bits / (float)(1 << 23);
}

double crypto_random_f64(void) {
    // Generate random 52-bit mantissa for [0, 1)
    uint64_t bits = crypto_random_u64() >> 12;  // Use upper 52 bits
    return (double)bits / (double)((uint64_t)1 << 52);
}

char* crypto_random_uuid(void) {
    uint8_t bytes[16];
    if (!random_bytes(bytes, 16)) return NULL;

    // Set version (4) and variant (1)
    bytes[6] = (bytes[6] & 0x0F) | 0x40;  // Version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80;  // Variant 1

    return format_uuid(bytes);
}