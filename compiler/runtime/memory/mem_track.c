/**
 * @file mem_track.c
 * @brief TML Runtime - Memory Tracking Implementation
 *
 * Implements allocation tracking for memory leak detection. Uses a simple
 * hash table to map pointers to allocation records.
 */

#include "mem_track.h"

#include "../diagnostics/log.h"

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
    char current_test_name[TML_MEM_TRACK_CTX_LEN]; /**< Active test name */
    char current_test_file[TML_MEM_TRACK_CTX_LEN]; /**< Active test file */
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
            RT_WARN("memory", "Program exited with %d memory leak(s)", leaks);
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

    // Snapshot current test context into the allocation record
    if (g_track.current_test_name[0]) {
        strncpy(bucket->record.test_name, g_track.current_test_name, TML_MEM_TRACK_CTX_LEN - 1);
        bucket->record.test_name[TML_MEM_TRACK_CTX_LEN - 1] = '\0';
    } else {
        bucket->record.test_name[0] = '\0';
    }
    if (g_track.current_test_file[0]) {
        strncpy(bucket->record.test_file, g_track.current_test_file, TML_MEM_TRACK_CTX_LEN - 1);
        bucket->record.test_file[TML_MEM_TRACK_CTX_LEN - 1] = '\0';
    } else {
        bucket->record.test_file[0] = '\0';
    }

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
    char old_test_name[TML_MEM_TRACK_CTX_LEN] = {0};
    char old_test_file[TML_MEM_TRACK_CTX_LEN] = {0};

    while (bucket) {
        if (bucket->record.ptr == old_ptr) {
            old_size = bucket->record.size;
            tag = bucket->record.tag;
            // Preserve original test context
            strncpy(old_test_name, bucket->record.test_name, TML_MEM_TRACK_CTX_LEN - 1);
            strncpy(old_test_file, bucket->record.test_file, TML_MEM_TRACK_CTX_LEN - 1);

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
        // Preserve test context from original allocation
        strncpy(new_bucket->record.test_name, old_test_name, TML_MEM_TRACK_CTX_LEN - 1);
        new_bucket->record.test_name[TML_MEM_TRACK_CTX_LEN - 1] = '\0';
        strncpy(new_bucket->record.test_file, old_test_file, TML_MEM_TRACK_CTX_LEN - 1);
        new_bucket->record.test_file[TML_MEM_TRACK_CTX_LEN - 1] = '\0';
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
                if (bucket->record.test_name[0]) {
                    fprintf(out, "    Test:     %s\n", bucket->record.test_name);
                }
                if (bucket->record.test_file[0]) {
                    fprintf(out, "    File:     %s\n", bucket->record.test_file);
                }
                fprintf(out, "\n");
                shown++;
                bucket = bucket->next;
            }
        }

        if (leak_count > 50) {
            fprintf(out, "  ... and %d more leaks not shown\n\n", leak_count - 50);
        }

        // Print per-test summary (group leaks by test name)
        // Use a simple O(n^2) scan — leak counts are small (max 50 shown)
        {
#define MAX_TEST_GROUPS 64
            struct {
                const char* name;
                const char* file;
                int count;
                uint64_t bytes;
            } groups[MAX_TEST_GROUPS];
            int num_groups = 0;
            int unknown_count = 0;
            uint64_t unknown_bytes = 0;

            for (int i = 0; i < HASH_BUCKETS; i++) {
                AllocBucket* b = g_track.buckets[i];
                while (b) {
                    if (b->record.test_name[0]) {
                        // Find existing group
                        int found = 0;
                        for (int g = 0; g < num_groups; g++) {
                            if (strcmp(groups[g].name, b->record.test_name) == 0) {
                                groups[g].count++;
                                groups[g].bytes += b->record.size;
                                found = 1;
                                break;
                            }
                        }
                        if (!found && num_groups < MAX_TEST_GROUPS) {
                            groups[num_groups].name = b->record.test_name;
                            groups[num_groups].file = b->record.test_file;
                            groups[num_groups].count = 1;
                            groups[num_groups].bytes = b->record.size;
                            num_groups++;
                        }
                    } else {
                        unknown_count++;
                        unknown_bytes += b->record.size;
                    }
                    b = b->next;
                }
            }

            if (num_groups > 0 || unknown_count > 0) {
                fprintf(out, "\n  Leaks by test:\n");
                for (int g = 0; g < num_groups; g++) {
                    fprintf(out, "    %-30s %3d leak(s), %llu bytes  [%s]\n", groups[g].name,
                            groups[g].count, (unsigned long long)groups[g].bytes,
                            groups[g].file ? groups[g].file : "?");
                }
                if (unknown_count > 0) {
                    fprintf(out, "    %-30s %3d leak(s), %llu bytes\n", "(no test context)",
                            unknown_count, (unsigned long long)unknown_bytes);
                }
                fprintf(out, "\n");
            }
#undef MAX_TEST_GROUPS
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
        RT_WARN("memory", "Tracking not initialized");
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

TML_MEM_EXPORT void tml_mem_track_set_test_context(const char* test_name, const char* test_file) {
    if (!g_track.initialized) {
        tml_mem_track_init();
    }

    TML_MUTEX_LOCK(g_track.mutex);

    if (test_name) {
        strncpy(g_track.current_test_name, test_name, TML_MEM_TRACK_CTX_LEN - 1);
        g_track.current_test_name[TML_MEM_TRACK_CTX_LEN - 1] = '\0';
    } else {
        g_track.current_test_name[0] = '\0';
    }

    if (test_file) {
        strncpy(g_track.current_test_file, test_file, TML_MEM_TRACK_CTX_LEN - 1);
        g_track.current_test_file[TML_MEM_TRACK_CTX_LEN - 1] = '\0';
    } else {
        g_track.current_test_file[0] = '\0';
    }

    TML_MUTEX_UNLOCK(g_track.mutex);
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
