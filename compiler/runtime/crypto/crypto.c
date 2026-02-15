/**
 * @file crypto.c
 * @brief TML Runtime - Cryptographic Functions Implementation
 *
 * Unified OpenSSL-based implementation for all platforms.
 * All operations (random, hash, HMAC, cipher, prime) use OpenSSL.
 */

#include "crypto_common.h"

#ifdef TML_HAS_OPENSSL
#include <openssl/rand.h>
#endif

// ============================================================================
// Random generation (OpenSSL RAND_bytes)
// ============================================================================

static int fill_random_bytes(uint8_t* buffer, size_t size) {
    if (size == 0)
        return 0;
#ifdef TML_HAS_OPENSSL
    return RAND_bytes(buffer, (int)size) == 1 ? 0 : -1;
#else
    (void)buffer;
    return -1;
#endif
}

// ============================================================================
// Buffer helper (uses TmlBuffer from crypto_common.h)
// ============================================================================

static TmlBuffer* create_buffer(int64_t capacity) {
    return tml_create_buffer(capacity);
}

// ============================================================================
// Public API: Random byte generation
// ============================================================================

TML_EXPORT void* crypto_random_bytes(int64_t size) {
    if (size <= 0)
        return create_buffer(0);
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
    return (float)(random_bits >> 8) / 16777216.0f;
}

TML_EXPORT double crypto_random_f64(void) {
    uint64_t random_bits;
    fill_random_bytes((uint8_t*)&random_bits, sizeof(random_bits));
    return (double)(random_bits >> 11) / 9007199254740992.0;
}

// ============================================================================
// Public API: UUID generation
// ============================================================================

TML_EXPORT const char* crypto_random_uuid(void) {
    uint8_t bytes[16];
    fill_random_bytes(bytes, 16);
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    char* uuid = (char*)malloc(37);
    if (!uuid)
        return "";
    static const char hex[] = "0123456789abcdef";
    char* p = uuid;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            *p++ = '-';
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
    for (int64_t i = 0; i < a->length; i++)
        result |= a->data[i] ^ b->data[i];
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
    for (size_t i = 0; i < len_a; i++)
        result |= (uint8_t)a[i] ^ (uint8_t)b[i];
    return result == 0 ? 1 : 0;
}

// ============================================================================
// Hash Functions (OpenSSL EVP)
// ============================================================================

#ifdef TML_HAS_OPENSSL

// Map TML hash names to OpenSSL names
static const char* map_hash_name(const char* name) {
    if (!name)
        return NULL;
    if (strcmp(name, "sha512-256") == 0)
        return "sha512-256";
    if (strcmp(name, "sha3-256") == 0)
        return "sha3-256";
    if (strcmp(name, "sha3-384") == 0)
        return "sha3-384";
    if (strcmp(name, "sha3-512") == 0)
        return "sha3-512";
    if (strcmp(name, "blake2b512") == 0)
        return "blake2b512";
    if (strcmp(name, "blake2s256") == 0)
        return "blake2s256";
    return name; // md5, sha1, sha256, sha384, sha512 are the same
}

static TmlBuffer* hash_string(const char* data, const char* algorithm) {
    const char* ossl_name = map_hash_name(algorithm);
    const EVP_MD* md = EVP_get_digestbyname(ossl_name);
    if (!md)
        return NULL;

    unsigned int digest_len = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    size_t data_len = data ? strlen(data) : 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        return NULL;

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        (data_len > 0 && EVP_DigestUpdate(ctx, data, data_len) != 1) ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    EVP_MD_CTX_free(ctx);

    TmlBuffer* result = create_buffer(digest_len);
    if (result) {
        memcpy(result->data, digest, digest_len);
        result->length = digest_len;
    }
    return result;
}

static TmlBuffer* hash_buffer(TmlBuffer* input, const char* algorithm) {
    if (!input)
        return NULL;

    const char* ossl_name = map_hash_name(algorithm);
    const EVP_MD* md = EVP_get_digestbyname(ossl_name);
    if (!md)
        return NULL;

    unsigned int digest_len = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        return NULL;

    if (EVP_DigestInit_ex(ctx, md, NULL) != 1 ||
        (input->length > 0 && EVP_DigestUpdate(ctx, input->data, input->length) != 1) ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return NULL;
    }
    EVP_MD_CTX_free(ctx);

    TmlBuffer* result = create_buffer(digest_len);
    if (result) {
        memcpy(result->data, digest, digest_len);
        result->length = digest_len;
    }
    return result;
}

#else
// No OpenSSL — stubs
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
// Public API: Streaming hash (OpenSSL EVP_MD_CTX)
// ============================================================================

#ifdef TML_HAS_OPENSSL

