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

// ============================================================================
// Buffer - Extended Integer Read/Write (Node.js compatible)
// ============================================================================

/**
 * @brief Writes an unsigned 8-bit integer at offset.
 */
TML_EXPORT void buffer_write_u8(TmlBuffer* buf, int64_t offset, int32_t value) {
    if (!buf || offset < 0)
        return;
    if (offset >= buf->len) {
        buffer_ensure_capacity(buf, offset + 1 - buf->len);
        // Zero-fill gap
        while (buf->len <= offset) {
            buf->data[buf->len++] = 0;
        }
    }
    buf->data[offset] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Reads an unsigned 8-bit integer at offset.
 */
TML_EXPORT int32_t buffer_read_u8(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset >= buf->len)
        return 0;
    return (int32_t)buf->data[offset];
}

/**
 * @brief Reads a signed 8-bit integer at offset.
 */
TML_EXPORT int32_t buffer_read_i8(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset >= buf->len)
        return 0;
    return (int8_t)buf->data[offset];
}

/**
 * @brief Writes an unsigned 16-bit integer at offset (little-endian).
 */
TML_EXPORT void buffer_write_u16_le(TmlBuffer* buf, int64_t offset, int32_t value) {
    if (!buf || offset < 0)
        return;
    int64_t needed = offset + 2;
    if (needed > buf->len) {
        buffer_ensure_capacity(buf, needed - buf->len);
        while (buf->len < needed)
            buf->data[buf->len++] = 0;
    }
    buf->data[offset] = (uint8_t)(value & 0xFF);
    buf->data[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
}

/**
 * @brief Writes an unsigned 16-bit integer at offset (big-endian).
 */
TML_EXPORT void buffer_write_u16_be(TmlBuffer* buf, int64_t offset, int32_t value) {
    if (!buf || offset < 0)
        return;
    int64_t needed = offset + 2;
    if (needed > buf->len) {
        buffer_ensure_capacity(buf, needed - buf->len);
        while (buf->len < needed)
            buf->data[buf->len++] = 0;
    }
    buf->data[offset] = (uint8_t)((value >> 8) & 0xFF);
    buf->data[offset + 1] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Reads an unsigned 16-bit integer at offset (little-endian).
 */
TML_EXPORT int32_t buffer_read_u16_le(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 2 > buf->len)
        return 0;
    return (int32_t)buf->data[offset] | ((int32_t)buf->data[offset + 1] << 8);
}

/**
 * @brief Reads an unsigned 16-bit integer at offset (big-endian).
 */
TML_EXPORT int32_t buffer_read_u16_be(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 2 > buf->len)
        return 0;
    return ((int32_t)buf->data[offset] << 8) | (int32_t)buf->data[offset + 1];
}

/**
 * @brief Reads a signed 16-bit integer at offset (little-endian).
 */
TML_EXPORT int32_t buffer_read_i16_le(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 2 > buf->len)
        return 0;
    uint16_t val = (uint16_t)buf->data[offset] | ((uint16_t)buf->data[offset + 1] << 8);
    return (int16_t)val;
}

/**
 * @brief Reads a signed 16-bit integer at offset (big-endian).
 */
TML_EXPORT int32_t buffer_read_i16_be(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 2 > buf->len)
        return 0;
    uint16_t val = ((uint16_t)buf->data[offset] << 8) | (uint16_t)buf->data[offset + 1];
    return (int16_t)val;
}

/**
 * @brief Writes an unsigned 32-bit integer at offset (little-endian).
 */
TML_EXPORT void buffer_write_u32_le(TmlBuffer* buf, int64_t offset, int64_t value) {
    if (!buf || offset < 0)
        return;
    int64_t needed = offset + 4;
    if (needed > buf->len) {
        buffer_ensure_capacity(buf, needed - buf->len);
        while (buf->len < needed)
            buf->data[buf->len++] = 0;
    }
    buf->data[offset] = (uint8_t)(value & 0xFF);
    buf->data[offset + 1] = (uint8_t)((value >> 8) & 0xFF);
    buf->data[offset + 2] = (uint8_t)((value >> 16) & 0xFF);
    buf->data[offset + 3] = (uint8_t)((value >> 24) & 0xFF);
}

/**
 * @brief Writes an unsigned 32-bit integer at offset (big-endian).
 */
