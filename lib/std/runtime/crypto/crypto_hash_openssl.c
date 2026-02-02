/**
 * TML Crypto Runtime - Hash Functions (OpenSSL Implementation)
 *
 * Uses OpenSSL EVP API for hash operations.
 * Supports all hash algorithms including SHA-3, BLAKE2, etc.
 */

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)

#include "crypto_internal.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <string.h>

// ============================================================================
// Hash Context Structure
// ============================================================================

struct TmlHashContext {
    EVP_MD_CTX* ctx;
    const EVP_MD* md;
    char algorithm[32];
};

// ============================================================================
// Hash Algorithm Mapping
// ============================================================================

static const EVP_MD* get_evp_md(const char* algorithm) {
    if (!algorithm) return NULL;

    if (strcasecmp(algorithm, "md5") == 0) return EVP_md5();
    if (strcasecmp(algorithm, "sha1") == 0) return EVP_sha1();
    if (strcasecmp(algorithm, "sha256") == 0) return EVP_sha256();
    if (strcasecmp(algorithm, "sha384") == 0) return EVP_sha384();
    if (strcasecmp(algorithm, "sha512") == 0) return EVP_sha512();
    if (strcasecmp(algorithm, "sha512-256") == 0) return EVP_sha512_256();

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    if (strcasecmp(algorithm, "sha3-256") == 0) return EVP_sha3_256();
    if (strcasecmp(algorithm, "sha3-384") == 0) return EVP_sha3_384();
    if (strcasecmp(algorithm, "sha3-512") == 0) return EVP_sha3_512();
    if (strcasecmp(algorithm, "blake2b512") == 0) return EVP_blake2b512();
    if (strcasecmp(algorithm, "blake2s256") == 0) return EVP_blake2s256();
#endif

    // Try generic lookup
    return EVP_get_digestbyname(algorithm);
}

// ============================================================================
// Hash Context Implementation
// ============================================================================

