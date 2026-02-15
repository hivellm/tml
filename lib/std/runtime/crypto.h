/**
 * TML Crypto Runtime Header
 *
 * This header declares the FFI functions for the TML crypto module.
 * Implementation uses OpenSSL for cryptographic operations.
 */

#ifndef TML_CRYPTO_H
#define TML_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Buffer type (opaque handle)
// ============================================================================

typedef struct TmlBuffer TmlBuffer;

TmlBuffer* tml_buffer_create(size_t size);
TmlBuffer* tml_buffer_from_data(const uint8_t* data, size_t len);
void tml_buffer_destroy(TmlBuffer* buf);
uint8_t* tml_buffer_data(TmlBuffer* buf);
size_t tml_buffer_len(TmlBuffer* buf);

// ============================================================================
// Hash functions
// ============================================================================

// One-shot hash functions
TmlBuffer* crypto_md5(const char* data);
TmlBuffer* crypto_md5_bytes(TmlBuffer* data);
TmlBuffer* crypto_sha1(const char* data);
TmlBuffer* crypto_sha1_bytes(TmlBuffer* data);
TmlBuffer* crypto_sha256(const char* data);
TmlBuffer* crypto_sha256_bytes(TmlBuffer* data);
TmlBuffer* crypto_sha384(const char* data);
TmlBuffer* crypto_sha384_bytes(TmlBuffer* data);
TmlBuffer* crypto_sha512(const char* data);
TmlBuffer* crypto_sha512_bytes(TmlBuffer* data);
TmlBuffer* crypto_sha512_256(const char* data);
TmlBuffer* crypto_sha512_256_bytes(TmlBuffer* data);
TmlBuffer* crypto_sha3_256(const char* data);
TmlBuffer* crypto_sha3_256_bytes(TmlBuffer* data);
TmlBuffer* crypto_sha3_384(const char* data);
TmlBuffer* crypto_sha3_384_bytes(TmlBuffer* data);
TmlBuffer* crypto_sha3_512(const char* data);
TmlBuffer* crypto_sha3_512_bytes(TmlBuffer* data);
TmlBuffer* crypto_blake2b512(const char* data);
TmlBuffer* crypto_blake2b512_bytes(TmlBuffer* data);
TmlBuffer* crypto_blake2b_custom(const char* data, int64_t output_len);
TmlBuffer* crypto_blake2s256(const char* data);
TmlBuffer* crypto_blake2s256_bytes(TmlBuffer* data);
TmlBuffer* crypto_blake3(const char* data);
TmlBuffer* crypto_blake3_bytes(TmlBuffer* data);
TmlBuffer* crypto_blake3_keyed(const char* data, TmlBuffer* key);
TmlBuffer* crypto_blake3_keyed_str(const char* key, const char* data);
TmlBuffer* crypto_blake3_keyed_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_blake3_derive_key(const char* context, TmlBuffer* input);

// Streaming hash
void* crypto_hash_create(const char* algorithm);
void crypto_hash_update_str(void* ctx, const char* data);
void crypto_hash_update_bytes(void* ctx, TmlBuffer* data);
TmlBuffer* crypto_hash_digest(void* ctx);
void* crypto_hash_copy(void* ctx);
void crypto_hash_destroy(void* ctx);

// ============================================================================
// HMAC functions
// ============================================================================

// One-shot HMAC
TmlBuffer* crypto_hmac_md5(const char* key, const char* data);
TmlBuffer* crypto_hmac_md5_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_sha1(const char* key, const char* data);
TmlBuffer* crypto_hmac_sha1_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_sha256(const char* key, const char* data);
TmlBuffer* crypto_hmac_sha256_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_sha384(const char* key, const char* data);
TmlBuffer* crypto_hmac_sha384_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_sha512(const char* key, const char* data);
TmlBuffer* crypto_hmac_sha512_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_sha512_256(const char* key, const char* data);
TmlBuffer* crypto_hmac_sha512_256_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_sha3_256(const char* key, const char* data);
TmlBuffer* crypto_hmac_sha3_256_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_sha3_384(const char* key, const char* data);
TmlBuffer* crypto_hmac_sha3_384_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_sha3_512(const char* key, const char* data);
TmlBuffer* crypto_hmac_sha3_512_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_blake2b(const char* key, const char* data);
TmlBuffer* crypto_hmac_blake2b_bytes(TmlBuffer* key, TmlBuffer* data);
TmlBuffer* crypto_hmac_blake2s(const char* key, const char* data);
TmlBuffer* crypto_hmac_blake2s_bytes(TmlBuffer* key, TmlBuffer* data);

