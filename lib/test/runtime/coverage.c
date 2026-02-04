// TML Code Coverage Runtime
// Tracks test coverage data using lock-free hash table for performance
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

static CRITICAL_SECTION g_coverage_lock;
static volatile LONG g_lock_initialized = 0;

static void ensure_lock_initialized(void) {
    if (InterlockedCompareExchange(&g_lock_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_coverage_lock);
    }
}

#define COVERAGE_LOCK()                                                                            \
    do {                                                                                           \
        ensure_lock_initialized();                                                                 \
        EnterCriticalSection(&g_coverage_lock);                                                    \
    } while (0)
#define COVERAGE_UNLOCK() LeaveCriticalSection(&g_coverage_lock)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#include <pthread.h>

// Atomic operations for GCC/Clang
#define ATOMIC_INCREMENT(ptr) __sync_add_and_fetch((ptr), 1)
#define ATOMIC_LOAD(ptr) ((int32_t)__sync_add_and_fetch((ptr), 0))
#define ATOMIC_STORE(ptr, val) __sync_lock_test_and_set((ptr), (val))
#define ATOMIC_CAS(ptr, expected, desired)                                                         \
    __sync_bool_compare_and_swap((ptr), (expected), (desired))
// Memory barrier
#define MEMORY_BARRIER() __sync_synchronize()

static pthread_mutex_t g_coverage_lock = PTHREAD_MUTEX_INITIALIZER;

#define COVERAGE_LOCK() pthread_mutex_lock(&g_coverage_lock)
#define COVERAGE_UNLOCK() pthread_mutex_unlock(&g_coverage_lock)
#endif

// Hash table parameters
#define HASH_TABLE_SIZE 4093 // Prime number for better distribution
#define MAX_NAME_LEN 192     // Function names rarely exceed this

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

// Legacy line/branch coverage (less frequent, keep simple)
typedef struct {
    char file[MAX_NAME_LEN];
    int32_t line;
    int32_t hit_count;
} LineCoverage;

typedef struct {
    char file[MAX_NAME_LEN];
    int32_t line;
    int32_t branch_id;
    int32_t hit_count;
} BranchCoverage;

#define INITIAL_CAPACITY 1024

static LineCoverage* g_lines = NULL;
static int32_t g_line_count = 0;
static int32_t g_line_capacity = 0;

static BranchCoverage* g_branches = NULL;
static int32_t g_branch_count = 0;
static int32_t g_branch_capacity = 0;

// Helper: Ensure line array has capacity (requires lock)
static void ensure_line_capacity(void) {
    if (g_lines == NULL) {
        g_line_capacity = INITIAL_CAPACITY;
        g_lines = (LineCoverage*)malloc(g_line_capacity * sizeof(LineCoverage));
    } else if (g_line_count >= g_line_capacity) {
        g_line_capacity *= 2;
        g_lines = (LineCoverage*)realloc(g_lines, g_line_capacity * sizeof(LineCoverage));
    }
}

// Helper: Ensure branch array has capacity (requires lock)
static void ensure_branch_capacity(void) {
    if (g_branches == NULL) {
        g_branch_capacity = INITIAL_CAPACITY;
        g_branches = (BranchCoverage*)malloc(g_branch_capacity * sizeof(BranchCoverage));
    } else if (g_branch_count >= g_branch_capacity) {
        g_branch_capacity *= 2;
        g_branches =
            (BranchCoverage*)realloc(g_branches, g_branch_capacity * sizeof(BranchCoverage));
    }
}

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

// Helper: Find or create line entry (requires lock)
static int32_t find_or_create_line(const char* file, int32_t line) {
    for (int32_t i = 0; i < g_line_count; i++) {
        if (g_lines[i].line == line && strcmp(g_lines[i].file, file) == 0) {
            return i;
        }
    }
    ensure_line_capacity();
    strncpy(g_lines[g_line_count].file, file, MAX_NAME_LEN - 1);
    g_lines[g_line_count].file[MAX_NAME_LEN - 1] = '\0';
    g_lines[g_line_count].line = line;
    g_lines[g_line_count].hit_count = 0;
    return g_line_count++;
}