typedef struct {
    EVP_MD_CTX* ctx;
    unsigned int digest_size;
} HashContext;

TML_EXPORT void* crypto_hash_create(const char* algorithm) {
    const char* ossl_name = map_hash_name(algorithm);
    const EVP_MD* md = EVP_get_digestbyname(ossl_name);
    if (!md)
        return NULL;

    HashContext* hctx = (HashContext*)malloc(sizeof(HashContext));
    if (!hctx)
        return NULL;

    hctx->ctx = EVP_MD_CTX_new();
    if (!hctx->ctx) {
        free(hctx);
        return NULL;
    }

    if (EVP_DigestInit_ex(hctx->ctx, md, NULL) != 1) {
        EVP_MD_CTX_free(hctx->ctx);
        free(hctx);
        return NULL;
    }

    hctx->digest_size = EVP_MD_get_size(md);
    return hctx;
}

TML_EXPORT void crypto_hash_update_str(void* handle, const char* data) {
    HashContext* hctx = (HashContext*)handle;
    if (!hctx || !data)
        return;
    size_t len = strlen(data);
    if (len > 0)
        EVP_DigestUpdate(hctx->ctx, data, len);
}

TML_EXPORT void crypto_hash_update_bytes(void* handle, void* data_handle) {
    HashContext* hctx = (HashContext*)handle;
    TmlBuffer* buf = (TmlBuffer*)data_handle;
    if (!hctx || !buf || buf->length <= 0)
        return;
    EVP_DigestUpdate(hctx->ctx, buf->data, buf->length);
}

TML_EXPORT void* crypto_hash_digest(void* handle) {
    HashContext* hctx = (HashContext*)handle;
    if (!hctx)
        return NULL;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (EVP_DigestFinal_ex(hctx->ctx, digest, &digest_len) != 1)
        return NULL;

    TmlBuffer* result = create_buffer(digest_len);
    if (result) {
        memcpy(result->data, digest, digest_len);
        result->length = digest_len;
    }
    return result;
}

TML_EXPORT void* crypto_hash_copy(void* handle) {
    HashContext* hctx = (HashContext*)handle;
    if (!hctx)
        return NULL;

    HashContext* new_hctx = (HashContext*)malloc(sizeof(HashContext));
    if (!new_hctx)
        return NULL;

    new_hctx->ctx = EVP_MD_CTX_new();
    if (!new_hctx->ctx) {
        free(new_hctx);
        return NULL;
    }

    if (EVP_MD_CTX_copy_ex(new_hctx->ctx, hctx->ctx) != 1) {
        EVP_MD_CTX_free(new_hctx->ctx);
        free(new_hctx);
        return NULL;
    }

    new_hctx->digest_size = hctx->digest_size;
    return new_hctx;
}

TML_EXPORT void crypto_hash_destroy(void* handle) {
    HashContext* hctx = (HashContext*)handle;
    if (!hctx)
        return;
    if (hctx->ctx)
        EVP_MD_CTX_free(hctx->ctx);
    free(hctx);
}

#else
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
// HMAC Functions (OpenSSL 3.0 EVP_MAC)
// ============================================================================

#ifdef TML_HAS_OPENSSL

static TmlBuffer* hmac_compute(const char* algorithm, const uint8_t* key, size_t key_len,
                               const uint8_t* data, size_t data_len) {
    const char* ossl_name = map_hash_name(algorithm);

    EVP_MAC* mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (!mac)
        return NULL;

    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    if (!ctx) {
        EVP_MAC_free(mac);
        return NULL;
    }

    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", (char*)ossl_name, 0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_MAC_init(ctx, key, key_len, params) != 1) {
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        return NULL;
    }

    if (data_len > 0 && EVP_MAC_update(ctx, data, data_len) != 1) {
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        return NULL;
    }

    size_t result_len = 0;
    if (EVP_MAC_final(ctx, NULL, &result_len, 0) != 1) {
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        return NULL;
    }

    TmlBuffer* result = create_buffer((int64_t)result_len);
    if (!result) {
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        return NULL;
    }

    if (EVP_MAC_final(ctx, result->data, &result_len, result_len) != 1) {
        free(result->data);
        free(result);
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        return NULL;
    }

    result->length = (int64_t)result_len;
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
    return result;
}

#else
static TmlBuffer* hmac_compute(const char* algorithm, const uint8_t* key, size_t key_len,
                               const uint8_t* data, size_t data_len) {
    (void)algorithm;
    (void)key;
    (void)key_len;
    (void)data;
    (void)data_len;
    return NULL;
}
#endif

