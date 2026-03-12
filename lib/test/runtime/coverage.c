// TML Code Coverage Runtime
// Minimal coverage instrumentation: tracks function calls via lock-free hash table.
// Report generation is handled by the C++ coordinator (testing_coverage.cpp).
// Note: _CRT_SECURE_NO_WARNINGS is defined via compile flags for all C runtime files

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Export functions from DLLs
#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Atomic operations for Windows
#define ATOMIC_INCREMENT(ptr) InterlockedIncrement((volatile LONG*)(ptr))
#define ATOMIC_LOAD(ptr) ((int32_t)InterlockedCompareExchange((volatile LONG*)(ptr), 0, 0))
#define ATOMIC_STORE(ptr, val) InterlockedExchange((volatile LONG*)(ptr), (val))
#define ATOMIC_CAS(ptr, expected, desired)                                                         \
    (InterlockedCompareExchange((volatile LONG*)(ptr), (desired), (expected)) == (expected))
// Memory barrier to ensure writes are visible
#define MEMORY_BARRIER() MemoryBarrier()
#else
#define TML_EXPORT __attribute__((visibility("default")))

// Atomic operations for GCC/Clang
#define ATOMIC_INCREMENT(ptr) __sync_add_and_fetch((ptr), 1)
#define ATOMIC_LOAD(ptr) ((int32_t)__sync_add_and_fetch((ptr), 0))
#define ATOMIC_STORE(ptr, val) __sync_lock_test_and_set((ptr), (val))
#define ATOMIC_CAS(ptr, expected, desired)                                                         \
    __sync_bool_compare_and_swap((ptr), (expected), (desired))
// Memory barrier
#define MEMORY_BARRIER() __sync_synchronize()
#endif

// Hash table parameters
#define HASH_TABLE_SIZE 32771 // Prime number, ~50% load factor for 16000+ functions
#define MAX_NAME_LEN 192      // Function names rarely exceed this

// FNV-1a hash function for strings
static uint32_t hash_string(const char* str) {
    if (!str)
        return 0;
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

// Hash table entry for functions
// State machine: occupied=0 -> occupied=1 (initializing) -> occupied=2 (ready)
typedef struct {
    volatile int32_t hit_count; // Atomic counter
    volatile int32_t occupied;  // 0=empty, 1=initializing, 2=ready
    char name[MAX_NAME_LEN];
} FuncEntry;

// Global hash table - statically allocated for lock-free access
static FuncEntry g_func_table[HASH_TABLE_SIZE];
static volatile int32_t g_func_count = 0;

// Lock-free function lookup/insert using open addressing
// Uses 3-state machine: 0=empty, 1=initializing, 2=ready
// Returns pointer to the entry's hit_count for atomic increment
static volatile int32_t* find_or_create_func_lockfree(const char* name) {
    if (!name)
        return NULL;

    uint32_t hash = hash_string(name);
    uint32_t idx = hash % HASH_TABLE_SIZE;
    uint32_t start_idx = idx;

    // Linear probing
    do {
        FuncEntry* entry = &g_func_table[idx];
        int32_t state = ATOMIC_LOAD(&entry->occupied);

        if (state == 2) {
            // Slot is ready, check if it's our key
            if (strcmp(entry->name, name) == 0) {
                return &entry->hit_count;
            }
            // Different key, continue probing
        } else if (state == 1) {
            // Slot is being initialized, spin-wait then check
            while ((state = ATOMIC_LOAD(&entry->occupied)) == 1) {
                // Spin wait - entry is being written
            }
            if (state == 2 && strcmp(entry->name, name) == 0) {
                return &entry->hit_count;
            }
            // Different key or entry abandoned, continue probing
        } else {
            // Empty slot (state == 0), try to claim it
            if (ATOMIC_CAS(&entry->occupied, 0, 1)) {
                // We claimed the slot, initialize it
                strncpy(entry->name, name, MAX_NAME_LEN - 1);
                entry->name[MAX_NAME_LEN - 1] = '\0';
                entry->hit_count = 0;
                MEMORY_BARRIER();                  // Ensure name is written before marking ready
                ATOMIC_STORE(&entry->occupied, 2); // Mark as ready
                ATOMIC_INCREMENT(&g_func_count);
                return &entry->hit_count;
            }
            // Someone else claimed it, re-check state
            state = ATOMIC_LOAD(&entry->occupied);
            if (state == 1) {
                // Wait for initialization
                while ((state = ATOMIC_LOAD(&entry->occupied)) == 1) {
                    // Spin wait
                }
            }
            if (state == 2 && strcmp(entry->name, name) == 0) {
                return &entry->hit_count;
            }
            // Different key, continue probing
        }

        idx = (idx + 1) % HASH_TABLE_SIZE;
    } while (idx != start_idx);

    // Table is full (shouldn't happen with proper sizing)
    return NULL;
}

// ============ Public API ============

// Lock-free function coverage - the most frequently called function
// Called by codegen-instrumented IR for every function entry
TML_EXPORT void tml_cover_func(const char* name) {
    volatile int32_t* hit_count = find_or_create_func_lockfree(name);
    if (hit_count) {
        ATOMIC_INCREMENT(hit_count);
    }
}

// Get total function count (used by coordinator via DLL export)
TML_EXPORT int32_t tml_get_func_count(void) {
    return ATOMIC_LOAD(&g_func_count);
}

// Get function name by index (iterates through hash table)
// Note: index is NOT stable - use for iteration only
TML_EXPORT const char* tml_get_func_name(int32_t idx) {
    int32_t count = 0;
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2) {
            if (count == idx) {
                return g_func_table[i].name;
            }
            count++;
        }
    }
    return NULL;
}

