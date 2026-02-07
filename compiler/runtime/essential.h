/**
 * @file essential.h
 * @brief TML Runtime - Essential Functions Header
 *
 * Core runtime declarations for the TML language. This header provides the
 * fundamental runtime functions that all TML programs depend on, including:
 *
 * - **I/O functions**: `print`, `println`, `panic`, `assert_tml`
 * - **String functions**: manipulation, comparison, and conversion
 * - **Time functions**: timestamps, sleep, and elapsed time measurement
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
 * ## Thread Safety
 *
 * Most functions are thread-safe. String functions using static buffers
 * (like `str_concat`) are NOT thread-safe and should not be used from
 * multiple threads simultaneously.
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
 * @brief Asserts a condition, panicking if false.
 * @param condition The condition to check (0 = false, non-zero = true).
 * @param message The message to display if assertion fails.
 */
void assert_tml(int32_t condition, const char* message);

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

/** @brief Prints a 32-bit floating point number. */
void print_f32(float n);

/** @brief Prints a 64-bit floating point number. */
void print_f64(double n);

/** @brief Prints a boolean as "true" or "false". */
void print_bool(int32_t b);

/** @brief Prints a character. */
void print_char(int32_t c);

// ============================================================================
// String Functions
// ============================================================================

/**
 * @brief Returns the length of a string.
 * @param s The null-terminated string.
 * @return The length in bytes, or 0 if s is NULL.
 */
int32_t str_len(const char* s);

/**
 * @brief Compares two strings for equality.
 * @param a First string.
 * @param b Second string.
 * @return 1 if equal, 0 if not equal.
 */
int32_t str_eq(const char* a, const char* b);

/**
 * @brief Computes a hash code for a string using djb2 algorithm.
 * @param s The string to hash.
 * @return Hash value as signed 32-bit integer.
 */
int32_t str_hash(const char* s);

/**
 * @brief Concatenates two strings.
 * @param a First string.
 * @param b Second string.
 * @return Pointer to static buffer containing result. NOT THREAD SAFE.
 * @warning Result is invalidated by next call to string functions using static buffer.
 */
const char* str_concat(const char* a, const char* b);

/**
 * @brief Optimized string concatenation with O(1) amortized complexity.
 * @param a Left string (may be modified in place if dynamic with capacity).
 * @param b Right string to append.
 * @return Concatenated string (may be same pointer as 'a' or new allocation).
 */
const char* str_concat_opt(const char* a, const char* b);

/**
 * @brief Concatenates 3 strings in a single allocation.
 * Optimized version for the common case of "a" + "b" + "c".
 */
const char* str_concat_3(const char* a, const char* b, const char* c);

/**
 * @brief Concatenates 4 strings in a single allocation.
 * Optimized version for "a" + "b" + "c" + "d".
 */
const char* str_concat_4(const char* a, const char* b, const char* c, const char* d);

/**
 * @brief Concatenates multiple strings in a single allocation.
 * @param strings Array of string pointers.
 * @param count Number of strings.
 * @return Concatenated string (caller owns, uses dynamic string header).
 */
const char* str_concat_n(const char** strings, int64_t count);

/**
 * @brief Extracts a substring.
 * @param s Source string.
 * @param start Starting index (0-based).
 * @param len Number of characters to extract.
 * @return Pointer to static buffer containing result.
 */
const char* str_substring(const char* s, int32_t start, int32_t len);

/**
 * @brief Extracts a slice of a string (exclusive end).
 * @param s Source string.
 * @param start Starting index (0-based).
 * @param end Ending index (exclusive).
 * @return Pointer to static buffer containing result.
 */
const char* str_slice(const char* s, int64_t start, int64_t end);

/**
 * @brief Checks if haystack contains needle.
 * @return 1 if found, 0 if not found.
 */
int32_t str_contains(const char* haystack, const char* needle);

/**
 * @brief Checks if string starts with prefix.
 * @return 1 if starts with prefix, 0 otherwise.
 */
int32_t str_starts_with(const char* s, const char* prefix);

/**
 * @brief Checks if string ends with suffix.
 * @return 1 if ends with suffix, 0 otherwise.
 */
int32_t str_ends_with(const char* s, const char* suffix);

/**
 * @brief Converts string to uppercase.
 * @return Pointer to static buffer containing result.
 */
const char* str_to_upper(const char* s);

/**
 * @brief Converts string to lowercase.
 * @return Pointer to static buffer containing result.
 */
const char* str_to_lower(const char* s);

/**
 * @brief Removes leading and trailing whitespace.
 * @return Pointer to static buffer containing result.
 */
const char* str_trim(const char* s);

/**
 * @brief Gets the character at a specific index.
 * @param s The string.
 * @param index The 0-based index.
 * @return The character code, or 0 if out of bounds.
 */
int32_t str_char_at(const char* s, int32_t index);

/**
 * @brief Converts a single byte to a 1-character string.
 * @param c The character byte.
 * @return Pointer to static buffer containing the character as string.
 */
const char* char_to_string(uint8_t c);

// ============================================================================
// Time Functions
// ============================================================================

/**
 * @brief Gets current time in milliseconds.
 * @return Milliseconds since system-dependent epoch.
 */
int32_t time_ms(void);

/**
 * @brief Gets current time in microseconds.
 * @return Microseconds since system-dependent epoch.
 */
int64_t time_us(void);

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

/**
 * @brief Sleeps for specified microseconds.
 * @param us Number of microseconds to sleep.
 */
void sleep_us(int64_t us);

/**
 * @brief Calculates elapsed time in milliseconds.
 * @param start Start timestamp from time_ms().
 * @return Elapsed milliseconds.
 */
int32_t elapsed_ms(int32_t start);

/**
 * @brief Calculates elapsed time in microseconds.
 * @param start Start timestamp from time_us().
 * @return Elapsed microseconds.
 */
int64_t elapsed_us(int64_t start);

/**
 * @brief Calculates elapsed time in nanoseconds.
 * @param start Start timestamp from time_ns().
 * @return Elapsed nanoseconds.
 */
int64_t elapsed_ns(int64_t start);

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
