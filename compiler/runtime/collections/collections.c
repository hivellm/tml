/**
 * @file collections.c
 * @brief TML Runtime - Collection FFI Exports
 *
 * Minimal C runtime for collection operations that cannot yet be pure TML.
 *
 * Buffer FFI: buffer_destroy, buffer_len — needed because crypto/zlib C modules
 * create TmlBuffer* objects internally. TML wraps them via @extern("c") FFI.
 *
 * List fallback: list_get, list_len — used by codegen for legacy list iteration
 * (loop.cpp, collections.cpp). Will be removed when list iteration is fully TML.
 *
 * All other list_* functions were removed in Phase 31 (string.c migration).
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

TML_EXPORT int64_t list_get(TmlList* list, int64_t index) {
    if (!list || index < 0 || index >= list->len)
        return 0;
    return ((int64_t*)list->data)[index];
}

TML_EXPORT int64_t list_len(TmlList* list) {
    return list ? list->len : 0;
}