// Helper: Find or create branch entry (requires lock)
static int32_t find_or_create_branch(const char* file, int32_t line, int32_t branch_id) {
    for (int32_t i = 0; i < g_branch_count; i++) {
        if (g_branches[i].line == line && g_branches[i].branch_id == branch_id &&
            strcmp(g_branches[i].file, file) == 0) {
            return i;
        }
    }
    ensure_branch_capacity();
    strncpy(g_branches[g_branch_count].file, file, MAX_NAME_LEN - 1);
    g_branches[g_branch_count].file[MAX_NAME_LEN - 1] = '\0';
    g_branches[g_branch_count].line = line;
    g_branches[g_branch_count].branch_id = branch_id;
    g_branches[g_branch_count].hit_count = 0;
    return g_branch_count++;
}

// ============ Public API ============

// Lock-free function coverage - the most frequently called function
TML_EXPORT void tml_cover_func(const char* name) {
    volatile int32_t* hit_count = find_or_create_func_lockfree(name);
    if (hit_count) {
        ATOMIC_INCREMENT(hit_count);
    }
}

TML_EXPORT void tml_cover_line(const char* file, int32_t line) {
    COVERAGE_LOCK();
    int32_t idx = find_or_create_line(file, line);
    if (idx >= 0) {
        g_lines[idx].hit_count++;
    }
    COVERAGE_UNLOCK();
}

TML_EXPORT void tml_cover_branch(const char* file, int32_t line, int32_t branch_id) {
    COVERAGE_LOCK();
    int32_t idx = find_or_create_branch(file, line, branch_id);
    if (idx >= 0) {
        g_branches[idx].hit_count++;
    }
    COVERAGE_UNLOCK();
}

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

TML_EXPORT int32_t tml_get_covered_line_count(void) {
    COVERAGE_LOCK();
    int32_t count = 0;
    for (int32_t i = 0; i < g_line_count; i++) {
        if (g_lines[i].hit_count > 0) {
            count++;
        }
    }
    COVERAGE_UNLOCK();
    return count;
}

TML_EXPORT int32_t tml_get_covered_branch_count(void) {
    COVERAGE_LOCK();
    int32_t count = 0;
    for (int32_t i = 0; i < g_branch_count; i++) {
        if (g_branches[i].hit_count > 0) {
            count++;
        }
    }
    COVERAGE_UNLOCK();
    return count;
}

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
            return 0; // Not found (empty slot)
        }
        if (state == 2 && strcmp(entry->name, name) == 0) {
            return ATOMIC_LOAD(&entry->hit_count) > 0 ? 1 : 0;
        }
        idx = (idx + 1) % HASH_TABLE_SIZE;
    } while (idx != start_idx);

    return 0;
}

TML_EXPORT int32_t tml_get_coverage_percent(void) {
    int32_t total = ATOMIC_LOAD(&g_func_count);
    if (total == 0)
        return 100;

    int32_t covered = tml_get_covered_func_count();
    return (covered * 100) / total;
}

// Get total function count
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

TML_EXPORT void tml_reset_coverage(void) {
    COVERAGE_LOCK();
    // Reset hash table
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        g_func_table[i].occupied = 0;
        g_func_table[i].hit_count = 0;
        g_func_table[i].name[0] = '\0';
    }
    g_func_count = 0;

    // Free dynamically allocated memory for lines/branches
    if (g_lines) {
        free(g_lines);
        g_lines = NULL;
    }
    if (g_branches) {
        free(g_branches);
        g_branches = NULL;
    }
    g_line_count = 0;
    g_line_capacity = 0;
    g_branch_count = 0;
    g_branch_capacity = 0;
    COVERAGE_UNLOCK();
}

