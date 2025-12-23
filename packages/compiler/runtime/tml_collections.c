// TML Runtime - Collections
// List, HashMap, Buffer, String utilities

#include "tml_runtime.h"

// ============ LIST (Dynamic Array) ============

struct List {
    int64_t* data;
    int64_t len;
    int64_t cap;
};

List* tml_list_new(void) {
    List* list = (List*)malloc(sizeof(List));
    list->cap = 8;
    list->len = 0;
    list->data = (int64_t*)malloc(sizeof(int64_t) * (size_t)list->cap);
    return list;
}

static void list_grow(List* list) {
    list->cap *= 2;
    list->data = (int64_t*)realloc(list->data, sizeof(int64_t) * (size_t)list->cap);
}

void tml_list_push(List* list, int64_t value) {
    if (list->len >= list->cap) {
        list_grow(list);
    }
    list->data[list->len++] = value;
}

int64_t tml_list_pop(List* list) {
    if (list->len == 0) return 0;
    return list->data[--list->len];
}

int64_t tml_list_get(List* list, int64_t index) {
    if (index < 0 || index >= list->len) return 0;
    return list->data[index];
}

void tml_list_set(List* list, int64_t index, int64_t value) {
    if (index >= 0 && index < list->len) {
        list->data[index] = value;
    }
}

int64_t tml_list_len(List* list) {
    return list->len;
}

int tml_list_is_empty(List* list) {
    return list->len == 0;
}

void tml_list_clear(List* list) {
    list->len = 0;
}

void tml_list_free(List* list) {
    free(list->data);
    free(list);
}

// Aliases for test compatibility
List* tml_list_create(int64_t capacity) {
    List* list = (List*)malloc(sizeof(List));
    list->cap = capacity > 0 ? capacity : 8;
    list->len = 0;
    list->data = (int64_t*)malloc(sizeof(int64_t) * (size_t)list->cap);
    return list;
}

int64_t tml_list_capacity(List* list) {
    return list->cap;
}

void tml_list_destroy(List* list) {
    tml_list_free(list);
}

// ============ HASHMAP ============

#define HASHMAP_SIZE 256

typedef struct HashEntry {
    int64_t key;
    int64_t value;
    int occupied;
} HashEntry;

struct HashMap {
    HashEntry* buckets;
    int64_t count;
};

HashMap* tml_hashmap_new(void) {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    map->buckets = (HashEntry*)calloc(HASHMAP_SIZE, sizeof(HashEntry));
    map->count = 0;
    return map;
}

