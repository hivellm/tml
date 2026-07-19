/**
 * @file essential.c
 * @brief TML Runtime - Essential Functions Implementation
 *
 * Core runtime implementation for the TML language. This file provides
 * the fundamental I/O functions and panic handling that all TML programs
 * depend on.
 *
 * ## Components
 *
 * - **I/O Functions**: `print`, `println`, `panic`, `assert_tml`
 * - **Type-specific print**: `print_i32`, `print_i64`, `print_f32`, etc.
 * - **Panic catching**: setjmp/longjmp based panic interception for tests
 *
 * ## Panic Catching
 *
 * The panic catching mechanism uses setjmp/longjmp to intercept panic calls
 * during test execution. This allows `@should_panic` tests to verify that
 * code correctly panics without terminating the test runner.
 *
 * The callback approach (via `tml_run_should_panic`) ensures that setjmp
 * remains on the stack while the test runs, which is required for longjmp
 * to work correctly.
 *
 * @see essential.h for function declarations
 */

// Extracted sections (private statics, no linkage changes):
//   essential_cpuid.c   — CPUID/XGETBV feature detection
//   essential_tracy.c   — Tracy profiler FFI bindings
//   essential_utf8.c    — UTF-8 byte-to-string encoding
//   essential_random.c  — Random seed generation (SplitMix64)
//   essential_ffi.c     — tml_str_from_cstr, tml_free
//   memory/str_free.c   — tml_str_free + PE image range checking
//
// Group C statics (panic/crash/VEH/I/O/test harness) MUST stay in this
// translation unit — static lib model means extern globals get duplicated
// across linked binaries, breaking setjmp/longjmp.

#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h> // _exit() on POSIX
#else
#include <fcntl.h> // _O_BINARY
#include <io.h>    // _setmode, _fileno
#endif

// Export macro for DLL visibility
#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
// Include Windows headers for SEH
#define WIN32_LEAN_AND_MEAN
#include <malloc.h> // _resetstkoflw()
#include <windows.h>
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// Backtrace support for panic handlers
#include "../diagnostics/backtrace.h"

// Structured logging API
#include "../diagnostics/log.h"

// ============================================================================
// Backtrace Configuration
// ============================================================================

/** @brief Flag to enable backtrace printing on panic (controlled by --backtrace flag). */
static int32_t tml_backtrace_on_panic = 0;

/** @brief Flag to prevent recursive backtrace during panic. */
static int32_t tml_in_panic = 0;

/**
 * @brief Enables backtrace printing on panic.
 *
 * Called by the runtime when the --backtrace flag is set.
 */
TML_EXPORT void tml_enable_backtrace_on_panic(void) {
    tml_backtrace_on_panic = 1;
}

/**
 * @brief Disables backtrace printing on panic.
 */
TML_EXPORT void tml_disable_backtrace_on_panic(void) {
    tml_backtrace_on_panic = 0;
}

/**
 * @brief Returns whether the current thread is panicking.
 * @return 1 if panicking, 0 otherwise
 */
TML_EXPORT int32_t tml_thread_panicking(void) {
    return tml_in_panic;
}

// ============================================================================
// Output Suppression (for test runner to suppress test output)
// ============================================================================

/** @brief Flag indicating whether output should be suppressed. */
static int32_t tml_suppress_output = 0;

/**
 * @brief Sets the output suppression flag.
 *
 * When set to non-zero, print/println functions will not produce output.
 * This is used by the test runner to suppress test output when not in
 * verbose mode.
 *
 * @param suppress Non-zero to suppress output, zero to enable output.
 */
TML_EXPORT void tml_set_output_suppressed(int32_t suppress) {
    tml_suppress_output = suppress;
    // Ensure immediate effect by flushing
    fflush(stdout);
    fflush(stderr);
}

/**
 * @brief Gets the current output suppression state.
 *
 * @return Non-zero if output is suppressed, zero otherwise.
 */
TML_EXPORT int32_t tml_get_output_suppressed(void) {
    return tml_suppress_output;
}

// ============================================================================
// Panic Catching State (for @should_panic tests)
// ============================================================================

/** @brief Jump buffer for panic catching via setjmp/longjmp. */
static jmp_buf tml_panic_jmp_buf;

/** @brief Flag indicating whether panic catching is active. */
static int32_t tml_catching_panic = 0;

/**
 * @brief Flag to temporarily suppress VEH interception.
 *
 * Set to 1 by code that uses its own SEH __try/__except (e.g., backtrace.c)
 * to prevent the VEH handler from intercepting exceptions that the SEH
 * handler should catch instead. Exported so runtime components can set it.
 */
TML_EXPORT volatile int32_t tml_veh_suppressed = 0;

/** @brief Buffer to store the panic message when caught. */
static char tml_panic_msg[1024] = {0};

/** @brief Buffer to store backtrace when panic is caught (for DLL test mode). */
static char tml_panic_backtrace[8192] = {0};

/** @brief Buffer to store JSON-formatted backtrace when panic is caught. */
static char tml_panic_backtrace_json[16384] = {0};

// ============================================================================
// Panic Hook System (user-configurable panic handler)
// ============================================================================

/** @brief Type for user-installed panic hook: void(*)(const char* message) */
typedef void (*tml_panic_hook_fn)(const char*);

/** @brief The currently installed panic hook, or NULL for default behavior. */
static tml_panic_hook_fn tml_panic_hook = NULL;

/**
 * @brief Install a custom panic hook.
 *
 * The hook is called with the panic message before the process terminates
 * (or before longjmp in catch_unwind context). Pass NULL to clear the hook.
 */
TML_EXPORT void tml_set_panic_hook(void* hook) {
    tml_panic_hook = (tml_panic_hook_fn)hook;
}

/**
 * @brief Get the current panic hook.
 * @return The current hook function pointer, or NULL if none is installed.
 */
TML_EXPORT void* tml_get_panic_hook(void) {
    return (void*)tml_panic_hook;
}

/**
 * @brief General-purpose catch_unwind: run a callback, catching any panics.
 *
 * Uses setjmp/longjmp to intercept panics. Thread-safe as long as only
 * one catch_unwind is active per thread.
 *
 * The callback receives a context pointer (for closures, this is the captures
 * pointer; for plain functions, pass NULL).
 *
 * @param fn_ptr Plain function pointer: void(*)(void). No captures.
 * @return 0 if callback completed normally, 1 if it panicked.
 *         After panic, call tml_get_panic_message() for the message.
 */
TML_EXPORT int32_t tml_catch_unwind_fn(void (*fn_ptr)(void)) {
    tml_panic_msg[0] = '\0';
    int32_t prev_catching = tml_catching_panic;
    jmp_buf prev_buf;
    // Save previous jmp_buf in case of nested catch_unwind
    memcpy(&prev_buf, &tml_panic_jmp_buf, sizeof(jmp_buf));

    tml_catching_panic = 1;
    if (setjmp(tml_panic_jmp_buf) == 0) {
        fn_ptr();
        // Restore previous state
        tml_catching_panic = prev_catching;
        memcpy(&tml_panic_jmp_buf, &prev_buf, sizeof(jmp_buf));
        return 0; // No panic
    } else {
        // Panic was caught via longjmp
        tml_catching_panic = prev_catching;
        memcpy(&tml_panic_jmp_buf, &prev_buf, sizeof(jmp_buf));
        return 1; // Panicked
    }
}

