#include <stdio.h>
#include <stdint.h>

// TML DLL function declarations
// On Windows, functions are imported from DLL
#ifdef _WIN32
__declspec(dllimport) int32_t tml_add(int32_t a, int32_t b);
__declspec(dllimport) int32_t tml_multiply(int32_t x, int32_t y);
__declspec(dllimport) int32_t tml_factorial(int32_t n);
#else
int32_t tml_add(int32_t a, int32_t b);
int32_t tml_multiply(int32_t x, int32_t y);
int32_t tml_factorial(int32_t n);
#endif

int main() {
    printf("Testing TML dynamic library (DLL) from C...\n\n");

    // Test add function
    int32_t sum = tml_add(10, 20);
    printf("tml_add(10, 20) = %d\n", sum);

    // Test multiply function
    int32_t product = tml_multiply(6, 9);
    printf("tml_multiply(6, 9) = %d\n", product);

    // Test factorial function
    int32_t fact = tml_factorial(6);
    printf("tml_factorial(6) = %d\n", fact);

    // Verify results
    int success = 1;
    if (sum != 30) {
        printf("ERROR: tml_add(10, 20) expected 30, got %d\n", sum);
        success = 0;
    }
    if (product != 54) {
        printf("ERROR: tml_multiply(6, 9) expected 54, got %d\n", product);
        success = 0;
    }
    if (fact != 720) {
        printf("ERROR: tml_factorial(6) expected 720, got %d\n", fact);
        success = 0;
    }

    if (success) {
        printf("\nAll tests passed! TML dynamic library works correctly.\n");
        return 0;
    } else {
        printf("\nSome tests failed!\n");
        return 1;
    }
}
