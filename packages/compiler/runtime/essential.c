// TML Runtime - Essential Functions
// This file combines all essential runtime functions for TML programs

// ============================================================================
// IO Functions (matches env_builtins_io.cpp)
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

// print(message: Str) -> Unit
void print(const char* message) {
    if (message) printf("%s", message);
}

// println(message: Str) -> Unit
void println(const char* message) {
    if (message) printf("%s\n", message);
    else printf("\n");
}

// panic(message: Str) -> Never
void panic(const char* message) {
    fprintf(stderr, "panic: %s\n", message ? message : "(null)");
    exit(1);
}

// assert(condition: Bool, message: Str) -> Unit
void assert_tml(int32_t condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message ? message : "(no message)");
        exit(1);
    }
}

// Type-specific print variants (for polymorphic print)
void print_i32(int32_t n) { printf("%d", n); }
void print_i64(int64_t n) { printf("%lld", (long long)n); }
void print_f32(float n) { printf("%g", n); }
void print_f64(double n) { printf("%g", n); }
void print_bool(int32_t b) { printf("%s", b ? "true" : "false"); }
void print_char(int32_t c) { printf("%c", (char)c); }

// ============================================================================
// String Functions (matches env_builtins_string.cpp)
// ============================================================================

// Static buffer for string operations
static char str_buffer[4096];

// str_len(s: Str) -> I32
int32_t str_len(const char* s) {
    if (!s) return 0;
    return (int32_t)strlen(s);
}

// str_eq(a: Str, b: Str) -> Bool
int32_t str_eq(const char* a, const char* b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

// str_hash(s: Str) -> I32
int32_t str_hash(const char* s) {
    if (!s) return 0;
    uint32_t hash = 5381;
    int c;
    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return (int32_t)hash;
}

// str_concat(a: Str, b: Str) -> Str
const char* str_concat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    if (len_a + len_b >= sizeof(str_buffer)) {
        len_b = sizeof(str_buffer) - len_a - 1;
    }
    memcpy(str_buffer, a, len_a);
    memcpy(str_buffer + len_a, b, len_b);
    str_buffer[len_a + len_b] = '\0';
    return str_buffer;
}

// str_substring(s: Str, start: I32, len: I32) -> Str
const char* str_substring(const char* s, int32_t start, int32_t len) {
    if (!s) return "";
    int32_t slen = (int32_t)strlen(s);
    if (start < 0 || start >= slen || len <= 0) return "";
    if (start + len > slen) len = slen - start;
    if (len >= (int32_t)sizeof(str_buffer)) len = sizeof(str_buffer) - 1;
    memcpy(str_buffer, s + start, len);
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_contains(haystack: Str, needle: Str) -> Bool
int32_t str_contains(const char* haystack, const char* needle) {
    if (!haystack || !needle) return 0;
    return strstr(haystack, needle) != NULL ? 1 : 0;
}

// str_starts_with(s: Str, prefix: Str) -> Bool
int32_t str_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return 0;
    size_t prefix_len = strlen(prefix);
    return strncmp(s, prefix, prefix_len) == 0 ? 1 : 0;
}

// str_ends_with(s: Str, suffix: Str) -> Bool
int32_t str_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) return 0;
    size_t s_len = strlen(s);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > s_len) return 0;
    return strcmp(s + s_len - suffix_len, suffix) == 0 ? 1 : 0;
}

