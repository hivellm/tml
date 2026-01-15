/**
 * @file mem_track.c
 * @brief TML Runtime - Memory Tracking Implementation
 *
 * Implements allocation tracking for memory leak detection. Uses a simple
 * hash table to map pointers to allocation records.
 */

#include "mem_track.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define TML_MUTEX CRITICAL_SECTION
#define TML_MUTEX_INIT(m) InitializeCriticalSection(&(m))
#define TML_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#define TML_MUTEX_LOCK(m) EnterCriticalSection(&(m))
#define TML_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))

static uint64_t get_timestamp_ns(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
}
#else
#include <pthread.h>
#include <time.h>
#define TML_MUTEX pthread_mutex_t
#define TML_MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
#define TML_MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#define TML_MUTEX_LOCK(m) pthread_mutex_lock(&(m))
#define TML_MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

// ============================================================================
// Hash Table for Allocation Tracking
// ============================================================================

/** Hash table bucket */
typedef struct AllocBucket {
    TmlAllocRecord record;
    struct AllocBucket* next;
} AllocBucket;

/** Number of hash buckets (power of 2 for fast modulo) */
#define HASH_BUCKETS 4096
#define HASH_MASK (HASH_BUCKETS - 1)

/** Global tracking state */
static struct {
    AllocBucket* buckets[HASH_BUCKETS];
    TmlMemStats stats;
    TML_MUTEX mutex;
    FILE* output;
    int32_t initialized;
    int32_t check_at_exit;
    uint64_t next_alloc_id;
} g_track = {0};

// ============================================================================
// Hash Function
// ============================================================================

static inline uint32_t hash_ptr(void* ptr) {
    // Simple pointer hash using multiplication
    uintptr_t val = (uintptr_t)ptr;
    val = (val ^ (val >> 16)) * 0x45d9f3b;
    val = (val ^ (val >> 16)) * 0x45d9f3b;
    return (uint32_t)(val & HASH_MASK);
}

// ============================================================================
// Initialization
// ============================================================================

static void tml_mem_track_atexit(void) {
    if (g_track.check_at_exit) {
        int32_t leaks = tml_mem_check_leaks();
        if (leaks > 0) {
            fprintf(g_track.output ? g_track.output : stderr,
                    "\n[TML Memory] Program exited with %d memory leak(s)\n", leaks);
        }
    }
    tml_mem_track_shutdown();
}

void tml_mem_track_init(void) {
    if (g_track.initialized) {
        return;
    }

    TML_MUTEX_INIT(g_track.mutex);

    for (int i = 0; i < HASH_BUCKETS; i++) {
        g_track.buckets[i] = NULL;
    }

    memset(&g_track.stats, 0, sizeof(g_track.stats));
    g_track.output = stderr;
    g_track.check_at_exit = 1; // Enable by default
    g_track.next_alloc_id = 1;
    g_track.initialized = 1;

    atexit(tml_mem_track_atexit);
}

void tml_mem_track_shutdown(void) {
    if (!g_track.initialized) {
        return;
    }

    TML_MUTEX_LOCK(g_track.mutex);

    // Free all buckets
    for (int i = 0; i < HASH_BUCKETS; i++) {
        AllocBucket* bucket = g_track.buckets[i];
        while (bucket) {
            AllocBucket* next = bucket->next;
            free(bucket);
            bucket = next;
        }
        g_track.buckets[i] = NULL;
    }

    TML_MUTEX_UNLOCK(g_track.mutex);
    TML_MUTEX_DESTROY(g_track.mutex);

    g_track.initialized = 0;
}

// ============================================================================
// Allocation Tracking
// ============================================================================

void tml_mem_track_alloc(void* ptr, size_t size, const char* tag) {
    if (!ptr)
        return;

    if (!g_track.initialized) {
        tml_mem_track_init();
    }

    TML_MUTEX_LOCK(g_track.mutex);

    uint32_t hash = hash_ptr(ptr);

    // Create new bucket
    AllocBucket* bucket = (AllocBucket*)malloc(sizeof(AllocBucket));
    if (!bucket) {
        TML_MUTEX_UNLOCK(g_track.mutex);
        return;
    }

    bucket->record.ptr = ptr;
    bucket->record.size = size;
    bucket->record.alloc_id = g_track.next_alloc_id++;
    bucket->record.timestamp_ns = get_timestamp_ns();
    bucket->record.tag = tag;
    bucket->next = g_track.buckets[hash];
    g_track.buckets[hash] = bucket;

    // Update stats
    g_track.stats.total_allocations++;
    g_track.stats.current_allocations++;
    g_track.stats.total_bytes_allocated += size;
    g_track.stats.current_bytes += size;

    if (g_track.stats.current_allocations > g_track.stats.peak_allocations) {
        g_track.stats.peak_allocations = g_track.stats.current_allocations;
    }
    if (g_track.stats.current_bytes > g_track.stats.peak_bytes) {
        g_track.stats.peak_bytes = g_track.stats.current_bytes;
    }

    TML_MUTEX_UNLOCK(g_track.mutex);
}

