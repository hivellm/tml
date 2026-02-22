/**
 * @file crypto_key.c
 * @brief TML Runtime - Cryptographic Key Management
 *
 * Implements key generation, import/export for:
 * - SecretKey (symmetric keys)
 * - PrivateKey / PublicKey (asymmetric: RSA, EC, Ed25519, Ed448, X25519, X448, DSA, DH)
 * - PEM, DER, JWK encoding
 *
 * Uses OpenSSL 3.0+ EVP API.
 */

#include "crypto_common.h"

#ifdef TML_HAS_OPENSSL

#include <openssl/decoder.h>
#include <openssl/encoder.h>

// ============================================================================
// SecretKey: symmetric key (raw bytes)
// ============================================================================

TML_EXPORT void* crypto_secret_key_create(void* buffer_handle) {
    TmlBuffer* buf = (TmlBuffer*)buffer_handle;
    if (!buf || buf->length <= 0)
        return NULL;
    TmlBuffer* key = tml_create_buffer_with_data(buf->data, buf->length);
    return (void*)key;
}

TML_EXPORT void* crypto_secret_key_export(void* handle) {
    TmlBuffer* key = (TmlBuffer*)handle;
    if (!key)
        return NULL;
    return (void*)tml_create_buffer_with_data(key->data, key->length);
}

TML_EXPORT void crypto_secret_key_destroy(void* handle) {
    TmlBuffer* key = (TmlBuffer*)handle;
    if (key) {
        if (key->data) {
            OPENSSL_cleanse(key->data, key->capacity);
            free(key->data);
        }
        free(key);
    }
}

TML_EXPORT void* crypto_generate_secret_key(int64_t size) {
    if (size <= 0)
        return NULL;
    TmlBuffer* key = tml_create_buffer(size);
    if (!key)
        return NULL;
    if (RAND_bytes(key->data, (int)size) != 1) {
        free(key->data);
        free(key);
        return NULL;
    }
    key->length = size;
    return (void*)key;
}

// ============================================================================
// PrivateKey: asymmetric private key (wraps EVP_PKEY*)
// ============================================================================

TML_EXPORT void* crypto_private_key_from_pem(const char* pem) {
    if (!pem)
        return NULL;
    BIO* bio = tml_bio_from_str(pem);
    if (!bio)
        return NULL;
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);
    return (void*)pkey;
}

TML_EXPORT void* crypto_private_key_from_pem_encrypted(const char* pem, const char* passphrase) {
    if (!pem)
        return NULL;
    BIO* bio = tml_bio_from_str(pem);
    if (!bio)
        return NULL;
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, (void*)passphrase);
    BIO_free(bio);
    return (void*)pkey;
}

TML_EXPORT void* crypto_private_key_from_der(void* buffer_handle) {
    TmlBuffer* buf = (TmlBuffer*)buffer_handle;
    if (!buf || buf->length <= 0)
        return NULL;
    const unsigned char* p = buf->data;
    EVP_PKEY* pkey = d2i_AutoPrivateKey(NULL, &p, (long)buf->length);
    return (void*)pkey;
}

TML_EXPORT void* crypto_private_key_from_jwk(const char* jwk) {
    // JWK import via OSSL_DECODER
    // For now, return NULL - JWK requires JSON parsing + OpenSSL 3.x decoder
    // TODO: Implement via OSSL_DECODER with "JWK" input type
    (void)jwk;
    return NULL;
}

TML_EXPORT const char* crypto_private_key_to_pem(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return tml_strdup("");
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return tml_strdup("");
    if (PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL) != 1) {
        BIO_free(bio);
        return tml_strdup("");
    }
    char* result = tml_bio_to_str(bio);
    BIO_free(bio);
    return result;
}

TML_EXPORT const char* crypto_private_key_to_pem_encrypted(void* handle, const char* passphrase,
                                                           const char* cipher_name) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey || !passphrase)
        return tml_strdup("");
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return tml_strdup("");
    const EVP_CIPHER* cipher = EVP_get_cipherbyname(cipher_name ? cipher_name : "aes-256-cbc");
    if (!cipher)
        cipher = EVP_aes_256_cbc();
    if (PEM_write_bio_PrivateKey(bio, pkey, cipher, (unsigned char*)passphrase,
                                 (int)strlen(passphrase), NULL, NULL) != 1) {
        BIO_free(bio);
        return tml_strdup("");
    }
    char* result = tml_bio_to_str(bio);
    BIO_free(bio);
    return result;
}

