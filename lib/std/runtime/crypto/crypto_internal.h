/**
 * TML Crypto Runtime - Internal Header
 *
 * Internal definitions shared between crypto implementation files.
 */

#ifndef TML_CRYPTO_INTERNAL_H
#define TML_CRYPTO_INTERNAL_H

#include "../crypto.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
#define TML_PLATFORM_WINDOWS 1
#define TML_PLATFORM_NAME "windows"
#elif defined(__APPLE__)
#define TML_PLATFORM_MACOS 1
#define TML_PLATFORM_NAME "macos"
#elif defined(__linux__)
#define TML_PLATFORM_LINUX 1
#define TML_PLATFORM_NAME "linux"
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define TML_PLATFORM_BSD 1
#define TML_PLATFORM_NAME "bsd"
#else
#define TML_PLATFORM_UNIX 1
#define TML_PLATFORM_NAME "unix"
#endif

// ============================================================================
// Buffer Structure (Internal)
// ============================================================================

struct TmlBuffer {
    uint8_t* data;
    size_t len;
    size_t capacity;
};

// Buffer helpers
TmlBuffer* tml_buffer_from_string(const char* str);
void tml_buffer_resize(TmlBuffer* buf, size_t new_len);
void tml_buffer_append(TmlBuffer* buf, const uint8_t* data, size_t len);
TmlBuffer* tml_buffer_slice(TmlBuffer* buf, size_t offset, size_t len);
TmlBuffer* tml_buffer_concat(TmlBuffer* a, TmlBuffer* b);

// ============================================================================
// List Structure (for algorithm lists)
// ============================================================================

typedef struct TmlList {
    char** items;
    size_t len;
    size_t capacity;
} TmlList;

TmlList* tml_list_create(size_t initial_capacity);
void tml_list_destroy(TmlList* list);
void tml_list_push(TmlList* list, const char* item);

// ============================================================================
// Hash Context (platform-specific implementation)
// ============================================================================

typedef struct TmlHashContext TmlHashContext;

TmlHashContext* hash_context_create(const char* algorithm);
void hash_context_update(TmlHashContext* ctx, const uint8_t* data, size_t len);
TmlBuffer* hash_context_digest(TmlHashContext* ctx);
TmlHashContext* hash_context_copy(TmlHashContext* ctx);
void hash_context_destroy(TmlHashContext* ctx);

// ============================================================================
// HMAC Context (platform-specific implementation)
// ============================================================================

typedef struct TmlHmacContext TmlHmacContext;

TmlHmacContext* hmac_context_create(const char* algorithm, const uint8_t* key, size_t key_len);
void hmac_context_update(TmlHmacContext* ctx, const uint8_t* data, size_t len);
TmlBuffer* hmac_context_digest(TmlHmacContext* ctx);
void hmac_context_destroy(TmlHmacContext* ctx);

// ============================================================================
// Cipher Context (platform-specific implementation)
// ============================================================================

typedef struct TmlCipherContext TmlCipherContext;

TmlCipherContext* cipher_context_create(const char* algorithm, const uint8_t* key, size_t key_len,
                                        const uint8_t* iv, size_t iv_len, bool encrypt);
void cipher_context_set_aad(TmlCipherContext* ctx, const uint8_t* aad, size_t aad_len);
void cipher_context_set_padding(TmlCipherContext* ctx, bool enabled);
size_t cipher_context_update(TmlCipherContext* ctx, const uint8_t* input, size_t input_len,
                             uint8_t* output, size_t output_size);
size_t cipher_context_finalize(TmlCipherContext* ctx, uint8_t* output, size_t output_size,
                               bool* success);
TmlBuffer* cipher_context_get_tag(TmlCipherContext* ctx);
void cipher_context_set_tag(TmlCipherContext* ctx, const uint8_t* tag, size_t tag_len);
void cipher_context_destroy(TmlCipherContext* ctx);

// ============================================================================
// Key Structures (platform-specific implementation)
// ============================================================================

typedef struct TmlSecretKey TmlSecretKey;
typedef struct TmlPrivateKey TmlPrivateKey;
typedef struct TmlPublicKey TmlPublicKey;

// Secret key
TmlSecretKey* secret_key_create(const uint8_t* data, size_t len);
TmlBuffer* secret_key_export(TmlSecretKey* key);
void secret_key_destroy(TmlSecretKey* key);

// Private key
TmlPrivateKey* private_key_from_pem(const char* pem);
TmlPrivateKey* private_key_from_pem_encrypted(const char* pem, const char* passphrase);
TmlPrivateKey* private_key_from_der(const uint8_t* der, size_t len);
TmlPrivateKey* private_key_from_jwk(const char* jwk);
char* private_key_to_pem(TmlPrivateKey* key);
char* private_key_to_pem_encrypted(TmlPrivateKey* key, const char* passphrase, const char* cipher);
TmlBuffer* private_key_to_der(TmlPrivateKey* key);
char* private_key_to_jwk(TmlPrivateKey* key);
TmlPublicKey* private_key_get_public(TmlPrivateKey* key);
void private_key_destroy(TmlPrivateKey* key);

