// TML Essential Runtime - Minimal Core Functions
// Only the absolute minimum required by the compiler
// All other functionality should be in package-specific runtimes

#ifndef TML_ESSENTIAL_H
#define TML_ESSENTIAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// ============ PANIC (essential error handling) ============
// Used by the compiler for unrecoverable errors
void tml_panic(const char* msg);

// ============ I/O FUNCTIONS ============
// print() is polymorphic in TML - the compiler generates appropriate
// printf() calls directly for I32, Bool, F64, etc.
// These functions are only used for string output.

void tml_print(const char* str);
void tml_println(const char* str);

// ============ STRING UTILITIES ============

int32_t tml_str_len(const char* s);
int32_t tml_str_eq(const char* a, const char* b);
int32_t tml_str_hash(const char* s);

// ============ TIME FUNCTIONS ============

int32_t tml_time_ms(void);
int64_t tml_time_us(void);
int64_t tml_time_ns(void);

// ============ FLOAT MATH FUNCTIONS ============

double tml_float_sqrt(double x);
double tml_float_pow(double base, int32_t exp);
double tml_float_abs(double x);
double tml_int_to_float(int32_t x);
int32_t tml_float_to_int(double x);
int32_t tml_float_round(double x);
int32_t tml_float_floor(double x);
int32_t tml_float_ceil(double x);

// ============ BIT MANIPULATION FUNCTIONS ============

uint32_t tml_float32_bits(float f);
float tml_float32_from_bits(uint32_t b);
uint64_t tml_float64_bits(double f);
double tml_float64_from_bits(uint64_t b);

// ============ SPECIAL FLOAT VALUES ============

double tml_infinity(int32_t sign);
double tml_nan(void);
int32_t tml_is_inf(double f, int32_t sign);
int32_t tml_is_nan(double f);

// ============ NEXTAFTER FUNCTIONS ============

double tml_nextafter(double x, double y);
float tml_nextafter32(float x, float y);

#endif // TML_ESSENTIAL_H
