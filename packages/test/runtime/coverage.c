// TML Code Coverage Runtime
// Tracks test coverage data

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum number of entries to track
#define MAX_FUNCTIONS 1024
#define MAX_LINES 8192
#define MAX_BRANCHES 4096
#define MAX_NAME_LEN 256

// Coverage data structures
typedef struct {
    char name[MAX_NAME_LEN];
    int32_t hit_count;
} FuncCoverage;

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

// Global coverage storage
static FuncCoverage g_functions[MAX_FUNCTIONS];
static int32_t g_func_count = 0;

static LineCoverage g_lines[MAX_LINES];
static int32_t g_line_count = 0;

static BranchCoverage g_branches[MAX_BRANCHES];
static int32_t g_branch_count = 0;

// Helper: Find or create function entry
static int32_t find_or_create_func(const char* name) {
    for (int32_t i = 0; i < g_func_count; i++) {
        if (strcmp(g_functions[i].name, name) == 0) {
            return i;
        }
    }
    if (g_func_count < MAX_FUNCTIONS) {
        strncpy(g_functions[g_func_count].name, name, MAX_NAME_LEN - 1);
        g_functions[g_func_count].name[MAX_NAME_LEN - 1] = '\0';
        g_functions[g_func_count].hit_count = 0;
        return g_func_count++;
    }
    return -1;
}

// Helper: Find or create line entry
static int32_t find_or_create_line(const char* file, int32_t line) {
    for (int32_t i = 0; i < g_line_count; i++) {
        if (g_lines[i].line == line && strcmp(g_lines[i].file, file) == 0) {
            return i;
        }
    }
    if (g_line_count < MAX_LINES) {
        strncpy(g_lines[g_line_count].file, file, MAX_NAME_LEN - 1);
        g_lines[g_line_count].file[MAX_NAME_LEN - 1] = '\0';
        g_lines[g_line_count].line = line;
        g_lines[g_line_count].hit_count = 0;
        return g_line_count++;
    }
    return -1;
}

// Helper: Find or create branch entry
static int32_t find_or_create_branch(const char* file, int32_t line, int32_t branch_id) {
    for (int32_t i = 0; i < g_branch_count; i++) {
        if (g_branches[i].line == line && g_branches[i].branch_id == branch_id &&
            strcmp(g_branches[i].file, file) == 0) {
            return i;
        }
    }
    if (g_branch_count < MAX_BRANCHES) {
        strncpy(g_branches[g_branch_count].file, file, MAX_NAME_LEN - 1);
        g_branches[g_branch_count].file[MAX_NAME_LEN - 1] = '\0';
        g_branches[g_branch_count].line = line;
        g_branches[g_branch_count].branch_id = branch_id;
        g_branches[g_branch_count].hit_count = 0;
        return g_branch_count++;
    }
    return -1;
}

// ============ Public API ============

void tml_cover_func(const char* name) {
    int32_t idx = find_or_create_func(name);
    if (idx >= 0) {
        g_functions[idx].hit_count++;
    }
}

void tml_cover_line(const char* file, int32_t line) {
    int32_t idx = find_or_create_line(file, line);
    if (idx >= 0) {
        g_lines[idx].hit_count++;
    }
}

void tml_cover_branch(const char* file, int32_t line, int32_t branch_id) {
    int32_t idx = find_or_create_branch(file, line, branch_id);
    if (idx >= 0) {
        g_branches[idx].hit_count++;
    }
}

int32_t tml_get_covered_func_count(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_func_count; i++) {
        if (g_functions[i].hit_count > 0) {
            count++;
        }
    }
    return count;
}

int32_t tml_get_covered_line_count(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_line_count; i++) {
        if (g_lines[i].hit_count > 0) {
            count++;
        }
    }
    return count;
}

int32_t tml_get_covered_branch_count(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_branch_count; i++) {
        if (g_branches[i].hit_count > 0) {
            count++;
        }
    }
    return count;
}

int32_t tml_is_func_covered(const char* name) {
    for (int32_t i = 0; i < g_func_count; i++) {
        if (strcmp(g_functions[i].name, name) == 0) {
            return g_functions[i].hit_count > 0 ? 1 : 0;
        }
    }
    return 0;
}

int32_t tml_get_coverage_percent(void) {
    if (g_func_count == 0)
        return 100;
    return (tml_get_covered_func_count() * 100) / g_func_count;
}

void tml_reset_coverage(void) {
    g_func_count = 0;
    g_line_count = 0;
    g_branch_count = 0;
}

void tml_print_coverage_report(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("                           CODE COVERAGE REPORT\n");
    printf("================================================================================\n");
    printf("\n");

    // Function coverage
    int32_t covered_funcs = tml_get_covered_func_count();
    printf("FUNCTION COVERAGE: %d/%d", covered_funcs, g_func_count);
    if (g_func_count > 0) {
        printf(" (%.1f%%)", (float)covered_funcs * 100.0f / (float)g_func_count);
    }
    printf("\n");
    printf("--------------------------------------------------------------------------------\n");

    for (int32_t i = 0; i < g_func_count; i++) {
        const char* status = g_functions[i].hit_count > 0 ? "[+]" : "[-]";
        printf("  %s %s (hits: %d)\n", status, g_functions[i].name, g_functions[i].hit_count);
    }

    if (g_func_count == 0) {
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
    printf("  Functions: %d covered / %d total\n", covered_funcs, g_func_count);
    if (g_line_count > 0) {
        printf("  Lines:     %d covered / %d total\n", tml_get_covered_line_count(), g_line_count);
    }
    if (g_branch_count > 0) {
        printf("  Branches:  %d covered / %d total\n", tml_get_covered_branch_count(),
               g_branch_count);
    }
    printf("================================================================================\n");
}
