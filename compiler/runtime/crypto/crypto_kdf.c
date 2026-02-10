/**
 * @file crypto_kdf.c
 * @brief TML Runtime - Key Derivation Functions (KDF)
 *
 * Implements KDF operations for:
 * - PBKDF2 (password-based key derivation)
 * - scrypt (memory-hard key derivation)
 * - HKDF (HMAC-based extract-and-expand key derivation)
 * - Argon2 (memory-hard password hashing, OpenSSL 3.2+)
 * - bcrypt (password hashing, stub - not natively in OpenSSL)
 *
 * Uses OpenSSL 3.0+ EVP_KDF API.
 */

#include "crypto_common.h"

#ifdef TML_HAS_OPENSSL

// ============================================================================
// PBKDF2
// ============================================================================

TML_EXPORT void* crypto_pbkdf2(const char* password, void* salt_handle, int64_t iterations,
                               int64_t key_length, const char* digest) {
    if (!password || !salt_handle || iterations <= 0 || key_length <= 0)
        return NULL;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;

    const EVP_MD* md = tml_get_md(digest);
    if (!md)
        md = EVP_sha256();

    TmlBuffer* out = tml_create_buffer(key_length);
    if (!out)
        return NULL;

    int rc = PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt->data, (int)salt->length,
                               (int)iterations, md, (int)key_length, out->data);
    if (rc != 1) {
        free(out->data);
        free(out);
        return NULL;
    }
    out->length = key_length;
    return (void*)out;
}

TML_EXPORT void* crypto_pbkdf2_bytes(void* password_handle, void* salt_handle, int64_t iterations,
                                     int64_t key_length, const char* digest) {
    if (!password_handle || !salt_handle || iterations <= 0 || key_length <= 0)
        return NULL;
    TmlBuffer* password = (TmlBuffer*)password_handle;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;

    const EVP_MD* md = tml_get_md(digest);
    if (!md)
        md = EVP_sha256();

    TmlBuffer* out = tml_create_buffer(key_length);
    if (!out)
        return NULL;

    int rc = PKCS5_PBKDF2_HMAC((const char*)password->data, (int)password->length, salt->data,
                               (int)salt->length, (int)iterations, md, (int)key_length, out->data);
    if (rc != 1) {
        free(out->data);
        free(out);
        return NULL;
    }
    out->length = key_length;
    return (void*)out;
}

// ============================================================================
// scrypt
// ============================================================================

TML_EXPORT void* crypto_scrypt(const char* password, void* salt_handle, int64_t key_length,
                               int64_t n, int64_t r, int64_t p, int64_t maxmem) {
    if (!password || !salt_handle || key_length <= 0)
        return NULL;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;

    TmlBuffer* out = tml_create_buffer(key_length);
    if (!out)
        return NULL;

    int rc =
        EVP_PBE_scrypt(password, strlen(password), salt->data, (size_t)salt->length, (uint64_t)n,
                       (uint64_t)r, (uint64_t)p, (uint64_t)maxmem, out->data, (size_t)key_length);
    if (rc != 1) {
        free(out->data);
        free(out);
        return NULL;
    }
    out->length = key_length;
    return (void*)out;
}

TML_EXPORT void* crypto_scrypt_bytes(void* password_handle, void* salt_handle, int64_t key_length,
                                     int64_t n, int64_t r, int64_t p, int64_t maxmem) {
    if (!password_handle || !salt_handle || key_length <= 0)
        return NULL;
    TmlBuffer* password = (TmlBuffer*)password_handle;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;

    TmlBuffer* out = tml_create_buffer(key_length);
    if (!out)
        return NULL;

    int rc = EVP_PBE_scrypt((const char*)password->data, (size_t)password->length, salt->data,
                            (size_t)salt->length, (uint64_t)n, (uint64_t)r, (uint64_t)p,
                            (uint64_t)maxmem, out->data, (size_t)key_length);
    if (rc != 1) {
        free(out->data);
        free(out);
        return NULL;
    }
    out->length = key_length;
    return (void*)out;
}

// ============================================================================
// HKDF (OpenSSL 3.0+ EVP_KDF API)
// ============================================================================