// Public key
TmlPublicKey* public_key_from_pem(const char* pem);
TmlPublicKey* public_key_from_der(const uint8_t* der, size_t len);
TmlPublicKey* public_key_from_jwk(const char* jwk);
char* public_key_to_pem(TmlPublicKey* key);
TmlBuffer* public_key_to_der(TmlPublicKey* key);
char* public_key_to_jwk(TmlPublicKey* key);
void public_key_destroy(TmlPublicKey* key);

// Key info
const char* key_get_type(void* key);
int64_t key_size_bits(void* key);
bool key_equals(void* key1, void* key2);

// Key generation
TmlPrivateKey* generate_rsa_key(int bits, int64_t exponent);
TmlPrivateKey* generate_ec_key(const char* curve);
TmlPrivateKey* generate_ed25519_key(void);
TmlPrivateKey* generate_ed448_key(void);
TmlPrivateKey* generate_x25519_key(void);
TmlPrivateKey* generate_x448_key(void);

// ============================================================================
// Sign/Verify (platform-specific implementation)
// ============================================================================

typedef struct TmlSignerContext TmlSignerContext;
typedef struct TmlVerifierContext TmlVerifierContext;

TmlSignerContext* signer_context_create(const char* algorithm, TmlPrivateKey* key);
void signer_context_update(TmlSignerContext* ctx, const uint8_t* data, size_t len);
TmlBuffer* signer_context_sign(TmlSignerContext* ctx);
void signer_context_destroy(TmlSignerContext* ctx);

TmlVerifierContext* verifier_context_create(const char* algorithm, TmlPublicKey* key);
void verifier_context_update(TmlVerifierContext* ctx, const uint8_t* data, size_t len);
bool verifier_context_verify(TmlVerifierContext* ctx, const uint8_t* signature, size_t sig_len);
void verifier_context_destroy(TmlVerifierContext* ctx);

// ============================================================================
// Random (platform-specific implementation)
// ============================================================================

bool random_bytes(uint8_t* buffer, size_t len);

// ============================================================================
// KDF (platform-specific implementation)
// ============================================================================

TmlBuffer* kdf_pbkdf2(const uint8_t* password, size_t password_len, const uint8_t* salt,
                      size_t salt_len, int64_t iterations, int64_t key_len, const char* digest);

TmlBuffer* kdf_scrypt(const uint8_t* password, size_t password_len, const uint8_t* salt,
                      size_t salt_len, int64_t key_len, int64_t n, int64_t r, int64_t p,
                      int64_t maxmem);

TmlBuffer* kdf_hkdf(const char* digest, const uint8_t* ikm, size_t ikm_len, const uint8_t* salt,
                    size_t salt_len, const uint8_t* info, size_t info_len, int64_t key_len);

TmlBuffer* kdf_hkdf_extract(const char* digest, const uint8_t* ikm, size_t ikm_len,
                            const uint8_t* salt, size_t salt_len);

TmlBuffer* kdf_hkdf_expand(const char* digest, const uint8_t* prk, size_t prk_len,
                           const uint8_t* info, size_t info_len, int64_t key_len);

TmlBuffer* kdf_argon2(const char* variant, const uint8_t* password, size_t password_len,
                      const uint8_t* salt, size_t salt_len, int64_t key_len, int64_t t, int64_t m,
                      int64_t p);

// ============================================================================
// X.509 (platform-specific implementation)
// ============================================================================

typedef struct TmlX509Certificate TmlX509Certificate;
typedef struct TmlX509Store TmlX509Store;

TmlX509Certificate* x509_from_pem(const char* pem);
TmlX509Certificate* x509_from_der(const uint8_t* der, size_t len);
void x509_destroy(TmlX509Certificate* cert);

char* x509_get_subject(TmlX509Certificate* cert);
char* x509_get_subject_cn(TmlX509Certificate* cert);
char* x509_get_subject_o(TmlX509Certificate* cert);
char* x509_get_issuer(TmlX509Certificate* cert);
char* x509_get_issuer_cn(TmlX509Certificate* cert);
char* x509_get_serial(TmlX509Certificate* cert);
char* x509_get_not_before(TmlX509Certificate* cert);
char* x509_get_not_after(TmlX509Certificate* cert);
int64_t x509_get_not_before_ts(TmlX509Certificate* cert);
int64_t x509_get_not_after_ts(TmlX509Certificate* cert);
TmlBuffer* x509_fingerprint(TmlX509Certificate* cert, const char* algorithm);
TmlPublicKey* x509_get_public_key(TmlX509Certificate* cert);
bool x509_verify(TmlX509Certificate* cert, TmlPublicKey* key);
bool x509_check_host(TmlX509Certificate* cert, const char* hostname);
bool x509_is_ca(TmlX509Certificate* cert);
char* x509_to_pem(TmlX509Certificate* cert);
TmlBuffer* x509_to_der(TmlX509Certificate* cert);
char* x509_to_text(TmlX509Certificate* cert);