// ============================================================================
// Test Crash Context (set by C++ test runner, read by VEH handler)
// ============================================================================

/** @brief Current test name for crash context reporting. */
static char tml_crash_ctx_test[256] = {0};
/** @brief Current test file for crash context reporting. */
static char tml_crash_ctx_file[512] = {0};
/** @brief Current suite name for crash context reporting. */
static char tml_crash_ctx_suite[256] = {0};

/**
 * @brief Set crash context before running a test.
 *
 * Called by the C++ test runner before each test so that if the test crashes,
 * the VEH handler can include the test name/file in the crash message.
 */
TML_EXPORT void tml_set_test_crash_context(const char* test_name, const char* test_file,
                                           const char* suite_name) {
    if (test_name) {
        strncpy(tml_crash_ctx_test, test_name, sizeof(tml_crash_ctx_test) - 1);
        tml_crash_ctx_test[sizeof(tml_crash_ctx_test) - 1] = '\0';
    } else {
        tml_crash_ctx_test[0] = '\0';
    }
    if (test_file) {
        strncpy(tml_crash_ctx_file, test_file, sizeof(tml_crash_ctx_file) - 1);
        tml_crash_ctx_file[sizeof(tml_crash_ctx_file) - 1] = '\0';
    } else {
        tml_crash_ctx_file[0] = '\0';
    }
    if (suite_name) {
        strncpy(tml_crash_ctx_suite, suite_name, sizeof(tml_crash_ctx_suite) - 1);
        tml_crash_ctx_suite[sizeof(tml_crash_ctx_suite) - 1] = '\0';
    } else {
        tml_crash_ctx_suite[0] = '\0';
    }
}

/** @brief Clear crash context after test completes. */
TML_EXPORT void tml_clear_test_crash_context(void) {
    tml_crash_ctx_test[0] = '\0';
    tml_crash_ctx_file[0] = '\0';
    tml_crash_ctx_suite[0] = '\0';
}

// ============================================================================
// Crash Severity (set by VEH handler, read by C++ test runner)
// ============================================================================

/**
 * @brief Crash severity levels for recovery policy decisions.
 *
 * The VEH handler classifies each crash and the C++ test runner uses the
 * severity to decide whether to continue the suite or abort.
 */
enum TmlCrashSeverity {
    CRASH_NONE = 0,            /**< No crash occurred */
    CRASH_NULL_DEREF = 1,      /**< AV read at low address (<0x10000) — safe to continue */
    CRASH_ARITHMETIC = 2,      /**< Integer/float divide by zero — safe to continue */
    CRASH_USE_AFTER_FREE = 3,  /**< AV read at high address — potential corruption */
    CRASH_WRITE_VIOLATION = 4, /**< AV write — memory corruption likely */
    CRASH_DEP_VIOLATION = 5,   /**< AV execute (DEP) — code corruption */
    CRASH_STACK_OVERFLOW = 6,  /**< Stack overflow — guard page consumed */
    CRASH_HEAP_CORRUPTION = 7, /**< Heap corruption (0xC0000374) */
    CRASH_UNKNOWN = 8          /**< Everything else — assume worst */
};

/** @brief Crash severity from most recent crash (set by VEH handler). */
static volatile int32_t tml_crash_severity = CRASH_NONE;

/** @brief Whether the current suite should be aborted after a dangerous crash. */
static volatile int32_t tml_crash_abort_suite = 0;

#ifdef _WIN32
/** @brief Static buffer for crash messages (avoids stack allocation in handler). */
static char tml_crash_msg_buf[1024] = {0};

/** @brief Raw backtrace frames captured by VEH handler (no symbol resolution). */
static void* tml_crash_bt_frames[32] = {0};

/** @brief Number of valid frames in tml_crash_bt_frames. */
static int32_t tml_crash_bt_count = 0;
#endif

/** @brief Get the crash severity from the most recent crash. */
TML_EXPORT int32_t tml_get_crash_severity(void) {
    return tml_crash_severity;
}

/** @brief Check if the current suite should be aborted. */
TML_EXPORT int32_t tml_get_crash_abort_suite(void) {
    return tml_crash_abort_suite;
}

/** @brief Clear crash severity (called before each test). */
TML_EXPORT void tml_clear_crash_severity(void) {
    tml_crash_severity = CRASH_NONE;
    tml_crash_abort_suite = 0;
#ifdef _WIN32
    tml_crash_bt_count = 0;
#endif
}

/** @brief Get raw backtrace frames from last crash. */
TML_EXPORT int32_t tml_get_crash_backtrace(void** out_frames, int32_t max_frames) {
#ifdef _WIN32
    int32_t count = tml_crash_bt_count < max_frames ? tml_crash_bt_count : max_frames;
    for (int32_t i = 0; i < count; i++) {
        out_frames[i] = tml_crash_bt_frames[i];
    }
    return count;
#else
    (void)out_frames;
    (void)max_frames;
    return 0;
#endif
}

// ============================================================================
// I/O Functions
// ============================================================================

/**
 * @brief Prints a string to stdout without a newline.
 *
 * Maps to TML's `print(message: Str) -> Unit` builtin.
 * Output is suppressed when tml_suppress_output is set.
 *
 * @param message The null-terminated string to print. NULL is ignored.
 */
TML_EXPORT void print(const char* message) {
    if (tml_suppress_output)
        return;
    if (message)
        printf("%s", message);
}

/**
 * @brief Prints a string to stdout followed by a newline.
 *
 * Maps to TML's `println(message: Str) -> Unit` builtin.
 * Output is suppressed when tml_suppress_output is set.
 *
 * @param message The null-terminated string to print. NULL prints only newline.
 */
TML_EXPORT void println(const char* message) {
    if (tml_suppress_output)
        return;
    if (message)
        printf("%s\n", message);
    else
        printf("\n");
}

/**
 * @brief Prints a string to stderr without a newline.
 *
 * Maps to TML's `eprint(message: Str) -> Unit` builtin.
 * Always written to stderr, not suppressed by tml_suppress_output.
 *
 * @param message The null-terminated string to print. NULL is ignored.
 */
TML_EXPORT void eprint(const char* message) {
    if (message)
        fprintf(stderr, "%s", message);
}

/**
 * @brief Prints a string to stderr followed by a newline.
 *
 * Maps to TML's `eprintln(message: Str) -> Unit` builtin.
 * Always written to stderr, not suppressed by tml_suppress_output.
 *
 * @param message The null-terminated string to print. NULL prints only newline.
 */
TML_EXPORT void eprintln(const char* message) {
    if (message)
        fprintf(stderr, "%s\n", message);
    else
        fprintf(stderr, "\n");
}

/**
 * @brief Write raw bytes from a buffer to stdout.
 *
 * Used by the TML frontend binary to emit serialized AST bytes to the
 * C++ parent process via the subprocess stdout pipe.
 *
 * @param buf_ptr Pointer to the byte buffer (as I64).
 * @param count   Number of bytes to write.
 * @return Number of bytes actually written.
 */