TML_EXPORT void buffer_write_u32_be(TmlBuffer* buf, int64_t offset, int64_t value) {
    if (!buf || offset < 0)
        return;
    int64_t needed = offset + 4;
    if (needed > buf->len) {
        buffer_ensure_capacity(buf, needed - buf->len);
        while (buf->len < needed)
            buf->data[buf->len++] = 0;
    }
    buf->data[offset] = (uint8_t)((value >> 24) & 0xFF);
    buf->data[offset + 1] = (uint8_t)((value >> 16) & 0xFF);
    buf->data[offset + 2] = (uint8_t)((value >> 8) & 0xFF);
    buf->data[offset + 3] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Reads an unsigned 32-bit integer at offset (little-endian).
 */
TML_EXPORT int64_t buffer_read_u32_le(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 4 > buf->len)
        return 0;
    return (int64_t)buf->data[offset] | ((int64_t)buf->data[offset + 1] << 8) |
           ((int64_t)buf->data[offset + 2] << 16) | ((int64_t)buf->data[offset + 3] << 24);
}

/**
 * @brief Reads an unsigned 32-bit integer at offset (big-endian).
 */
TML_EXPORT int64_t buffer_read_u32_be(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 4 > buf->len)
        return 0;
    return ((int64_t)buf->data[offset] << 24) | ((int64_t)buf->data[offset + 1] << 16) |
           ((int64_t)buf->data[offset + 2] << 8) | (int64_t)buf->data[offset + 3];
}

/**
 * @brief Reads a signed 32-bit integer at offset (little-endian).
 */
TML_EXPORT int32_t buffer_read_i32_le(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 4 > buf->len)
        return 0;
    uint32_t val = (uint32_t)buf->data[offset] | ((uint32_t)buf->data[offset + 1] << 8) |
                   ((uint32_t)buf->data[offset + 2] << 16) |
                   ((uint32_t)buf->data[offset + 3] << 24);
    return (int32_t)val;
}

/**
 * @brief Reads a signed 32-bit integer at offset (big-endian).
 */
TML_EXPORT int32_t buffer_read_i32_be(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 4 > buf->len)
        return 0;
    uint32_t val = ((uint32_t)buf->data[offset] << 24) | ((uint32_t)buf->data[offset + 1] << 16) |
                   ((uint32_t)buf->data[offset + 2] << 8) | (uint32_t)buf->data[offset + 3];
    return (int32_t)val;
}

/**
 * @brief Writes an unsigned 64-bit integer at offset (little-endian).
 */
