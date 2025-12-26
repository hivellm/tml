#ifndef TML_MATH_LIB_H
#define TML_MATH_LIB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// TML library: math_lib
// Auto-generated C header for FFI

int32_t tml_add(int32_t a, int32_t b);
int32_t tml_subtract(int32_t a, int32_t b);
int32_t tml_multiply(int32_t x, int32_t y);
int32_t tml_divide(int32_t x, int32_t y);
int32_t tml_factorial(int32_t n);
int32_t tml_power(int32_t base, int32_t exp);
int32_t tml_abs(int32_t x);
int32_t tml_max(int32_t a, int32_t b);
int32_t tml_min(int32_t a, int32_t b);

#ifdef __cplusplus
}
#endif

#endif // TML_MATH_LIB_H
