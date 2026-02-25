/**
 * @file log.c
 * @brief TML Runtime - C Logging Implementation
 *
 * Implements the C logging API for the TML runtime. By default, log messages
 * are written to stderr with a simple format. When a callback is set (by the
 * C++ Logger), messages are routed through the callback instead.
 *
 * Thread safety: The log level and callback are set once at initialization.
 * The actual fprintf to stderr is atomic per-call on most platforms.
 */

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* Global state */
static int g_rt_log_level = RT_LOG_WARN; /* Default: only warnings and above */
static rt_log_callback_t g_rt_log_callback = NULL;
static int g_rt_log_format = 0;    /* 0=text, 1=JSON, 2=compact */
static FILE* g_rt_log_file = NULL; /* File sink (NULL = disabled) */

/* Module filter table: up to 32 per-module overrides */
#define RT_LOG_MAX_FILTERS 32
typedef struct {
    char module[64];
    int level;
} rt_log_filter_entry_t;
static rt_log_filter_entry_t g_rt_log_filters[RT_LOG_MAX_FILTERS];
static int g_rt_log_filter_count = 0;
static int g_rt_log_filter_default = -1; /* -1 = not set, use g_rt_log_level */

/* Level name strings */
static const char* g_level_names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"};
/* Short-form level names */
static const char* g_level_short[] = {"TR", "DB", "IN", "WN", "ER", "FA", "--"};

/* Forward declarations for internal helpers */
static int rt_log_check_module(int level, const char* module);
static void rt_log_write_to(FILE* f, int level, const char* module, const char* message,
                            const char* fields);
static void rt_log_output(int level, const char* module, const char* message, const char* fields);

void rt_log_set_level(int level) {
    if (level >= RT_LOG_TRACE && level <= RT_LOG_OFF) {
        g_rt_log_level = level;
    }
}

int rt_log_get_level(void) {
    return g_rt_log_level;
}

int rt_log_enabled(int level) {
    return level >= g_rt_log_level;
}

void rt_log_set_callback(rt_log_callback_t callback) {
    g_rt_log_callback = callback;
}

void rt_log_va(int level, const char* module, const char* fmt, va_list args) {
    /* Fast-path: skip if below minimum level */
    if (level < g_rt_log_level || level >= RT_LOG_OFF) {
        if (g_rt_log_filter_count == 0 || !rt_log_check_module(level, module)) {
            return;
        }
    } else if (!rt_log_check_module(level, module)) {
        return;
    }

    /* Format the message */
    char buf[2048];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len < 0) {
        buf[0] = '\0';
    } else if ((size_t)len >= sizeof(buf)) {
        /* Truncated — add indicator */
        buf[sizeof(buf) - 4] = '.';
        buf[sizeof(buf) - 3] = '.';
        buf[sizeof(buf) - 2] = '.';
        buf[sizeof(buf) - 1] = '\0';
    }

    rt_log_output(level, module, buf, NULL);
}

void rt_log(int level, const char* module, const char* fmt, ...) {
    /* Fast-path: skip if below minimum level */
    if (level < g_rt_log_level || level >= RT_LOG_OFF) {
        if (g_rt_log_filter_count == 0)
            return;
    }

    va_list args;
    va_start(args, fmt);
    rt_log_va(level, module, fmt, args);
    va_end(args);
}

/* Internal: check module filter for a specific module */
static int rt_log_check_module(int level, const char* module) {
    if (g_rt_log_filter_count > 0 && module) {
        for (int i = 0; i < g_rt_log_filter_count; i++) {
            if (strcmp(g_rt_log_filters[i].module, module) == 0) {
                return level >= g_rt_log_filters[i].level;
            }
        }
        /* No specific filter for this module — use filter default or global level */
        if (g_rt_log_filter_default >= 0) {
            return level >= g_rt_log_filter_default;
        }
    }
    return level >= g_rt_log_level;
}

/* Internal: get current time in HH:MM:SS.mmm format */
static void rt_get_timestamp(char* buf, size_t buf_size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);

    if (tm_info) {
#ifdef _WIN32
        SYSTEMTIME st;
        GetSystemTime(&st);
        int ms = st.wMilliseconds;
        snprintf(buf, buf_size, "%02d:%02d:%02d.%03d", tm_info->tm_hour, tm_info->tm_min,
                 tm_info->tm_sec, ms);
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int ms = (int)(tv.tv_usec / 1000);
        snprintf(buf, buf_size, "%02d:%02d:%02d.%03d", tm_info->tm_hour, tm_info->tm_min,
                 tm_info->tm_sec, ms);
#endif
    } else {
        snprintf(buf, buf_size, "??:??:??.???");
    }
}