TML_EXPORT int64_t tml_write_stdout_bytes(int64_t buf_ptr, int64_t count) {
    if (!buf_ptr || count <= 0)
        return 0;
#ifdef _WIN32
    /* On Windows, stdout is text-mode by default and translates \n (0x0A) to
     * \r\n (0x0D 0x0A) for BOTH fwrite() and _write(). Binary AST data may
     * contain 0x0A bytes anywhere (e.g. varint for string length 10 =
     * "Visibility"), so we switch stdout to binary mode once and then write.
     * _setmode persists for the lifetime of the process — that's intentional:
     * main_frontend.exe writes only binary AST to stdout, no text. */
    static int stdout_binary_mode_set = 0;
    if (!stdout_binary_mode_set) {
        _setmode(_fileno(stdout), _O_BINARY);
        stdout_binary_mode_set = 1;
    }
#endif
    size_t written = fwrite((void*)buf_ptr, 1, (size_t)count, stdout);
    fflush(stdout);
    return (int64_t)written;
}

/**
 * @brief Read an entire file into a TML string.
 *
 * Opens the file at `path`, reads all bytes, and returns a heap-allocated
 * TML string (null-terminated, owned by the caller). Returns an empty string
 * on error (file not found, permission denied, etc.).
 *
 * @param path  Null-terminated C string with the file path.
 * @return      Pointer to a heap-allocated null-terminated string (TML Str).
 */
