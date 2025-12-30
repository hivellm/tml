// TML Runtime - Essential Functions (IO only)
// Other functions are in separate files: string.c, mem.c, time.c, math.c, etc.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// print(message: Str) -> Unit
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