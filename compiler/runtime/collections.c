/**
 * @file collections.c
 * @brief TML Runtime - Collection Functions
 *
 * Implements dynamic collection types for the TML runtime.
 *
 * ## Components
 *
 * - **Dynamic List**: list_* functions for growable arrays
 */

#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// Dynamic List (Array) Functions
// ============================================================================

/**
 * @brief Internal structure for TML dynamic list.
 * Layout: { ptr data, int64_t len, int64_t capacity, int64_t elem_size }
 */
typedef struct TmlList {
    void* data;
    int64_t len;
    int64_t capacity;
    int64_t elem_size;
} TmlList;

/**
 * @brief Creates a new dynamic list with initial capacity.
 *
 * @param initial_capacity Initial capacity (number of elements).
 * @return Pointer to the list structure.
 */
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

/**
 * @brief Destroys a list and frees all memory.
 */
TML_EXPORT void list_destroy(TmlList* list) {
    if (!list)
        return;
    if (list->data)
        free(list->data);
    free(list);
}

/**
 * @brief Pushes an element onto the end of the list.
 */
TML_EXPORT void list_push(TmlList* list, int64_t value) {
    if (!list)
        return;

    // Grow if needed
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

/**
 * @brief Pops an element from the end of the list.
 */
TML_EXPORT int64_t list_pop(TmlList* list) {
    if (!list || list->len == 0)
        return 0;
    return ((int64_t*)list->data)[--list->len];
}

/**
 * @brief Gets an element at the given index.
 */
TML_EXPORT int64_t list_get(TmlList* list, int64_t index) {
    if (!list || index < 0 || index >= list->len)
        return 0;
    return ((int64_t*)list->data)[index];
}

/**
 * @brief Sets an element at the given index.
 */
TML_EXPORT void list_set(TmlList* list, int64_t index, int64_t value) {
    if (!list || index < 0 || index >= list->len)
        return;
    ((int64_t*)list->data)[index] = value;
}

/**
 * @brief Returns the number of elements in the list.
 */
TML_EXPORT int64_t list_len(TmlList* list) {
    return list ? list->len : 0;
}

/**
 * @brief Returns the capacity of the list.
 */
TML_EXPORT int64_t list_capacity(TmlList* list) {
    return list ? list->capacity : 0;
}

/**
 * @brief Clears the list without freeing memory.
 */
TML_EXPORT void list_clear(TmlList* list) {
    if (list)
        list->len = 0;
}

/**
 * @brief Returns 1 if the list is empty, 0 otherwise.
 */
TML_EXPORT int32_t list_is_empty(TmlList* list) {
    return !list || list->len == 0;
}

/**
 * @brief Removes an element at the given index.
 */
TML_EXPORT void list_remove(TmlList* list, int64_t index) {
    if (!list || index < 0 || index >= list->len)
        return;

    // Shift elements down
    int64_t* data = (int64_t*)list->data;
    for (int64_t i = index; i < list->len - 1; i++) {
        data[i] = data[i + 1];
    }
    list->len--;
}
