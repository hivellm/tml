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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <bcrypt.h>
#include <windows.h>
#pragma comment(lib, "bcrypt.lib")
#elif defined(__APPLE__)
#define TML_EXPORT __attribute__((visibility("default")))
#include <stdlib.h> // arc4random_buf
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
    if (size == 0)
        return 0;

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
    if (fd < 0)
        return -1;
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
    if (!buf)
        return NULL;
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
    if (!buf)
        return NULL;

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
    if (!buf || buf->length <= 0)
        return;
    fill_random_bytes(buf->data, buf->length);
}

TML_EXPORT void crypto_random_fill_range(void* handle, int64_t offset, int64_t size) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || offset < 0 || size <= 0)
        return;
    if (offset + size > buf->length)
        return;
    fill_random_bytes(buf->data + offset, size);
}

// ============================================================================
// Public API: Random integer generation
// ============================================================================

TML_EXPORT int64_t crypto_random_int(int64_t min, int64_t max) {
    if (min >= max)
        return min;

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
    return (float)(random_bits >> 8) / 16777216.0f; // 2^24
}

TML_EXPORT double crypto_random_f64(void) {
    uint64_t random_bits;
    fill_random_bytes((uint8_t*)&random_bits, sizeof(random_bits));
    // Convert to double in [0, 1) by dividing by 2^53
    return (double)(random_bits >> 11) / 9007199254740992.0; // 2^53
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
    if (!uuid)
        return "";

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

    if (!a || !b || a->length != b->length)
        return 0;

    volatile uint8_t result = 0;
    for (int64_t i = 0; i < a->length; i++) {
        result |= a->data[i] ^ b->data[i];
    }
    return result == 0 ? 1 : 0;
}

TML_EXPORT int32_t crypto_timing_safe_equal_str(const char* a, const char* b) {
    if (!a || !b)
        return 0;

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    if (len_a != len_b)
        return 0;

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

// ============================================================================
// Hash Functions Implementation (Windows CNG)
// ============================================================================

#ifdef _WIN32

// Hash context structure
typedef struct {
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_HASH_HANDLE hash;
    uint8_t* hash_object;
    DWORD hash_object_size;
    DWORD digest_size;
} HashContext;

// Get algorithm handle by name
static BCRYPT_ALG_HANDLE get_hash_algorithm(const char* name, DWORD* digest_size) {
    BCRYPT_ALG_HANDLE alg = NULL;
    LPCWSTR alg_id = NULL;

    if (strcmp(name, "md5") == 0) {
        alg_id = BCRYPT_MD5_ALGORITHM;
        *digest_size = 16;
    } else if (strcmp(name, "sha1") == 0) {
        alg_id = BCRYPT_SHA1_ALGORITHM;
        *digest_size = 20;
    } else if (strcmp(name, "sha256") == 0) {
        alg_id = BCRYPT_SHA256_ALGORITHM;
        *digest_size = 32;
    } else if (strcmp(name, "sha384") == 0) {
        alg_id = BCRYPT_SHA384_ALGORITHM;
        *digest_size = 48;
    } else if (strcmp(name, "sha512") == 0) {
        alg_id = BCRYPT_SHA512_ALGORITHM;
        *digest_size = 64;
    } else if (strcmp(name, "sha512-256") == 0) {
        // SHA-512/256 not directly available in CNG, use SHA-512 and truncate
        alg_id = BCRYPT_SHA512_ALGORITHM;
        *digest_size = 32; // Will truncate
    } else {
        return NULL;
    }

    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, alg_id, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return NULL;
    }
    return alg;
}

