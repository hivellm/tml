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

/**
 * @brief Returns the first element of the list.
 */
TML_EXPORT int64_t list_first(TmlList* list) {
    if (!list || list->len == 0)
        return 0;
    return ((int64_t*)list->data)[0];
}

/**
 * @brief Returns the last element of the list.
 */
TML_EXPORT int64_t list_last(TmlList* list) {
    if (!list || list->len == 0)
        return 0;
    return ((int64_t*)list->data)[list->len - 1];
}

// ============================================================================
// HashMap Functions
// ============================================================================

/**
 * @brief Internal structure for TML hash map entry.
 */
typedef struct TmlHashEntry {
    int64_t key;
    int64_t value;
    int32_t occupied;
    int32_t deleted;
} TmlHashEntry;

/**
 * @brief Internal structure for TML hash map.
 */
typedef struct TmlHashMap {
    TmlHashEntry* entries;
    int64_t len;
    int64_t capacity;
} TmlHashMap;

/**
 * @brief Internal structure for TML hash map iterator.
 */
typedef struct TmlHashMapIter {
    TmlHashMap* map;
    int64_t index;
} TmlHashMapIter;

/**
 * @brief Simple hash function for i64 keys.
 */
static int64_t hash_key(int64_t key) {
    // FNV-1a inspired hash
    uint64_t h = 14695981039346656037ULL;
    h ^= (uint64_t)key;
    h *= 1099511628211ULL;
    return (int64_t)(h & 0x7FFFFFFFFFFFFFFFULL);
}

/**
 * @brief Creates a new hash map with initial capacity.
 */
TML_EXPORT TmlHashMap* hashmap_create(int64_t initial_capacity) {
    TmlHashMap* map = (TmlHashMap*)malloc(sizeof(TmlHashMap));
    if (!map)
        return NULL;

    int64_t cap = initial_capacity > 0 ? initial_capacity : 16;
    map->entries = (TmlHashEntry*)calloc((size_t)cap, sizeof(TmlHashEntry));
    if (!map->entries) {
        free(map);
        return NULL;
    }
    map->len = 0;
    map->capacity = cap;
    return map;
}

/**
 * @brief Destroys a hash map and frees all memory.
 */
TML_EXPORT void hashmap_destroy(TmlHashMap* map) {
    if (!map)
        return;
    if (map->entries)
        free(map->entries);
    free(map);
}

/**
 * @brief Sets a key-value pair in the hash map.
 */
TML_EXPORT void hashmap_set(TmlHashMap* map, int64_t key, int64_t value) {
    if (!map)
        return;

    // Resize if load factor > 0.7
    if (map->len * 10 > map->capacity * 7) {
        int64_t new_cap = map->capacity * 2;
        TmlHashEntry* new_entries = (TmlHashEntry*)calloc((size_t)new_cap, sizeof(TmlHashEntry));
        if (!new_entries)
            return;

        // Rehash all entries
        for (int64_t i = 0; i < map->capacity; i++) {
            if (map->entries[i].occupied && !map->entries[i].deleted) {
                int64_t h = hash_key(map->entries[i].key) % new_cap;
                while (new_entries[h].occupied) {
                    h = (h + 1) % new_cap;
                }
                new_entries[h].key = map->entries[i].key;
                new_entries[h].value = map->entries[i].value;
                new_entries[h].occupied = 1;
            }
        }
        free(map->entries);
        map->entries = new_entries;
        map->capacity = new_cap;
    }

    int64_t h = hash_key(key) % map->capacity;
    while (map->entries[h].occupied && !map->entries[h].deleted && map->entries[h].key != key) {
        h = (h + 1) % map->capacity;
    }

    if (!map->entries[h].occupied || map->entries[h].deleted) {
        map->len++;
    }
    map->entries[h].key = key;
    map->entries[h].value = value;
    map->entries[h].occupied = 1;
    map->entries[h].deleted = 0;
}

/**
 * @brief Gets a value by key from the hash map.
 */
TML_EXPORT int64_t hashmap_get(TmlHashMap* map, int64_t key) {
    if (!map || map->len == 0)
        return 0;

    int64_t h = hash_key(key) % map->capacity;
    int64_t start = h;
    while (map->entries[h].occupied) {
        if (!map->entries[h].deleted && map->entries[h].key == key) {
            return map->entries[h].value;
        }
        h = (h + 1) % map->capacity;
        if (h == start)
            break;
    }
    return 0;
}

