/**
 * @file console.c
 * @brief TML Runtime - Console global state management
 *
 * Provides global mutable state for the std::console module:
 * - Named timers (start timestamps in nanoseconds)
 * - Named counters (increment counts)
 * - Indentation level for group/group_end
 *
 * Uses simple linear-scan arrays with fixed capacity. This is a
 * developer debugging tool, not a production hot path.
 *
 * Thread safety: NOT thread-safe. Console is a debugging aid
 * intended for single-threaded development use.
 */

#include <stdint.h>
#include <string.h>

/* ================================================================
 * Inspector integration — console output forwarding
 * ================================================================
 *
 * When the TML inspector (CDP WebSocket server) is active, console output
 * should also be forwarded to connected DevTools clients as
 * Runtime.consoleAPICalled events.  To avoid a hard link dependency on
 * inspector.c inside test executables, we use a callback registered at
 * runtime by the inspector when it initialises.
 */

typedef void (*console_forward_fn_t)(const char* type, const char* message);
static console_forward_fn_t g_console_forward = NULL;

/**
 * Register a callback that receives (type, message) pairs for every console
 * output call.  Called by tml_inspector_init() when the inspector starts.
 * Pass NULL to deregister (inspector shutdown).
 */
void rt_console_set_forward(console_forward_fn_t fn) {
    g_console_forward = fn;
}

/* ================================================================
 * Configuration
 * ================================================================ */

#define CONSOLE_MAX_TIMERS 64
#define CONSOLE_MAX_COUNTERS 64
#define CONSOLE_MAX_LABEL 128

/* ================================================================
 * Timer state
 * ================================================================ */

typedef struct {
    char label[CONSOLE_MAX_LABEL];
    int64_t start_ns;
    int active;
} console_timer_t;

static console_timer_t g_timers[CONSOLE_MAX_TIMERS];
static int g_timer_count = 0;

/* ================================================================
 * Counter state
 * ================================================================ */

typedef struct {
    char label[CONSOLE_MAX_LABEL];
    int64_t count;
} console_counter_t;

static console_counter_t g_counters[CONSOLE_MAX_COUNTERS];
static int g_counter_count = 0;

/* ================================================================
 * Indentation state
 * ================================================================ */

static int g_indent_level = 0;

/* ================================================================
 * Timer functions
 * ================================================================ */

/**
 * Start a named timer. Stores the given nanosecond timestamp.
 * If the label already exists and is active, it is restarted.
 * Returns 0 on success, -1 if table is full.
 */
int64_t rt_console_timer_start(const char* label, int64_t now_ns) {
    /* Search for existing timer with this label */
    for (int i = 0; i < g_timer_count; i++) {
        if (g_timers[i].active && strcmp(g_timers[i].label, label) == 0) {
            g_timers[i].start_ns = now_ns;
            return 0;
        }
    }
    /* Try to reuse an inactive slot */
    for (int i = 0; i < g_timer_count; i++) {
        if (!g_timers[i].active) {
            strncpy(g_timers[i].label, label, CONSOLE_MAX_LABEL - 1);
            g_timers[i].label[CONSOLE_MAX_LABEL - 1] = '\0';
            g_timers[i].start_ns = now_ns;
            g_timers[i].active = 1;
            return 0;
        }
    }
    /* Add new entry */
    if (g_timer_count >= CONSOLE_MAX_TIMERS) {
        return -1; /* table full */
    }
    strncpy(g_timers[g_timer_count].label, label, CONSOLE_MAX_LABEL - 1);
    g_timers[g_timer_count].label[CONSOLE_MAX_LABEL - 1] = '\0';
    g_timers[g_timer_count].start_ns = now_ns;
    g_timers[g_timer_count].active = 1;
    g_timer_count++;
    return 0;
}

/**
 * End a named timer and return elapsed nanoseconds.
 * Returns -1 if the label was not found or not active.
 */
int64_t rt_console_timer_end(const char* label, int64_t now_ns) {
    for (int i = 0; i < g_timer_count; i++) {
        if (g_timers[i].active && strcmp(g_timers[i].label, label) == 0) {
            int64_t elapsed = now_ns - g_timers[i].start_ns;
            g_timers[i].active = 0;
            return elapsed;
        }
    }
    return -1; /* not found */
}

/* ================================================================
 * Counter functions
 * ================================================================ */

/**
 * Increment a named counter and return the new count.
 * Creates the counter with count=1 if it doesn't exist.
 * Returns -1 if table is full.
 */
int64_t rt_console_count_inc(const char* label) {
    /* Search for existing counter */
    for (int i = 0; i < g_counter_count; i++) {
        if (strcmp(g_counters[i].label, label) == 0) {
            g_counters[i].count++;
            return g_counters[i].count;
        }
    }
    /* Add new counter */
    if (g_counter_count >= CONSOLE_MAX_COUNTERS) {
        return -1; /* table full */
    }
    strncpy(g_counters[g_counter_count].label, label, CONSOLE_MAX_LABEL - 1);
    g_counters[g_counter_count].label[CONSOLE_MAX_LABEL - 1] = '\0';
    g_counters[g_counter_count].count = 1;
    g_counter_count++;
    return 1;
}

/**
 * Reset a named counter to 0. Returns 0 on success, -1 if not found.
 */
int64_t rt_console_count_reset(const char* label) {
    for (int i = 0; i < g_counter_count; i++) {
        if (strcmp(g_counters[i].label, label) == 0) {
            g_counters[i].count = 0;
            return 0;
        }
    }
    return -1; /* not found */
}

/* ================================================================
 * Indentation functions
 * ================================================================ */

/**
 * Increase indentation level by 1. Returns new level.
 */
int64_t rt_console_indent_inc(void) {
    g_indent_level++;
    return (int64_t)g_indent_level;
}

/**
 * Decrease indentation level by 1 (minimum 0). Returns new level.
 */
int64_t rt_console_indent_dec(void) {
    if (g_indent_level > 0) {
        g_indent_level--;
    }
    return (int64_t)g_indent_level;
}

/**
 * Get current indentation level.
 */
int64_t rt_console_indent_get(void) {
    return (int64_t)g_indent_level;
}