TML_EXPORT void tml_print_coverage_report(void) {
    int32_t func_count = tml_get_func_count();

    printf("\n");
    printf("================================================================================\n");
    printf("                           CODE COVERAGE REPORT\n");
    printf("================================================================================\n");
    printf("\n");

    // Function coverage
    int32_t covered_funcs = tml_get_covered_func_count();
    printf("FUNCTION COVERAGE: %d/%d", covered_funcs, func_count);
    if (func_count > 0) {
        printf(" (%.1f%%)", (float)covered_funcs * 100.0f / (float)func_count);
    }
    printf("\n");
    printf("--------------------------------------------------------------------------------\n");

    // Iterate through hash table
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2) {
            int32_t hits = ATOMIC_LOAD(&g_func_table[i].hit_count);
            const char* status = hits > 0 ? "[+]" : "[-]";
            printf("  %s %s (hits: %d)\n", status, g_func_table[i].name, hits);
        }
    }

    if (func_count == 0) {
        printf("  (no functions tracked)\n");
    }

    // Line coverage
    if (g_line_count > 0) {
        int32_t covered_lines = tml_get_covered_line_count();
        printf("\n");
        printf("LINE COVERAGE: %d/%d", covered_lines, g_line_count);
        printf(" (%.1f%%)\n", (float)covered_lines * 100.0f / (float)g_line_count);
        printf(
            "--------------------------------------------------------------------------------\n");

        // Group by file
        char current_file[MAX_NAME_LEN] = "";
        for (int32_t i = 0; i < g_line_count; i++) {
            if (strcmp(current_file, g_lines[i].file) != 0) {
                strncpy(current_file, g_lines[i].file, MAX_NAME_LEN - 1);
                printf("  %s:\n", current_file);
            }
            const char* status = g_lines[i].hit_count > 0 ? "+" : "-";
            printf("    %s L%d (hits: %d)\n", status, g_lines[i].line, g_lines[i].hit_count);
        }
    }

    // Branch coverage
    if (g_branch_count > 0) {
        int32_t covered_branches = tml_get_covered_branch_count();
        printf("\n");
        printf("BRANCH COVERAGE: %d/%d", covered_branches, g_branch_count);
        printf(" (%.1f%%)\n", (float)covered_branches * 100.0f / (float)g_branch_count);
        printf(
            "--------------------------------------------------------------------------------\n");

        for (int32_t i = 0; i < g_branch_count; i++) {
            const char* status = g_branches[i].hit_count > 0 ? "+" : "-";
            printf("  %s %s:L%d:B%d (hits: %d)\n", status, g_branches[i].file, g_branches[i].line,
                   g_branches[i].branch_id, g_branches[i].hit_count);
        }
    }

    printf("\n");
    printf("================================================================================\n");
    printf("                              SUMMARY\n");
    printf("================================================================================\n");
    printf("  Functions: %d covered / %d total\n", covered_funcs, func_count);
    if (g_line_count > 0) {
        printf("  Lines:     %d covered / %d total\n", tml_get_covered_line_count(), g_line_count);
    }
    if (g_branch_count > 0) {
        printf("  Branches:  %d covered / %d total\n", tml_get_covered_branch_count(),
               g_branch_count);
    }
    printf("================================================================================\n");
}

// Alias for codegen compatibility
TML_EXPORT void print_coverage_report(void) {
    tml_print_coverage_report();
}

