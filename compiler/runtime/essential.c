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

#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Export macro for DLL visibility
#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
// Include Windows headers for SEH
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

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

/** @brief Buffer to store the panic message when caught. */
static char tml_panic_msg[1024] = {0};

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
void print(const char* message) {
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
void println(const char* message) {
    if (tml_suppress_output)
        return;
    if (message)
        printf("%s\n", message);
    else
        printf("\n");
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
void panic(const char* message) {
    // If we're in panic catching mode, save the message and longjmp back
    if (tml_catching_panic) {
        if (message) {
            snprintf(tml_panic_msg, sizeof(tml_panic_msg), "%s", message);
        } else {
            tml_panic_msg[0] = '\0';
        }
        longjmp(tml_panic_jmp_buf, 1);
    }

    // Normal panic behavior - print message and exit
    fprintf(stderr, "panic: %s\n", message ? message : "(null)");
    exit(1);
}

/**
 * @brief Asserts a condition, panicking if false.
 *
 * Maps to TML's `assert(condition: Bool, message: Str) -> Unit` builtin.
 * Note: Named `assert_tml` to avoid conflict with C's `assert` macro.
 *
 * When panic catching is active (in test mode), this uses the panic
 * mechanism to properly longjmp back to the test harness instead of
 * calling exit() which can cause stack corruption in DLL context.
 *
 * @param condition The condition to check (0 = false, non-zero = true).
 * @param message The message to display if assertion fails.
 */
void assert_tml(int32_t condition, const char* message) {
    if (!condition) {
        // Build assertion failed message
        static char assert_msg[1024];
        snprintf(assert_msg, sizeof(assert_msg), "assertion failed: %s",
                 message ? message : "(no message)");

        // Use panic mechanism if catching is enabled (DLL test context)
        // This properly longjmps back instead of calling exit()
        if (tml_catching_panic) {
            snprintf(tml_panic_msg, sizeof(tml_panic_msg), "%s", assert_msg);
            longjmp(tml_panic_jmp_buf, 1);
        }

        // Normal mode - print and exit
        fprintf(stderr, "%s\n", assert_msg);
        exit(1);
    }
}

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
            longjmp(tml_panic_jmp_buf, 1);
        }

        // Normal mode - print and exit
        fprintf(stderr, "%s\n", assert_msg);
        exit(1);
    }
}

// ============================================================================
// Type-Specific Print Variants (for polymorphic print)
// ============================================================================

/** @brief Prints a 32-bit signed integer to stdout. */
void print_i32(int32_t n) {
    if (tml_suppress_output)
        return;
    printf("%d", n);
}

/** @brief Prints a 64-bit signed integer to stdout. */
void print_i64(int64_t n) {
    if (tml_suppress_output)
        return;
    printf("%lld", (long long)n);
}

/** @brief Prints a 32-bit floating point number to stdout. */
void print_f32(float n) {
    if (tml_suppress_output)
        return;
    printf("%g", n);
}

/** @brief Prints a 64-bit floating point number to stdout. */
void print_f64(double n) {
    if (tml_suppress_output)
        return;
    printf("%g", n);
}

/** @brief Prints a boolean as "true" or "false" to stdout. */
void print_bool(int32_t b) {
    if (tml_suppress_output)
        return;
    printf("%s", b ? "true" : "false");
}