// One-shot HMAC with string key and data
TML_EXPORT void* crypto_hmac_sha256(const char* key, const char* data) {
    return hmac_compute("sha256", (const uint8_t*)key, key ? strlen(key) : 0, (const uint8_t*)data,
                        data ? strlen(data) : 0);
}
TML_EXPORT void* crypto_hmac_sha256_bytes(void* key_handle, void* data_handle) {
    TmlBuffer* k = (TmlBuffer*)key_handle;
    TmlBuffer* d = (TmlBuffer*)data_handle;
    if (!k || !d)
        return NULL;
    return hmac_compute("sha256", k->data, k->length, d->data, d->length);
}
TML_EXPORT void* crypto_hmac_sha512(const char* key, const char* data) {
    return hmac_compute("sha512", (const uint8_t*)key, key ? strlen(key) : 0, (const uint8_t*)data,
                        data ? strlen(data) : 0);
}
TML_EXPORT void* crypto_hmac_sha512_bytes(void* key_handle, void* data_handle) {
    TmlBuffer* k = (TmlBuffer*)key_handle;
    TmlBuffer* d = (TmlBuffer*)data_handle;
    if (!k || !d)
        return NULL;
    return hmac_compute("sha512", k->data, k->length, d->data, d->length);
}
TML_EXPORT void* crypto_hmac_sha384(const char* key, const char* data) {
    return hmac_compute("sha384", (const uint8_t*)key, key ? strlen(key) : 0, (const uint8_t*)data,
                        data ? strlen(data) : 0);
}
TML_EXPORT void* crypto_hmac_sha1(const char* key, const char* data) {
    return hmac_compute("sha1", (const uint8_t*)key, key ? strlen(key) : 0, (const uint8_t*)data,
                        data ? strlen(data) : 0);
}
TML_EXPORT void* crypto_hmac_md5(const char* key, const char* data) {
    return hmac_compute("md5", (const uint8_t*)key, key ? strlen(key) : 0, (const uint8_t*)data,
                        data ? strlen(data) : 0);
}

// ============================================================================
// Streaming HMAC (OpenSSL 3.0 EVP_MAC)
// ============================================================================

#ifdef TML_HAS_OPENSSL

typedef struct {
    EVP_MAC_CTX* ctx;
    EVP_MAC* mac;
} HmacContext;

static HmacContext* hmac_ctx_create(const char* algorithm, const uint8_t* key_data,
                                    size_t key_len) {
    const char* ossl_name = map_hash_name(algorithm);

    EVP_MAC* mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (!mac)
        return NULL;

    EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
    if (!ctx) {
        EVP_MAC_free(mac);
        return NULL;
    }

    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", (char*)ossl_name, 0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_MAC_init(ctx, key_data, key_len, params) != 1) {
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        return NULL;
    }

    HmacContext* hctx = (HmacContext*)malloc(sizeof(HmacContext));
    if (!hctx) {
        EVP_MAC_CTX_free(ctx);
        EVP_MAC_free(mac);
        return NULL;
    }

    hctx->ctx = ctx;
    hctx->mac = mac;
    return hctx;
}

TML_EXPORT void* crypto_hmac_create(const char* algorithm, const char* key) {
    size_t key_len = key ? strlen(key) : 0;
    return hmac_ctx_create(algorithm, (const uint8_t*)key, key_len);
}

TML_EXPORT void* crypto_hmac_create_bytes(const char* algorithm, void* key_handle) {
    TmlBuffer* key = (TmlBuffer*)key_handle;
    const uint8_t* key_data = key ? key->data : NULL;
    size_t key_len = key ? (size_t)key->length : 0;
    return hmac_ctx_create(algorithm, key_data, key_len);
}

TML_EXPORT void crypto_hmac_update_str(void* handle, const char* data) {
    HmacContext* hctx = (HmacContext*)handle;
    if (!hctx || !data)
        return;
    size_t len = strlen(data);
    if (len > 0)
        EVP_MAC_update(hctx->ctx, (const unsigned char*)data, len);
}

TML_EXPORT void crypto_hmac_update_bytes(void* handle, void* data_handle) {
    HmacContext* hctx = (HmacContext*)handle;
    TmlBuffer* buf = (TmlBuffer*)data_handle;
    if (!hctx || !buf || buf->length <= 0)
        return;
    EVP_MAC_update(hctx->ctx, buf->data, buf->length);
}

TML_EXPORT void* crypto_hmac_digest(void* handle) {
    HmacContext* hctx = (HmacContext*)handle;
    if (!hctx)
        return NULL;

    size_t digest_len = 0;
    if (EVP_MAC_final(hctx->ctx, NULL, &digest_len, 0) != 1)
        return NULL;

    TmlBuffer* result = create_buffer((int64_t)digest_len);
    if (!result)
        return NULL;

    if (EVP_MAC_final(hctx->ctx, result->data, &digest_len, digest_len) != 1) {
        free(result->data);
        free(result);
        return NULL;
    }

    result->length = (int64_t)digest_len;
    return result;
}

