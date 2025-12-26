// TML Runtime - Time Functions
// Matches: env_builtins_time.cpp

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

// ============ Time Reading ============

// time_ms() -> I32
#ifdef _WIN32
int32_t time_ms(void) {
    return (int32_t)GetTickCount();
}
#else
int32_t time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}
#endif

// time_us() -> I64
#ifdef _WIN32
int64_t time_us(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (int64_t)(count.QuadPart * 1000000 / freq.QuadPart);
}
#else
int64_t time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000000LL + tv.tv_usec);
}
#endif

// time_ns() -> I64
#ifdef _WIN32
int64_t time_ns(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (int64_t)(count.QuadPart * 1000000000 / freq.QuadPart);
}
#else
int64_t time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}
#endif

// ============ Sleep ============

// sleep_ms(ms: I32) -> Unit
#ifdef _WIN32
void sleep_ms(int32_t ms) {
    Sleep((DWORD)ms);
}
#else
void sleep_ms(int32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}
#endif

// sleep_us(us: I64) -> Unit
#ifdef _WIN32
void sleep_us(int64_t us) {
    DWORD ms = (DWORD)(us / 1000);
    if (ms == 0 && us > 0) ms = 1;
    Sleep(ms);
}
#else
void sleep_us(int64_t us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}
#endif

// ============ Elapsed Time ============

// elapsed_ms(start: I32) -> I32
int32_t elapsed_ms(int32_t start) {
    return time_ms() - start;
}

// elapsed_us(start: I64) -> I64
int64_t elapsed_us(int64_t start) {
    return time_us() - start;
}

// elapsed_ns(start: I64) -> I64
int64_t elapsed_ns(int64_t start) {
    return time_ns() - start;
}

// ============ Elapsed Seconds (formatted string) ============

#include <stdio.h>

static char elapsed_buffer[32];

// elapsed_secs(start_ms: I32) -> Str - Returns "X.XXX" format
const char* elapsed_secs(int32_t start_ms) {
    int32_t elapsed = time_ms() - start_ms;
    double secs = elapsed / 1000.0;
    snprintf(elapsed_buffer, sizeof(elapsed_buffer), "%.3f", secs);
    return elapsed_buffer;
}

// ============ Instant API (like Rust's std::time::Instant) ============

// instant_now() -> I64 - Get current instant in nanoseconds
int64_t instant_now(void) {
    return time_ns();
}

// instant_elapsed(start: I64) -> I64 - Get elapsed duration in nanoseconds
int64_t instant_elapsed(int64_t start) {
    return time_ns() - start;
}

// ============ Duration API ============

// duration_as_millis_f64(duration_ns: I64) -> F64
double duration_as_millis_f64(int64_t duration_ns) {
    return duration_ns / 1000000.0;
}

// duration_format_secs(duration_ns: I64) -> Str - Returns "X.XXXXXX" format
const char* duration_format_secs(int64_t duration_ns) {
    double secs = duration_ns / 1000000000.0;
    snprintf(elapsed_buffer, sizeof(elapsed_buffer), "%.6f", secs);
    return elapsed_buffer;
}