/* Internal: write a formatted line to a FILE* in the current format */
static void rt_log_write_to(FILE* f, int level, const char* module, const char* message,
                            const char* fields) {
    const char* mod = module ? module : "runtime";
    const char* msg = message ? message : "";
    const char* lvl = (level >= 0 && level <= 5) ? g_level_names[level] : "???";
    const char* lvl_short = (level >= 0 && level <= 5) ? g_level_short[level] : "??";
    char timestamp[32];
    rt_get_timestamp(timestamp, sizeof(timestamp));

    if (g_rt_log_format == 1) {
        /* JSON format */
        fprintf(f, "{\"ts\":\"%s\",\"level\":\"%s\",\"module\":\"%s\",\"msg\":\"", timestamp, lvl,
                mod);
        /* Escape JSON special chars in message */
        for (const char* p = msg; *p; p++) {
            switch (*p) {
            case '"':
                fputs("\\\"", f);
                break;
            case '\\':
                fputs("\\\\", f);
                break;
            case '\n':
                fputs("\\n", f);
                break;
            case '\r':
                fputs("\\r", f);
                break;
            case '\t':
                fputs("\\t", f);
                break;
            default:
                fputc(*p, f);
            }
        }
        fprintf(f, "\"");
        /* Append structured fields as JSON properties */
        if (fields && fields[0]) {
            const char* p = fields;
            while (*p) {
                const char* eq = strchr(p, '=');
                const char* sep = strchr(p, ';');
                if (!eq)
                    break;
                if (!sep)
                    sep = p + strlen(p);
                fprintf(f, ",\"");
                fwrite(p, 1, (size_t)(eq - p), f);
                fprintf(f, "\":\"");
                fwrite(eq + 1, 1, (size_t)(sep - eq - 1), f);
                fprintf(f, "\"");
                p = *sep ? sep + 1 : sep;
            }
        }
        fprintf(f, "}\n");
    } else if (g_rt_log_format == 2) {
        /* Compact format with timestamp */
        fprintf(f, "%s %s [%s] %s", timestamp, lvl_short, mod, msg);
        if (fields && fields[0]) {
            fprintf(f, " {%s}", fields);
        }
        fprintf(f, "\n");
    } else {
        /* Text format (default) with timestamp */
        fprintf(f, "%s %s [%s] %s", timestamp, lvl, mod, msg);
        if (fields && fields[0]) {
            fprintf(f, " | %s", fields);
        }
        fprintf(f, "\n");
    }
}

/* Internal: output a log message to all sinks */
static void rt_log_output(int level, const char* module, const char* message, const char* fields) {
    /* Route through callback if set (C++ Logger integration) */
    if (g_rt_log_callback) {
        /* For structured messages, append fields to message */
        if (fields && fields[0]) {
            char buf[4096];
            snprintf(buf, sizeof(buf), "%s | %s", message ? message : "", fields);
            g_rt_log_callback(level, module, buf);
        } else {
            g_rt_log_callback(level, module, message);
        }
    } else {
        /* Default: write to stderr */
        rt_log_write_to(stderr, level, module, message, fields);
    }

    /* Also write to file sink if open */
    if (g_rt_log_file) {
        rt_log_write_to(g_rt_log_file, level, module, message, fields);
        /* Auto-flush on Error and Fatal */
        if (level >= RT_LOG_ERROR) {
            fflush(g_rt_log_file);
        }
    }
}

void rt_log_msg(int level, const char* module, const char* message) {
    /* Fast-path: global level check */
    if (level < g_rt_log_level || level >= RT_LOG_OFF) {
        /* Even if global level rejects, module filter may accept */
        if (g_rt_log_filter_count == 0 || !rt_log_check_module(level, module)) {
            return;
        }
    } else if (!rt_log_check_module(level, module)) {
        return;
    }

    rt_log_output(level, module, message, NULL);
}

/* ========================================================================== */
/* Phase 4.4: Advanced logging features                                       */
/* ========================================================================== */

/* Internal helper: parse a single level string */
static int rt_parse_level(const char* s, int len) {
    if (len == 5 && strncmp(s, "trace", 5) == 0)
        return RT_LOG_TRACE;
    if (len == 5 && strncmp(s, "debug", 5) == 0)
        return RT_LOG_DEBUG;
    if (len == 4 && strncmp(s, "info", 4) == 0)
        return RT_LOG_INFO;
    if (len == 4 && strncmp(s, "warn", 4) == 0)
        return RT_LOG_WARN;
    if (len == 5 && strncmp(s, "error", 5) == 0)
        return RT_LOG_ERROR;
    if (len == 5 && strncmp(s, "fatal", 5) == 0)
        return RT_LOG_FATAL;
    if (len == 3 && strncmp(s, "off", 3) == 0)
        return RT_LOG_OFF;
    /* Try uppercase */
    if (len == 5 && strncmp(s, "TRACE", 5) == 0)
        return RT_LOG_TRACE;
    if (len == 5 && strncmp(s, "DEBUG", 5) == 0)
        return RT_LOG_DEBUG;
    if (len == 4 && strncmp(s, "INFO", 4) == 0)
        return RT_LOG_INFO;
    if (len == 4 && strncmp(s, "WARN", 4) == 0)
        return RT_LOG_WARN;
    if (len == 5 && strncmp(s, "ERROR", 5) == 0)
        return RT_LOG_ERROR;
    if (len == 5 && strncmp(s, "FATAL", 5) == 0)
        return RT_LOG_FATAL;
    if (len == 3 && strncmp(s, "OFF", 3) == 0)
        return RT_LOG_OFF;
    return -1; /* unknown */
}

