/**
 * TML Crypto Runtime - Hash Functions (Windows BCrypt Implementation)
 *
 * Uses Windows CNG (Cryptography API: Next Generation) via BCrypt.
 */

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "crypto_internal.h"

#include <bcrypt.h>
#include <stdio.h>
#include <windows.h>

#pragma comment(lib, "bcrypt.lib")

// ============================================================================
// Hash Algorithm Mapping
// ============================================================================

typedef struct {
    const char* name;
    LPCWSTR bcrypt_alg;
    size_t digest_size;
} HashAlgorithmInfo;

// Forward-declare SHA-3 algorithm names (may not be available on older Windows)
#ifndef BCRYPT_SHA3_256_ALGORITHM_EARLY
#define BCRYPT_SHA3_256_ALGORITHM_EARLY L"SHA3-256"
#define BCRYPT_SHA3_384_ALGORITHM_EARLY L"SHA3-384"
#define BCRYPT_SHA3_512_ALGORITHM_EARLY L"SHA3-512"
#endif

static const HashAlgorithmInfo HASH_ALGORITHMS[] = {
    {"md5", BCRYPT_MD5_ALGORITHM, 16},
    {"sha1", BCRYPT_SHA1_ALGORITHM, 20},
    {"sha256", BCRYPT_SHA256_ALGORITHM, 32},
    {"sha384", BCRYPT_SHA384_ALGORITHM, 48},
    {"sha512", BCRYPT_SHA512_ALGORITHM, 64},
    {"sha3-256", BCRYPT_SHA3_256_ALGORITHM_EARLY, 32},
    {"sha3-384", BCRYPT_SHA3_384_ALGORITHM_EARLY, 48},
    {"sha3-512", BCRYPT_SHA3_512_ALGORITHM_EARLY, 64},
    {NULL, NULL, 0}};

static const HashAlgorithmInfo* find_hash_algorithm(const char* name) {
    for (const HashAlgorithmInfo* info = HASH_ALGORITHMS; info->name != NULL; info++) {
        if (_stricmp(info->name, name) == 0) {
            return info;
        }
    }
    return NULL;
}

// ============================================================================
// Cached Algorithm Provider (avoids per-hash BCryptOpenAlgorithmProvider)
// ============================================================================

typedef struct {
    BCRYPT_ALG_HANDLE handle;
    DWORD hash_object_size;
    DWORD digest_size;
    int initialized;
} CachedAlgProvider;

// One cached provider per algorithm (indexed by position in HASH_ALGORITHMS)
#define MAX_CACHED_ALGS 8
static CachedAlgProvider cached_providers[MAX_CACHED_ALGS];
static INIT_ONCE cached_init_once[MAX_CACHED_ALGS];
static volatile int cached_once_initialized = 0;

static void ensure_once_init(void) {
    if (!cached_once_initialized) {
        for (int i = 0; i < MAX_CACHED_ALGS; i++) {
            InitOnceInitialize(&cached_init_once[i]);
            cached_providers[i].initialized = 0;
        }
        cached_once_initialized = 1;
    }
}

// Thread-safe initialization callback for a single algorithm provider
static BOOL CALLBACK init_alg_provider_callback(PINIT_ONCE once, PVOID param, PVOID* context) {
    (void)once;
    int idx = (int)(intptr_t)param;
    const HashAlgorithmInfo* info = &HASH_ALGORITHMS[idx];

    CachedAlgProvider* prov = &cached_providers[idx];

    NTSTATUS status = BCryptOpenAlgorithmProvider(&prov->handle, info->bcrypt_alg, NULL, 0);
    if (!BCRYPT_SUCCESS(status))
        return FALSE;

    DWORD result_size;
    status = BCryptGetProperty(prov->handle, BCRYPT_OBJECT_LENGTH, (PBYTE)&prov->hash_object_size,
                               sizeof(DWORD), &result_size, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(prov->handle, 0);
        return FALSE;
    }

    status = BCryptGetProperty(prov->handle, BCRYPT_HASH_LENGTH, (PBYTE)&prov->digest_size,
                               sizeof(DWORD), &result_size, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(prov->handle, 0);
        return FALSE;
    }

    prov->initialized = 1;
    return TRUE;
}

