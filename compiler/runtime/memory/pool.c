/**
 * @file pool.c
 * @brief TML Runtime - Object Pool Functions
 *
 * Implements object pooling for efficient memory allocation of frequently
 * created/destroyed objects. Supports both global pools and thread-local pools.
 *
 * ## Components
 *
 * - **Global Pools**: pool_acquire, pool_release (declared in runtime.cpp)
 * - **Thread-Local Pools**: tls_pool_acquire, tls_pool_release (declared in runtime.cpp)
 * - **Internal**: pool_init, pool_destroy (static, used by TLS pool management)
 *
 * ## Usage
 *
 * Object pools are used by `@pool` decorated classes in TML to reduce
 * allocation overhead for frequently allocated objects.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define TML_THREAD_LOCAL __declspec(thread)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#define TML_THREAD_LOCAL __thread
#endif

// ============================================================================
// Object Pool Functions (for @pool classes)
// ============================================================================

/**
 * @brief Pool structure for object pooling.
 *
 * Layout: { free_list_ptr, block_list_ptr, capacity, count }
 * Objects in the free list are linked via their first pointer-sized field.
 */
typedef struct TmlPool {
    void* free_list;  // Head of free object list
    void* block_list; // List of allocated memory blocks
    int64_t capacity; // Total pool capacity
    int64_t count;    // Current number of objects in pool
} TmlPool;

/** @brief Block header for pool memory management. */
typedef struct TmlPoolBlock {
    struct TmlPoolBlock* next; // Next block in list
    int64_t object_count;      // Number of objects in this block
    int64_t object_size;       // Size of each object
    // Objects follow immediately after this header
} TmlPoolBlock;

/**
 * @brief Initializes a pool with the given initial capacity.
 *
 * @param pool Pointer to the pool structure.
 * @param object_size Size of each object in bytes.
 * @param initial_capacity Initial number of objects to pre-allocate.
 */
// pool_init — internal only (not declared in runtime.cpp, used by tls_pool_get_or_create)
static void pool_init(TmlPool* pool, int64_t object_size, int64_t initial_capacity) {
    pool->free_list = NULL;
    pool->block_list = NULL;
    pool->capacity = 0;
    pool->count = 0;

    if (initial_capacity > 0 && object_size > 0) {
        // Allocate initial block
        size_t block_size = sizeof(TmlPoolBlock) + (size_t)(initial_capacity * object_size);
        TmlPoolBlock* block = (TmlPoolBlock*)malloc(block_size);
        if (block) {
            block->next = NULL;
            block->object_count = initial_capacity;
            block->object_size = object_size;
            pool->block_list = block;

            // Link all objects into free list
            char* obj_start = (char*)(block + 1);
            for (int64_t i = 0; i < initial_capacity; i++) {
                void** obj = (void**)(obj_start + i * object_size);
                *obj = pool->free_list;
                pool->free_list = obj;
            }
            pool->capacity = initial_capacity;
        }
    }
}

/**
 * @brief Acquires an object from the pool.
 *
 * If the pool is empty, allocates a new block of objects.
 *
 * @param pool Pointer to the pool structure.
 * @param object_size Size of each object in bytes.
 * @return Pointer to the acquired object (zeroed memory).
 */
TML_EXPORT void* pool_acquire(TmlPool* pool, int64_t object_size) {
    if (pool->free_list != NULL) {
        // Pop from free list
        void* obj = pool->free_list;
        pool->free_list = *(void**)obj;
        pool->count++;
        // Zero the memory for clean initialization
        memset(obj, 0, (size_t)object_size);
        return obj;
    }

    // Pool is empty - allocate new block
    // Double capacity or start with 16 objects
    int64_t new_count = pool->capacity > 0 ? pool->capacity : 16;
    size_t block_size = sizeof(TmlPoolBlock) + (size_t)(new_count * object_size);
    TmlPoolBlock* block = (TmlPoolBlock*)malloc(block_size);
    if (!block) {
        // Fallback to direct allocation on OOM
        void* obj = malloc((size_t)object_size);
        if (obj)
            memset(obj, 0, (size_t)object_size);
        return obj;
    }

    block->next = (TmlPoolBlock*)pool->block_list;
    block->object_count = new_count;
    block->object_size = object_size;
    pool->block_list = block;

    // Link all but first object into free list
    char* obj_start = (char*)(block + 1);
    for (int64_t i = 1; i < new_count; i++) {
        void** obj = (void**)(obj_start + i * object_size);
        *obj = pool->free_list;
        pool->free_list = obj;
    }
    pool->capacity += new_count;
    pool->count++;

    // Return first object (zeroed)
    memset(obj_start, 0, (size_t)object_size);
    return obj_start;
}