// Get function hit count by index
TML_EXPORT int32_t tml_get_func_hits(int32_t idx) {
    int32_t count = 0;
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2) {
            if (count == idx) {
                return ATOMIC_LOAD(&g_func_table[i].hit_count);
            }
            count++;
        }
    }
    return 0;
}

// Get number of functions with hit_count > 0
TML_EXPORT int32_t tml_get_covered_func_count(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2 &&
            ATOMIC_LOAD(&g_func_table[i].hit_count) > 0) {
            count++;
        }
    }
    return count;
}

// Write covered functions to a file (for subprocess communication in EXE mode)
// Called by the dispatcher epilogue when TML_COVERAGE_FILE env var is set
TML_EXPORT void tml_coverage_write_file(const char* filename) {
    if (!filename) {
        return;
    }

    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "[Coverage] Failed to open %s for writing\n", filename);
        return;
    }

    // Iterate through hash table and write all covered functions
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2) {
            int32_t hits = ATOMIC_LOAD(&g_func_table[i].hit_count);
            if (hits > 0) {
                fprintf(f, "%s\n", g_func_table[i].name);
            }
        }
    }

    fclose(f);
}

// ============ TML test::coverage Module API ============
// These functions are referenced by lib/test/src/coverage/mod.tml via @extern FFI.
// They provide the public TML API for coverage tracking and queries.

// Simple line/branch tracking (not used by the compiler's instrumentation,
// but available for manual use via the test::coverage TML module)
#define LINE_TABLE_SIZE 8192
#define BRANCH_TABLE_SIZE 4096

typedef struct {
    volatile int32_t occupied;
    char file[128];
    int32_t line;
} LineEntry;

typedef struct {
    volatile int32_t occupied;
    char file[128];
    int32_t line;
    int32_t branch_id;
} BranchEntry;

static LineEntry g_line_table[LINE_TABLE_SIZE];
static volatile int32_t g_line_count = 0;
static BranchEntry g_branch_table[BRANCH_TABLE_SIZE];
static volatile int32_t g_branch_count = 0;

// Reset all coverage data (functions, lines, branches)
TML_EXPORT void tml_reset_coverage(void) {
    // Reset function table
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        ATOMIC_STORE(&g_func_table[i].occupied, 0);
        g_func_table[i].hit_count = 0;
        g_func_table[i].name[0] = '\0';
    }
    ATOMIC_STORE(&g_func_count, 0);

    // Reset line table
    for (int32_t i = 0; i < LINE_TABLE_SIZE; i++) {
        ATOMIC_STORE(&g_line_table[i].occupied, 0);
    }
    ATOMIC_STORE(&g_line_count, 0);

    // Reset branch table
    for (int32_t i = 0; i < BRANCH_TABLE_SIZE; i++) {
        ATOMIC_STORE(&g_branch_table[i].occupied, 0);
    }
    ATOMIC_STORE(&g_branch_count, 0);
}

