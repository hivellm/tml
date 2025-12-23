// TML Runtime - Core Utilities
// Black box and SIMD operations

#include "tml_runtime.h"

// ============ PANIC (error handling) ============

void tml_panic(const char* msg) {
    fprintf(stderr, "panic: %s\n", msg);
    exit(1);
}

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

// ============ BLACK BOX (prevent optimization) ============

int32_t tml_black_box_i32(int32_t value) {
#if defined(_MSC_VER)
    volatile int32_t v = value;
    return v;
#else
    __asm__ volatile("" : "+r"(value) : : "memory");
    return value;
#endif
}

int64_t tml_black_box_i64(int64_t value) {
#if defined(_MSC_VER)
    volatile int64_t v = value;
    return v;
#else
    __asm__ volatile("" : "+r"(value) : : "memory");
    return value;
#endif
}

// ============ SIMD OPERATIONS ============

int64_t tml_simd_sum_i32(const int32_t* arr, int64_t len) {
    int64_t sum = 0;
    #pragma clang loop vectorize(enable) interleave(enable)
    for (int64_t i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}

int64_t tml_simd_sum_i64(const int64_t* arr, int64_t len) {
    int64_t sum = 0;
    #pragma clang loop vectorize(enable) interleave(enable)
    for (int64_t i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}

double tml_simd_sum_f64(const double* arr, int64_t len) {
    double sum = 0.0;
    #pragma clang loop vectorize(enable) interleave(enable)
    for (int64_t i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}

double tml_simd_dot_f64(const double* a, const double* b, int64_t len) {
    double sum = 0.0;
    #pragma clang loop vectorize(enable) interleave(enable)
    for (int64_t i = 0; i < len; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

void tml_simd_fill_i32(int32_t* arr, int32_t value, int64_t len) {
    #pragma clang loop vectorize(enable) interleave(enable)
    for (int64_t i = 0; i < len; i++) {
        arr[i] = value;
    }
}

void tml_simd_add_i32(const int32_t* a, const int32_t* b, int32_t* c, int64_t len) {
    #pragma clang loop vectorize(enable) interleave(enable)
    for (int64_t i = 0; i < len; i++) {
        c[i] = a[i] + b[i];
    }
}

void tml_simd_mul_i32(const int32_t* a, const int32_t* b, int32_t* c, int64_t len) {
    #pragma clang loop vectorize(enable) interleave(enable)
    for (int64_t i = 0; i < len; i++) {
        c[i] = a[i] * b[i];
    }
}
