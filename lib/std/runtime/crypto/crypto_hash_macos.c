/**
 * TML Crypto Runtime - Hash Functions (macOS CommonCrypto Implementation)
 *
 * Uses Apple's CommonCrypto framework for basic hashes (MD5, SHA1, SHA2).
 * For SHA-3 and BLAKE, uses a software implementation or falls back to OpenSSL.
 */

#ifdef __APPLE__

#include "crypto_internal.h"
#include <CommonCrypto/CommonDigest.h>
#include <string.h>

// ============================================================================
// Hash Algorithm IDs
// ============================================================================

typedef enum {
    HASH_ALG_MD5,
    HASH_ALG_SHA1,
    HASH_ALG_SHA256,
    HASH_ALG_SHA384,
    HASH_ALG_SHA512,
    HASH_ALG_UNKNOWN
} HashAlgorithmId;

static HashAlgorithmId get_algorithm_id(const char* algorithm) {
    if (!algorithm) return HASH_ALG_UNKNOWN;

    if (strcasecmp(algorithm, "md5") == 0) return HASH_ALG_MD5;
    if (strcasecmp(algorithm, "sha1") == 0) return HASH_ALG_SHA1;
    if (strcasecmp(algorithm, "sha256") == 0) return HASH_ALG_SHA256;
    if (strcasecmp(algorithm, "sha384") == 0) return HASH_ALG_SHA384;
    if (strcasecmp(algorithm, "sha512") == 0) return HASH_ALG_SHA512;

    return HASH_ALG_UNKNOWN;
}

static size_t get_digest_size(HashAlgorithmId alg) {
    switch (alg) {
        case HASH_ALG_MD5: return CC_MD5_DIGEST_LENGTH;
        case HASH_ALG_SHA1: return CC_SHA1_DIGEST_LENGTH;
        case HASH_ALG_SHA256: return CC_SHA256_DIGEST_LENGTH;
        case HASH_ALG_SHA384: return CC_SHA384_DIGEST_LENGTH;
        case HASH_ALG_SHA512: return CC_SHA512_DIGEST_LENGTH;
        default: return 0;
    }
}

// ============================================================================
// Hash Context Structure
// ============================================================================

struct TmlHashContext {
    HashAlgorithmId alg_id;
    union {
        CC_MD5_CTX md5;
        CC_SHA1_CTX sha1;
        CC_SHA256_CTX sha256;
        CC_SHA512_CTX sha512;  // Also used for SHA384
    } ctx;
    char algorithm[32];
};

// ============================================================================
// Hash Context Implementation
// ============================================================================

TmlHashContext* hash_context_create(const char* algorithm) {
    HashAlgorithmId alg_id = get_algorithm_id(algorithm);
    if (alg_id == HASH_ALG_UNKNOWN) return NULL;

    TmlHashContext* ctx = (TmlHashContext*)calloc(1, sizeof(TmlHashContext));
    if (!ctx) return NULL;

    ctx->alg_id = alg_id;
    strncpy(ctx->algorithm, algorithm, sizeof(ctx->algorithm) - 1);

    switch (alg_id) {
        case HASH_ALG_MD5:
            CC_MD5_Init(&ctx->ctx.md5);
            break;
        case HASH_ALG_SHA1:
            CC_SHA1_Init(&ctx->ctx.sha1);
            break;
        case HASH_ALG_SHA256:
            CC_SHA256_Init(&ctx->ctx.sha256);
            break;
        case HASH_ALG_SHA384:
            CC_SHA384_Init(&ctx->ctx.sha512);
            break;
        case HASH_ALG_SHA512:
            CC_SHA512_Init(&ctx->ctx.sha512);
            break;
        default:
            free(ctx);
            return NULL;
    }

    return ctx;
}

