// TML Standard Library - Collections Runtime Implementation
// Implements: List, HashMap, Buffer

#include "collections.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// List - Dynamic array implementation
// ============================================================================

TmlList* list_create(int64_t initial_capacity) {
    TmlList* list = (TmlList*)malloc(sizeof(TmlList));
    if (!list)
        return NULL;

    if (initial_capacity < 8)
        initial_capacity = 8;

    list->data = (int64_t*)malloc(sizeof(int64_t) * initial_capacity);
    if (!list->data) {
        free(list);
        return NULL;
    }

    list->len = 0;
    list->capacity = initial_capacity;
    return list;
}

void list_destroy(TmlList* list) {
    if (!list)
        return;
    free(list->data);
    free(list);
}

static void list_grow(TmlList* list) {
    int64_t new_capacity = list->capacity * 2;
    int64_t* new_data = (int64_t*)realloc(list->data, sizeof(int64_t) * new_capacity);
    if (new_data) {
        list->data = new_data;
        list->capacity = new_capacity;
    }
}

void list_push(TmlList* list, int64_t value) {
    if (!list)
        return;
    if (list->len >= list->capacity) {
        list_grow(list);
    }
    list->data[list->len++] = value;
}

int64_t list_pop(TmlList* list) {
    if (!list || list->len == 0)
        return 0;
    return list->data[--list->len];
}

int64_t list_get(TmlList* list, int64_t index) {
    if (!list || index < 0 || index >= list->len)
        return 0;
    return list->data[index];
}

void list_set(TmlList* list, int64_t index, int64_t value) {
    if (!list || index < 0 || index >= list->len)
        return;
    list->data[index] = value;
}

int64_t list_len(TmlList* list) {
    return list ? list->len : 0;
}

int64_t list_capacity(TmlList* list) {
    return list ? list->capacity : 0;
}

void list_clear(TmlList* list) {
    if (list)
        list->len = 0;
}

int32_t list_is_empty(TmlList* list) {
    return !list || list->len == 0;
}

void list_resize(TmlList* list, int64_t new_len) {
    if (!list || new_len < 0)
        return;

    // Grow capacity if needed
    if (new_len > list->capacity) {
        int64_t new_capacity = list->capacity;
        while (new_capacity < new_len) {
            new_capacity *= 2;
        }
        int64_t* new_data = (int64_t*)realloc(list->data, sizeof(int64_t) * new_capacity);
        if (!new_data)
            return;
        list->data = new_data;
        list->capacity = new_capacity;
    }

    // Fill new elements with 0 if growing
    if (new_len > list->len) {
        memset(list->data + list->len, 0, sizeof(int64_t) * (new_len - list->len));
    }

    list->len = new_len;
}

void list_reserve(TmlList* list, int64_t min_capacity) {
    if (!list || min_capacity <= list->capacity)
        return;

    int64_t* new_data = (int64_t*)realloc(list->data, sizeof(int64_t) * min_capacity);
    if (new_data) {
        list->data = new_data;
        list->capacity = min_capacity;
    }
}

void list_shrink_to_fit(TmlList* list) {
    if (!list || list->len == 0)
        return;

    int64_t* new_data = (int64_t*)realloc(list->data, sizeof(int64_t) * list->len);
    if (new_data) {
        list->data = new_data;
        list->capacity = list->len;
    }
}

int64_t list_remove(TmlList* list, int64_t index) {
    if (!list || index < 0 || index >= list->len)
        return 0;

    int64_t value = list->data[index];

    // Shift elements left
    memmove(list->data + index, list->data + index + 1, sizeof(int64_t) * (list->len - index - 1));

    list->len--;
    return value;
}

void list_insert(TmlList* list, int64_t index, int64_t value) {
    if (!list || index < 0 || index > list->len)
        return;

    // Grow if needed
    if (list->len >= list->capacity) {
        list_grow(list);
    }

    // Shift elements right
    if (index < list->len) {
        memmove(list->data + index + 1, list->data + index, sizeof(int64_t) * (list->len - index));
    }

    list->data[index] = value;
    list->len++;
}

void list_reverse(TmlList* list) {
    if (!list || list->len <= 1)
        return;

    int64_t left = 0;
    int64_t right = list->len - 1;
    while (left < right) {
        int64_t temp = list->data[left];
        list->data[left] = list->data[right];
        list->data[right] = temp;
        left++;
        right--;
    }
}