TML_EXPORT const char* tml_read_file_to_string(const char* path) {
    if (!path) {
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    FILE* f = fopen(path, "rb");
    if (!f) {
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        char* empty = (char*)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/**
 * @brief Terminates the program with an error message.
 *
 * If panic catching is enabled (inside `tml_run_should_panic`), saves
 * the message and performs a longjmp back to the test harness. Otherwise,
 * prints the message to stderr and terminates via `exit(1)`.
 *
 * Maps to TML's `panic(message: Str) -> Never` builtin.
 *
 * @param message The panic message. NULL is printed as "(null)".
 */
TML_EXPORT void panic(const char* message) {
    // Call user-installed panic hook if present
    if (tml_panic_hook) {
        tml_panic_hook(message ? message : "(null)");
    }

    // If we're in panic catching mode, save the message and longjmp back
    if (tml_catching_panic) {
        if (message) {
            snprintf(tml_panic_msg, sizeof(tml_panic_msg), "%s", message);
        } else {
            tml_panic_msg[0] = '\0';
        }
        // Capture backtrace before longjmp (stack will be unwound)
        tml_panic_backtrace[0] = '\0';
        tml_panic_backtrace_json[0] = '\0';
        if (tml_backtrace_on_panic) {
            Backtrace* bt = backtrace_capture_full(2); // Skip internal frames
            if (bt) {
                backtrace_resolve_all(bt);
                char* formatted = backtrace_format(bt);
                if (formatted) {
                    snprintf(tml_panic_backtrace, sizeof(tml_panic_backtrace), "%s", formatted);
                    free(formatted);
                }
                char* json = backtrace_format_json(bt);
                if (json) {
                    snprintf(tml_panic_backtrace_json, sizeof(tml_panic_backtrace_json), "%s",
                             json);
                    free(json);
                }
                backtrace_free(bt);
            }
        }
        longjmp(tml_panic_jmp_buf, 1);
    }

    // Normal panic behavior - print message and exit
    RT_FATAL("runtime", "panic: %s", message ? message : "(null)");

    // Print backtrace if enabled and not already in panic (avoid recursion)
    if (tml_backtrace_on_panic && !tml_in_panic) {
        tml_in_panic = 1;
        RT_ERROR("runtime", "Backtrace:");
        backtrace_print(2); // Skip panic() and backtrace_print()
        tml_in_panic = 0;
    }

    exit(1);
}

// assert_tml (2-arg) — REMOVED (Phase 49, dead: codegen only emits assert_tml_loc)

/**
 * @brief Asserts a condition with file and line information.
 *
 * This version is used when the compiler can provide source location.
 * Provides much better error messages for debugging test failures.
 *
 * @param condition The condition to check.
 * @param message The assertion message.
 * @param file The source file name.
 * @param line The line number.
 */
TML_EXPORT void assert_tml_loc(int32_t condition, const char* message, const char* file,
                               int32_t line) {
    if (!condition) {
        // Build assertion failed message with location
        static char assert_msg[2048];
        snprintf(assert_msg, sizeof(assert_msg), "assertion failed at %s:%d: %s",
                 file ? file : "<unknown>", line, message ? message : "(no message)");

        // Use panic mechanism if catching is enabled (DLL test context)
        if (tml_catching_panic) {
            snprintf(tml_panic_msg, sizeof(tml_panic_msg), "%s", assert_msg);
            // Capture backtrace before longjmp (stack will be unwound)
            tml_panic_backtrace[0] = '\0';
            tml_panic_backtrace_json[0] = '\0';
            if (tml_backtrace_on_panic) {
                Backtrace* bt = backtrace_capture_full(3); // Skip internal frames
                if (bt) {
                    backtrace_resolve_all(bt);
                    char* formatted = backtrace_format(bt);
                    if (formatted) {
                        snprintf(tml_panic_backtrace, sizeof(tml_panic_backtrace), "%s", formatted);
                        free(formatted);
                    }
                    char* json = backtrace_format_json(bt);
                    if (json) {
                        snprintf(tml_panic_backtrace_json, sizeof(tml_panic_backtrace_json), "%s",
                                 json);
                        free(json);
                    }
                    backtrace_free(bt);
                }
            }
            longjmp(tml_panic_jmp_buf, 1);
        }

        // Normal mode - print and exit
        RT_FATAL("runtime", "%s", assert_msg);

        // Print backtrace if enabled
        if (tml_backtrace_on_panic && !tml_in_panic) {
            tml_in_panic = 1;
            RT_ERROR("runtime", "Backtrace:");
            backtrace_print(2);
            tml_in_panic = 0;
        }

        exit(1);
    }
}

// ============================================================================
// Type-Specific Print Variants (for polymorphic print)
// ============================================================================

/** @brief Prints a 32-bit signed integer to stdout. */
TML_EXPORT void print_i32(int32_t n) {
    if (tml_suppress_output)
        return;
    printf("%d", n);
}

/** @brief Prints a 64-bit signed integer to stdout. */
TML_EXPORT void print_i64(int64_t n) {
    if (tml_suppress_output)
        return;
    printf("%lld", (long long)n);
}

// print_f32 — REMOVED (Phase 37, dead code: no declare in runtime.cpp)

/** @brief Prints a 64-bit floating point number to stdout. */
TML_EXPORT void print_f64(double n) {
    if (tml_suppress_output)
        return;
    printf("%g", n);
}

/** @brief Prints a boolean as "true" or "false" to stdout. */
TML_EXPORT void print_bool(int32_t b) {
    if (tml_suppress_output)
        return;
    printf("%s", b ? "true" : "false");
}

// print_char — REMOVED (Phase 37, dead code: no declare in runtime.cpp)

// ============================================================================
// Float Formatting Functions (Phase 46: moved from inline IR in runtime.cpp)
// ============================================================================
// These wrap variadic snprintf, which TML cannot call directly via @extern.
// Called from TML lowlevel blocks in core::fmt::impls and core::fmt::float.
// Use mem_alloc instead of malloc so the memory tracker can track these
// allocations and tml_str_free can properly deregister them.
extern void* mem_alloc(int64_t);

/** @brief Formats a double using %g format. Returns heap-allocated string. */
TML_EXPORT char* f64_to_string(double val) {
    char* buf = (char*)mem_alloc(32);
    snprintf(buf, 32, "%g", val);
    return buf;
}

/** @brief Formats a float using %g format. Returns heap-allocated string. */
TML_EXPORT char* f32_to_string(float val) {
    char* buf = (char*)mem_alloc(32);
    snprintf(buf, 32, "%g", (double)val);
    return buf;
}

/** @brief Formats a double with fixed precision. Clamps precision to 0-20. */
TML_EXPORT char* f64_to_string_precision(double val, int64_t prec) {
    if (prec < 0)
        prec = 0;
    if (prec > 20)
        prec = 20;
    char* buf = (char*)mem_alloc(64);
    snprintf(buf, 64, "%.*f", (int)prec, val);
    return buf;
}

/** @brief Formats a float with fixed precision. Clamps precision to 0-20. */
TML_EXPORT char* f32_to_string_precision(float val, int64_t prec) {
    return f64_to_string_precision((double)val, prec);
}

/** @brief Formats a double in scientific notation (%e or %E). */
TML_EXPORT char* f64_to_exp_string(double val, int32_t uppercase) {
    char* buf = (char*)mem_alloc(32);
    snprintf(buf, 32, uppercase ? "%E" : "%e", val);
    return buf;
}

/** @brief Formats a float in scientific notation (%e or %E). */
TML_EXPORT char* f32_to_exp_string(float val, int32_t uppercase) {
    return f64_to_exp_string((double)val, uppercase);
}

// ============================================================================
// Panic Catching Functions (for @should_panic tests)
// ============================================================================

/** @brief Callback type for test functions (void -> void). */
typedef void (*tml_test_fn)(void);

/**
 * @brief Runs a test function that is expected to panic.
 *
 * This function uses the callback pattern to keep setjmp on the stack
 * while the test runs. The test function is passed as a function pointer
 * generated by LLVM IR.
 *
 * ## How it works:
 * 1. Sets up panic catching by setting `tml_catching_panic = 1`
 * 2. Calls setjmp to establish a return point
 * 3. Runs the test function
 * 4. If panic() is called, longjmp returns here with value 1
 * 5. Returns whether the test panicked
 *
 * @param test_fn The test function to execute.
 * @return 1 if the test panicked (success), 0 if it didn't (failure).
 */
/* Defined below, next to tml_run_test_with_catch: both share one catch
 * implementation so the panic/longjmp path has a single behaviour. */

/**
 * @brief Gets the last panic message.
 *
 * Returns the message from the most recently caught panic. Only valid
 * after `tml_run_should_panic` returns 1.
 *
 * @return The panic message string.
 */
TML_EXPORT const char* tml_get_panic_message(void) {
    return tml_panic_msg;
}

/**
 * @brief Describes the last test failure, escaped for embedding in JSON.
 *
 * The NDJSON test dispatcher writes this into the `"error"` field of a
 * `test_fail` event. Panic messages routinely contain `"` and (on Windows)
 * backslashes from source paths, which would produce unparseable NDJSON if
 * emitted raw, so `"`, `\` and control characters are escaped per RFC 8259.
 *
 * When the test failed without panicking (it simply returned a non-zero code)
 * there is no panic message, and a generic description is returned instead.
 *
 * @return A NUL-terminated JSON-safe failure description. Points to a static
 *         buffer; valid until the next call.
 */
TML_EXPORT const char* tml_test_error_json(void) {
    static char escaped[sizeof(tml_panic_msg) * 6 + 1];
    if (tml_panic_msg[0] == '\0') {
        return "non-zero exit";
    }
    size_t out = 0;
    for (size_t i = 0; tml_panic_msg[i] != '\0' && out + 6 < sizeof(escaped); ++i) {
        unsigned char c = (unsigned char)tml_panic_msg[i];
        if (c == '"' || c == '\\') {
            escaped[out++] = '\\';
            escaped[out++] = (char)c;
        } else if (c == '\n') {
            escaped[out++] = '\\';
            escaped[out++] = 'n';
        } else if (c == '\r') {
            escaped[out++] = '\\';
            escaped[out++] = 'r';
        } else if (c == '\t') {
            escaped[out++] = '\\';
            escaped[out++] = 't';
        } else if (c < 0x20) {
            out += (size_t)snprintf(escaped + out, sizeof(escaped) - out, "\\u%04X", c);
        } else {
            escaped[out++] = (char)c;
        }
    }
    escaped[out] = '\0';
    return escaped;
}

/**
 * @brief Gets the backtrace from the last caught panic.
 *
 * Returns the formatted backtrace string captured at the panic site.
 * Only valid after `tml_run_should_panic` returns 1 and if backtrace
 * was enabled via `tml_enable_backtrace_on_panic`.
 *
 * @return The backtrace string, or empty string if not available.
 */
TML_EXPORT const char* tml_get_panic_backtrace(void) {
    return tml_panic_backtrace;
}

TML_EXPORT const char* tml_get_panic_backtrace_json(void) {
    return tml_panic_backtrace_json;
}

/** @brief Callback type for test functions that return int (int -> void args). */
typedef int32_t (*tml_test_entry_fn)(void);

/** @brief Flag indicating test mode is active (for better error messages). */
static int32_t tml_test_mode = 0;

/** @brief Exit code from caught test failure. */
#ifndef _WIN32
static int32_t tml_test_exit_code = 0;
#endif

/**
 * @brief Enables test mode for better panic handling.
 *
 * When test mode is enabled, panics will be caught and logged
 * with more detailed information rather than immediately exiting.
 */
void tml_enable_test_mode(void) {
    tml_test_mode = 1;
}

/**
 * @brief Disables test mode.
 */
void tml_disable_test_mode(void) {
    tml_test_mode = 0;
}

// Unix signal handlers - not used on Windows (uses VEH instead)
#ifndef _WIN32
/** @brief Signal handler for catching crashes during tests. */
static void tml_signal_handler(int sig) {
    const char* sig_name = "unknown signal";
    switch (sig) {
    case SIGSEGV:
        sig_name = "SIGSEGV (Segmentation fault)";
        break;
    case SIGFPE:
        sig_name = "SIGFPE (Floating point exception)";
        break;
    case SIGILL:
        sig_name = "SIGILL (Illegal instruction)";
        break;
    case SIGBUS:
        sig_name = "SIGBUS (Bus error)";
        break;
    case SIGABRT:
        sig_name = "SIGABRT (Abort)";
        break;
    }

    // Store crash info and longjmp back if we're catching panics
    if (tml_catching_panic) {
        snprintf(tml_panic_msg, sizeof(tml_panic_msg), "CRASH: %s", sig_name);
        longjmp(tml_panic_jmp_buf, 2); // Use 2 to distinguish from panic
    }

    // Otherwise, print and exit
    RT_FATAL("runtime", "FATAL: %s", sig_name);
    fflush(stderr);
    _exit(128 + sig);
}

/** @brief Previous signal handlers (to restore after test). */
static void (*prev_sigsegv)(int) = NULL;
static void (*prev_sigfpe)(int) = NULL;
static void (*prev_sigill)(int) = NULL;
static void (*prev_sigabrt)(int) = NULL;
static void (*prev_sigbus)(int) = NULL;

/** @brief Install signal handlers for test crash catching. */
static void tml_install_signal_handlers(void) {
    prev_sigsegv = signal(SIGSEGV, tml_signal_handler);
    prev_sigfpe = signal(SIGFPE, tml_signal_handler);
    prev_sigill = signal(SIGILL, tml_signal_handler);
    prev_sigabrt = signal(SIGABRT, tml_signal_handler);
    prev_sigbus = signal(SIGBUS, tml_signal_handler);
}

/** @brief Restore previous signal handlers after test. */
static void tml_restore_signal_handlers(void) {
    signal(SIGSEGV, prev_sigsegv ? prev_sigsegv : SIG_DFL);
    signal(SIGFPE, prev_sigfpe ? prev_sigfpe : SIG_DFL);
    signal(SIGILL, prev_sigill ? prev_sigill : SIG_DFL);
    signal(SIGABRT, prev_sigabrt ? prev_sigabrt : SIG_DFL);
    signal(SIGBUS, prev_sigbus ? prev_sigbus : SIG_DFL);
}
#endif // _WIN32

// ============================================================================
// Windows Vectored Exception Handler (VEH)
// ============================================================================

#ifdef _WIN32
/**
 * @brief Get human-readable name for a Windows exception code.
 *
 * Exported so the C++ test runner can use it instead of maintaining
 * its own duplicate switch statement.
 */
TML_EXPORT const char* tml_get_exception_name(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "ACCESS_VIOLATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "INTEGER_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
        return "INTEGER_OVERFLOW";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return "FLOAT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "FLOAT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:
        return "FLOAT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "FLOAT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:
        return "FLOAT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:
        return "FLOAT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:
        return "FLOAT_UNDERFLOW";
    case EXCEPTION_STACK_OVERFLOW:
        return "STACK_OVERFLOW";
    case 0xC0000028:
        return "BAD_STACK";
    case 0xC0000374:
        return "HEAP_CORRUPTION";
    case 0xC0000409:
        return "STACK_BUFFER_OVERRUN";
    default:
        return "UNKNOWN_EXCEPTION";
    }
}

/** @brief VEH handler handle (returned by AddVectoredExceptionHandler). */
static PVOID tml_veh_handle = NULL;

/** @brief Reference count for VEH handler - thread-safe. */
static volatile LONG tml_filter_refcount = 0;

/**
 * @brief Classify crash severity and set abort-suite flag.
 *
 * Called by the VEH handler to determine recovery policy:
 * - CRASH_NULL_DEREF / CRASH_ARITHMETIC: safe to continue suite
 * - Everything else: abort suite (assume corrupted state)
 */
static void tml_classify_crash(DWORD code, EXCEPTION_RECORD* rec) {
    if (code == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
        ULONG_PTR op = rec->ExceptionInformation[0];   // 0=read, 1=write, 8=execute
        ULONG_PTR addr = rec->ExceptionInformation[1]; // faulting address

        if (op == 1) {
            tml_crash_severity = CRASH_WRITE_VIOLATION;
            tml_crash_abort_suite = 1;
        } else if (op == 8) {
            tml_crash_severity = CRASH_DEP_VIOLATION;
            tml_crash_abort_suite = 1;
        } else if (addr < 0x10000) {
            // Read at low address — null/near-null dereference, safe to continue
            tml_crash_severity = CRASH_NULL_DEREF;
            tml_crash_abort_suite = 0;
        } else {
            // Read at high address — use-after-free or wild pointer
            tml_crash_severity = CRASH_USE_AFTER_FREE;
            tml_crash_abort_suite = 1;
        }
    } else if (code == EXCEPTION_STACK_OVERFLOW) {
        tml_crash_severity = CRASH_STACK_OVERFLOW;
        tml_crash_abort_suite = 1;
    } else if (code == 0xC0000374) { // HEAP_CORRUPTION
        tml_crash_severity = CRASH_HEAP_CORRUPTION;
        tml_crash_abort_suite = 1;
    } else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_FLT_DIVIDE_BY_ZERO) {
        tml_crash_severity = CRASH_ARITHMETIC;
        tml_crash_abort_suite = 0;
    } else {
        tml_crash_severity = CRASH_UNKNOWN;
        tml_crash_abort_suite = 1;
    }
}

/**
 * @brief Saved CONTEXT from setjmp point for crash recovery.
 *
 * Populated by tml_save_recovery_context() when entering tml_run_test_with_catch.
 * Used by the VEH handler for severe crashes (corrupted stack) where longjmp and
 * SEH unwinding both fail. The VEH handler calls RtlRestoreContext() with this
 * saved context to resume at the setjmp point without any stack walking.
 */
#ifdef _M_X64
static CONTEXT tml_recovery_context;
static volatile int32_t tml_recovery_context_valid = 0;
#endif

#ifdef _WIN32
/**
 * @brief Vectored Exception Handler (VEH) for crash catching.
 *
 * Uses VEH instead of SetUnhandledExceptionFilter because:
 * 1. VEH runs BEFORE SEH frame unwinding, so the stack is still intact
 * 2. longjmp is safe from VEH because the stack hasn't been unwound
 * 3. SetUnhandledExceptionFilter runs AFTER SEH unwinding, making
 *    longjmp cause STATUS_BAD_STACK (0xC0000028)
 *
 * Only handles fatal exceptions (ACCESS_VIOLATION, etc.) and only when
 * tml_catching_panic is set (i.e., we're inside tml_run_test_with_catch).
 *
 * Enhanced diagnostics: fault address, read/write/execute, RIP, RSP, RBP,
 * raw backtrace frames (no symbol resolution — that's done post-recovery).
 */
static LONG WINAPI tml_veh_handler(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;

    // Only intercept fatal hardware exceptions when we're actively catching
    // and VEH is not suppressed (e.g., by backtrace.c's own SEH protection)
    if (!tml_catching_panic || tml_veh_suppressed) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Only handle fatal exceptions - skip C++ exceptions, breakpoints, etc.
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case 0xC0000374: // HEAP_CORRUPTION
    case 0xC0000409: // STACK_BUFFER_OVERRUN
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Classify crash severity and set abort-suite flag
    tml_classify_crash(code, info->ExceptionRecord);

    // For STACK_OVERFLOW: restore guard page FIRST, before any stack usage.
    // Without this, the next stack overflow kills the process without exception.
    if (code == EXCEPTION_STACK_OVERFLOW) {
        _resetstkoflw();
    }

    // Use static buffer for message formatting (not stack-allocated).
    // Critical for STACK_OVERFLOW where stack space is exhausted.
    char* msg = tml_crash_msg_buf;
    const int msg_size = (int)sizeof(tml_crash_msg_buf);
    int len = 0;

    // Header line with exception name
    len += snprintf(msg + len, msg_size - len, "\nCRASH: %s (0x%08lX)\n",
                    tml_get_exception_name(code), (unsigned long)code);

    // ACCESS_VIOLATION details: fault address + read/write/execute
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        ULONG_PTR op = info->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR fault_addr = info->ExceptionRecord->ExceptionInformation[1];
        const char* op_str = (op == 0) ? "READ" : (op == 1) ? "WRITE" : "EXECUTE";

        if (fault_addr < 0x10000) {
            len += snprintf(msg + len, msg_size - len, "  Fault:   0x%016llX (null pointer %s)\n",
                            (unsigned long long)fault_addr, op_str);
        } else {
            len += snprintf(msg + len, msg_size - len, "  Fault:   0x%016llX (%s)\n",
                            (unsigned long long)fault_addr, op_str);
        }
    }

    // Register dump: RIP (where it crashed), RSP, RBP
#ifdef _M_X64
    CONTEXT* ctx = info->ContextRecord;
    len += snprintf(msg + len, msg_size - len,
                    "  RIP:     0x%016llX\n"
                    "  RSP:     0x%016llX\n"
                    "  RBP:     0x%016llX\n",
                    (unsigned long long)ctx->Rip, (unsigned long long)ctx->Rsp,
                    (unsigned long long)ctx->Rbp);
#endif

    // Test context
    if (tml_crash_ctx_test[0]) {
        len +=
            snprintf(msg + len, msg_size - len,
                     "  Test:    %s\n"
                     "  File:    %s\n"
                     "  Suite:   %s\n",
                     tml_crash_ctx_test, tml_crash_ctx_file[0] ? tml_crash_ctx_file : "(unknown)",
                     tml_crash_ctx_suite[0] ? tml_crash_ctx_suite : "(unknown)");
    }

    // Capture raw backtrace frames (safe in VEH — no heap allocation).
    // Skip for STACK_OVERFLOW where CaptureStackBackTrace may not be safe.
    if (code != EXCEPTION_STACK_OVERFLOW) {
        tml_crash_bt_count = (int32_t)CaptureStackBackTrace(0, 32, tml_crash_bt_frames, NULL);

        if (tml_crash_bt_count > 0) {
            len += snprintf(msg + len, msg_size - len, "  Backtrace (%d frames):\n",
                            tml_crash_bt_count);
            // Print first 8 frames inline (keep message compact)
            int show = tml_crash_bt_count < 8 ? tml_crash_bt_count : 8;
            for (int i = 0; i < show && len < msg_size - 40; i++) {
                len += snprintf(msg + len, msg_size - len, "    [%d] 0x%016llX\n", i,
                                (unsigned long long)(uintptr_t)tml_crash_bt_frames[i]);
            }
            if (tml_crash_bt_count > 8) {
                len += snprintf(msg + len, msg_size - len, "    ... +%d more frames\n",
                                tml_crash_bt_count - 8);
            }
        }
    } else {
        tml_crash_bt_count = 0;
    }

    // Write to stderr using low-level API for reliability during crash
    DWORD written;
    HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);
    if (len > 0) {
        WriteFile(hStderr, msg, (DWORD)len, &written, NULL);
        FlushFileBuffers(hStderr);
    }

    // Store structured message in global for retrieval by C++ test runner
    if (tml_crash_ctx_test[0]) {
        if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
            ULONG_PTR op = info->ExceptionRecord->ExceptionInformation[0];
            ULONG_PTR fault_addr = info->ExceptionRecord->ExceptionInformation[1];
            const char* op_str = (op == 0) ? "READ" : (op == 1) ? "WRITE" : "EXECUTE";
            snprintf(tml_panic_msg, sizeof(tml_panic_msg),
                     "CRASH: %s (%s at 0x%016llX) in test \"%s\" [%s]",
                     tml_get_exception_name(code), op_str, (unsigned long long)fault_addr,
                     tml_crash_ctx_test, tml_crash_ctx_file[0] ? tml_crash_ctx_file : "?");
        } else {
            snprintf(tml_panic_msg, sizeof(tml_panic_msg),
                     "CRASH: %s (0x%08lX) in test \"%s\" [%s]", tml_get_exception_name(code),
                     (unsigned long)code, tml_crash_ctx_test,
                     tml_crash_ctx_file[0] ? tml_crash_ctx_file : "?");
        }
    } else {
        snprintf(tml_panic_msg, sizeof(tml_panic_msg), "CRASH: %s (0x%08lX)",
                 tml_get_exception_name(code), (unsigned long)code);
    }

    // Recovery strategy: ALWAYS recover via longjmp or context redirection.
    //
    // VEH runs BEFORE SEH unwinding, so the stack frames are still on the stack.
    // For the common case (null deref, arithmetic), the stack is fully intact and
    // longjmp works directly.
    //
    // For severe crashes (heap corruption, write violation, use-after-free with wild
    // pointers), the stack/RBP may be corrupted. longjmp on MSVC x64 internally calls
    // RtlUnwindEx which walks the stack — if the stack is corrupted, this crashes too.
    // So for severe crashes we redirect execution via CONTEXT modification:
    // set RIP to our recovery thunk and RSP to a known-good value from the jmp_buf,
    // then return EXCEPTION_CONTINUE_EXECUTION. The CPU resumes at the thunk without
    // any stack unwinding.
    //
    // Why this matters: previously we used EXCEPTION_CONTINUE_SEARCH for severe crashes,
    // relying on SEH __except to catch them. But SEH unwinding ALSO walks the stack,
    // and when the stack is corrupted (RBP=0x81), SEH unwinding itself crashes,
    // killing the entire process. Direct context redirection avoids all stack walking.
    tml_catching_panic = 0;

    if (!tml_crash_abort_suite) {
        // Recoverable: null deref, arithmetic — stack intact, longjmp is safe
        longjmp(tml_panic_jmp_buf, 2);
        // longjmp doesn't return
    }