// One-shot hash of string
static TmlBuffer* hash_string(const char* data, const char* algorithm) {
    DWORD digest_size = 0;
    BCRYPT_ALG_HANDLE alg = get_hash_algorithm(algorithm, &digest_size);
    if (!alg)
        return NULL;

    // Handle SHA-512/256 special case
    int truncate_to_256 = (strcmp(algorithm, "sha512-256") == 0);
    DWORD actual_digest_size = truncate_to_256 ? 64 : digest_size;

    // Get hash object size
    DWORD hash_object_size = 0;
    DWORD result_size = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hash_object_size, sizeof(DWORD),
                      &result_size, 0);

    uint8_t* hash_object = (uint8_t*)malloc(hash_object_size);
    if (!hash_object) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    BCRYPT_HASH_HANDLE hash;
    NTSTATUS status = BCryptCreateHash(alg, &hash, hash_object, hash_object_size, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        free(hash_object);
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    // Hash the data
    size_t data_len = data ? strlen(data) : 0;
    if (data_len > 0) {
        BCryptHashData(hash, (PUCHAR)data, (ULONG)data_len, 0);
    }

    // Get digest
    uint8_t* digest_buf = (uint8_t*)malloc(actual_digest_size);
    if (!digest_buf) {
        BCryptDestroyHash(hash);
        free(hash_object);
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    BCryptFinishHash(hash, digest_buf, actual_digest_size, 0);

    // Create result buffer
    TmlBuffer* result = create_buffer(digest_size);
    if (result) {
        memcpy(result->data, digest_buf,
               digest_size); // Copy only digest_size bytes (truncate if needed)
        result->length = digest_size;
    }

    free(digest_buf);
    BCryptDestroyHash(hash);
    free(hash_object);
    BCryptCloseAlgorithmProvider(alg, 0);

    return result;
}

// One-shot hash of buffer
static TmlBuffer* hash_buffer(TmlBuffer* input, const char* algorithm) {
    if (!input)
        return NULL;

    DWORD digest_size = 0;
    BCRYPT_ALG_HANDLE alg = get_hash_algorithm(algorithm, &digest_size);
    if (!alg)
        return NULL;

    int truncate_to_256 = (strcmp(algorithm, "sha512-256") == 0);
    DWORD actual_digest_size = truncate_to_256 ? 64 : digest_size;

    DWORD hash_object_size = 0;
    DWORD result_size = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hash_object_size, sizeof(DWORD),
                      &result_size, 0);

    uint8_t* hash_object = (uint8_t*)malloc(hash_object_size);
    if (!hash_object) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    BCRYPT_HASH_HANDLE hash;
    NTSTATUS status = BCryptCreateHash(alg, &hash, hash_object, hash_object_size, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        free(hash_object);
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    if (input->length > 0) {
        BCryptHashData(hash, input->data, (ULONG)input->length, 0);
    }

    uint8_t* digest_buf = (uint8_t*)malloc(actual_digest_size);
    if (!digest_buf) {
        BCryptDestroyHash(hash);
        free(hash_object);
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    BCryptFinishHash(hash, digest_buf, actual_digest_size, 0);

    TmlBuffer* result = create_buffer(digest_size);
    if (result) {
        memcpy(result->data, digest_buf, digest_size);
        result->length = digest_size;
    }

    free(digest_buf);
    BCryptDestroyHash(hash);
    free(hash_object);
    BCryptCloseAlgorithmProvider(alg, 0);

    return result;
}

#else
// Non-Windows implementations (stubs for now)
static TmlBuffer* hash_string(const char* data, const char* algorithm) {
    (void)data;
    (void)algorithm;
    return NULL;
}

static TmlBuffer* hash_buffer(TmlBuffer* input, const char* algorithm) {
    (void)input;
    (void)algorithm;
    return NULL;
}
#endif

// ============================================================================
// Public API: One-shot hash functions
// ============================================================================

TML_EXPORT void* crypto_md5(const char* data) {
    return hash_string(data, "md5");
}

TML_EXPORT void* crypto_md5_bytes(void* handle) {
    return hash_buffer((TmlBuffer*)handle, "md5");
}

TML_EXPORT void* crypto_sha1(const char* data) {
    return hash_string(data, "sha1");
}

TML_EXPORT void* crypto_sha1_bytes(void* handle) {
    return hash_buffer((TmlBuffer*)handle, "sha1");
}

TML_EXPORT void* crypto_sha256(const char* data) {
    return hash_string(data, "sha256");
}

TML_EXPORT void* crypto_sha256_bytes(void* handle) {
    return hash_buffer((TmlBuffer*)handle, "sha256");
}

TML_EXPORT void* crypto_sha384(const char* data) {
    return hash_string(data, "sha384");
}

TML_EXPORT void* crypto_sha384_bytes(void* handle) {
    return hash_buffer((TmlBuffer*)handle, "sha384");
}

TML_EXPORT void* crypto_sha512(const char* data) {
    return hash_string(data, "sha512");
}

TML_EXPORT void* crypto_sha512_bytes(void* handle) {
    return hash_buffer((TmlBuffer*)handle, "sha512");
}

TML_EXPORT void* crypto_sha512_256(const char* data) {
    return hash_string(data, "sha512-256");
}

TML_EXPORT void* crypto_sha512_256_bytes(void* handle) {
    return hash_buffer((TmlBuffer*)handle, "sha512-256");
}

// ============================================================================
// Public API: Bytes to hex/base64 conversion
// ============================================================================

TML_EXPORT const char* crypto_bytes_to_hex(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || buf->length <= 0) {
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }

    char* hex = (char*)malloc(buf->length * 2 + 1);
    if (!hex)
        return "";

    static const char hex_chars[] = "0123456789abcdef";
    for (int64_t i = 0; i < buf->length; i++) {
        hex[i * 2] = hex_chars[buf->data[i] >> 4];
        hex[i * 2 + 1] = hex_chars[buf->data[i] & 0x0F];
    }
    hex[buf->length * 2] = '\0';

    return hex;
}

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

TML_EXPORT const char* crypto_bytes_to_base64(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || buf->length <= 0) {
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }

    size_t input_len = (size_t)buf->length;
    size_t output_len = 4 * ((input_len + 2) / 3);
    char* output = (char*)malloc(output_len + 1);
    if (!output)
        return "";

    size_t i, j;
    for (i = 0, j = 0; i < input_len;) {
        uint32_t octet_a = i < input_len ? buf->data[i++] : 0;
        uint32_t octet_b = i < input_len ? buf->data[i++] : 0;
        uint32_t octet_c = i < input_len ? buf->data[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = base64_chars[(triple >> 6) & 0x3F];
        output[j++] = base64_chars[triple & 0x3F];
    }

    // Add padding
    size_t mod = input_len % 3;
    if (mod == 1) {
        output[output_len - 1] = '=';
        output[output_len - 2] = '=';
    } else if (mod == 2) {
        output[output_len - 1] = '=';
    }

    output[output_len] = '\0';
    return output;
}

TML_EXPORT void* crypto_hex_to_bytes(const char* hex) {
    if (!hex)
        return NULL;

    size_t len = strlen(hex);
    if (len % 2 != 0)
        return NULL;

    TmlBuffer* buf = create_buffer(len / 2);
    if (!buf)
        return NULL;

    for (size_t i = 0; i < len; i += 2) {
        char high = hex[i];
        char low = hex[i + 1];

        uint8_t val = 0;
        if (high >= '0' && high <= '9')
            val = (high - '0') << 4;
        else if (high >= 'a' && high <= 'f')
            val = (high - 'a' + 10) << 4;
        else if (high >= 'A' && high <= 'F')
            val = (high - 'A' + 10) << 4;
        else {
            free(buf->data);
            free(buf);
            return NULL;
        }

        if (low >= '0' && low <= '9')
            val |= (low - '0');
        else if (low >= 'a' && low <= 'f')
            val |= (low - 'a' + 10);
        else if (low >= 'A' && low <= 'F')
            val |= (low - 'A' + 10);
        else {
            free(buf->data);
            free(buf);
            return NULL;
        }

        buf->data[i / 2] = val;
    }
    buf->length = len / 2;

    return buf;
}

static int base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

TML_EXPORT void* crypto_base64_to_bytes(const char* b64) {
    if (!b64)
        return NULL;

    size_t len = strlen(b64);
    if (len == 0)
        return create_buffer(0);

    // Calculate output length
    size_t padding = 0;
    if (len >= 1 && b64[len - 1] == '=')
        padding++;
    if (len >= 2 && b64[len - 2] == '=')
        padding++;

    size_t output_len = (len / 4) * 3 - padding;
    TmlBuffer* buf = create_buffer(output_len);
    if (!buf)
        return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < len;) {
        int a = (i < len && b64[i] != '=') ? base64_decode_char(b64[i++]) : 0;
        int b = (i < len && b64[i] != '=') ? base64_decode_char(b64[i++]) : 0;
        int c = (i < len && b64[i] != '=') ? base64_decode_char(b64[i++]) : 0;
        int d = (i < len && b64[i] != '=') ? base64_decode_char(b64[i++]) : 0;

        if (a < 0 || b < 0 || c < 0 || d < 0) {
            free(buf->data);
            free(buf);
            return NULL;
        }

        uint32_t triple = (a << 18) + (b << 12) + (c << 6) + d;

        if (j < output_len)
            buf->data[j++] = (triple >> 16) & 0xFF;
        if (j < output_len)
            buf->data[j++] = (triple >> 8) & 0xFF;
        if (j < output_len)
            buf->data[j++] = triple & 0xFF;
    }

    buf->length = output_len;
    return buf;
}