TML_EXPORT void crypto_hmac_destroy(void* handle) {
    HmacContext* hctx = (HmacContext*)handle;
    if (!hctx)
        return;
    if (hctx->ctx)
        EVP_MAC_CTX_free(hctx->ctx);
    if (hctx->mac)
        EVP_MAC_free(hctx->mac);
    free(hctx);
}

#else
TML_EXPORT void* crypto_hmac_create(const char* algorithm, const char* key) {
    (void)algorithm;
    (void)key;
    return NULL;
}
TML_EXPORT void* crypto_hmac_create_bytes(const char* algorithm, void* key_handle) {
    (void)algorithm;
    (void)key_handle;
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
// Cipher Functions (OpenSSL EVP_CIPHER)
// ============================================================================

#ifdef TML_HAS_OPENSSL

typedef struct {
    EVP_CIPHER_CTX* ctx;
    int is_aead;
} CipherContext;

TML_EXPORT void* crypto_cipher_create(const char* algorithm, void* key_handle, void* iv_handle,
                                      int64_t encrypt) {
    TmlBuffer* key = (TmlBuffer*)key_handle;
    TmlBuffer* iv = (TmlBuffer*)iv_handle;
    if (!algorithm || !key)
        return NULL;

    // Map algorithm name to OpenSSL cipher
    const EVP_CIPHER* cipher = EVP_get_cipherbyname(algorithm);
    if (!cipher) {
        // Try common aliases
        if (strcmp(algorithm, "aes-128-cbc") == 0)
            cipher = EVP_aes_128_cbc();
        else if (strcmp(algorithm, "aes-192-cbc") == 0)
            cipher = EVP_aes_192_cbc();
        else if (strcmp(algorithm, "aes-256-cbc") == 0)
            cipher = EVP_aes_256_cbc();
        else if (strcmp(algorithm, "aes-128-gcm") == 0)
            cipher = EVP_aes_128_gcm();
        else if (strcmp(algorithm, "aes-192-gcm") == 0)
            cipher = EVP_aes_192_gcm();
        else if (strcmp(algorithm, "aes-256-gcm") == 0)
            cipher = EVP_aes_256_gcm();
        else if (strcmp(algorithm, "aes-128-ctr") == 0)
            cipher = EVP_aes_128_ctr();
        else if (strcmp(algorithm, "aes-256-ctr") == 0)
            cipher = EVP_aes_256_ctr();
        else if (strcmp(algorithm, "des-ede3-cbc") == 0)
            cipher = EVP_des_ede3_cbc();
        else if (strcmp(algorithm, "chacha20-poly1305") == 0)
            cipher = EVP_chacha20_poly1305();
        else if (strcmp(algorithm, "chacha20") == 0)
            cipher = EVP_chacha20();
        else
            return NULL;
    }

    CipherContext* cctx = (CipherContext*)malloc(sizeof(CipherContext));
    if (!cctx)
        return NULL;

    cctx->ctx = EVP_CIPHER_CTX_new();
    if (!cctx->ctx) {
        free(cctx);
        return NULL;
    }

    int mode = EVP_CIPHER_get_mode(cipher);
    cctx->is_aead = (mode == EVP_CIPH_GCM_MODE || mode == EVP_CIPH_CCM_MODE);

    const unsigned char* iv_data = iv ? iv->data : NULL;

    if (EVP_CipherInit_ex(cctx->ctx, cipher, NULL, key->data, iv_data, (int)encrypt) != 1) {
        EVP_CIPHER_CTX_free(cctx->ctx);
        free(cctx);
        return NULL;
    }

    return cctx;
}

TML_EXPORT void crypto_cipher_set_aad(void* handle, void* aad_handle) {
    CipherContext* cctx = (CipherContext*)handle;
    TmlBuffer* aad = (TmlBuffer*)aad_handle;
    if (!cctx || !aad)
        return;
    int outl;
    EVP_CipherUpdate(cctx->ctx, NULL, &outl, aad->data, (int)aad->length);
}

TML_EXPORT void crypto_cipher_set_aad_str(void* handle, const char* aad) {
    CipherContext* cctx = (CipherContext*)handle;
    if (!cctx || !aad)
        return;
    int outl;
    EVP_CipherUpdate(cctx->ctx, NULL, &outl, (const unsigned char*)aad, (int)strlen(aad));
}

TML_EXPORT void crypto_cipher_set_padding(void* handle, int32_t enabled) {
    CipherContext* cctx = (CipherContext*)handle;
    if (!cctx)
        return;
    EVP_CIPHER_CTX_set_padding(cctx->ctx, enabled);
}

TML_EXPORT void crypto_cipher_update_str(void* handle, const char* data, void* output_handle) {
    CipherContext* cctx = (CipherContext*)handle;
    TmlBuffer* output = (TmlBuffer*)output_handle;
    if (!cctx || !data || !output)
        return;
    int data_len = (int)strlen(data);
    int out_len = data_len + EVP_MAX_BLOCK_LENGTH;
    if (output->capacity < out_len) {
        uint8_t* new_data = (uint8_t*)realloc(output->data, out_len);
        if (!new_data)
            return;
        output->data = new_data;
        output->capacity = out_len;
    }
    EVP_CipherUpdate(cctx->ctx, output->data + output->length, &out_len, (const unsigned char*)data,
                     data_len);
    output->length += out_len;
}

TML_EXPORT void crypto_cipher_update_bytes(void* handle, void* data_handle, void* output_handle) {
    CipherContext* cctx = (CipherContext*)handle;
    TmlBuffer* data = (TmlBuffer*)data_handle;
    TmlBuffer* output = (TmlBuffer*)output_handle;
    if (!cctx || !data || !output)
        return;
    int out_len = (int)data->length + EVP_MAX_BLOCK_LENGTH;
    if (output->capacity < output->length + out_len) {
        int64_t new_cap = output->length + out_len;
        uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
        if (!new_data)
            return;
        output->data = new_data;
        output->capacity = new_cap;
    }
    EVP_CipherUpdate(cctx->ctx, output->data + output->length, &out_len, data->data,
                     (int)data->length);
    output->length += out_len;
}

TML_EXPORT int32_t crypto_cipher_finalize(void* handle, void* output_handle) {
    CipherContext* cctx = (CipherContext*)handle;
    TmlBuffer* output = (TmlBuffer*)output_handle;
    if (!cctx || !output)
        return 0;
    int out_len = 0;
    if (output->capacity < output->length + EVP_MAX_BLOCK_LENGTH) {
        int64_t new_cap = output->length + EVP_MAX_BLOCK_LENGTH;
        uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
        if (!new_data)
            return 0;
        output->data = new_data;
        output->capacity = new_cap;
    }
    if (EVP_CipherFinal_ex(cctx->ctx, output->data + output->length, &out_len) != 1)
        return 0;
    output->length += out_len;
    return 1;
}

TML_EXPORT void* crypto_cipher_get_tag(void* handle) {
    CipherContext* cctx = (CipherContext*)handle;
    if (!cctx)
        return NULL;
    TmlBuffer* tag = create_buffer(16);
    if (!tag)
        return NULL;
    if (EVP_CIPHER_CTX_ctrl(cctx->ctx, EVP_CTRL_AEAD_GET_TAG, 16, tag->data) != 1) {
        free(tag->data);
        free(tag);
        return NULL;
    }
    tag->length = 16;
    return tag;
}

TML_EXPORT void crypto_cipher_set_tag(void* handle, void* tag_handle) {
    CipherContext* cctx = (CipherContext*)handle;
    TmlBuffer* tag = (TmlBuffer*)tag_handle;
    if (!cctx || !tag)
        return;
    EVP_CIPHER_CTX_ctrl(cctx->ctx, EVP_CTRL_AEAD_SET_TAG, (int)tag->length, tag->data);
}

TML_EXPORT void crypto_cipher_destroy(void* handle) {
    CipherContext* cctx = (CipherContext*)handle;
    if (!cctx)
        return;
    if (cctx->ctx)
        EVP_CIPHER_CTX_free(cctx->ctx);
    free(cctx);
}

#else
TML_EXPORT void* crypto_cipher_create(const char* a, void* k, void* iv, int64_t e) {
    (void)a;
    (void)k;
    (void)iv;
    (void)e;
    return NULL;
}
TML_EXPORT void crypto_cipher_set_aad(void* h, void* a) {
    (void)h;
    (void)a;
}
TML_EXPORT void crypto_cipher_set_aad_str(void* h, const char* a) {
    (void)h;
    (void)a;
}
TML_EXPORT void crypto_cipher_set_padding(void* h, int32_t e) {
    (void)h;
    (void)e;
}
TML_EXPORT void crypto_cipher_update_str(void* h, const char* d, void* o) {
    (void)h;
    (void)d;
    (void)o;
}
TML_EXPORT void crypto_cipher_update_bytes(void* h, void* d, void* o) {
    (void)h;
    (void)d;
    (void)o;
}
TML_EXPORT int32_t crypto_cipher_finalize(void* h, void* o) {
    (void)h;
    (void)o;
    return 0;
}
TML_EXPORT void* crypto_cipher_get_tag(void* h) {
    (void)h;
    return NULL;
}
TML_EXPORT void crypto_cipher_set_tag(void* h, void* t) {
    (void)h;
    (void)t;
}
TML_EXPORT void crypto_cipher_destroy(void* h) {
    (void)h;
}
#endif

// ============================================================================
// Buffer utilities (str_to_bytes, bytes_to_str, concat, slice)
// ============================================================================

TML_EXPORT const char* crypto_bytes_to_str(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || buf->length <= 0) {
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    char* str = (char*)malloc(buf->length + 1);
    if (!str)
        return "";
    memcpy(str, buf->data, buf->length);
    str[buf->length] = '\0';
    return str;
}

TML_EXPORT void* crypto_str_to_bytes(const char* s) {
    if (!s)
        return create_buffer(0);
    size_t len = strlen(s);
    TmlBuffer* buf = create_buffer(len);
    if (!buf)
        return NULL;
    memcpy(buf->data, s, len);
    buf->length = len;
    return buf;
}

TML_EXPORT void* crypto_concat_buffers3(void* a_h, void* b_h, void* c_h) {
    TmlBuffer* a = (TmlBuffer*)a_h;
    TmlBuffer* b = (TmlBuffer*)b_h;
    TmlBuffer* c = (TmlBuffer*)c_h;
    int64_t total = (a ? a->length : 0) + (b ? b->length : 0) + (c ? c->length : 0);
    TmlBuffer* result = create_buffer(total);
    if (!result)
        return NULL;
    int64_t offset = 0;
    if (a && a->length > 0) {
        memcpy(result->data, a->data, a->length);
        offset += a->length;
    }
    if (b && b->length > 0) {
        memcpy(result->data + offset, b->data, b->length);
        offset += b->length;
    }
    if (c && c->length > 0) {
        memcpy(result->data + offset, c->data, c->length);
    }
    result->length = total;
    return result;
}

TML_EXPORT void* crypto_buffer_slice(void* handle, int64_t offset, int64_t length) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || offset < 0 || offset >= buf->length)
        return create_buffer(0);
    int64_t actual = (offset + length > buf->length) ? (buf->length - offset) : length;
    TmlBuffer* result = create_buffer(actual);
    if (!result)
        return NULL;
    memcpy(result->data, buf->data + offset, actual);
    result->length = actual;
    return result;
}

