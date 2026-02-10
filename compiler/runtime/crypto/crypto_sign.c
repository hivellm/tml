/**
 * @file crypto_sign.c
 * @brief TML Runtime - Digital Signature Functions
 *
 * Implements streaming sign/verify using EVP_DigestSign/EVP_DigestVerify:
 * - RSA with SHA-1/256/384/512
 * - RSA-PSS with SHA-256/384/512
 * - ECDSA with SHA-1/256/384/512
 * - Ed25519, Ed448 (single-shot, NULL digest)
 * - DSA with SHA-1/256
 *
 * Uses OpenSSL 3.0+ EVP API.
 */

#include "crypto_common.h"

#ifdef TML_HAS_OPENSSL

// ============================================================================
// Internal: signing context wrapper
// ============================================================================

typedef struct {
    EVP_MD_CTX* ctx;
    int is_pss;
} TmlSignCtx;

// ============================================================================
// Internal: parse algorithm string to EVP_MD and detect PSS mode
//
// Algorithm strings:
//   "RSA-SHA1", "RSA-SHA256", "RSA-SHA384", "RSA-SHA512"
//   "RSA-PSS-SHA256", "RSA-PSS-SHA384", "RSA-PSS-SHA512"
//   "ECDSA-SHA1", "ECDSA-SHA256", "ECDSA-SHA384", "ECDSA-SHA512"
//   "Ed25519", "Ed448"
//   "DSA-SHA1", "DSA-SHA256"
// ============================================================================

static const EVP_MD* parse_sign_algorithm(const char* algorithm, int* out_is_pss) {
    if (!algorithm)
        return NULL;
    *out_is_pss = 0;

    // Ed25519 / Ed448: no digest needed (single-shot signing)
    if (strcmp(algorithm, "Ed25519") == 0 || strcmp(algorithm, "Ed448") == 0) {
        return NULL; /* NULL md signals single-shot mode */
    }

    // RSA-PSS variants
    if (strcmp(algorithm, "RSA-PSS-SHA256") == 0) {
        *out_is_pss = 1;
        return EVP_sha256();
    }
    if (strcmp(algorithm, "RSA-PSS-SHA384") == 0) {
        *out_is_pss = 1;
        return EVP_sha384();
    }
    if (strcmp(algorithm, "RSA-PSS-SHA512") == 0) {
        *out_is_pss = 1;
        return EVP_sha512();
    }

    // RSA variants
    if (strcmp(algorithm, "RSA-SHA1") == 0)
        return EVP_sha1();
    if (strcmp(algorithm, "RSA-SHA256") == 0)
        return EVP_sha256();
    if (strcmp(algorithm, "RSA-SHA384") == 0)
        return EVP_sha384();
    if (strcmp(algorithm, "RSA-SHA512") == 0)
        return EVP_sha512();

    // ECDSA variants
    if (strcmp(algorithm, "ECDSA-SHA1") == 0)
        return EVP_sha1();
    if (strcmp(algorithm, "ECDSA-SHA256") == 0)
        return EVP_sha256();
    if (strcmp(algorithm, "ECDSA-SHA384") == 0)
        return EVP_sha384();
    if (strcmp(algorithm, "ECDSA-SHA512") == 0)
        return EVP_sha512();

    // DSA variants
    if (strcmp(algorithm, "DSA-SHA1") == 0)
        return EVP_sha1();
    if (strcmp(algorithm, "DSA-SHA256") == 0)
        return EVP_sha256();

    // Fallback: try OpenSSL's digest name lookup for the hash portion
    // e.g. strip prefix up to last '-' and try that
    const char* dash = strrchr(algorithm, '-');
    if (dash) {
        return EVP_get_digestbyname(dash + 1);
    }

    return NULL;
}

// ============================================================================
// Internal: check if algorithm is Ed25519/Ed448 (single-shot, no streaming)
// ============================================================================

static int is_single_shot_algorithm(const char* algorithm) {
    if (!algorithm)
        return 0;
    return (strcmp(algorithm, "Ed25519") == 0 || strcmp(algorithm, "Ed448") == 0);
}

// ============================================================================
// Signer API
// ============================================================================

