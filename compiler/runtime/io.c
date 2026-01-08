/**
 * @file io.c
 * @brief TML Runtime - IO Functions (Standalone)
 *
 * Standalone I/O functions for the TML language. This file contains basic
 * I/O operations that can be used independently of essential.c.
 *
 * ## Note
 *
 * Most projects should use essential.c which includes these functions plus
 * panic catching support. This file is provided for minimal builds that
 * don't need the full runtime.
 *
 * ## Components
 *
 * - **Output**: `print`, `println`
 * - **Control flow**: `panic`, `assert_tml`
 * - **Type-specific print**: `print_i32`, `print_i64`, `print_f32`, etc.
 *
 * @see essential.c for the complete runtime with panic catching
 * @see env_builtins_io.cpp for compiler builtin registration
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Prints a string to stdout without a newline.
 * Maps to TML's `print(message: Str) -> Unit` builtin.
 */
void print(const char* message) {
    if (message)
        printf("%s", message);
}

// println(message: Str) -> Unit
void println(const char* message) {
    if (message)
        printf("%s\n", message);
    else
        printf("\n");
}

// panic(message: Str) -> Never
void panic(const char* message) {
    fprintf(stderr, "panic: %s\n", message ? message : "(null)");
    exit(1);
}

// assert(condition: Bool, message: Str) -> Unit
void assert_tml(int32_t condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message ? message : "(no message)");
        exit(1);
    }
}

// Type-specific print variants (for polymorphic print)
void print_i32(int32_t n) {
    printf("%d", n);
}
void print_i64(int64_t n) {
    printf("%lld", (long long)n);
}
void print_f32(float n) {
    printf("%g", n);
}
void print_f64(double n) {
    printf("%g", n);
}
void print_bool(int32_t b) {
    printf("%s", b ? "true" : "false");
}
void print_char(int32_t c) {
    printf("%c", (char)c);
}