TmlHashContext* hash_context_create(const char* algorithm) {
    const EVP_MD* md = get_evp_md(algorithm);
    if (!md) return NULL;

    TmlHashContext* ctx = (TmlHashContext*)calloc(1, sizeof(TmlHashContext));
    if (!ctx) return NULL;

    ctx->ctx = EVP_MD_CTX_new();
    if (!ctx->ctx) {
        free(ctx);
        return NULL;
    }

    ctx->md = md;
    strncpy(ctx->algorithm, algorithm, sizeof(ctx->algorithm) - 1);

    if (EVP_DigestInit_ex(ctx->ctx, md, NULL) != 1) {
        EVP_MD_CTX_free(ctx->ctx);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void hash_context_update(TmlHashContext* ctx, const uint8_t* data, size_t len) {
    if (!ctx || !ctx->ctx || !data || len == 0) return;
    EVP_DigestUpdate(ctx->ctx, data, len);
}

TmlBuffer* hash_context_digest(TmlHashContext* ctx) {
    if (!ctx || !ctx->ctx) return NULL;

    unsigned int digest_len = EVP_MD_size(ctx->md);
    TmlBuffer* result = tml_buffer_create(digest_len);
    if (!result) return NULL;

    if (EVP_DigestFinal_ex(ctx->ctx, result->data, &digest_len) != 1) {
        tml_buffer_destroy(result);
        return NULL;
    }

    result->len = digest_len;
    return result;
}

TmlHashContext* hash_context_copy(TmlHashContext* ctx) {
    if (!ctx) return NULL;

    TmlHashContext* copy = (TmlHashContext*)calloc(1, sizeof(TmlHashContext));
    if (!copy) return NULL;

    copy->ctx = EVP_MD_CTX_new();
    if (!copy->ctx) {
        free(copy);
        return NULL;
    }

    if (EVP_MD_CTX_copy_ex(copy->ctx, ctx->ctx) != 1) {
        EVP_MD_CTX_free(copy->ctx);
        free(copy);
        return NULL;
    }

    copy->md = ctx->md;
    strncpy(copy->algorithm, ctx->algorithm, sizeof(copy->algorithm) - 1);

    return copy;
}

void hash_context_destroy(TmlHashContext* ctx) {
    if (!ctx) return;
    if (ctx->ctx) {
        EVP_MD_CTX_free(ctx->ctx);
    }
    free(ctx);
}

// ============================================================================
// One-shot Hash Helper
// ============================================================================

static TmlBuffer* hash_oneshot(const char* algorithm, const uint8_t* data, size_t len) {
    const EVP_MD* md = get_evp_md(algorithm);
    if (!md) return NULL;

    unsigned int digest_len = EVP_MD_size(md);
    TmlBuffer* result = tml_buffer_create(digest_len);
    if (!result) return NULL;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        tml_buffer_destroy(result);
        return NULL;
    }

    int success = EVP_DigestInit_ex(ctx, md, NULL) == 1 &&
                  EVP_DigestUpdate(ctx, data, len) == 1 &&
                  EVP_DigestFinal_ex(ctx, result->data, &digest_len) == 1;

    EVP_MD_CTX_free(ctx);

    if (!success) {
        tml_buffer_destroy(result);
        return NULL;
    }

    result->len = digest_len;
    return result;
}

// ============================================================================
// Public API - One-shot Hash Functions
// ============================================================================

TmlBuffer* crypto_md5(const char* data) {
    return hash_oneshot("md5", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_md5_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("md5", NULL, 0);
    return hash_oneshot("md5", data->data, data->len);
}

TmlBuffer* crypto_sha1(const char* data) {
    return hash_oneshot("sha1", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha1_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("sha1", NULL, 0);
    return hash_oneshot("sha1", data->data, data->len);
}

TmlBuffer* crypto_sha256(const char* data) {
    return hash_oneshot("sha256", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha256_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("sha256", NULL, 0);
    return hash_oneshot("sha256", data->data, data->len);
}

TmlBuffer* crypto_sha384(const char* data) {
    return hash_oneshot("sha384", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha384_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("sha384", NULL, 0);
    return hash_oneshot("sha384", data->data, data->len);
}

TmlBuffer* crypto_sha512(const char* data) {
    return hash_oneshot("sha512", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha512_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("sha512", NULL, 0);
    return hash_oneshot("sha512", data->data, data->len);
}

TmlBuffer* crypto_sha512_256(const char* data) {
    return hash_oneshot("sha512-256", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha512_256_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("sha512-256", NULL, 0);
    return hash_oneshot("sha512-256", data->data, data->len);
}

TmlBuffer* crypto_sha3_256(const char* data) {
    return hash_oneshot("sha3-256", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha3_256_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("sha3-256", NULL, 0);
    return hash_oneshot("sha3-256", data->data, data->len);
}

TmlBuffer* crypto_sha3_384(const char* data) {
    return hash_oneshot("sha3-384", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha3_384_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("sha3-384", NULL, 0);
    return hash_oneshot("sha3-384", data->data, data->len);
}

TmlBuffer* crypto_sha3_512(const char* data) {
    return hash_oneshot("sha3-512", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha3_512_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("sha3-512", NULL, 0);
    return hash_oneshot("sha3-512", data->data, data->len);
}

TmlBuffer* crypto_blake2b512(const char* data) {
    return hash_oneshot("blake2b512", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_blake2b512_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("blake2b512", NULL, 0);
    return hash_oneshot("blake2b512", data->data, data->len);
}

TmlBuffer* crypto_blake2b_custom(const char* data, int64_t output_len) {
    // OpenSSL's BLAKE2b512 is fixed at 512 bits
    // For custom lengths, would need to use EVP_DigestInit with params
    // For now, just return NULL for unsupported lengths
    if (output_len != 64) return NULL;
    return crypto_blake2b512(data);
}

TmlBuffer* crypto_blake2s256(const char* data) {
    return hash_oneshot("blake2s256", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_blake2s256_bytes(TmlBuffer* data) {
    if (!data) return hash_oneshot("blake2s256", NULL, 0);
    return hash_oneshot("blake2s256", data->data, data->len);
}

// ============================================================================
// BLAKE3 - Not in OpenSSL, needs external implementation
// ============================================================================

TmlBuffer* crypto_blake3(const char* data) {
    (void)data;
    return NULL;  // Not available in OpenSSL
}

TmlBuffer* crypto_blake3_bytes(TmlBuffer* data) {
    (void)data;
    return NULL;
}

TmlBuffer* crypto_blake3_keyed(const char* data, TmlBuffer* key) {
    (void)data;
    (void)key;
    return NULL;
}

TmlBuffer* crypto_blake3_keyed_str(const char* key, const char* data) {
    (void)key;
    (void)data;
    return NULL;
}

TmlBuffer* crypto_blake3_keyed_bytes(TmlBuffer* key, TmlBuffer* data) {
    (void)key;
    (void)data;
    return NULL;
}

TmlBuffer* crypto_blake3_derive_key(const char* context, TmlBuffer* input) {
    (void)context;
    (void)input;
    return NULL;
}

// ============================================================================
// Streaming Hash API
// ============================================================================

void* crypto_hash_create(const char* algorithm) {
    return hash_context_create(algorithm);
}

void crypto_hash_update_str(void* ctx, const char* data) {
    if (!ctx || !data) return;
    hash_context_update((TmlHashContext*)ctx, (const uint8_t*)data, strlen(data));
}

void crypto_hash_update_bytes(void* ctx, TmlBuffer* data) {
    if (!ctx || !data) return;
    hash_context_update((TmlHashContext*)ctx, data->data, data->len);
}

TmlBuffer* crypto_hash_digest(void* ctx) {
    return hash_context_digest((TmlHashContext*)ctx);
}

void* crypto_hash_copy(void* ctx) {
    return hash_context_copy((TmlHashContext*)ctx);
}

void crypto_hash_destroy(void* ctx) {
    hash_context_destroy((TmlHashContext*)ctx);
}

#endif // __linux__ || __FreeBSD__ || __NetBSD__ || __OpenBSD__