TML_EXPORT void* crypto_signer_create(const char* algorithm, void* key_handle) {
    if (!algorithm || !key_handle)
        return NULL;

    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    int is_pss = 0;
    const EVP_MD* md = parse_sign_algorithm(algorithm, &is_pss);

    // For non-EdDSA algorithms, md must be valid
    if (!is_single_shot_algorithm(algorithm) && !md)
        return NULL;

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx)
        return NULL;

    // Initialize the signing context
    // For Ed25519/Ed448: pass NULL as md (single-shot mode)
    if (EVP_DigestSignInit(md_ctx, NULL, md, NULL, pkey) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    // For RSA-PSS: set padding mode
    if (is_pss) {
        EVP_PKEY_CTX* pkey_ctx = EVP_MD_CTX_get_pkey_ctx(md_ctx);
        if (!pkey_ctx || EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) <= 0) {
            EVP_MD_CTX_free(md_ctx);
            return NULL;
        }
    }

    TmlSignCtx* sign_ctx = (TmlSignCtx*)malloc(sizeof(TmlSignCtx));
    if (!sign_ctx) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }
    sign_ctx->ctx = md_ctx;
    sign_ctx->is_pss = is_pss;

    return (void*)sign_ctx;
}

TML_EXPORT void crypto_signer_update_str(void* handle, const char* data) {
    if (!handle || !data)
        return;
    TmlSignCtx* sign_ctx = (TmlSignCtx*)handle;
    size_t len = strlen(data);
    if (len == 0)
        return;
    EVP_DigestSignUpdate(sign_ctx->ctx, data, len);
}

TML_EXPORT void crypto_signer_update_bytes(void* handle, void* buffer_handle) {
    if (!handle || !buffer_handle)
        return;
    TmlSignCtx* sign_ctx = (TmlSignCtx*)handle;
    TmlBuffer* buf = (TmlBuffer*)buffer_handle;
    if (!buf->data || buf->length <= 0)
        return;
    EVP_DigestSignUpdate(sign_ctx->ctx, buf->data, (size_t)buf->length);
}

TML_EXPORT void* crypto_signer_sign(void* handle) {
    if (!handle)
        return NULL;
    TmlSignCtx* sign_ctx = (TmlSignCtx*)handle;

    // Determine the signature length
    size_t sig_len = 0;
    if (EVP_DigestSignFinal(sign_ctx->ctx, NULL, &sig_len) != 1)
        return NULL;

    // Allocate buffer and produce signature
    unsigned char* sig = (unsigned char*)malloc(sig_len);
    if (!sig)
        return NULL;

    if (EVP_DigestSignFinal(sign_ctx->ctx, sig, &sig_len) != 1) {
        free(sig);
        return NULL;
    }

    TmlBuffer* result = tml_create_buffer_with_data(sig, (int64_t)sig_len);
    free(sig);
    return (void*)result;
}

TML_EXPORT void crypto_signer_destroy(void* handle) {
    if (!handle)
        return;
    TmlSignCtx* sign_ctx = (TmlSignCtx*)handle;
    if (sign_ctx->ctx)
        EVP_MD_CTX_free(sign_ctx->ctx);
    free(sign_ctx);
}

// ============================================================================
// Verifier API
// ============================================================================

TML_EXPORT void* crypto_verifier_create(const char* algorithm, void* key_handle) {
    if (!algorithm || !key_handle)
        return NULL;

    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    int is_pss = 0;
    const EVP_MD* md = parse_sign_algorithm(algorithm, &is_pss);

    // For non-EdDSA algorithms, md must be valid
    if (!is_single_shot_algorithm(algorithm) && !md)
        return NULL;

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx)
        return NULL;

    // Initialize the verification context
    if (EVP_DigestVerifyInit(md_ctx, NULL, md, NULL, pkey) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    // For RSA-PSS: set padding mode
    if (is_pss) {
        EVP_PKEY_CTX* pkey_ctx = EVP_MD_CTX_get_pkey_ctx(md_ctx);
        if (!pkey_ctx || EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) <= 0) {
            EVP_MD_CTX_free(md_ctx);
            return NULL;
        }
    }

    TmlSignCtx* verify_ctx = (TmlSignCtx*)malloc(sizeof(TmlSignCtx));
    if (!verify_ctx) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }
    verify_ctx->ctx = md_ctx;
    verify_ctx->is_pss = is_pss;

    return (void*)verify_ctx;
}

