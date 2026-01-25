// TML Standard Library - Collections Runtime
// Implements: List, HashMap, Buffer

#ifndef STD_COLLECTIONS_H
#define STD_COLLECTIONS_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// List - Dynamic array
// ============================================================================

typedef struct {
    int64_t* data;
    int64_t len;
    int64_t capacity;
} TmlList;

TmlList* list_create(int64_t initial_capacity);
void list_destroy(TmlList* list);
void list_push(TmlList* list, int64_t value);
int64_t list_pop(TmlList* list);
int64_t list_get(TmlList* list, int64_t index);
void list_set(TmlList* list, int64_t index, int64_t value);
int64_t list_len(TmlList* list);
int64_t list_capacity(TmlList* list);
void list_clear(TmlList* list);
int32_t list_is_empty(TmlList* list);
void list_resize(TmlList* list, int64_t new_len);
void list_reserve(TmlList* list, int64_t min_capacity);
void list_shrink_to_fit(TmlList* list);
int64_t list_remove(TmlList* list, int64_t index);
void list_insert(TmlList* list, int64_t index, int64_t value);
void list_reverse(TmlList* list);

// ============================================================================
// HashMap - Key-value store (i64 -> i64)
// ============================================================================

typedef struct HashEntry {
    int64_t key;
    int64_t value;
    bool occupied;
    bool deleted;
} HashEntry;

typedef struct {
    HashEntry* entries;
    int64_t capacity;
    int64_t len;
} TmlHashMap;

TmlHashMap* hashmap_create(int64_t initial_capacity);
void hashmap_destroy(TmlHashMap* map);
void hashmap_set(TmlHashMap* map, int64_t key, int64_t value);
int64_t hashmap_get(TmlHashMap* map, int64_t key);
bool hashmap_has(TmlHashMap* map, int64_t key);
bool hashmap_remove(TmlHashMap* map, int64_t key);
int64_t hashmap_len(TmlHashMap* map);
void hashmap_clear(TmlHashMap* map);

// ============================================================================
// HashMap Iterator
// ============================================================================

typedef struct {
    TmlHashMap* map;
    int64_t index;     // Current index in entries array
    int64_t remaining; // Remaining entries to iterate
} TmlHashMapIter;

TmlHashMapIter* hashmap_iter_create(TmlHashMap* map);
void hashmap_iter_destroy(TmlHashMapIter* iter);
bool hashmap_iter_has_next(TmlHashMapIter* iter);
void hashmap_iter_next(TmlHashMapIter* iter);
int64_t hashmap_iter_key(TmlHashMapIter* iter);
int64_t hashmap_iter_value(TmlHashMapIter* iter);

// ============================================================================
// Buffer - Byte buffer for binary data
// ============================================================================

typedef struct {
    uint8_t* data;
    int64_t len;
    int64_t capacity;
    int64_t read_pos;
} TmlBuffer;

TmlBuffer* buffer_create(int64_t initial_capacity);
void buffer_destroy(TmlBuffer* buf);
void buffer_write_byte(TmlBuffer* buf, int32_t byte);
void buffer_write_i32(TmlBuffer* buf, int32_t value);
void buffer_write_i64(TmlBuffer* buf, int64_t value);
int32_t buffer_read_byte(TmlBuffer* buf);
int32_t buffer_read_i32(TmlBuffer* buf);
int64_t buffer_read_i64(TmlBuffer* buf);
int64_t buffer_len(TmlBuffer* buf);
int64_t buffer_capacity(TmlBuffer* buf);
int64_t buffer_remaining(TmlBuffer* buf);
void buffer_clear(TmlBuffer* buf);
void buffer_reset_read(TmlBuffer* buf);

#endif // STD_COLLECTIONS_H