#ifdef _M_X64
    // Severe crash: restore full CPU context from the setjmp point.
    //
    // RtlRestoreContext() restores ALL registers (RIP, RSP, RBP, etc.) from
    // the saved CONTEXT without any stack walking or SEH unwinding. This is
    // the safest recovery method when the stack is corrupted.
    //
    // After restoration, execution resumes at the RtlCaptureContext call site
    // in tml_run_test_with_catch, where we check tml_catching_panic==0 to
    // detect that we arrived via crash recovery rather than normal return.
    if (tml_recovery_context_valid) {
        RtlRestoreContext(&tml_recovery_context, NULL);
        // RtlRestoreContext does not return
    }
#endif

    // Fallback: try longjmp (may crash if stack is corrupted, but at this point
    // we have no better option — the process would die anyway)
    longjmp(tml_panic_jmp_buf, 2);
    return EXCEPTION_CONTINUE_SEARCH; // unreachable, but satisfies compiler
}

/** @brief Install the VEH crash handler (thread-safe, ref-counted). */
static void tml_install_exception_filter(void) {
    if (InterlockedIncrement(&tml_filter_refcount) == 1) {
        // First reference - install VEH as first handler (1 = first in chain)
        tml_veh_handle = AddVectoredExceptionHandler(1, tml_veh_handler);
        // Disable Windows Error Reporting popup
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        // Reserve extra stack space for STACK_OVERFLOW handler.
        // Without this, the handler may not have enough stack to even format a message.
        ULONG stack_guarantee = 65536; // 64 KB
        SetThreadStackGuarantee(&stack_guarantee);
    }
}