TML_EXPORT void crypto_verifier_update_str(void* handle, const char* data) {
    if (!handle || !data)
        return;
    TmlSignCtx* verify_ctx = (TmlSignCtx*)handle;
    size_t len = strlen(data);
    if (len == 0)
        return;
    EVP_DigestVerifyUpdate(verify_ctx->ctx, data, len);
}

TML_EXPORT void crypto_verifier_update_bytes(void* handle, void* buffer_handle) {
    if (!handle || !buffer_handle)
        return;
    TmlSignCtx* verify_ctx = (TmlSignCtx*)handle;
    TmlBuffer* buf = (TmlBuffer*)buffer_handle;
    if (!buf->data || buf->length <= 0)
        return;
    EVP_DigestVerifyUpdate(verify_ctx->ctx, buf->data, (size_t)buf->length);
}

TML_EXPORT int32_t crypto_verifier_verify(void* handle, void* sig_buffer_handle) {
    if (!handle || !sig_buffer_handle)
        return 0;
    TmlSignCtx* verify_ctx = (TmlSignCtx*)handle;
    TmlBuffer* sig = (TmlBuffer*)sig_buffer_handle;
    if (!sig->data || sig->length <= 0)
        return 0;

    int result = EVP_DigestVerifyFinal(verify_ctx->ctx, sig->data, (size_t)sig->length);
    return (result == 1) ? 1 : 0;
}

TML_EXPORT void crypto_verifier_destroy(void* handle) {
    if (!handle)
        return;
    TmlSignCtx* verify_ctx = (TmlSignCtx*)handle;
    if (verify_ctx->ctx)
        EVP_MD_CTX_free(verify_ctx->ctx);
    free(verify_ctx);
}

// ============================================================================
// RSA-PSS one-shot sign/verify with explicit parameters
// ============================================================================

static const EVP_MD* get_mgf1_md(const char* mgf1_hash) {
    if (!mgf1_hash || strlen(mgf1_hash) == 0)
        return EVP_sha256();
    if (strcmp(mgf1_hash, "SHA-1") == 0 || strcmp(mgf1_hash, "SHA1") == 0)
        return EVP_sha1();
    if (strcmp(mgf1_hash, "SHA-256") == 0 || strcmp(mgf1_hash, "SHA256") == 0)
        return EVP_sha256();
    if (strcmp(mgf1_hash, "SHA-384") == 0 || strcmp(mgf1_hash, "SHA384") == 0)
        return EVP_sha384();
    if (strcmp(mgf1_hash, "SHA-512") == 0 || strcmp(mgf1_hash, "SHA512") == 0)
        return EVP_sha512();
    return EVP_get_digestbyname(mgf1_hash);
}