/** @brief Prints a character to stdout. */
void print_char(int32_t c) {
    if (tml_suppress_output)
        return;
    printf("%c", (char)c);
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
int32_t tml_run_should_panic(tml_test_fn test_fn) {
    tml_panic_msg[0] = '\0';
    tml_catching_panic = 1;

    if (setjmp(tml_panic_jmp_buf) == 0) {
        // First time through - run the test
        test_fn();
        // If we get here, test didn't panic
        tml_catching_panic = 0;
        return 0; // Failure - expected panic but didn't get one
    } else {
        // Got here via longjmp - panic was caught
        tml_catching_panic = 0;
        return 1; // Success - panic was caught
    }
}

/**
 * @brief Gets the last panic message.
 *
 * Returns the message from the most recently caught panic. Only valid
 * after `tml_run_should_panic` returns 1.
 *
 * @return The panic message string.
 */
const char* tml_get_panic_message(void) {
    return tml_panic_msg;
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
    fprintf(stderr, "FATAL: %s\n", sig_name);
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
static const char* tml_get_exception_name(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "ACCESS_VIOLATION (Segmentation fault)";
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
    default:
        return "UNKNOWN_EXCEPTION";
    }
}

/** @brief Previous unhandled exception filter. */
static LPTOP_LEVEL_EXCEPTION_FILTER tml_prev_filter = NULL;

/** @brief Reference count for exception filter - thread-safe. */
static volatile LONG tml_filter_refcount = 0;

/** @brief Flag indicating if the filter is installed. */
static volatile LONG tml_filter_installed = 0;

/** @brief Top-level exception filter for crash catching. */
static LONG WINAPI tml_exception_filter(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;

    // Format and print the crash message immediately to stderr
    // Using WriteFile for reliability in exception context
    char msg[256];
    int len = snprintf(msg, sizeof(msg), "CRASH: %s (0x%08lX)\n", tml_get_exception_name(code),
                       (unsigned long)code);

    DWORD written;
    HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);
    WriteFile(hStderr, msg, (DWORD)len, &written, NULL);
    FlushFileBuffers(hStderr);

    // Store in global for potential retrieval
    snprintf(tml_panic_msg, sizeof(tml_panic_msg), "CRASH: %s (0x%08lX)",
             tml_get_exception_name(code), (unsigned long)code);

    // NOTE: Do NOT call longjmp from exception filter!
    // longjmp from a Windows exception handler causes STATUS_BAD_STACK (0xC0000028)
    // because the stack state is undefined during exception handling.
    // Instead, let the test runner's SEH (__try/__except) handle the exception.

    // Let the default handler run
    if (tml_prev_filter) {
        return tml_prev_filter(info);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/** @brief Install the unhandled exception filter (thread-safe, ref-counted). */
static void tml_install_exception_filter(void) {
    // Increment ref count and install filter only once
    if (InterlockedIncrement(&tml_filter_refcount) == 1) {
        // First reference - actually install the filter
        if (InterlockedCompareExchange(&tml_filter_installed, 1, 0) == 0) {
            tml_prev_filter = SetUnhandledExceptionFilter(tml_exception_filter);
            // Also disable Windows Error Reporting popup
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        }
    }
}

/** @brief Remove the unhandled exception filter (thread-safe, ref-counted). */
static void tml_remove_exception_filter(void) {
    // Decrement ref count but don't actually remove the filter
    // The filter stays installed for the lifetime of the process to avoid
    // race conditions in parallel test execution
    if (tml_filter_refcount > 0) {
        InterlockedDecrement(&tml_filter_refcount);
    }
    // NOTE: We intentionally don't remove the filter to avoid race conditions.
    // The filter is harmless when not actively catching panics.
}
#endif

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
TML_EXPORT int32_t tml_run_test_with_catch(tml_test_entry_fn test_fn) {
    tml_panic_msg[0] = '\0';
    tml_catching_panic = 1;

#ifdef _WIN32
    // Windows: Use SetUnhandledExceptionFilter for crash catching
    tml_install_exception_filter();

    int jmp_result = setjmp(tml_panic_jmp_buf);
    if (jmp_result == 0) {
        // First time through - run the test
        int32_t result = test_fn();
        tml_catching_panic = 0;
        tml_remove_exception_filter();
        return result;
    } else if (jmp_result == 1) {
        // Got here via longjmp from panic()
        tml_catching_panic = 0;
        tml_remove_exception_filter();
        fprintf(stderr, "panic: %s\n", tml_panic_msg[0] ? tml_panic_msg : "(no message)");
        fflush(stderr);
        return -1;
    } else {
        // Got here via longjmp from exception filter (jmp_result == 2)
        tml_catching_panic = 0;
        tml_remove_exception_filter();
        // Message already printed by the filter
        return -2;
    }

#else
    // Unix: Use signal handlers
    tml_install_signal_handlers();

    int jmp_result = setjmp(tml_panic_jmp_buf);
    if (jmp_result == 0) {
        // First time through - run the test
        int32_t result = test_fn();
        tml_catching_panic = 0;
        tml_restore_signal_handlers();
        return result;
    } else if (jmp_result == 1) {
        // Got here via longjmp from panic()
        tml_catching_panic = 0;
        tml_restore_signal_handlers();
        fprintf(stderr, "panic: %s\n", tml_panic_msg[0] ? tml_panic_msg : "(no message)");
        fflush(stderr);
        return -1;
    } else {
        // Got here via longjmp from signal handler (jmp_result == 2)
        tml_catching_panic = 0;
        tml_restore_signal_handlers();
        fprintf(stderr, "%s\n", tml_panic_msg[0] ? tml_panic_msg : "CRASH: Unknown");
        fflush(stderr);
        return -2;
    }
#endif
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

/** @brief Buffer for float formatting output. */
static char float_format_buffer[256];

/**
 * @brief Formats a float with specified precision.
 *
 * @param value The floating point value to format.
 * @param precision Number of decimal places.
 * @return A static buffer containing the formatted string.
 */
const char* float_to_precision(double value, int32_t precision) {
    if (precision < 0)
        precision = 0;
    if (precision > 20)
        precision = 20;
    snprintf(float_format_buffer, sizeof(float_format_buffer), "%.*f", precision, value);
    return float_format_buffer;
}

/**
 * @brief Formats a float in scientific notation.
 *
 * @param value The floating point value to format.
 * @param uppercase If non-zero, uses 'E' instead of 'e'.
 * @return A static buffer containing the formatted string.
 */
const char* float_to_exp(double value, int32_t uppercase) {
    if (uppercase) {
        snprintf(float_format_buffer, sizeof(float_format_buffer), "%E", value);
    } else {
        snprintf(float_format_buffer, sizeof(float_format_buffer), "%e", value);
    }
    return float_format_buffer;
}

/**
 * @brief Checks if a float is NaN.
 *
 * @param value The floating point value to check.
 * @return 1 if NaN, 0 otherwise.
 */
int32_t f64_is_nan(double value) {
    return isnan(value) ? 1 : 0;
}

/**
 * @brief Checks if a float is infinite.
 *
 * @param value The floating point value to check.
 * @return 1 if infinite, 0 otherwise.
 */
int32_t f64_is_infinite(double value) {
    return isinf(value) ? 1 : 0;
}

// ============================================================================
// UTF-8 String Encoding Functions
// ============================================================================

/** @brief Buffer for UTF-8 character output (max 4 bytes + null terminator). */
static char utf8_char_buffer[8];

/**
 * @brief Converts a 2-byte UTF-8 sequence to a string.
 *
 * Used for Unicode code points U+0080 to U+07FF.
 *
 * @param b1 First byte (110xxxxx).
 * @param b2 Second byte (10xxxxxx).
 * @return A static buffer containing the 2-byte UTF-8 string.
 */
TML_EXPORT const char* utf8_2byte_to_string(uint8_t b1, uint8_t b2) {
    utf8_char_buffer[0] = (char)b1;
    utf8_char_buffer[1] = (char)b2;
    utf8_char_buffer[2] = '\0';
    return utf8_char_buffer;
}

/**
 * @brief Converts a 3-byte UTF-8 sequence to a string.
 *
 * Used for Unicode code points U+0800 to U+FFFF.
 *
 * @param b1 First byte (1110xxxx).
 * @param b2 Second byte (10xxxxxx).
 * @param b3 Third byte (10xxxxxx).
 * @return A static buffer containing the 3-byte UTF-8 string.
 */
TML_EXPORT const char* utf8_3byte_to_string(uint8_t b1, uint8_t b2, uint8_t b3) {
    utf8_char_buffer[0] = (char)b1;
    utf8_char_buffer[1] = (char)b2;
    utf8_char_buffer[2] = (char)b3;
    utf8_char_buffer[3] = '\0';
    return utf8_char_buffer;
}

/**
 * @brief Converts a 4-byte UTF-8 sequence to a string.
 *
 * Used for Unicode code points U+10000 to U+10FFFF.
 *
 * @param b1 First byte (11110xxx).
 * @param b2 Second byte (10xxxxxx).
 * @param b3 Third byte (10xxxxxx).
 * @param b4 Fourth byte (10xxxxxx).
 * @return A static buffer containing the 4-byte UTF-8 string.
 */
TML_EXPORT const char* utf8_4byte_to_string(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    utf8_char_buffer[0] = (char)b1;
    utf8_char_buffer[1] = (char)b2;
    utf8_char_buffer[2] = (char)b3;
    utf8_char_buffer[3] = (char)b4;
    utf8_char_buffer[4] = '\0';
    return utf8_char_buffer;
}

// ============================================================================
// Random Seed Generation
// ============================================================================

/** @brief Global counter for generating unique seeds. */
static uint64_t tml_seed_counter = 0;

/**
 * @brief Gets a unique random seed value.
 *
 * Uses a combination of a monotonic counter and the process address space
 * to generate unique seeds across multiple calls.
 *
 * @return A 64-bit seed value.
 */
TML_EXPORT uint64_t tml_random_seed(void) {
    // Increment counter atomically (simple version - not thread-safe)
    uint64_t counter = ++tml_seed_counter;

    // Mix in the address of the counter variable for additional entropy
    uint64_t addr = (uint64_t)(uintptr_t)&tml_seed_counter;

    // Simple mixing function (similar to SplitMix64)
    uint64_t seed = counter ^ addr;
    seed = seed * 0x9E3779B97F4A7C15ULL;
    seed ^= seed >> 30;
    seed = seed * 0xBF58476D1CE4E5B9ULL;
    seed ^= seed >> 27;

    return seed;
}

// NOTE: The following functions have been moved to separate files:
// - Pool functions (pool_init, pool_acquire, etc.) -> pool.c
// - TLS pool functions (tls_pool_acquire, etc.) -> pool.c
// - List functions (list_create, list_push, etc.) -> collections.c
// - Sync functions (tml_mutex_*, tml_rwlock_*, tml_condvar_*, tml_thread_*) -> sync.c

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
