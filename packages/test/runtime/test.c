// TML Test Package - Assertion Runtime
// Testing utilities and assertions

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============ TEST ASSERTIONS ============

void tml_assert(bool condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        exit(1);
    }
}

void tml_assert_eq_i32(int32_t left, int32_t right, const char* message) {
    if (left != right) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Expected: %d\n", right);
        fprintf(stderr, "   Got:      %d\n", left);
        exit(1);
    }
}

void tml_assert_ne_i32(int32_t left, int32_t right, const char* message) {
    if (left == right) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Values should be different but both are: %d\n", left);
        exit(1);
    }
}

void tml_assert_eq_str(const char* left, const char* right, const char* message) {
    if (strcmp(left, right) != 0) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Expected: \"%s\"\n", right);
        fprintf(stderr, "   Got:      \"%s\"\n", left);
        exit(1);
    }
}

void tml_assert_eq_bool(bool left, bool right, const char* message) {
    if (left != right) {
        fprintf(stderr, "\n\033[31mASSERTION FAILED\033[0m: %s\n", message);
        fprintf(stderr, "   Expected: %s\n", right ? "true" : "false");
        fprintf(stderr, "   Got:      %s\n", left ? "true" : "false");
        exit(1);
    }
}