// ============================================================================
// HashMap - Open addressing with linear probing
// ============================================================================

static uint64_t hash_i64(int64_t key) {
    // FNV-1a inspired hash for int64
    uint64_t h = 14695981039346656037ULL;
    uint8_t* bytes = (uint8_t*)&key;
    for (int i = 0; i < 8; i++) {
        h ^= bytes[i];
        h *= 1099511628211ULL;
    }
    return h;
}

TmlHashMap* hashmap_create(int64_t initial_capacity) {
    TmlHashMap* map = (TmlHashMap*)malloc(sizeof(TmlHashMap));
    if (!map)
        return NULL;

    if (initial_capacity < 16)
        initial_capacity = 16;

    map->entries = (HashEntry*)calloc(initial_capacity, sizeof(HashEntry));
    if (!map->entries) {
        free(map);
        return NULL;
    }

    map->capacity = initial_capacity;
    map->len = 0;
    return map;
}

void hashmap_destroy(TmlHashMap* map) {
    if (!map)
        return;
    free(map->entries);
    free(map);
}

static void hashmap_grow(TmlHashMap* map) {
    int64_t old_capacity = map->capacity;
    HashEntry* old_entries = map->entries;

    int64_t new_capacity = old_capacity * 2;
    map->entries = (HashEntry*)calloc(new_capacity, sizeof(HashEntry));
    if (!map->entries) {
        map->entries = old_entries;
        return;
    }

    map->capacity = new_capacity;
    map->len = 0;

    // Rehash all entries
    for (int64_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].occupied && !old_entries[i].deleted) {
            hashmap_set(map, old_entries[i].key, old_entries[i].value);
        }
    }

    free(old_entries);
}

void hashmap_set(TmlHashMap* map, int64_t key, int64_t value) {
    if (!map)
        return;

    // Grow if load factor > 0.7
    if (map->len * 10 > map->capacity * 7) {
        hashmap_grow(map);
    }

    uint64_t hash = hash_i64(key);
    int64_t index = hash % map->capacity;
    int64_t first_deleted = -1;

    for (int64_t i = 0; i < map->capacity; i++) {
        int64_t probe = (index + i) % map->capacity;
        HashEntry* entry = &map->entries[probe];

        if (!entry->occupied) {
            // Found empty slot
            if (first_deleted >= 0) {
                probe = first_deleted;
                entry = &map->entries[probe];
            }
            entry->key = key;
            entry->value = value;
            entry->occupied = true;
            entry->deleted = false;
            map->len++;
            return;
        }

        if (entry->deleted && first_deleted < 0) {
            first_deleted = probe;
            continue;
        }

        if (!entry->deleted && entry->key == key) {
            // Key exists, update value
            entry->value = value;
            return;
        }
    }

    // Use first deleted slot if we found one
    if (first_deleted >= 0) {
        HashEntry* entry = &map->entries[first_deleted];
        entry->key = key;
        entry->value = value;
        entry->occupied = true;
        entry->deleted = false;
        map->len++;
    }
}

int64_t hashmap_get(TmlHashMap* map, int64_t key) {
    if (!map)
        return 0;

    uint64_t hash = hash_i64(key);
    int64_t index = hash % map->capacity;

    for (int64_t i = 0; i < map->capacity; i++) {
        int64_t probe = (index + i) % map->capacity;
        HashEntry* entry = &map->entries[probe];

        if (!entry->occupied)
            return 0; // Not found
        if (!entry->deleted && entry->key == key) {
            return entry->value;
        }
    }

    return 0;
}

bool hashmap_has(TmlHashMap* map, int64_t key) {
    if (!map)
        return false;

    uint64_t hash = hash_i64(key);
    int64_t index = hash % map->capacity;

    for (int64_t i = 0; i < map->capacity; i++) {
        int64_t probe = (index + i) % map->capacity;
        HashEntry* entry = &map->entries[probe];

        if (!entry->occupied)
            return false;
        if (!entry->deleted && entry->key == key) {
            return true;
        }
    }

    return false;
}

bool hashmap_remove(TmlHashMap* map, int64_t key) {
    if (!map)
        return false;

    uint64_t hash = hash_i64(key);
    int64_t index = hash % map->capacity;

    for (int64_t i = 0; i < map->capacity; i++) {
        int64_t probe = (index + i) % map->capacity;
        HashEntry* entry = &map->entries[probe];

        if (!entry->occupied)
            return false;
        if (!entry->deleted && entry->key == key) {
            entry->deleted = true;
            map->len--;
            return true;
        }
    }

    return false;
}

