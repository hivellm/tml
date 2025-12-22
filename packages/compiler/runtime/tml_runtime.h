// TML Runtime - Common Header
// Provides shared includes and declarations for all runtime modules

#ifndef TML_RUNTIME_H
#define TML_RUNTIME_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>

// ============ BLACK BOX ============
int32_t tml_black_box_i32(int32_t value);
int64_t tml_black_box_i64(int64_t value);

// ============ SIMD ============
int64_t tml_simd_sum_i32(const int32_t* arr, int64_t len);
int64_t tml_simd_sum_i64(const int64_t* arr, int64_t len);
double tml_simd_sum_f64(const double* arr, int64_t len);
double tml_simd_dot_f64(const double* a, const double* b, int64_t len);
void tml_simd_fill_i32(int32_t* arr, int32_t value, int64_t len);
void tml_simd_add_i32(const int32_t* a, const int32_t* b, int32_t* c, int64_t len);
void tml_simd_mul_i32(const int32_t* a, const int32_t* b, int32_t* c, int64_t len);

// ============ THREADS ============
typedef void (*ThreadFunc)(void* arg);
void* tml_thread_spawn(ThreadFunc func, void* arg);
void tml_thread_join(void* handle);
void tml_thread_yield(void);
void tml_thread_sleep_ms(int64_t ms);

// ============ CHANNELS ============
typedef struct Channel Channel;
Channel* tml_channel_new(int64_t capacity);
void tml_channel_send(Channel* ch, void* data);
void* tml_channel_recv(Channel* ch);
int tml_channel_try_send(Channel* ch, void* data);
void* tml_channel_try_recv(Channel* ch);
void tml_channel_close(Channel* ch);
int tml_channel_is_closed(Channel* ch);
void tml_channel_free(Channel* ch);

// ============ MUTEX ============
typedef struct Mutex Mutex;
Mutex* tml_mutex_new(void);
void tml_mutex_lock(Mutex* m);
void tml_mutex_unlock(Mutex* m);
int tml_mutex_try_lock(Mutex* m);
void tml_mutex_free(Mutex* m);

// ============ WAIT GROUP ============
typedef struct WaitGroup WaitGroup;
WaitGroup* tml_waitgroup_new(void);
void tml_waitgroup_add(WaitGroup* wg, int64_t delta);
void tml_waitgroup_done(WaitGroup* wg);
void tml_waitgroup_wait(WaitGroup* wg);
void tml_waitgroup_free(WaitGroup* wg);

// ============ ATOMIC ============
typedef struct AtomicCounter AtomicCounter;
AtomicCounter* tml_atomic_new(int64_t initial);
int64_t tml_atomic_load(AtomicCounter* a);
void tml_atomic_store(AtomicCounter* a, int64_t value);
int64_t tml_atomic_add(AtomicCounter* a, int64_t delta);
int64_t tml_atomic_sub(AtomicCounter* a, int64_t delta);
void tml_atomic_free(AtomicCounter* a);

// ============ LIST ============
typedef struct List List;
List* tml_list_new(void);
void tml_list_push(List* list, int64_t value);
int64_t tml_list_pop(List* list);
int64_t tml_list_get(List* list, int64_t index);
void tml_list_set(List* list, int64_t index, int64_t value);
int64_t tml_list_len(List* list);
int tml_list_is_empty(List* list);
void tml_list_clear(List* list);
void tml_list_free(List* list);

// ============ HASHMAP ============
typedef struct HashMap HashMap;
HashMap* tml_hashmap_new(void);
void tml_hashmap_insert(HashMap* map, int64_t key, int64_t value);
int64_t tml_hashmap_get(HashMap* map, int64_t key);
int tml_hashmap_contains(HashMap* map, int64_t key);
void tml_hashmap_remove(HashMap* map, int64_t key);
int64_t tml_hashmap_len(HashMap* map);
void tml_hashmap_clear(HashMap* map);
void tml_hashmap_free(HashMap* map);

// ============ BUFFER ============
typedef struct Buffer Buffer;
Buffer* tml_buffer_new(int64_t capacity);
void tml_buffer_write_byte(Buffer* buf, int32_t byte);
void tml_buffer_write_i32(Buffer* buf, int32_t value);
void tml_buffer_write_i64(Buffer* buf, int64_t value);
int32_t tml_buffer_read_byte(Buffer* buf);
int32_t tml_buffer_read_i32(Buffer* buf);
int64_t tml_buffer_read_i64(Buffer* buf);
int64_t tml_buffer_len(Buffer* buf);
void tml_buffer_reset(Buffer* buf);
void tml_buffer_free(Buffer* buf);

// ============ STRING ============
const char* tml_str_concat(const char* a, const char* b);
int64_t tml_str_len(const char* s);
int tml_str_eq(const char* a, const char* b);

// ============ STRING MAP ============
typedef struct StrMap StrMap;
StrMap* tml_strmap_new(void);
void tml_strmap_insert(StrMap* map, const char* key, const char* value);
const char* tml_strmap_get(StrMap* map, const char* key);
int tml_strmap_contains(StrMap* map, const char* key);
void tml_strmap_remove(StrMap* map, const char* key);
int64_t tml_strmap_len(StrMap* map);
void tml_strmap_free(StrMap* map);

// ============ TIME ============
int32_t tml_time_ms(void);
int64_t tml_time_us(void);
int64_t tml_time_ns(void);
const char* tml_elapsed_secs(int32_t start_ms);
int32_t tml_elapsed_ms(int32_t start_ms);

// ============ INSTANT API ============
int64_t tml_instant_now(void);
int64_t tml_instant_elapsed(int64_t start_us);
double tml_duration_as_secs_f64(int64_t duration_us);
double tml_duration_as_millis_f64(int64_t duration_us);
int64_t tml_duration_as_millis(int64_t duration_us);
int64_t tml_duration_as_secs(int64_t duration_us);
const char* tml_duration_format_ms(int64_t duration_us);
const char* tml_duration_format_secs(int64_t duration_us);

// ============ FLOAT ============
const char* tml_float_to_fixed(double value, int32_t decimals);
const char* tml_float_to_precision(double value, int32_t precision);
const char* tml_float_to_string(double value);

#endif // TML_RUNTIME_H