static uint64_t hash_i64(int64_t key) {
    uint64_t x = (uint64_t)key;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

void tml_hashmap_insert(HashMap* map, int64_t key, int64_t value) {
    uint64_t idx = hash_i64(key) % HASHMAP_SIZE;
    for (int i = 0; i < HASHMAP_SIZE; i++) {
        uint64_t probe = (idx + (uint64_t)i) % HASHMAP_SIZE;
        if (!map->buckets[probe].occupied || map->buckets[probe].key == key) {
            if (!map->buckets[probe].occupied) map->count++;
            map->buckets[probe].key = key;
            map->buckets[probe].value = value;
            map->buckets[probe].occupied = 1;
            return;
        }
    }
}

int64_t tml_hashmap_get(HashMap* map, int64_t key) {
    uint64_t idx = hash_i64(key) % HASHMAP_SIZE;
    for (int i = 0; i < HASHMAP_SIZE; i++) {
        uint64_t probe = (idx + (uint64_t)i) % HASHMAP_SIZE;
        if (!map->buckets[probe].occupied) return 0;
        if (map->buckets[probe].key == key) {
            return map->buckets[probe].value;
        }
    }
    return 0;
}

int tml_hashmap_contains(HashMap* map, int64_t key) {
    uint64_t idx = hash_i64(key) % HASHMAP_SIZE;
    for (int i = 0; i < HASHMAP_SIZE; i++) {
        uint64_t probe = (idx + (uint64_t)i) % HASHMAP_SIZE;
        if (!map->buckets[probe].occupied) return 0;
        if (map->buckets[probe].key == key) return 1;
    }
    return 0;
}

static void tml_hashmap_remove_internal(HashMap* map, int64_t key) {
    uint64_t idx = hash_i64(key) % HASHMAP_SIZE;
    for (int i = 0; i < HASHMAP_SIZE; i++) {
        uint64_t probe = (idx + (uint64_t)i) % HASHMAP_SIZE;
        if (!map->buckets[probe].occupied) return;
        if (map->buckets[probe].key == key) {
            map->buckets[probe].occupied = 0;
            map->count--;
            return;
        }
    }
}

int64_t tml_hashmap_len(HashMap* map) {
    return map->count;
}

void tml_hashmap_clear(HashMap* map) {
    memset(map->buckets, 0, sizeof(HashEntry) * HASHMAP_SIZE);
    map->count = 0;
}

void tml_hashmap_free(HashMap* map) {
    free(map->buckets);
    free(map);
}

// ============ BUFFER ============

struct Buffer {
    uint8_t* data;
    int64_t len;
    int64_t cap;
    int64_t pos;
};

Buffer* tml_buffer_new(int64_t capacity) {
    Buffer* buf = (Buffer*)malloc(sizeof(Buffer));
    buf->cap = capacity > 0 ? capacity : 64;
    buf->len = 0;
    buf->pos = 0;
    buf->data = (uint8_t*)malloc((size_t)buf->cap);
    return buf;
}

static void buffer_grow(Buffer* buf, int64_t needed) {
    while (buf->cap < buf->len + needed) {
        buf->cap *= 2;
    }
    buf->data = (uint8_t*)realloc(buf->data, (size_t)buf->cap);
}

void tml_buffer_write_byte(Buffer* buf, int32_t byte) {
    if (buf->len >= buf->cap) buffer_grow(buf, 1);
    buf->data[buf->len++] = (uint8_t)byte;
}

void tml_buffer_write_i32(Buffer* buf, int32_t value) {
    if (buf->len + 4 > buf->cap) buffer_grow(buf, 4);
    memcpy(buf->data + buf->len, &value, 4);
    buf->len += 4;
}

void tml_buffer_write_i64(Buffer* buf, int64_t value) {
    if (buf->len + 8 > buf->cap) buffer_grow(buf, 8);
    memcpy(buf->data + buf->len, &value, 8);
    buf->len += 8;
}

int32_t tml_buffer_read_byte(Buffer* buf) {
    if (buf->pos >= buf->len) return -1;
    return buf->data[buf->pos++];
}

int32_t tml_buffer_read_i32(Buffer* buf) {
    if (buf->pos + 4 > buf->len) return 0;
    int32_t value;
    memcpy(&value, buf->data + buf->pos, 4);
    buf->pos += 4;
    return value;
}

int64_t tml_buffer_read_i64(Buffer* buf) {
    if (buf->pos + 8 > buf->len) return 0;
    int64_t value;
    memcpy(&value, buf->data + buf->pos, 8);
    buf->pos += 8;
    return value;
}

int64_t tml_buffer_len(Buffer* buf) {
    return buf->len;
}

void tml_buffer_reset(Buffer* buf) {
    buf->len = 0;
    buf->pos = 0;
}

void tml_buffer_free(Buffer* buf) {
    free(buf->data);
    free(buf);
}

// ============ STRING UTILITIES ============

static char str_buffer[4096];

const char* tml_str_concat(const char* a, const char* b) {
    snprintf(str_buffer, sizeof(str_buffer), "%s%s", a, b);
    return str_buffer;
}

int64_t tml_str_len(const char* s) {
    return (int64_t)strlen(s);
}

int tml_str_eq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

// ============ STRING HASHMAP ============

#define STRMAP_SIZE 256

typedef struct StrMapEntry {
    char* key;
    char* value;
    int occupied;
} StrMapEntry;

struct StrMap {
    StrMapEntry* buckets;
    int64_t count;
};

static uint64_t hash_str(const char* str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + (uint64_t)c;
    }
    return hash;
}

StrMap* tml_strmap_new(void) {
    StrMap* map = (StrMap*)malloc(sizeof(StrMap));
    map->buckets = (StrMapEntry*)calloc(STRMAP_SIZE, sizeof(StrMapEntry));
    map->count = 0;
    return map;
}