// Streaming HMAC
void* crypto_hmac_create(const char* algorithm, const char* key);
void* crypto_hmac_create_bytes(const char* algorithm, TmlBuffer* key);
void crypto_hmac_update_str(void* ctx, const char* data);
void crypto_hmac_update_bytes(void* ctx, TmlBuffer* data);
TmlBuffer* crypto_hmac_digest(void* ctx);
void crypto_hmac_destroy(void* ctx);

// ============================================================================
// Cipher functions
// ============================================================================

void* crypto_cipher_create(const char* algorithm, TmlBuffer* key, TmlBuffer* iv, int encrypt);
void crypto_cipher_set_aad(void* ctx, TmlBuffer* aad);
void crypto_cipher_set_aad_str(void* ctx, const char* aad);
void crypto_cipher_set_padding(void* ctx, bool enabled);
void crypto_cipher_update_str(void* ctx, const char* data, TmlBuffer* output);
void crypto_cipher_update_bytes(void* ctx, TmlBuffer* data, TmlBuffer* output);
bool crypto_cipher_finalize(void* ctx, TmlBuffer* output);
TmlBuffer* crypto_cipher_get_tag(void* ctx);
void crypto_cipher_set_tag(void* ctx, TmlBuffer* tag);
void crypto_cipher_destroy(void* ctx);

// AES-GCM helpers
TmlBuffer* crypto_aes_gcm_encrypt(TmlBuffer* key, TmlBuffer* nonce, TmlBuffer* plaintext,
                                  TmlBuffer* aad);
TmlBuffer* crypto_aes_gcm_decrypt(TmlBuffer* key, TmlBuffer* nonce, TmlBuffer* ciphertext,
                                  TmlBuffer* aad, TmlBuffer* tag);
TmlBuffer* crypto_aes_gcm_get_tag(TmlBuffer* ciphertext);

// ============================================================================
// Sign/Verify functions
// ============================================================================

void* crypto_signer_create(const char* algorithm, void* private_key);
void crypto_signer_update_str(void* ctx, const char* data);
void crypto_signer_update_bytes(void* ctx, TmlBuffer* data);
TmlBuffer* crypto_signer_sign(void* ctx);
void crypto_signer_destroy(void* ctx);

void* crypto_verifier_create(const char* algorithm, void* public_key);
void crypto_verifier_update_str(void* ctx, const char* data);
void crypto_verifier_update_bytes(void* ctx, TmlBuffer* data);
bool crypto_verifier_verify(void* ctx, TmlBuffer* signature);
void crypto_verifier_destroy(void* ctx);

TmlBuffer* crypto_sign_rsa_pss(void* key, const char* data, int64_t salt_len,
                               const char* mgf1_hash);
bool crypto_verify_rsa_pss(void* key, const char* data, TmlBuffer* sig, int64_t salt_len,
                           const char* mgf1_hash);

// ============================================================================
// Key functions
// ============================================================================

// Secret key
void* crypto_secret_key_create(TmlBuffer* data);
TmlBuffer* crypto_secret_key_export(void* key);
void crypto_secret_key_destroy(void* key);
void* crypto_generate_secret_key(int64_t size);

// Private key
void* crypto_private_key_from_pem(const char* pem);
void* crypto_private_key_from_pem_encrypted(const char* pem, const char* passphrase);
void* crypto_private_key_from_der(TmlBuffer* der);
void* crypto_private_key_from_jwk(const char* jwk);
char* crypto_private_key_to_pem(void* key);
char* crypto_private_key_to_pem_encrypted(void* key, const char* passphrase, const char* cipher);
TmlBuffer* crypto_private_key_to_der(void* key);
char* crypto_private_key_to_jwk(void* key);
void* crypto_private_key_get_public(void* key);
void crypto_private_key_destroy(void* key);