int32_t tml_mem_track_free(void* ptr) {
    if (!ptr || !g_track.initialized)
        return 0;

    TML_MUTEX_LOCK(g_track.mutex);

    uint32_t hash = hash_ptr(ptr);
    AllocBucket** prev = &g_track.buckets[hash];
    AllocBucket* bucket = g_track.buckets[hash];

    while (bucket) {
        if (bucket->record.ptr == ptr) {
            // Found it - remove from list
            *prev = bucket->next;

            // Update stats
            g_track.stats.total_deallocations++;
            g_track.stats.current_allocations--;
            g_track.stats.current_bytes -= bucket->record.size;

            free(bucket);
            TML_MUTEX_UNLOCK(g_track.mutex);
            return 1;
        }
        prev = &bucket->next;
        bucket = bucket->next;
    }

    // Pointer not found - could be double-free or external allocation
    g_track.stats.invalid_frees++;
    TML_MUTEX_UNLOCK(g_track.mutex);
    return 0;
}

void tml_mem_track_realloc(void* old_ptr, void* new_ptr, size_t new_size) {
    if (!g_track.initialized) {
        tml_mem_track_init();
    }

    if (old_ptr == NULL) {
        // This is effectively a new allocation
        tml_mem_track_alloc(new_ptr, new_size, "realloc");
        return;
    }

    if (new_ptr == NULL) {
        // Realloc failed, old pointer is still valid
        return;
    }

    TML_MUTEX_LOCK(g_track.mutex);

    // Find and update old allocation
    uint32_t old_hash = hash_ptr(old_ptr);
    AllocBucket** prev = &g_track.buckets[old_hash];
    AllocBucket* bucket = g_track.buckets[old_hash];
    size_t old_size = 0;
    const char* tag = "realloc";

    while (bucket) {
        if (bucket->record.ptr == old_ptr) {
            old_size = bucket->record.size;
            tag = bucket->record.tag;

            // Remove from old location
            *prev = bucket->next;
            free(bucket);
            break;
        }
        prev = &bucket->next;
        bucket = bucket->next;
    }

    // Add at new location
    uint32_t new_hash = hash_ptr(new_ptr);
    AllocBucket* new_bucket = (AllocBucket*)malloc(sizeof(AllocBucket));
    if (new_bucket) {
        new_bucket->record.ptr = new_ptr;
        new_bucket->record.size = new_size;
        new_bucket->record.alloc_id = g_track.next_alloc_id++;
        new_bucket->record.timestamp_ns = get_timestamp_ns();
        new_bucket->record.tag = tag;
        new_bucket->next = g_track.buckets[new_hash];
        g_track.buckets[new_hash] = new_bucket;
    }

    // Update stats
    g_track.stats.current_bytes = g_track.stats.current_bytes - old_size + new_size;
    if (g_track.stats.current_bytes > g_track.stats.peak_bytes) {
        g_track.stats.peak_bytes = g_track.stats.current_bytes;
    }

    TML_MUTEX_UNLOCK(g_track.mutex);
}

// ============================================================================
// Leak Checking
// ============================================================================

