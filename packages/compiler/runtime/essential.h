// TML Essential Runtime - Minimal Core Functions

#ifndef ESSENTIAL_H
#define ESSENTIAL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Core I/O
void panic(const char* msg);
void print(const char* str);
void println(const char* str);

// Polymorphic print variants (compiler generates calls based on type)
void print_i32(int32_t n);
void print_i64(int64_t n);
void print_f64(double n);
void print_bool(int32_t b);

// String utilities
int32_t str_len(const char* s);
int32_t str_eq(const char* a, const char* b);
int32_t str_hash(const char* s);

// Time functions
int32_t time_ms(void);
int64_t time_us(void);
int64_t time_ns(void);

#endif // ESSENTIAL_H