void rt_log_set_filter(const char* filter_spec) {
    g_rt_log_filter_count = 0;
    g_rt_log_filter_default = -1;

    if (!filter_spec || !filter_spec[0])
        return;

    const char* p = filter_spec;
    while (*p) {
        /* Find the next comma or end */
        const char* comma = strchr(p, ',');
        int token_len = comma ? (int)(comma - p) : (int)strlen(p);

        /* Find '=' within token */
        const char* eq = NULL;
        for (int i = 0; i < token_len; i++) {
            if (p[i] == '=') {
                eq = p + i;
                break;
            }
        }

        if (eq) {
            int mod_len = (int)(eq - p);
            const char* lvl_str = eq + 1;
            int lvl_len = token_len - mod_len - 1;
            int level = rt_parse_level(lvl_str, lvl_len);
            if (level < 0)
                level = RT_LOG_INFO; /* default if unrecognized */

            if (mod_len == 1 && p[0] == '*') {
                /* Wildcard default */
                g_rt_log_filter_default = level;
            } else if (g_rt_log_filter_count < RT_LOG_MAX_FILTERS) {
                int copy_len = mod_len < 63 ? mod_len : 63;
                memcpy(g_rt_log_filters[g_rt_log_filter_count].module, p, (size_t)copy_len);
                g_rt_log_filters[g_rt_log_filter_count].module[copy_len] = '\0';
                g_rt_log_filters[g_rt_log_filter_count].level = level;
                g_rt_log_filter_count++;
            }
        } else if (token_len > 0) {
            /* Bare module name — set to TRACE */
            if (g_rt_log_filter_count < RT_LOG_MAX_FILTERS) {
                int copy_len = token_len < 63 ? token_len : 63;
                memcpy(g_rt_log_filters[g_rt_log_filter_count].module, p, (size_t)copy_len);
                g_rt_log_filters[g_rt_log_filter_count].module[copy_len] = '\0';
                g_rt_log_filters[g_rt_log_filter_count].level = RT_LOG_TRACE;
                g_rt_log_filter_count++;
            }
        }

        p = comma ? comma + 1 : p + token_len;
    }

    /* If a wildcard default was set, update the global level to the
       minimum across all configured levels for fast-path filtering */
    if (g_rt_log_filter_default >= 0) {
        int min_level = g_rt_log_filter_default;
        for (int i = 0; i < g_rt_log_filter_count; i++) {
            if (g_rt_log_filters[i].level < min_level) {
                min_level = g_rt_log_filters[i].level;
            }
        }
        g_rt_log_level = min_level;
    }
}

int rt_log_module_enabled(int level, const char* module) {
    return rt_log_check_module(level, module);
}

void rt_log_structured(int level, const char* module, const char* message, const char* fields) {
    /* Fast-path: global level check */
    if (level >= RT_LOG_OFF)
        return;
    if (!rt_log_check_module(level, module))
        return;

    rt_log_output(level, module, message, fields);
}

void rt_log_set_format(int format) {
    if (format >= 0 && format <= 2) {
        g_rt_log_format = format;
    }
}

int rt_log_get_format(void) {
    return g_rt_log_format;
}

int rt_log_open_file(const char* path) {
    if (g_rt_log_file) {
        fclose(g_rt_log_file);
        g_rt_log_file = NULL;
    }
    if (!path || !path[0])
        return 0;
    g_rt_log_file = fopen(path, "a");
    return g_rt_log_file ? 1 : 0;
}

void rt_log_close_file(void) {
    if (g_rt_log_file) {
        fflush(g_rt_log_file);
        fclose(g_rt_log_file);
        g_rt_log_file = NULL;
    }
}

int rt_log_init_from_env(void) {
    const char* env = getenv("TML_LOG");
    if (!env || !env[0])
        return 0;

    /* Check if it looks like a simple level name (no '=' or ',') */
    if (!strchr(env, '=') && !strchr(env, ',')) {
        int level = rt_parse_level(env, (int)strlen(env));
        if (level >= 0) {
            rt_log_set_level(level);
            return 1;
        }
    }

    /* Otherwise treat as a filter spec */
    rt_log_set_filter(env);
    return 1;
}
