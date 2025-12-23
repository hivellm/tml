// TML Essential Runtime - Minimal Core Implementation
// Panic + basic I/O + math builtins

#include "tml_essential.h"
#include <string.h>

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