TML_EXPORT void buffer_write_u64_le(TmlBuffer* buf, int64_t offset, uint64_t value) {
    if (!buf || offset < 0)
        return;
    int64_t needed = offset + 8;
    if (needed > buf->len) {
        buffer_ensure_capacity(buf, needed - buf->len);
        while (buf->len < needed)
            buf->data[buf->len++] = 0;
    }
    for (int i = 0; i < 8; i++) {
        buf->data[offset + i] = (uint8_t)((value >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief Writes an unsigned 64-bit integer at offset (big-endian).
 */
TML_EXPORT void buffer_write_u64_be(TmlBuffer* buf, int64_t offset, uint64_t value) {
    if (!buf || offset < 0)
        return;
    int64_t needed = offset + 8;
    if (needed > buf->len) {
        buffer_ensure_capacity(buf, needed - buf->len);
        while (buf->len < needed)
            buf->data[buf->len++] = 0;
    }
    for (int i = 0; i < 8; i++) {
        buf->data[offset + i] = (uint8_t)((value >> ((7 - i) * 8)) & 0xFF);
    }
}

/**
 * @brief Reads an unsigned 64-bit integer at offset (little-endian).
 */
TML_EXPORT uint64_t buffer_read_u64_le(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 8 > buf->len)
        return 0;
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value |= (uint64_t)buf->data[offset + i] << (i * 8);
    }
    return value;
}

/**
 * @brief Reads an unsigned 64-bit integer at offset (big-endian).
 */
TML_EXPORT uint64_t buffer_read_u64_be(TmlBuffer* buf, int64_t offset) {
    if (!buf || offset < 0 || offset + 8 > buf->len)
        return 0;
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value |= (uint64_t)buf->data[offset + i] << ((7 - i) * 8);
    }
    return value;
}

/**
 * @brief Reads a signed 64-bit integer at offset (little-endian).
 */
TML_EXPORT int64_t buffer_read_i64_le(TmlBuffer* buf, int64_t offset) {
    return (int64_t)buffer_read_u64_le(buf, offset);
}

/**
 * @brief Reads a signed 64-bit integer at offset (big-endian).
 */
TML_EXPORT int64_t buffer_read_i64_be(TmlBuffer* buf, int64_t offset) {
    return (int64_t)buffer_read_u64_be(buf, offset);
}

// ============================================================================
// Buffer - Float Read/Write
// ============================================================================

/**
 * @brief Writes a 32-bit float at offset (little-endian).
 */
TML_EXPORT void buffer_write_f32_le(TmlBuffer* buf, int64_t offset, float value) {
    if (!buf || offset < 0)
        return;
    union {
        float f;
        uint32_t i;
    } u;
    u.f = value;
    buffer_write_u32_le(buf, offset, u.i);
}

/**
 * @brief Writes a 32-bit float at offset (big-endian).
 */
TML_EXPORT void buffer_write_f32_be(TmlBuffer* buf, int64_t offset, float value) {
    if (!buf || offset < 0)
        return;
    union {
        float f;
        uint32_t i;
    } u;
    u.f = value;
    buffer_write_u32_be(buf, offset, u.i);
}

/**
 * @brief Reads a 32-bit float at offset (little-endian).
 */
TML_EXPORT float buffer_read_f32_le(TmlBuffer* buf, int64_t offset) {
    union {
        float f;
        uint32_t i;
    } u;
    u.i = (uint32_t)buffer_read_u32_le(buf, offset);
    return u.f;
}

/**
 * @brief Reads a 32-bit float at offset (big-endian).
 */
TML_EXPORT float buffer_read_f32_be(TmlBuffer* buf, int64_t offset) {
    union {
        float f;
        uint32_t i;
    } u;
    u.i = (uint32_t)buffer_read_u32_be(buf, offset);
    return u.f;
}

/**
 * @brief Writes a 64-bit double at offset (little-endian).
 */
TML_EXPORT void buffer_write_f64_le(TmlBuffer* buf, int64_t offset, double value) {
    if (!buf || offset < 0)
        return;
    union {
        double d;
        uint64_t i;
    } u;
    u.d = value;
    buffer_write_u64_le(buf, offset, u.i);
}

/**
 * @brief Writes a 64-bit double at offset (big-endian).
 */
TML_EXPORT void buffer_write_f64_be(TmlBuffer* buf, int64_t offset, double value) {
    if (!buf || offset < 0)
        return;
    union {
        double d;
        uint64_t i;
    } u;
    u.d = value;
    buffer_write_u64_be(buf, offset, u.i);
}

/**
 * @brief Reads a 64-bit double at offset (little-endian).
 */
TML_EXPORT double buffer_read_f64_le(TmlBuffer* buf, int64_t offset) {
    union {
        double d;
        uint64_t i;
    } u;
    u.i = buffer_read_u64_le(buf, offset);
    return u.d;
}

/**
 * @brief Reads a 64-bit double at offset (big-endian).
 */
TML_EXPORT double buffer_read_f64_be(TmlBuffer* buf, int64_t offset) {
    union {
        double d;
        uint64_t i;
    } u;
    u.i = buffer_read_u64_be(buf, offset);
    return u.d;
}

// ============================================================================
// Buffer - Index Access and Manipulation
// ============================================================================

/**
 * @brief Gets a byte at the given index.
 */
TML_EXPORT int32_t buffer_get(TmlBuffer* buf, int64_t index) {
    if (!buf || index < 0 || index >= buf->len)
        return 0;
    return (int32_t)buf->data[index];
}

/**
 * @brief Sets a byte at the given index.
 */
TML_EXPORT void buffer_set(TmlBuffer* buf, int64_t index, int32_t value) {
    if (!buf || index < 0 || index >= buf->len)
        return;
    buf->data[index] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Fills the buffer with a value from start to end.
 */
TML_EXPORT void buffer_fill(TmlBuffer* buf, int32_t value, int64_t start, int64_t end) {
    if (!buf)
        return;
    if (start < 0)
        start = 0;
    if (end < 0 || end > buf->len)
        end = buf->len;
    for (int64_t i = start; i < end; i++) {
        buf->data[i] = (uint8_t)(value & 0xFF);
    }
}

/**
 * @brief Copies bytes from source buffer to target buffer.
 */
TML_EXPORT int64_t buffer_copy(TmlBuffer* source, TmlBuffer* target, int64_t target_start,
                               int64_t source_start, int64_t source_end) {
    if (!source || !target)
        return 0;
    if (source_start < 0)
        source_start = 0;
    if (source_end < 0 || source_end > source->len)
        source_end = source->len;
    if (target_start < 0)
        target_start = 0;

    int64_t bytes_to_copy = source_end - source_start;
    if (bytes_to_copy <= 0)
        return 0;

    // Ensure target has enough space
    int64_t needed = target_start + bytes_to_copy;
    if (needed > target->len) {
        buffer_ensure_capacity(target, needed - target->len);
        while (target->len < needed)
            target->data[target->len++] = 0;
    }

    // Copy bytes
    for (int64_t i = 0; i < bytes_to_copy; i++) {
        target->data[target_start + i] = source->data[source_start + i];
    }
    return bytes_to_copy;
}

/**
 * @brief Creates a new buffer that is a slice of this buffer.
 */
TML_EXPORT TmlBuffer* buffer_slice(TmlBuffer* buf, int64_t start, int64_t end) {
    if (!buf)
        return buffer_create(0);
    if (start < 0)
        start = 0;
    if (end < 0 || end > buf->len)
        end = buf->len;
    if (start >= end)
        return buffer_create(0);

    int64_t slice_len = end - start;
    TmlBuffer* result = buffer_create(slice_len);
    if (!result)
        return NULL;

    for (int64_t i = 0; i < slice_len; i++) {
        result->data[i] = buf->data[start + i];
    }
    result->len = slice_len;
    return result;
}

// ============================================================================
// Buffer - Comparison and Search
// ============================================================================

/**
 * @brief Compares two buffers.
 * Returns: -1 if buf1 < buf2, 0 if equal, 1 if buf1 > buf2
 */
TML_EXPORT int32_t buffer_compare(TmlBuffer* buf1, TmlBuffer* buf2) {
    if (!buf1 && !buf2)
        return 0;
    if (!buf1)
        return -1;
    if (!buf2)
        return 1;

    int64_t min_len = buf1->len < buf2->len ? buf1->len : buf2->len;
    for (int64_t i = 0; i < min_len; i++) {
        if (buf1->data[i] < buf2->data[i])
            return -1;
        if (buf1->data[i] > buf2->data[i])
            return 1;
    }

    if (buf1->len < buf2->len)
        return -1;
    if (buf1->len > buf2->len)
        return 1;
    return 0;
}

/**
 * @brief Checks if two buffers are equal.
 */
TML_EXPORT int32_t buffer_equals(TmlBuffer* buf1, TmlBuffer* buf2) {
    return buffer_compare(buf1, buf2) == 0;
}

/**
 * @brief Finds the first occurrence of a byte value.
 * Returns -1 if not found.
 */
TML_EXPORT int64_t buffer_index_of(TmlBuffer* buf, int32_t value, int64_t start) {
    if (!buf)
        return -1;
    if (start < 0)
        start = 0;
    for (int64_t i = start; i < buf->len; i++) {
        if (buf->data[i] == (uint8_t)(value & 0xFF))
            return i;
    }
    return -1;
}

/**
 * @brief Finds the last occurrence of a byte value.
 * Returns -1 if not found.
 */
TML_EXPORT int64_t buffer_last_index_of(TmlBuffer* buf, int32_t value, int64_t start) {
    if (!buf || buf->len == 0)
        return -1;
    if (start < 0 || start >= buf->len)
        start = buf->len - 1;
    for (int64_t i = start; i >= 0; i--) {
        if (buf->data[i] == (uint8_t)(value & 0xFF))
            return i;
    }
    return -1;
}

/**
 * @brief Checks if buffer contains a byte value.
 */
TML_EXPORT int32_t buffer_includes(TmlBuffer* buf, int32_t value, int64_t start) {
    return buffer_index_of(buf, value, start) >= 0;
}

// ============================================================================
// Buffer - Byte Swapping
// ============================================================================

/**
 * @brief Swaps byte order for 16-bit values in place.
 */
TML_EXPORT void buffer_swap16(TmlBuffer* buf) {
    if (!buf || buf->len < 2)
        return;
    for (int64_t i = 0; i + 1 < buf->len; i += 2) {
        uint8_t tmp = buf->data[i];
        buf->data[i] = buf->data[i + 1];
        buf->data[i + 1] = tmp;
    }
}

/**
 * @brief Swaps byte order for 32-bit values in place.
 */
TML_EXPORT void buffer_swap32(TmlBuffer* buf) {
    if (!buf || buf->len < 4)
        return;
    for (int64_t i = 0; i + 3 < buf->len; i += 4) {
        uint8_t tmp0 = buf->data[i];
        uint8_t tmp1 = buf->data[i + 1];
        buf->data[i] = buf->data[i + 3];
        buf->data[i + 1] = buf->data[i + 2];
        buf->data[i + 2] = tmp1;
        buf->data[i + 3] = tmp0;
    }
}

/**
 * @brief Swaps byte order for 64-bit values in place.
 */
TML_EXPORT void buffer_swap64(TmlBuffer* buf) {
    if (!buf || buf->len < 8)
        return;
    for (int64_t i = 0; i + 7 < buf->len; i += 8) {
        for (int j = 0; j < 4; j++) {
            uint8_t tmp = buf->data[i + j];
            buf->data[i + j] = buf->data[i + 7 - j];
            buf->data[i + 7 - j] = tmp;
        }
    }
}

// ============================================================================
// Buffer - String Conversion
// ============================================================================

/**
 * @brief Converts buffer to hexadecimal string.
 */
TML_EXPORT char* buffer_to_hex(TmlBuffer* buf) {
    if (!buf || buf->len == 0) {
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }

    char* hex = (char*)malloc((size_t)(buf->len * 2 + 1));
    if (!hex)
        return NULL;

    static const char hex_chars[] = "0123456789abcdef";
    for (int64_t i = 0; i < buf->len; i++) {
        hex[i * 2] = hex_chars[(buf->data[i] >> 4) & 0xF];
        hex[i * 2 + 1] = hex_chars[buf->data[i] & 0xF];
    }
    hex[buf->len * 2] = '\0';
    return hex;
}

/**
 * @brief Creates a buffer from hexadecimal string.
 */
TML_EXPORT TmlBuffer* buffer_from_hex(const char* hex) {
    if (!hex)
        return buffer_create(0);

    size_t hex_len = 0;
    while (hex[hex_len])
        hex_len++;

    if (hex_len % 2 != 0)
        return buffer_create(0);

    int64_t buf_len = hex_len / 2;
    TmlBuffer* buf = buffer_create(buf_len);
    if (!buf)
        return NULL;

    for (int64_t i = 0; i < buf_len; i++) {
        char c1 = hex[i * 2];
        char c2 = hex[i * 2 + 1];
        int v1 = (c1 >= '0' && c1 <= '9')   ? c1 - '0'
                 : (c1 >= 'a' && c1 <= 'f') ? c1 - 'a' + 10
                 : (c1 >= 'A' && c1 <= 'F') ? c1 - 'A' + 10
                                            : 0;
        int v2 = (c2 >= '0' && c2 <= '9')   ? c2 - '0'
                 : (c2 >= 'a' && c2 <= 'f') ? c2 - 'a' + 10
                 : (c2 >= 'A' && c2 <= 'F') ? c2 - 'A' + 10
                                            : 0;
        buf->data[i] = (uint8_t)((v1 << 4) | v2);
    }
    buf->len = buf_len;
    return buf;
}

/**
 * @brief Converts buffer to UTF-8 string.
 */
TML_EXPORT char* buffer_to_string(TmlBuffer* buf) {
    if (!buf || buf->len == 0) {
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }

    char* str = (char*)malloc((size_t)(buf->len + 1));
    if (!str)
        return NULL;

    for (int64_t i = 0; i < buf->len; i++) {
        str[i] = (char)buf->data[i];
    }
    str[buf->len] = '\0';
    return str;
}

/**
 * @brief Creates a buffer from UTF-8 string.
 */
TML_EXPORT TmlBuffer* buffer_from_string(const char* str) {
    if (!str)
        return buffer_create(0);

    size_t str_len = 0;
    while (str[str_len])
        str_len++;

    TmlBuffer* buf = buffer_create((int64_t)str_len);
    if (!buf)
        return NULL;

    for (size_t i = 0; i < str_len; i++) {
        buf->data[i] = (uint8_t)str[i];
    }
    buf->len = (int64_t)str_len;
    return buf;
}

/**
 * @brief Concatenates multiple buffers.
 */
TML_EXPORT TmlBuffer* buffer_concat(TmlBuffer** buffers, int64_t count) {
    if (!buffers || count <= 0)
        return buffer_create(0);

    // Calculate total length
    int64_t total_len = 0;
    for (int64_t i = 0; i < count; i++) {
        if (buffers[i])
            total_len += buffers[i]->len;
    }

    TmlBuffer* result = buffer_create(total_len);
    if (!result)
        return NULL;

    // Copy all buffers
    int64_t pos = 0;
    for (int64_t i = 0; i < count; i++) {
        if (buffers[i]) {
            for (int64_t j = 0; j < buffers[i]->len; j++) {
                result->data[pos++] = buffers[i]->data[j];
            }
        }
    }
    result->len = total_len;
    return result;
}