/** @brief Remove the VEH crash handler (thread-safe, ref-counted). */
static void tml_remove_exception_filter(void) {
    if (InterlockedDecrement(&tml_filter_refcount) == 0) {
        if (tml_veh_handle) {
            RemoveVectoredExceptionHandler(tml_veh_handle);
            tml_veh_handle = NULL;
        }
    }
}
#endif // inner _WIN32 (tml_veh_handler block, line 956)
#endif // _WIN32 (VEH declarations, line 849)

/**
 * @brief Runs a test function with panic and crash catching.
 *
 * This is used by the test harness to run individual tests while
 * catching panics AND crashes (SIGSEGV, etc.) and preserving error information.
 *
 * On Windows, uses Vectored Exception Handling (VEH) for proper crash catching.
 * On Unix, uses signal handlers with setjmp/longjmp.
 *
 * @param test_fn The test function to execute (returns i32).
 * @return The test result: 0 for success, -1 for panic, -2 for crash, or the test's return value.
 */
/// Helper: run test_fn wrapped in SEH __try/__except.
/// This is a separate function because MSVC forbids setjmp and __try in the same function.
/// Returns -2 on crash, or the test's return value on success.
// ============================================================================
// Per-test timeout enforcement
// ============================================================================

/** @brief Timeout in milliseconds for individual test functions. 0 = disabled. */
static int32_t tml_test_timeout_ms = 0;