TML_EXPORT void* crypto_sign_rsa_pss(void* key_handle, const char* data, int64_t salt_length,
                                     const char* mgf1_hash) {
    if (!key_handle || !data)
        return NULL;

    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    const EVP_MD* mgf1_md = get_mgf1_md(mgf1_hash);
    if (!mgf1_md)
        return NULL;

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx)
        return NULL;

    EVP_PKEY_CTX* pkey_ctx = NULL;
    if (EVP_DigestSignInit(md_ctx, &pkey_ctx, mgf1_md, NULL, pkey) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    // Set RSA-PSS padding
    if (EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) <= 0) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    // Set salt length (-1 = digest length, -2 = max, or explicit value)
    int sl = (salt_length < 0) ? RSA_PSS_SALTLEN_DIGEST : (int)salt_length;
    if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, sl) <= 0) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    // Set MGF1 hash
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, mgf1_md) <= 0) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    // Feed data
    size_t data_len = strlen(data);
    if (EVP_DigestSignUpdate(md_ctx, data, data_len) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    // Get signature length
    size_t sig_len = 0;
    if (EVP_DigestSignFinal(md_ctx, NULL, &sig_len) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    // Produce signature
    unsigned char* sig = (unsigned char*)malloc(sig_len);
    if (!sig) {
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    if (EVP_DigestSignFinal(md_ctx, sig, &sig_len) != 1) {
        free(sig);
        EVP_MD_CTX_free(md_ctx);
        return NULL;
    }

    EVP_MD_CTX_free(md_ctx);

    TmlBuffer* result = tml_create_buffer_with_data(sig, (int64_t)sig_len);
    free(sig);
    return (void*)result;
}

TML_EXPORT int32_t crypto_verify_rsa_pss(void* key_handle, const char* data, void* sig_handle,
                                         int64_t salt_length, const char* mgf1_hash) {
    if (!key_handle || !data || !sig_handle)
        return 0;

    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    TmlBuffer* sig = (TmlBuffer*)sig_handle;
    if (!sig->data || sig->length <= 0)
        return 0;

    const EVP_MD* mgf1_md = get_mgf1_md(mgf1_hash);
    if (!mgf1_md)
        return 0;

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    if (!md_ctx)
        return 0;

    EVP_PKEY_CTX* pkey_ctx = NULL;
    if (EVP_DigestVerifyInit(md_ctx, &pkey_ctx, mgf1_md, NULL, pkey) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return 0;
    }

    // Set RSA-PSS padding
    if (EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) <= 0) {
        EVP_MD_CTX_free(md_ctx);
        return 0;
    }

    // Set salt length
    int sl = (salt_length < 0) ? RSA_PSS_SALTLEN_DIGEST : (int)salt_length;
    if (EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, sl) <= 0) {
        EVP_MD_CTX_free(md_ctx);
        return 0;
    }

    // Set MGF1 hash
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, mgf1_md) <= 0) {
        EVP_MD_CTX_free(md_ctx);
        return 0;
    }

    // Feed data
    size_t data_len = strlen(data);
    if (EVP_DigestVerifyUpdate(md_ctx, data, data_len) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return 0;
    }

    // Verify
    int result = EVP_DigestVerifyFinal(md_ctx, sig->data, (size_t)sig->length);
    EVP_MD_CTX_free(md_ctx);

    return (result == 1) ? 1 : 0;
}

#else /* !TML_HAS_OPENSSL */

// ============================================================================
// Stubs when OpenSSL is not available
// ============================================================================

TML_EXPORT void* crypto_signer_create(const char* algorithm, void* key_handle) {
    (void)algorithm;
    (void)key_handle;
    return NULL;
}
TML_EXPORT void crypto_signer_update_str(void* handle, const char* data) {
    (void)handle;
    (void)data;
}
TML_EXPORT void crypto_signer_update_bytes(void* handle, void* buffer_handle) {
    (void)handle;
    (void)buffer_handle;
}
TML_EXPORT void* crypto_signer_sign(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void crypto_signer_destroy(void* handle) {
    (void)handle;
}

TML_EXPORT void* crypto_verifier_create(const char* algorithm, void* key_handle) {
    (void)algorithm;
    (void)key_handle;
    return NULL;
}
TML_EXPORT void crypto_verifier_update_str(void* handle, const char* data) {
    (void)handle;
    (void)data;
}
TML_EXPORT void crypto_verifier_update_bytes(void* handle, void* buffer_handle) {
    (void)handle;
    (void)buffer_handle;
}
TML_EXPORT int32_t crypto_verifier_verify(void* handle, void* sig_buffer_handle) {
    (void)handle;
    (void)sig_buffer_handle;
    return 0;
}
TML_EXPORT void crypto_verifier_destroy(void* handle) {
    (void)handle;
}

TML_EXPORT void* crypto_sign_rsa_pss(void* key_handle, const char* data, int64_t salt_length,
                                     const char* mgf1_hash) {
    (void)key_handle;
    (void)data;
    (void)salt_length;
    (void)mgf1_hash;
    return NULL;
}
TML_EXPORT int32_t crypto_verify_rsa_pss(void* key_handle, const char* data, void* sig_handle,
                                         int64_t salt_length, const char* mgf1_hash) {
    (void)key_handle;
    (void)data;
    (void)sig_handle;
    (void)salt_length;
    (void)mgf1_hash;
    return 0;
}

#endif /* TML_HAS_OPENSSL */