/**
 * @brief Checks if a key exists in the hash map.
 */
TML_EXPORT int32_t hashmap_has(TmlHashMap* map, int64_t key) {
    if (!map || map->len == 0)
        return 0;

    int64_t h = hash_key(key) % map->capacity;
    int64_t start = h;
    while (map->entries[h].occupied) {
        if (!map->entries[h].deleted && map->entries[h].key == key) {
            return 1;
        }
        h = (h + 1) % map->capacity;
        if (h == start)
            break;
    }
    return 0;
}

/**
 * @brief Removes a key from the hash map.
 */
TML_EXPORT int32_t hashmap_remove(TmlHashMap* map, int64_t key) {
    if (!map || map->len == 0)
        return 0;

    int64_t h = hash_key(key) % map->capacity;
    int64_t start = h;
    while (map->entries[h].occupied) {
        if (!map->entries[h].deleted && map->entries[h].key == key) {
            map->entries[h].deleted = 1;
            map->len--;
            return 1;
        }
        h = (h + 1) % map->capacity;
        if (h == start)
            break;
    }
    return 0;
}

/**
 * @brief Returns the number of entries in the hash map.
 */
TML_EXPORT int64_t hashmap_len(TmlHashMap* map) {
    return map ? map->len : 0;
}

/**
 * @brief Clears the hash map without freeing memory.
 */
TML_EXPORT void hashmap_clear(TmlHashMap* map) {
    if (!map)
        return;
    for (int64_t i = 0; i < map->capacity; i++) {
        map->entries[i].occupied = 0;
        map->entries[i].deleted = 0;
    }
    map->len = 0;
}

/**
 * @brief Creates an iterator for the hash map.
 */
TML_EXPORT TmlHashMapIter* hashmap_iter_create(TmlHashMap* map) {
    TmlHashMapIter* iter = (TmlHashMapIter*)malloc(sizeof(TmlHashMapIter));
    if (!iter)
        return NULL;
    iter->map = map;
    iter->index = -1;
    // Advance to first valid entry
    if (map) {
        for (int64_t i = 0; i < map->capacity; i++) {
            if (map->entries[i].occupied && !map->entries[i].deleted) {
                iter->index = i;
                break;
            }
        }
    }
    return iter;
}

/**
 * @brief Destroys a hash map iterator.
 */
TML_EXPORT void hashmap_iter_destroy(TmlHashMapIter* iter) {
    if (iter)
        free(iter);
}

/**
 * @brief Checks if the iterator has more entries.
 */
TML_EXPORT int32_t hashmap_iter_has_next(TmlHashMapIter* iter) {
    if (!iter || !iter->map)
        return 0;
    return iter->index >= 0 && iter->index < iter->map->capacity;
}

/**
 * @brief Returns the current key.
 */
TML_EXPORT int64_t hashmap_iter_key(TmlHashMapIter* iter) {
    if (!iter || !iter->map || iter->index < 0)
        return 0;
    return iter->map->entries[iter->index].key;
}

/**
 * @brief Returns the current value.
 */
TML_EXPORT int64_t hashmap_iter_value(TmlHashMapIter* iter) {
    if (!iter || !iter->map || iter->index < 0)
        return 0;
    return iter->map->entries[iter->index].value;
}

/**
 * @brief Advances to the next entry.
 */
TML_EXPORT void hashmap_iter_next(TmlHashMapIter* iter) {
    if (!iter || !iter->map)
        return;
    for (int64_t i = iter->index + 1; i < iter->map->capacity; i++) {
        if (iter->map->entries[i].occupied && !iter->map->entries[i].deleted) {
            iter->index = i;
            return;
        }
    }
    iter->index = -1; // No more entries
}

// ============================================================================
// Buffer Functions
// ============================================================================

/**
 * @brief Internal structure for TML buffer.
 */
typedef struct TmlBuffer {
    uint8_t* data;
    int64_t len;
    int64_t capacity;
    int64_t read_pos;
} TmlBuffer;

/**
 * @brief Creates a new buffer with initial capacity.
 */
