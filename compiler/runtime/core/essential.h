/**
 * @file essential.h
 * @brief TML Runtime - Essential Functions Header
 *
 * Core runtime declarations for the TML language. This header provides the
 * fundamental runtime functions that all TML programs depend on, including:
 *
 * - **I/O functions**: `print`, `println`, `panic`, `assert_tml_loc`
 * - **Time functions**: `time_ns`, `sleep_ms` (see time/time.c for Instant API)
 * - **Memory functions**: allocation, deallocation, and memory operations
 * - **Panic catching**: infrastructure for `@should_panic` tests
 * - **Async helpers**: simple block_on implementations for sync async functions
 *
 * ## Usage
 *
 * This header is automatically included by the TML compiler when generating
 * LLVM IR that calls runtime functions. User code should not include this
 * directly.
 *
 * ## Note on Strings
 *
 * String operations (concat, compare, slice, etc.) are implemented in pure TML
 * (lib/core/src/str.tml) or as inline LLVM IR (str_eq, str_concat_opt).
 * No C string functions are needed in the runtime.
 */

#ifndef TML_ESSENTIAL_H
#define TML_ESSENTIAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Output Suppression (for test runner)
// ============================================================================

/**
 * @brief Sets the output suppression flag.
 *
 * When set to non-zero, print/println functions will not produce output.
 * This is used by the test runner to suppress test output when not in
 * verbose mode.
 *
 * @param suppress Non-zero to suppress output, zero to enable output.
 */
void tml_set_output_suppressed(int32_t suppress);

/**
 * @brief Gets the current output suppression state.
 * @return Non-zero if output is suppressed, zero otherwise.
 */
int32_t tml_get_output_suppressed(void);

// ============================================================================
// IO Functions
// ============================================================================

/**
 * @brief Prints a string to stdout without a newline.
 * @param message The null-terminated string to print. If NULL, prints nothing.
 */
void print(const char* message);

/**
 * @brief Prints a string to stdout followed by a newline.
 * @param message The null-terminated string to print. If NULL, prints only newline.
 */
void println(const char* message);

/**
 * @brief Terminates the program with an error message.
 *
 * If panic catching is enabled (via `tml_run_should_panic`), the panic is
 * caught and control returns to the test harness. Otherwise, prints the
 * message to stderr and calls `exit(1)`.
 *
 * @param message The panic message. If NULL, prints "(null)".
 * @note This function never returns in normal operation.
 */
void panic(const char* message);

/**
 * @brief Asserts a condition with file and line information.
 *
 * @param condition The condition to check.
 * @param message The assertion message.
 * @param file The source file name.
 * @param line The line number.
 */
void assert_tml_loc(int32_t condition, const char* message, const char* file, int32_t line);

// ============================================================================
// Type-Specific Print Functions
// ============================================================================

/** @brief Prints a 32-bit signed integer. */
void print_i32(int32_t n);

/** @brief Prints a 64-bit signed integer. */
void print_i64(int64_t n);

/** @brief Prints a 64-bit floating point number. */
void print_f64(double n);

/** @brief Prints a boolean as "true" or "false". */
void print_bool(int32_t b);

// print_char — REMOVED (Phase 37/49, no .c impl, no codegen declare)

// String functions — REMOVED (Phase 49)
// All 17 string functions (str_len, str_eq, str_hash, str_concat, str_concat_3,
// str_concat_4, str_concat_n, str_substring, str_slice, str_contains,
// str_starts_with, str_ends_with, str_to_upper, str_to_lower, str_trim,
// str_char_at, char_to_string) had NO implementation in any .c file.
// str_eq and str_concat_opt are inlined as LLVM IR in runtime.cpp.
// All string operations are implemented in pure TML (lib/core/src/str.tml).

// ============================================================================
// Time Functions
// ============================================================================

/**
 * @brief Gets current time in nanoseconds.
 * @return Nanoseconds since system-dependent epoch.
 */
int64_t time_ns(void);

/**
 * @brief Sleeps for specified milliseconds.
 * @param ms Number of milliseconds to sleep.
 */
void sleep_ms(int32_t ms);

// ============================================================================
// Memory Functions
// ============================================================================

/**
 * @brief Allocates memory.
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure.
 */
void* mem_alloc(int64_t size);

/**
 * @brief Allocates zero-initialized memory.
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure.
 */
void* mem_alloc_zeroed(int64_t size);

/**
 * @brief Reallocates memory to a new size.
 * @param ptr Pointer to existing allocation.
 * @param new_size New size in bytes.
 * @return Pointer to reallocated memory, or NULL on failure.
 */
void* mem_realloc(void* ptr, int64_t new_size);

/**
 * @brief Frees allocated memory.
 * @param ptr Pointer to memory to free.
 */
void mem_free(void* ptr);