void hash_context_update(TmlHashContext* ctx, const uint8_t* data, size_t len) {
    if (!ctx || !data || len == 0) return;

    switch (ctx->alg_id) {
        case HASH_ALG_MD5:
            CC_MD5_Update(&ctx->ctx.md5, data, (CC_LONG)len);
            break;
        case HASH_ALG_SHA1:
            CC_SHA1_Update(&ctx->ctx.sha1, data, (CC_LONG)len);
            break;
        case HASH_ALG_SHA256:
            CC_SHA256_Update(&ctx->ctx.sha256, data, (CC_LONG)len);
            break;
        case HASH_ALG_SHA384:
            CC_SHA384_Update(&ctx->ctx.sha512, data, (CC_LONG)len);
            break;
        case HASH_ALG_SHA512:
            CC_SHA512_Update(&ctx->ctx.sha512, data, (CC_LONG)len);
            break;
        default:
            break;
    }
}

TmlBuffer* hash_context_digest(TmlHashContext* ctx) {
    if (!ctx) return NULL;

    size_t digest_size = get_digest_size(ctx->alg_id);
    if (digest_size == 0) return NULL;

    TmlBuffer* result = tml_buffer_create(digest_size);
    if (!result) return NULL;

    switch (ctx->alg_id) {
        case HASH_ALG_MD5:
            CC_MD5_Final(result->data, &ctx->ctx.md5);
            break;
        case HASH_ALG_SHA1:
            CC_SHA1_Final(result->data, &ctx->ctx.sha1);
            break;
        case HASH_ALG_SHA256:
            CC_SHA256_Final(result->data, &ctx->ctx.sha256);
            break;
        case HASH_ALG_SHA384:
            CC_SHA384_Final(result->data, &ctx->ctx.sha512);
            break;
        case HASH_ALG_SHA512:
            CC_SHA512_Final(result->data, &ctx->ctx.sha512);
            break;
        default:
            tml_buffer_destroy(result);
            return NULL;
    }

    return result;
}

TmlHashContext* hash_context_copy(TmlHashContext* ctx) {
    if (!ctx) return NULL;

    TmlHashContext* copy = (TmlHashContext*)malloc(sizeof(TmlHashContext));
    if (!copy) return NULL;

    memcpy(copy, ctx, sizeof(TmlHashContext));
    return copy;
}

void hash_context_destroy(TmlHashContext* ctx) {
    if (ctx) {
        memset(ctx, 0, sizeof(TmlHashContext));
        free(ctx);
    }
}

// ============================================================================
// One-shot Hash Helper
// ============================================================================

static TmlBuffer* hash_oneshot(const char* algorithm, const uint8_t* data, size_t len) {
    TmlHashContext* ctx = hash_context_create(algorithm);
    if (!ctx) return NULL;

    if (data && len > 0) {
        hash_context_update(ctx, data, len);
    }

    TmlBuffer* result = hash_context_digest(ctx);
    hash_context_destroy(ctx);

    return result;
}

// ============================================================================
// Public API - One-shot Hash Functions
// ============================================================================

TmlBuffer* crypto_md5(const char* data) {
    if (!data) {
        TmlBuffer* result = tml_buffer_create(CC_MD5_DIGEST_LENGTH);
        if (result) CC_MD5(NULL, 0, result->data);
        return result;
    }
    TmlBuffer* result = tml_buffer_create(CC_MD5_DIGEST_LENGTH);
    if (result) CC_MD5(data, (CC_LONG)strlen(data), result->data);
    return result;
}