// Get or lazily initialize a cached algorithm provider (thread-safe)
static CachedAlgProvider* get_cached_provider(int alg_index) {
    ensure_once_init();
    if (alg_index < 0 || alg_index >= MAX_CACHED_ALGS)
        return NULL;

    InitOnceExecuteOnce(&cached_init_once[alg_index], init_alg_provider_callback,
                        (PVOID)(intptr_t)alg_index, NULL);

    if (cached_providers[alg_index].initialized) {
        return &cached_providers[alg_index];
    }
    return NULL;
}

// Find algorithm index in HASH_ALGORITHMS table
static int find_hash_algorithm_index(const char* name) {
    for (int i = 0; HASH_ALGORITHMS[i].name != NULL; i++) {
        if (_stricmp(HASH_ALGORITHMS[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// Hash Context Structure
// ============================================================================

struct TmlHashContext {
    BCRYPT_ALG_HANDLE alg_handle; // borrowed from cache (do NOT close)
    BCRYPT_HASH_HANDLE hash_handle;
    uint8_t* hash_object;
    DWORD hash_object_size;
    DWORD digest_size;
    char algorithm[32];
    int owns_alg_handle; // 0 = cached (don't close), 1 = owns (close on destroy)
};

// ============================================================================
// Hash Context Implementation
// ============================================================================

TmlHashContext* hash_context_create(const char* algorithm) {
    int alg_idx = find_hash_algorithm_index(algorithm);
    if (alg_idx < 0)
        return NULL;

    // Use cached provider (avoids BCryptOpenAlgorithmProvider + BCryptGetProperty per call)
    CachedAlgProvider* prov = get_cached_provider(alg_idx);
    if (!prov)
        return NULL;

    TmlHashContext* ctx = (TmlHashContext*)calloc(1, sizeof(TmlHashContext));
    if (!ctx)
        return NULL;

    strncpy(ctx->algorithm, algorithm, sizeof(ctx->algorithm) - 1);
    ctx->alg_handle = prov->handle;
    ctx->hash_object_size = prov->hash_object_size;
    ctx->digest_size = prov->digest_size;
    ctx->owns_alg_handle = 0; // Borrowed from cache

    // Allocate hash object
    ctx->hash_object = (uint8_t*)malloc(ctx->hash_object_size);
    if (!ctx->hash_object) {
        free(ctx);
        return NULL;
    }

    // Create hash (using cached algorithm handle)
    NTSTATUS status = BCryptCreateHash(ctx->alg_handle, &ctx->hash_handle, ctx->hash_object,
                                       ctx->hash_object_size,
                                       NULL, // No key (not HMAC)
                                       0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        free(ctx->hash_object);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void hash_context_update(TmlHashContext* ctx, const uint8_t* data, size_t len) {
    if (!ctx || !data || len == 0)
        return;

    BCryptHashData(ctx->hash_handle, (PUCHAR)data, (ULONG)len, 0);
}

TmlBuffer* hash_context_digest(TmlHashContext* ctx) {
    if (!ctx)
        return NULL;

    TmlBuffer* result = tml_buffer_create(ctx->digest_size);
    if (!result)
        return NULL;

    NTSTATUS status = BCryptFinishHash(ctx->hash_handle, result->data, ctx->digest_size, 0);

    if (!BCRYPT_SUCCESS(status)) {
        tml_buffer_destroy(result);
        return NULL;
    }

    return result;
}

TmlHashContext* hash_context_copy(TmlHashContext* ctx) {
    if (!ctx)
        return NULL;

    TmlHashContext* copy = (TmlHashContext*)calloc(1, sizeof(TmlHashContext));
    if (!copy)
        return NULL;

    memcpy(copy, ctx, sizeof(TmlHashContext));
    copy->owns_alg_handle = 0; // Copy borrows from cache too

    // Allocate new hash object
    copy->hash_object = (uint8_t*)malloc(ctx->hash_object_size);
    if (!copy->hash_object) {
        free(copy);
        return NULL;
    }

    // Duplicate hash
    NTSTATUS status = BCryptDuplicateHash(ctx->hash_handle, &copy->hash_handle, copy->hash_object,
                                          copy->hash_object_size, 0);

    if (!BCRYPT_SUCCESS(status)) {
        free(copy->hash_object);
        free(copy);
        return NULL;
    }

    return copy;
}

void hash_context_destroy(TmlHashContext* ctx) {
    if (!ctx)
        return;

    if (ctx->hash_handle) {
        BCryptDestroyHash(ctx->hash_handle);
    }
    if (ctx->owns_alg_handle && ctx->alg_handle) {
        BCryptCloseAlgorithmProvider(ctx->alg_handle, 0);
    }
    if (ctx->hash_object) {
        SecureZeroMemory(ctx->hash_object, ctx->hash_object_size);
        free(ctx->hash_object);
    }
    free(ctx);
}

// ============================================================================
// One-shot Hash Helper
// ============================================================================

static TmlBuffer* hash_oneshot(const char* algorithm, const uint8_t* data, size_t len) {
    TmlHashContext* ctx = hash_context_create(algorithm);
    if (!ctx)
        return NULL;

    hash_context_update(ctx, data, len);
    TmlBuffer* result = hash_context_digest(ctx);
    hash_context_destroy(ctx);

    return result;
}

// ============================================================================
// Public API - One-shot Hash Functions
// ============================================================================

TmlBuffer* crypto_md5(const char* data) {
    return hash_oneshot("md5", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_md5_bytes(TmlBuffer* data) {
    if (!data)
        return hash_oneshot("md5", NULL, 0);
    return hash_oneshot("md5", data->data, data->len);
}

TmlBuffer* crypto_sha1(const char* data) {
    return hash_oneshot("sha1", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha1_bytes(TmlBuffer* data) {
    if (!data)
        return hash_oneshot("sha1", NULL, 0);
    return hash_oneshot("sha1", data->data, data->len);
}

TmlBuffer* crypto_sha256(const char* data) {
    return hash_oneshot("sha256", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha256_bytes(TmlBuffer* data) {
    if (!data)
        return hash_oneshot("sha256", NULL, 0);
    return hash_oneshot("sha256", data->data, data->len);
}

TmlBuffer* crypto_sha384(const char* data) {
    return hash_oneshot("sha384", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha384_bytes(TmlBuffer* data) {
    if (!data)
        return hash_oneshot("sha384", NULL, 0);
    return hash_oneshot("sha384", data->data, data->len);
}

TmlBuffer* crypto_sha512(const char* data) {
    return hash_oneshot("sha512", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha512_bytes(TmlBuffer* data) {
    if (!data)
        return hash_oneshot("sha512", NULL, 0);
    return hash_oneshot("sha512", data->data, data->len);
}

// SHA-512/256 - Windows doesn't have native support, implement manually
TmlBuffer* crypto_sha512_256(const char* data) {
    // Compute SHA-512 and truncate to 256 bits
    TmlBuffer* full = crypto_sha512(data);
    if (!full)
        return NULL;

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
    if (!full)
        return NULL;

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
// SHA-3 - Windows 10 1903+ supports via BCRYPT_SHA3_256_ALGORITHM etc.
// For older Windows, we'd need a software implementation
// ============================================================================

TmlBuffer* crypto_sha3_256(const char* data) {
    // Try via cached provider (will fail gracefully if SHA3 not available)
    return hash_oneshot("sha3-256", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha3_256_bytes(TmlBuffer* data) {
    if (!data)
        return crypto_sha3_256(NULL);
    return hash_oneshot("sha3-256", data->data, data->len);
}

TmlBuffer* crypto_sha3_384(const char* data) {
    return hash_oneshot("sha3-384", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha3_384_bytes(TmlBuffer* data) {
    if (!data)
        return crypto_sha3_384(NULL);
    return hash_oneshot("sha3-384", data->data, data->len);
}

TmlBuffer* crypto_sha3_512(const char* data) {
    return hash_oneshot("sha3-512", (const uint8_t*)data, data ? strlen(data) : 0);
}

TmlBuffer* crypto_sha3_512_bytes(TmlBuffer* data) {
    if (!data)
        return crypto_sha3_512(NULL);
    return hash_oneshot("sha3-512", data->data, data->len);
}

// ============================================================================
// BLAKE2 - Not natively supported in Windows BCrypt
// Would need software implementation or use a library like libsodium
// ============================================================================

TmlBuffer* crypto_blake2b512(const char* data) {
    // BLAKE2 not available in BCrypt
    // TODO: Add software implementation or link libsodium
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
// BLAKE3 - Not natively supported, needs external implementation
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
    if (!ctx || !data)
        return;
    hash_context_update((TmlHashContext*)ctx, (const uint8_t*)data, strlen(data));
}

void crypto_hash_update_bytes(void* ctx, TmlBuffer* data) {
    if (!ctx || !data)
        return;
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

#endif // _WIN32