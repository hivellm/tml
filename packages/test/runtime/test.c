// TML Test Package - Assertion Runtime
// Testing utilities and assertions
//
// Note: TML's polymorphic assertions work at the compiler level.
// The compiler generates calls to type-specific runtime functions
// based on the argument types at compile time.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============ TEST ASSERTIONS ============

// Basic assertion
void assert(bool condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        exit(1);
    }
}

// Type-specific equality assertions
// The TML compiler generates calls to these based on argument types

void assert_eq_i32(int32_t left, int32_t right, const char* message) {
    if (left != right) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Expected: %d\n", right);
        fprintf(stderr, "   Got:      %d\n", left);
        exit(1);
    }
}

void assert_ne_i32(int32_t left, int32_t right, const char* message) {
    if (left == right) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Values should be different but both are: %d\n", left);
        exit(1);
    }
}

void assert_eq_str(const char* left, const char* right, const char* message) {
    if (strcmp(left, right) != 0) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Expected: \"%s\"\n", right);
        fprintf(stderr, "   Got:      \"%s\"\n", left);
        exit(1);
    }
}

void assert_eq_bool(bool left, bool right, const char* message) {
    if (left != right) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Expected: %s\n", right ? "true" : "false");
        fprintf(stderr, "   Got:      %s\n", left ? "true" : "false");
        exit(1);
    }
}

void assert_eq_i64(int64_t left, int64_t right, const char* message) {
    if (left != right) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Expected: %lld\n", (long long)right);
        fprintf(stderr, "   Got:      %lld\n", (long long)left);
        exit(1);
    }
}

void assert_eq_f64(double left, double right, const char* message) {
    if (left != right) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Expected: %f\n", right);
        fprintf(stderr, "   Got:      %f\n", left);
        exit(1);
    }
}