// ============================================================================
// Public API: Hex encoding/decoding
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

// ============================================================================
// Public API: Base64 encoding/decoding
// ============================================================================

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
        uint32_t a = i < input_len ? buf->data[i++] : 0;
        uint32_t b = i < input_len ? buf->data[i++] : 0;
        uint32_t c = i < input_len ? buf->data[i++] : 0;
        uint32_t triple = (a << 16) + (b << 8) + c;
        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = base64_chars[(triple >> 6) & 0x3F];
        output[j++] = base64_chars[triple & 0x3F];
    }
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
    // Strip trailing padding for length calculation
    size_t padding = 0;
    if (len >= 1 && b64[len - 1] == '=')
        padding++;
    if (len >= 2 && b64[len - 2] == '=')
        padding++;
    size_t data_len = len - padding; // length without padding chars
    size_t output_len = (len / 4) * 3 - padding;
    TmlBuffer* buf = create_buffer(output_len);
    if (!buf)
        return NULL;
    size_t i, j;
    for (i = 0, j = 0; i < data_len;) {
        int a = (i < data_len) ? base64_decode_char(b64[i++]) : 0;
        int b = (i < data_len) ? base64_decode_char(b64[i++]) : 0;
        int c = (i < data_len) ? base64_decode_char(b64[i++]) : 0;
        int d = (i < data_len) ? base64_decode_char(b64[i++]) : 0;
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
// Public API: Base64url encoding/decoding
// ============================================================================

TML_EXPORT const char* crypto_bytes_to_base64url(void* handle) {
    char* b64 = (char*)crypto_bytes_to_base64(handle);
    if (!b64)
        return NULL;
    // Convert + to -, / to _, remove padding
    for (char* p = b64; *p; p++) {
        if (*p == '+')
            *p = '-';
        else if (*p == '/')
            *p = '_';
    }
    size_t len = strlen(b64);
    while (len > 0 && b64[len - 1] == '=')
        b64[--len] = '\0';
    return b64;
}

TML_EXPORT void* crypto_base64url_to_bytes(const char* b64url) {
    if (!b64url)
        return NULL;
    size_t input_len = strlen(b64url);
    size_t padding = (4 - (input_len % 4)) % 4;
    size_t padded_len = input_len + padding;
    char* padded = (char*)malloc(padded_len + 1);
    if (!padded)
        return NULL;
    for (size_t i = 0; i < input_len; i++) {
        char c = b64url[i];
        if (c == '-')
            c = '+';
        else if (c == '_')
            c = '/';
        padded[i] = c;
    }
    for (size_t i = input_len; i < padded_len; i++)
        padded[i] = '=';
    padded[padded_len] = '\0';
    void* result = crypto_base64_to_bytes(padded);
    free(padded);
    return result;
}

// ============================================================================
// Prime number operations (OpenSSL BN)
// ============================================================================

#ifdef TML_HAS_OPENSSL

TML_EXPORT void* crypto_generate_prime(int64_t bits) {
    BIGNUM* bn = BN_new();
    if (!bn)
        return NULL;
    if (BN_generate_prime_ex(bn, (int)bits, 0, NULL, NULL, NULL) != 1) {
        BN_free(bn);
        return NULL;
    }
    int num_bytes = BN_num_bytes(bn);
    TmlBuffer* buf = create_buffer(num_bytes);
    if (!buf) {
        BN_free(bn);
        return NULL;
    }
    BN_bn2bin(bn, buf->data);
    buf->length = num_bytes;
    BN_free(bn);
    return buf;
}

TML_EXPORT void* crypto_generate_safe_prime(int64_t bits) {
    BIGNUM* bn = BN_new();
    if (!bn)
        return NULL;
    if (BN_generate_prime_ex(bn, (int)bits, 1, NULL, NULL, NULL) != 1) {
        BN_free(bn);
        return NULL;
    }
    int num_bytes = BN_num_bytes(bn);
    TmlBuffer* buf = create_buffer(num_bytes);
    if (!buf) {
        BN_free(bn);
        return NULL;
    }
    BN_bn2bin(bn, buf->data);
    buf->length = num_bytes;
    BN_free(bn);
    return buf;
}

TML_EXPORT int32_t crypto_check_prime(void* handle) {
    TmlBuffer* buf = (TmlBuffer*)handle;
    if (!buf || buf->length <= 0)
        return 0;
    BIGNUM* bn = BN_bin2bn(buf->data, (int)buf->length, NULL);
    if (!bn)
        return 0;
    BN_CTX* ctx = BN_CTX_new();
    int result = BN_check_prime(bn, ctx, NULL);
    BN_CTX_free(ctx);
    BN_free(bn);
    return result == 1 ? 1 : 0;
}

TML_EXPORT int32_t crypto_check_prime_rounds(void* handle, int64_t rounds) {
    (void)rounds; // OpenSSL's BN_check_prime uses its own round count
    return crypto_check_prime(handle);
}

#else
TML_EXPORT void* crypto_generate_prime(int64_t bits) {
    (void)bits;
    return NULL;
}
TML_EXPORT void* crypto_generate_safe_prime(int64_t bits) {
    (void)bits;
    return NULL;
}
TML_EXPORT int32_t crypto_check_prime(void* handle) {
    (void)handle;
    return 0;
}
TML_EXPORT int32_t crypto_check_prime_rounds(void* handle, int64_t rounds) {
    (void)handle;
    (void)rounds;
    return 0;
}
#endif

// ============================================================================
// Non-cryptographic Fast Hash Functions (FNV-1a, Murmur2)
// Pure C — no OpenSSL needed
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

TML_EXPORT const char* crypto_u32_to_hex(uint32_t value) {
    char* hex = (char*)malloc(9);
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
    char* hex = (char*)malloc(17);
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

// ============================================================================
// Algorithm Discovery & Constants
// ============================================================================

// Forward declarations for list_create/list_push from collections.c
typedef struct TmlCollectionList {
    void* data;
    int64_t len;
    int64_t capacity;
    int64_t elem_size;
} TmlCollectionList;

extern TmlCollectionList* list_create(int64_t initial_capacity);
extern void list_push(TmlCollectionList* list, int64_t value);

static void list_push_str(TmlCollectionList* list, const char* str) {
    char* dup = tml_strdup(str);
    if (dup)
        list_push(list, (int64_t)(uintptr_t)dup);
}

TML_EXPORT void* crypto_get_hashes(void) {
    TmlCollectionList* list = list_create(16);
    if (!list)
        return NULL;
    list_push_str(list, "md5");
    list_push_str(list, "sha1");
    list_push_str(list, "sha256");
    list_push_str(list, "sha384");
    list_push_str(list, "sha512");
    list_push_str(list, "sha512-256");
    list_push_str(list, "sha3-256");
    list_push_str(list, "sha3-384");
    list_push_str(list, "sha3-512");
    list_push_str(list, "blake2b512");
    list_push_str(list, "blake2s256");
    return list;
}

TML_EXPORT void* crypto_get_ciphers(void) {
    TmlCollectionList* list = list_create(16);
    if (!list)
        return NULL;
    list_push_str(list, "aes-128-cbc");
    list_push_str(list, "aes-192-cbc");
    list_push_str(list, "aes-256-cbc");
    list_push_str(list, "aes-128-ctr");
    list_push_str(list, "aes-256-ctr");
    list_push_str(list, "aes-128-gcm");
    list_push_str(list, "aes-192-gcm");
    list_push_str(list, "aes-256-gcm");
    list_push_str(list, "chacha20");
    list_push_str(list, "chacha20-poly1305");
    list_push_str(list, "des-ede3-cbc");
    return list;
}

// NOTE: crypto_get_curves is defined in crypto_ecdh.c

// Cipher info lookup table
typedef struct {
    const char* name;
    int64_t key_length;
    int64_t iv_length;
    int64_t block_size;
    const char* mode;
} CipherInfoEntry;

static const CipherInfoEntry CIPHER_TABLE[] = {
    {"aes-128-cbc", 16, 16, 16, "cbc"}, {"aes-192-cbc", 24, 16, 16, "cbc"},
    {"aes-256-cbc", 32, 16, 16, "cbc"}, {"aes-128-ctr", 16, 16, 1, "ctr"},
    {"aes-256-ctr", 32, 16, 1, "ctr"},  {"aes-128-gcm", 16, 12, 1, "gcm"},
    {"aes-192-gcm", 24, 12, 1, "gcm"},  {"aes-256-gcm", 32, 12, 1, "gcm"},
    {"aes-128-ccm", 16, 12, 1, "ccm"},  {"aes-256-ccm", 32, 12, 1, "ccm"},
    {"chacha20", 32, 16, 1, "stream"},  {"chacha20-poly1305", 32, 12, 1, "aead"},
    {"des-ede3-cbc", 24, 8, 8, "cbc"},  {"bf-cbc", 16, 8, 8, "cbc"},
    {"rc4", 16, 0, 1, "stream"},        {NULL, 0, 0, 0, NULL}};

static const CipherInfoEntry* find_cipher(const char* name) {
    if (!name)
        return NULL;
    for (int i = 0; CIPHER_TABLE[i].name != NULL; i++) {
        if (strcmp(name, CIPHER_TABLE[i].name) == 0)
            return &CIPHER_TABLE[i];
    }
    return NULL;
}

TML_EXPORT int32_t crypto_cipher_exists(const char* name) {
    return find_cipher(name) != NULL ? 1 : 0;
}

TML_EXPORT int64_t crypto_cipher_key_length(const char* name) {
    const CipherInfoEntry* info = find_cipher(name);
    return info ? info->key_length : -1;
}

TML_EXPORT int64_t crypto_cipher_iv_length(const char* name) {
    const CipherInfoEntry* info = find_cipher(name);
    return info ? info->iv_length : -1;
}

TML_EXPORT int64_t crypto_cipher_block_size(const char* name) {
    const CipherInfoEntry* info = find_cipher(name);
    return info ? info->block_size : -1;
}

TML_EXPORT const char* crypto_cipher_mode(const char* name) {
    const CipherInfoEntry* info = find_cipher(name);
    if (!info)
        return "";
    return tml_strdup(info->mode);
}

// ============================================================================
// FIPS Mode (OpenSSL)
// ============================================================================

#ifdef TML_HAS_OPENSSL

TML_EXPORT int32_t crypto_fips_mode(void) {
    return EVP_default_properties_is_fips_enabled(NULL) ? 1 : 0;
}

TML_EXPORT int32_t crypto_set_fips_mode(int32_t enabled) {
    return EVP_default_properties_enable_fips(NULL, enabled) == 1 ? 1 : 0;
}

#else

TML_EXPORT int32_t crypto_fips_mode(void) {
    return 0;
}
TML_EXPORT int32_t crypto_set_fips_mode(int32_t enabled) {
    (void)enabled;
    return 0;
}

#endif

// ============================================================================
// Secure Heap (OpenSSL)
// ============================================================================

#ifdef TML_HAS_OPENSSL

TML_EXPORT int64_t crypto_secure_heap_used(void) {
    if (!CRYPTO_secure_malloc_initialized())
        return 0;
    return (int64_t)CRYPTO_secure_used();
}

#else

TML_EXPORT int64_t crypto_secure_heap_used(void) {
    return 0;
}

#endif

// ============================================================================
// Engine Support (deprecated in OpenSSL 3.0, but we provide the API)
// ============================================================================

TML_EXPORT int32_t crypto_set_engine(const char* engine_id) {
    (void)engine_id;
    return 0; // Engines are deprecated in OpenSSL 3.0+
}