// str_to_upper(s: Str) -> Str
const char* str_to_upper(const char* s) {
    if (!s) return "";
    size_t len = strlen(s);
    if (len >= sizeof(str_buffer)) len = sizeof(str_buffer) - 1;
    for (size_t i = 0; i < len; i++) {
        str_buffer[i] = (char)toupper((unsigned char)s[i]);
    }
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_to_lower(s: Str) -> Str
const char* str_to_lower(const char* s) {
    if (!s) return "";
    size_t len = strlen(s);
    if (len >= sizeof(str_buffer)) len = sizeof(str_buffer) - 1;
    for (size_t i = 0; i < len; i++) {
        str_buffer[i] = (char)tolower((unsigned char)s[i]);
    }
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_trim(s: Str) -> Str
const char* str_trim(const char* s) {
    if (!s) return "";
    while (isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) len--;
    if (len >= sizeof(str_buffer)) len = sizeof(str_buffer) - 1;
    memcpy(str_buffer, s, len);
    str_buffer[len] = '\0';
    return str_buffer;
}

// str_char_at(s: Str, index: I32) -> Char
int32_t str_char_at(const char* s, int32_t index) {
    if (!s || index < 0 || index >= (int32_t)strlen(s)) return 0;
    return (int32_t)(unsigned char)s[index];
}

// ============================================================================
// Time Functions (matches env_builtins_time.cpp)
// ============================================================================

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

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

// ============================================================================
// Memory Functions (matches env_builtins_mem.cpp)
// ============================================================================

// mem_alloc(size: I64) -> *Unit
void* mem_alloc(int64_t size) {
    return malloc((size_t)size);
}

// mem_alloc_zeroed(size: I64) -> *Unit
void* mem_alloc_zeroed(int64_t size) {
    return calloc(1, (size_t)size);
}

// mem_realloc(ptr: *Unit, new_size: I64) -> *Unit
void* mem_realloc(void* ptr, int64_t new_size) {
    return realloc(ptr, (size_t)new_size);
}

// mem_free(ptr: *Unit) -> Unit
void mem_free(void* ptr) {
    free(ptr);
}

// mem_copy(dest: *Unit, src: *Unit, size: I64) -> Unit
void mem_copy(void* dest, const void* src, int64_t size) {
    memcpy(dest, src, (size_t)size);
}

// mem_move(dest: *Unit, src: *Unit, size: I64) -> Unit
void mem_move(void* dest, const void* src, int64_t size) {
    memmove(dest, src, (size_t)size);
}

// mem_set(ptr: *Unit, value: I32, size: I64) -> Unit
void mem_set(void* ptr, int32_t value, int64_t size) {
    memset(ptr, value, (size_t)size);
}

// mem_zero(ptr: *Unit, size: I64) -> Unit
void mem_zero(void* ptr, int64_t size) {
    memset(ptr, 0, (size_t)size);
}

// mem_compare(a: *Unit, b: *Unit, size: I64) -> I32
int32_t mem_compare(const void* a, const void* b, int64_t size) {
    return memcmp(a, b, (size_t)size);
}

// mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool
int32_t mem_eq(const void* a, const void* b, int64_t size) {
    return memcmp(a, b, (size_t)size) == 0 ? 1 : 0;
}

// ============================================================================
// Instant/Duration API (matches codegen/builtins/time.cpp)
// ============================================================================

static char elapsed_buffer[32];

// elapsed_secs(start_ms: I32) -> Str - Returns "X.XXX" format
const char* elapsed_secs(int32_t start_ms) {
    int32_t elapsed = time_ms() - start_ms;
    double secs = elapsed / 1000.0;
    snprintf(elapsed_buffer, sizeof(elapsed_buffer), "%.3f", secs);
    return elapsed_buffer;
}

// instant_now() -> I64 - Get current instant in nanoseconds
int64_t instant_now(void) {
    return time_ns();
}

// instant_elapsed(start: I64) -> I64 - Get elapsed duration in nanoseconds
int64_t instant_elapsed(int64_t start) {
    return time_ns() - start;
}

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

// ============================================================================
// Math Functions (matches codegen/builtins/math.cpp)
// ============================================================================

#include <math.h>

// black_box - Compiler barrier to prevent optimization
#ifdef _MSC_VER
#pragma optimize("", off)
int32_t black_box_i32(int32_t value) { return value; }
int64_t black_box_i64(int64_t value) { return value; }
#pragma optimize("", on)
#else
__attribute__((noinline))
int32_t black_box_i32(int32_t value) {
    __asm__ volatile("" : "+r"(value));
    return value;
}
__attribute__((noinline))
int64_t black_box_i64(int64_t value) {
    __asm__ volatile("" : "+r"(value));
    return value;
}
#endif

// SIMD operations (scalar fallback)
int64_t simd_sum_i32(const int32_t* arr, int64_t len) {
    int64_t sum = 0;
    for (int64_t i = 0; i < len; i++) sum += arr[i];
    return sum;
}

double simd_sum_f64(const double* arr, int64_t len) {
    double sum = 0.0;
    for (int64_t i = 0; i < len; i++) sum += arr[i];
    return sum;
}

double simd_dot_f64(const double* a, const double* b, int64_t len) {
    double sum = 0.0;
    for (int64_t i = 0; i < len; i++) sum += a[i] * b[i];
    return sum;
}

// Float conversion functions
static char float_buffer[64];

const char* float_to_fixed(double value, int32_t decimals) {
    if (decimals < 0) decimals = 0;
    if (decimals > 20) decimals = 20;
    snprintf(float_buffer, sizeof(float_buffer), "%.*f", decimals, value);
    return float_buffer;
}

const char* float_to_precision(double value, int32_t precision) {
    if (precision < 1) precision = 1;
    if (precision > 21) precision = 21;
    snprintf(float_buffer, sizeof(float_buffer), "%.*g", precision, value);
    return float_buffer;
}

const char* float_to_string(double value) {
    snprintf(float_buffer, sizeof(float_buffer), "%g", value);
    return float_buffer;
}

double int_to_float(int32_t value) { return (double)value; }
int32_t float_to_int(double value) { return (int32_t)value; }
int32_t float_round(double value) { return (int32_t)round(value); }
int32_t float_floor(double value) { return (int32_t)floor(value); }
int32_t float_ceil(double value) { return (int32_t)ceil(value); }
double float_abs(double value) { return fabs(value); }
double float_sqrt(double value) { return sqrt(value); }
double float_pow(double base, int32_t exp) { return pow(base, (double)exp); }

// Bit manipulation
uint32_t float32_bits(float f) {
    union { float f; uint32_t u; } conv;
    conv.f = f;
    return conv.u;
}

float float32_from_bits(uint32_t b) {
    union { uint32_t u; float f; } conv;
    conv.u = b;
    return conv.f;
}

uint64_t float64_bits(double f) {
    union { double d; uint64_t u; } conv;
    conv.d = f;
    return conv.u;
}

double float64_from_bits(uint64_t b) {
    union { uint64_t u; double d; } conv;
    conv.u = b;
    return conv.d;
}

// Special float values
double infinity(int32_t sign) { return sign >= 0 ? INFINITY : -INFINITY; }
double nan_val(void) { return NAN; }

int32_t is_inf(double f, int32_t sign) {
    if (sign > 0) return isinf(f) && f > 0 ? 1 : 0;
    if (sign < 0) return isinf(f) && f < 0 ? 1 : 0;
    return isinf(f) ? 1 : 0;
}

int32_t is_nan(double f) { return isnan(f) ? 1 : 0; }

float nextafter32(float x, float y) { return nextafterf(x, y); }