/**
 * @brief Releases an object back to the pool.
 *
 * @param pool Pointer to the pool structure.
 * @param obj Pointer to the object to release.
 */
TML_EXPORT void pool_release(TmlPool* pool, void* obj) {
    if (obj == NULL)
        return;

    // Push onto free list
    *(void**)obj = pool->free_list;
    pool->free_list = obj;
    pool->count--;
}

// pool_destroy — internal only (not declared in runtime.cpp)
__attribute__((unused))
static void pool_destroy(TmlPool* pool) {
    TmlPoolBlock* block = (TmlPoolBlock*)pool->block_list;
    while (block != NULL) {
        TmlPoolBlock* next = block->next;
        free(block);
        block = next;
    }
    pool->free_list = NULL;
    pool->block_list = NULL;
    pool->capacity = 0;
    pool->count = 0;
}

// pool_count — REMOVED (Phase 41, dead: no declare in runtime.cpp, 0 TML callers)
// pool_capacity — REMOVED (Phase 41, dead: no declare in runtime.cpp, 0 TML callers)

// ============================================================================
// Thread-Local Pool Functions (for @pool(thread_local: true) classes)
// ============================================================================

/** @brief Maximum number of thread-local pools per thread. */
#define TML_MAX_TLS_POOLS 64

/** @brief Thread-local pool registry entry. */
typedef struct TmlTlsPoolEntry {
    const char* class_name; // Class name for identification
    TmlPool pool;           // The actual pool
    int64_t object_size;    // Size of objects in this pool
    int32_t initialized;    // Whether pool is initialized
} TmlTlsPoolEntry;

/** @brief Thread-local pool registry (per thread). */
static TML_THREAD_LOCAL TmlTlsPoolEntry tls_pools[TML_MAX_TLS_POOLS];
static TML_THREAD_LOCAL int32_t tls_pool_count = 0;
static TML_THREAD_LOCAL int32_t tls_initialized = 0;

/**
 * @brief Initializes thread-local pool storage for current thread.
 */
static void tls_pools_init(void) {
    if (!tls_initialized) {
        memset(tls_pools, 0, sizeof(tls_pools));
        tls_pool_count = 0;
        tls_initialized = 1;
    }
}

/**
 * @brief Finds or creates a thread-local pool for a class.
 *
 * @param class_name The class name identifier.
 * @param object_size Size of objects for this class.
 * @return Pointer to the thread-local pool.
 */
static TmlPool* tls_pool_get_or_create(const char* class_name, int64_t object_size) {
    tls_pools_init();

    // Search for existing pool
    for (int32_t i = 0; i < tls_pool_count; i++) {
        if (tls_pools[i].class_name == class_name ||
            (tls_pools[i].class_name && class_name &&
             strcmp(tls_pools[i].class_name, class_name) == 0)) {
            return &tls_pools[i].pool;
        }
    }

    // Create new pool if space available
    if (tls_pool_count < TML_MAX_TLS_POOLS) {
        TmlTlsPoolEntry* entry = &tls_pools[tls_pool_count++];
        entry->class_name = class_name;
        entry->object_size = object_size;
        entry->initialized = 1;
        pool_init(&entry->pool, object_size, 16); // Initial capacity of 16
        return &entry->pool;
    }

    // Registry full - return NULL (caller should fallback to global pool)
    return NULL;
}

/**
 * @brief Acquires an object from a thread-local pool.
 *
 * @param class_name The class name identifier.
 * @param object_size Size of the object in bytes.
 * @return Pointer to the acquired object (zeroed memory).
 */
TML_EXPORT void* tls_pool_acquire(const char* class_name, int64_t object_size) {
    TmlPool* pool = tls_pool_get_or_create(class_name, object_size);
    if (pool) {
        return pool_acquire(pool, object_size);
    }
    // Fallback to direct allocation
    void* obj = malloc((size_t)object_size);
    if (obj)
        memset(obj, 0, (size_t)object_size);
    return obj;
}

/**
 * @brief Releases an object back to a thread-local pool.
 *
 * @param class_name The class name identifier.
 * @param obj Pointer to the object to release.
 * @param object_size Size of the object (for fallback).
 */
TML_EXPORT void tls_pool_release(const char* class_name, void* obj, int64_t object_size) {
    if (obj == NULL)
        return;

    TmlPool* pool = tls_pool_get_or_create(class_name, object_size);
    if (pool) {
        pool_release(pool, obj);
    } else {
        // Fallback - object was allocated outside pool
        free(obj);
    }
}

// tls_pools_cleanup — REMOVED (Phase 41, dead: no declare in runtime.cpp, 0 TML callers)
// tls_pool_stats — REMOVED (Phase 41, dead: no declare in runtime.cpp, 0 TML callers)
