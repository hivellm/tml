/**
 * Example: Using TML Math Library from C
 *
 * Build the TML library first:
 *   tml build examples/ffi/math_lib.tml --crate-type=lib --emit-header --out-dir=examples/ffi
 *
 * Then compile this C program:
 *   Windows: clang use_math_lib.c -o use_math_lib.exe math_lib.lib
 *   Linux:   clang use_math_lib.c -o use_math_lib libmath_lib.a
 */

#include <stdio.h>
#include "math_lib.h"

int main() {
    printf("=== TML Math Library Test ===\n\n");

    // Basic arithmetic
    printf("Arithmetic Operations:\n");
    printf("  10 + 5 = %d\n", tml_add(10, 5));
    printf("  10 - 5 = %d\n", tml_subtract(10, 5));
    printf("  10 * 5 = %d\n", tml_multiply(10, 5));
    printf("  10 / 5 = %d\n", tml_divide(10, 5));
    printf("  10 / 0 = %d (safe)\n\n", tml_divide(10, 0));

    // Mathematical functions
    printf("Mathematical Functions:\n");
    printf("  factorial(5) = %d\n", tml_factorial(5));
    printf("  factorial(10) = %d\n", tml_factorial(10));
    printf("  power(2, 8) = %d\n", tml_power(2, 8));
    printf("  power(3, 4) = %d\n", tml_power(3, 4));
    printf("  abs(-42) = %d\n", tml_abs(-42));
    printf("  abs(42) = %d\n\n", tml_abs(42));

    // Comparison functions
    printf("Comparison Functions:\n");
    printf("  max(15, 20) = %d\n", tml_max(15, 20));
    printf("  min(15, 20) = %d\n", tml_min(15, 20));
    printf("  max(-5, -10) = %d\n", tml_max(-5, -10));
    printf("  min(-5, -10) = %d\n\n", tml_min(-5, -10));

    // Verify correctness
    printf("Verification:\n");
    int passed = 0;
    int total = 0;

    #define TEST(expr, expected) do { \
        total++; \
        if ((expr) == (expected)) { \
            printf("  ✓ " #expr " == %d\n", expected); \
            passed++; \
        } else { \
            printf("  ✗ " #expr " != %d (got %d)\n", expected, (expr)); \
        } \
    } while(0)

    TEST(tml_add(3, 7), 10);
    TEST(tml_multiply(6, 7), 42);
    TEST(tml_factorial(6), 720);
    TEST(tml_power(2, 10), 1024);
    TEST(tml_abs(-100), 100);
    TEST(tml_max(50, 25), 50);

    printf("\n%d/%d tests passed\n", passed, total);

    if (passed == total) {
        printf("\nAll tests passed! ✓\n");
        return 0;
    } else {
        printf("\nSome tests failed! ✗\n");
        return 1;
    }
}