TML_EXPORT void* crypto_private_key_to_der(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return NULL;
    unsigned char* der = NULL;
    int len = i2d_PrivateKey(pkey, &der);
    if (len <= 0 || !der)
        return NULL;
    TmlBuffer* buf = tml_create_buffer_with_data(der, len);
    OPENSSL_free(der);
    return (void*)buf;
}

TML_EXPORT const char* crypto_private_key_to_jwk(void* handle) {
    // TODO: Implement JWK export
    (void)handle;
    return tml_strdup("");
}

TML_EXPORT void* crypto_private_key_get_public(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return NULL;

    // Serialize public key to DER and re-read
    unsigned char* der = NULL;
    int len = i2d_PUBKEY(pkey, &der);
    if (len <= 0 || !der)
        return NULL;

    const unsigned char* p = der;
    EVP_PKEY* pub = d2i_PUBKEY(NULL, &p, len);
    OPENSSL_free(der);
    return (void*)pub;
}

TML_EXPORT void crypto_private_key_destroy(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (pkey)
        EVP_PKEY_free(pkey);
}

// ============================================================================
// PublicKey: asymmetric public key (wraps EVP_PKEY*)
// ============================================================================

TML_EXPORT void* crypto_public_key_from_pem(const char* pem) {
    if (!pem)
        return NULL;
    BIO* bio = tml_bio_from_str(pem);
    if (!bio)
        return NULL;
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);
    return (void*)pkey;
}

TML_EXPORT void* crypto_public_key_from_der(void* buffer_handle) {
    TmlBuffer* buf = (TmlBuffer*)buffer_handle;
    if (!buf || buf->length <= 0)
        return NULL;
    const unsigned char* p = buf->data;
    EVP_PKEY* pkey = d2i_PUBKEY(NULL, &p, (long)buf->length);
    return (void*)pkey;
}

TML_EXPORT void* crypto_public_key_from_jwk(const char* jwk) {
    // TODO: Implement JWK import
    (void)jwk;
    return NULL;
}

TML_EXPORT const char* crypto_public_key_to_pem(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return tml_strdup("");
    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio)
        return tml_strdup("");
    if (PEM_write_bio_PUBKEY(bio, pkey) != 1) {
        BIO_free(bio);
        return tml_strdup("");
    }
    char* result = tml_bio_to_str(bio);
    BIO_free(bio);
    return result;
}

TML_EXPORT void* crypto_public_key_to_der(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return NULL;
    unsigned char* der = NULL;
    int len = i2d_PUBKEY(pkey, &der);
    if (len <= 0 || !der)
        return NULL;
    TmlBuffer* buf = tml_create_buffer_with_data(der, len);
    OPENSSL_free(der);
    return (void*)buf;
}

TML_EXPORT const char* crypto_public_key_to_jwk(void* handle) {
    // TODO: Implement JWK export
    (void)handle;
    return tml_strdup("");
}

TML_EXPORT void crypto_public_key_destroy(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (pkey)
        EVP_PKEY_free(pkey);
}

// ============================================================================
// Key metadata
// ============================================================================

TML_EXPORT const char* crypto_key_get_type(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return tml_strdup("unknown");
    int type = EVP_PKEY_base_id(pkey);
    switch (type) {
    case EVP_PKEY_RSA:
        return tml_strdup("rsa");
    case EVP_PKEY_RSA_PSS:
        return tml_strdup("rsa-pss");
    case EVP_PKEY_DSA:
        return tml_strdup("dsa");
    case EVP_PKEY_DH:
        return tml_strdup("dh");
    case EVP_PKEY_EC:
        return tml_strdup("ec");
    case EVP_PKEY_ED25519:
        return tml_strdup("ed25519");
    case EVP_PKEY_ED448:
        return tml_strdup("ed448");
    case EVP_PKEY_X25519:
        return tml_strdup("x25519");
    case EVP_PKEY_X448:
        return tml_strdup("x448");
    default:
        return tml_strdup("unknown");
    }
}

TML_EXPORT int64_t crypto_key_size_bits(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return 0;
    return (int64_t)EVP_PKEY_bits(pkey);
}

TML_EXPORT int32_t crypto_key_equals(void* handle1, void* handle2) {
    EVP_PKEY* a = (EVP_PKEY*)handle1;
    EVP_PKEY* b = (EVP_PKEY*)handle2;
    if (!a || !b)
        return 0;
    return EVP_PKEY_eq(a, b) == 1 ? 1 : 0;
}

