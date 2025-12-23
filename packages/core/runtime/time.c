// TML Core Runtime - Time Functions (Higher-level APIs)
// NOTE: Basic time_ms, time_us, time_ns are in tml_essential.c
// This file provides higher-level time utilities (Instant, Duration, etc.)

#include <stdint.h>
#include <stdio.h>

// Forward declarations for essential runtime functions
extern int32_t tml_time_ms(void);
extern int64_t tml_time_us(void);
extern int64_t tml_time_ns(void);

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