static void* hkdf_derive_impl(const char* digest, const uint8_t* ikm_data, size_t ikm_len,
                              const uint8_t* salt_data, size_t salt_len, const uint8_t* info_data,
                              size_t info_len, int64_t key_length, const char* mode_str) {
    if (!ikm_data || ikm_len == 0 || key_length <= 0)
        return NULL;

    EVP_KDF* kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
    if (!kdf)
        return NULL;

    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx)
        return NULL;

    const char* md_name = digest ? digest : "SHA256";

    OSSL_PARAM params[6];
    int idx = 0;

    params[idx++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, (char*)md_name, 0);
    params[idx++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void*)ikm_data, ikm_len);
    if (salt_data && salt_len > 0) {
        params[idx++] =
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void*)salt_data, salt_len);
    }
    if (info_data && info_len > 0) {
        params[idx++] =
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void*)info_data, info_len);
    }
    if (mode_str) {
        params[idx++] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_MODE, (char*)mode_str, 0);
    }
    params[idx] = OSSL_PARAM_construct_end();

    TmlBuffer* out = tml_create_buffer(key_length);
    if (!out) {
        EVP_KDF_CTX_free(kctx);
        return NULL;
    }

    if (EVP_KDF_derive(kctx, out->data, (size_t)key_length, params) != 1) {
        free(out->data);
        free(out);
        EVP_KDF_CTX_free(kctx);
        return NULL;
    }

    out->length = key_length;
    EVP_KDF_CTX_free(kctx);
    return (void*)out;
}

TML_EXPORT void* crypto_hkdf(const char* digest, void* ikm_handle, void* salt_handle,
                             const char* info, int64_t key_length) {
    if (!ikm_handle)
        return NULL;
    TmlBuffer* ikm = (TmlBuffer*)ikm_handle;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;

    const uint8_t* info_data = info ? (const uint8_t*)info : NULL;
    size_t info_len = info ? strlen(info) : 0;

    return hkdf_derive_impl(digest, ikm->data, (size_t)ikm->length, salt ? salt->data : NULL,
                            salt ? (size_t)salt->length : 0, info_data, info_len, key_length, NULL);
}

TML_EXPORT void* crypto_hkdf_bytes(const char* digest, void* ikm_handle, void* salt_handle,
                                   void* info_handle, int64_t key_length) {
    if (!ikm_handle)
        return NULL;
    TmlBuffer* ikm = (TmlBuffer*)ikm_handle;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;
    TmlBuffer* info = (TmlBuffer*)info_handle;

    return hkdf_derive_impl(digest, ikm->data, (size_t)ikm->length, salt ? salt->data : NULL,
                            salt ? (size_t)salt->length : 0, info ? info->data : NULL,
                            info ? (size_t)info->length : 0, key_length, NULL);
}

TML_EXPORT void* crypto_hkdf_extract(const char* digest, void* ikm_handle, void* salt_handle) {
    if (!ikm_handle)
        return NULL;
    TmlBuffer* ikm = (TmlBuffer*)ikm_handle;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;

    /* Extract mode output length equals the hash digest length */
    const EVP_MD* md = tml_get_md(digest ? digest : "SHA256");
    if (!md)
        md = EVP_sha256();
    int64_t out_len = (int64_t)EVP_MD_size(md);

    return hkdf_derive_impl(digest, ikm->data, (size_t)ikm->length, salt ? salt->data : NULL,
                            salt ? (size_t)salt->length : 0, NULL, 0, out_len, "EXTRACT_ONLY");
}

TML_EXPORT void* crypto_hkdf_expand(const char* digest, void* prk_handle, void* info_handle,
                                    int64_t key_length) {
    if (!prk_handle || key_length <= 0)
        return NULL;
    TmlBuffer* prk = (TmlBuffer*)prk_handle;
    TmlBuffer* info = (TmlBuffer*)info_handle;

    return hkdf_derive_impl(digest, prk->data, (size_t)prk->length, NULL, 0,
                            info ? info->data : NULL, info ? (size_t)info->length : 0, key_length,
                            "EXPAND_ONLY");
}

// ============================================================================
// Argon2 (OpenSSL 3.2+ when available)
// ============================================================================

/*
 * Argon2 support in OpenSSL is available from version 3.2+.
 * We attempt to fetch the KDF at runtime; if unavailable we return NULL.
 */

static const char* argon2_variant_name(const char* variant) {
    if (!variant)
        return "ARGON2ID";
    if (strcmp(variant, "argon2i") == 0 || strcmp(variant, "Argon2i") == 0)
        return "ARGON2I";
    if (strcmp(variant, "argon2d") == 0 || strcmp(variant, "Argon2d") == 0)
        return "ARGON2D";
    /* Default to argon2id */
    return "ARGON2ID";
}

