// TML Runtime - Memory Functions
// Matches: env_builtins_mem.cpp

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// ============ Allocation ============

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

// ============ Memory Operations ============

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

// ============ Memory Comparison ============

// mem_compare(a: *Unit, b: *Unit, size: I64) -> I32
int32_t mem_compare(const void* a, const void* b, int64_t size) {
    return memcmp(a, b, (size_t)size);
}

// mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool
int32_t mem_eq(const void* a, const void* b, int64_t size) {
    return memcmp(a, b, (size_t)size) == 0 ? 1 : 0;
}