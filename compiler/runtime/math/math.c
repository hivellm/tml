/**
 * @file math.c
 * @brief TML Runtime - Math Functions (Active Only)
 *
 * Implements mathematical functions and utilities for the TML language.
 *
 * ## Active Components
 *
 * - **Black box**: `black_box_i32`, `black_box_i64`, `black_box_f64` (prevent optimization)
 * - **SIMD operations**: `simd_sum_i32`, `simd_sum_f64`, `simd_dot_f64`
 * - **Float formatting**: `float_to_fixed`, `float_to_precision`, `float_to_string`
 * - **Float to string**: `f64_to_string`, `f32_to_string`, `f*_to_string_precision`,
 *   `f*_to_exp_string`
 * - **Integer formatting**: `i64_to_binary_str`, `i64_to_octal_str`, `i64_to_*_hex_str`
 * - **Nextafter**: `nextafter32` (nextafter is already in libm)
 *
 * ## Removed (replaced by LLVM intrinsics/instructions in math.cpp)
 *
 * - float_round, float_floor, float_ceil → @llvm.round/floor/ceil
 * - float_abs, float_sqrt, float_pow → @llvm.fabs/sqrt/pow
 * - float32_bits, float32_from_bits, float64_bits, float64_from_bits → bitcast
 * - infinity, nan_val, is_inf, is_nan → LLVM constants + fcmp
 * - int_to_float, float_to_int → sitofp/fptosi
 * - f32_is_nan, f32_is_infinite → fcmp uno / fabs+fcmp in string.cpp
 *
 * @see codegen/builtins/math.cpp for compiler builtin registration
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Black Box (prevent optimization)
// ============================================================================

// Compiler barrier to prevent optimization
#ifdef _MSC_VER
#pragma optimize("", off)
int32_t black_box_i32(int32_t value) {
    return value;
}
int64_t black_box_i64(int64_t value) {
    return value;
}
double black_box_f64(double value) {
    return value;
}
#pragma optimize("", on)
#else
__attribute__((noinline)) int32_t black_box_i32(int32_t value) {
    __asm__ volatile("" : "+r"(value));
    return value;
}
__attribute__((noinline)) int64_t black_box_i64(int64_t value) {
    __asm__ volatile("" : "+r"(value));
    return value;
}
__attribute__((noinline)) double black_box_f64(double value) {
    __asm__ volatile("" : "+x"(value));
    return value;
}
#endif

// ============ SIMD OPERATIONS ============

// Sum of i32 array using SIMD-friendly loop
int64_t simd_sum_i32(const int32_t* arr, int64_t len) {
    int64_t sum = 0;
    for (int64_t i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}

// Sum of f64 array
double simd_sum_f64(const double* arr, int64_t len) {
    double sum = 0.0;
    for (int64_t i = 0; i < len; i++) {
        sum += arr[i];
    }
    return sum;
}

// Dot product of f64 arrays
double simd_dot_f64(const double* a, const double* b, int64_t len) {
    double sum = 0.0;
    for (int64_t i = 0; i < len; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

// ============ FLOAT CONVERSION FUNCTIONS ============

static char float_buffer[64];

// float_to_fixed(value: F64, decimals: I32) -> Str
const char* float_to_fixed(double value, int32_t decimals) {
    if (decimals < 0)
        decimals = 0;
    if (decimals > 20)
        decimals = 20;
    snprintf(float_buffer, sizeof(float_buffer), "%.*f", decimals, value);
    return float_buffer;
}

// float_to_string(value: F64) -> Str
const char* float_to_string(double value) {
    snprintf(float_buffer, sizeof(float_buffer), "%g", value);
    return float_buffer;
}

// f64_to_string(value: F64) -> Str
// Alias for float_to_string, used by MIR codegen
const char* f64_to_string(double value) {
    return float_to_string(value);
}

// f32_to_string(value: F32) -> Str
// Used by MIR codegen for F32.to_string()
const char* f32_to_string(float value) {
    snprintf(float_buffer, sizeof(float_buffer), "%g", (double)value);
    return float_buffer;
}

// f64_to_string_precision(value: F64, precision: I64) -> Str
const char* f64_to_string_precision(double value, int64_t precision) {
    if (precision < 0)
        precision = 0;
    if (precision > 20)
        precision = 20;
    snprintf(float_buffer, sizeof(float_buffer), "%.*f", (int)precision, value);
    return float_buffer;
}

// f32_to_string_precision(value: F32, precision: I64) -> Str
const char* f32_to_string_precision(float value, int64_t precision) {
    return f64_to_string_precision((double)value, precision);
}

// f64_to_exp_string(value: F64, uppercase: Bool) -> Str
const char* f64_to_exp_string(double value, int32_t uppercase) {
    snprintf(float_buffer, sizeof(float_buffer), uppercase ? "%E" : "%e", value);
    return float_buffer;
}

// f32_to_exp_string(value: F32, uppercase: Bool) -> Str
const char* f32_to_exp_string(float value, int32_t uppercase) {
    return f64_to_exp_string((double)value, uppercase);
}

// ============================================================================
// Integer formatting functions (binary, octal, hex)
// ============================================================================

static char* fmt_alloc_copy(const char* src, int start, int end) {
    int len = end - start;
    char* buf = (char*)malloc(len + 1);
    if (!buf)
        return "";
    memcpy(buf, src + start, len);
    buf[len] = '\0';
    return buf;
}

// i64_to_binary_str(value: I64) -> Str (binary with "0b" prefix)
const char* i64_to_binary_str(int64_t value) {
    if (value == 0) {
        char* r = (char*)malloc(4);
        if (!r)
            return "";
        r[0] = '0';
        r[1] = 'b';
        r[2] = '0';
        r[3] = '\0';
        return r;
    }
    uint64_t uval = (uint64_t)value;
    char temp[67];
    int pos = 66;
    temp[pos] = '\0';
    while (uval != 0 && pos > 2) {
        temp[--pos] = (uval & 1) ? '1' : '0';
        uval >>= 1;
    }
    temp[--pos] = 'b';
    temp[--pos] = '0';
    return fmt_alloc_copy(temp, pos, 67);
}

// i64_to_octal_str(value: I64) -> Str (octal with "0o" prefix)
const char* i64_to_octal_str(int64_t value) {
    if (value == 0) {
        char* r = (char*)malloc(4);
        if (!r)
            return "";
        r[0] = '0';
        r[1] = 'o';
        r[2] = '0';
        r[3] = '\0';
        return r;
    }
    uint64_t uval = (uint64_t)value;
    char temp[25];
    int pos = 24;
    temp[pos] = '\0';
    while (uval != 0 && pos > 2) {
        temp[--pos] = '0' + (char)(uval & 7);
        uval >>= 3;
    }
    temp[--pos] = 'o';
    temp[--pos] = '0';
    return fmt_alloc_copy(temp, pos, 25);
}

// i64_to_lower_hex_str(value: I64) -> Str (hex with "0x" prefix, lowercase)
const char* i64_to_lower_hex_str(int64_t value) {
    if (value == 0) {
        char* r = (char*)malloc(4);
        if (!r)
            return "";
        r[0] = '0';
        r[1] = 'x';
        r[2] = '0';
        r[3] = '\0';
        return r;
    }
    uint64_t uval = (uint64_t)value;
    char temp[19];
    int pos = 18;
    temp[pos] = '\0';
    const char* digits = "0123456789abcdef";
    while (uval != 0 && pos > 2) {
        temp[--pos] = digits[uval & 0xF];
        uval >>= 4;
    }
    temp[--pos] = 'x';
    temp[--pos] = '0';
    return fmt_alloc_copy(temp, pos, 19);
}

// i64_to_upper_hex_str(value: I64) -> Str (hex with "0x" prefix, uppercase)
const char* i64_to_upper_hex_str(int64_t value) {
    if (value == 0) {
        char* r = (char*)malloc(4);
        if (!r)
            return "";
        r[0] = '0';
        r[1] = 'x';
        r[2] = '0';
        r[3] = '\0';
        return r;
    }
    uint64_t uval = (uint64_t)value;
    char temp[19];
    int pos = 18;
    temp[pos] = '\0';
    const char* digits = "0123456789ABCDEF";
    while (uval != 0 && pos > 2) {
        temp[--pos] = digits[uval & 0xF];
        uval >>= 4;
    }
    temp[--pos] = 'x';
    temp[--pos] = '0';
    return fmt_alloc_copy(temp, pos, 19);
}

// ============ NEXTAFTER FUNCTIONS ============

// nextafter(x: F64, y: F64) -> F64
// Already available as math.h function (linked from libm)

// nextafter32(x: F32, y: F32) -> F32
float nextafter32(float x, float y) {
    return nextafterf(x, y);
}