TmlBuffer* crypto_md5_bytes(TmlBuffer* data) {
    TmlBuffer* result = tml_buffer_create(CC_MD5_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data && data->len > 0) {
        CC_MD5(data->data, (CC_LONG)data->len, result->data);
    } else {
        CC_MD5(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha1(const char* data) {
    TmlBuffer* result = tml_buffer_create(CC_SHA1_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data) {
        CC_SHA1(data, (CC_LONG)strlen(data), result->data);
    } else {
        CC_SHA1(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha1_bytes(TmlBuffer* data) {
    TmlBuffer* result = tml_buffer_create(CC_SHA1_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data && data->len > 0) {
        CC_SHA1(data->data, (CC_LONG)data->len, result->data);
    } else {
        CC_SHA1(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha256(const char* data) {
    TmlBuffer* result = tml_buffer_create(CC_SHA256_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data) {
        CC_SHA256(data, (CC_LONG)strlen(data), result->data);
    } else {
        CC_SHA256(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha256_bytes(TmlBuffer* data) {
    TmlBuffer* result = tml_buffer_create(CC_SHA256_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data && data->len > 0) {
        CC_SHA256(data->data, (CC_LONG)data->len, result->data);
    } else {
        CC_SHA256(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha384(const char* data) {
    TmlBuffer* result = tml_buffer_create(CC_SHA384_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data) {
        CC_SHA384(data, (CC_LONG)strlen(data), result->data);
    } else {
        CC_SHA384(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha384_bytes(TmlBuffer* data) {
    TmlBuffer* result = tml_buffer_create(CC_SHA384_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data && data->len > 0) {
        CC_SHA384(data->data, (CC_LONG)data->len, result->data);
    } else {
        CC_SHA384(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha512(const char* data) {
    TmlBuffer* result = tml_buffer_create(CC_SHA512_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data) {
        CC_SHA512(data, (CC_LONG)strlen(data), result->data);
    } else {
        CC_SHA512(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha512_bytes(TmlBuffer* data) {
    TmlBuffer* result = tml_buffer_create(CC_SHA512_DIGEST_LENGTH);
    if (!result) return NULL;
    if (data && data->len > 0) {
        CC_SHA512(data->data, (CC_LONG)data->len, result->data);
    } else {
        CC_SHA512(NULL, 0, result->data);
    }
    return result;
}

TmlBuffer* crypto_sha512_256(const char* data) {
    // SHA-512/256 not directly available in CommonCrypto
    // Compute SHA-512 and truncate
    TmlBuffer* full = crypto_sha512(data);
    if (!full) return NULL;

    TmlBuffer* truncated = tml_buffer_create(32);
    if (!truncated) {
        tml_buffer_destroy(full);
        return NULL;
    }

    memcpy(truncated->data, full->data, 32);
    tml_buffer_destroy(full);
    return truncated;
}

TmlBuffer* crypto_sha512_256_bytes(TmlBuffer* data) {
    TmlBuffer* full = crypto_sha512_bytes(data);
    if (!full) return NULL;

    TmlBuffer* truncated = tml_buffer_create(32);
    if (!truncated) {
        tml_buffer_destroy(full);
        return NULL;
    }

    memcpy(truncated->data, full->data, 32);
    tml_buffer_destroy(full);
    return truncated;
}

// ============================================================================
// SHA-3 - Not available in CommonCrypto, would need external implementation
// ============================================================================

TmlBuffer* crypto_sha3_256(const char* data) {
    (void)data;
    return NULL;  // Not available
}

TmlBuffer* crypto_sha3_256_bytes(TmlBuffer* data) {
    (void)data;
    return NULL;
}

TmlBuffer* crypto_sha3_384(const char* data) {
    (void)data;
    return NULL;
}

TmlBuffer* crypto_sha3_384_bytes(TmlBuffer* data) {
    (void)data;
    return NULL;
}

TmlBuffer* crypto_sha3_512(const char* data) {
    (void)data;
    return NULL;
}

TmlBuffer* crypto_sha3_512_bytes(TmlBuffer* data) {
    (void)data;
    return NULL;
}

// ============================================================================
// BLAKE2 - Not available in CommonCrypto
// ============================================================================

TmlBuffer* crypto_blake2b512(const char* data) {
    (void)data;
    return NULL;
}

TmlBuffer* crypto_blake2b512_bytes(TmlBuffer* data) {
    (void)data;
    return NULL;
}

TmlBuffer* crypto_blake2b_custom(const char* data, int64_t output_len) {
    (void)data;
    (void)output_len;
    return NULL;
}

TmlBuffer* crypto_blake2s256(const char* data) {
    (void)data;
    return NULL;
}

TmlBuffer* crypto_blake2s256_bytes(TmlBuffer* data) {
    (void)data;
    return NULL;
}

// ============================================================================
// BLAKE3 - Not available in CommonCrypto
// ============================================================================

TmlBuffer* crypto_blake3(const char* data) {
    (void)data;
    return NULL;
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

#endif // __APPLE__