// ============================================================================
// Public API: Streaming hash functions
// ============================================================================

#ifdef _WIN32

TML_EXPORT void* crypto_hash_create(const char* algorithm) {
    DWORD digest_size = 0;
    BCRYPT_ALG_HANDLE alg = get_hash_algorithm(algorithm, &digest_size);
    if (!alg)
        return NULL;

    HashContext* ctx = (HashContext*)malloc(sizeof(HashContext));
    if (!ctx) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    ctx->alg = alg;
    ctx->digest_size = digest_size;

    DWORD result_size = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&ctx->hash_object_size, sizeof(DWORD),
                      &result_size, 0);

    ctx->hash_object = (uint8_t*)malloc(ctx->hash_object_size);
    if (!ctx->hash_object) {
        BCryptCloseAlgorithmProvider(alg, 0);
        free(ctx);
        return NULL;
    }

    NTSTATUS status =
        BCryptCreateHash(alg, &ctx->hash, ctx->hash_object, ctx->hash_object_size, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        free(ctx->hash_object);
        BCryptCloseAlgorithmProvider(alg, 0);
        free(ctx);
        return NULL;
    }

    return ctx;
}

TML_EXPORT void crypto_hash_update_str(void* handle, const char* data) {
    HashContext* ctx = (HashContext*)handle;
    if (!ctx || !data)
        return;

    size_t len = strlen(data);
    if (len > 0) {
        BCryptHashData(ctx->hash, (PUCHAR)data, (ULONG)len, 0);
    }
}