int64_t hashmap_len(TmlHashMap* map) {
    return map ? map->len : 0;
}

void hashmap_clear(TmlHashMap* map) {
    if (!map)
        return;
    memset(map->entries, 0, sizeof(HashEntry) * map->capacity);
    map->len = 0;
}

// ============================================================================
// Buffer - Byte buffer for binary data
// ============================================================================

TmlBuffer* buffer_create(int64_t initial_capacity) {
    TmlBuffer* buf = (TmlBuffer*)malloc(sizeof(TmlBuffer));
    if (!buf)
        return NULL;

    if (initial_capacity < 64)
        initial_capacity = 64;

    buf->data = (uint8_t*)malloc(initial_capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->len = 0;
    buf->capacity = initial_capacity;
    buf->read_pos = 0;
    return buf;
}

void buffer_destroy(TmlBuffer* buf) {
    if (!buf)
        return;
    free(buf->data);
    free(buf);
}

static void buffer_grow(TmlBuffer* buf, int64_t min_capacity) {
    int64_t new_capacity = buf->capacity * 2;
    if (new_capacity < min_capacity)
        new_capacity = min_capacity;

    uint8_t* new_data = (uint8_t*)realloc(buf->data, new_capacity);
    if (new_data) {
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
}

void buffer_write_byte(TmlBuffer* buf, int32_t byte) {
    if (!buf)
        return;
    if (buf->len >= buf->capacity) {
        buffer_grow(buf, buf->capacity + 1);
    }
    buf->data[buf->len++] = (uint8_t)(byte & 0xFF);
}

void buffer_write_i32(TmlBuffer* buf, int32_t value) {
    if (!buf)
        return;
    if (buf->len + 4 > buf->capacity) {
        buffer_grow(buf, buf->len + 4);
    }
    // Little-endian
    buf->data[buf->len++] = (uint8_t)(value & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 8) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 16) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 24) & 0xFF);
}

void buffer_write_i64(TmlBuffer* buf, int64_t value) {
    if (!buf)
        return;
    if (buf->len + 8 > buf->capacity) {
        buffer_grow(buf, buf->len + 8);
    }
    // Little-endian
    buf->data[buf->len++] = (uint8_t)(value & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 8) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 16) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 24) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 32) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 40) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 48) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 56) & 0xFF);
}

int32_t buffer_read_byte(TmlBuffer* buf) {
    if (!buf || buf->read_pos >= buf->len)
        return -1;
    return buf->data[buf->read_pos++];
}

int32_t buffer_read_i32(TmlBuffer* buf) {
    if (!buf || buf->read_pos + 4 > buf->len)
        return 0;
    int32_t value = 0;
    value |= buf->data[buf->read_pos++];
    value |= buf->data[buf->read_pos++] << 8;
    value |= buf->data[buf->read_pos++] << 16;
    value |= buf->data[buf->read_pos++] << 24;
    return value;
}

int64_t buffer_read_i64(TmlBuffer* buf) {
    if (!buf || buf->read_pos + 8 > buf->len)
        return 0;
    int64_t value = 0;
    value |= (int64_t)buf->data[buf->read_pos++];
    value |= (int64_t)buf->data[buf->read_pos++] << 8;
    value |= (int64_t)buf->data[buf->read_pos++] << 16;
    value |= (int64_t)buf->data[buf->read_pos++] << 24;
    value |= (int64_t)buf->data[buf->read_pos++] << 32;
    value |= (int64_t)buf->data[buf->read_pos++] << 40;
    value |= (int64_t)buf->data[buf->read_pos++] << 48;
    value |= (int64_t)buf->data[buf->read_pos++] << 56;
    return value;
}

int64_t buffer_len(TmlBuffer* buf) {
    return buf ? buf->len : 0;
}

int64_t buffer_capacity(TmlBuffer* buf) {
    return buf ? buf->capacity : 0;
}

int64_t buffer_remaining(TmlBuffer* buf) {
    if (!buf)
        return 0;
    return buf->len - buf->read_pos;
}

void buffer_clear(TmlBuffer* buf) {
    if (!buf)
        return;
    buf->len = 0;
    buf->read_pos = 0;
}

void buffer_reset_read(TmlBuffer* buf) {
    if (buf)
        buf->read_pos = 0;
}