#ifdef _WIN32

/* Persistent watchdog — one thread, reused for every test. */
static HANDLE g_wd_start_event = NULL; /* auto-reset: main signals "test starting" */
static HANDLE g_wd_done_event = NULL;  /* auto-reset: main signals "test finished" */
static volatile int32_t g_wd_timeout_ms = 0;
/* 1 while a test is running with the watchdog armed; see tml_watchdog_cancel. */
static volatile int32_t g_wd_armed = 0;

static DWORD WINAPI persistent_watchdog_fn(LPVOID param) {
    (void)param;
    for (;;) {
        /* Wait for main thread to start a new test. */
        DWORD r = WaitForSingleObject(g_wd_start_event, INFINITE);
        if (r != WAIT_OBJECT_0)
            return 0; /* handles closed → shutdown */

        int32_t tms = g_wd_timeout_ms;
        if (tms <= 0)
            continue; /* timeout disabled */

        /* Wait for main thread to signal test-done. */
        r = WaitForSingleObject(g_wd_done_event, (DWORD)tms);
        if (r == WAIT_TIMEOUT) {
            RT_FATAL("test", "TIMEOUT: test exceeded %dms limit — killed", tms);
            fflush(stderr);
            fflush(stdout);
            TerminateProcess(GetCurrentProcess(), 99);
        }
        /* WAIT_OBJECT_0 → test finished in time. Loop. */
    }
}

/**
 * @brief Set the per-test timeout in milliseconds.
 * Creates the persistent watchdog thread on first call (if timeout > 0).
 * Set to 0 to disable (default).
 */
TML_EXPORT void tml_set_test_timeout(int32_t timeout_ms) {
    tml_test_timeout_ms = timeout_ms;
    if (timeout_ms > 0 && g_wd_start_event == NULL) {
        g_wd_start_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        g_wd_done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (g_wd_start_event && g_wd_done_event) {
            HANDLE t = CreateThread(NULL, 0, persistent_watchdog_fn, NULL, 0, NULL);
            if (t)
                CloseHandle(t); /* detach — runs until process exits */
        }
    }
}

/**
 * @brief Run a test function with per-test timeout enforcement.
 * Overhead: two SetEvent calls (~microseconds). No thread creation per test.
 */
static int32_t tml_run_with_timeout(tml_test_entry_fn test_fn) {
    if (tml_test_timeout_ms <= 0 || !g_wd_start_event) {
        return test_fn();
    }
    g_wd_timeout_ms = tml_test_timeout_ms;
    g_wd_armed = 1;
    SetEvent(g_wd_start_event); /* start the watchdog timer */
    int32_t result = test_fn();
    g_wd_armed = 0;
    SetEvent(g_wd_done_event); /* cancel the watchdog timer */
    return result;
}

/**
 * @brief Cancel an armed watchdog after a non-local exit from the test.
 *
 * When a test panics or crashes, control leaves tml_run_with_timeout via
 * longjmp, so the `SetEvent(g_wd_done_event)` above never runs. The watchdog
 * would then fire on an already-handled failure and TerminateProcess(99),
 * turning a clean `test_fail` into a bogus timeout kill. Every non-local exit
 * path in tml_run_test_with_catch calls this.
 */
static void tml_watchdog_cancel(void) {
    if (g_wd_armed && g_wd_done_event) {
        g_wd_armed = 0;
        SetEvent(g_wd_done_event);
    }
}

#else /* non-Windows */

/**
 * @brief Set the per-test timeout in milliseconds.
 * Set to 0 to disable (default).
 */
TML_EXPORT void tml_set_test_timeout(int32_t timeout_ms) {
    tml_test_timeout_ms = timeout_ms;
}

static int32_t tml_run_with_timeout(tml_test_entry_fn test_fn) {
    /* TODO: pthread-based timeout */
    return test_fn();
}

#endif

static int32_t tml_run_test_seh(tml_test_entry_fn test_fn) {
    // NOTE: this deliberately does NOT wrap the call in `__try/__except`.
    //
    // Crashes are already fully handled by the VEH handler installed in
    // tml_run_test_with_catch (VEH runs before any SEH frame handler and either
    // longjmp(2)s or redirects via RtlRestoreContext). The old `__try/__except`
    // was documented as a belt-and-suspenders fallback, but it actively broke the
    // panic path: `panic()` longjmps back to the setjmp in tml_run_test_with_catch,
    // and on Windows x64 longjmp unwinds via RtlUnwindEx. Crossing this SEH scope
    // made that unwind fail with __fastfail(FAST_FAIL_INCORRECT_STACK)
    // (STATUS_STACK_BUFFER_OVERRUN, 0xC0000409), killing the process instead of
    // reporting a clean test failure.
    //
    // Note: tml_run_should_panic used to carry its own small setjmp frame and
    // fastfailed the same way on a real panic; it now delegates here so there is
    // exactly one panic/unwind path to keep working.
    return tml_run_with_timeout(test_fn);
}

/**
 * @brief Shared implementation behind tml_run_test_with_catch / tml_run_should_panic.
 *
 * @param quiet When non-zero, suppress the RT_FATAL report for a caught panic.
 *              `@should_panic` tests expect the panic, so printing
 *              "FATAL [runtime] panic: ..." there is noise — and worse, the test
 *              coordinator scans subprocess stderr for that marker and would
 *              misreport an expected panic as an X003 crash. Crash reports
 *              (jmp_result 2 / context recovery) are always printed: a crash is
 *              never an expected outcome, even for @should_panic.
 * @return 0 on success, -1 if the body panicked, -2 if it crashed, or the body's
 *         own non-zero return value.
 */
