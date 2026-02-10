/**
 * @file crypto_rsa.c
 * @brief TML Runtime - RSA Encryption/Decryption and AES-GCM
 *
 * Implements RSA public/private key encryption/decryption with multiple
 * padding modes (PKCS#1 v1.5, OAEP with various hash algorithms, raw/none),
 * plus AES-256-GCM authenticated encryption for hybrid encryption schemes.
 *
 * Uses OpenSSL 3.0+ EVP API.
 */

#include "crypto_common.h"

#ifdef TML_HAS_OPENSSL

// ============================================================================
// Internal helpers
// ============================================================================

/**
 * Map a hash algorithm name string to an EVP_MD.
 * Supports: "sha1", "sha256", "sha384", "sha512".
 */
static const EVP_MD* get_oaep_md(const char* name) {
    if (!name)
        return EVP_sha256();
    if (strcmp(name, "sha1") == 0)
        return EVP_sha1();
    if (strcmp(name, "sha256") == 0)
        return EVP_sha256();
    if (strcmp(name, "sha384") == 0)
        return EVP_sha384();
    if (strcmp(name, "sha512") == 0)
        return EVP_sha512();
    return EVP_sha256();
}

/**
 * Configure RSA padding on an EVP_PKEY_CTX based on a padding mode string.
 *
 * Supported padding values:
 *   "pkcs1"       - RSA_PKCS1_PADDING
 *   "oaep-sha1"   - RSA_PKCS1_OAEP_PADDING with SHA-1
 *   "oaep-sha256" - RSA_PKCS1_OAEP_PADDING with SHA-256
 *   "oaep-sha384" - RSA_PKCS1_OAEP_PADDING with SHA-384
 *   "oaep-sha512" - RSA_PKCS1_OAEP_PADDING with SHA-512
 *   "none"        - RSA_NO_PADDING
 *
 * Returns 1 on success, 0 on failure.
 */
static int set_rsa_padding(EVP_PKEY_CTX* ctx, const char* padding) {
    if (!ctx || !padding)
        return 0;

    if (strcmp(padding, "pkcs1") == 0) {
        return EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) > 0;
    } else if (strcmp(padding, "oaep-sha1") == 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
            return 0;
        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha1()) <= 0)
            return 0;
        return 1;
    } else if (strcmp(padding, "oaep-sha256") == 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
            return 0;
        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0)
            return 0;
        return 1;
    } else if (strcmp(padding, "oaep-sha384") == 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
            return 0;
        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha384()) <= 0)
            return 0;
        return 1;
    } else if (strcmp(padding, "oaep-sha512") == 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
            return 0;
        if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha512()) <= 0)
            return 0;
        return 1;
    } else if (strcmp(padding, "none") == 0) {
        return EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_NO_PADDING) > 0;
    }

    return 0;
}

/**
 * Configure custom OAEP padding with specific hash, MGF1 hash, and optional label.
 * Returns 1 on success, 0 on failure.
 */
static int set_rsa_oaep_custom(EVP_PKEY_CTX* ctx, const char* hash, const char* mgf1_hash,
                               TmlBuffer* label) {
    if (!ctx)
        return 0;

    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0)
        return 0;

    const EVP_MD* oaep_md = get_oaep_md(hash);
    if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, oaep_md) <= 0)
        return 0;

    const EVP_MD* mgf1_md = get_oaep_md(mgf1_hash);
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, mgf1_md) <= 0)
        return 0;

    if (label && label->data && label->length > 0) {
        /* EVP_PKEY_CTX_set0_rsa_oaep_label takes ownership of the buffer,
         * so we must provide a copy allocated with OPENSSL_malloc. */
        unsigned char* label_copy = (unsigned char*)OPENSSL_malloc((size_t)label->length);
        if (!label_copy)
            return 0;
        memcpy(label_copy, label->data, (size_t)label->length);
        if (EVP_PKEY_CTX_set0_rsa_oaep_label(ctx, label_copy, (int)label->length) <= 0) {
            OPENSSL_free(label_copy);
            return 0;
        }
    }

    return 1;
}

