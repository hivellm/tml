/**
 * @file collections.c
 * @brief TML Runtime - Collection Functions
 *
 * Note: List[T], HashMap[K,V], and Buffer TML types are now pure TML.
 * The C list_* functions below are ONLY kept because string.c
 * (str_split, str_chars, str_join) still calls them.
 * They will be removed when string functions migrate to TML.
 *
 * buffer_destroy and buffer_len are thin FFI exports kept because
 * crypto and zlib C modules create TmlBuffer* objects internally
 * and the TML-side code calls these via @extern("buffer_destroy") etc.
 *
 * - List[T]: see lib/std/src/collections/list.tml
 * - HashMap[K,V]: see lib/std/src/collections/hashmap.tml
 * - Buffer: see lib/std/src/collections/buffer.tml
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
// The crypto and zlib C modules create TmlBuffer* objects internally using
// tml_create_buffer() from crypto_common.h / zlib_internal.h.
// TML code wraps these as Buffer { handle: ptr } and calls buffer_destroy /
// buffer_len via @extern FFI declarations.  The memory layout is:
//   offset 0: data pointer, offset 8: len, offset 16: capacity, offset 24: read_pos

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
// C List Functions (kept for string.c dependency — remove with string migration)
// ============================================================================

typedef struct TmlList {
    void* data;
    int64_t len;
    int64_t capacity;
    int64_t elem_size;
} TmlList;

TML_EXPORT TmlList* list_create(int64_t initial_capacity) {
    TmlList* list = (TmlList*)malloc(sizeof(TmlList));
    if (!list)
        return NULL;
    int64_t cap = initial_capacity > 0 ? initial_capacity : 4;
    list->data = malloc((size_t)(cap * sizeof(int64_t)));
    list->len = 0;
    list->capacity = cap;
    list->elem_size = sizeof(int64_t);
    return list;
}

TML_EXPORT TmlList* list_new(int64_t initial_capacity) {
    return list_create(initial_capacity);
}

TML_EXPORT void list_destroy(TmlList* list) {
    if (!list)
        return;
    if (list->data)
        free(list->data);
    free(list);
}

TML_EXPORT void list_push(TmlList* list, int64_t value) {
    if (!list)
        return;
    if (list->len >= list->capacity) {
        int64_t new_cap = list->capacity * 2;
        void* new_data = realloc(list->data, (size_t)(new_cap * sizeof(int64_t)));
        if (!new_data)
            return;
        list->data = new_data;
        list->capacity = new_cap;
    }
    ((int64_t*)list->data)[list->len++] = value;
}

TML_EXPORT int64_t list_pop(TmlList* list) {
    if (!list || list->len == 0)
        return 0;
    return ((int64_t*)list->data)[--list->len];
}

TML_EXPORT int64_t list_get(TmlList* list, int64_t index) {
    if (!list || index < 0 || index >= list->len)
        return 0;
    return ((int64_t*)list->data)[index];
}

TML_EXPORT void list_set(TmlList* list, int64_t index, int64_t value) {
    if (!list || index < 0 || index >= list->len)
        return;
    ((int64_t*)list->data)[index] = value;
}

TML_EXPORT int64_t list_len(TmlList* list) {
    return list ? list->len : 0;
}

TML_EXPORT int64_t list_capacity(TmlList* list) {
    return list ? list->capacity : 0;
}

TML_EXPORT void list_clear(TmlList* list) {
    if (list)
        list->len = 0;
}

TML_EXPORT int32_t list_is_empty(TmlList* list) {
    return !list || list->len == 0;
}

TML_EXPORT void list_remove(TmlList* list, int64_t index) {
    if (!list || index < 0 || index >= list->len)
        return;
    int64_t* data = (int64_t*)list->data;
    for (int64_t i = index; i < list->len - 1; i++)
        data[i] = data[i + 1];
    list->len--;
}

TML_EXPORT int64_t list_first(TmlList* list) {
    if (!list || list->len == 0)
        return 0;
    return ((int64_t*)list->data)[0];
}

TML_EXPORT int64_t list_last(TmlList* list) {
    if (!list || list->len == 0)
        return 0;
    return ((int64_t*)list->data)[list->len - 1];
}
