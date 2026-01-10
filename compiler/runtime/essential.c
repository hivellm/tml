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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 *
 * @param message The null-terminated string to print. NULL is ignored.
 */
void print(const char* message) {
    if (message)
        printf("%s", message);
}

/**
 * @brief Prints a string to stdout followed by a newline.
 *
 * Maps to TML's `println(message: Str) -> Unit` builtin.
 *
 * @param message The null-terminated string to print. NULL prints only newline.
 */
void println(const char* message) {
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
 * @param condition The condition to check (0 = false, non-zero = true).
 * @param message The message to display if assertion fails.
 */
void assert_tml(int32_t condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message ? message : "(no message)");
        exit(1);
    }
}

// ============================================================================
// Type-Specific Print Variants (for polymorphic print)
// ============================================================================

/** @brief Prints a 32-bit signed integer to stdout. */
void print_i32(int32_t n) {
    printf("%d", n);
}

/** @brief Prints a 64-bit signed integer to stdout. */
void print_i64(int64_t n) {
    printf("%lld", (long long)n);
}

/** @brief Prints a 32-bit floating point number to stdout. */
void print_f32(float n) {
    printf("%g", n);
}

/** @brief Prints a 64-bit floating point number to stdout. */
void print_f64(double n) {
    printf("%g", n);
}

/** @brief Prints a boolean as "true" or "false" to stdout. */
void print_bool(int32_t b) {
    printf("%s", b ? "true" : "false");
}

/** @brief Prints a character to stdout. */
void print_char(int32_t c) {
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