// Check if a specific function has been covered
TML_EXPORT int32_t tml_is_func_covered(const char* name) {
    if (!name)
        return 0;

    uint32_t hash = hash_string(name);
    uint32_t idx = hash % HASH_TABLE_SIZE;
    uint32_t start_idx = idx;

    do {
        FuncEntry* entry = &g_func_table[idx];
        int32_t state = ATOMIC_LOAD(&entry->occupied);

        if (state == 0) {
            return 0; // Empty slot = not found
        }
        if (state == 2 && strcmp(entry->name, name) == 0) {
            return ATOMIC_LOAD(&entry->hit_count) > 0 ? 1 : 0;
        }
        idx = (idx + 1) % HASH_TABLE_SIZE;
    } while (idx != start_idx);

    return 0;
}

// Mark a line as covered
TML_EXPORT void tml_cover_line(const char* file, int32_t line) {
    if (!file)
        return;

    int32_t count = ATOMIC_LOAD(&g_line_count);
    if (count >= LINE_TABLE_SIZE)
        return;

    // Check if already tracked
    for (int32_t i = 0; i < LINE_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_line_table[i].occupied) == 0)
            break;
        if (g_line_table[i].line == line && strcmp(g_line_table[i].file, file) == 0)
            return; // Already tracked
    }

    int32_t idx = ATOMIC_LOAD(&g_line_count);
    if (idx < LINE_TABLE_SIZE) {
        strncpy(g_line_table[idx].file, file, 127);
        g_line_table[idx].file[127] = '\0';
        g_line_table[idx].line = line;
        ATOMIC_STORE(&g_line_table[idx].occupied, 1);
        ATOMIC_INCREMENT(&g_line_count);
    }
}

// Get number of covered lines
TML_EXPORT int32_t tml_get_covered_line_count(void) {
    return ATOMIC_LOAD(&g_line_count);
}

// Mark a branch as covered
TML_EXPORT void tml_cover_branch(const char* file, int32_t line, int32_t branch_id) {
    if (!file)
        return;

    int32_t count = ATOMIC_LOAD(&g_branch_count);
    if (count >= BRANCH_TABLE_SIZE)
        return;

    // Check if already tracked
    for (int32_t i = 0; i < BRANCH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_branch_table[i].occupied) == 0)
            break;
        if (g_branch_table[i].line == line && g_branch_table[i].branch_id == branch_id &&
            strcmp(g_branch_table[i].file, file) == 0)
            return; // Already tracked
    }

    int32_t idx = ATOMIC_LOAD(&g_branch_count);
    if (idx < BRANCH_TABLE_SIZE) {
        strncpy(g_branch_table[idx].file, file, 127);
        g_branch_table[idx].file[127] = '\0';
        g_branch_table[idx].line = line;
        g_branch_table[idx].branch_id = branch_id;
        ATOMIC_STORE(&g_branch_table[idx].occupied, 1);
        ATOMIC_INCREMENT(&g_branch_count);
    }
}

// Get number of covered branches
TML_EXPORT int32_t tml_get_covered_branch_count(void) {
    return ATOMIC_LOAD(&g_branch_count);
}

// Get coverage percentage (based on function coverage)
TML_EXPORT int32_t tml_get_coverage_percent(void) {
    int32_t total = ATOMIC_LOAD(&g_func_count);
    if (total == 0)
        return 0;
    int32_t covered = 0;
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2 &&
            ATOMIC_LOAD(&g_func_table[i].hit_count) > 0) {
            covered++;
        }
    }
    return (covered * 100) / total;
}

// Print a simple coverage report to stdout
TML_EXPORT void tml_print_coverage_report(void) {
    int32_t total = ATOMIC_LOAD(&g_func_count);
    int32_t covered = 0;
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2 &&
            ATOMIC_LOAD(&g_func_table[i].hit_count) > 0) {
            covered++;
        }
    }
    printf("\n=== Coverage Report ===\n");
    printf("Functions: %d/%d", covered, total);
    if (total > 0) {
        printf(" (%d%%)", (covered * 100) / total);
    }
    printf("\n");
    printf("Lines:     %d\n", ATOMIC_LOAD(&g_line_count));
    printf("Branches:  %d\n", ATOMIC_LOAD(&g_branch_count));
    printf("=======================\n");
}
