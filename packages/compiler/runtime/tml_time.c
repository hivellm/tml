// TML Runtime - Time and Float Functions
// Time functions, Instant API, Float formatting

#include "tml_runtime.h"

// ============ TIME FUNCTIONS ============

#ifdef _WIN32
static LARGE_INTEGER perf_freq;
static int freq_initialized = 0;

static void init_perf_freq(void) {
    if (!freq_initialized) {
        QueryPerformanceFrequency(&perf_freq);
        freq_initialized = 1;
    }
}
#endif

int32_t tml_time_ms(void) {
#ifdef _WIN32
    return (int32_t)GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

int64_t tml_time_us(void) {
#ifdef _WIN32
    init_perf_freq();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (int64_t)(now.QuadPart * 1000000 / perf_freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
#endif
}

int64_t tml_time_ns(void) {
#ifdef _WIN32
    init_perf_freq();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (int64_t)(now.QuadPart * 1000000000 / perf_freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + (int64_t)ts.tv_nsec;
#endif
}

static char elapsed_buffer[64];

const char* tml_elapsed_secs(int32_t start_ms) {
    int32_t now = tml_time_ms();
    double elapsed = (double)(now - start_ms) / 1000.0;
    snprintf(elapsed_buffer, sizeof(elapsed_buffer), "%.3f", elapsed);
    return elapsed_buffer;
}

int32_t tml_elapsed_ms(int32_t start_ms) {
    return tml_time_ms() - start_ms;
}

// ============ INSTANT API (like Rust's std::time::Instant) ============

int64_t tml_instant_now(void) {
    return tml_time_us();
}

int64_t tml_instant_elapsed(int64_t start_us) {
    return tml_time_us() - start_us;
}

double tml_duration_as_secs_f64(int64_t duration_us) {
    return (double)duration_us / 1000000.0;
}

double tml_duration_as_millis_f64(int64_t duration_us) {
    return (double)duration_us / 1000.0;
}

int64_t tml_duration_as_millis(int64_t duration_us) {
    return duration_us / 1000;
}

int64_t tml_duration_as_secs(int64_t duration_us) {
    return duration_us / 1000000;
}

const char* tml_duration_format_ms(int64_t duration_us) {
    double ms = (double)duration_us / 1000.0;
    snprintf(elapsed_buffer, sizeof(elapsed_buffer), "%.3f", ms);
    return elapsed_buffer;
}

const char* tml_duration_format_secs(int64_t duration_us) {
    double secs = (double)duration_us / 1000000.0;
    snprintf(elapsed_buffer, sizeof(elapsed_buffer), "%.6f", secs);
    return elapsed_buffer;
}

// ============ FLOAT FUNCTIONS ============

static char float_buffer[64];

const char* tml_float_to_fixed(double value, int32_t decimals) {
    if (decimals < 0) decimals = 0;
    if (decimals > 20) decimals = 20;
    snprintf(float_buffer, sizeof(float_buffer), "%.*f", decimals, value);
    return float_buffer;
}

const char* tml_float_to_precision(double value, int32_t precision) {
    if (precision < 1) precision = 1;
    if (precision > 21) precision = 21;
    snprintf(float_buffer, sizeof(float_buffer), "%.*g", precision, value);
    return float_buffer;
}

const char* tml_float_to_string(double value) {
    snprintf(float_buffer, sizeof(float_buffer), "%g", value);
    return float_buffer;
}