TML_EXPORT void* crypto_argon2(const char* variant, const char* password, void* salt_handle,
                               int64_t key_length, int64_t time_cost, int64_t memory_cost,
                               int64_t parallelism) {
    if (!password || !salt_handle || key_length <= 0)
        return NULL;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;

    const char* alg_name = argon2_variant_name(variant);
    EVP_KDF* kdf = EVP_KDF_fetch(NULL, alg_name, NULL);
    if (!kdf)
        return NULL; /* Argon2 not available in this OpenSSL build */

    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx)
        return NULL;

    uint32_t t = (uint32_t)time_cost;
    uint32_t m = (uint32_t)memory_cost;
    uint32_t p_val = (uint32_t)parallelism;
    size_t pass_len = strlen(password);

    OSSL_PARAM params[7];
    int idx = 0;
    params[idx++] =
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, (void*)password, pass_len);
    params[idx++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void*)salt->data,
                                                      (size_t)salt->length);
    params[idx++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &t);
    params[idx++] = OSSL_PARAM_construct_uint32("memcost", &m);
    params[idx++] = OSSL_PARAM_construct_uint32("threads", &p_val);
    params[idx++] = OSSL_PARAM_construct_uint32("lanes", &p_val);
    params[idx] = OSSL_PARAM_construct_end();

    TmlBuffer* out = tml_create_buffer(key_length);
    if (!out) {
        EVP_KDF_CTX_free(kctx);
        return NULL;
    }

    if (EVP_KDF_derive(kctx, out->data, (size_t)key_length, params) != 1) {
        free(out->data);
        free(out);
        EVP_KDF_CTX_free(kctx);
        return NULL;
    }

    out->length = key_length;
    EVP_KDF_CTX_free(kctx);
    return (void*)out;
}

TML_EXPORT void* crypto_argon2_bytes(const char* variant, void* password_handle, void* salt_handle,
                                     int64_t key_length, int64_t time_cost, int64_t memory_cost,
                                     int64_t parallelism) {
    if (!password_handle || !salt_handle || key_length <= 0)
        return NULL;
    TmlBuffer* password = (TmlBuffer*)password_handle;
    TmlBuffer* salt = (TmlBuffer*)salt_handle;

    const char* alg_name = argon2_variant_name(variant);
    EVP_KDF* kdf = EVP_KDF_fetch(NULL, alg_name, NULL);
    if (!kdf)
        return NULL;

    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx)
        return NULL;

    uint32_t t = (uint32_t)time_cost;
    uint32_t m = (uint32_t)memory_cost;
    uint32_t p_val = (uint32_t)parallelism;

    OSSL_PARAM params[7];
    int idx = 0;
    params[idx++] = OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_PASSWORD, (void*)password->data, (size_t)password->length);
    params[idx++] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void*)salt->data,
                                                      (size_t)salt->length);
    params[idx++] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &t);
    params[idx++] = OSSL_PARAM_construct_uint32("memcost", &m);
    params[idx++] = OSSL_PARAM_construct_uint32("threads", &p_val);
    params[idx++] = OSSL_PARAM_construct_uint32("lanes", &p_val);
    params[idx] = OSSL_PARAM_construct_end();

    TmlBuffer* out = tml_create_buffer(key_length);
    if (!out) {
        EVP_KDF_CTX_free(kctx);
        return NULL;
    }

    if (EVP_KDF_derive(kctx, out->data, (size_t)key_length, params) != 1) {
        free(out->data);
        free(out);
        EVP_KDF_CTX_free(kctx);
        return NULL;
    }

    out->length = key_length;
    EVP_KDF_CTX_free(kctx);
    return (void*)out;
}

TML_EXPORT int32_t crypto_argon2_verify(const char* encoded_hash, const char* password) {
    /*
     * OpenSSL's Argon2 KDF does not provide a built-in verify-from-encoded-string
     * function. A full implementation would parse the PHC string format
     * ($argon2id$v=19$m=...,t=...,p=...$salt$hash), re-derive the key, and
     * compare in constant time. For now, return 0 (verification failed) as a
     * stub until the PHC parser is implemented.
     */
    (void)encoded_hash;
    (void)password;
    return 0;
}

TML_EXPORT const char* crypto_argon2_hash(const char* variant, const char* password,
                                          int64_t time_cost, int64_t memory_cost,
                                          int64_t parallelism) {
    /*
     * Produces a PHC-format encoded hash string. Requires generating a random
     * salt, running Argon2, and formatting the output. Stub until PHC encoder
     * is implemented.
     */
    (void)variant;
    (void)password;
    (void)time_cost;
    (void)memory_cost;
    (void)parallelism;
    return tml_strdup("");
}

// ============================================================================
// bcrypt (not natively available in OpenSSL)
// ============================================================================