TmlX509Store* x509_store_create(void);
TmlX509Store* x509_store_system(void);
bool x509_store_add_cert(TmlX509Store* store, TmlX509Certificate* cert);
bool x509_store_verify(TmlX509Store* store, TmlX509Certificate* cert);
void x509_store_destroy(TmlX509Store* store);

// ============================================================================
// DH (platform-specific implementation)
// ============================================================================

typedef struct TmlDH TmlDH;

TmlDH* dh_create(const uint8_t* prime, size_t prime_len, const uint8_t* generator, size_t gen_len);
TmlDH* dh_create_group(const char* group_name);
TmlDH* dh_generate(int64_t prime_bits);
void dh_generate_keys(TmlDH* dh);
TmlBuffer* dh_get_public_key(TmlDH* dh);
TmlBuffer* dh_get_private_key(TmlDH* dh);
void dh_set_public_key(TmlDH* dh, const uint8_t* key, size_t len);
void dh_set_private_key(TmlDH* dh, const uint8_t* key, size_t len);
TmlBuffer* dh_get_prime(TmlDH* dh);
TmlBuffer* dh_get_generator(TmlDH* dh);
TmlBuffer* dh_compute_secret(TmlDH* dh, const uint8_t* other_public, size_t len);
int64_t dh_check(TmlDH* dh);
void dh_destroy(TmlDH* dh);

// ============================================================================
// ECDH (platform-specific implementation)
// ============================================================================

typedef struct TmlECDH TmlECDH;

TmlECDH* ecdh_create(const char* curve_name);
void ecdh_generate_keys(TmlECDH* ecdh);
TmlBuffer* ecdh_get_public_key(TmlECDH* ecdh, const char* format);
TmlBuffer* ecdh_get_private_key(TmlECDH* ecdh);
bool ecdh_set_public_key(TmlECDH* ecdh, const uint8_t* key, size_t len);
bool ecdh_set_private_key(TmlECDH* ecdh, const uint8_t* key, size_t len);
TmlBuffer* ecdh_compute_secret(TmlECDH* ecdh, const uint8_t* other_public, size_t len);
void ecdh_destroy(TmlECDH* ecdh);

TmlBuffer* x25519_compute(const uint8_t* private_key, const uint8_t* public_key);
TmlBuffer* x448_compute(const uint8_t* private_key, const uint8_t* public_key);
TmlBuffer* x25519_public_from_private(const uint8_t* private_key);
TmlBuffer* x448_public_from_private(const uint8_t* private_key);

// ============================================================================
// RSA Encryption (platform-specific implementation)
// ============================================================================

TmlBuffer* rsa_public_encrypt(TmlPublicKey* key, const uint8_t* data, size_t len,
                              const char* padding);
TmlBuffer* rsa_private_decrypt(TmlPrivateKey* key, const uint8_t* data, size_t len,
                               const char* padding);
TmlBuffer* rsa_private_encrypt(TmlPrivateKey* key, const uint8_t* data, size_t len,
                               const char* padding);
TmlBuffer* rsa_public_decrypt(TmlPublicKey* key, const uint8_t* data, size_t len,
                              const char* padding);

TmlBuffer* rsa_public_encrypt_oaep(TmlPublicKey* key, const uint8_t* data, size_t len,
                                   const char* hash, const char* mgf1_hash, const uint8_t* label,
                                   size_t label_len);
TmlBuffer* rsa_private_decrypt_oaep(TmlPrivateKey* key, const uint8_t* data, size_t len,
                                    const char* hash, const char* mgf1_hash, const uint8_t* label,
                                    size_t label_len);

// ============================================================================
// Algorithm Lists (platform-specific implementation)
// ============================================================================

TmlList* get_supported_hashes(void);
TmlList* get_supported_ciphers(void);
TmlList* get_supported_curves(void);
bool is_hash_supported(const char* name);
bool is_cipher_supported(const char* name);
bool is_curve_supported(const char* name);

// ============================================================================
// FIPS (platform-specific implementation)
// ============================================================================

bool fips_mode(void);
bool set_fips_mode(bool enabled);

// ============================================================================
// UUID Helper
// ============================================================================

char* format_uuid(const uint8_t* bytes);

#ifdef __cplusplus
}
#endif

#endif // TML_CRYPTO_INTERNAL_H