static int32_t tml_run_test_catch_impl(tml_test_entry_fn test_fn, int32_t quiet) {
    tml_panic_msg[0] = '\0';
    tml_catching_panic = 1;

#ifdef _WIN32
    // Windows: Use VEH for crash reporting + context redirection for recovery
    tml_install_exception_filter();

#ifdef _M_X64
    // Save recovery context BEFORE setjmp. RtlCaptureContext saves the full CPU
    // state. If a severe crash occurs and longjmp/SEH both fail, the VEH handler
    // calls RtlRestoreContext(&tml_recovery_context) which resumes here.
    // We detect the recovery by checking tml_catching_panic == 0 (set by VEH).
    RtlCaptureContext(&tml_recovery_context);
    tml_recovery_context_valid = 1;

    if (tml_catching_panic == 0) {
        // We got here via RtlRestoreContext from the VEH handler (severe crash).
        // tml_panic_msg was already filled by the VEH handler.
        tml_recovery_context_valid = 0;
        tml_watchdog_cancel();
        tml_remove_exception_filter();
        RT_FATAL("runtime", "%s",
                 tml_panic_msg[0] ? tml_panic_msg
                                  : "CRASH: Unknown (recovered via context restore)");
        fflush(stderr);
        return -2;
    }
#endif

    int jmp_result = setjmp(tml_panic_jmp_buf);
    if (jmp_result == 0) {
        // Run the test. VEH handler intercepts crashes and either:
        // 1. longjmp directly (recoverable: null deref, arithmetic)
        // 2. RtlRestoreContext (severe: heap corruption, wild pointer)
        int32_t result = tml_run_test_seh(test_fn);
        tml_catching_panic = 0;
#ifdef _M_X64
        tml_recovery_context_valid = 0;
#endif
        tml_remove_exception_filter();
        return result;
    } else if (jmp_result == 1) {
        // Got here via longjmp from panic()
        tml_catching_panic = 0;
#ifdef _M_X64
        tml_recovery_context_valid = 0;
#endif
        tml_watchdog_cancel();
        tml_remove_exception_filter();
        if (!quiet) {
            RT_FATAL("runtime", "panic: %s", tml_panic_msg[0] ? tml_panic_msg : "(no message)");
            fflush(stderr);
        }
        return -1;
    } else {
        // Got here via longjmp (jmp_result == 2) from VEH handler (recoverable crash)
        tml_catching_panic = 0;
#ifdef _M_X64
        tml_recovery_context_valid = 0;
#endif
        tml_watchdog_cancel();
        tml_remove_exception_filter();
        RT_FATAL("runtime", "%s", tml_panic_msg[0] ? tml_panic_msg : "CRASH: Unknown");
        fflush(stderr);
        return -2;
    }

#else
    // Unix: Use signal handlers
    tml_install_signal_handlers();

    int jmp_result = setjmp(tml_panic_jmp_buf);
    if (jmp_result == 0) {
        // First time through - run the test (with timeout if configured)
        int32_t result = tml_run_with_timeout(test_fn);
        tml_catching_panic = 0;
        tml_restore_signal_handlers();
        return result;
    } else if (jmp_result == 1) {
        // Got here via longjmp from panic()
        tml_catching_panic = 0;
        tml_restore_signal_handlers();
        if (!quiet) {
            RT_FATAL("runtime", "panic: %s", tml_panic_msg[0] ? tml_panic_msg : "(no message)");
            fflush(stderr);
        }
        return -1;
    } else {
        // Got here via longjmp from signal handler (jmp_result == 2)
        tml_catching_panic = 0;
        tml_restore_signal_handlers();
        RT_FATAL("runtime", "%s", tml_panic_msg[0] ? tml_panic_msg : "CRASH: Unknown");
        fflush(stderr);
        return -2;
    }
#endif
}

TML_EXPORT int32_t tml_run_test_with_catch(tml_test_entry_fn test_fn) {
    return tml_run_test_catch_impl(test_fn, /*quiet=*/0);
}

/**
 * @brief Runs a `@should_panic` test body and reports whether it panicked.
 *
 * Delegates to the same catch machinery as tml_run_test_with_catch instead of
 * carrying its own setjmp/longjmp. The private copy used to fastfail with
 * STATUS_STACK_BUFFER_OVERRUN (0xC0000409) when the body actually panicked:
 * longjmp back into this small frame did not survive, while the identical
 * panic unwinding into tml_run_test_with_catch's frame always worked. That
 * defect went unnoticed because the repository contained no `@should_panic`
 * test at all (see the note at lib/core/tests/convert/convert.test.tml:371,
 * "These need @should_panic support"). Sharing one implementation means the
 * panic path has exactly one behaviour to keep working.
 *
 * A crash (-2) does NOT satisfy `@should_panic` — only a real panic does.
 *
 * @param test_fn The test body to execute.
 * @return 1 if the body panicked (success), 0 otherwise.
 */
int32_t tml_run_should_panic(tml_test_fn test_fn) {
    // Cast is safe on every supported ABI: the callee's return value is simply
    // ignored when the body is unit-returning.
    int32_t rc = tml_run_test_catch_impl((tml_test_entry_fn)(void (*)(void))test_fn, /*quiet=*/1);
    return rc == -1 ? 1 : 0;
}

/**
 * @brief Checks if the panic message contains expected substring.
 *
 * Used by `@should_panic(expected = "message")` tests to verify
 * the panic message content.
 *
 * @param expected The substring to search for.
 * @return 1 if found or expected is empty/NULL, 0 if not found.
 */
int32_t tml_panic_message_contains(const char* expected) {
    if (!expected || !expected[0]) {
        // No expected message specified - any panic is fine
        return 1;
    }
    return strstr(tml_panic_msg, expected) != NULL;
}

// ============================================================================
// Float Formatting Functions
// ============================================================================

// float_to_precision, float_to_exp — REMOVED (Phase 37)
// Replaced by inline LLVM IR using @snprintf in runtime.cpp (Phase 32)
// Declares removed in Phase 36

// f64_is_nan, f64_is_infinite — REMOVED (Phase 27)
// Replaced by pure LLVM IR: fcmp uno / fabs+fcmp oeq in string.cpp
// TML lowlevel calls replaced by pure TML (value != value) in fmt/float.tml

// Remaining functions extracted to separate files — see file header for full list.

// ============================================================================
// Windows DLL Entry Point
// ============================================================================

#ifdef _WIN32
/**
 * @brief DLL entry point - installs crash handler on load.
 *
 * This ensures that crash handling is set up as early as possible
 * when the runtime DLL is loaded.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    (void)fdwReason;

    // NOTE: Do NOT install exception filters in DllMain!
    // When multiple test DLLs are loaded in parallel threads, each DLL's
    // DllMain would overwrite the previous exception filter, causing race
    // conditions and crashes.
    //
    // The exception filter is installed/removed only in tml_run_test_with_catch()
    // which properly scopes the filter to each test function execution.

    return TRUE;
}
#endif

// End of essential.c — see file header for extracted sections.
