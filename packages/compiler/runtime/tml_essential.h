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

#endif // TML_ESSENTIAL_H
