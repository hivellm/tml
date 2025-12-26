/**
 * Example: Using TML Math Library as a Dynamic Library (DLL)
 *
 * Build the TML DLL first:
 *   tml build examples/ffi/math_lib.tml --crate-type=dylib --emit-header --out-dir=examples/ffi
 *
 * Then compile this C program:
 *   Windows: clang use_math_dll.c -o use_math_dll.exe math_lib.lib
 *   Linux:   clang use_math_dll.c -o use_math_dll -L. -lmath_lib
 *
 * Run (Windows):
 *   ./use_math_dll.exe
 *
 * Run (Linux):
 *   LD_LIBRARY_PATH=. ./use_math_dll
 */

#include <stdio.h>
#include "math_lib.h"

int main() {
    printf("=== TML Math Library DLL Test ===\n\n");

    // Demonstrate that we're using a DLL by showing that the functions
    // are loaded at runtime, not linked statically
    printf("Dynamic Library Usage:\n");
    printf("  This program links to math_lib.dll at runtime\n");
    printf("  The DLL must be in the same directory or in PATH\n\n");

    // Basic arithmetic
    printf("Basic Operations:\n");
    int32_t sum = tml_add(100, 50);
    printf("  100 + 50 = %d\n", sum);

    int32_t product = tml_multiply(12, 12);
    printf("  12 * 12 = %d\n", product);

    // Mathematical functions
    printf("\nAdvanced Functions:\n");
    int32_t fact = tml_factorial(7);
    printf("  7! = %d\n", fact);

    int32_t pow = tml_power(3, 5);
    printf("  3^5 = %d\n", pow);

    // Test all functions
    printf("\nComprehensive Tests:\n");
    int passed = 0;
    int total = 0;

    #define TEST(expr, expected, desc) do { \
        total++; \
        int32_t result = (expr); \
        if (result == (expected)) { \
            printf("  ✓ %s: %d\n", desc, result); \
            passed++; \
        } else { \
            printf("  ✗ %s: expected %d, got %d\n", desc, (expected), result); \
        } \
    } while(0)

    TEST(tml_add(25, 75), 100, "Addition");
    TEST(tml_subtract(100, 30), 70, "Subtraction");
    TEST(tml_multiply(8, 9), 72, "Multiplication");
    TEST(tml_divide(144, 12), 12, "Division");
    TEST(tml_factorial(5), 120, "Factorial");
    TEST(tml_power(2, 8), 256, "Power");
    TEST(tml_abs(-500), 500, "Absolute value");
    TEST(tml_max(42, 17), 42, "Maximum");
    TEST(tml_min(42, 17), 17, "Minimum");

    printf("\n%d/%d tests passed\n", passed, total);

    if (passed == total) {
        printf("\nAll DLL functions working correctly! ✓\n");
        return 0;
    } else {
        printf("\nSome DLL tests failed! ✗\n");
        return 1;
    }
}
