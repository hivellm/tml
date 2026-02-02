/**
 * TML Crypto Runtime - Cipher Functions (Windows BCrypt Implementation)
 *
 * Uses Windows CNG (Cryptography API: Next Generation) via BCrypt.
 * Supports AES in various modes (CBC, CTR, GCM, CCM).
 */

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "crypto_internal.h"
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>

#pragma comment(lib, "bcrypt.lib")

// ============================================================================
// Cipher Algorithm Mapping
// ============================================================================

typedef struct {
    const char* name;
    LPCWSTR bcrypt_alg;
    LPCWSTR chaining_mode;
    size_t key_size;
    size_t iv_size;
    size_t block_size;
    bool is_aead;
} CipherAlgorithmInfo;

static const CipherAlgorithmInfo CIPHER_ALGORITHMS[] = {
    { "aes-128-cbc", BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_CBC, 16, 16, 16, false },
    { "aes-192-cbc", BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_CBC, 24, 16, 16, false },
    { "aes-256-cbc", BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_CBC, 32, 16, 16, false },
    { "aes-128-gcm", BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_GCM, 16, 12, 16, true },
    { "aes-192-gcm", BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_GCM, 24, 12, 16, true },
    { "aes-256-gcm", BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_GCM, 32, 12, 16, true },
    { "aes-128-ccm", BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_CCM, 16, 12, 16, true },
    { "aes-256-ccm", BCRYPT_AES_ALGORITHM, BCRYPT_CHAIN_MODE_CCM, 32, 12, 16, true },
    { NULL, NULL, NULL, 0, 0, 0, false }
};

static const CipherAlgorithmInfo* find_cipher_algorithm(const char* name) {
    for (const CipherAlgorithmInfo* info = CIPHER_ALGORITHMS; info->name != NULL; info++) {
        if (_stricmp(info->name, name) == 0) {
            return info;
        }
    }
    return NULL;
}

// ============================================================================
// Cipher Context Structure
// ============================================================================

struct TmlCipherContext {
    BCRYPT_ALG_HANDLE alg_handle;
    BCRYPT_KEY_HANDLE key_handle;
    uint8_t* key_object;
    DWORD key_object_size;
    uint8_t* iv;
    size_t iv_len;
    uint8_t* aad;
    size_t aad_len;
    uint8_t tag[16];  // GCM/CCM auth tag
    size_t tag_len;
    bool is_encrypt;
    bool is_aead;
    bool padding_enabled;
    TmlBuffer* output;
    char algorithm[32];
};

// ============================================================================
// Cipher Context Implementation
// ============================================================================

TmlCipherContext* cipher_context_create(const char* algorithm, const uint8_t* key, size_t key_len,
                                        const uint8_t* iv, size_t iv_len, bool encrypt) {
    const CipherAlgorithmInfo* info = find_cipher_algorithm(algorithm);
    if (!info) return NULL;

    if (key_len != info->key_size) return NULL;
    if (iv_len != info->iv_size && info->iv_size > 0) return NULL;

    TmlCipherContext* ctx = (TmlCipherContext*)calloc(1, sizeof(TmlCipherContext));
    if (!ctx) return NULL;

    strncpy(ctx->algorithm, algorithm, sizeof(ctx->algorithm) - 1);
    ctx->is_encrypt = encrypt;
    ctx->is_aead = info->is_aead;
    ctx->padding_enabled = true;
    ctx->tag_len = 16;

    // Open algorithm provider
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &ctx->alg_handle,
        info->bcrypt_alg,
        NULL,
        0
    );
    if (!BCRYPT_SUCCESS(status)) {
        free(ctx);
        return NULL;
    }

    // Set chaining mode
    status = BCryptSetProperty(
        ctx->alg_handle,
        BCRYPT_CHAINING_MODE,
        (PBYTE)info->chaining_mode,
        (ULONG)(wcslen(info->chaining_mode) + 1) * sizeof(WCHAR),
        0
    );
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(ctx->alg_handle, 0);
        free(ctx);
        return NULL;
    }

    // Get key object size
    DWORD result_size;
    status = BCryptGetProperty(
        ctx->alg_handle,
        BCRYPT_OBJECT_LENGTH,
        (PBYTE)&ctx->key_object_size,
        sizeof(DWORD),
        &result_size,
        0
    );
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(ctx->alg_handle, 0);
        free(ctx);
        return NULL;
    }

    // Allocate key object
    ctx->key_object = (uint8_t*)malloc(ctx->key_object_size);
    if (!ctx->key_object) {
        BCryptCloseAlgorithmProvider(ctx->alg_handle, 0);
        free(ctx);
        return NULL;
    }

    // Generate key from raw bytes
    status = BCryptGenerateSymmetricKey(
        ctx->alg_handle,
        &ctx->key_handle,
        ctx->key_object,
        ctx->key_object_size,
        (PUCHAR)key,
        (ULONG)key_len,
        0
    );
    if (!BCRYPT_SUCCESS(status)) {
        free(ctx->key_object);
        BCryptCloseAlgorithmProvider(ctx->alg_handle, 0);
        free(ctx);
        return NULL;
    }

    // Copy IV
    if (iv && iv_len > 0) {
        ctx->iv = (uint8_t*)malloc(iv_len);
        if (!ctx->iv) {
            BCryptDestroyKey(ctx->key_handle);
            free(ctx->key_object);
            BCryptCloseAlgorithmProvider(ctx->alg_handle, 0);
            free(ctx);
            return NULL;
        }
        memcpy(ctx->iv, iv, iv_len);
        ctx->iv_len = iv_len;
    }

    ctx->output = tml_buffer_create(1024);
    if (!ctx->output) {
        if (ctx->iv) free(ctx->iv);
        BCryptDestroyKey(ctx->key_handle);
        free(ctx->key_object);
        BCryptCloseAlgorithmProvider(ctx->alg_handle, 0);
        free(ctx);
        return NULL;
    }
    ctx->output->len = 0;

    return ctx;
}