TML_EXPORT void crypto_hash_update_bytes(void* handle, void* data_handle) {
    HashContext* ctx = (HashContext*)handle;
    TmlBuffer* buf = (TmlBuffer*)data_handle;
    if (!ctx || !buf || buf->length <= 0)
        return;

    BCryptHashData(ctx->hash, buf->data, (ULONG)buf->length, 0);
}

TML_EXPORT void* crypto_hash_digest(void* handle) {
    HashContext* ctx = (HashContext*)handle;
    if (!ctx)
        return NULL;

    TmlBuffer* result = create_buffer(ctx->digest_size);
    if (!result)
        return NULL;

    BCryptFinishHash(ctx->hash, result->data, ctx->digest_size, 0);
    result->length = ctx->digest_size;

    return result;
}

TML_EXPORT void* crypto_hash_copy(void* handle) {
    HashContext* ctx = (HashContext*)handle;
    if (!ctx)
        return NULL;

    HashContext* new_ctx = (HashContext*)malloc(sizeof(HashContext));
    if (!new_ctx)
        return NULL;

    new_ctx->digest_size = ctx->digest_size;
    new_ctx->hash_object_size = ctx->hash_object_size;

    // Open new algorithm provider
    DWORD dummy_size;
    new_ctx->alg = get_hash_algorithm("sha256", &dummy_size); // Will be overwritten
    if (!new_ctx->alg) {
        free(new_ctx);
        return NULL;
    }

    new_ctx->hash_object = (uint8_t*)malloc(new_ctx->hash_object_size);
    if (!new_ctx->hash_object) {
        BCryptCloseAlgorithmProvider(new_ctx->alg, 0);
        free(new_ctx);
        return NULL;
    }

    NTSTATUS status = BCryptDuplicateHash(ctx->hash, &new_ctx->hash, new_ctx->hash_object,
                                          new_ctx->hash_object_size, 0);
    if (!BCRYPT_SUCCESS(status)) {
        free(new_ctx->hash_object);
        BCryptCloseAlgorithmProvider(new_ctx->alg, 0);
        free(new_ctx);
        return NULL;
    }

    return new_ctx;
}

