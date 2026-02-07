/**
 * @file log.h
 * @brief TML Runtime - C Logging API
 *
 * Provides structured logging functions for the TML C runtime.
 * These functions output to stderr by default and can be configured
 * to route through the C++ Logger when running inside the compiler.
 *
 * ## Log Levels
 *
 * | Level | Value | Description                    |
 * |-------|-------|--------------------------------|
 * | TRACE |   0   | Fine-grained internal tracing  |
 * | DEBUG |   1   | Debugging information          |
 * | INFO  |   2   | General informational messages |
 * | WARN  |   3   | Potential issues               |
 * | ERROR |   4   | Recoverable errors             |
 * | FATAL |   5   | Unrecoverable errors           |
 * | OFF   |   6   | Disables all logging           |
 *
 * ## Usage
 *
 * ```c
 * rt_log(RT_LOG_WARN, "memory", "Leak detected: %zu bytes", size);
 * rt_log(RT_LOG_FATAL, "runtime", "Panic: %s", message);
 * RT_WARN("memory", "Leak detected: %zu bytes", size);
 * ```
 */

#ifndef RT_LOG_H
#define RT_LOG_H

#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log level constants matching C++ LogLevel enum */
#define RT_LOG_TRACE 0
#define RT_LOG_DEBUG 1
#define RT_LOG_INFO 2
#define RT_LOG_WARN 3
#define RT_LOG_ERROR 4
#define RT_LOG_FATAL 5
#define RT_LOG_OFF 6

/**
 * @brief Log a formatted message at the given level.
 *
 * @param level   Log level (RT_LOG_TRACE through RT_LOG_FATAL)
 * @param module  Module tag (e.g., "runtime", "memory", "text")
 * @param fmt     printf-style format string
 * @param ...     Format arguments
 */
void rt_log(int level, const char* module, const char* fmt, ...);

/**
 * @brief Log a formatted message with va_list.
 *
 * @param level   Log level
 * @param module  Module tag
 * @param fmt     printf-style format string
 * @param args    va_list arguments
 */
void rt_log_va(int level, const char* module, const char* fmt, va_list args);

/**
 * @brief Set the minimum log level for the C runtime logger.
 *
 * Messages below this level will be silently discarded.
 * Default is RT_LOG_WARN.
 *
 * @param level  Minimum log level (RT_LOG_TRACE through RT_LOG_OFF)
 */
void rt_log_set_level(int level);

/**
 * @brief Get the current minimum log level.
 *
 * @return Current minimum log level
 */
int rt_log_get_level(void);

/**
 * @brief Check if a message at the given level would be logged.
 *
 * Use this for fast-path filtering before constructing expensive messages.
 *
 * @param level  Log level to check
 * @return 1 if the level would be logged, 0 otherwise
 */
int rt_log_enabled(int level);

/**
 * @brief Type for custom log output callback.
 *
 * When set, log messages are routed through this callback instead of
 * going directly to stderr. The C++ Logger uses this to capture
 * runtime log messages into the unified log stream.
 *
 * @param level   Log level
 * @param module  Module tag string
 * @param message Formatted message string
 */
typedef void (*rt_log_callback_t)(int level, const char* module, const char* message);

/**
 * @brief Log a pre-formatted message (no printf formatting).
 *
 * This is the non-variadic entry point used by TML programs.
 * The message string is passed directly without printf-style formatting.
 *
 * @param level   Log level (RT_LOG_TRACE through RT_LOG_FATAL)
 * @param module  Module tag (e.g., "app", "server")
 * @param message Pre-formatted message string
 */
void rt_log_msg(int level, const char* module, const char* message);

/**
 * @brief Set a custom log callback.
 *
 * When a callback is set, all runtime log messages are routed through
 * the callback instead of being written directly to stderr.
 * Pass NULL to restore direct stderr output.
 *
 * @param callback  Custom log callback, or NULL for default stderr output
 */
void rt_log_set_callback(rt_log_callback_t callback);

/* ========================================================================== */
/* Phase 4.4: Advanced logging features for TML programs                      */
/* ========================================================================== */

/**
 * @brief Set a module-level filter specification.
 *
 * Format: "module1=level,module2=level,*=default_level"
 * Example: "server=debug,db=trace,*=warn"
 *
 * @param filter_spec  Filter specification string (copied internally)
 */
void rt_log_set_filter(const char* filter_spec);

/**
 * @brief Check if a message at the given level from a specific module would be logged.
 *
 * Respects both the global level and per-module filter overrides.
 *
 * @param level   Log level to check
 * @param module  Module tag to check
 * @return 1 if the message would be logged, 0 otherwise
 */
int rt_log_module_enabled(int level, const char* module);

/**
 * @brief Log a structured message with key-value fields.
 *
 * Outputs a message along with structured fields in "key=value" format.
 * When output format is JSON, fields become JSON properties.
 *
 * @param level    Log level
 * @param module   Module tag
 * @param message  The log message
 * @param fields   Semicolon-separated key=value pairs (e.g., "method=GET;status=200;ms=42")
 */
void rt_log_structured(int level, const char* module, const char* message, const char* fields);

/**
 * @brief Set the log output format.
 *
 * @param format  0 = text (default), 1 = JSON, 2 = compact
 */
void rt_log_set_format(int format);

/**
 * @brief Get the current log output format.
 *
 * @return Current format: 0 = text, 1 = JSON, 2 = compact
 */
int rt_log_get_format(void);

/**
 * @brief Open a file sink for logging.
 *
 * All subsequent log messages will also be written to the given file.
 * The file is opened in append mode. Call rt_log_close_file() to close.
 *
 * @param path  File path to write logs to
 * @return 1 on success, 0 on failure
 */
int rt_log_open_file(const char* path);

/**
 * @brief Close the file sink, if one is open.
 */
void rt_log_close_file(void);

/**
 * @brief Configure logging from the TML_LOG environment variable.
 *
 * Reads TML_LOG and applies it as a filter spec. If TML_LOG is not set,
 * does nothing. CLI flags take precedence if already configured.
 *
 * @return 1 if TML_LOG was found and applied, 0 otherwise
 */
int rt_log_init_from_env(void);

/* Convenience macros for common log levels */
#define RT_TRACE(module, fmt, ...) rt_log(RT_LOG_TRACE, module, fmt, ##__VA_ARGS__)
#define RT_DEBUG(module, fmt, ...) rt_log(RT_LOG_DEBUG, module, fmt, ##__VA_ARGS__)
#define RT_INFO(module, fmt, ...) rt_log(RT_LOG_INFO, module, fmt, ##__VA_ARGS__)
#define RT_WARN(module, fmt, ...) rt_log(RT_LOG_WARN, module, fmt, ##__VA_ARGS__)
#define RT_ERROR(module, fmt, ...) rt_log(RT_LOG_ERROR, module, fmt, ##__VA_ARGS__)
#define RT_FATAL(module, fmt, ...) rt_log(RT_LOG_FATAL, module, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* RT_LOG_H */