// ============================================================================
// RSA Public Key Encryption
// ============================================================================

TML_EXPORT void* crypto_rsa_public_encrypt(void* key_handle, void* data_handle,
                                           const char* padding) {
    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    TmlBuffer* input = (TmlBuffer*)data_handle;
    if (!pkey || !input || !padding)
        return NULL;
    if (input->length <= 0)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (!set_rsa_padding(ctx, padding)) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    /* Determine output size */
    size_t outlen = 0;
    if (EVP_PKEY_encrypt(ctx, NULL, &outlen, input->data, (size_t)input->length) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    TmlBuffer* result = tml_create_buffer((int64_t)outlen);
    if (!result) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_encrypt(ctx, result->data, &outlen, input->data, (size_t)input->length) <= 0) {
        free(result->data);
        free(result);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    result->length = (int64_t)outlen;
    EVP_PKEY_CTX_free(ctx);
    return (void*)result;
}

// ============================================================================
// RSA Public Key Encryption with Custom OAEP
// ============================================================================

TML_EXPORT void* crypto_rsa_public_encrypt_oaep(void* key_handle, void* data_handle,
                                                const char* hash, const char* mgf1_hash,
                                                void* label_handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    TmlBuffer* input = (TmlBuffer*)data_handle;
    TmlBuffer* label = (TmlBuffer*)label_handle;
    if (!pkey || !input)
        return NULL;
    if (input->length <= 0)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (!set_rsa_oaep_custom(ctx, hash, mgf1_hash, label)) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    size_t outlen = 0;
    if (EVP_PKEY_encrypt(ctx, NULL, &outlen, input->data, (size_t)input->length) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    TmlBuffer* result = tml_create_buffer((int64_t)outlen);
    if (!result) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_encrypt(ctx, result->data, &outlen, input->data, (size_t)input->length) <= 0) {
        free(result->data);
        free(result);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    result->length = (int64_t)outlen;
    EVP_PKEY_CTX_free(ctx);
    return (void*)result;
}

// ============================================================================
// RSA Private Key Decryption
// ============================================================================

TML_EXPORT void* crypto_rsa_private_decrypt(void* key_handle, void* data_handle,
                                            const char* padding) {
    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    TmlBuffer* input = (TmlBuffer*)data_handle;
    if (!pkey || !input || !padding)
        return NULL;
    if (input->length <= 0)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (!set_rsa_padding(ctx, padding)) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    /* Determine output size */
    size_t outlen = 0;
    if (EVP_PKEY_decrypt(ctx, NULL, &outlen, input->data, (size_t)input->length) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    TmlBuffer* result = tml_create_buffer((int64_t)outlen);
    if (!result) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_decrypt(ctx, result->data, &outlen, input->data, (size_t)input->length) <= 0) {
        free(result->data);
        free(result);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    result->length = (int64_t)outlen;
    EVP_PKEY_CTX_free(ctx);
    return (void*)result;
}

// ============================================================================
// RSA Private Key Decryption with Custom OAEP
// ============================================================================

TML_EXPORT void* crypto_rsa_private_decrypt_oaep(void* key_handle, void* data_handle,
                                                 const char* hash, const char* mgf1_hash,
                                                 void* label_handle) {
    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    TmlBuffer* input = (TmlBuffer*)data_handle;
    TmlBuffer* label = (TmlBuffer*)label_handle;
    if (!pkey || !input)
        return NULL;
    if (input->length <= 0)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (!set_rsa_oaep_custom(ctx, hash, mgf1_hash, label)) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    size_t outlen = 0;
    if (EVP_PKEY_decrypt(ctx, NULL, &outlen, input->data, (size_t)input->length) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    TmlBuffer* result = tml_create_buffer((int64_t)outlen);
    if (!result) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_decrypt(ctx, result->data, &outlen, input->data, (size_t)input->length) <= 0) {
        free(result->data);
        free(result);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    result->length = (int64_t)outlen;
    EVP_PKEY_CTX_free(ctx);
    return (void*)result;
}

// ============================================================================
// RSA Private Key Encrypt (raw private key operation, e.g., for signatures)
// ============================================================================

TML_EXPORT void* crypto_rsa_private_encrypt(void* key_handle, void* data_handle,
                                            const char* padding) {
    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    TmlBuffer* input = (TmlBuffer*)data_handle;
    if (!pkey || !input || !padding)
        return NULL;
    if (input->length <= 0)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_sign_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    /* For raw private encrypt, use PKCS1 or no padding */
    if (strcmp(padding, "pkcs1") == 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            return NULL;
        }
    } else if (strcmp(padding, "none") == 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_NO_PADDING) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            return NULL;
        }
    } else {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    /* Determine output size */
    size_t outlen = 0;
    if (EVP_PKEY_sign(ctx, NULL, &outlen, input->data, (size_t)input->length) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    TmlBuffer* result = tml_create_buffer((int64_t)outlen);
    if (!result) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_sign(ctx, result->data, &outlen, input->data, (size_t)input->length) <= 0) {
        free(result->data);
        free(result);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    result->length = (int64_t)outlen;
    EVP_PKEY_CTX_free(ctx);
    return (void*)result;
}

// ============================================================================
// RSA Public Key Decrypt (raw public key operation, e.g., for verification)
// ============================================================================

TML_EXPORT void* crypto_rsa_public_decrypt(void* key_handle, void* data_handle,
                                           const char* padding) {
    EVP_PKEY* pkey = (EVP_PKEY*)key_handle;
    TmlBuffer* input = (TmlBuffer*)data_handle;
    if (!pkey || !input || !padding)
        return NULL;
    if (input->length <= 0)
        return NULL;

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (!ctx)
        return NULL;

    if (EVP_PKEY_verify_recover_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    /* For raw public decrypt, use PKCS1 or no padding */
    if (strcmp(padding, "pkcs1") == 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            return NULL;
        }
    } else if (strcmp(padding, "none") == 0) {
        if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_NO_PADDING) <= 0) {
            EVP_PKEY_CTX_free(ctx);
            return NULL;
        }
    } else {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    /* Determine output size */
    size_t outlen = 0;
    if (EVP_PKEY_verify_recover(ctx, NULL, &outlen, input->data, (size_t)input->length) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    TmlBuffer* result = tml_create_buffer((int64_t)outlen);
    if (!result) {
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    if (EVP_PKEY_verify_recover(ctx, result->data, &outlen, input->data, (size_t)input->length) <=
        0) {
        free(result->data);
        free(result);
        EVP_PKEY_CTX_free(ctx);
        return NULL;
    }

    result->length = (int64_t)outlen;
    EVP_PKEY_CTX_free(ctx);
    return (void*)result;
}

// ============================================================================
// AES-256-GCM Authenticated Encryption
// ============================================================================

/**
 * AES-256-GCM encrypt.
 *
 * @param key_handle   TmlBuffer* with 32 bytes (256-bit key)
 * @param nonce_handle TmlBuffer* with 12 bytes (96-bit IV/nonce)
 * @param data_handle  TmlBuffer* with plaintext
 * @param aad_handle   TmlBuffer* with additional authenticated data (may be NULL)
 * @return TmlBuffer* with ciphertext + 16-byte GCM tag appended
 */
TML_EXPORT void* crypto_aes_gcm_encrypt(void* key_handle, void* nonce_handle, void* data_handle,
                                        void* aad_handle) {
    TmlBuffer* key = (TmlBuffer*)key_handle;
    TmlBuffer* nonce = (TmlBuffer*)nonce_handle;
    TmlBuffer* data = (TmlBuffer*)data_handle;
    TmlBuffer* aad = (TmlBuffer*)aad_handle;

    if (!key || !nonce || !data)
        return NULL;
    if (key->length != 32)
        return NULL; /* AES-256 requires 32-byte key */
    if (nonce->length != 12)
        return NULL; /* GCM standard 96-bit nonce */

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return NULL;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    /* Set IV length (12 bytes is default for GCM, but be explicit) */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key->data, nonce->data) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    /* Process AAD if provided */
    int outlen = 0;
    if (aad && aad->data && aad->length > 0) {
        if (EVP_EncryptUpdate(ctx, NULL, &outlen, aad->data, (int)aad->length) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return NULL;
        }
    }

    /* Allocate output: ciphertext (same size as plaintext) + 16-byte tag */
    int64_t ct_capacity = data->length + 16;
    TmlBuffer* result = tml_create_buffer(ct_capacity);
    if (!result) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    /* Encrypt plaintext */
    outlen = 0;
    if (EVP_EncryptUpdate(ctx, result->data, &outlen, data->data, (int)data->length) != 1) {
        free(result->data);
        free(result);
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    int total_len = outlen;

    /* Finalize encryption */
    if (EVP_EncryptFinal_ex(ctx, result->data + total_len, &outlen) != 1) {
        free(result->data);
        free(result);
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    total_len += outlen;

    /* Append 16-byte GCM authentication tag */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, result->data + total_len) != 1) {
        free(result->data);
        free(result);
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    total_len += 16;

    result->length = (int64_t)total_len;
    EVP_CIPHER_CTX_free(ctx);
    return (void*)result;
}

// ============================================================================
// AES-GCM Get Tag (standalone tag extraction)
// ============================================================================

/**
 * Extract the GCM tag from a ciphertext+tag buffer produced by crypto_aes_gcm_encrypt.
 * The tag is the last 16 bytes.
 *
 * @param ctx_handle TmlBuffer* with ciphertext+tag (from crypto_aes_gcm_encrypt)
 * @return TmlBuffer* with 16-byte tag
 */
TML_EXPORT void* crypto_aes_gcm_get_tag(void* ctx_handle) {
    TmlBuffer* ct = (TmlBuffer*)ctx_handle;
    if (!ct || ct->length < 16)
        return NULL;

    /* Tag is the last 16 bytes of the ciphertext+tag buffer */
    TmlBuffer* tag = tml_create_buffer(16);
    if (!tag)
        return NULL;

    memcpy(tag->data, ct->data + ct->length - 16, 16);
    tag->length = 16;
    return (void*)tag;
}

// ============================================================================
// AES-256-GCM Authenticated Decryption
// ============================================================================

/**
 * AES-256-GCM decrypt.
 *
 * @param key_handle   TmlBuffer* with 32 bytes (256-bit key)
 * @param nonce_handle TmlBuffer* with 12 bytes (96-bit IV/nonce)
 * @param data_handle  TmlBuffer* with ciphertext (without tag)
 * @param aad_handle   TmlBuffer* with additional authenticated data (may be NULL)
 * @param tag_handle   TmlBuffer* with 16-byte GCM authentication tag
 * @return TmlBuffer* with plaintext, or NULL if authentication fails
 */
TML_EXPORT void* crypto_aes_gcm_decrypt(void* key_handle, void* nonce_handle, void* data_handle,
                                        void* aad_handle, void* tag_handle) {
    TmlBuffer* key = (TmlBuffer*)key_handle;
    TmlBuffer* nonce = (TmlBuffer*)nonce_handle;
    TmlBuffer* data = (TmlBuffer*)data_handle;
    TmlBuffer* aad = (TmlBuffer*)aad_handle;
    TmlBuffer* tag = (TmlBuffer*)tag_handle;

    if (!key || !nonce || !data || !tag)
        return NULL;
    if (key->length != 32)
        return NULL; /* AES-256 requires 32-byte key */
    if (nonce->length != 12)
        return NULL; /* GCM standard 96-bit nonce */
    if (tag->length != 16)
        return NULL; /* GCM tag is 16 bytes */

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return NULL;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    /* Set IV length */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key->data, nonce->data) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    /* Process AAD if provided */
    int outlen = 0;
    if (aad && aad->data && aad->length > 0) {
        if (EVP_DecryptUpdate(ctx, NULL, &outlen, aad->data, (int)aad->length) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return NULL;
        }
    }

    /* Allocate output buffer (plaintext is at most same size as ciphertext) */
    TmlBuffer* result = tml_create_buffer(data->length > 0 ? data->length : 1);
    if (!result) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    /* Decrypt ciphertext */
    outlen = 0;
    if (EVP_DecryptUpdate(ctx, result->data, &outlen, data->data, (int)data->length) != 1) {
        free(result->data);
        free(result);
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    int total_len = outlen;

    /* Set expected GCM tag before finalization */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag->data) != 1) {
        free(result->data);
        free(result);
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    /* Finalize decryption - this verifies the tag */
    if (EVP_DecryptFinal_ex(ctx, result->data + total_len, &outlen) <= 0) {
        /* Authentication failed - tag mismatch */
        free(result->data);
        free(result);
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }
    total_len += outlen;

    result->length = (int64_t)total_len;
    EVP_CIPHER_CTX_free(ctx);
    return (void*)result;
}

#else /* !TML_HAS_OPENSSL */

// ============================================================================
// Stubs when OpenSSL is not available
// ============================================================================

TML_EXPORT void* crypto_rsa_public_encrypt(void* key_handle, void* data_handle,
                                           const char* padding) {
    (void)key_handle;
    (void)data_handle;
    (void)padding;
    return NULL;
}

TML_EXPORT void* crypto_rsa_public_encrypt_oaep(void* key_handle, void* data_handle,
                                                const char* hash, const char* mgf1_hash,
                                                void* label_handle) {
    (void)key_handle;
    (void)data_handle;
    (void)hash;
    (void)mgf1_hash;
    (void)label_handle;
    return NULL;
}

TML_EXPORT void* crypto_rsa_private_decrypt(void* key_handle, void* data_handle,
                                            const char* padding) {
    (void)key_handle;
    (void)data_handle;
    (void)padding;
    return NULL;
}

TML_EXPORT void* crypto_rsa_private_decrypt_oaep(void* key_handle, void* data_handle,
                                                 const char* hash, const char* mgf1_hash,
                                                 void* label_handle) {
    (void)key_handle;
    (void)data_handle;
    (void)hash;
    (void)mgf1_hash;
    (void)label_handle;
    return NULL;
}

TML_EXPORT void* crypto_rsa_private_encrypt(void* key_handle, void* data_handle,
                                            const char* padding) {
    (void)key_handle;
    (void)data_handle;
    (void)padding;
    return NULL;
}

TML_EXPORT void* crypto_rsa_public_decrypt(void* key_handle, void* data_handle,
                                           const char* padding) {
    (void)key_handle;
    (void)data_handle;
    (void)padding;
    return NULL;
}

TML_EXPORT void* crypto_aes_gcm_encrypt(void* key_handle, void* nonce_handle, void* data_handle,
                                        void* aad_handle) {
    (void)key_handle;
    (void)nonce_handle;
    (void)data_handle;
    (void)aad_handle;
    return NULL;
}

TML_EXPORT void* crypto_aes_gcm_get_tag(void* ctx_handle) {
    (void)ctx_handle;
    return NULL;
}

TML_EXPORT void* crypto_aes_gcm_decrypt(void* key_handle, void* nonce_handle, void* data_handle,
                                        void* aad_handle, void* tag_handle) {
    (void)key_handle;
    (void)nonce_handle;
    (void)data_handle;
    (void)aad_handle;
    (void)tag_handle;
    return NULL;
}

#endif /* TML_HAS_OPENSSL */
