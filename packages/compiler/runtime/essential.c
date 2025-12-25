// TML Essential Runtime - Minimal Core Implementation

#include "essential.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

// ============ Core I/O ============

void panic(const char* msg) {
    fprintf(stderr, "panic: %s\n", msg);
    exit(1);
}

void print(const char* str) {
    printf("%s", str);
}

void println(const char* str) {
    printf("%s\n", str);
}

void print_i32(int32_t n) {
    printf("%d", n);
}

void print_i64(int64_t n) {
    printf("%lld", (long long)n);
}

void print_f64(double n) {
    printf("%g", n);
}

void print_bool(int32_t b) {
    printf("%s", b ? "true" : "false");
}

// ============ String Utilities ============

int32_t str_len(const char* s) {
    if (!s) return 0;
    return (int32_t)strlen(s);
}

int32_t str_eq(const char* a, const char* b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

int32_t str_hash(const char* s) {
    if (!s) return 0;
    // DJB2 hash algorithm
    uint32_t hash = 5381;
    int c;
    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return (int32_t)hash;
}

// ============ Time Functions ============

#ifdef _WIN32
int32_t time_ms(void) {
    return (int32_t)GetTickCount();
}

int64_t time_us(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (int64_t)(count.QuadPart * 1000000 / freq.QuadPart);
}

int64_t time_ns(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (int64_t)(count.QuadPart * 1000000000 / freq.QuadPart);
}
#else
int32_t time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

int64_t time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000000LL + tv.tv_usec);
}

int64_t time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}
#endif