// Public key
void* crypto_public_key_from_pem(const char* pem);
void* crypto_public_key_from_der(TmlBuffer* der);
void* crypto_public_key_from_jwk(const char* jwk);
char* crypto_public_key_to_pem(void* key);
TmlBuffer* crypto_public_key_to_der(void* key);
char* crypto_public_key_to_jwk(void* key);
void crypto_public_key_destroy(void* key);

// Key info
const char* crypto_key_get_type(void* key);
int64_t crypto_key_size_bits(void* key);
bool crypto_key_equals(void* key1, void* key2);
int64_t crypto_rsa_get_modulus_length(void* key);
int64_t crypto_rsa_get_public_exponent(void* key);
const char* crypto_ec_get_curve_name(void* key);

// Key generation
void* crypto_generate_rsa_key(int64_t bits, int64_t exponent);
void* crypto_generate_rsa_pss_key(int64_t bits, int64_t exponent);
void* crypto_generate_dsa_key(int64_t bits);
void* crypto_generate_ec_key(const char* curve);
void* crypto_generate_ed25519_key(void);
void* crypto_generate_ed448_key(void);
void* crypto_generate_x25519_key(void);
void* crypto_generate_x448_key(void);
void* crypto_generate_dh_key(int64_t bits);

// ============================================================================
// KDF functions
// ============================================================================

TmlBuffer* crypto_pbkdf2(const char* password, TmlBuffer* salt, int64_t iterations, int64_t keylen,
                         const char* digest);
TmlBuffer* crypto_pbkdf2_bytes(TmlBuffer* password, TmlBuffer* salt, int64_t iterations,
                               int64_t keylen, const char* digest);
TmlBuffer* crypto_scrypt(const char* password, TmlBuffer* salt, int64_t keylen, int64_t n,
                         int64_t r, int64_t p, int64_t maxmem);
TmlBuffer* crypto_scrypt_bytes(TmlBuffer* password, TmlBuffer* salt, int64_t keylen, int64_t n,
                               int64_t r, int64_t p, int64_t maxmem);
TmlBuffer* crypto_hkdf(const char* digest, TmlBuffer* ikm, TmlBuffer* salt, const char* info,
                       int64_t keylen);
TmlBuffer* crypto_hkdf_bytes(const char* digest, TmlBuffer* ikm, TmlBuffer* salt, TmlBuffer* info,
                             int64_t keylen);
TmlBuffer* crypto_hkdf_extract(const char* digest, TmlBuffer* ikm, TmlBuffer* salt);
TmlBuffer* crypto_hkdf_expand(const char* digest, TmlBuffer* prk, TmlBuffer* info, int64_t keylen);
TmlBuffer* crypto_argon2(const char* variant, const char* password, TmlBuffer* salt, int64_t keylen,
                         int64_t t, int64_t m, int64_t p);
TmlBuffer* crypto_argon2_bytes(const char* variant, TmlBuffer* password, TmlBuffer* salt,
                               int64_t keylen, int64_t t, int64_t m, int64_t p);

// ============================================================================
// Random functions
// ============================================================================

TmlBuffer* crypto_random_bytes(int64_t size);
void crypto_random_fill(TmlBuffer* buf);
void crypto_random_fill_range(TmlBuffer* buf, int64_t offset, int64_t size);
int64_t crypto_random_int(int64_t min, int64_t max);
uint8_t crypto_random_u8(void);
uint16_t crypto_random_u16(void);
uint32_t crypto_random_u32(void);
uint64_t crypto_random_u64(void);
int32_t crypto_random_i32(void);
int64_t crypto_random_i64(void);
float crypto_random_f32(void);
double crypto_random_f64(void);
char* crypto_random_uuid(void);
bool crypto_timing_safe_equal(TmlBuffer* a, TmlBuffer* b);
bool crypto_timing_safe_equal_str(const char* a, const char* b);
TmlBuffer* crypto_generate_prime(int64_t bits);
TmlBuffer* crypto_generate_safe_prime(int64_t bits);
bool crypto_check_prime(TmlBuffer* candidate);
bool crypto_check_prime_rounds(TmlBuffer* candidate, int64_t rounds);