TML_EXPORT void crypto_hash_destroy(void* handle) {
    HashContext* ctx = (HashContext*)handle;
    if (!ctx)
        return;

    if (ctx->hash)
        BCryptDestroyHash(ctx->hash);
    if (ctx->hash_object)
        free(ctx->hash_object);
    if (ctx->alg)
        BCryptCloseAlgorithmProvider(ctx->alg, 0);
    free(ctx);
}

#else
// Non-Windows stubs
TML_EXPORT void* crypto_hash_create(const char* algorithm) {
    (void)algorithm;
    return NULL;
}
TML_EXPORT void crypto_hash_update_str(void* handle, const char* data) {
    (void)handle;
    (void)data;
}
TML_EXPORT void crypto_hash_update_bytes(void* handle, void* data_handle) {
    (void)handle;
    (void)data_handle;
}
TML_EXPORT void* crypto_hash_digest(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void* crypto_hash_copy(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void crypto_hash_destroy(void* handle) {
    (void)handle;
}
#endif

// ============================================================================
// HMAC Functions Implementation
// ============================================================================

#ifdef _WIN32

// HMAC context structure
typedef struct {
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_HASH_HANDLE hash;
    uint8_t* hash_object;
    DWORD hash_object_size;
    DWORD digest_size;
} HmacContext;

// One-shot HMAC helper
static TmlBuffer* hmac_compute(const char* algorithm, const uint8_t* key, size_t key_len,
                               const uint8_t* data, size_t data_len) {
    DWORD digest_size = 0;
    BCRYPT_ALG_HANDLE alg = get_hash_algorithm(algorithm, &digest_size);
    if (!alg)
        return NULL;

    // Open with HMAC flag
    BCryptCloseAlgorithmProvider(alg, 0);

    LPCWSTR alg_id = NULL;
    if (strcmp(algorithm, "md5") == 0)
        alg_id = BCRYPT_MD5_ALGORITHM;
    else if (strcmp(algorithm, "sha1") == 0)
        alg_id = BCRYPT_SHA1_ALGORITHM;
    else if (strcmp(algorithm, "sha256") == 0)
        alg_id = BCRYPT_SHA256_ALGORITHM;
    else if (strcmp(algorithm, "sha384") == 0)
        alg_id = BCRYPT_SHA384_ALGORITHM;
    else if (strcmp(algorithm, "sha512") == 0)
        alg_id = BCRYPT_SHA512_ALGORITHM;
    else
        return NULL;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, alg_id, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status))
        return NULL;

    // Get hash object size
    DWORD hash_object_size = 0;
    DWORD result_size = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hash_object_size, sizeof(DWORD),
                      &result_size, 0);

    uint8_t* hash_object = (uint8_t*)malloc(hash_object_size);
    if (!hash_object) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    BCRYPT_HASH_HANDLE hash;
    status =
        BCryptCreateHash(alg, &hash, hash_object, hash_object_size, (PUCHAR)key, (ULONG)key_len, 0);
    if (!BCRYPT_SUCCESS(status)) {
        free(hash_object);
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    // Hash the data
    if (data_len > 0) {
        BCryptHashData(hash, (PUCHAR)data, (ULONG)data_len, 0);
    }

    // Get digest
    TmlBuffer* result = create_buffer(digest_size);
    if (!result) {
        BCryptDestroyHash(hash);
        free(hash_object);
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    BCryptFinishHash(hash, result->data, digest_size, 0);
    result->length = digest_size;

    BCryptDestroyHash(hash);
    free(hash_object);
    BCryptCloseAlgorithmProvider(alg, 0);

    return result;
}

// One-shot HMAC with string key and data
TML_EXPORT void* crypto_hmac_sha256(const char* key, const char* data) {
    size_t key_len = key ? strlen(key) : 0;
    size_t data_len = data ? strlen(data) : 0;
    return hmac_compute("sha256", (const uint8_t*)key, key_len, (const uint8_t*)data, data_len);
}

TML_EXPORT void* crypto_hmac_sha256_bytes(void* key_handle, void* data_handle) {
    TmlBuffer* key = (TmlBuffer*)key_handle;
    TmlBuffer* data = (TmlBuffer*)data_handle;
    if (!key || !data)
        return NULL;
    return hmac_compute("sha256", key->data, key->length, data->data, data->length);
}

TML_EXPORT void* crypto_hmac_sha512(const char* key, const char* data) {
    size_t key_len = key ? strlen(key) : 0;
    size_t data_len = data ? strlen(data) : 0;
    return hmac_compute("sha512", (const uint8_t*)key, key_len, (const uint8_t*)data, data_len);
}

TML_EXPORT void* crypto_hmac_sha512_bytes(void* key_handle, void* data_handle) {
    TmlBuffer* key = (TmlBuffer*)key_handle;
    TmlBuffer* data = (TmlBuffer*)data_handle;
    if (!key || !data)
        return NULL;
    return hmac_compute("sha512", key->data, key->length, data->data, data->length);
}

TML_EXPORT void* crypto_hmac_sha384(const char* key, const char* data) {
    size_t key_len = key ? strlen(key) : 0;
    size_t data_len = data ? strlen(data) : 0;
    return hmac_compute("sha384", (const uint8_t*)key, key_len, (const uint8_t*)data, data_len);
}

TML_EXPORT void* crypto_hmac_sha1(const char* key, const char* data) {
    size_t key_len = key ? strlen(key) : 0;
    size_t data_len = data ? strlen(data) : 0;
    return hmac_compute("sha1", (const uint8_t*)key, key_len, (const uint8_t*)data, data_len);
}

TML_EXPORT void* crypto_hmac_md5(const char* key, const char* data) {
    size_t key_len = key ? strlen(key) : 0;
    size_t data_len = data ? strlen(data) : 0;
    return hmac_compute("md5", (const uint8_t*)key, key_len, (const uint8_t*)data, data_len);
}

// Streaming HMAC
TML_EXPORT void* crypto_hmac_create(const char* algorithm, const char* key) {
    DWORD digest_size = 0;

    LPCWSTR alg_id = NULL;
    if (strcmp(algorithm, "md5") == 0) {
        alg_id = BCRYPT_MD5_ALGORITHM;
        digest_size = 16;
    } else if (strcmp(algorithm, "sha1") == 0) {
        alg_id = BCRYPT_SHA1_ALGORITHM;
        digest_size = 20;
    } else if (strcmp(algorithm, "sha256") == 0) {
        alg_id = BCRYPT_SHA256_ALGORITHM;
        digest_size = 32;
    } else if (strcmp(algorithm, "sha384") == 0) {
        alg_id = BCRYPT_SHA384_ALGORITHM;
        digest_size = 48;
    } else if (strcmp(algorithm, "sha512") == 0) {
        alg_id = BCRYPT_SHA512_ALGORITHM;
        digest_size = 64;
    } else
        return NULL;

    BCRYPT_ALG_HANDLE alg;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, alg_id, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status))
        return NULL;

    HmacContext* ctx = (HmacContext*)malloc(sizeof(HmacContext));
    if (!ctx) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return NULL;
    }

    ctx->alg = alg;
    ctx->digest_size = digest_size;

    DWORD result_size = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&ctx->hash_object_size, sizeof(DWORD),
                      &result_size, 0);

    ctx->hash_object = (uint8_t*)malloc(ctx->hash_object_size);
    if (!ctx->hash_object) {
        BCryptCloseAlgorithmProvider(alg, 0);
        free(ctx);
        return NULL;
    }

    size_t key_len = key ? strlen(key) : 0;
    status = BCryptCreateHash(alg, &ctx->hash, ctx->hash_object, ctx->hash_object_size, (PUCHAR)key,
                              (ULONG)key_len, 0);
    if (!BCRYPT_SUCCESS(status)) {
        free(ctx->hash_object);
        BCryptCloseAlgorithmProvider(alg, 0);
        free(ctx);
        return NULL;
    }

    return ctx;
}

