#include <stdio.h>
#include <stdint.h>

// TML library function declarations
// These functions are exported from test_lib.tml with tml_ prefix
int32_t tml_add(int32_t a, int32_t b);
int32_t tml_multiply(int32_t x, int32_t y);
int32_t tml_factorial(int32_t n);

int main() {
    printf("Testing TML static library from C...\n\n");

    // Test add function
    int32_t sum = tml_add(5, 3);
    printf("tml_add(5, 3) = %d\n", sum);

    // Test multiply function
    int32_t product = tml_multiply(4, 7);
    printf("tml_multiply(4, 7) = %d\n", product);

    // Test factorial function
    int32_t fact = tml_factorial(5);
    printf("tml_factorial(5) = %d\n", fact);

    // Verify results
    int success = 1;
    if (sum != 8) {
        printf("ERROR: tml_add(5, 3) expected 8, got %d\n", sum);
        success = 0;
    }
    if (product != 28) {
        printf("ERROR: tml_multiply(4, 7) expected 28, got %d\n", product);
        success = 0;
    }
    if (fact != 120) {
        printf("ERROR: tml_factorial(5) expected 120, got %d\n", fact);
        success = 0;
    }

    if (success) {
        printf("\nAll tests passed! TML static library works correctly.\n");
        return 0;
    } else {
        printf("\nSome tests failed!\n");
        return 1;
    }
}