void tml_strmap_insert(StrMap* map, const char* key, const char* value) {
    uint64_t idx = hash_str(key) % STRMAP_SIZE;
    for (int i = 0; i < STRMAP_SIZE; i++) {
        uint64_t probe = (idx + (uint64_t)i) % STRMAP_SIZE;
        if (!map->buckets[probe].occupied) {
            map->buckets[probe].key = _strdup(key);
            map->buckets[probe].value = _strdup(value);
            map->buckets[probe].occupied = 1;
            map->count++;
            return;
        }
        if (strcmp(map->buckets[probe].key, key) == 0) {
            free(map->buckets[probe].value);
            map->buckets[probe].value = _strdup(value);
            return;
        }
    }
}

const char* tml_strmap_get(StrMap* map, const char* key) {
    uint64_t idx = hash_str(key) % STRMAP_SIZE;
    for (int i = 0; i < STRMAP_SIZE; i++) {
        uint64_t probe = (idx + (uint64_t)i) % STRMAP_SIZE;
        if (!map->buckets[probe].occupied) return "";
        if (strcmp(map->buckets[probe].key, key) == 0) {
            return map->buckets[probe].value;
        }
    }
    return "";
}

int tml_strmap_contains(StrMap* map, const char* key) {
    uint64_t idx = hash_str(key) % STRMAP_SIZE;
    for (int i = 0; i < STRMAP_SIZE; i++) {
        uint64_t probe = (idx + (uint64_t)i) % STRMAP_SIZE;
        if (!map->buckets[probe].occupied) return 0;
        if (strcmp(map->buckets[probe].key, key) == 0) return 1;
    }
    return 0;
}

void tml_strmap_remove(StrMap* map, const char* key) {
    uint64_t idx = hash_str(key) % STRMAP_SIZE;
    for (int i = 0; i < STRMAP_SIZE; i++) {
        uint64_t probe = (idx + (uint64_t)i) % STRMAP_SIZE;
        if (!map->buckets[probe].occupied) return;
        if (strcmp(map->buckets[probe].key, key) == 0) {
            free(map->buckets[probe].key);
            free(map->buckets[probe].value);
            map->buckets[probe].occupied = 0;
            map->count--;
            return;
        }
    }
}

int64_t tml_strmap_len(StrMap* map) {
    return map->count;
}

void tml_strmap_free(StrMap* map) {
    for (int i = 0; i < STRMAP_SIZE; i++) {
        if (map->buckets[i].occupied) {
            free(map->buckets[i].key);
            free(map->buckets[i].value);
        }
    }
    free(map->buckets);
    free(map);
}

// ============ COLLECTION ALIASES FOR TEST COMPATIBILITY ============

// HashMap aliases
HashMap* tml_hashmap_create(int64_t capacity) {
    return tml_hashmap_new();
}

void tml_hashmap_set(HashMap* map, int64_t key, int64_t value) {
    tml_hashmap_insert(map, key, value);
}

bool tml_hashmap_has(HashMap* map, int64_t key) {
    return tml_hashmap_contains(map, key) != 0;
}

bool tml_hashmap_remove_key(HashMap* map, int64_t key) {
    int had_key = tml_hashmap_contains(map, key);
    if (had_key) {
        tml_hashmap_remove_internal(map, key);
    }
    return had_key != 0;
}

void tml_hashmap_destroy(HashMap* map) {
    tml_hashmap_free(map);
}

// Buffer aliases
Buffer* tml_buffer_create(int64_t capacity) {
    return tml_buffer_new(capacity);
}

int64_t tml_buffer_capacity(Buffer* buf) {
    return buf->cap;
}

int64_t tml_buffer_remaining(Buffer* buf) {
    return buf->cap - buf->len;
}

void tml_buffer_reset_read(Buffer* buf) {
    buf->pos = 0;
}

void tml_buffer_clear(Buffer* buf) {
    buf->len = 0;
    buf->pos = 0;
}

void tml_buffer_destroy(Buffer* buf) {
    tml_buffer_free(buf);
}

// HashMap remove that returns bool
bool tml_hashmap_remove(HashMap* map, int64_t key) {
    return tml_hashmap_remove_key(map, key);
}