void cipher_context_set_aad(TmlCipherContext* ctx, const uint8_t* aad, size_t aad_len) {
    if (!ctx || !ctx->is_aead || !aad || aad_len == 0) return;

    if (ctx->aad) free(ctx->aad);
    ctx->aad = (uint8_t*)malloc(aad_len);
    if (ctx->aad) {
        memcpy(ctx->aad, aad, aad_len);
        ctx->aad_len = aad_len;
    }
}

void cipher_context_set_padding(TmlCipherContext* ctx, bool enabled) {
    if (ctx) ctx->padding_enabled = enabled;
}

size_t cipher_context_update(TmlCipherContext* ctx, const uint8_t* input, size_t input_len,
                            uint8_t* output, size_t output_size) {
    if (!ctx || !input || input_len == 0) return 0;

    // For AEAD modes, we accumulate data and process in finalize
    tml_buffer_append(ctx->output, input, input_len);
    return 0;  // No output until finalize for simplicity
}

size_t cipher_context_finalize(TmlCipherContext* ctx, uint8_t* output, size_t output_size, bool* success) {
    if (!ctx || !success) {
        if (success) *success = false;
        return 0;
    }

    *success = false;
    NTSTATUS status;
    ULONG result_len = 0;

    if (ctx->is_aead) {
        // GCM/CCM encryption/decryption
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
        BCRYPT_INIT_AUTH_MODE_INFO(auth_info);

        auth_info.pbNonce = ctx->iv;
        auth_info.cbNonce = (ULONG)ctx->iv_len;
        auth_info.pbAuthData = ctx->aad;
        auth_info.cbAuthData = (ULONG)ctx->aad_len;
        auth_info.pbTag = ctx->tag;
        auth_info.cbTag = (ULONG)ctx->tag_len;

        // Allocate output buffer
        size_t out_len = ctx->output->len + 16;  // Extra for potential padding
        uint8_t* out_buf = (uint8_t*)malloc(out_len);
        if (!out_buf) return 0;

        if (ctx->is_encrypt) {
            auth_info.dwFlags = 0;

            status = BCryptEncrypt(
                ctx->key_handle,
                ctx->output->data,
                (ULONG)ctx->output->len,
                &auth_info,
                NULL, 0,  // IV handled in auth_info
                out_buf,
                (ULONG)out_len,
                &result_len,
                0
            );
        } else {
            auth_info.dwFlags = 0;

            status = BCryptDecrypt(
                ctx->key_handle,
                ctx->output->data,
                (ULONG)ctx->output->len,
                &auth_info,
                NULL, 0,
                out_buf,
                (ULONG)out_len,
                &result_len,
                0
            );
        }

        if (BCRYPT_SUCCESS(status)) {
            if (output && output_size >= result_len) {
                memcpy(output, out_buf, result_len);
            }
            *success = true;
        }

        free(out_buf);
    } else {
        // CBC mode
        DWORD flags = ctx->padding_enabled ? BCRYPT_BLOCK_PADDING : 0;
        size_t out_len = ctx->output->len + 16;  // Block padding
        uint8_t* out_buf = (uint8_t*)malloc(out_len);
        if (!out_buf) return 0;

        uint8_t* iv_copy = NULL;
        if (ctx->iv && ctx->iv_len > 0) {
            iv_copy = (uint8_t*)malloc(ctx->iv_len);
            if (iv_copy) memcpy(iv_copy, ctx->iv, ctx->iv_len);
        }

        if (ctx->is_encrypt) {
            status = BCryptEncrypt(
                ctx->key_handle,
                ctx->output->data,
                (ULONG)ctx->output->len,
                NULL,
                iv_copy,
                (ULONG)ctx->iv_len,
                out_buf,
                (ULONG)out_len,
                &result_len,
                flags
            );
        } else {
            status = BCryptDecrypt(
                ctx->key_handle,
                ctx->output->data,
                (ULONG)ctx->output->len,
                NULL,
                iv_copy,
                (ULONG)ctx->iv_len,
                out_buf,
                (ULONG)out_len,
                &result_len,
                flags
            );
        }

        if (iv_copy) free(iv_copy);

        if (BCRYPT_SUCCESS(status)) {
            if (output && output_size >= result_len) {
                memcpy(output, out_buf, result_len);
            }
            *success = true;
        }

        free(out_buf);
    }

    return result_len;
}

