// TML Runtime - Essential Functions Header
// Declarations for all essential runtime functions

#ifndef TML_ESSENTIAL_H
#define TML_ESSENTIAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// IO Functions
// ============================================================================

void print(const char* message);
void println(const char* message);
void panic(const char* message);
void assert_tml(int32_t condition, const char* message);

// Type-specific print variants
void print_i32(int32_t n);
void print_i64(int64_t n);
void print_f32(float n);
void print_f64(double n);
void print_bool(int32_t b);
void print_char(int32_t c);

// ============================================================================
// String Functions
// ============================================================================

int32_t str_len(const char* s);
int32_t str_eq(const char* a, const char* b);
int32_t str_hash(const char* s);
const char* str_concat(const char* a, const char* b);
const char* str_substring(const char* s, int32_t start, int32_t len);
int32_t str_contains(const char* haystack, const char* needle);
int32_t str_starts_with(const char* s, const char* prefix);
int32_t str_ends_with(const char* s, const char* suffix);
const char* str_to_upper(const char* s);
const char* str_to_lower(const char* s);
const char* str_trim(const char* s);
int32_t str_char_at(const char* s, int32_t index);

// ============================================================================
// Time Functions
// ============================================================================

int32_t time_ms(void);
int64_t time_us(void);
int64_t time_ns(void);
void sleep_ms(int32_t ms);
void sleep_us(int64_t us);
int32_t elapsed_ms(int32_t start);
int64_t elapsed_us(int64_t start);
int64_t elapsed_ns(int64_t start);

// ============================================================================
// Memory Functions
// ============================================================================

void* mem_alloc(int64_t size);
void* mem_alloc_zeroed(int64_t size);
void* mem_realloc(void* ptr, int64_t new_size);
void mem_free(void* ptr);
void mem_copy(void* dest, const void* src, int64_t size);
void mem_move(void* dest, const void* src, int64_t size);
void mem_set(void* ptr, int32_t value, int64_t size);
void mem_zero(void* ptr, int64_t size);
int32_t mem_compare(const void* a, const void* b, int64_t size);
int32_t mem_eq(const void* a, const void* b, int64_t size);

#ifdef __cplusplus
}
#endif

#endif // TML_ESSENTIAL_H