TML_EXPORT int64_t crypto_rsa_get_modulus_length(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return 0;
    if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA && EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA_PSS)
        return 0;
    return (int64_t)EVP_PKEY_bits(pkey);
}

TML_EXPORT int64_t crypto_rsa_get_public_exponent(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return 0;
    BIGNUM* e = NULL;
    if (EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &e) != 1 || !e)
        return 0;
    int64_t result = (int64_t)BN_get_word(e);
    BN_free(e);
    return result;
}

TML_EXPORT const char* crypto_ec_get_curve_name(void* handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)handle;
    if (!pkey)
        return tml_strdup("");
    char name[256] = {0};
    size_t name_len = sizeof(name);
    if (EVP_PKEY_get_utf8_string_param(pkey, OSSL_PKEY_PARAM_GROUP_NAME, name, name_len,
                                       &name_len) != 1) {
        return tml_strdup("");
    }
    return tml_strdup(name);
}

TML_EXPORT const char* crypto_jwk_extract_k(const char* jwk) {
    // Simple JSON parser to extract "k" field from JWK
    // JWK format: {"kty":"oct","k":"base64url-encoded-key"}
    if (!jwk)
        return tml_strdup("");
    const char* k_start = strstr(jwk, "\"k\"");
    if (!k_start)
        return tml_strdup("");
    k_start = strchr(k_start + 3, '"');
    if (!k_start)
        return tml_strdup("");
    k_start++; // skip opening quote
    const char* k_end = strchr(k_start, '"');
    if (!k_end)
        return tml_strdup("");
    size_t len = k_end - k_start;
    char* result = (char*)mem_alloc((int64_t)(len + 1));
    if (!result)
        return tml_strdup("");
    memcpy(result, k_start, len);
    result[len] = '\0';
    return result;
}

// ============================================================================
// Key generation
// ============================================================================

