/**
 * @file collections.c
 * @brief TML Runtime - Collection FFI Exports
 *
 * Minimal C runtime for collection operations that cannot yet be pure TML.
 *
 * Buffer FFI: buffer_destroy, buffer_len — needed because crypto/zlib C modules
 * create TmlBuffer* objects internally. TML wraps them via @extern("c") FFI.
 *
 * List fallback: list_create, list_push, list_get, list_len — used by codegen
 * for legacy list iteration (loop.cpp, collections.cpp) and by the crypto C
 * runtime (crypto.c, crypto_ecdh.c) for get_hashes/get_ciphers/get_curves.
 * Will be removed when crypto runtime is migrated to pure TML.
 *
 * Pure TML implementations: lib/std/src/collections/list.tml
 */

#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// Buffer FFI Exports (kept for crypto/zlib C interop)
// ============================================================================

typedef struct TmlBuffer {
    uint8_t* data;
    int64_t len;
    int64_t capacity;
    int64_t read_pos;
} TmlBuffer;

TML_EXPORT void buffer_destroy(TmlBuffer* buf) {
    if (!buf)
        return;
    if (buf->data)
        free(buf->data);
    free(buf);
}

TML_EXPORT int64_t buffer_len(TmlBuffer* buf) {
    return buf ? buf->len : 0;
}

// ============================================================================
// List Fallback (legacy codegen — loop.cpp, collections.cpp)
// ============================================================================

typedef struct TmlList {
    void* data;
    int64_t len;
    int64_t capacity;
    int64_t elem_size;
} TmlList;

TML_EXPORT TmlList* list_create(int64_t initial_capacity) {
    TmlList* list = (TmlList*)calloc(1, sizeof(TmlList));
    if (!list)
        return NULL;
    if (initial_capacity <= 0)
        initial_capacity = 8;
    list->data = calloc((size_t)initial_capacity, sizeof(int64_t));
    list->len = 0;
    list->capacity = initial_capacity;
    list->elem_size = sizeof(int64_t);
    return list;
}

TML_EXPORT void list_push(TmlList* list, int64_t value) {
    if (!list)
        return;
    if (list->len >= list->capacity) {
        int64_t new_cap = list->capacity * 2;
        void* new_data = realloc(list->data, (size_t)new_cap * sizeof(int64_t));
        if (!new_data)
            return;
        list->data = new_data;
        list->capacity = new_cap;
    }
    ((int64_t*)list->data)[list->len++] = value;
}

TML_EXPORT int64_t list_get(TmlList* list, int64_t index) {
    if (!list || index < 0 || index >= list->len)
        return 0;
    return ((int64_t*)list->data)[index];
}

TML_EXPORT int64_t list_len(TmlList* list) {
    return list ? list->len : 0;
}