TML_EXPORT TmlBuffer* buffer_create(int64_t initial_capacity) {
    TmlBuffer* buf = (TmlBuffer*)malloc(sizeof(TmlBuffer));
    if (!buf)
        return NULL;

    int64_t cap = initial_capacity > 0 ? initial_capacity : 64;
    buf->data = (uint8_t*)malloc((size_t)cap);
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    buf->len = 0;
    buf->capacity = cap;
    buf->read_pos = 0;
    return buf;
}

/**
 * @brief Destroys a buffer and frees all memory.
 */
TML_EXPORT void buffer_destroy(TmlBuffer* buf) {
    if (!buf)
        return;
    if (buf->data)
        free(buf->data);
    free(buf);
}

/**
 * @brief Ensures buffer has enough capacity, growing if needed.
 */
static void buffer_ensure_capacity(TmlBuffer* buf, int64_t needed) {
    if (buf->len + needed <= buf->capacity)
        return;
    int64_t new_cap = buf->capacity * 2;
    while (new_cap < buf->len + needed) {
        new_cap *= 2;
    }
    uint8_t* new_data = (uint8_t*)realloc(buf->data, (size_t)new_cap);
    if (!new_data)
        return;
    buf->data = new_data;
    buf->capacity = new_cap;
}

/**
 * @brief Writes a single byte to the buffer.
 */
TML_EXPORT void buffer_write_byte(TmlBuffer* buf, int32_t value) {
    if (!buf)
        return;
    buffer_ensure_capacity(buf, 1);
    buf->data[buf->len++] = (uint8_t)value;
}

/**
 * @brief Writes an i32 to the buffer (little endian).
 */
TML_EXPORT void buffer_write_i32(TmlBuffer* buf, int32_t value) {
    if (!buf)
        return;
    buffer_ensure_capacity(buf, 4);
    buf->data[buf->len++] = (uint8_t)(value & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 8) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 16) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 24) & 0xFF);
}

/**
 * @brief Writes an i64 to the buffer (little endian).
 */
TML_EXPORT void buffer_write_i64(TmlBuffer* buf, int64_t value) {
    if (!buf)
        return;
    buffer_ensure_capacity(buf, 8);
    for (int i = 0; i < 8; i++) {
        buf->data[buf->len++] = (uint8_t)((value >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief Reads a single byte from the buffer.
 */
TML_EXPORT int32_t buffer_read_byte(TmlBuffer* buf) {
    if (!buf || buf->read_pos >= buf->len)
        return 0;
    return (int32_t)buf->data[buf->read_pos++];
}

/**
 * @brief Reads an i32 from the buffer (little endian).
 */
TML_EXPORT int32_t buffer_read_i32(TmlBuffer* buf) {
    if (!buf || buf->read_pos + 4 > buf->len)
        return 0;
    int32_t value = 0;
    value |= (int32_t)buf->data[buf->read_pos++];
    value |= (int32_t)buf->data[buf->read_pos++] << 8;
    value |= (int32_t)buf->data[buf->read_pos++] << 16;
    value |= (int32_t)buf->data[buf->read_pos++] << 24;
    return value;
}

/**
 * @brief Reads an i64 from the buffer (little endian).
 */
TML_EXPORT int64_t buffer_read_i64(TmlBuffer* buf) {
    if (!buf || buf->read_pos + 8 > buf->len)
        return 0;
    int64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value |= (int64_t)buf->data[buf->read_pos++] << (i * 8);
    }
    return value;
}

/**
 * @brief Returns the number of bytes written to the buffer.
 */
TML_EXPORT int64_t buffer_len(TmlBuffer* buf) {
    return buf ? buf->len : 0;
}

/**
 * @brief Returns the capacity of the buffer.
 */
TML_EXPORT int64_t buffer_capacity(TmlBuffer* buf) {
    return buf ? buf->capacity : 0;
}

/**
 * @brief Returns the number of bytes remaining to read.
 */
TML_EXPORT int64_t buffer_remaining(TmlBuffer* buf) {
    if (!buf)
        return 0;
    return buf->len - buf->read_pos;
}

/**
 * @brief Clears the buffer (resets length and read position).
 */
TML_EXPORT void buffer_clear(TmlBuffer* buf) {
    if (!buf)
        return;
    buf->len = 0;
    buf->read_pos = 0;
}

/**
 * @brief Resets the read position to the beginning.
 */
TML_EXPORT void buffer_reset_read(TmlBuffer* buf) {
    if (!buf)
        return;
    buf->read_pos = 0;
}
