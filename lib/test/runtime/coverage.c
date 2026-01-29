// TML Code Coverage Runtime
// Tracks test coverage data
// Note: _CRT_SECURE_NO_WARNINGS is defined via compile flags for all C runtime files

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Export functions from DLLs
#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

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

TML_EXPORT void tml_cover_func(const char* name) {
    int32_t idx = find_or_create_func(name);
    if (idx >= 0) {
        g_functions[idx].hit_count++;
    }
}

TML_EXPORT void tml_cover_line(const char* file, int32_t line) {
    int32_t idx = find_or_create_line(file, line);
    if (idx >= 0) {
        g_lines[idx].hit_count++;
    }
}

TML_EXPORT void tml_cover_branch(const char* file, int32_t line, int32_t branch_id) {
    int32_t idx = find_or_create_branch(file, line, branch_id);
    if (idx >= 0) {
        g_branches[idx].hit_count++;
    }
}

TML_EXPORT int32_t tml_get_covered_func_count(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_func_count; i++) {
        if (g_functions[i].hit_count > 0) {
            count++;
        }
    }
    return count;
}

TML_EXPORT int32_t tml_get_covered_line_count(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_line_count; i++) {
        if (g_lines[i].hit_count > 0) {
            count++;
        }
    }
    return count;
}

TML_EXPORT int32_t tml_get_covered_branch_count(void) {
    int32_t count = 0;
    for (int32_t i = 0; i < g_branch_count; i++) {
        if (g_branches[i].hit_count > 0) {
            count++;
        }
    }
    return count;
}

TML_EXPORT int32_t tml_is_func_covered(const char* name) {
    for (int32_t i = 0; i < g_func_count; i++) {
        if (strcmp(g_functions[i].name, name) == 0) {
            return g_functions[i].hit_count > 0 ? 1 : 0;
        }
    }
    return 0;
}

TML_EXPORT int32_t tml_get_coverage_percent(void) {
    if (g_func_count == 0)
        return 100;
    return (tml_get_covered_func_count() * 100) / g_func_count;
}

// Get total function count
TML_EXPORT int32_t tml_get_func_count(void) {
    return g_func_count;
}

// Get function name by index (for iteration)
TML_EXPORT const char* tml_get_func_name(int32_t idx) {
    if (idx >= 0 && idx < g_func_count) {
        return g_functions[idx].name;
    }
    return NULL;
}

// Get function hit count by index
TML_EXPORT int32_t tml_get_func_hits(int32_t idx) {
    if (idx >= 0 && idx < g_func_count) {
        return g_functions[idx].hit_count;
    }
    return 0;
}

TML_EXPORT void tml_reset_coverage(void) {
    g_func_count = 0;
    g_line_count = 0;
    g_branch_count = 0;
}

TML_EXPORT void tml_print_coverage_report(void) {
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

    int32_t covered_funcs = tml_get_covered_func_count();
    double coverage_pct = g_func_count > 0 ? (100.0 * covered_funcs / g_func_count) : 0.0;

    fprintf(f, "{\n");
    fprintf(f, "  \"total_functions\": %d,\n", g_func_count);
    fprintf(f, "  \"covered_functions\": %d,\n", covered_funcs);
    fprintf(f, "  \"coverage_percent\": %.2f,\n", coverage_pct);
    fprintf(f, "  \"functions\": [\n");

    for (int32_t i = 0; i < g_func_count; i++) {
        fprintf(f, "    {\"name\": \"%s\", \"calls\": %d}%s\n", g_functions[i].name,
                g_functions[i].hit_count, (i + 1 < g_func_count) ? "," : "");
    }

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

    int32_t covered_funcs = tml_get_covered_func_count();
    double coverage_pct = g_func_count > 0 ? (100.0 * covered_funcs / g_func_count) : 0.0;

    // Calculate total calls
    int64_t total_calls = 0;
    for (int32_t i = 0; i < g_func_count; i++) {
        total_calls += g_functions[i].hit_count;
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
    fprintf(f, "        <div class=\"stat-value\">%d / %d</div>\n", covered_funcs, g_func_count);
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
    for (int32_t i = 0; i < g_func_count; i++) {
        if (g_functions[i].hit_count > max_calls) {
            max_calls = g_functions[i].hit_count;
        }
    }

    for (int32_t i = 0; i < g_func_count; i++) {
        int is_covered = g_functions[i].hit_count > 0;
        double bar_width = (g_functions[i].hit_count * 100.0) / max_calls;

        fprintf(f, "        <tr>\n");
        fprintf(f, "          <td>%s</td>\n", g_functions[i].name);
        fprintf(f, "          <td class=\"calls\">%d</td>\n", g_functions[i].hit_count);
        fprintf(f, "          <td class=\"%s\">%s", is_covered ? "covered" : "uncovered",
                is_covered ? "&#x2713;" : "&#x2717;");
        if (is_covered && bar_width > 0) {
            fprintf(f, "<span class=\"bar\" style=\"width: %.0fpx; background: #00d26a;\"></span>",
                    bar_width);
        }
        fprintf(f, "</td>\n");
        fprintf(f, "        </tr>\n");
    }

    fprintf(f, "      </tbody>\n");
    fprintf(f, "    </table>\n");
    fprintf(f, "  </div>\n");
    fprintf(f, "</body>\n");
    fprintf(f, "</html>\n");

    fclose(f);
    printf("[Coverage] HTML report written to %s\n", filename);
}