int32_t tml_mem_check_leaks(void) {
    if (!g_track.initialized)
        return 0;

    TML_MUTEX_LOCK(g_track.mutex);

    FILE* out = g_track.output ? g_track.output : stderr;
    int32_t leak_count = 0;
    uint64_t leak_bytes = 0;

    // Collect all leaks
    for (int i = 0; i < HASH_BUCKETS; i++) {
        AllocBucket* bucket = g_track.buckets[i];
        while (bucket) {
            leak_count++;
            leak_bytes += bucket->record.size;
            bucket = bucket->next;
        }
    }

    if (leak_count > 0) {
        fprintf(out, "\n");
        fprintf(
            out,
            "================================================================================\n");
        fprintf(out, "                         TML MEMORY LEAK REPORT\n");
        fprintf(
            out,
            "================================================================================\n");
        fprintf(out, "\n");
        fprintf(out, "Detected %d unfreed allocation(s) totaling %llu bytes:\n\n", leak_count,
                (unsigned long long)leak_bytes);

        int shown = 0;
        for (int i = 0; i < HASH_BUCKETS && shown < 50; i++) {
            AllocBucket* bucket = g_track.buckets[i];
            while (bucket && shown < 50) {
                fprintf(out, "  Leak #%d:\n", shown + 1);
                fprintf(out, "    Address:  %p\n", bucket->record.ptr);
                fprintf(out, "    Size:     %llu bytes\n", (unsigned long long)bucket->record.size);
                fprintf(out, "    Alloc ID: %llu\n", (unsigned long long)bucket->record.alloc_id);
                if (bucket->record.tag) {
                    fprintf(out, "    Tag:      %s\n", bucket->record.tag);
                }
                fprintf(out, "\n");
                shown++;
                bucket = bucket->next;
            }
        }

        if (leak_count > 50) {
            fprintf(out, "  ... and %d more leaks not shown\n\n", leak_count - 50);
        }

        fprintf(
            out,
            "================================================================================\n");
        fprintf(out, "Summary: %d leak(s), %llu bytes lost\n", leak_count,
                (unsigned long long)leak_bytes);
        fprintf(
            out,
            "================================================================================\n");
    }

    TML_MUTEX_UNLOCK(g_track.mutex);
    return leak_count;
}

// ============================================================================
// Statistics
// ============================================================================

void tml_mem_get_stats(TmlMemStats* stats) {
    if (!stats)
        return;

    if (!g_track.initialized) {
        memset(stats, 0, sizeof(TmlMemStats));
        return;
    }

    TML_MUTEX_LOCK(g_track.mutex);
    *stats = g_track.stats;
    TML_MUTEX_UNLOCK(g_track.mutex);
}

void tml_mem_print_stats(void) {
    if (!g_track.initialized) {
        fprintf(stderr, "[TML Memory] Tracking not initialized\n");
        return;
    }

    TML_MUTEX_LOCK(g_track.mutex);

    FILE* out = g_track.output ? g_track.output : stderr;

    fprintf(out, "\n");
    fprintf(out,
            "================================================================================\n");
    fprintf(out, "                         TML MEMORY STATISTICS\n");
    fprintf(out,
            "================================================================================\n");
    fprintf(out, "\n");
    fprintf(out, "  Total allocations:      %llu\n",
            (unsigned long long)g_track.stats.total_allocations);
    fprintf(out, "  Total deallocations:    %llu\n",
            (unsigned long long)g_track.stats.total_deallocations);
    fprintf(out, "  Current allocations:    %llu\n",
            (unsigned long long)g_track.stats.current_allocations);
    fprintf(out, "  Peak allocations:       %llu\n",
            (unsigned long long)g_track.stats.peak_allocations);
    fprintf(out, "\n");
    fprintf(out, "  Total bytes allocated:  %llu\n",
            (unsigned long long)g_track.stats.total_bytes_allocated);
    fprintf(out, "  Current bytes in use:   %llu\n",
            (unsigned long long)g_track.stats.current_bytes);
    fprintf(out, "  Peak bytes in use:      %llu\n", (unsigned long long)g_track.stats.peak_bytes);
    fprintf(out, "\n");
    fprintf(out, "  Invalid frees:          %llu\n",
            (unsigned long long)g_track.stats.invalid_frees);
    fprintf(out, "\n");
    fprintf(out,
            "================================================================================\n");

    TML_MUTEX_UNLOCK(g_track.mutex);
}

void tml_mem_set_check_at_exit(int32_t enable) {
    g_track.check_at_exit = enable;
}

void tml_mem_set_output(void* fp) {
    g_track.output = (FILE*)fp;
}

// ============================================================================
// Tracked Allocation Functions (wrappers)
// ============================================================================

void* tml_mem_alloc_tracked(int64_t size, const char* tag) {
    void* ptr = malloc((size_t)size);
    tml_mem_track_alloc(ptr, (size_t)size, tag);
    return ptr;
}

void* tml_mem_alloc_zeroed_tracked(int64_t size, const char* tag) {
    void* ptr = calloc(1, (size_t)size);
    tml_mem_track_alloc(ptr, (size_t)size, tag);
    return ptr;
}

void* tml_mem_realloc_tracked(void* ptr, int64_t new_size, const char* tag) {
    void* new_ptr = realloc(ptr, (size_t)new_size);
    tml_mem_track_realloc(ptr, new_ptr, (size_t)new_size);
    (void)tag; // Tag preserved from original allocation
    return new_ptr;
}

void tml_mem_free_tracked(void* ptr) {
    if (ptr) {
        tml_mem_track_free(ptr);
        free(ptr);
    }
}