// Write coverage report to JSON file
TML_EXPORT void write_coverage_json(const char* filename) {
    if (!filename)
        filename = "coverage.json";

    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot write coverage to %s\n", filename);
        return;
    }

    int32_t func_count = tml_get_func_count();
    int32_t covered_funcs = tml_get_covered_func_count();
    double coverage_pct = func_count > 0 ? (100.0 * covered_funcs / func_count) : 0.0;

    fprintf(f, "{\n");
    fprintf(f, "  \"total_functions\": %d,\n", func_count);
    fprintf(f, "  \"covered_functions\": %d,\n", covered_funcs);
    fprintf(f, "  \"coverage_percent\": %.2f,\n", coverage_pct);
    fprintf(f, "  \"functions\": [\n");

    int32_t written = 0;
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2) {
            if (written > 0)
                fprintf(f, ",\n");
            fprintf(f, "    {\"name\": \"%s\", \"calls\": %d}", g_func_table[i].name,
                    ATOMIC_LOAD(&g_func_table[i].hit_count));
            written++;
        }
    }
    if (written > 0)
        fprintf(f, "\n");

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);

    printf("[Coverage] JSON data written to %s\n", filename);
}

// Write coverage report to HTML file
TML_EXPORT void write_coverage_html(const char* filename) {
    if (!filename)
        filename = "coverage.html";

    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot write coverage to %s\n", filename);
        return;
    }

    int32_t func_count = tml_get_func_count();
    int32_t covered_funcs = tml_get_covered_func_count();
    double coverage_pct = func_count > 0 ? (100.0 * covered_funcs / func_count) : 0.0;

    // Calculate total calls
    int64_t total_calls = 0;
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2) {
            total_calls += ATOMIC_LOAD(&g_func_table[i].hit_count);
        }
    }

    // Write HTML
    fprintf(f, "<!DOCTYPE html>\n");
    fprintf(f, "<html lang=\"en\">\n");
    fprintf(f, "<head>\n");
    fprintf(f, "  <meta charset=\"UTF-8\">\n");
    fprintf(f, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(f, "  <title>TML Code Coverage Report</title>\n");
    fprintf(f, "  <style>\n");
    fprintf(f, "    :root { --bg: #1a1a2e; --surface: #16213e; --primary: #0f3460; --accent: "
               "#e94560; --text: #eee; --dim: #888; }\n");
    fprintf(f, "    body { font-family: 'Segoe UI', system-ui, sans-serif; background: var(--bg); "
               "color: var(--text); margin: 0; padding: 20px; }\n");
    fprintf(f, "    .container { max-width: 1000px; margin: 0 auto; }\n");
    fprintf(f, "    h1 { color: var(--accent); margin-bottom: 10px; }\n");
    fprintf(f, "    .subtitle { color: var(--dim); margin-bottom: 30px; }\n");
    fprintf(f, "    .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, "
               "1fr)); gap: 20px; margin-bottom: 30px; }\n");
    fprintf(f, "    .stat-card { background: var(--surface); padding: 20px; border-radius: 8px; "
               "border-left: 4px solid var(--accent); }\n");
    fprintf(f, "    .stat-value { font-size: 2em; font-weight: bold; color: var(--accent); }\n");
    fprintf(f, "    .stat-label { color: var(--dim); font-size: 0.9em; margin-top: 5px; }\n");
    fprintf(f, "    .progress-bar { background: var(--primary); border-radius: 10px; height: 20px; "
               "margin: 20px 0; overflow: hidden; }\n");
    fprintf(f, "    .progress-fill { background: linear-gradient(90deg, #00d26a, #70e000); height: "
               "100%%; transition: width 0.5s; }\n");
    fprintf(f, "    table { width: 100%%; border-collapse: collapse; background: var(--surface); "
               "border-radius: 8px; overflow: hidden; }\n");
    fprintf(f, "    th, td { padding: 12px 16px; text-align: left; border-bottom: 1px solid "
               "var(--primary); }\n");
    fprintf(f, "    th { background: var(--primary); color: var(--text); font-weight: 600; }\n");
    fprintf(f, "    tr:hover { background: rgba(233, 69, 96, 0.1); }\n");
    fprintf(f, "    .calls { text-align: right; font-family: monospace; }\n");
    fprintf(f, "    .covered { color: #00d26a; }\n");
    fprintf(f, "    .uncovered { color: var(--accent); }\n");
    fprintf(f, "    .bar { display: inline-block; height: 8px; background: var(--accent); "
               "border-radius: 4px; margin-left: 10px; }\n");
    fprintf(f, "  </style>\n");
    fprintf(f, "</head>\n");
    fprintf(f, "<body>\n");
    fprintf(f, "  <div class=\"container\">\n");
    fprintf(f, "    <h1>TML Code Coverage Report</h1>\n");
    fprintf(f, "    <p class=\"subtitle\">Generated by TML Compiler</p>\n");
    fprintf(f, "\n");
    fprintf(f, "    <div class=\"stats\">\n");
    fprintf(f, "      <div class=\"stat-card\">\n");
    fprintf(f, "        <div class=\"stat-value\">%.1f%%</div>\n", coverage_pct);
    fprintf(f, "        <div class=\"stat-label\">Function Coverage</div>\n");
    fprintf(f, "      </div>\n");
    fprintf(f, "      <div class=\"stat-card\">\n");
    fprintf(f, "        <div class=\"stat-value\">%d / %d</div>\n", covered_funcs, func_count);
    fprintf(f, "        <div class=\"stat-label\">Functions Covered</div>\n");
    fprintf(f, "      </div>\n");
    fprintf(f, "      <div class=\"stat-card\">\n");
    fprintf(f, "        <div class=\"stat-value\">%lld</div>\n", (long long)total_calls);
    fprintf(f, "        <div class=\"stat-label\">Total Calls</div>\n");
    fprintf(f, "      </div>\n");
    fprintf(f, "    </div>\n");
    fprintf(f, "\n");
    fprintf(f, "    <div class=\"progress-bar\">\n");
    fprintf(f, "      <div class=\"progress-fill\" style=\"width: %.1f%%;\"></div>\n",
            coverage_pct);
    fprintf(f, "    </div>\n");
    fprintf(f, "\n");
    fprintf(f, "    <table>\n");
    fprintf(f, "      <thead>\n");
    fprintf(f, "        <tr>\n");
    fprintf(f, "          <th>Function</th>\n");
    fprintf(f, "          <th class=\"calls\">Calls</th>\n");
    fprintf(f, "          <th>Status</th>\n");
    fprintf(f, "        </tr>\n");
    fprintf(f, "      </thead>\n");
    fprintf(f, "      <tbody>\n");

    // Find max calls for bar scaling
    int32_t max_calls = 1;
    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2) {
            int32_t hits = ATOMIC_LOAD(&g_func_table[i].hit_count);
            if (hits > max_calls) {
                max_calls = hits;
            }
        }
    }

    for (int32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        if (ATOMIC_LOAD(&g_func_table[i].occupied) == 2) {
            int32_t hits = ATOMIC_LOAD(&g_func_table[i].hit_count);
            int is_covered = hits > 0;
            double bar_width = (hits * 100.0) / max_calls;

            fprintf(f, "        <tr>\n");
            fprintf(f, "          <td>%s</td>\n", g_func_table[i].name);
            fprintf(f, "          <td class=\"calls\">%d</td>\n", hits);
            fprintf(f, "          <td class=\"%s\">%s", is_covered ? "covered" : "uncovered",
                    is_covered ? "&#x2713;" : "&#x2717;");
            if (is_covered && bar_width > 0) {
                fprintf(f,
                        "<span class=\"bar\" style=\"width: %.0fpx; background: #00d26a;\"></span>",
                        bar_width);
            }
            fprintf(f, "</td>\n");
            fprintf(f, "        </tr>\n");
        }
    }

    fprintf(f, "      </tbody>\n");
    fprintf(f, "    </table>\n");
    fprintf(f, "  </div>\n");
    fprintf(f, "</body>\n");
    fprintf(f, "</html>\n");

    fclose(f);
    printf("[Coverage] HTML report written to %s\n", filename);
}