TML_EXPORT void* crypto_generate_rsa_key(int64_t bits, int64_t exponent) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, (int)bits) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (exponent > 0) {
        BIGNUM* e = BN_new();
        BN_set_word(e, (unsigned long)exponent);
        EVP_PKEY_CTX_set1_rsa_keygen_pubexp(ctx, e);
        BN_free(e);
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

TML_EXPORT void* crypto_generate_rsa_pss_key(int64_t bits, int64_t exponent) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA_PSS, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, (int)bits) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (exponent > 0) {
        BIGNUM* e = BN_new();
        BN_set_word(e, (unsigned long)exponent);
        EVP_PKEY_CTX_set1_rsa_keygen_pubexp(ctx, e);
        BN_free(e);
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

TML_EXPORT void* crypto_generate_dsa_key(int64_t bits) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DSA, NULL);
    if (!ctx)
        return NULL;

    // Generate DSA parameters
    EVP_PKEY* params = NULL;
    if (EVP_PKEY_paramgen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (EVP_PKEY_CTX_set_dsa_paramgen_bits(ctx, (int)bits) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (EVP_PKEY_paramgen(ctx, &params) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);

    // Generate key from parameters
    ctx = EVP_PKEY_CTX_new(params, NULL);
    EVP_PKEY_free(params);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

TML_EXPORT void* crypto_generate_ec_key(const char* curve_name) {
    if (!curve_name)
        return NULL;
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    // Map common names to OpenSSL NID
    int nid = OBJ_txt2nid(curve_name);
    if (nid == NID_undef) {
        // Try alternate names
        if (strcmp(curve_name, "P-256") == 0)
            nid = NID_X9_62_prime256v1;
        else if (strcmp(curve_name, "P-384") == 0)
            nid = NID_secp384r1;
        else if (strcmp(curve_name, "P-521") == 0)
            nid = NID_secp521r1;
        else {
            EVP_PKEY_CTX_free(ctx);
            return NULL;
        }
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, nid) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

TML_EXPORT void* crypto_generate_ed25519_key(void) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

TML_EXPORT void* crypto_generate_ed448_key(void) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED448, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

TML_EXPORT void* crypto_generate_x25519_key(void) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

TML_EXPORT void* crypto_generate_x448_key(void) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X448, NULL);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

TML_EXPORT void* crypto_generate_dh_key(int64_t bits) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, NULL);
    if (!ctx)
        return NULL;

    // Generate DH parameters
    EVP_PKEY* params = NULL;
    if (EVP_PKEY_paramgen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (EVP_PKEY_CTX_set_dh_paramgen_prime_len(ctx, (int)bits) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    if (EVP_PKEY_paramgen(ctx, &params) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);

    // Generate key from parameters
    ctx = EVP_PKEY_CTX_new(params, NULL);
    EVP_PKEY_free(params);
    if (!ctx)
        return NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }
    EVP_PKEY_CTX_free(ctx);
    return (void*)pkey;
}

#else /* !TML_HAS_OPENSSL */

// ============================================================================
// Stubs when OpenSSL is not available
// ============================================================================

TML_EXPORT void* crypto_secret_key_create(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void* crypto_secret_key_export(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void crypto_secret_key_destroy(void* handle) {
    (void)handle;
}
TML_EXPORT void* crypto_generate_secret_key(int64_t size) {
    (void)size;
    return NULL;
}

TML_EXPORT void* crypto_private_key_from_pem(const char* pem) {
    (void)pem;
    return NULL;
}
TML_EXPORT void* crypto_private_key_from_pem_encrypted(const char* pem, const char* pass) {
    (void)pem;
    (void)pass;
    return NULL;
}
TML_EXPORT void* crypto_private_key_from_der(void* buf) {
    (void)buf;
    return NULL;
}
TML_EXPORT void* crypto_private_key_from_jwk(const char* jwk) {
    (void)jwk;
    return NULL;
}
TML_EXPORT const char* crypto_private_key_to_pem(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_private_key_to_pem_encrypted(void* handle, const char* pass,
                                                           const char* cipher) {
    (void)handle;
    (void)pass;
    (void)cipher;
    return "";
}
TML_EXPORT void* crypto_private_key_to_der(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT const char* crypto_private_key_to_jwk(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT void* crypto_private_key_get_public(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT void crypto_private_key_destroy(void* handle) {
    (void)handle;
}

TML_EXPORT void* crypto_public_key_from_pem(const char* pem) {
    (void)pem;
    return NULL;
}
TML_EXPORT void* crypto_public_key_from_der(void* buf) {
    (void)buf;
    return NULL;
}
TML_EXPORT void* crypto_public_key_from_jwk(const char* jwk) {
    (void)jwk;
    return NULL;
}
TML_EXPORT const char* crypto_public_key_to_pem(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT void* crypto_public_key_to_der(void* handle) {
    (void)handle;
    return NULL;
}
TML_EXPORT const char* crypto_public_key_to_jwk(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT void crypto_public_key_destroy(void* handle) {
    (void)handle;
}

TML_EXPORT const char* crypto_key_get_type(void* handle) {
    (void)handle;
    return "unknown";
}
TML_EXPORT int64_t crypto_key_size_bits(void* handle) {
    (void)handle;
    return 0;
}
TML_EXPORT int32_t crypto_key_equals(void* h1, void* h2) {
    (void)h1;
    (void)h2;
    return 0;
}
TML_EXPORT int64_t crypto_rsa_get_modulus_length(void* handle) {
    (void)handle;
    return 0;
}
TML_EXPORT int64_t crypto_rsa_get_public_exponent(void* handle) {
    (void)handle;
    return 0;
}
TML_EXPORT const char* crypto_ec_get_curve_name(void* handle) {
    (void)handle;
    return "";
}
TML_EXPORT const char* crypto_jwk_extract_k(const char* jwk) {
    (void)jwk;
    return "";
}

TML_EXPORT void* crypto_generate_rsa_key(int64_t bits, int64_t exp) {
    (void)bits;
    (void)exp;
    return NULL;
}
TML_EXPORT void* crypto_generate_rsa_pss_key(int64_t bits, int64_t exp) {
    (void)bits;
    (void)exp;
    return NULL;
}
TML_EXPORT void* crypto_generate_dsa_key(int64_t bits) {
    (void)bits;
    return NULL;
}
TML_EXPORT void* crypto_generate_ec_key(const char* curve) {
    (void)curve;
    return NULL;
}
TML_EXPORT void* crypto_generate_ed25519_key(void) {
    return NULL;
}
TML_EXPORT void* crypto_generate_ed448_key(void) {
    return NULL;
}
TML_EXPORT void* crypto_generate_x25519_key(void) {
    return NULL;
}
TML_EXPORT void* crypto_generate_x448_key(void) {
    return NULL;
}
TML_EXPORT void* crypto_generate_dh_key(int64_t bits) {
    (void)bits;
    return NULL;
}

#endif /* TML_HAS_OPENSSL */
