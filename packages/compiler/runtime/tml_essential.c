// TML Essential Runtime - Minimal Core Implementation
// Panic + basic I/O + math builtins

#include "tml_essential.h"
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

// ============ PANIC (error handling) ============

void tml_panic(const char* msg) {
    fprintf(stderr, "panic: %s\n", msg);
    exit(1);
}

// ============ I/O FUNCTIONS ============
// Note: print() is polymorphic in TML. The compiler generates direct printf()
// calls for I32, Bool, F64, etc. These functions only handle string output.

void tml_print(const char* str) {
    printf("%s", str);
}

void tml_println(const char* str) {
    printf("%s\n", str);
}

// ============ STRING UTILITIES ============

int32_t tml_str_len(const char* s) {
    return (int32_t)strlen(s);
}

int32_t tml_str_eq(const char* a, const char* b) {
    return strcmp(a, b) == 0 ? 1 : 0;
}

int32_t tml_str_hash(const char* s) {
    // Simple djb2 hash
    uint32_t hash = 5381;
    int c;
    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return (int32_t)hash;
}

// ============ TIME FUNCTIONS ============

#ifdef _WIN32
// Windows implementation using QueryPerformanceCounter
static int64_t get_performance_frequency(void) {
    static int64_t freq = 0;
    if (freq == 0) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        freq = f.QuadPart;
    }
    return freq;
}

static int64_t get_performance_counter(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

int32_t tml_time_ms(void) {
    return (int32_t)(GetTickCount64());
}

int64_t tml_time_us(void) {
    int64_t freq = get_performance_frequency();
    int64_t counter = get_performance_counter();
    return (counter * 1000000) / freq;
}

int64_t tml_time_ns(void) {
    int64_t freq = get_performance_frequency();
    int64_t counter = get_performance_counter();
    return (counter * 1000000000) / freq;
}

#else
// POSIX implementation using clock_gettime
int32_t tml_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int64_t tml_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

int64_t tml_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}
#endif

// ============ FLOAT MATH FUNCTIONS ============

double tml_float_sqrt(double x) {
    return sqrt(x);
}

double tml_float_pow(double base, int32_t exp) {
    return pow(base, (double)exp);
}

double tml_float_abs(double x) {
    return fabs(x);
}

double tml_int_to_float(int32_t x) {
    return (double)x;
}

int32_t tml_float_to_int(double x) {
    return (int32_t)x;
}

int32_t tml_float_round(double x) {
    return (int32_t)round(x);
}

int32_t tml_float_floor(double x) {
    return (int32_t)floor(x);
}

int32_t tml_float_ceil(double x) {
    return (int32_t)ceil(x);
}

// ============ BIT MANIPULATION FUNCTIONS ============

// Type-punning unions for bit casting
typedef union {
    float f;
    uint32_t u;
} float32_bits_t;

typedef union {
    double d;
    uint64_t u;
} float64_bits_t;

uint32_t tml_float32_bits(float f) {
    float32_bits_t bits;
    bits.f = f;
    return bits.u;
}

float tml_float32_from_bits(uint32_t b) {
    float32_bits_t bits;
    bits.u = b;
    return bits.f;
}

uint64_t tml_float64_bits(double f) {
    float64_bits_t bits;
    bits.d = f;
    return bits.u;
}

double tml_float64_from_bits(uint64_t b) {
    float64_bits_t bits;
    bits.u = b;
    return bits.d;
}

// ============ SPECIAL FLOAT VALUES ============

double tml_infinity(int32_t sign) {
    if (sign >= 0) {
        return INFINITY;
    } else {
        return -INFINITY;
    }
}

double tml_nan(void) {
    return NAN;
}

int32_t tml_is_inf(double f, int32_t sign) {
    if (sign == 0) {
        return isinf(f) ? 1 : 0;
    } else if (sign > 0) {
        return (isinf(f) && f > 0) ? 1 : 0;
    } else {
        return (isinf(f) && f < 0) ? 1 : 0;
    }
}

int32_t tml_is_nan(double f) {
    return isnan(f) ? 1 : 0;
}

// ============ NEXTAFTER FUNCTIONS ============

double tml_nextafter(double x, double y) {
    return nextafter(x, y);
}

float tml_nextafter32(float x, float y) {
    return nextafterf(x, y);
}