TML_EXPORT const char* crypto_bcrypt_hash(const char* password, int64_t rounds) {
    /* OpenSSL does not provide a native bcrypt implementation. */
    (void)password;
    (void)rounds;
    return tml_strdup("");
}

TML_EXPORT int32_t crypto_bcrypt_verify(const char* hash, const char* password) {
    /* OpenSSL does not provide a native bcrypt implementation. */
    (void)hash;
    (void)password;
    return 0;
}

#else /* !TML_HAS_OPENSSL */

// ============================================================================
// Stubs when OpenSSL is not available
// ============================================================================

TML_EXPORT void* crypto_pbkdf2(const char* password, void* salt_handle, int64_t iterations,
                               int64_t key_length, const char* digest) {
    (void)password;
    (void)salt_handle;
    (void)iterations;
    (void)key_length;
    (void)digest;
    return NULL;
}

TML_EXPORT void* crypto_pbkdf2_bytes(void* password_handle, void* salt_handle, int64_t iterations,
                                     int64_t key_length, const char* digest) {
    (void)password_handle;
    (void)salt_handle;
    (void)iterations;
    (void)key_length;
    (void)digest;
    return NULL;
}

TML_EXPORT void* crypto_scrypt(const char* password, void* salt_handle, int64_t key_length,
                               int64_t n, int64_t r, int64_t p, int64_t maxmem) {
    (void)password;
    (void)salt_handle;
    (void)key_length;
    (void)n;
    (void)r;
    (void)p;
    (void)maxmem;
    return NULL;
}

TML_EXPORT void* crypto_scrypt_bytes(void* password_handle, void* salt_handle, int64_t key_length,
                                     int64_t n, int64_t r, int64_t p, int64_t maxmem) {
    (void)password_handle;
    (void)salt_handle;
    (void)key_length;
    (void)n;
    (void)r;
    (void)p;
    (void)maxmem;
    return NULL;
}

TML_EXPORT void* crypto_hkdf(const char* digest, void* ikm_handle, void* salt_handle,
                             const char* info, int64_t key_length) {
    (void)digest;
    (void)ikm_handle;
    (void)salt_handle;
    (void)info;
    (void)key_length;
    return NULL;
}

TML_EXPORT void* crypto_hkdf_bytes(const char* digest, void* ikm_handle, void* salt_handle,
                                   void* info_handle, int64_t key_length) {
    (void)digest;
    (void)ikm_handle;
    (void)salt_handle;
    (void)info_handle;
    (void)key_length;
    return NULL;
}

TML_EXPORT void* crypto_hkdf_extract(const char* digest, void* ikm_handle, void* salt_handle) {
    (void)digest;
    (void)ikm_handle;
    (void)salt_handle;
    return NULL;
}

TML_EXPORT void* crypto_hkdf_expand(const char* digest, void* prk_handle, void* info_handle,
                                    int64_t key_length) {
    (void)digest;
    (void)prk_handle;
    (void)info_handle;
    (void)key_length;
    return NULL;
}

TML_EXPORT void* crypto_argon2(const char* variant, const char* password, void* salt_handle,
                               int64_t key_length, int64_t time_cost, int64_t memory_cost,
                               int64_t parallelism) {
    (void)variant;
    (void)password;
    (void)salt_handle;
    (void)key_length;
    (void)time_cost;
    (void)memory_cost;
    (void)parallelism;
    return NULL;
}

TML_EXPORT void* crypto_argon2_bytes(const char* variant, void* password_handle, void* salt_handle,
                                     int64_t key_length, int64_t time_cost, int64_t memory_cost,
                                     int64_t parallelism) {
    (void)variant;
    (void)password_handle;
    (void)salt_handle;
    (void)key_length;
    (void)time_cost;
    (void)memory_cost;
    (void)parallelism;
    return NULL;
}

TML_EXPORT int32_t crypto_argon2_verify(const char* encoded_hash, const char* password) {
    (void)encoded_hash;
    (void)password;
    return 0;
}

TML_EXPORT const char* crypto_argon2_hash(const char* variant, const char* password,
                                          int64_t time_cost, int64_t memory_cost,
                                          int64_t parallelism) {
    (void)variant;
    (void)password;
    (void)time_cost;
    (void)memory_cost;
    (void)parallelism;
    return "";
}

TML_EXPORT const char* crypto_bcrypt_hash(const char* password, int64_t rounds) {
    (void)password;
    (void)rounds;
    return "";
}

TML_EXPORT int32_t crypto_bcrypt_verify(const char* hash, const char* password) {
    (void)hash;
    (void)password;
    return 0;
}

#endif /* TML_HAS_OPENSSL */