// ============================================================================
// X.509 functions
// ============================================================================

void* crypto_x509_from_pem(const char* pem);
void* crypto_x509_from_der(TmlBuffer* der);
char* crypto_x509_get_subject(void* cert);
char* crypto_x509_get_subject_cn(void* cert);
char* crypto_x509_get_subject_o(void* cert);
char* crypto_x509_get_subject_ou(void* cert);
char* crypto_x509_get_subject_c(void* cert);
char* crypto_x509_get_subject_st(void* cert);
char* crypto_x509_get_subject_l(void* cert);
char* crypto_x509_get_issuer(void* cert);
char* crypto_x509_get_issuer_cn(void* cert);
char* crypto_x509_get_issuer_o(void* cert);
char* crypto_x509_get_serial(void* cert);
char* crypto_x509_get_not_before(void* cert);
char* crypto_x509_get_not_after(void* cert);
int64_t crypto_x509_get_not_before_ts(void* cert);
int64_t crypto_x509_get_not_after_ts(void* cert);
char* crypto_x509_fingerprint_sha1(void* cert);
char* crypto_x509_fingerprint_sha256(void* cert);
char* crypto_x509_fingerprint_sha512(void* cert);
void* crypto_x509_get_public_key(void* cert);
char* crypto_x509_get_sig_alg(void* cert);
bool crypto_x509_is_ca(void* cert);
int64_t crypto_x509_get_key_usage(void* cert);
void* crypto_x509_get_san(void* cert);
bool crypto_x509_verify(void* cert, void* public_key);
bool crypto_x509_check_issued(void* cert, void* issuer);
bool crypto_x509_check_host(void* cert, const char* hostname);
bool crypto_x509_check_email(void* cert, const char* email);
bool crypto_x509_check_ip(void* cert, const char* ip);
bool crypto_x509_check_private_key(void* cert, void* key);
bool crypto_x509_is_valid_now(void* cert);
char* crypto_x509_to_pem(void* cert);
TmlBuffer* crypto_x509_to_der(void* cert);
char* crypto_x509_to_text(void* cert);
void crypto_x509_destroy(void* cert);

// X.509 store
void* crypto_x509_store_create(void);
void* crypto_x509_store_system(void);
bool crypto_x509_store_add_cert(void* store, void* cert);
int64_t crypto_x509_store_add_pem_file(void* store, const char* path);
bool crypto_x509_store_verify(void* store, void* cert);
bool crypto_x509_store_verify_chain(void* store, void* cert, void* chain_handles);
void crypto_x509_store_destroy(void* store);

// PEM helpers
int64_t crypto_x509_count_pem_certs(const char* pem);
char* crypto_x509_extract_pem_cert(const char* pem, int64_t index);

// ============================================================================
// Diffie-Hellman functions
// ============================================================================

void* crypto_dh_create(TmlBuffer* prime, TmlBuffer* generator);
void* crypto_dh_generate(int64_t prime_length);
void* crypto_dh_create_group(const char* group_name);
void crypto_dh_generate_keys(void* dh);
TmlBuffer* crypto_dh_get_public_key(void* dh);
TmlBuffer* crypto_dh_get_private_key(void* dh);
void crypto_dh_set_public_key(void* dh, TmlBuffer* key);
void crypto_dh_set_private_key(void* dh, TmlBuffer* key);
TmlBuffer* crypto_dh_get_prime(void* dh);
TmlBuffer* crypto_dh_get_generator(void* dh);
TmlBuffer* crypto_dh_compute_secret(void* dh, TmlBuffer* other_public);
int64_t crypto_dh_check(void* dh);
void crypto_dh_destroy(void* dh);
TmlBuffer* crypto_dh_group_get_prime(const char* group_name);
TmlBuffer* crypto_dh_group_get_generator(const char* group_name);