TML_EXPORT void crypto_hmac_update_str(void* handle, const char* data) {
    HmacContext* ctx = (HmacContext*)handle;
    if (!ctx || !data)
        return;

    size_t len = strlen(data);
    if (len > 0) {
        BCryptHashData(ctx->hash, (PUCHAR)data, (ULONG)len, 0);
    }
}

TML_EXPORT void crypto_hmac_update_bytes(void* handle, void* data_handle) {
    HmacContext* ctx = (HmacContext*)handle;
    TmlBuffer* buf = (TmlBuffer*)data_handle;
    if (!ctx || !buf || buf->length <= 0)
        return;

    BCryptHashData(ctx->hash, buf->data, (ULONG)buf->length, 0);
}

TML_EXPORT void* crypto_hmac_digest(void* handle) {
    HmacContext* ctx = (HmacContext*)handle;
    if (!ctx)
        return NULL;

    TmlBuffer* result = create_buffer(ctx->digest_size);
    if (!result)
        return NULL;

    BCryptFinishHash(ctx->hash, result->data, ctx->digest_size, 0);
    result->length = ctx->digest_size;

    return result;
}

TML_EXPORT void crypto_hmac_destroy(void* handle) {
    HmacContext* ctx = (HmacContext*)handle;
    if (!ctx)
        return;

    if (ctx->hash)
        BCryptDestroyHash(ctx->hash);
    if (ctx->hash_object)
        free(ctx->hash_object);
    if (ctx->alg)
        BCryptCloseAlgorithmProvider(ctx->alg, 0);
    free(ctx);
}

