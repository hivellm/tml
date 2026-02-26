/**
 * @file crypto_common.h
 * @brief Shared types and utilities for TML crypto runtime modules.
 *
 * This header is included by all crypto_*.c files. It defines:
 * - TmlBuffer struct (matching TML's std::collections::Buffer ABI)
 * - Buffer creation/manipulation helpers
 * - TML_EXPORT macro for symbol visibility
 * - OpenSSL includes (when available)
 */

#ifndef TML_CRYPTO_COMMON_H
#define TML_CRYPTO_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compat.h"

// ============================================================================
// Platform export macro
// ============================================================================

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// Memory allocation — use mem_alloc/mem_free so the memory tracker can track
// ============================================================================

extern void* mem_alloc(int64_t);
extern void* mem_realloc(void*, int64_t);
extern void mem_free(void*);

// ============================================================================
// Buffer structure (matching TML's std::collections::Buffer ABI)
// ============================================================================

typedef struct {
    uint8_t* data;
    int64_t length;
    int64_t capacity;
    int64_t read_pos;
} TmlBuffer;

static inline TmlBuffer* tml_create_buffer(int64_t capacity) {
    int64_t cap = capacity > 0 ? capacity : 1;
    // Single allocation: header (32 bytes) + data (cap bytes) in one block.
    // Buffer.destroy() in TML detects inline data (data == header + 32)
    // and skips the separate data free.
    TmlBuffer* buf = (TmlBuffer*)mem_alloc((int64_t)(sizeof(TmlBuffer) + cap));
    if (!buf)
        return NULL;
    buf->data = (uint8_t*)(buf + 1); // data immediately follows header
    buf->length = 0;
    buf->capacity = cap;
    buf->read_pos = 0;
    return buf;
}

static inline TmlBuffer* tml_create_buffer_with_data(const uint8_t* data, int64_t len) {
    TmlBuffer* buf = tml_create_buffer(len);
    if (!buf)
        return NULL;
    memcpy(buf->data, data, (size_t)len);
    buf->length = len;
    return buf;
}

static inline char* tml_strdup(const char* s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char* copy = (char*)mem_alloc((int64_t)(len + 1));
    if (!copy)
        return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

// ============================================================================
// OpenSSL includes (when TML_HAS_OPENSSL is defined)
// ============================================================================

#ifdef TML_HAS_OPENSSL

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

// Helper: get the EVP_MD from a digest name string
static inline const EVP_MD* tml_get_md(const char* name) {
    if (!name)
        return NULL;
    return EVP_get_digestbyname(name);
}

// Helper: create a BIO from a string
static inline BIO* tml_bio_from_str(const char* s) {
    if (!s)
        return NULL;
    return BIO_new_mem_buf(s, -1);
}

// Helper: read BIO contents to a malloc'd string
static inline char* tml_bio_to_str(BIO* bio) {
    if (!bio)
        return NULL;
    char* data = NULL;
    long len = BIO_get_mem_data(bio, &data);
    if (len <= 0 || !data)
        return tml_strdup("");
    char* result = (char*)mem_alloc((int64_t)(len + 1));
    if (!result)
        return NULL;
    memcpy(result, data, len);
    result[len] = '\0';
    return result;
}

#endif /* TML_HAS_OPENSSL */

#endif /* TML_CRYPTO_COMMON_H */