// ============================================================================
// ECDH functions
// ============================================================================

void* crypto_ecdh_create(const char* curve_name);
void crypto_ecdh_generate_keys(void* ecdh);
TmlBuffer* crypto_ecdh_get_public_key(void* ecdh, const char* format);
TmlBuffer* crypto_ecdh_get_private_key(void* ecdh);
bool crypto_ecdh_set_public_key(void* ecdh, TmlBuffer* key);
bool crypto_ecdh_set_private_key(void* ecdh, TmlBuffer* key);
TmlBuffer* crypto_ecdh_compute_secret(void* ecdh, TmlBuffer* other_public);
void crypto_ecdh_destroy(void* ecdh);
TmlBuffer* crypto_ecdh_convert_key(TmlBuffer* key, const char* curve, const char* from_fmt,
                                   const char* to_fmt);
TmlBuffer* crypto_x25519(TmlBuffer* private_key, TmlBuffer* public_key);
TmlBuffer* crypto_x448(TmlBuffer* private_key, TmlBuffer* public_key);
TmlBuffer* crypto_x25519_generate_private(void);
TmlBuffer* crypto_x448_generate_private(void);
TmlBuffer* crypto_x25519_public_from_private(TmlBuffer* private_key);
TmlBuffer* crypto_x448_public_from_private(TmlBuffer* private_key);
void* crypto_get_curves(void);
bool crypto_is_curve_supported(const char* curve_name);

// ============================================================================
// RSA encryption functions
// ============================================================================

TmlBuffer* crypto_rsa_public_encrypt(void* key, TmlBuffer* data, const char* padding);
TmlBuffer* crypto_rsa_public_encrypt_oaep(void* key, TmlBuffer* data, const char* hash,
                                          const char* mgf1_hash, TmlBuffer* label);
TmlBuffer* crypto_rsa_private_decrypt(void* key, TmlBuffer* data, const char* padding);
TmlBuffer* crypto_rsa_private_decrypt_oaep(void* key, TmlBuffer* data, const char* hash,
                                           const char* mgf1_hash, TmlBuffer* label);
TmlBuffer* crypto_rsa_private_encrypt(void* key, TmlBuffer* data, const char* padding);
TmlBuffer* crypto_rsa_public_decrypt(void* key, TmlBuffer* data, const char* padding);

// ============================================================================
// Utility functions
// ============================================================================

char* crypto_bytes_to_hex(TmlBuffer* data);
TmlBuffer* crypto_hex_to_bytes(const char* hex);
char* crypto_bytes_to_base64(TmlBuffer* data);
TmlBuffer* crypto_base64_to_bytes(const char* b64);
char* crypto_bytes_to_base64url(TmlBuffer* data);
TmlBuffer* crypto_base64url_to_bytes(const char* b64url);
TmlBuffer* crypto_str_to_bytes(const char* str);
char* crypto_bytes_to_str(TmlBuffer* data);
TmlBuffer* crypto_buffer_slice(TmlBuffer* buf, int64_t offset, int64_t len);
TmlBuffer* crypto_concat_buffers3(TmlBuffer* a, TmlBuffer* b, TmlBuffer* c);
char* crypto_jwk_extract_k(const char* jwk);

// Algorithm info
void* crypto_get_hashes(void);
void* crypto_get_ciphers(void);
bool crypto_cipher_exists(const char* name);
int64_t crypto_cipher_key_length(const char* name);
int64_t crypto_cipher_iv_length(const char* name);
int64_t crypto_cipher_block_size(const char* name);
char* crypto_cipher_mode(const char* name);

// FIPS
bool crypto_fips_mode(void);
bool crypto_set_fips_mode(bool enabled);
int64_t crypto_secure_heap_used(void);
bool crypto_set_engine(const char* engine_id);

#ifdef __cplusplus
}
#endif

#endif // TML_CRYPTO_H