#else
// Non-Windows stubs
TML_EXPORT void* crypto_hmac_sha256(const char* key, const char* data) {
    (void)key;
    (void)data;
    return NULL;
}
TML_EXPORT void* crypto_hmac_sha256_bytes(void* key, void* data) {
    (void)key;
    (void)data;
    return NULL;
}
TML_EXPORT void* crypto_hmac_sha512(const char* key, const char* data) {
    (void)key;
    (void)data;
    return NULL;
}
TML_EXPORT void* crypto_hmac_sha512_bytes(void* key, void* data) {
    (void)key;
    (void)data;
    return NULL;
}
TML_EXPORT void* crypto_hmac_sha384(const char* key, const char* data) {
    (void)key;
    (void)data;
    return NULL;
}
TML_EXPORT void* crypto_hmac_sha1(const char* key, const char* data) {
    (void)key;
    (void)data;
    return NULL;
}
TML_EXPORT void* crypto_hmac_md5(const char* key, const char* data) {
    (void)key;
    (void)data;
    return NULL;
}
TML_EXPORT void* crypto_hmac_create(const char* algorithm, const char* key) {
    (void)algorithm;
    (void)key;
    return NULL;
}
TML_EXPORT void crypto_hmac_update_str(void* handle, const char* data) {
    (void)handle;
    (void)data;
}
TML_EXPORT void crypto_hmac_update_bytes(void* handle, void* data_handle) {
    (void)handle;
    (void)data_handle;
}
TML_EXPORT void* crypto_hmac_digest(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void crypto_hmac_destroy(void* handle) {
    (void)handle;
}
#endif

// ============================================================================
// Cipher Functions (stubs - full implementation requires OpenSSL)
// ============================================================================

TML_EXPORT void* crypto_cipher_create(const char* algorithm, void* key, void* iv, int64_t encrypt) {
    (void)algorithm;
    (void)key;
    (void)iv;
    (void)encrypt;
    // Stub: Return NULL to indicate failure
    return NULL;
}

TML_EXPORT void crypto_cipher_set_aad(void* handle, void* aad) {
    (void)handle;
    (void)aad;
}

TML_EXPORT void crypto_cipher_set_aad_str(void* handle, const char* aad) {
    (void)handle;
    (void)aad;
}

TML_EXPORT void crypto_cipher_set_padding(void* handle, int32_t enabled) {
    (void)handle;
    (void)enabled;
}

TML_EXPORT void crypto_cipher_update_str(void* handle, const char* data, void* output) {
    (void)handle;
    (void)data;
    (void)output;
}

TML_EXPORT void crypto_cipher_update_bytes(void* handle, void* data, void* output) {
    (void)handle;
    (void)data;
    (void)output;
}

TML_EXPORT int32_t crypto_cipher_finalize(void* handle, void* output) {
    (void)handle;
    (void)output;
    return 0; // Return false to indicate failure
}

TML_EXPORT void* crypto_cipher_get_tag(void* handle) {
    (void)handle;
    return NULL;
}

TML_EXPORT void crypto_cipher_set_tag(void* handle, void* tag) {
    (void)handle;
    (void)tag;
}

TML_EXPORT void crypto_cipher_destroy(void* handle) {
    (void)handle;
}

// Additional conversion utilities for cipher (not already defined above)
TML_EXPORT const char* crypto_bytes_to_str(void* handle) {
    (void)handle;
    return "";
}

TML_EXPORT void* crypto_str_to_bytes(const char* s) {
    (void)s;
    return NULL;
}

TML_EXPORT void* crypto_concat_buffers3(void* a, void* b, void* c) {
    (void)a;
    (void)b;
    (void)c;
    return NULL;
}

TML_EXPORT void* crypto_buffer_slice(void* handle, int64_t offset, int64_t length) {
    (void)handle;
    (void)offset;
    (void)length;
    return NULL;
}

// ============================================================================
// Non-cryptographic Fast Hash Functions (FNV-1a, Murmur2)
// For ETags, checksums, and hash tables - NOT for security!
// ============================================================================

// FNV-1a 32-bit constants
#define FNV32_OFFSET_BASIS 2166136261u
#define FNV32_PRIME 16777619u

// FNV-1a 64-bit constants
#define FNV64_OFFSET_BASIS 14695981039346656037ull
#define FNV64_PRIME 1099511628211ull

// FNV-1a 32-bit hash of string
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

// FNV-1a 32-bit hash of buffer
TML_EXPORT uint32_t crypto_fnv1a32_bytes(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    uint32_t hash = FNV32_OFFSET_BASIS;
    if (buf && buf->data) {
        for (int64_t i = 0; i < buf->length; i++) {
            hash ^= buf->data[i];
            hash *= FNV32_PRIME;
        }
    }
    return hash;
}

// FNV-1a 64-bit hash of string
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

// FNV-1a 64-bit hash of buffer
TML_EXPORT uint64_t crypto_fnv1a64_bytes(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    uint64_t hash = FNV64_OFFSET_BASIS;
    if (buf && buf->data) {
        for (int64_t i = 0; i < buf->length; i++) {
            hash ^= buf->data[i];
            hash *= FNV64_PRIME;
        }
    }
    return hash;
}

// MurmurHash2 64-bit (MurmurHash64A for 64-bit platforms)
// Based on Austin Appleby's original implementation (public domain)
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
        h ^= (uint64_t)data8[6] << 48; // fallthrough
    case 6:
        h ^= (uint64_t)data8[5] << 40; // fallthrough
    case 5:
        h ^= (uint64_t)data8[4] << 32; // fallthrough
    case 4:
        h ^= (uint64_t)data8[3] << 24; // fallthrough
    case 3:
        h ^= (uint64_t)data8[2] << 16; // fallthrough
    case 2:
        h ^= (uint64_t)data8[1] << 8; // fallthrough
    case 1:
        h ^= (uint64_t)data8[0];
        h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

// MurmurHash2 64-bit from buffer
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
        h ^= (uint64_t)data8[6] << 48; // fallthrough
    case 6:
        h ^= (uint64_t)data8[5] << 40; // fallthrough
    case 5:
        h ^= (uint64_t)data8[4] << 32; // fallthrough
    case 4:
        h ^= (uint64_t)data8[3] << 24; // fallthrough
    case 3:
        h ^= (uint64_t)data8[2] << 16; // fallthrough
    case 2:
        h ^= (uint64_t)data8[1] << 8; // fallthrough
    case 1:
        h ^= (uint64_t)data8[0];
        h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

// MurmurHash2 32-bit (MurmurHash2 for 32-bit)
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
        h ^= data8[2] << 16; // fallthrough
    case 2:
        h ^= data8[1] << 8; // fallthrough
    case 1:
        h ^= data8[0];
        h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

// MurmurHash2 32-bit from buffer
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
        h ^= data8[2] << 16; // fallthrough
    case 2:
        h ^= data8[1] << 8; // fallthrough
    case 1:
        h ^= data8[0];
        h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

// Convert 32-bit hash to hex string
TML_EXPORT const char* crypto_u32_to_hex(uint32_t value) {
    char* hex = (char*)malloc(9); // 8 hex chars + null
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

// Convert 64-bit hash to hex string
TML_EXPORT const char* crypto_u64_to_hex(uint64_t value) {
    char* hex = (char*)malloc(17); // 16 hex chars + null
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
