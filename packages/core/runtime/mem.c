// TML Standard Library - Memory Runtime
// Wrappers for malloc/free system calls

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// ============ MEMORY ALLOCATION ============

// Allocate memory for N elements of size sizeof(i32)
int32_t* tml_alloc(int64_t count) {
    return (int32_t*)malloc(count * sizeof(int32_t));
}

void tml_dealloc(int32_t* ptr) {
    free(ptr);
}

// ============ MEMORY READ/WRITE ============

int32_t tml_read_i32(const int32_t* ptr) {
    return *ptr;
}

void tml_write_i32(int32_t* ptr, int32_t value) {
    *ptr = value;
}

// ============ POINTER OFFSET ============

int32_t* tml_ptr_offset(int32_t* ptr, int64_t offset) {
    return ptr + offset;
}