TmlBuffer* cipher_context_get_tag(TmlCipherContext* ctx) {
    if (!ctx || !ctx->is_aead) return NULL;
    return tml_buffer_from_data(ctx->tag, ctx->tag_len);
}

void cipher_context_set_tag(TmlCipherContext* ctx, const uint8_t* tag, size_t tag_len) {
    if (!ctx || !ctx->is_aead || !tag || tag_len > 16) return;
    memcpy(ctx->tag, tag, tag_len);
    ctx->tag_len = tag_len;
}

void cipher_context_destroy(TmlCipherContext* ctx) {
    if (!ctx) return;

    if (ctx->key_handle) {
        BCryptDestroyKey(ctx->key_handle);
    }
    if (ctx->alg_handle) {
        BCryptCloseAlgorithmProvider(ctx->alg_handle, 0);
    }
    if (ctx->key_object) {
        SecureZeroMemory(ctx->key_object, ctx->key_object_size);
        free(ctx->key_object);
    }
    if (ctx->iv) {
        SecureZeroMemory(ctx->iv, ctx->iv_len);
        free(ctx->iv);
    }
    if (ctx->aad) {
        free(ctx->aad);
    }
    if (ctx->output) {
        tml_buffer_destroy(ctx->output);
    }
    SecureZeroMemory(ctx->tag, sizeof(ctx->tag));
    free(ctx);
}

// ============================================================================
// Public API
// ============================================================================

void* crypto_cipher_create(const char* algorithm, TmlBuffer* key, TmlBuffer* iv, int encrypt) {
    if (!key) return NULL;
    return cipher_context_create(
        algorithm,
        key->data, key->len,
        iv ? iv->data : NULL, iv ? iv->len : 0,
        encrypt != 0
    );
}

void crypto_cipher_set_aad(void* ctx, TmlBuffer* aad) {
    if (!ctx || !aad) return;
    cipher_context_set_aad((TmlCipherContext*)ctx, aad->data, aad->len);
}

void crypto_cipher_set_aad_str(void* ctx, const char* aad) {
    if (!ctx || !aad) return;
    cipher_context_set_aad((TmlCipherContext*)ctx, (const uint8_t*)aad, strlen(aad));
}

void crypto_cipher_set_padding(void* ctx, bool enabled) {
    cipher_context_set_padding((TmlCipherContext*)ctx, enabled);
}

void crypto_cipher_update_str(void* ctx, const char* data, TmlBuffer* output) {
    if (!ctx || !data) return;
    TmlCipherContext* c = (TmlCipherContext*)ctx;
    tml_buffer_append(c->output, (const uint8_t*)data, strlen(data));
}

void crypto_cipher_update_bytes(void* ctx, TmlBuffer* data, TmlBuffer* output) {
    if (!ctx || !data) return;
    TmlCipherContext* c = (TmlCipherContext*)ctx;
    tml_buffer_append(c->output, data->data, data->len);
}

bool crypto_cipher_finalize(void* ctx, TmlBuffer* output) {
    if (!ctx || !output) return false;

    TmlCipherContext* c = (TmlCipherContext*)ctx;
    size_t max_out = c->output->len + 32;  // Extra for padding/tag

    uint8_t* out_buf = (uint8_t*)malloc(max_out);
    if (!out_buf) return false;

    bool success = false;
    size_t out_len = cipher_context_finalize(c, out_buf, max_out, &success);

    if (success && out_len > 0) {
        tml_buffer_resize(output, out_len);
        memcpy(output->data, out_buf, out_len);
    }

    free(out_buf);
    return success;
}

TmlBuffer* crypto_cipher_get_tag(void* ctx) {
    return cipher_context_get_tag((TmlCipherContext*)ctx);
}

void crypto_cipher_set_tag(void* ctx, TmlBuffer* tag) {
    if (!tag) return;
    cipher_context_set_tag((TmlCipherContext*)ctx, tag->data, tag->len);
}

void crypto_cipher_destroy(void* ctx) {
    cipher_context_destroy((TmlCipherContext*)ctx);
}

#endif // _WIN32