/**
 * @brief Copies memory (non-overlapping regions).
 * @param dest Destination pointer.
 * @param src Source pointer.
 * @param size Number of bytes to copy.
 */
void mem_copy(void* dest, const void* src, int64_t size);

/**
 * @brief Moves memory (handles overlapping regions).
 * @param dest Destination pointer.
 * @param src Source pointer.
 * @param size Number of bytes to move.
 */
void mem_move(void* dest, const void* src, int64_t size);

/**
 * @brief Sets memory to a value.
 * @param ptr Pointer to memory.
 * @param value Value to set (truncated to byte).
 * @param size Number of bytes to set.
 */
void mem_set(void* ptr, int32_t value, int64_t size);

/**
 * @brief Zeros memory.
 * @param ptr Pointer to memory.
 * @param size Number of bytes to zero.
 */
void mem_zero(void* ptr, int64_t size);

/**
 * @brief Compares two memory regions.
 * @param a First memory region.
 * @param b Second memory region.
 * @param size Number of bytes to compare.
 * @return <0 if a<b, 0 if equal, >0 if a>b.
 */
int32_t mem_compare(const void* a, const void* b, int64_t size);

/**
 * @brief Checks if two memory regions are equal.
 * @param a First memory region.
 * @param b Second memory region.
 * @param size Number of bytes to compare.
 * @return 1 if equal, 0 if not equal.
 */
int32_t mem_eq(const void* a, const void* b, int64_t size);

// ============================================================================
// Panic Catching (for @should_panic tests)
// ============================================================================

/**
 * @brief Callback type for test functions.
 *
 * Test functions take no arguments and return nothing. They are expected
 * to either complete normally or call `panic()`.
 */
typedef void (*tml_test_fn)(void);

/**
 * @brief Runs a test function that is expected to panic.
 *
 * This function uses setjmp/longjmp to catch panics. The test function is
 * executed, and if it calls `panic()`, control returns here instead of
 * terminating the program.
 *
 * @param test_fn The test function to execute.
 * @return 1 if the test panicked (success for @should_panic), 0 if it didn't.
 */
int32_t tml_run_should_panic(tml_test_fn test_fn);

/**
 * @brief Gets the last panic message.
 *
 * Valid only after `tml_run_should_panic` returns 1.
 *
 * @return The panic message from the caught panic.
 */
const char* tml_get_panic_message(void);

/**
 * @brief Gets the backtrace from the last caught panic.
 *
 * Returns the formatted backtrace string captured at the panic site.
 * Only valid after `tml_run_should_panic` returns 1 and if backtrace
 * was enabled via `tml_enable_backtrace_on_panic`.
 *
 * @return The backtrace string, or empty string if not available.
 */
const char* tml_get_panic_backtrace(void);

/**
 * @brief Gets the backtrace from the last caught panic in JSON format.
 *
 * Returns the backtrace as a JSON array of frame objects.
 * Only valid after a panic was caught and if backtrace
 * was enabled via `tml_enable_backtrace_on_panic`.
 *
 * @return The JSON backtrace string, or "[]" if not available.
 */
const char* tml_get_panic_backtrace_json(void);

/**
 * @brief Checks if the panic message contains expected text.
 * @param expected The substring to search for.
 * @return 1 if found or expected is empty, 0 if not found.
 */
int32_t tml_panic_message_contains(const char* expected);

// ============================================================================
// Async Runtime (see async.h for full API)
// ============================================================================

/** @brief Forward declaration for async executor. */
struct TmlExecutor;

/** @brief Forward declaration for async task. */
struct TmlTask;

/** @brief Forward declaration for poll result. */
struct TmlPoll;

/**
 * @brief Simple block_on for synchronous async functions returning I64.
 *
 * Extracts the Ready value from a Poll struct. Used by the compiler for
 * async functions that always return immediately.
 *
 * @param poll_ptr Pointer to TmlPoll struct.
 * @return The i64 value from Poll::Ready.
 */
int64_t tml_block_on_simple_i64(void* poll_ptr);

/**
 * @brief Simple block_on for synchronous async functions returning I32.
 * @param poll_ptr Pointer to TmlPoll struct.
 * @return The i32 value from Poll::Ready.
 */
int32_t tml_block_on_simple_i32(void* poll_ptr);

/**
 * @brief Simple block_on for synchronous async functions returning F64.
 * @param poll_ptr Pointer to TmlPoll struct.
 * @return The f64 value from Poll::Ready.
 */
double tml_block_on_simple_f64(void* poll_ptr);

/**
 * @brief Simple block_on for synchronous async functions returning pointer.
 * @param poll_ptr Pointer to TmlPoll struct.
 * @return The pointer value from Poll::Ready.
 */
void* tml_block_on_simple_ptr(void* poll_ptr);

#ifdef __cplusplus
}
#endif

#endif // TML_ESSENTIAL_H
